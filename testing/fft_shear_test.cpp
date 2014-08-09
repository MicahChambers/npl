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
 * @file shear_test.cpp Test to show that shearing using the fourier transform 
 * on each line works as well as manual shearing.
 *
 *****************************************************************************/

/******************************************************************************
 * @file fft_test.cpp
 * @brief This file is specifically to test forward, reverse of fft image
 * procesing functions.
 ******************************************************************************/

#include <version.h>
#include <string>
#include <stdexcept>

#include "mrimage.h"
#include "mrimage_utils.h"
#include "ndarray_utils.h"
#include "iterators.h"
#include "accessors.h"

using namespace npl;
using namespace std;

int closeCompare(shared_ptr<const MRImage> a, shared_ptr<const MRImage> b)
{
	if(a->ndim() != b->ndim()) {
		cerr << "Error image dimensionality differs" << endl;
		return -1;
	}
	
	for(size_t dd=0; dd<a->ndim(); dd++) {
		if(a->dim(dd) != b->dim(dd)) {
			cerr << "Image size in the " << dd << " direction differs" << endl;
			return -1;
		}
	}

	std::vector<int64_t> index(3);
	OrderConstIter<double> ita(a);
	OrderConstIter<double> itb(b);
	itb.setOrder(ita.getOrder());
	for(ita.goBegin(), itb.goBegin(); !ita.eof() && !itb.eof(); ++ita, ++itb) {
		double diff = fabs(*ita - *itb);
		if(diff > 3) {
			ita.index(3, index.data());
			cerr << "Images differ!" << endl;
			for(size_t ii=0; ii<3; ii++)
				cerr << index[ii] << ",";
			cerr << endl;
			return -1;
		}
	}

	return 0;
}

shared_ptr<MRImage> manualShearImage(shared_ptr<MRImage> in, size_t dir,
		size_t len, double* dist)
{
	int64_t index[3];
	double cindex[3];
	std::vector<double> center(in->ndim());
	for(size_t ii=0; ii<center.size(); ii++) {
		center[ii] = in->dim(ii)/2.;
	}

	auto mshear = dynamic_pointer_cast<MRImage>(in->copy());
	LinInterp3DView<double> lininterp(in);

	// stop at the end of x lines
	ChunkIter<double> it(mshear);
	std::vector<int64_t> csize(in->ndim(), 1);
	csize[dir] = 0;
	it.setChunkSize(csize.size(), csize.data()); 
	for(it.goBegin(); !it.eof(); it.nextChunk()) {
		it.index(3, index);
		
		double lineshift = 0;
		for(size_t ii=0; ii<len; ii++) {
			if(ii != dir)
				lineshift += -dist[ii]*(index[ii]-center[ii]);
		}
		
		for(; !it.isChunkEnd(); ++it) {
			it.index(3, index);

			for(size_t ii=0; ii<3; ii++) {
				cindex[ii] = index[ii];
			}

			// grab value
			cindex[dir] += lineshift;
			double v = lininterp(cindex[0], cindex[1], cindex[2]);
			it.set(v);
		}
	}
	return mshear;
}

int main()
{
	int64_t index[3];
	// create an image
	size_t sz[] = {128, 128, 128};
	auto in = createMRImage(sizeof(sz)/sizeof(size_t), sz, FLOAT64);

	// fill with sphere
	cerr << "Creating Sphere...";
	OrderIter<double> sit(in);
	while(!sit.eof()) {
		sit.index(3, index);
		double dist = 0;
		for(size_t ii=0; ii<3 ; ii++) {
			dist += (index[ii]-sz[ii]/2.)*(index[ii]-sz[ii]/2.);
		}
		if(sqrt(dist) < 10)
			sit.set(sqrt(dist));
		else
			sit.set(0);

		++sit;
	}
	cerr << "Done.\nWriting...";
	in->write("original.nii.gz");
	
	double shear[3] = {1, 0.2, .4};

	// manual shear
	cerr << "Done.\nShearing Manually...";
	auto kshear = dynamic_pointer_cast<MRImage>(in->copy());
	clock_t cl = clock();
	shearImageKern(kshear, 0, 3, shear);
	cl = clock() - cl;
	cerr << cl << " Ticks, Done.\nWriting Manual Shear...";
	kshear->write("kern_shear.nii.gz");
	
	cerr << "Done.\nShearing with FFT...";
	auto fshear = dynamic_pointer_cast<MRImage>(in->copy());
	cl = clock();
	shearImageFFT(fshear, 0, 3, shear);
	cl = clock() - cl;
	cerr << cl << " Ticks, Done.\nWriting FFT-Shear...";
	fshear->write("fourier_shear.nii.gz");
	cerr << "Done.\nComparing...";
	if(closeCompare(fshear, kshear) != 0)
		return -1;
	cerr << "Done.\n";
	

	return 0;
}
