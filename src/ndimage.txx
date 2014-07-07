#include <algorithm>
#include <cstring>
#include <typeinfo>

#include "nifti.h"

namespace npl {

/**
 * @brief Constructor with initializer list
 *
 * @param a_args dimensions of input, the length of this initializer list
 * may not be fully used if a_args is longer than D. If it is shorter
 * then D then additional dimensions are left as size 1.
 */
template <int D,typename T>
NDImageStore<D,T>::NDImageStore(std::initializer_list<size_t> a_args) :
	NDArrayStore<D,T>(a_args)
{
	orientDefault();
}

/**
 * @brief Constructor with array to set size
 *
 * @param dim dimensions of input 
 */
template <int D,typename T>
NDImageStore<D,T>::NDImageStore(size_t dim[D]) : 
	NDArrayStore<D,T>(dim)
{
	orientDefault();
}

/**
 * @brief Constructor with array to set size
 *
 * @param dim dimensions of input 
 */
template <int D,typename T>
NDImageStore<D,T>::NDImageStore(const std::vector<size_t>& dim) : 
	NDArrayStore<D,T>(dim)
{
	orientDefault();
}

/**
 * @brief Default orientation (dir=ident, space=1 and origin=0)
 */
template <int D,typename T>
void NDImageStore<D,T>::orientDefault()
{
	
	for(size_t ii=0; ii<D; ii++) {
		m_space[ii] = 1;
		m_origin[ii] = 0;
		for(size_t jj=0; jj<D; jj++) {
			m_dir[ii*D+jj] = (ii==jj);
		}
	}

	updateAffine();
}

/**
 * @brief Updates index->RAS affine transform cache
 */
template <int D,typename T>
void NDImageStore<D,T>::updateAffine()
{
	// first DxD section
	for(size_t ii=0; ii<D; ii++) {
		for(size_t jj=0; jj<D; jj++) {
			m_affine[ii*(D+1)+jj] = m_dir[ii*D+jj]*m_space[jj];
		}
	}
		
	// bottom row
	for(size_t jj=0; jj<D; jj++) 
		m_affine[D*(D+1)+jj] = 0;
	
	// last column
	for(size_t ii=0; ii<D; ii++) 
		m_affine[ii*(D+1)+D] = 0;

	// bottom right
	m_affine[D*(D+1)+D] = 1;
}

/**
 * @brief Just dump image information
 */
template <int D,typename T>
void NDImageStore<D,T>::printSelf()
{
	std::cerr << D << "D Image\n[";
	for(size_t ii=0; ii<D; ii++)
		std::cerr << dim(ii) << ", ";
	std::cerr << "]\n";

	std::cerr << "Orientation:\nOrigin: [";
	for(size_t ii=0; ii<D; ii++) 
		std::cerr << m_origin[ii] << ", ";
	std::cerr << "\nSpacing: [";
	for(size_t ii=0; ii<D; ii++) 
		std::cerr << m_space[ii] << ", ";
	std::cerr << "\nDirection:\n";
	for(size_t ii=0; ii<D; ii++) {
		std::cerr << "[";
		for(size_t jj=0; jj<D; jj++) 
			std::cerr << m_dir[ii*D+jj] << ", ";
		std::cerr << "]\n";
	}
	std::cerr << "\nAffine:\n";
	for(size_t ii=0; ii<D+1; ii++) {
		std::cerr << "[";
		for(size_t jj=0; jj<D+1; jj++) 
			std::cerr << m_affine[ii*(D+1)+jj] << ", ";
		std::cerr << "]\n";
	}
}

template <typename T, typename DIMT>
NDImage* createNDImage(size_t ndim, DIMT* dims)
{
	const size_t MAXDIM = 7;
	size_t truedim[MAXDIM];
	switch(ndim) {
		case 1:
			for(size_t ii=0; ii<ndim; ii++)
				truedim[ii] = (size_t)dims[ii];
			return NDImageStore<1, T>(truedim);
		case 2:
			for(size_t ii=0; ii<ndim; ii++)
				truedim[ii] = (size_t)dims[ii];
			return NDImageStore<2, T>(truedim);
		case 3:
			for(size_t ii=0; ii<ndim; ii++)
				truedim[ii] = (size_t)dims[ii];
			return NDImageStore<3, T>(truedim);
		case 4:
			for(size_t ii=0; ii<ndim; ii++)
				truedim[ii] = (size_t)dims[ii];
			return NDImageStore<4, T>(truedim);
		case 5:
			for(size_t ii=0; ii<ndim; ii++)
				truedim[ii] = (size_t)dims[ii];
			return NDImageStore<5, T>(truedim);
		case 6:
			for(size_t ii=0; ii<ndim; ii++)
				truedim[ii] = (size_t)dims[ii];
			return NDImageStore<6, T>(truedim);
		case 7:
			for(size_t ii=0; ii<ndim; ii++)
				truedim[ii] = (size_t)dims[ii];
			return NDImageStore<7, T>(truedim);
		default:
			std::cerr << "Unsupported dimension: " << ndim << std::endl;
			return NULL;

	}
	return NULL;
}

template <int D, typename T>
double& NDImageStore<D,T>::space(size_t d) 
{ 
	assert(d<D); 
	return m_space[d]; 
};

template <int D, typename T>
double& NDImageStore<D,T>::origin(size_t d) 
{
	assert(d<D); 
	return m_origin[d]; 
};

template <int D, typename T>
double& NDImageStore<D,T>::direction(size_t d1, size_t d2) 
{
	assert(d1 < D && d2 < D);
	return m_dir[D*d1+d2]; 
};

template <int D, typename T>
double& NDImageStore<D,T>::affine(size_t d1, size_t d2) 
{ 
	assert(d1 < D+1 && d2 < D+1);
	return m_dir[(D+1)*d1+d2]; 
};

template <int D, typename T>
const double& NDImageStore<D,T>::space(size_t d) const 
{ 
	assert(d < D);
	return m_space[d]; 
};

template <int D, typename T>
const double& NDImageStore<D,T>::origin(size_t d) const 
{
	assert(d < D);
	return m_origin[d]; 
};

template <int D, typename T>
const double& NDImageStore<D,T>::direction(size_t d1, size_t d2) const 
{ 
	assert(d1 < D && d2 < D);
	return m_dir[D*d1+d2]; 
};

template <int D, typename T>
const double& NDImageStore<D,T>::affine(size_t d1, size_t d2) const 
{
	assert(d1 < D+1 && d2 < D+1);
	return m_dir[(D+1)*d1+d2]; 
};

/* 
 * type() Function specialized for all available types
 */

template <int D, typename T>
PixelT NDImageStore<D,T>::type() const 
{
	if(typeid(T) == typeid(uint8_t)) 
		return UINT8;
	else if(typeid(T) == typeid(int8_t)) 
		return INT8;
	else if(typeid(T) == typeid(uint16_t)) 
		return UINT16;
	else if(typeid(T) == typeid(int16_t)) 
		return INT16;
	else if(typeid(T) == typeid(uint32_t)) 
		return UINT32;
	else if(typeid(T) == typeid(int32_t)) 
		return INT32;
	else if(typeid(T) == typeid(uint64_t)) 
		return UINT64;
	else if(typeid(T) == typeid(int64_t)) 
		return INT64;
	else if(typeid(T) == typeid(float)) 
		return FLOAT32;
	else if(typeid(T) == typeid(double)) 
		return FLOAT64;
	else if(typeid(T) == typeid(long double)) 
		return FLOAT128;
	else if(typeid(T) == typeid(cfloat_t)) 
		return COMPLEX64;
	else if(typeid(T) == typeid(cdouble_t)) 
		return COMPLEX128;
	else if(typeid(T) == typeid(cquad_t)) 
		return COMPLEX256;
	else if(typeid(T) == typeid(rgb_t)) 
		return RGB24;
	else if(typeid(T) == typeid(rgba_t)) 
		return RGBA32;
	return UNKNOWN_TYPE;
}

#undef TYPEFUNC

/*******************************************************************************
 * Image Writers 
 ******************************************************************************/

template <int D, typename T>
int NDImageStore<D,T>::write(std::string filename, double version) const
{
	std::string mode = "wb";
	const size_t BSIZE = 1024*1024; //1M
	gzFile gz;

	// remove .gz to find the "real" format, 
	std::string nogz;
	if(filename.substr(filename.size()-3, 3) == ".gz") {
		nogz = filename.substr(0, filename.size()-3);
	} else {
		// if no .gz, then make encoding "transparent" (plain)
		nogz = filename;
		mode += 'T';
	}
	
	// go ahead and open
	gz = gzopen(filename.c_str(), mode.c_str());
	gzbuffer(gz, BSIZE);

	if(nogz.substr(nogz.size()-4, 4) == ".nii") {
		if(version >= 2) {
			if(writeNifti2Image(gz) != 0) {
				std::cerr << "Error writing" << std:: endl;
				gzclose(gz);
				return -1;
			}
		} else {
			if(writeNifti1Image(gz) != 0) {
				std::cerr << "Error writing" << std:: endl;
				gzclose(gz);
				return -1;
			}
		}
	} else {
		std::cerr << "Unknown filetype: " << nogz.substr(nogz.rfind('.')) 
			<< std::endl;
		gzclose(gz);
		return -1;
	}

	gzclose(gz);
	return 0;
}

template <int D, typename T>
int NDImageStore<D,T>::writeNifti1Image(gzFile file) const
{
	int ret = writeNifti1Header(file);
	if(ret != 0) 
		return ret;
	ret = writePixels(file);
	return ret;
}

template <int D, typename T>
int NDImageStore<D,T>::writeNifti2Image(gzFile file) const
{
	int ret = writeNifti2Header(file);
	if(ret != 0) 
		return ret;
	ret = writePixels(file);
	return ret;
}

template <int D, typename T>
int NDImageStore<D,T>::writeNifti1Header(gzFile file) const
{
	static_assert(sizeof(nifti1_header) == 348, "Error, nifti header packing failed");
	nifti1_header header;
	std::fill((char*)&header, ((char*)&header)+sizeof(nifti1_header), 0);

	header.sizeof_hdr = 348;

	if(m_freqdim >= 0 && m_freqdim <= 2) 
		header.dim_info.bits.freqdim = m_freqdim; 
	if(m_phasedim>= 0 && m_phasedim<= 2) 
		header.dim_info.bits.phasedim = m_phasedim; 
	if(m_slicedim >= 0 && m_slicedim <= 2) 
		header.dim_info.bits.slicedim = m_slicedim; 

	// dimensions
	header.ndim = (short)ndim();
	for(size_t ii=0; ii<ndim(); ii++) {
		assert(dim(ii) <= SHRT_MAX);
		header.dim[ii] = (short)dim(ii);
	}

	header.datatype = type();
	header.bitpix = sizeof(T)*8;

	assert(m_slice_start < SHRT_MAX);
	assert(m_slice_end < SHRT_MAX);

	header.slice_start = m_slice_start;
	header.slice_end = m_slice_end;
	header.slice_duration = m_slice_duration;
	header.slice_code = m_slice_order;

	header.qform_code = 1;

	// orientation
	if(D > 2)
		header.toffset = m_origin[3];
	for(size_t ii=0; ii<3 && ii<D; ii++) 
		header.qoffset[ii] = m_origin[ii];

	for(size_t ii=0; ii<7 && ii<D; ii++) 
		header.pixdim[ii] = m_space[ii];
	
	Matrix<3,3> rotate; 
	for(size_t rr=0; rr<3 && rr<D; rr++) {
		for(size_t cc=0; cc<3 && cc<D; cc++) {
			rotate(rr,cc) = m_dir(rr,cc);
		}
	}

	double det = determinant(rotate);
	if(det > 0) 
		header.qfac = 1;
	 else {
		header.qfac = -1;
		rotate(0,2) = -rotate(0,2);
		rotate(1,2) = -rotate(1,2);
		rotate(2,2) = -rotate(2,2);
	 }


	 double a = 0.5*sqrt(rotate(0,0)+rotate(1,1)+rotate(2,2)+1);
	 double b = 0.5*sqrt(rotate(0,0)-(rotate(1,1)+rotate(2,2))+1);
	 double c = 0.5*sqrt(rotate(1,1)-(rotate(0,0)+rotate(2,2))+1);
	 double d = 0.5*sqrt(rotate(2,2)-(rotate(0,0)+rotate(1,1))+1);

	 if(fabs(a) > 0.001) {
		 b = 0.25*(rotate(2,1)-rotate(1,2))/a;
		 c = 0.25*(rotate(0,2)-rotate(2,0))/a;
		 d = 0.25*(rotate(1,0)-rotate(0,1))/a;
	 } else if(fabs(b) > 0.001) {
		 c = 0.25*(rotate(0,1)+rotate(1,0))/b;
		 d = 0.25*(rotate(0,2)+rotate(2,0))/b;
		 a = 0.25*(rotate(2,1)-rotate(1,2))/b;
	 } else if(fabs(c) > 0.001) {
		 b = 0.25*(rotate(0,1)+rotate(1,0))/c ;
		 d = 0.25*(rotate(1,2)+rotate(2,1))/c ;
		 a = 0.25*(rotate(0,2)-rotate(2,0))/c ;
	 } else {
		 b = 0.25*(rotate(0,2)+rotate(2,0))/d ;
		 c = 0.25*(rotate(1,2)+rotate(2,1))/d ;
		 a = 0.25*(rotate(1,0)-rotate(0,1))/d ;
	 }

	 if(a < 0.0) {
		 b=-b;
		 c=-c;
		 d=-d;
//		 a=-a; 
	 }

	header.quatern[0] = b;
	header.quatern[1] = c;
	header.quatern[2] = d;
	
	//magic
	strncpy(header.magic,"n+1\0", 4);
	header.vox_offset = 352;


	// write over extension
	char ext[4] = {0,0,0,0};
	
	gzwrite(file, &header, sizeof(header));
	gzwrite(file, ext, sizeof(ext));

	return 0;
}

template <int D, typename T>
int NDImageStore<D,T>::writeNifti2Header(gzFile file) const
{
	static_assert(sizeof(nifti2_header) == 540, "Error, nifti header packing failed");
	nifti2_header header;
	std::fill((char*)&header, ((char*)&header)+sizeof(nifti2_header), 0);

	header.sizeof_hdr = 348;

	if(m_freqdim >= 0 && m_freqdim <= 2) 
		header.dim_info.bits.freqdim = m_freqdim; 
	if(m_phasedim>= 0 && m_phasedim<= 2) 
		header.dim_info.bits.phasedim = m_phasedim; 
	if(m_slicedim >= 0 && m_slicedim <= 2) 
		header.dim_info.bits.slicedim = m_slicedim; 

	// dimensions
	header.ndim = ndim();
	for(size_t ii=0; ii<ndim(); ii++) {
		header.dim[ii] = dim(ii);
	}

	header.datatype = type();
	header.bitpix = sizeof(T)*8;

	header.slice_start = m_slice_start;
	header.slice_end = m_slice_end;
	header.slice_duration = m_slice_duration;
	header.slice_code = m_slice_order;

	header.qform_code = 1;

	// orientation
	if(D > 2)
		header.toffset = m_origin[3];
	for(size_t ii=0; ii<3 && ii<D; ii++) 
		header.qoffset[ii] = m_origin[ii];

	for(size_t ii=0; ii<7 && ii<D; ii++) 
		header.pixdim[ii] = m_space[ii];
	
	Matrix<3,3> rotate; 
	for(size_t rr=0; rr<3 && rr<D; rr++) {
		for(size_t cc=0; cc<3 && cc<D; cc++) {
			rotate(rr,cc) = m_dir(rr,cc);
		}
	}

	double det = determinant(rotate);
	if(det > 0) 
		header.qfac = 1;
	 else {
		header.qfac = -1;
		rotate(0,2) = -rotate(0,2);
		rotate(1,2) = -rotate(1,2);
		rotate(2,2) = -rotate(2,2);
	 }


	 double a = 0.5*sqrt(rotate(0,0)+rotate(1,1)+rotate(2,2)+1);
	 double b = 0.5*sqrt(rotate(0,0)-(rotate(1,1)+rotate(2,2))+1);
	 double c = 0.5*sqrt(rotate(1,1)-(rotate(0,0)+rotate(2,2))+1);
	 double d = 0.5*sqrt(rotate(2,2)-(rotate(0,0)+rotate(1,1))+1);

	 if(fabs(a) > 0.001) {
		 b = 0.25*(rotate(2,1)-rotate(1,2))/a;
		 c = 0.25*(rotate(0,2)-rotate(2,0))/a;
		 d = 0.25*(rotate(1,0)-rotate(0,1))/a;
	 } else if(fabs(b) > 0.001) {
		 c = 0.25*(rotate(0,1)+rotate(1,0))/b;
		 d = 0.25*(rotate(0,2)+rotate(2,0))/b;
		 a = 0.25*(rotate(2,1)-rotate(1,2))/b;
	 } else if(fabs(c) > 0.001) {
		 b = 0.25*(rotate(0,1)+rotate(1,0))/c ;
		 d = 0.25*(rotate(1,2)+rotate(2,1))/c ;
		 a = 0.25*(rotate(0,2)-rotate(2,0))/c ;
	 } else {
		 b = 0.25*(rotate(0,2)+rotate(2,0))/d ;
		 c = 0.25*(rotate(1,2)+rotate(2,1))/d ;
		 a = 0.25*(rotate(1,0)-rotate(0,1))/d ;
	 }

	 if(a < 0.0) {
		 b=-b;
		 c=-c;
		 d=-d;
//		 a=-a; 
	 }

	header.quatern[0] = b;
	header.quatern[1] = c;
	header.quatern[2] = d;
	
	//magic
	strncpy(header.magic,"n+1\0", 4);
	header.vox_offset = 352;


	// write over extension
	char ext[4] = {0,0,0,0};
	
	gzwrite(file, &header, sizeof(header));
	gzwrite(file, ext, sizeof(ext));
	
	return 0;
}
//
//template<>
//int NDImage::writePixels<cfloat_t>(gzFile file) const
//{
//	// x is the fastest in nifti, for us it is the slowest
//	list<size_t> order;
//	for(size_t ii=0 ; ii<order.size(); ii++)
//		order.push_back(ii);
//
//	cdouble_t tmp;
//	for(auto it = cbegin_cdbl(order); !it.isEnd(); ++it) {
//		double re = it.get().real();
//		double im = it.get().imag();
//		gzwrite(file, &re, sizeof(double));
//		gzwrite(file, &im, sizeof(double));
//	}
//	return 0;
//}
//
//template<>
//int NDImage::writePixels<cdouble_t>(gzFile file) const
//{
//	// x is the fastest in nifti, for us it is the slowest
//	list<size_t> order;
//	for(size_t ii=0 ; ii<order.size(); ii++)
//		order.push_back(ii);
//
//	cdouble_t tmp;
//	for(auto it = cbegin_cdbl(order); !it.isEnd(); ++it) {
//		float re = it.get().real();
//		float im = it.get().imag();
//		gzwrite(file, &re, sizeof(float));
//		gzwrite(file, &im, sizeof(float));
//	}
//	return 0;
//}
//
//template <>
//int NDImage::writePixels<rgba_t>(gzFile file) const
//{
//
//	// x is the fastest in nifti, for us it is the slowest
//	list<size_t> order;
//	for(size_t ii=0 ; ii<order.size(); ii++)
//		order.push_back(ii);
//
//	for(auto it = cbegin_rgba(order); !it.isEnd(); ++it) {
//		char r = it.get().red;
//		char g = it.get().green;
//		char b = it.get().blue;
//		char a = it.get().alpha;
//		gzwrite(file, &r, sizeof(char));
//		gzwrite(file, &g, sizeof(char));
//		gzwrite(file, &b, sizeof(char));
//		gzwrite(file, &a, sizeof(char));
//	}
//	return 0;
//}

template <int D, typename T>
int NDImageStore<D,T>::writePixels(gzFile file) const
{
	// x is the fastest in nifti, for us it is the slowest
	list<size_t> order;
	for(size_t ii=0 ; ii<order.size(); ii++)
		order.push_back(ii);

	T tmp;
	switch(type()) {
		case INT8:
		case UINT8:
		case INT16:
		case UINT16:
		case INT32:
		case UINT32:
		case INT64:
		case UINT64:
			for(auto it = cbegin_int64(order); !it.isEnd(); ++it) {
				tmp = (T)*it;
				gzwrite(file, &tmp, sizeof(T));
			}
			break;
		case FLOAT32:
		case FLOAT64:
			for(auto it = cbegin_dbl(order); !it.isEnd(); ++it) {
				tmp = (T)*it;
				gzwrite(file, &tmp, sizeof(T));
			}
			break;
		case FLOAT128:
			for(auto it = cbegin_quad(order); !it.isEnd(); ++it) {
				tmp = (T)*it;
				gzwrite(file, &tmp, sizeof(T));
			}
			break;
		case COMPLEX64:
			for(auto it = cbegin_cfloat(order); !it.isEnd(); ++it) {
				tmp = (T)*it;
				float re = it.get().real();
				float im = it.get().imag();
				gzwrite(file, &re, sizeof(float));
				gzwrite(file, &im, sizeof(float));
			}
		case COMPLEX128:
			for(auto it = cbegin_cdbl(order); !it.isEnd(); ++it) {
				tmp = (T)*it;
				double re = it.get().real();
				double im = it.get().imag();
				gzwrite(file, &re, sizeof(double));
				gzwrite(file, &im, sizeof(double));
			}
			break;
		case COMPLEX256:
			for(auto it = cbegin_cquad(order); !it.isEnd(); ++it) {
				long double re = it.get().real();
				long double im = it.get().imag();
				gzwrite(file, &re, sizeof(long double));
				gzwrite(file, &im, sizeof(long double));
			}
			break;
		case RGB24:
			for(auto it = cbegin_rgba(order); !it.isEnd(); ++it) {
				char red = it.get().red;
				char green = it.get().green;
				char blue = it.get().blue;
				gzwrite(file, &red, sizeof(char));
				gzwrite(file, &green, sizeof(char));
				gzwrite(file, &blue, sizeof(char));
			}
			break;
		case RGBA32:
			for(auto it = cbegin_rgba(order); !it.isEnd(); ++it) {
				char red = it.get().red;
				char green = it.get().green;
				char blue = it.get().blue;
				char alpha = it.get().alpha;
				gzwrite(file, &red, sizeof(char));
				gzwrite(file, &green, sizeof(char));
				gzwrite(file, &blue, sizeof(char));
				gzwrite(file, &alpha, sizeof(char));
			}
			break;
		default:
			std::cerr << "Error Uknown Type!" << std:: endl;
			return -1;
	}
	return 0;

}

} //npl