/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file distortioncorr.cpp Distortion correction (1D b-spline registration)
 *
 *****************************************************************************/

#include <tclap/CmdLine.h>
#include <version.h>
#include <string>
#include <iomanip>
#include <stdexcept>
#include <fstream>

#include "mrimage.h"
#include "nplio.h"
#include "mrimage_utils.h"
#include "ndarray_utils.h"
#include "iterators.h"
#include "accessors.h"
#include "registration.h"

using namespace npl;
using namespace std;

#define VERYDEBUG
#include "macros.h"

int main(int argc, char** argv)
{
try {
	/*
	 * Command Line
	 */

	TCLAP::CmdLine cmd("Computes a 1D B-spline deformation to morph the moving "
			"image to match a fixed image. For a 4D input, the 0'th volume "
			"will be used. TODO: correlation", ' ', __version__ );

	TCLAP::ValueArg<string> a_fixed("f", "fixed", "Fixed image.", true, "",
			"*.nii.gz", cmd);
	TCLAP::ValueArg<string> a_moving("m", "moving", "Moving image. ", true, 
			"", "*.nii.gz", cmd);

	TCLAP::ValueArg<string> a_metric("M", "metric", "Metric to use. "
			"(NMI, MI, VI, COR-unimplemented)", false, "", "metric", cmd);
	TCLAP::ValueArg<string> a_out("o", "out", "Registered version of "
			"moving image. Note that if the input is 4D, this will NOT "
			"apply the distortion to multiple time-points. To do that use "
			"nplApplyDeform (which will handle motion correctly).", 
			false, "", "*.nii.gz", cmd);
	TCLAP::ValueArg<string> a_transform("t", "transform", "File to write "
			"transform parameters to. This will be a nifti image the B-spline "
			"parameters and the the phase encoding direction set to the "
			"direction of deformation." , false, "", "*.nii", cmd);

	TCLAP::ValueArg<double> a_jacreg("J", "jacreg", "Jacobian regularizer "
			"weight", false, 0.00001, "lambda", cmd);
	TCLAP::ValueArg<double> a_tpsreg("T", "tpsreg", "Thin-Plate-Spline (TPS) "
			"regularizer weight", false, 0.0001, "lambda", cmd);

	TCLAP::MultiArg<double> a_sigmas("S", "sigmas", "Smoothing standard "
			"deviations at each step of the registration.", false, 
			"sd", cmd);

	TCLAP::ValueArg<char> a_dir("d", "direction", "Distortion direction "
			"x or y or z etc.... By default the phase encode direction of "
			"the moving image will be used (if set).", false, 200,
			"n", cmd);
	TCLAP::ValueArg<double> a_bspace("s", "bspline-space", 
			"Spacing of B-Spline knots." , false, 200, "n", cmd);
	TCLAP::ValueArg<int> a_bins("b", "bins", "Bins to use in information "
			"metric to estimate the joint distribution. This is the "
			"the number of bins in the marginal distribution.", false, 200,
			"n", cmd);
	TCLAP::ValueArg<int> a_parzen("r", "radius", "Radius in parzen window "
			"for bins", false, 5, "n", cmd);

	cmd.parse(argc, argv);

	// set up sigmas
	vector<double> sigmas({3,1.5,.5});
	if(a_sigmas.isSet()) 
		sigmas.assign(a_sigmas.begin(), a_sigmas.end());

	/*************************************************************************
	 * Read Inputs
	 *************************************************************************/

	// fixed image
	cerr << "Reading Inputs...";
	ptr<MRImage> moving, in_moving;
	ptr<MRImage> fixed = readMRImage(a_fixed.getValue());
	size_t ndim;
	{
	in_moving = readMRImage(a_moving.getValue());
	cerr << "Done" << endl;
	ndim = min(fixed->ndim(), in_moving->ndim());
	
	cerr << "Extracting first " << ndim << " dims of Fixed Image" << endl;
	fixed = dPtrCast<MRImage>(fixed->copyCast(ndim, fixed->dim(), FLOAT32));

	cerr << "Extracting first " << ndim << " dims of Moving Image" << endl;
	in_moving = dPtrCast<MRImage>(in_moving->copyCast(ndim, in_moving->dim(),
				FLOAT32));

	cerr << "Done\nPutting Moving Image into Fixed Space...";
	moving = dPtrCast<MRImage>(fixed->createAnother());
	vector<int64_t> ind(ndim);
	vector<double> point(ndim);
	
	// Create Interpolator, ensuring that radius is at least 1 pixel in the
	// output space 
	LanczosInterpNDView<double> interp(in_moving);
	interp.setRadius(ceil(fixed->spacing(0)/in_moving->spacing(0)));
	interp.m_ras = true;
	for(NDIter<double> it(moving); !it.eof(); ++it) {
		// get point 
		it.index(ind.size(), ind.data());
		moving->indexToPoint(ind.size(), ind.data(), point.data());

		// sample 
		it.set(interp.get(point));
	}
	}
	cerr << "Done" << endl;

	// Get direction
	int dir = moving->m_phasedim;
	if(a_dir.isSet()) {
		if(a_dir.getValue() < 'x' || a_dir.getValue() > 'z') {
			cerr << "Invalid direction (use x,y or z): " << a_dir.getValue()
				<< endl;
			return -1;
		}
		dir = a_dir.getValue() - (int)'x';
	} else if(dir < 0) {
		cerr << "Error, no direction set, and no phase-encode direction set "
			"in moving image!" << endl;
		return -1;
	}
	
	/*************************************************************************
	 * Registration
	 *************************************************************************/
	ptr<MRImage> transform;
	if(a_metric.getValue() == "COR") {
		cerr << "COR not yet implemented!" << endl;
	} else {
		cout << "Done\nNon-Rigidly Registering with " << a_metric.getValue() 
			<< "..." << endl;

		transform = infoDistCor(fixed, moving, dir, a_bspace.getValue(), 
				a_jacreg.getValue(), a_tpsreg.getValue(), sigmas,
				a_bins.getValue(), a_parzen.getValue(), a_metric.getValue());
	}

	cout << "Finished\nWriting output.";
	if(a_transform.isSet()) 
		transform->write(a_transform.getValue());

	if(a_out.isSet()) {
		// Apply Rigid Transform.
		// Copy input moving then sample
		auto out = dPtrCast<MRImage>(in_moving->copyCast(FLOAT32));
		
		// Create Sampler for Transform and moving view
		BSplineView<double> bvw(transform);
		bvw.m_ras = true;
		LanczosInterpNDView<double> mvw(in_moving);

		// Temps
		vector<double> cind(ndim);
		vector<double> pt(ndim);
		double def;
		double ddef;

		// Iterate over output, and apply transform
		for(NDIter<double> it(out); !it.eof(); ++it) {
			it.index(cind);
			out->indexToPoint(cind.size(), cind.data(), pt.data());
			bvw.get(pt.size(), pt.data(), transform->m_phasedim, def, ddef);
			cind[dir] += def;
			it.set(mvw.get(cind)*(1+ddef));
		}
		out->write(a_out.getValue());
	}
	cout << "Done" << endl;

} catch (TCLAP::ArgException &e)  // catch any exceptions
{ std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl; }
}



