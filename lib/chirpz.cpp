/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * NPL is free software: you can redistribute it and/or modify it under the
 * terms of the BSD 2-Clause License available in LICENSE or at
 * http://opensource.org/licenses/BSD-2-Clause
 *
 * @file chirpz.cpp Functions for performing the chirpz transform
 *
 *****************************************************************************/

#include "chirpz.h"

#include <cmath>
#include <complex>
#include <iostream>
#include <algorithm>
#include <stdexcept>

#include "fftw3.h"
#include "basic_functions.h"
#include "basic_plot.h"

using namespace std;
//#define DEBUG

namespace npl {

/**
 * @brief Interpolate the input array, filling the output array
 *
 * @param isize 	Size of in
 * @param in 		Values to interpolate
 * @param osize 	Size of out
 * @param out 		Output array, filled with interpolated values of in
 */
void interp(int64_t isize, fftw_complex* in, int64_t osize, fftw_complex* out)
{
	// fill/average pad
	int64_t radius = 3;
	double ratio = (double)(isize)/(double)osize;

	// copy/center
	for(size_t oo=0; oo<osize; oo++) {
		double cii = ratio*oo;
		int64_t center = round(cii);

		complex<double> sum = 0;
		for(int64_t ii=center-radius; ii<=center+radius; ii++) {
			if(ii>=0 && ii<isize) {
				complex<double> tmp(in[ii][0], in[ii][1]);
				sum += lanczosKern(ii-cii, radius)*tmp;
			}
		}
		out[oo][0] = sum.real();
		out[oo][1] = sum.imag();
	}
}

/**
 * @brief Fills the input array (chirp) with a chirp of the specified type
 *
 * @param sz 		Size of output array
 * @param chirp 	Output array
 * @param origsz 	Original size, decides maximum frequency reached
 * @param upratio 	Ratio of upsampling performed. This may be different than
 * 					sz/origsz
 * @param alpha 	Positive term in exp
 * @param fft 		Whether to fft the output (put it in frequency domain)
 */
void createChirp(int64_t sz, fftw_complex* chirp, int64_t origsz,
		double upratio, double alpha, bool center, bool fft)
{
//	assert(sz%2==1);
	const double PI = acos(-1);
	const complex<double> I(0,1);

	auto fwd_plan = fftw_plan_dft_1d((int)sz, chirp, chirp, FFTW_FORWARD,
				FFTW_MEASURE | FFTW_PRESERVE_INPUT);

	for(int64_t ii=0; ii<sz; ii++) {
		double xx = 0;
		if(center)
			xx = (ii-sz/2.)/upratio;
		else
			xx = ii/upratio;
		auto tmp = std::exp(-I*PI*alpha*xx*xx/(double)origsz);
		chirp[ii][0] = tmp.real();
		chirp[ii][1] = tmp.imag();
	}

	if(fft) {
		fftw_execute(fwd_plan);
		double norm = 1./sz;
		for(size_t ii=0; ii<sz; ii++) {
			chirp[ii][0] *= norm;
			chirp[ii][1] *= norm;
		}
	}

	fftw_destroy_plan(fwd_plan);
}

/**
 * @brief Performs chirpz transform with a as fractional parameter by N^2
 * algorithm.
 *
 * @param isize Length of input array
 * @param usize Length of upsampled array
 * @param inout Input/Output Array (length = isize)
 * @param buffer Buffer to perform upsampling in. Should be size of usize
 * @param alpha	Fraction/Alpha To raise exp() term to
 * @param debug Whether to write out debug information (including plots!)
 */
void chirpzFT_brute2(size_t isize, size_t usize, fftw_complex* inout,
		fftw_complex* buffer, double alpha, bool debug)
{
	const complex<double> I(0,1);
	const double PI = acos(-1);

	// zero
	for(size_t ii=0; ii<usize; ii++) {
		buffer[ii][0] = 0;
		buffer[ii][1] = 0;
	}

	fftw_complex* upsampled = &buffer[0];

	if(debug) {
		writePlotReIm("brute2_in.svg", isize, inout);
	}
	// upsample input
	interp(isize, inout, usize, upsampled);

	if(debug) {
		writePlotReIm("brute2_upin.svg", usize, upsampled);
	}

	std::vector<complex<double>> workspace(usize);

	// pre-multiply
	for(int64_t nn = 0; nn<usize; nn++) {
		complex<double> tmp1(upsampled[nn][0], upsampled[nn][1]);
		double freq = (double)nn*isize/usize; // go to isize frequency, not usize
		double pos = (double)nn/usize; // go to isize frequency, not usize
		complex<double> tmp2 = exp(-PI*I*freq*pos*alpha);
		tmp1 *= tmp2;
		upsampled[nn][0] = tmp1.real();
		upsampled[nn][1] = tmp1.imag();
	}

	if(debug) {
		writePlotReIm("brute2_premult.svg", usize, upsampled);
	}

	/*
	 * convolve
	 */
	double normfactor = (double)isize/(double)usize;
	for(int64_t ii=0; ii<usize; ii++) {
		double ff = (double)ii - usize/2.;
		workspace[ii] = 0;
		for(int64_t jj=0; jj<usize; jj++) {
			double xx = jj;
			complex<double> tmp1(upsampled[jj][0], upsampled[jj][1]);
			double freq = isize*(ff-xx)/usize;
			double pos = (ff-xx)/usize;
			complex<double> tmp2 = exp(PI*I*freq*pos*alpha);
			workspace[ii] += tmp1*tmp2;
		}
		workspace[ii] *= normfactor;
	}

	if(debug) {
		writePlotReIm("brute2_convolve.svg", workspace);
	}

	for(int64_t nn = 0; nn<usize; nn++) {
		double ff = (double)nn - usize/2.;
		double freq = (double)ff*isize/usize; // go to isize frequency, not usize
		double pos = (double)ff/usize; // go to isize frequency, not usize
		complex<double> tmp = exp(-PI*I*freq*pos*alpha);
		tmp *= workspace[nn];
		upsampled[nn][0] = tmp.real();
		upsampled[nn][1] = tmp.imag();
	}

	if(debug) {
		writePlotReIm("brute2_postmult.svg", usize, upsampled);
	}

	interp(usize, upsampled, isize, inout);

	if(debug) {
		writePlotReIm("brute2_out.svg", isize, inout);
	}
}

/**
 * @brief Comptues the chirpzFFT transform using FFTW for n log n performance.
 *
 * @param isize Size of input/output
 * @param in Input array, may be the same as out, length sz
 * @param out Output array, may be the same as input, length sz
 * @param a Fraction of full space to compute
 * @param debug Write out diagnostic plots
 */
void chirpzFT_brute2(size_t isize, fftw_complex* in, fftw_complex* out, double a,
		bool debug)
{
	// there are 3 sizes: isize: the original size of the input array, usize :
	// the size of the upsampled array, and uppadsize the padded+upsampled
	// size, we want both uppadsize and usize to be odd, and we want uppadsize
	// to be the product of small primes (3,5,7)
	double approxratio = 1;
	int64_t usize = round2(isize*approxratio);

	size_t bsz = isize+usize;
	auto buffer = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*bsz);
	fftw_complex* current = &buffer[0];

