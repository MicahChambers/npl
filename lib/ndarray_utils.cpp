/*******************************************************************************
This file is part of Neuro Programs and Libraries (NPL), 

Written and Copyrighted by by Micah C. Chambers (micahc.vt@gmail.com)

The Neuro Programs and Libraries are free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License as published
by the Free Software Foundation, either version 3 of the License, or (at your
option) any later version.

The Neural Programs and Libraries are distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with
the Neural Programs Library.  If not, see <http://www.gnu.org/licenses/>.
*******************************************************************************/

/******************************************************************************
 * @file ndarray_utils.cpp
 * @brief This file contains common functions which are useful for processing
 * of N-dimensional arrays and their derived counterparts (MRImage for
 * example). All of these functions return pointers to NDArray types, however
 * if an image is passed in, then the output will also be an image, you just 
 * need to cast the output using std::dynamic_pointer_cast<MRImage>(out). 
 * mrimage_utils.h is for more specific image-processing algorithm, this if for
 * generally data of any dimension, without regard to orientation.
 ******************************************************************************/

#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include "ndarray_utils.h"
#include "ndarray.h"
#include "npltypes.h"
#include "matrix.h"
#include "iterators.h"
#include "fftw3.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <memory>
#include <stdexcept>

namespace npl {

using std::vector;
using std::shared_ptr;

int hob (int num)
{
	if (!num)
		return 0;

	int ret = 1;

	while (num >>= 1)
		ret <<= 1;

	return ret;
}

int64_t round2(int64_t in)
{
	int64_t just_hob = hob(in);
	if(just_hob == in)
		return in;
	else
		return (in<<1);
}

/**
 * @brief Performs in-place fft. Note that the input should already be padded
 * and a complex type
 *
 * @param in
 * @param dim
 */
//void fft1d(shared_ptr<NDArray> in, size_t dd, bool inverse)
//{
//	// plan and execute 
//	double* indata = fftw_alloc_real(in->dim(dd));
//	fftw_complex *data = fftw_alloc_complex(in->dim(dd));
//	fftw_plan plan;
//	if(inverse) {
//		plan = fftw_plan_dft_1d(in->dim(dd), data, data, FFTW_BACKWARD,
//				FFTW_MEASURE);
//	} else {
//		plan = fftw_plan_dft_1d(in->dim(dd), data, data, FFTW_FORWARD,
//				FFTW_MEASURE);
//	}
//
//	OrderIter<cdouble_t> oit(in);
//	oit.setOrder({dd}); 
//	OrderConstIter<cdouble_t> iit(in);
//	iit.setOrder(oit.getOrder()); // make dd the fastest
//	while(!iit.eof() && !oit.eof()) {
//
//		// fill array
//		for(size_t ii=0; ii<in->dim(dd); ++iit, ii++) {
//			data[ii][0] = (*iit).real();
//			data[ii][1] = (*iit).imag();
//		}
//		
//		// execute
//		fftw_execute(plan);
//		
//		// write array
//		cdouble_t tmp;
//		for(size_t ii=0; ii<in->dim(dd); ++oit, ii++) {
//			tmp.real(data[ii][0]);
//			tmp.real(data[ii][1]);
//			data[ii][0] = (*iit).real();
//			data[ii][1] = (*iit).imag();
//			oit.set(tmp);
//		}
//		
//	}
//
//}

/**
 * @brief Perform fourier transform on the dimensions specified. Those
 * dimensions will be padded out. The output of this will be a double. 
 * If len = 0 or dim == NULL, then ALL dimensions will be transformed.
 *
 * @param in Input image to inverse fourier trnasform
 * @param len length of input dimension array
 * @param dim dimensions to transform
 *
 * @return Image with specified dimensions in the real domain. Image will
 * differ in size from input.
 */
shared_ptr<NDArray> ifft_c2r(shared_ptr<const NDArray> in)
{
	if(in->type() != COMPLEX128 && in->type() != COMPLEX64 && in->type() !=
				COMPLEX256) {
		std::invalid_argument("Input to fft_c2r is not complex!");
	}
	
	size_t ndim = in->ndim();;

	/*
	 * pad out the given dimensions
	 */

	// start with original size and then round up specified dimensions
	cerr << "Input Dimensions: [";
	for(size_t dd=0; dd<in->ndim(); dd++)
		cerr << in->dim(dd) << ",";
	cerr << "]" << endl;
	size_t inpixel = 1;
	size_t outpixel = 1;
	std::vector<int> insize(in->dim(), in->dim()+ndim);
	std::vector<size_t> outsize(in->dim(), in->dim()+ndim);
	for(size_t ii=0; ii<ndim; ii++) {
		insize[ii] = in->dim(ii);
		inpixel *= insize[ii];
		outsize[ii] = insize[ii];
		outpixel *= outsize[ii];
	}

	auto idata = fftw_alloc_complex(inpixel);
	auto odata = fftw_alloc_complex(outpixel);

	// plan
	fftw_plan plan = fftw_plan_dft((int)insize.size(), insize.data(), idata,
				odata, FFTW_FORWARD, FFTW_MEASURE);

	// copy data into idata
	OrderConstIter<cdouble_t> iit(in);
	for(size_t ii=0; !iit.eof(); ++iit, ii++) {
		idata[ii][0] = (*iit).real();
		idata[ii][1] = (*iit).imag();
	}

	// fourier transform
	fftw_execute(plan);
	fftw_free(idata);

	// copy data out
	auto out = in->copyCast(outsize.size(), outsize.data(), FLOAT64);

	OrderIter<cdouble_t> oit(out);
	cdouble_t tmp;
	for(size_t ii=0; !oit.eof(); ii++, ++oit) {
		tmp.real(odata[ii][0]);
		tmp.imag(odata[ii][1]);
		oit.set(tmp);;
	}

	cerr << "Out Dimensions: [";
	for(size_t dd=0; dd<out->ndim(); dd++)
		cerr << out->dim(dd) << ",";
	cerr << "]" << endl;
	
	fftw_free(odata);
	return out;
}

/**
 * @brief Perform fourier transform on the dimensions specified. Those
 * dimensions will be padded out. The output of this will be a complex double.
 * If len = 0 or dim == NULL, then ALL dimensions will be transformed.
 *
 * @param in Input image to fourier transform
 *
 * @return Complex image, which is the result of inverse fourier transforming 
 * the (Real) input image. Note that the last dimension only contains the real
 * frequencies, but all other dimensions contain both
 */
shared_ptr<NDArray> fft_r2c(shared_ptr<const NDArray> in)
{
	if(in->type() == COMPLEX128 || in->type() == COMPLEX64 || in->type() ==
				COMPLEX256 || in->type() == RGB24 || in->type() == RGBA32) {
		std::invalid_argument("Input to fft_r2c is not scalar!");
	}
	
	size_t ndim = in->ndim();;

	/*
	 * pad out the given dimensions
	 */

	// start with original size and then round up specified dimensions
	cerr << "Input Dimensions: [";
	for(size_t dd=0; dd<in->ndim(); dd++)
		cerr << in->dim(dd) << ",";
	cerr << "]" << endl;
	size_t inpixel = 1;
	size_t outpixel = 1;
	std::vector<int> padsize(in->dim(), in->dim()+ndim);
	std::vector<size_t> outsize(in->dim(), in->dim()+ndim);
	for(size_t ii=0; ii<ndim; ii++) {
		padsize[ii] = round2(in->dim(ii));
		inpixel *= padsize[ii];
		outsize[ii] = padsize[ii];
		outpixel *= outsize[ii];
	}

	auto idata = fftw_alloc_complex(inpixel);
	auto odata = fftw_alloc_complex(outpixel);

	// plan
	fftw_plan plan = fftw_plan_dft((int)padsize.size(), padsize.data(),
				idata, odata, FFTW_BACKWARD, FFTW_MEASURE);

	// copy data into idata
	OrderConstIter<cdouble_t> iit(in);
	for(size_t ii=0; !iit.eof(); ++iit, ii++) {
		idata[ii][0] = (*iit).real();
		idata[ii][1] = (*iit).imag();
	}

	// fourier transform
	fftw_execute(plan);
	fftw_free(idata);

	// copy data out
	auto out = in->copyCast(outsize.size(), outsize.data(), COMPLEX128);

	OrderIter<cdouble_t> oit(out);
	cdouble_t tmp;
	for(size_t ii=0; !oit.eof(); ii++, ++oit) {
		tmp.real(odata[ii][0]/inpixel);
		tmp.imag(odata[ii][1]/inpixel);
		oit.set(tmp);;
	}

	cerr << "Out Dimensions (with hermitian symmetry): [";
	for(size_t dd=0; dd<out->ndim(); dd++)
		cerr << out->dim(dd) << ",";
	cerr << "]" << endl;
	
	fftw_free(odata);
	return out;
}

//shared_ptr<NDArray> ppfft(shared_ptr<NDArray> in, size_t len, size_t* dims)
//{
//	// compute the 3D foureir transform for the entire image (or at least the
//	// dimensions specified)
//	auto fourier = fft_r2c(in);
//
//	// compute for each fan
//	for(size_t ii=0; ii<len; ii++) {
//
//	}
//}


/**
 * @brief Returns whether two NDArrays have the same dimensions, and therefore
 * can be element-by-element compared/operated on. elL is set to true if left
 * is elevatable to right (ie all dimensions match or are missing or are unary).
 * elR is the same but for the right. 
 *
 * Strictly R is elevatable if all dimensions that don't match are missing or 1
 * Strictly L is elevatable if all dimensions that don't match are missing or 1
 *
 * Examples of *elR = true (return false):
 *
 * left = [10, 20, 1]
 * right = [10, 20, 39]
 *
 * left = [10]
 * right = [10, 20, 39]
 *
 * Examples where neither elR or elL (returns true):
 *
 * left = [10, 20, 39]
 * right = [10, 20, 39]
 *
 * Examples where neither elR or elL (returns false):
 *
 * left = [10, 20, 9]
 * right = [10, 20, 39]
 *
 * left = [10, 1, 9]
 * right = [10, 20, 1]
 *
 * @param left	NDArray input
 * @param right NDArray input
 * @param elL Whether left is elevatable to right (see description of function)
 * @param elR Whether right is elevatable to left (see description of function)
 *
 * @return 
 */
bool comparable(const NDArray* left, const NDArray* right, bool* elL, bool* elR)
{
	bool ret = true;

	bool rightEL = true;
	bool leftEL = true;

	for(size_t ii = 0; ii < left->ndim(); ii++) {
		if(ii < right->ndim()) {
			if(right->dim(ii) != left->dim(ii)) {
				ret = false;
				// if not 1, then R is not elevateable
				if(right->dim(ii) != 1)
					rightEL = false;
			}
		}
	}
	
	for(size_t ii = 0; ii < right->ndim(); ii++) {
		if(ii < left->ndim()) {
			if(right->dim(ii) != left->dim(ii)) {
				ret = false;
				// if not 1, then R is not elevateable
				if(left->dim(ii) != 1)
					leftEL = false;
			}
		}
	}
	
	if(ret) {
		leftEL = false;
		rightEL = false;
	}

	if(elL) *elL = leftEL;
	if(elR) *elR = rightEL;

	return ret;
}

/**
 * @brief Erode an binary array repeatedly
 *
 * @param in Input to erode
 * @param reps Number of radius-1 kernel erosions to perform
 *
 * @return Eroded Image
 */
shared_ptr<NDArray> erode(shared_ptr<NDArray> in, size_t reps)
{
	std::vector<int64_t> index1(in->ndim(), 0);
	std::vector<int64_t> index2(in->ndim(), 0);
	auto prev = in->copy();
	auto out = in->copy();
	for(size_t rr=0; rr<reps; ++rr) {
		std::swap(prev, out);
		
		KernelIter<int> it(prev);
		it.setRadius(1);
		OrderIter<int> oit(out);
		oit.setOrder(it.getOrder());
		// for each pixels neighborhood, smooth neightbors
		for(oit.goBegin(), it.goBegin(); !it.eof(); ++it, ++oit) {
			oit.index(index1.size(), index1.data());
			it.center_index(index2.size(), index2.data());

			// if any of the neighbors are 0, then set to 0
			bool erodeme = false;
			for(size_t ii=0; ii<it.ksize(); ++ii) {
				if(it.offset(ii) == 0) {
					erodeme = true;
				}
			}

			if(erodeme) 
				oit.set(0);
		}
	}

	return out;
}

/**
 * @brief Dilate an binary array repeatedly
 *
 * @param in Input to dilate
 * @param reps Number of radius-1 kernel dilations to perform
 *
 * @return Dilated Image
 */
shared_ptr<NDArray> dilate(shared_ptr<NDArray> in, size_t reps)
{
	std::vector<int64_t> index1(in->ndim(), 0);
	std::vector<int64_t> index2(in->ndim(), 0);
	auto prev = in->copy();
	auto out = in->copy();
	for(size_t rr=0; rr<reps; ++rr) {
		std::swap(prev, out);
		
		KernelIter<int> it(prev);
		it.setRadius(1);
		OrderIter<int> oit(out);
		oit.setOrder(it.getOrder());
		// for each pixels neighborhood, smooth neightbors
		for(oit.goBegin(), it.goBegin(); !it.eof(); ++it, ++oit) {
			oit.index(index1.size(), index1.data());
			it.center_index(index2.size(), index2.data());
			for(size_t ii=0; ii<in->ndim(); ++ii) {
				if(index1[ii] != index2[ii]) {
					throw std::logic_error("Error differece in iteration!");
				}
			}

			// if any of the neighbors are 0, then set to 0
			bool dilateme = false;
			int dilval = 0;
			for(size_t ii=0; ii<it.ksize(); ++ii) {
				if(it.offset(ii) != 0) {
					dilval = it.offset(ii);
					dilateme = true;
				}
			}

			if(dilateme) 
				oit.set(dilval);
		}
	}

	return out;
}


} // npl
#endif  //IMAGE_PROCESSING_H


