/*******************************************************************************
This file is part of Neuro Programs and Libraries (NPL), 

Written and Copyrighted by by Micah C. Chambers (micahc.vt@gmail.com)

The Neuro Programs and Libraries is free software: you can redistribute it and/or
modify it under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your option)
any later version.

The Neural Programs and Libraries are distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with
the Neural Programs Library.  If not, see <http://www.gnu.org/licenses/>.
*******************************************************************************/

#include "npltypes.h"
#include "slicer.h"
#include <iostream>

namespace npl {

template <size_t D, typename T>
NDArrayStore<D,T>::NDArrayStore(const std::initializer_list<size_t>& a_args) : 
			_m_data(NULL)
{
	size_t tmp[D];
	
	// set dimensions with matching size to the minimum length, ignoring
	// any extra parts of a_args
	auto it = a_args.begin();
	for(size_t ii=0; ii < D; ii++) {
		if(it != a_args.end()) {
			tmp[ii] = *it;
			++it;
		} else {
			tmp[ii] = 1;
		}
	}

	resize(tmp);
}

template <size_t D, typename T>
NDArrayStore<D,T>::NDArrayStore(const std::vector<size_t>& a_args) : _m_data(NULL)
{
	size_t tmp[D];
	
	// set dimensions with matching size to the minimum length, ignoring
	// any extra parts of a_args
	auto it = a_args.begin();
	for(size_t ii=0; ii < D; ii++) {
		if(it != a_args.end()) {
			tmp[ii] = *it;
			++it;
		} else {
			tmp[ii] = 1;
		}
	}

	resize(tmp);
}

template <size_t D, typename T>
NDArrayStore<D,T>::NDArrayStore(size_t len, const size_t* a_args) : _m_data(NULL)
{
	size_t tmp[D];
	
	// set dimensions with matching size to the minimum length, ignoring
	// any extra parts of a_args
	for(size_t ii=0; ii<D; ii++) {
		if(ii < len)
			tmp[ii] = a_args[ii];
		else
			tmp[ii] = 1;
	}

	resize(tmp);
}

template <size_t D, typename T>
NDArrayStore<D,T>::NDArrayStore(size_t len, const size_t* dim, T* ptr) : _m_data(NULL)
{
	if(len != D) {
		std::cerr << "Length of input size (" << len << ") array to " << D 
			<< "D array is not allowed." << endl;
	}
	graft(dim, ptr);
}

template <size_t D, typename T>
void NDArrayStore<D,T>::updateStrides()
{
	_m_stride[D-1] = 1;
	for(int64_t ii=D-2; ii>=0; ii--) 
		_m_stride[ii] = _m_stride[ii+1]*_m_dim[ii+1];
}

/**
 * @brief Graft an ND dataset into the NDArray. Any old data is deleted and 
 * the dimensions are set to those passed. *
 *
 * @tparam D Array dimensionality
 * @tparam T Type of data stored
 * @param dim[D] dimension of graft data, and new dimensions after this 
 * @param ptr Pointer to new data
 */
template <size_t D, typename T>
void NDArrayStore<D,T>::graft(const size_t dim[D], T* ptr)
{
	if(_m_data) 
		delete[] _m_data;

	for(size_t ii=0; ii<D; ii++) 
		_m_dim[ii] = dim[ii];

	_m_data = ptr;

	updateStrides();
}

/**
 * @brief If the size is different from the current size, then it allocates
 * a new chunk of memory, fills it with zeros and then copies the values from
 * the original image into the new image. If the size is the same, it does 
 * nothing.
 *
 * @tparam D
 * @tparam T
 * @param dim[D]
 */
template <size_t D, typename T>
void NDArrayStore<D,T>::resize(const size_t dim[D])
{
	if(_m_data) {
		///////////////////////////////////////////////////////
		// Do nothing if the input dimensions match the current
		///////////////////////////////////////////////////////
		bool samedim = true;
		for(size_t ii=0; ii<D; ii++) {
			if(_m_dim[ii] != dim[ii]) {
				samedim = false;
				break;
			}
		}

		if(samedim)
			return;
	
		////////////////////////////////
		// Create New Data and zero fill
		////////////////////////////////
		size_t dsize = 1;
		for(size_t ii=0; ii<D; ii++) 
			dsize *= dim[ii];
		T* newdata = new T[dsize];
		std::fill(newdata, newdata+dsize, 0);
		
		// copy the old array to the new, by creating slicers with regions of
		//interest which have the minimum size of the original or new 
		int64_t roi_lower[D];
		int64_t roi_upper[D];
		for(size_t dd=0; dd < D; dd++) {
			roi_lower[dd] = 0;
			roi_upper[dd] = std::min<int64_t>(dim[dd], _m_dim[dd]);
		}

		Slicer oldit(D, _m_dim);
		Slicer newit(D, dim);
		oldit.setROI(D, roi_lower, roi_upper);
		newit.setROI(D, roi_lower, roi_upper);

		// copy the data
		for(oldit.goBegin(), newit.goBegin(); !newit.eof(); ++newit, ++oldit) {
			newdata[*newit] = _m_data[*oldit];
		}
		assert(newit.eof() && oldit.eof());

		// now copy the dimension
		for(size_t ii=0; ii<D; ii++) {
			_m_dim[ii] = dim[ii];
		}

		// set up data pointer
		delete[] _m_data;
		_m_data = newdata;
	} else {
		// just create the data
		size_t dsize = 1;
		for(size_t ii=0; ii<D; ii++) {
			_m_dim[ii] = dim[ii];
			dsize *= _m_dim[ii];
		}
		
		// allocate
		_m_data = new T[dsize];

		// zero fill
		std::fill(_m_data, _m_data+dsize, 0);
	}

	updateStrides();
}

template <size_t D, typename T>
size_t NDArrayStore<D,T>::bytes() const
{
	size_t out = 1;
	for(size_t ii=0; ii<D; ii++)
		out*= _m_dim[ii];
	return out*sizeof(T);
}

template <size_t D, typename T>
size_t NDArrayStore<D,T>::elements() const
{
	size_t out = 1;
	for(size_t ii=0; ii<D; ii++)
		out*= _m_dim[ii];
	return out;
}

template <size_t D, typename T>
size_t NDArrayStore<D,T>::ndim() const
{
	return D;
}

template <size_t D, typename T>
size_t NDArrayStore<D,T>::dim(size_t dir) const
{
	return _m_dim[dir];
}

template <size_t D, typename T>
const size_t* NDArrayStore<D,T>::dim() const
{
	return _m_dim;
}

/* 
 * Get Address of a Particular Index 
 * TODO add *outside
 */
template <size_t D, typename T>
int64_t NDArrayStore<D,T>::getLinIndex(std::initializer_list<int64_t> index) const
{
	int64_t out = 0;

	// copy the dimensions 
	int64_t ii=0;
	for(auto it=index.begin(); it != index.end() && ii<D; ii++, ++it) {
		assert(*it >= 0);
		assert(*it < _m_dim[ii]);
		// set position
		out += _m_stride[ii]*(*it);
	}
	
	assert(out < elements());
	return out;
}

template <size_t D, typename T>
inline
int64_t NDArrayStore<D,T>::getLinIndex(const int64_t* index) const
{
	int64_t out = 0;

	// copy the dimensions 
	for(size_t ii = 0; ii<D; ii++) {
		assert(index[ii] >= 0);
		assert(index[ii] < _m_dim[ii]);

		// set position
		out += _m_stride[ii]*index[ii];
	}

	assert(out < elements());
	return out;
}

template <size_t D, typename T>
inline
int64_t NDArrayStore<D,T>::getLinIndex(const std::vector<int64_t>& index) const
{
	size_t out = 0;

	// copy the dimensions 
	size_t ii=0;
	for(auto it=index.begin(); it != index.end() && ii<D; ++it, ii++) {
		assert(index[ii] >= 0);
		assert(index[ii] < _m_dim[ii]);

		// set position
		out += _m_stride[ii]*(*it);
	}
	
	assert(out < elements());
	return out;
}
	
/**
 * @brief Used instead of the normal mapper, we want all the upper dimensions 
 * to be treated as flat
 *
 * @param x
 * @param y
 * @param z
 * @param t
 *
 * @return 
 */
template <size_t D, typename T>
inline
int64_t NDArrayStore<D,T>::getLinIndex(int64_t x, int64_t y, int64_t z, 
			int64_t t) const
{
	int64_t out = 0;
	int64_t tmp[4] = {x,y,z,t};
	for(size_t ii=0; ii<3; ii++) {
		assert(tmp[ii] >= 0 && tmp[ii] < dim(ii));
		if(ii<D)
			out += tmp[ii]*_m_stride[ii];
	}

	// assert(z < _m_stride[2]);
	out += t;
	return out;
};
	
template <size_t D, typename T>
const T& NDArrayStore<D,T>::operator[](std::initializer_list<int64_t> index) const
{
	return _m_data[getLinIndex(index)];
}

template <size_t D, typename T>
const T& NDArrayStore<D,T>::operator[](const std::vector<int64_t>& index) const
{
	return _m_data[getLinIndex(index)];
}

template <size_t D, typename T>
const T& NDArrayStore<D,T>::operator[](const int64_t* index) const
{
	return _m_data[getLinIndex(index)];
}

template <size_t D, typename T>
const T& NDArrayStore<D,T>::operator[](int64_t pixel) const
{
	return _m_data[pixel];
}

template <size_t D, typename T>
T& NDArrayStore<D,T>::operator[](std::initializer_list<int64_t> index) 
{
	return _m_data[getLinIndex(index)];
}

template <size_t D, typename T>
T& NDArrayStore<D,T>::operator[](const std::vector<int64_t>& index) 
{
	return _m_data[getLinIndex(index)];
}

template <size_t D, typename T>
T& NDArrayStore<D,T>::operator[](const int64_t* index) 
{
	return _m_data[getLinIndex(index)];
}

template <size_t D, typename T>
T& NDArrayStore<D,T>::operator[](int64_t pixel) 
{
	return _m_data[pixel];
}

//template <size_t D, typename T>
//int NDArrayStore<D,T>::opself(const NDArray* right, 
//		double(*func)(double,double), bool elevR)
//{
//	bool canElev = false;
//	bool comp = comparable(this, right, NULL, &canElev);
//	if(comp) {
//		for(size_t ii=0; ii<elements(); ii++) {
//			double result = func(get_dbl(ii), right->get_dbl(ii));
//			set_dbl(ii, result);
//		}
//	} else if(canElev && elevR) {
//		// match dimensions, to make this work, we need to iterate through the 
//		// common dimensions fastest, the unique dimensions slowest, the way
//		// iterators work, if you specify an order, you specify the fastest
//		std::list<size_t> commondim;
//		for(size_t ii=0; ii < ndim() && ii < right->ndim(); ii++) {
//			if(right->dim(ii) != 1)
//				commondim.push_front(ii);
//		}
//		for(Slicer lit(ndim(), dim(), commondim); !lit.isEnd() ; ) {
//			// iterate together until right hits the end, then restart
//			for(Slicer rit(ndim(), dim(), commondim); 
//						!lit.isEnd() && !rit.isEnd(); ++lit, ++rit) {
//				double result = func(get_dbl(*rit), get_dbl(*rit));
//				set_dbl(*lit, result);
//			}
//		}
//	} else {
//		std::cerr << "Input Images are not conformable, failing" << endl;
//		return -1;
//	}
//
//	return 0;
//}
//
//template <size_t D, typename T>
//std::shared_ptr<NDArray> NDArrayStore<D,T>::opnew(const NDArray* right, 
//		double(*func)(double,double), bool elevR)
//{
//	auto out = clone();
//	if(out->opself(right, func, elevR) != 0)
//		return NULL;
//	return out;
//}
//
//template <size_t D, typename T>
//std::shared_ptr<NDArray> NDArrayStore<D,T>::clone() const
//{
//	std::vector<size_t> newdims(_m_dim, _m_dim+D);
//	auto out = std::make_shared<NDArrayStore<D,T>>(newdims);
//
//	size_t total = 1;
//	for(size_t ii=0; ii<D; ii++)
//		total *= _m_dim[ii];
//
//	std::copy(_m_data, _m_data+total, out->_m_data);
//	std::copy(_m_dim, _m_dim+D, out->_m_dim);
//	std::copy(_m_stride, _m_stride+D, out->_m_stride);
//
//	return out;
//}

template <size_t D, typename T>
shared_ptr<NDArray> NDArrayStore<D,T>::copy() const
{
	shared_ptr<NDArrayStore> out(new NDArrayStore<D,T>(D, _m_dim));
	for(size_t ii=0; ii<elements(); ii++) 
		out->_m_data[ii] = _m_data[ii];

	return out;
}

//template <size_t D, typename T>
//template <size_t NEWD, typename NEWT>
//shared_ptr<NDArray> NDArrayStore<D,T>::copyCastStatic(const size_t* dim) const
//{
//	shared_ptr<NDArrayStore<NEWD, NEWT>> out(new NDArrayStore<NEWD,NEWT>(NEWD, dim));
//	
//	// Set up slicers to iterate through the input and output images. Only
//	// common dimensions are iterated over, and only the minimum of the two
//	// sizes are used for ROI. so a 10x10x10 image cast to a 20x5 image will
//	// iterator copy ROI 10x5x1 
//	Slicer iit(D, _m_dim);
//	Slicer oit(NEWD, dim);
//	
//	std::vector<std::pair<int64_t,int64_t>> roi(max(NEWD, D));
//	for(size_t ii=0; ii<max(NEWD, D); ii++) {
//		if(ii < min(NEWD, D)) {
//			roi[ii].first = 0;
//			roi[ii].second = min(dim[ii], _m_dim[ii]);
//		} else {
//			roi[ii].first = 0;
//			roi[ii].second = 0;
//		}
//	}
//
//	iit.setROI(roi);
//	oit.setROI(roi);
//
//	// use the larger of the two order vectors as the reference
//	if(NEWD > D) {
//		iit.setOrder(oit.getOrder());
//	} else {
//		oit.setOrder(iit.getOrder());
//	}
//
//	// perform copy/cast
//	for(iit.goBegin(), oit.goBegin(); !oit.eof() && !iit.eof(); ++oit, ++iit) 
//		out->_m_data[*oit] = (NEWT)_m_data[*iit];
//
//	return out;
//}
//
//template <size_t D, typename T>
//template <typename NEWT>
//shared_ptr<NDArray> NDArrayStore<D,T>::copyCastTType(size_t newdims, 
//			const size_t* newsize) const
//{
//	switch(newdims) {
//		case 1:
//			return copyCastStatic<1, NEWT>(newsize);
//		case 2:
//			return copyCastStatic<2, NEWT>(newsize);
//		case 3:
//			return copyCastStatic<3, NEWT>(newsize);
//		case 4:
//			return copyCastStatic<4, NEWT>(newsize);
//		case 5:
//			return copyCastStatic<5, NEWT>(newsize);
//		case 6:
//			return copyCastStatic<6, NEWT>(newsize);
//		case 7:
//			return copyCastStatic<7, NEWT>(newsize);
//		case 8:
//			return copyCastStatic<8, NEWT>(newsize);
//		case 9:
//			return copyCastStatic<9, NEWT>(newsize);
//		default:
//			throw std::invalid_argument("Unknown image dimension passed to "
//					"copyCastTType the programmers may need to add more "
//					"dimensions to the shared objects at build time");
//			return NULL;
//	}
//	return NULL;
//}
//
//template <size_t D, typename T>
//shared_ptr<NDArray> NDArrayStore<D,T>::copyCast(size_t newdims, 
//		const size_t* newsize, PixelT newtype) const
//{
//	shared_ptr<NDArray> out;
//	
//	switch(newtype) {
//		case UINT8:
//			return copyCastTType<uint8_t>(newdims, newsize);
//		case INT16:
//			return copyCastTType<int16_t>(newdims, newsize);
//		case INT32:
//			return copyCastTType<int32_t>(newdims, newsize);
//		case FLOAT32:
//			return copyCastTType<float>(newdims, newsize);
//		case COMPLEX64:
//			return copyCastTType<cfloat_t>(newdims, newsize);
//		case FLOAT64:
//			return copyCastTType<double>(newdims, newsize);
//		case RGB24:
//			return copyCastTType<rgb_t>(newdims, newsize);
//		case INT8:
//			return copyCastTType<int8_t>(newdims, newsize);
//		case UINT16:
//			return copyCastTType<uint16_t>(newdims, newsize);
//		case UINT32:
//			return copyCastTType<uint32_t>(newdims, newsize);
//		case INT64:
//			return copyCastTType<int64_t>(newdims, newsize);
//		case UINT64:
//			return copyCastTType<uint64_t>(newdims, newsize);
//		case FLOAT128:
//			return copyCastTType<long double>(newdims, newsize);
//		case COMPLEX128:
//			return copyCastTType<cdouble_t>(newdims, newsize);
//		case COMPLEX256:
//			return copyCastTType<cquad_t>(newdims, newsize);
//		case RGBA32:
//			return copyCastTType<rgba_t>(newdims, newsize);
//		default:
//		case UNKNOWN_TYPE:
//		throw std::invalid_argument("Unknown type specified for cast");
//		break;
//	}
//	return NULL;
//}


/* 
 * type() Function specialized for all available types
 */
template <size_t D, typename T>
PixelT NDArrayStore<D,T>::type() const 
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


} //npl
#undef GETSETIMPL