	// copy input to buffer, must shift because of chirp being centered
//	std::rotate_copy(&in[0][0], &in[isize/2][0], &in[isize][0], &current[0][0]);
	std::copy(&in[0][0], &in[isize][0], &current[0][0]);

	chirpzFT_brute2(isize, usize, current, &buffer[isize], a, debug);

	// copy current to output
	for(size_t ii=0; ii<isize; ii++) {
		out[ii][0] = current[ii][0];
		out[ii][1] = current[ii][1];
	}

	fftw_free(buffer);
}

/**
 * @brief Comptues the chirpzFFT transform by interpolating a pre-computed
 * space line. Note 0 <= a <= 1, to choose positive versus negative you must
 * do a forward (negative) or backward (positive) FFT first
 *
 * @param isize Size of input/output (Should be in fourier space)
 * @param in Input array, may be the same as out, length sz
 * @param out Output array, must not be the same as input, length sz
 * @param a Fraction of full space to compute
 */
void zoom(size_t isize, fftw_complex* in, fftw_complex* out, double a)
{
	if(in == out)
		throw std::invalid_argument("Input/Output cannot be equal");
	if(a < -1 || a > 1)
		throw std::invalid_argument("Zoom (a) must satisfy: 0 <= a <= 1");

	// fill/average pad
	int64_t radius = 4;

	// copy/center
	for(size_t oo=0; oo<isize; oo++) {
//		double cii = (oo-((isize-1.)/2.))*a+(isize-1.)/2.;
		double cii = (oo-(isize/2.))*a+isize/2.;
		int64_t center = round(cii);

		complex<double> sum = 0;
		for(int64_t ii=center-radius; ii<=center+radius; ii++) {
			if(ii>=0 && ii<isize) {
				complex<double> tmp(in[ii][0], in[ii][1]);
				sum += lanczosKern(ii-cii, radius)*tmp;
			}
		}
		out[oo][0] = sum.real();
		out[oo][1] = sum.imag();
	}
}

