/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file motioncorr.cpp Perform motion correction on a 4D Image
 *
 *****************************************************************************/

#include <string>
#include <fstream>
#include <iterator>

#include <tclap/CmdLine.h>

#include "lbfgs.h"
#include "registration.h"

#include "nplio.h"
#include "mrimage.h"
#include "iterators.h"
#include "accessors.h"
#include "ndarray_utils.h"
#include "version.h"
#include "macros.h"

using namespace std;
using namespace npl;

std::ostream_iterator<double> doubleoit(std::cout, ", ");

/**
 * @brief Computes motion parameters from an fMRI image
 *
 * @param fmri Input fMRI 
 * @param reftime Volume to use for reference 
 * @param sigmas Standard deviation (in phyiscal space)
 * @param hardstops Lower bound on negative correlation  -1 would mean that it
 * stops when it hits -1
 *
 * @return 
 */
vector<vector<double>> computeMotion(ptr<const MRImage> fmri, int reftime,
		const vector<double>& sigmas, const vector<double>& hardstops)
{
    using namespace std::placeholders;
    using std::bind;

	vector<vector<double>> motion;
	
	// extract reference volumes and pre-smooth
	vector<size_t> vsize(fmri->dim(), fmri->dim()+fmri->ndim());
	vsize[3] = 0;
	
	auto refvol = dPtrCast<MRImage>(fmri->extractCast(4, vsize.data(),
				FLOAT32));

	// create working volume
	auto movvol = dPtrCast<MRImage>(refvol->createAnother());

	// Iterators
	Vector3DConstIter<double> iit(fmri); /** Input Iterator */
	Vector3DIter<double> mit(movvol);  /** Moving Volume Iterator */
	Vector3DIter<double> fit(refvol);  /** Fixed Volume Iterator */

	// Registration Tools
	RigidCorrComputer comp(refvol, movvol, true);

	// create value and gradient functions
	auto vfunc = bind(&RigidCorrComputer::value, &comp, _1, _2);
	auto vgfunc = bind(&RigidCorrComputer::valueGrad, &comp, _1, _2, _3);
	auto gfunc = bind(&RigidCorrComputer::grad, &comp, _1, _2);
	
	// initialize optimizer
	LBFGSOpt opt(6, vfunc, gfunc, vgfunc);
	opt.stop_Its = 10000;
	opt.stop_X = 0;
	opt.stop_G = 0;
	opt.stop_F = 0.0001;
	opt.state_x.setZero();
	
	for(size_t tt=0; tt<fmri->tlen(); tt++) {
		cerr << "Time " << tt << " / " << fmri->tlen() << endl;
		if(tt == reftime) {
			motion.push_back(vector<double>());
			motion.back().resize(9, 0);
		} else {

			/****************************************************************
			 * Registration
			 ***************************************************************/
			cerr << setw(20) << "Init Rigid:  " << setw(7) << " : " 
				<< opt.state_x.transpose() << endl;

			for(size_t ii=0; ii<sigmas.size(); ii++) {

				/* Extract and Smooth Moving Volume */
				for(iit.goBegin(), mit.goBegin(), fit.goBegin(); !iit.eof(); 
								++iit, ++mit, ++fit) {
					mit.set(iit[tt]);
					fit.set(iit[reftime]);
				}
				for(size_t dd=0; dd<3; dd++) {
					gaussianSmooth1D(movvol, dd, sigmas[ii]);
					gaussianSmooth1D(refvol, dd, sigmas[ii]);
				}
				movvol->write("mov_"+to_string(tt)+"_"+to_string(ii)+".nii.gz");
				refvol->write("ref_"+to_string(tt)+"_"+to_string(ii)+".nii.gz");

				// run the optimizer
				opt.optimize();
				opt.stop_F_under = hardstops[ii];
				StopReason stopr = opt.optimize();
				cerr << Optimizer::explainStop(stopr) << endl;

				cerr << setw(20) << "After Rigid: " << setw(4) << ii << " : " 
					<< opt.state_x.transpose() << endl;
			}
			cerr << setw(20) << "Final Rigid: " << setw(4) << tt << " : " 
				<< opt.state_x.transpose() << endl;
			cerr << "==========================================" << endl;

			/******************************************************************
			 * Results
			 *****************************************************************/
			motion.push_back(vector<double>());
			auto& m = motion.back();
			m.resize(9, 0);

			// set values from parameters, and convert to RAS coordinate so that no
			// matter the sampling after smoothing the values remain
			for(size_t ii=0; ii<3; ii++) {
				m[ii] = (movvol->dim(ii)-1)/2.;
				m[ii+3] = opt.state_x[ii];
				m[ii+6] = opt.state_x[ii+3];
			}
		}
	}

	return motion;
}

