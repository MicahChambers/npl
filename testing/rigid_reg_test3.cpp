/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * NPL is free software: you can redistribute it and/or modify it under the
 * terms of the BSD 2-Clause License available in LICENSE or at
 * http://opensource.org/licenses/BSD-2-Clause
 *
 * @file rigid_reg_test3.cpp Tests mutual information derivative
 *
 *****************************************************************************/

#include "mrimage.h"
#include "iterators.h"
#include "accessors.h"
#include "ndarray_utils.h"
#include "mrimage_utils.h"
#include "registration.h"
#include "byteswap.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <memory>
#include <cstring>

using namespace std;
using namespace npl;

double gaussGen(double x, double y, double z, double xsz, double ysz, double zsz)
{
    double v = exp(-pow(xsz/2-x,2)/9)*exp(-pow(ysz/2-y,2)/16)*exp(-pow(zsz/2-z,2)/64);
    if(v > 0.00001)
        return v;
    else
        return 0;
}

shared_ptr<MRImage> gaussianImage()
{
    // create an image
    size_t sz[] = {64,64,64};
    int64_t index[3];
    auto in = createMRImage(sizeof(sz)/sizeof(size_t), sz, FLOAT64);

    // fill with a shape that is somewhat unique when rotated.
    OrderIter<double> sit(in);
    double sum = 0;
    while(!sit.eof()) {
        sit.index(3, index);
        double v= gaussGen(index[0], index[1], index[2], sz[0], sz[1], sz[2]);
        sit.set(v);
        sum += v;
        ++sit;
    }

    for(sit.goBegin(); !sit.eof(); ++sit)
        sit.set(sit.get()/sum);

    return in;
}

shared_ptr<MRImage> squareImage()
{
    // create test image
	int64_t index[3];
	size_t sz[] = {64, 64, 64};
	auto in = createMRImage(sizeof(sz)/sizeof(size_t), sz, FLOAT64);

	// fill with square
	OrderIter<double> sit(in);
	while(!sit.eof()) {
		sit.index(3, index);
		if(index[0] > sz[0]/4 && index[0] < 2*sz[0]/3 &&
				index[1] > sz[1]/5 && index[1] < sz[1]/2 &&
				index[2] > sz[2]/3 && index[2] < 2*sz[2]/3) {
			sit.set(1);
		} else {
			sit.set(0);
		}
		++sit;
	}

    return in;
};

int main()
{
    // create test image
    auto img = gaussianImage();
    //auto img = squareImage();

    // rotate it
    auto moved = dPtrCast<MRImage>(img->copy());
    rotateImageShearFFT(moved, .1, .1, .2);

    shiftImageFFT(moved, 0, 5);
    shiftImageFFT(moved, 1, 7);
    shiftImageFFT(moved, 2, -2);

    // invert
    for(NDIter<double> it(moved); !it.eof(); ++it)
        it.set(-it.get());

    if(information3DDerivTest(0.1, 0.06, img, moved) != 0) {
		cerr << "Error too large" << endl;
        return -1;
	}
    return 0;
}

