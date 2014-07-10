#include <iostream>
#include "mrimage.h"

using namespace std;
using namespace npl;

ostream& operator<<(ostream& os, const std::vector<size_t>& vec);
ostream& operator<<(ostream& os, const std::vector<double>& vec);

int main()
{
	/* Read the Image */
	MRImage* img1 = readMRImage("../../testing/test_nifti3.nii.gz", true);
	if(!img1) {
		std::cerr << "Failed to open image!" << std::endl;
		return -1;
	}

	// write version 2 image
	img1->write("rwrw_test1.nii.gz", 2);
	
	///////////////////////////////////////
	// read version 2 image (be verbose)
	auto img2 = readMRImage("rwrw_test1.nii.gz", true);
	
	// compare metadata
	if(img1->affine().det() != img2->affine().det()) {
		cerr << "Error, mismatch of affine matrices between version1 "
			"and version2" << endl;
	}

	if(img1->m_freqdim != img2->m_freqdim) {
		cerr << "Error, mismatch of frequency encoding info between version1 "
			"and version2" << endl;
	}
	if(img1->m_phasedim != img2->m_phasedim ) {
		cerr << "Error, mismatch of phase encoding info between version1 "
			"and version2" << endl;
	}
	
	if(img1->m_slicedim != img2->m_slicedim) {
		cerr << "Error, mismatch of slice encoding info between version1 "
			"and version2" << endl;
	}
	
	if(img1->m_slice_duration != img2->m_slice_duration) {
		if(img1->m_slice_timing.size() != img2->m_slice_timing.size()) {
			cerr << "Mismatch in slice timing" << endl;
			return -1;
		}

		for(size_t ii=0; ii<img1->m_slice_timing.size(); ii++) {
			if(img1->m_slice_timing[ii] != img2->m_slice_timing[ii]) {
				cerr << "Mismatch in slice timing" << endl;
				return -1;
			}
		}
	}
	
	// write version 1 image
	img2->write("rwrw_test2.nii.gz", 1);
	
	///////////////////////////////////////
	// read version 1 image
	auto img3 = readMRImage("rwrw_test2.nii.gz", true);
	
	// compare metadata
	if(img1->affine().det() != img3->affine().det()) {
		cerr << "Error, mismatch of affine matrices between version1 "
			"and version2" << endl;
	}

	if(img1->m_freqdim != img3->m_freqdim) {
		cerr << "Error, mismatch of frequency encoding info between version1 "
			"and version2" << endl;
	}
	if(img1->m_phasedim != img3->m_phasedim ) {
		cerr << "Error, mismatch of phase encoding info between version1 "
			"and version2" << endl;
	}
	
	if(img1->m_slicedim != img3->m_slicedim) {
		cerr << "Error, mismatch of slice encoding info between version1 "
			"and version2" << endl;
	}
	
	if(img1->m_slice_duration != img3->m_slice_duration) {
		if(img1->m_slice_timing.size() != img3->m_slice_timing.size()) {
			cerr << "Mismatch in slice timing" << endl;
			return -1;
		}

		for(size_t ii=0; ii<img1->m_slice_timing.size(); ii++) {
			if(img1->m_slice_timing[ii] != img3->m_slice_timing[ii]) {
				cerr << "Mismatch in slice timing" << endl;
				return -1;
			}
		}
	}

	return 0;
}

ostream& operator<<(ostream& os, const std::vector<size_t>& vec)
{
	os << "[ ";
	for(auto v : vec) {
		os << std::setw(4) << v << ",";
	}
	os << " ]";
	return os;
}

ostream& operator<<(ostream& os, const std::vector<double>& vec)
{
	os << "[";
	for(auto v : vec) {
		os << std::setw(7) << std::setprecision(2) << std::fixed << v << ",";
	}
	os << "] ";
	return os;
}


