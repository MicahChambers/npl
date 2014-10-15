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
 * @file fft_rotate_test.cpp A test of the shear/fourier shift based rotation
 * function
 *
 *****************************************************************************/

#include <string>
#include <stdexcept>

#include <Eigen/Geometry> 

#include "mrimage.h"
#include "mrimage_utils.h"
#include "utility.h"
#include "ndarray_utils.h"
#include "iterators.h"
#include "accessors.h"

using namespace npl;
using namespace std;
using Eigen::Matrix3d;
using Eigen::Vector3d;
using Eigen::AngleAxisd;

int closeCompare(shared_ptr<const MRImage> a, shared_ptr<const MRImage> b, 
        double thresh = .01)
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

	OrderConstIter<double> ita(a);
	OrderConstIter<double> itb(b);
	itb.setOrder(ita.getOrder());
	for(ita.goBegin(), itb.goBegin(); !ita.eof() && !itb.eof(); ++ita, ++itb) {
		double diff = fabs(*ita - *itb);
		if(diff > thresh) {
			cerr << "Images differ by " << diff << endl;
			return -1;
		}
	}

	return 0;
}

int main()
{
	// create an image
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

	in->write("original.nii.gz");

    const double Rx = -3.14159/7;
    const double Ry = 3.14159/4;
    const double Rz = 3.14159/10;

    // linear ntperolation
	cerr << "!Rotating manually" << endl;
	clock_t c = clock();
	auto out1 = dynamic_pointer_cast<MRImage>(linearRotate(Rx, Ry, Rz, in));
	c = clock() - c;
	cerr << "!Linear Rotate took " << c/(double)CLOCKS_PER_SEC << "s" << endl;
	out1->write("brute_rotated.nii.gz");
	cerr << "Done" << endl;
	
    // Shear/FFT Rotation 
	cerr << "!Rotating with shears/FFT" << endl;
    c = clock();
    auto out2 = dynamic_pointer_cast<MRImage>(in->copy());
	rotateImageShearFFT(out2, Rx, Ry, Rz);
	c = clock() - c;
	out2->write("fft_rotated.nii.gz");
	cerr << "!Shear FFT Rotate took " << c/(double)CLOCKS_PER_SEC << "s" << endl;
	
    // Shear/FFT Rotation  using the rectangular window
    cerr << "!Rotating with shears/FFT/Rect Window" << endl;
    c = clock();
    auto out3 = dynamic_pointer_cast<MRImage>(in->copy());
	rotateImageShearFFT(out3, Rx, Ry, Rz, npl::rectWindow);
	c = clock() - c;
	out3->write("fft_rect_rotated.nii.gz");
	cerr << "!Shear FFT Rect Rotate took " << c/(double)CLOCKS_PER_SEC << "s" << endl;
	
    cerr << "!Rotating with shears/kernel" << endl;
    c = clock();
    auto out4 = dynamic_pointer_cast<MRImage>(in->copy());
	rotateImageShearKern(out4, Rx, Ry, Rz);
	c = clock() - c;
	out4->write("kshear_rotated.nii.gz");
	cerr << "!Shear Kernel Rotate took " << c/(double)CLOCKS_PER_SEC << "s" << endl;
    
    cerr << "Testing Linear/Sinc Window FFT" << endl;
    if(closeCompare(out1, out2, 0.5) != 0)
        return -1;
    cerr << "Testing Linear/Kernel Shearing" << endl;
    if(closeCompare(out1, out4, 0.5) != 0)
        return -1;
    cerr << "Testing Linear/Rectangular Window FFT" << endl;
    if(closeCompare(out1, out3, 0.5) != 0)
        return -1;

	return 0;
}


