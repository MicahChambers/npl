/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * NPL is free software: you can redistribute it and/or modify it under the
 * terms of the BSD 2-Clause License available in LICENSE or at
 * http://opensource.org/licenses/BSD-2-Clause
 *
 * @file nifti_test1.cpp
 *
 *****************************************************************************/

#include <iostream>
#include <memory>

#include "mrimage.h"
#include "accessors.h"
#include "nplio.h"

using namespace std;
using namespace npl;

int main()
{
	std::map<int64_t,double> slice_timing;
	Matrix3d direction;
    direction << .36,.48,-.8,-.8,.6,0,.48,.64,.6;

    Vector3d spacing;
    spacing << 1.1,1.2,1.3;

    Vector3d origin;
    origin << 12,32,-3;

	{
	/* Create an image with: 0.5+x+y*100+z*10000*/
	auto dblversion = createMRImage({10, 23, 39}, FLOAT64);
	NDView<double> acc(dblversion);

	for(int64_t zz=0; zz < dblversion->dim(2); zz++) {
		for(int64_t yy=0; yy < dblversion->dim(1); yy++) {
			for(int64_t xx=0; xx < dblversion->dim(0); xx++) {
				double pix = xx+yy*100+zz*10000+.5;
				acc.set({xx,yy,zz}, pix);
			}
		}
	}

	dblversion->m_freqdim = 1;
	dblversion->m_phasedim = 0;
	dblversion->m_slicedim = 2;
	dblversion->updateSliceTiming(.01, 0, 38, SEQ);
	slice_timing = dblversion->m_slice_timing;
	dblversion->setOrient(origin, spacing, direction, 1);

	/* Write the Image */
	dblversion->write("test1a.nii.gz", false);

	/* Create a Cast Copy and write */
	auto intversion = dblversion->copyCast(dblversion->ndim(),
			dblversion->dim(), INT64);
	dynamic_pointer_cast<MRImage>(intversion)->write("test1b.nii.gz");

	auto floatversion = intversion->copyCast(FLOAT32);
	dynamic_pointer_cast<MRImage>(intversion)->write("test1c.nii.gz");
	}

	/* Read the Image */
	auto dblversion = readMRImage("test1a.nii.gz", true);
	auto intversion = readMRImage("test1b.nii.gz", true);
	auto floatversion = readMRImage("test1c.nii.gz", true);

	NDView<int> v1(dblversion);
	NDView<int> v2(intversion);
	NDView<int> v3(floatversion);
	// check int version vs dbl version
	if(intversion->ndim() != dblversion->ndim())
		return -1;
	if(intversion->dim(0) != dblversion->dim(0) || intversion->dim(1) !=
			dblversion->dim(1) || intversion->dim(2) != dblversion->dim(2)) {
		std::cerr << "Mismatch between dimension of written images!" << std::endl;
		return -1;
	}

	// check int version versus floatversion
	if(intversion->ndim() != floatversion->ndim())
		return -1;
	if(intversion->dim(0) != floatversion->dim(0) || intversion->dim(1) !=
			floatversion->dim(1) || intversion->dim(2) != floatversion->dim(2)) {
		std::cerr << "Mismatch between dimension of written images!" << std::endl;
		return -1;
	}

	/* Check the Image */
	for(int64_t zz=0; zz < dblversion->dim(2); zz++) {
		for(int64_t yy=0; yy < dblversion->dim(1); yy++) {
			for(int64_t xx=0; xx < dblversion->dim(0); xx++) {
				if(v1[{xx,yy,zz}] != v2[{xx,yy,zz}]) {
					cerr << "Int/Double Mismatch!" << endl;
					return -1;
				}
				if(v1[{xx,yy,zz}] != v3[{xx,yy,zz}]) {
					cerr << "Int/Float Mismatch!" << endl;
					return -1;
				}
			}
		}
	}

	// check metadata
	if(dblversion->m_freqdim != 1) {
		cerr << "Wrong Frequency Dim!" << endl;
		return -1;
	}
	if(dblversion->m_phasedim != 0) {
		cerr << "Wrong Phase Dim!" << endl;
		return -1;
	}
	if(dblversion->m_slicedim!= 2) {
		cerr << "Wrong Slice Dim!" << endl;
		return -1;
	}

	if(fabs(dblversion->m_slice_duration - 0.01) > 1e-5) {
		cerr << "Wrong Slice Duration!" << endl;
		cerr << dblversion->m_slice_duration << " vs " << 0.01 << endl;
		return -1;
	}
	if(dblversion->m_slice_start != 0) {
		cerr << "Wrong Slice Start!" << endl;
		return -1;
	}
	if(dblversion->m_slice_end!= 38) {
		cerr << "Wrong Slice End!" << endl;
		return -1;
	}
	if(dblversion->m_slice_order!= SEQ) {
		cerr << "Wrong Slice Order!" << endl;
		return -1;
	}

	// be more course now
	if(dblversion->m_freqdim != 1 ||  dblversion->m_phasedim != 0 ||
			dblversion->m_slicedim != 2 ||
			fabs(dblversion->m_slice_duration-0.01) > 1e-5 ||
			dblversion->m_slice_start != 0 ||  dblversion->m_slice_end != 38 ||
			dblversion->m_slice_order != SEQ) {
		cerr << "Difference in double image metadata!" << endl;
		return -1;
	}

	if(floatversion->m_freqdim != 1 ||  floatversion->m_phasedim != 0 ||
			floatversion->m_slicedim != 2 ||
			fabs(floatversion->m_slice_duration-0.01) > 1e-5 ||
			floatversion->m_slice_start != 0 ||  floatversion->m_slice_end != 38 ||
			floatversion->m_slice_order != SEQ) {
		cerr << "Difference in float image metadata!" << endl;
		return -1;
	}

	if(intversion->m_freqdim != 1 ||  intversion->m_phasedim != 0 ||
			intversion->m_slicedim != 2 ||
			fabs(intversion->m_slice_duration-0.01) > 1e-5 ||
			intversion->m_slice_start != 0 ||  intversion->m_slice_end != 38 ||
			intversion->m_slice_order != SEQ) {
		cerr << "Difference in int image metadata!" << endl;
		return -1;
	}

	if(slice_timing.size() != dblversion->m_slice_timing.size()) {
		cerr << "Slice Timing Size Mismatch in double image" << endl;
		return -1;
	}
	if(slice_timing.size() != floatversion->m_slice_timing.size()) {
		cerr << "Slice Timing Size Mismatch in float image" << endl;
		return -1;
	}
	if(slice_timing.size() != intversion->m_slice_timing.size()) {
		cerr << "Slice Timing Size Mismatch in int image" << endl;
		return -1;
	}

	for(auto it = slice_timing.begin(); it != slice_timing.end(); ++it) {
		if(fabs(it->second - dblversion->m_slice_timing[it->first]) > 1e-5) {
			cerr << "Timing Mismatch in double image" << endl;
			return -1;
		}
		if(fabs(it->second - intversion->m_slice_timing[it->first]) > 1e-5) {
			cerr << "Timing Mismatch in int image" << endl;
			return -1;
		}
		if(fabs(it->second - floatversion->m_slice_timing[it->first]) > 1e-5) {
			cerr << "Timing Mismatch in float image" << endl;
			return -1;
		}
	}

	double diff = 0;
	for(size_t ii=0; ii<spacing.rows(); ii++){
		diff += pow(spacing(ii) - dblversion->spacing(ii),2);
	}
	if(diff > 1e-5) {
		cerr << "Diffrence in Spacng!" << endl;
		cerr << "dblversion loaded:" << endl << dblversion->getSpacing() << endl;
		cerr << "Original:" << endl << spacing << endl;
		return -1;
	}

	diff=0;
	for(size_t ii=0; ii<origin.rows(); ii++){
		diff += pow(origin(ii) - dblversion->origin(ii),2);
	}
	if(diff > 1e-5){
		cerr << "Diffrence in Origin!" << endl;
		return -1;
	}

	diff=0;
	for(size_t ii=0; ii<direction.rows(); ii++){
		for(size_t jj=0; jj<direction.cols(); jj++){
			diff += pow(direction(ii,jj) - dblversion->direction(ii,jj),2);
		}
	}
	if(diff > 1e-5) {
		cerr << "Difference in Direction!" << endl;
		cerr << "Original: " << direction << "\nvs.\n" <<
			dblversion->getDirection() << endl;
		return -1;
	}

	return 0;
}


