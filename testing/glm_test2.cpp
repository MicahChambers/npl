/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * NPL is free software: you can redistribute it and/or modify it under the
 * terms of the BSD 2-Clause License available in LICENSE or at
 * http://opensource.org/licenses/BSD-2-Clause
 *
 * @file glm_test2.cpp Tests large scale GLM
 *
 *****************************************************************************/

#include <version.h>
#include <string>
#include <stdexcept>
#include <random>

#include <Eigen/Dense>
#include "statistics.h"
#include "mrimage.h"
#include "mrimage_utils.h"
#include "ndarray_utils.h"
#include "iterators.h"
#include "accessors.h"
#include "utility.h"
#include "nplio.h"
#include "basic_plot.h"
#include "fmri_inference.h"

using std::string;
using Eigen::MatrixXd;

using namespace npl;

int main(int argc, char** argv)
{

	vector<size_t> voldim({12,17,13,9});
	vector<size_t> fdim({12,17,13,1024});
	ptr<MRImage> realbeta;
	ptr<MRImage> fmri;

	if(argc == 4) {
		fdim[0] = voldim[0] = atoi(argv[1]);
		fdim[1] = voldim[1] = atoi(argv[2]);
		fdim[2] = voldim[2] = atoi(argv[3]);
	}

	// create X and fill it with chirps frequencies:
	// .01Hz - .1Hz
	MatrixXd X(fdim[3], voldim[3]);
	for(size_t rr=0; rr<X.rows(); rr++ ) {
		for(size_t cc=0; cc<X.cols(); cc++) {
			X(rr,cc) = cos(M_PI*rr*(1+cc)/100);
		}
	}

	/*
	 * Create Test Image
	 */
	realbeta = createMRImage(voldim.size(), voldim.data(), FLOAT32);
	fmri = createMRImage(fdim.size(), fdim.data(), FLOAT32);
	fillCircle(realbeta, 3, 1);
	fillGaussian(fmri);
	size_t blen = voldim[3];
	double noise_sd = 1;
	VectorXd se = noise_sd*(pseudoInverse(X.transpose()*X)).diagonal().array().sqrt();
	cerr<<"StdErr: "<<se.transpose()<<endl;

	// add values from beta*X, cols of X correspond to the highest dim of beta
	for(Vector3DIter<double> bit(realbeta), it(fmri); !it.eof(); ++it, ++bit) {
		VectorXd b(X.cols());
		for(size_t bb=0; bb<X.cols(); bb++)
			b[bb] = bit[bb];
		VectorXd y = X*b;

		// Add noise to samples
		for(size_t rr=0; rr<X.rows(); rr++)
			it.set(rr, it[rr]+y[rr]);
	}

	writeMRImage(fmri, "signal_noise.nii.gz");
	writeMRImage(realbeta, "betas.nii.gz");

	auto t_est = dPtrCast<MRImage>(fmri->copyCast(4, voldim.data()));
	auto p_est = dPtrCast<MRImage>(fmri->copyCast(4, voldim.data()));
	auto b_est = dPtrCast<MRImage>(fmri->copyCast(4, voldim.data()));

	fmriGLM(fmri, X, b_est, t_est, p_est);
	writeMRImage(b_est, "beta_est.nii.gz");
	writeMRImage(p_est, "p_est.nii.gz");
	writeMRImage(t_est, "t_est.nii.gz");

	// Perform Comparison
	size_t total = 0;
	size_t errors = 0;
	for(Vector3DConstIter<double> bit(b_est), tbit(realbeta);
				!tbit.eof() && !bit.eof(); ++bit, ++tbit) {
		for(size_t ii=0; ii<blen; ii++) {
			if(fabs(bit[ii] - tbit[ii]) > 2*se[ii]) {
				cerr<<bit[ii]<<" vs "<<tbit[ii]<<" > "<<2*se[ii]<<endl;
				errors++;
			}
			total++;
		}
	}

	cerr<<"Errors: " << errors<<endl;
	cerr<<"Total Comparisons: " << total<<endl;
	cerr<<"Expected Errors:"<<total*0.05<<endl;
	if(errors > total*0.05) {
		cerr<<"Too many erorrs! "<<errors<<" vs "<<total*0.05<<endl;
		return -1;
	}

	return 0;
}
