/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * NPL is free software: you can redistribute it and/or modify it under the
 * terms of the BSD 2-Clause License available in LICENSE or at
 * http://opensource.org/licenses/BSD-2-Clause
 *
 * @file smooth_test1.cpp
 *
 *****************************************************************************/

#include "mrimage.h"
#include "iterators.h"
#include "accessors.h"
#include "ndarray_utils.h"
#include "mrimage_utils.h"
#include "byteswap.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <memory>
#include <cstring>

using namespace std;
using namespace npl;

shared_ptr<MRImage> testimage()
{
	// create an image
	int64_t index[3];
	size_t sz[] = {107, 78, 100};
	auto in = createMRImage(sizeof(sz)/sizeof(size_t), sz, FLOAT64);

	// fill with square
	OrderIter<double> sit(in);
	while(!sit.eof()) {
		sit.index(3, index);
		if(index[0] > sz[0]/10 && index[0] < sz[0]/3 &&
				index[1] > sz[1]/5 && index[1] < sz[1]/2 &&
				index[2] > sz[2]/3 && index[2] < 2*sz[2]/3) {
			sit.set(1);
		} else {
			sit.set(0);
		}
		++sit;
	}

    return in;
}

int main()
{
    auto img = testimage();
    img->write("original1.nii.gz");

//    auto img2 = smoothDownsample(img, .001);
//    img->write("smooth_tiny.nii.gz");
//
//    img2 = smoothDownsample(img, .1);
//    img2->write("smooth_small.nii.gz");
//
    auto img2 = smoothDownsample(img, 1);
    img2->write("smooth_medium1.nii.gz");
//
//    img2 = smoothDownsample(img, 5);
//    img2->write("smooth_large.nii.gz");
//
    // adjust the spacing
    img->spacing(0) = 1;
    img->spacing(1) = 5;
    img->spacing(2) = 10;

    img->write("original2.nii.gz");
//    // smooth again, should be non-isotropic now
//    img2 = smoothDownsample(img, .001);
//    img->write("smooth_tiny2.nii.gz");
//
//    img2 = smoothDownsample(img, .1);
//    img2->write("smooth_small2.nii.gz");
//
    img2 = smoothDownsample(img, 5);
    img2->write("smooth_medium2.nii.gz");
//
//    img2 = smoothDownsample(img, 10);
//    img2->write("smooth_huge2.nii.gz");

    return 0;
}