int main(int argc, char** argv)
{
	try {
	/*
	 * Command Line
	 */

	TCLAP::CmdLine cmd("Motions corrects a 4D Image.", ' ', 
			__version__ );

	TCLAP::ValueArg<string> a_in("i", "input", "Input 4D Image.", true, "",
			"*.nii.gz", cmd);
	TCLAP::ValueArg<string> a_out("o", "output", "Output 4D motion-corrected "
			"Image.", false, "", "*.nii.gz", cmd);
	TCLAP::ValueArg<int> a_ref("r", "ref", "Reference timepoint, < 0 values "
			"will result in reference at the middle volume, otherwise this "
			"indicates a timepoint (starting at 0)", false, -1, "T", cmd);

	TCLAP::ValueArg<string> a_motion("m", "motion", "Ouput motion as 9 column "
			"text file. Columns are Center (x,y,z), Shift in (x,y,z), and "
			"Rotation about (x,y,z).", false, "", "*.txt", cmd);
	TCLAP::ValueArg<string> a_inmotion("M", "inmotion", "Input motion as 9 "
			"column text file. Columns are Center (x,y,z), Shift in (x,y,z), "
			"and Rotation about (x,y,z). If this is set, then instead of "
			"estimating motion, the inverse motion parameters are just "
			"applied.", false, "", "*.txt", cmd);
	TCLAP::MultiArg<double> a_sigmas("s", "sigmas", "Smoothing standard "
			"deviations. These are the steps of the registration.", false, 
			"sd", cmd);
	TCLAP::MultiArg<double> a_thresh("t", "thresh", "Stop threshold "
			"for correlation at each step.", false, 
			"corr", cmd);

	cmd.parse(argc, argv);

	/**********
	 * Input
	 *********/
	
	// read fMRI
	ptr<MRImage> fmri = readMRImage(a_in.getValue());
	if(fmri->tlen() == 1 || fmri->ndim() != 4) {
		cerr << "Expect a 4D Image, but input had " << fmri->ndim() << 
			" dimensions  and " << fmri->tlen() << " volumes." << endl;
		return -1;
	}
	
	// set reference volume
	int ref = a_ref.getValue();
	if(ref < 0 || ref >= fmri->tlen())
		ref = fmri->tlen()/2;

	// construct variables to get a particular volume
	vector<vector<double>> motion;

	// set up sigmas
	vector<double> sigmas({1,0.5,0});
	if(a_sigmas.isSet()) 
		sigmas.assign(a_sigmas.begin(), a_sigmas.end());
	
	// set up threshold
	vector<double> thresh({0.99,0.999,0.9999});
	if(a_thresh.isSet()) 
		thresh.assign(a_sigmas.begin(), a_sigmas.end());
	for(auto& v: thresh) 
		v = -fabs(v);

	if(thresh.size() != sigmas.size()) {
		cerr << "Threshold and Sigmas must indicate the same number of steps\n";
		return -1;
	}

	if(a_inmotion.isSet()) {
		motion = readNumericCSV(a_inmotion.getValue());

		// Check Motion Results
		if(motion.size() != fmri->tlen()) {
			cerr << "Input motion rows doesn't match input fMRI timepoints!" 
				<< endl; 
			return -1;
		}

		for(size_t ll=0; ll < motion.size(); ll++) {
			if(motion[ll].size() != 9) {
				cerr << "On line " << ll << ", width should be 9 but is " 
					<< motion[ll].size() << endl;
				return -1;
			}
		}

	} else {
		// Compute Motion
	
		motion = computeMotion(fmri, ref, sigmas, thresh);
	}

	// Write to Motion File
	if(a_motion.isSet()) {
		ofstream ofs(a_motion.getValue());
		if(!ofs.is_open()) {
			cerr<<"Error opening "<< a_motion.getValue()<<" for writing\n";
			return -1;
		}
		for(auto& line : motion) {
			assert(line.size() == 0);
			for(size_t ii=0; ii<9; ii++) {
				if(ii != 0)
					ofs << " ";
				ofs<<setw(15)<<setprecision(10)<<line[ii];
			}
			ofs << "\n";
		}
		ofs << "\n";
	}

//	// apply motion parameters
//	NDIter<double> fit(fmri);
//	for(size_t tt=0; tt<fmri->tlen(); tt++) {
//		// extract timepoint
//		for(iit.goBegin(), vit.goBegin(); !iit.eof(); ++iit, ++vit) 
//			vit.set(iit[tt]);
//		
//		// convert motion parameters to centered rotation + translation
//		copy(motion[tt].begin(), motion[tt].begin()+3, rigid.center.data());
//		copy(motion[tt].begin()+3, motion[tt].begin()+6, rigid.shift.data());
//		copy(motion[tt].begin()+6, motion[tt].end(), rigid.rotation.data());
//		rigid.toIndexCoords(vol, true);
//		cerr << "Rigid Transform: " << tt << "\n" << rigid <<endl;
//		
//		// Apply Rigid Transform
//		rotateImageShearFFT(vol, rigid.rotation[0], rigid.rotation[1], 
//				rigid.rotation[2]);
//		vol->write("rotated"+to_string(tt)+".nii.gz");
//		for(size_t dd=0; dd<3; dd++) 
//			shiftImageFFT(vol, dd, rigid.shift[dd]);
//		vol->write("shifted_rotated"+to_string(tt)+".nii.gz");
//
//		// Copy Result Back to input image
//		for(iit.goBegin(), vit.goBegin(); !iit.eof(); ++iit, ++vit) 
//			iit.set(tt, vit[0]);
//	}
//
//	if(a_out.isSet()) 
//		fmri->write(a_out.getValue());

	} catch (TCLAP::ArgException &e)  // catch any exceptions
	{ std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl; }
}