/**
 * @brief Performs the chirpz-transform using linear intpolation and the FFT.
 * For greater speed pre-allocate the buffer and use the other chirpzFT_zoom
 *
 * @param isize Input size
 * @param in	Input line
 * @param out	Output line
 * @param a		Zoom factor (alpha)
 */
void chirpzFT_zoom(size_t isize, fftw_complex* in, fftw_complex* out,
		double a)
{
 	auto buffer = (fftw_complex* )fftw_malloc(sizeof(fftw_complex)*isize);
	chirpzFT_zoom(isize, in, out, buffer, a);
 	fftw_free(buffer);
}

/**
 * @brief Performs the chirpz-transform using linear intpolation and teh FFT.
 *
 * @param isize 	Input size
 * @param in		Input line
 * @param out		Output line
 * @param buffer	Buffer, should be size isize
 * @param a			Zoom factor (alpha)
 */
void chirpzFT_zoom(size_t isize, fftw_complex* in, fftw_complex* out,
		fftw_complex* buffer, double a)
{
	if(a > 1 || a < -1)
		throw std::invalid_argument("Zoom (a) must satisfy: -1 <= a <= 1");
	auto dir = a < 0 ? FFTW_FORWARD : FFTW_BACKWARD;

	fftw_plan plan = fftw_plan_dft_1d(isize, buffer, buffer, dir, FFTW_MEASURE);
	for(size_t ii=0; ii<isize; ii++) {
		buffer[ii][0] = in[ii][0];
		buffer[ii][1] = in[ii][1];
	}

	fftw_execute(plan);

	// normalize
	double norm = 1./isize;
	for(size_t ii=0; ii<isize; ii++) {
		buffer[ii][0] *= norm;
		buffer[ii][1] *= norm;
	}

	// rotate, making 0 frequency in the middle
	std::rotate(&buffer[0][0], &buffer[isize/2][0], &buffer[isize][0]);
	zoom(isize, buffer, out, fabs(a));

	fftw_destroy_plan(plan);
}


/**
 * @brief Comptues the chirpzFFT transform using FFTW for n log n performance.
 *
 * @param isize Size of input/output
 * @param in Input array, may be the same as out, length sz
 * @param out Output array, may be the same as input, length sz
 * @param a Fraction of full space to compute
 * @param debug Write out diagnostic plots
 */
void chirpzFFT(size_t isize, fftw_complex* in, fftw_complex* out, double a,
		bool debug)
{
	// there are 3 sizes: isize: the original size of the input array, usize :
	// the size of the upsampled array, and uppadsize the padded+upsampled
	// size, we want both uppadsize and usize to be odd, and we want uppadsize
	// to be the product of small primes (3,5,7)
	double approxratio = 1;
	int64_t usize = round2(isize*approxratio);
	int64_t uppadsize = usize*4;
	double upratio = (double)usize/(double)isize;

	size_t bsz = isize+4*uppadsize;
	auto buffer = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*bsz);
	auto current = &buffer[0];
	auto prechirp = &buffer[isize+uppadsize];
	auto postchirp = &buffer[isize+2*uppadsize];
	auto convchirp = &buffer[isize+3*uppadsize];

	createChirp(uppadsize, prechirp, isize, upratio, a, false, false);
	createChirp(uppadsize, postchirp, isize, upratio, a, true, false);
	createChirp(uppadsize, convchirp, isize, upratio, -a, true, true);

	// copy input to buffer
	std::copy(&in[0][0], &in[isize][0], &current[0][0]);

	chirpzFFT(isize, usize, current, uppadsize, &buffer[isize],
			prechirp, convchirp, postchirp, debug);

	// copy current to output
	for(size_t ii=0; ii<isize; ii++) {
		out[ii][0] = current[ii][0];
		out[ii][1] = current[ii][1];
	}

	fftw_free(buffer);
}

