/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * NPL is free software: you can redistribute it and/or modify it under the
 * terms of the BSD 2-Clause License available in LICENSE or at
 * http://opensource.org/licenses/BSD-2-Clause
 *
 * @file simplesegment.cpp Segmentater based on KMeans or Expectation
 * maximization.
 *
 *****************************************************************************/

#include <string>

#include <tclap/CmdLine.h>
#include "nplio.h"
#include "mrimage.h"
#include "iterators.h"
#include "accessors.h"
#include "ndarray_utils.h"
#include "statistics.h"
#include "version.h"
#include "macros.h"

using namespace std;
using namespace npl;

int main(int argc, char** argv)
{
	cerr << "Version: " << __version__ << endl;
	try {
	/*
	 * Command Line
	 */

	TCLAP::CmdLine cmd("Segments image(s) based on intensity", ' ',
			__version__ );

	TCLAP::MultiArg<string> a_in("i", "input", "Input image. If this is 4D "
			"then each of the volumes is treated as a dimension, the same as "
			"for multiple inputs.", true, "*.nii.gz", cmd);
	TCLAP::ValueArg<string> a_out("o", "out", "Output image (3D,INT32).",
			true, "", "*.nii.gz", cmd);
	TCLAP::ValueArg<int> a_clusters("c", "clusters", "Number of clusters",
			false, 3, "<labelcount>", cmd);
	TCLAP::MultiArg<int> a_dims("d", "dims", "Dimensions to use for "
			"classification. This is counted across inputs (-i), so if you "
			"give a 4D image with 3 timepoints and a 4D image with 2 "
			"timepoints then to select the first volume you would do -d 0. "
			"To also select the last you would do -d 0 -d 4",
			false, "*.nii.gz", cmd);
	TCLAP::SwitchArg a_expmax("E", "expmax", "Use gaussian mixture and "
			"expectation maximization algorithm rather than K-means for "
			"clustering", cmd);
	TCLAP::SwitchArg a_standardize("S", "standardize", "Standardize dimensions "
			"so that no one dimension dominates the others because it has "
			"large scale. Should be used for k-means", cmd);

	cmd.parse(argc, argv);

	/**********
	 * Input
	 *********/
	// read inputs
	list<vector<double>> insamples;
	size_t cdim = 0;
	size_t nrows = 0;
	vector<size_t> osize;
	VectorXd origin, spacing;
	MatrixXd direction;
	for(auto it=a_in.begin(); it!=a_in.end(); ++it) {
		auto inimg = readMRImage(*it);
		size_t tlen = inimg->tlen();
		size_t volsize = inimg->elements()/tlen;
		if(nrows == 0){
			nrows = volsize;
			osize.resize(min(inimg->ndim(),3UL));
			for(size_t dd=0; dd<osize.size(); dd++)
				osize[dd] = inimg->dim(dd);
			direction = inimg->getDirection();
			spacing = inimg->getSpacing();
			origin = inimg->getOrigin();
		} else if(nrows != volsize) {
			cerr << "Input volumes must have same number of pixels" << endl;
			cerr << "Input: " << *it << " has different number from the rest!"
				<< endl;
			return -1;
		}

		for(size_t tt=0; tt<tlen; tt++, cdim++) {
			bool use = !a_dims.isSet();
			for(auto dit = a_dims.begin(); dit != a_dims.end(); ++dit) {
				if(*dit == cdim)
					use = true;
			}

			if(use) {
				cerr << "Including " << tt << " from " << *it << endl;
				// copy
				insamples.push_back(vector<double>());
				insamples.back().resize(nrows);
				size_t rr=0;
				for(Vector3DIter<double> it(inimg); !it.eof(); ++it, rr++) {
					insamples.back()[rr] = it[tt];
				}
			}
		}
	}

	if(insamples.size() == 0) {
		cerr << "No Data Selected!" << endl;
		return -1;
	}

	// from insamples, create samples
	MatrixXd samples(nrows, insamples.size());
	size_t cc=0;
	for(auto it=insamples.begin(); it!=insamples.end(); ++it, ++cc) {
		for(size_t rr=0; rr<nrows; rr++) {
			samples(rr, cc) = (*it)[rr];
		}
	}

	if(a_standardize.isSet()) {
		VectorXd means = samples.colwise().mean();
		cerr << "Mean: " << means.transpose() << endl;
		VectorXd var = samples.colwise().squaredNorm().array()/samples.rows();
		var = var.array() - means.array().square();
		cerr << "Var: " << var.transpose() << endl;
		cerr << samples.row(0) << endl;
		for(size_t cc=0; cc<samples.cols(); cc++) {
			samples.col(cc) = (samples.col(cc).array() - means[cc])/
				sqrt(var[cc]);
		}
		cerr << samples.row(0) << endl;
	}

	// Create Classifier
	ptr<Classifier> classifier;
	if(a_expmax.isSet()) {
		cerr << "Expectation Maximization" << endl;
		classifier.reset(new ExpMax(samples.cols(), a_clusters.getValue()));
	} else {
		cerr << "K-Means" << endl;
		classifier.reset(new KMeans(samples.cols(), a_clusters.getValue()));
	}

	// Perform Classification
	cerr << "Computing Groups...";
	classifier->compute(samples);
	cerr << "Done!" << endl;
	cerr << "Classifying...";
	Eigen::VectorXi labels = classifier->classify(samples);
	cerr << "Done!" << endl;

	// Create Output Image
	auto segmented = createMRImage(osize.size(), osize.data(), INT32);
	segmented->setOrient(origin, spacing, direction);

	size_t ii = 0;
	for(FlatIter<double> it(segmented); !it.eof(); ++it, ++ii) {
		it.set(labels[ii]);
	}

	segmented->write("pre-relabel.nii.gz");
	// free sup some membory
	samples.resize(0,0);
	labels.resize(0);

	cerr << "Performing Connected Component Analysis...";
	segmented = dPtrCast<MRImage>(relabelConnected(segmented));
	cerr << "Done";

	assert(ii == nrows);
	segmented->write(a_out.getValue());

	} catch (TCLAP::ArgException &e)  // catch any exceptions
	{ std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl; }
}


