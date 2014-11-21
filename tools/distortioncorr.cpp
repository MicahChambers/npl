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
	cerr << "Version: " << __version__ << endl;
	cerr << "Command Line: " << endl;
	for(int ii=0; ii<argc; ii++)
		cerr << argv[ii] << " ";

	try {
	/*
	 * Command Line
	 */

	TCLAP::CmdLine cmd("Computes a 1D B-spline deformation to morph the moving "
			"image to match a fixed image. For a 4D input, the 0'th volume "
			"will be used. TODO: correlation", ' ', __version__ );

	TCLAP::ValueArg<string> a_fixed("f", "fixed", "Fixed image.", false, "",
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
	TCLAP::ValueArg<string> a_field("F", "field", "Write distortion field, "
			"not parameters but sampled B-Spline field at each point in "
			"moving space. ", false, "", "*.nii.gz", cmd);

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

	TCLAP::ValueArg<double> a_stopx("x", "minstep", "Minimum step (change in "
			"parameters, x) to consider taking. If steps drop below this size, "
			"optimization stops." , false, 1e-5, "stepsize", cmd);
	TCLAP::ValueArg<double> a_beta("B", "beta", "Beta in linesearch. This is "
			"the fraction of the previous step size to consider. So if "
			"the linesearch starts at 1, the second step size will be B, "
			"the third will be B^2 and so on.", false, 0.5, "beta", cmd);
	TCLAP::ValueArg<int> a_hist("H", "history", "History for LBFGS to "
			"use when estimating Hessian. More will be more computationally "
			"taxing and less gradient-descent like, but potentially reducing "
			"the number of steps to the minimum.", false, 8, "n", cmd);

	TCLAP::SwitchArg a_otsu("O", "otsu-thresh", "Threshold original images "
			"using otsu thresholding. This reduces computation time by "
			"creating more zero-gradient points in the image. (Which are "
			"ignored in certain calculations)", cmd);

	TCLAP::ValueArg<string> a_apply("a", "apply", "Apply the provided "
			"distortion field parameters. this input should be the output "
			"transform from this program (-t/--transform).", false, "",
			"*.nii.gz", cmd);

	TCLAP::ValueArg<string> a_motion("R", "motion", "Motion parameters, which "
			"is important for applying motion correction to the distortion "
			"field.", false, "", "*.rtm", cmd);

	cmd.parse(argc, argv);

	// set up sigmas
	vector<double> sigmas({3,1.5,.5});
	if(a_sigmas.isSet())
		sigmas.assign(a_sigmas.begin(), a_sigmas.end());

	/*************************************************************************
	 * Read Inputs
	 *************************************************************************/

	if(!a_fixed.isSet() && !a_apply.isSet()) {
		cerr << "Either a fixed image (-f/--fixed, for registration) or "
			"transform (-a/--apply) must be provided" << endl;
		return -1;
	}

	// fixed image
	cerr << "Reading Moving...";
	ptr<MRImage> moving = readMRImage(a_moving.getValue());
	cerr << "Done" << endl;

	size_t ndim = min(moving->ndim(), 3lu);
	int dir;
	ptr<MRImage> transform;
	if(a_fixed.isSet()) {
		cerr << "Creating transform using non-linear distortion correction"
			<< endl;
		ptr<MRImage> in_fixed = readMRImage(a_fixed.getValue());
		cerr << "Done" << endl;
		ndim = min(in_fixed->ndim(), ndim);

		cerr << "Extracting first " << ndim << " dims of Fixed Image" << endl;
		in_fixed = dPtrCast<MRImage>(in_fixed->copyCast(ndim, in_fixed->dim(), FLOAT32));

		cerr << "Extracting first " << ndim << " dims of Moving Image" << endl;
		moving = dPtrCast<MRImage>(moving->copyCast(ndim, moving->dim(), FLOAT32));

		cerr << "Done\nPutting Fixed image in Moving Space...";
		auto fixed = dPtrCast<MRImage>(moving->createAnother(), FLOAT32);
		vector<int64_t> ind(ndim);
		vector<double> point(ndim);

		// Create Interpolator, ensuring that radius is at least 1 pixel in the
		// output space
		LanczosInterpNDView<double> interp(in_fixed);
		interp.setRadius(3*ceil(moving->spacing(0)/in_fixed->spacing(0)));
		interp.m_ras = true;
		for(NDIter<double> it(fixed); !it.eof(); ++it) {
			// get point
			it.index(ind.size(), ind.data());
			fixed->indexToPoint(ind.size(), ind.data(), point.data());

			// sample
			it.set(interp.get(point));
		}
		cerr << "Done" << endl;
		in_fixed.reset();

		// Get direction
		dir = moving->m_phasedim;
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
		if(a_metric.getValue() == "COR") {
			cerr << "COR not yet implemented!" << endl;
		} else {
			cout << "Done\nNon-Rigidly Registering with " << a_metric.getValue()
				<< "..." << endl;

			transform = infoDistCor(fixed, moving, a_otsu.isSet(),
					dir, a_bspace.getValue(),
					a_jacreg.getValue(), a_tpsreg.getValue(), sigmas,
					a_bins.getValue(), a_parzen.getValue(), a_metric.getValue(),
					a_hist.getValue(), a_stopx.getValue(), a_beta.getValue());
		}

		cout << "Finished\nWriting output.";
		if(a_transform.isSet())
			transform->write(a_transform.getValue());

	} else if(a_apply.isSet()) {
		cerr << "Using " << a_apply.getValue() << endl;
		transform = readMRImage(a_apply.getValue());

		// Get direction
		dir = transform->m_phasedim;
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

	} else {
		cerr << "Neither apply (-a/--apply) nor fixed (-f/--fixed) set!" << endl;
		return -1;
	}

	/*
	 * Create Field and Jacobian Maps, in 3 or less dimensions, the write
	 */
	cerr << "Estimating distortion field " << endl;
	auto field = dPtrCast<MRImage>(moving->createAnother(ndim, moving->dim(), FLOAT32));
	BSplineView<double> bsp_vw(transform);
	bsp_vw.m_boundmethod = ZEROFLUX;

	double dcind[3]; // index in distortion image
	double pt[3]; // point
	for(NDIter<double> fit(field); !fit.eof(); ++fit) {
		// Compute Continuous Index of Point in Deform Image
		fit.index(ndim, dcind);
		field->indexToPoint(ndim, dcind, pt);
		transform->pointToIndex(ndim, pt, dcind);

		// Sample B-Spline Value and Derivative at Current Position
		double def = 0, ddef = 0;
		bsp_vw.get(ndim, dcind, dir, def, ddef);
		fit.set(def);
	}
	ptr<MRImage> dfield = dPtrCast<MRImage>(derivative(field));
	cerr << "Done" << endl;

	/*
	 * Write Outputs
	 */
	if(a_field.isSet())
		field->write(a_field.getValue());

	/*
	 * For each time point apply inverse motion to field map, then apply
	 * distortion correction and finally apply forward motion to fMRI
	 */
	if(a_out.isSet()) {
		cerr << "Applying distortion correction" << endl;
		// Re-Read Moving, in case we reduced dimensions earlier
		moving = readMRImage(a_moving.getValue());
		NDConstView<double> move_vw(moving);

		// Read Motion
		vector<vector<double>> motion;
		if(a_motion.isSet()) {
			motion = readNumericCSV(a_motion.getValue());
		} else if(moving->tlen() > 1) {
			cerr << "WARNING! NO MOTION PROVIDED BUT INPUT IS 4D!" << endl;
		}

		LinInterp3DView<double> f_vw(field);
		f_vw.m_boundmethod = ZEROFLUX;
		f_vw.m_ras = true;
		LanczosInterp3DView<double> mov_vw(moving);

		Rigid3DTrans irigid;
		double dcind[3]; // index in distortion image
		Vector3d pt; // point
		auto out = dPtrCast<MRImage>(moving->createAnother(FLOAT32));
		for(size_t tt=0; tt<out->tlen(); ++tt) {

			// Create Rotated Version of B-Spline
			Matrix3d Rinv, R;
			R.setIdentity();
			Rinv.setIdentity();
			if(!motion.empty()) {
				irigid.ras_coord = true;
				for(size_t dd=0; dd<3; dd++) {
					irigid.center[dd] = motion[tt][dd];
					irigid.rotation[dd] = motion[tt][dd+3];
					irigid.shift[dd] = motion[tt][dd+6];
				}
				R = irigid.rotMatrix();
				irigid.invert();
				Rinv = irigid.rotMatrix();
			}

			cerr << "Inverse Rigid: " << endl << irigid << endl;
			// For Each Point in output,
			// Find rotated point
			// Add distortion to index
			// Sample
			for(Vector3DIter<double> oit(out), fit(field), dfit(dfield);
						!oit.eof(); ++oit, ++fit, ++dfit) {

				// Get Deformation at un-rotated point
				double def = *fit;

				// Compute derivative in direction of distortion AFTER rotation
				// at un-rotated point
				double mag = 0;
				double ddef = 0;
				for(size_t dd=0; dd<3; dd++) {
					ddef += dfit[dd]*R(dd, dir)/dfield->spacing(dd);
					mag += R(dd, dir)*R(dd, dir);
				}
				mag = sqrt(mag);
				ddef = ddef/mag;

				oit.index(ndim, dcind);
				out->indexToPoint(ndim, dcind, pt.array().data());

				pt = Rinv*(pt-irigid.center) + irigid.center + irigid.shift;
				out->pointToIndex(ndim, pt.array().data(), dcind);
				dcind[dir] += def/out->spacing(dir);
				double Fm = mov_vw(dcind[0], dcind[1], dcind[2], tt);

				if(Fm < 1e-10 || ddef < -1) Fm = 0;
				oit.set(tt, Fm*(1+ddef));
			}
		}
		cerr << "Done" << endl;
		out->write(a_out.getValue());
	}
	cout << "Done" << endl;

} catch (TCLAP::ArgException &e)  // catch any exceptions
{ std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl; }
}