/**
 * @brief Comptues the chirpzFFT transform using FFTW for n log n performance.
 *
 * This version needs for chirps to already been calculated. This is useful
 * if you are running a large number of inputs with the same alpha
 *
 * @param isize 	Size of input/output
 * @param usize 	Size that we should upsample input to
 * @param inout		Input array, length sz
 * @param uppadsize	Padded+upsampled array size
 * @param buffer	Complex buffer used for upsampling, size = uppadsize
 * @param prechirp  Pre-multiplication chirp (see other chirpzFFT functionfor
 *                  how it might be generated)
 * @param convchirp Chirp in frequency domain which will be convolved with the
 *                  prechirp-multiplied input data
 * @param postchirp Same as prechirp, but shifted
 * @param debug     Write out diagnostic plots
 */
void chirpzFFT(size_t isize, size_t usize, fftw_complex* inout,
		size_t uppadsize, fftw_complex* buffer, fftw_complex* prechirp,
		fftw_complex* convchirp, fftw_complex* postchirp, bool debug)
{
	const complex<double> I(0,1);

	// zero
	for(size_t ii=0; ii<uppadsize; ii++) {
		buffer[ii][0] = 0;
		buffer[ii][1] = 0;
	}

	fftw_complex* sigbuff = &buffer[0]; // note the overlap with upsampled
	fftw_complex* upsampled = &buffer[uppadsize/2-usize/2];

	fftw_plan sigbuff_plan_fwd = fftw_plan_dft_1d(uppadsize, sigbuff, sigbuff,
			FFTW_FORWARD, FFTW_MEASURE);
	fftw_plan sigbuff_plan_rev = fftw_plan_dft_1d(uppadsize, sigbuff, sigbuff,
			FFTW_BACKWARD, FFTW_MEASURE);

	if(debug) {
		writePlotReIm("fft_prechirp.svg", uppadsize, prechirp);
		writePlotReIm("fft_postchirp.svg", uppadsize, postchirp);
		writePlotReIm("fft_convchirp.svg", uppadsize, convchirp);
		writePlotReIm("fft_in.svg", isize, inout);
	}
	// upsample input
	interp(isize, inout, usize, upsampled);

	if(debug) {
		writePlotReIm("fft_upin.svg", usize, upsampled);
	}

	// pre-multiply
	for(int64_t nn = 0; nn<usize; nn++) {
		complex<double> tmp1(prechirp[nn][0], prechirp[nn][1]);
		complex<double> tmp2(upsampled[nn][0], upsampled[nn][1]);
		tmp1 *= tmp2;
		upsampled[nn][0] = tmp1.real();
		upsampled[nn][1] = tmp1.imag();
	}

	if(debug) {
		writePlotReIm("fft_premult.svg", usize, upsampled);
	}

	/*
	 * convolve
	 */
	fftw_execute(sigbuff_plan_fwd);
	double normfactor = (double)isize/(usize*uppadsize);
	for(size_t ii=0; ii<uppadsize; ii++) {
		sigbuff[ii][0] *= normfactor;
		sigbuff[ii][1] *= normfactor;
	}

	for(size_t ii=0; ii<uppadsize; ii++) {
		complex<double> tmp1(sigbuff[ii][0], sigbuff[ii][1]);
		complex<double> tmp2(convchirp[ii][0], convchirp[ii][1]);
		tmp1 *= tmp2;
		sigbuff[ii][0] = tmp1.real();
		sigbuff[ii][1] = tmp1.imag();
	}
	fftw_execute(sigbuff_plan_rev);

	normfactor = uppadsize;
	for(size_t ii=0; ii<uppadsize; ii++) {
		sigbuff[ii][0] *= normfactor;
		sigbuff[ii][1] *= normfactor;
	}

	if(debug) {
		writePlotReIm("fft_convolve.svg", uppadsize, sigbuff);
	}

	// circular shift
	std::rotate(&sigbuff[0][0], &sigbuff[uppadsize/2-usize/2][0],
			&sigbuff[uppadsize][0]);
	if(debug) {
		writePlotReIm("fft_rotated.svg", uppadsize, sigbuff);
	}

	for(int64_t nn = 0; nn<usize; nn++) {
		complex<double> tmp1(postchirp[nn+uppadsize/2-usize/2][0],
				postchirp[nn+uppadsize/2-usize/2][1]);
		complex<double> tmp2(upsampled[nn][0], upsampled[nn][1]);
		tmp1 *= tmp2;
		upsampled[nn][0] = tmp1.real();
		upsampled[nn][1] = tmp1.imag();
	}


	if(debug) {
		writePlotReIm("fft_postmult.svg", uppadsize, sigbuff);
	}

	interp(usize, upsampled, isize, inout);

	if(debug) {
		writePlotReIm("fft_out.svg", isize, inout);
	}

	fftw_destroy_plan(sigbuff_plan_rev);
	fftw_destroy_plan(sigbuff_plan_fwd);
}

