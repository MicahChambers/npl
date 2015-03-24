/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * NPL is free software: you can redistribute it and/or modify it under the
 * terms of the BSD 2-Clause License available in LICENSE or at
 * http://opensource.org/licenses/BSD-2-Clause
 *
 * @file slicer_test_roi.cpp Tests region-of-interest specification in Slicer
 * class
 *
 *****************************************************************************/

#include <iostream>

#include "slicer.h"

using namespace std;
using namespace npl;

int main()
{
	size_t dim[3] = {10, 10, 10};
	size_t nelements = 1;
	for(size_t ii=0; ii<3; ii++)
		nelements *= dim[ii];
	std::vector<size_t> data(nelements);

	Slicer slicer(3, dim);

	int64_t roi1[3] = {3, 5, 4};
	size_t roiz[3] = {5, 4, 3};
	slicer.setROI(3, roiz, roi1);
	slicer.goBegin();
	for(int64_t xx=3; xx<=7; xx++) {
		for(int64_t yy=5; yy<=8; yy++) {
			for(int64_t zz=4; zz<=6; zz++) {
				if(100*xx+10*yy+zz != *slicer) {
					cerr << "Error slicer mismatch with known values during "
						"3D roi" << endl;
					return -1;
				}
				++slicer;
			}
		}
	}

	slicer.setROI(2, roiz, roi1);
	slicer.goBegin();
	for(int64_t xx=3; xx<=7; xx++) {
		for(int64_t yy=5; yy<=8; yy++) {
			for(int64_t zz=0; zz<10; zz++) {
				if(100*xx+10*yy+zz != *slicer) {
					cerr << "Error slicer mismatch with known values during "
						"2D roi" << endl;
					return -1;
				}
				++slicer;
			}
		}
	}

	slicer.setROI(1, roiz, roi1);
	slicer.goBegin();
	for(int64_t xx=3; xx<=7; xx++) {
		for(int64_t yy=0; yy<10; yy++) {
			for(int64_t zz=0; zz<10; zz++) {
				if(100*xx+10*yy+zz != *slicer) {
					cerr << "Error slicer mismatch with known values during "
						"1D roi" << endl;
					return -1;
				}
				++slicer;
			}
		}
	}

	slicer.setOrder({0,1,2});
	slicer.goBegin();
	for(int64_t zz=0; zz<10; zz++) {
		for(int64_t yy=0; yy<10; yy++) {
			for(int64_t xx=3; xx<=7; xx++) {
				if(100*xx+10*yy+zz != *slicer) {
					cerr << "Error slicer mismatch with known values during "
						"1D ROI, when order changed" << endl;
					return -1;
				}
				++slicer;
			}
		}
	}

	slicer.setROI(3, roiz, roi1);
	slicer.goBegin();
	for(int64_t zz=4; zz<=6; zz++) {
		for(int64_t yy=5; yy<=8; yy++) {
			for(int64_t xx=3; xx<=7; xx++) {
				if(100*xx+10*yy+zz != *slicer) {
					cerr << "Error slicer mismatch with known values during "
						"3D roi, after changing order" << endl;
					return -1;
				}
				++slicer;
			}
		}
	}

	cerr << "Success!" << endl;
	return 0;
}