/**
 * @brief Performs chirpz transform with a as fractional parameter by N^2
 * algorithm.
 *
 * @param len	Length of input array
 * @param in	Input Array (length = len)
 * @param out	Output Array (length = len)
 * @param a		Fraction/Alpha To raise exp() term to
 */
void chirpzFT_brute(size_t len, fftw_complex* in, fftw_complex* out, double a)
{
	const complex<double> I(0,1);
	const double PI = acos(-1);
	int64_t ilen = len;

	for(int64_t ii=0; ii<ilen; ii++) {
		double ff=(ii-(ilen)/2.);
		out[ii][0]=0;
		out[ii][1]=0;

		for(int64_t jj=0; jj<ilen; jj++) {
			complex<double> tmp1(in[jj][0], in[jj][1]);
			complex<double> tmp2 = tmp1*std::exp(-2.*PI*I*a*(double)jj*ff/(double)ilen);

			out[ii][0] += tmp2.real();
			out[ii][1] += tmp2.imag();
		}
	}
}

/**
 * @brief Plots an array of complex points with the Real and Imaginary Parts
 *
 * @param file	Filename
 * @param in	Array input
 */
void writePlotReIm(std::string file, const std::vector<complex<double>>& in)
{
	std::vector<double> realv(in.size());
	std::vector<double> imv(in.size());
	for(size_t ii=0; ii<in.size(); ii++) {
		realv[ii] = in[ii].real();
		imv[ii] = in[ii].imag();
	}

	Plotter plt;
	plt.addArray(in.size(), realv.data());
	plt.addArray(in.size(), imv.data());
	plt.write(file);
}

/**
 * @brief Plots an array of complex points with the Real and Imaginary Parts
 *
 * @param file	Filename
 * @param insz	Size of in
 * @param in	Array input
 */
void writePlotReIm(std::string file, size_t insz, fftw_complex* in)
{
	std::vector<double> realv(insz);
	std::vector<double> imv(insz);
	for(size_t ii=0; ii<insz; ii++) {
		realv[ii] = in[ii][0];
		imv[ii] = in[ii][1];
	}

	Plotter plt;
	plt.addArray(insz, realv.data());
	plt.addArray(insz, imv.data());
	plt.write(file);
}

/**
 * @brief Plots an array of complex points with the Real and Imaginary Parts
 *
 * @param file	Filename
 * @param insz	Size of in
 * @param in	Array input
 */
void writePlotAbsAng(std::string file, size_t insz, fftw_complex* in)
{
	double phasemax = -INFINITY;
	double phasemin = INFINITY;
	double absmax = -INFINITY;
	double absmin = INFINITY;
	std::vector<double> absv(insz);
	std::vector<double> angv(insz);
	for(size_t ii=0; ii<insz; ii++) {
		angv[ii] = atan2(in[ii][0], in[ii][1]);
		absv[ii] = sqrt(pow(in[ii][0],2)+pow(in[ii][1],2));
		phasemax = std::max(phasemax, angv[ii]);
		absmax = std::max(absmax, absv[ii]);
		phasemin = std::min(phasemin, angv[ii]);
		absmin = std::min(absmin, absv[ii]);
	}

	Plotter plt;
	plt.addArray(insz, absv.data());
	plt.addArray(insz, angv.data());
	plt.write(file);
}

}

