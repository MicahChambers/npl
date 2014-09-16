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
 * @file ndarray.cpp
 *
 *****************************************************************************/
#include <iostream>

#include "ndarray.h"
#include "iterators.h"
#include "macros.h"

#include "npltypes.h"
#include "utility.h"

using std::make_shared;

namespace npl {

/******************************************************************************
 * Helper Functions. We put these here because they are internally by NDArray
 * and NDArrayStore; we don't want to call across libraries if we can avoid
 * it.
 ******************************************************************************/

/**
 * @brief Template helper for creating new images.
 *
 * @tparam T Type of voxels
 * @param ndim Length of input dimension array
 * @param dim Size of new image
 *
 * @return New NDArray with defaults set
 */
template <typename T>
shared_ptr<NDArray> createNDArrayHelp(size_t ndim, const size_t* dim)
{
	switch(ndim) {
		case 1:
			return std::make_shared<NDArrayStore<1, T>>(ndim, dim);
		case 2:
			return std::make_shared<NDArrayStore<2, T>>(ndim, dim);
		case 3:
			return std::make_shared<NDArrayStore<3, T>>(ndim, dim);
		case 4:
			return std::make_shared<NDArrayStore<4, T>>(ndim, dim);
		case 5:
			return std::make_shared<NDArrayStore<5, T>>(ndim, dim);
		case 6:
			return std::make_shared<NDArrayStore<6, T>>(ndim, dim);
		case 7:
			return std::make_shared<NDArrayStore<7, T>>(ndim, dim);
		case 8:
			return std::make_shared<NDArrayStore<8, T>>(ndim, dim);
		default:
            throw std::invalid_argument("Unsupported len, dimension: " +
                    to_string(ndim) + " in\n" + __FUNCTION_STR__);
	}

	return NULL;
}

/**
 * @brief Creates a new NDArray with dimensions set by ndim, and size set by
 * size. Output pixel type is decided by ptype variable.
 *
 * @param ndim number of image dimensions
 * @param size size of image, in each dimension
 * @param ptype Pixel type npl::PixelT
 *
 * @return New image, default orientation
 */
shared_ptr<NDArray> createNDArray(size_t ndim, const size_t* size, PixelT ptype)
{
	switch(ptype) {
         case UINT8:
			return createNDArrayHelp<uint8_t>(ndim, size);
        break;
         case INT16:
			return createNDArrayHelp<int16_t>(ndim, size);
        break;
         case INT32:
			return createNDArrayHelp<int32_t>(ndim, size);
        break;
         case FLOAT32:
			return createNDArrayHelp<float>(ndim, size);
        break;
         case COMPLEX64:
			return createNDArrayHelp<cfloat_t>(ndim, size);
        break;
         case FLOAT64:
			return createNDArrayHelp<double>(ndim, size);
        break;
         case RGB24:
			return createNDArrayHelp<rgb_t>(ndim, size);
        break;
         case INT8:
			return createNDArrayHelp<int8_t>(ndim, size);
        break;
         case UINT16:
			return createNDArrayHelp<uint16_t>(ndim, size);
        break;
         case UINT32:
			return createNDArrayHelp<uint32_t>(ndim, size);
        break;
         case INT64:
			return createNDArrayHelp<int64_t>(ndim, size);
        break;
         case UINT64:
			return createNDArrayHelp<uint64_t>(ndim, size);
        break;
         case FLOAT128:
			return createNDArrayHelp<long double>(ndim, size);
        break;
         case COMPLEX128:
			return createNDArrayHelp<cdouble_t>(ndim, size);
        break;
         case COMPLEX256:
			return createNDArrayHelp<cquad_t>(ndim, size);
        break;
         case RGBA32:
			return createNDArrayHelp<rgba_t>(ndim, size);
        break;
		 default:
            throw std::invalid_argument("Unsupported pixel type: " +
                    to_string(ptype) + " in\n" + __FUNCTION_STR__);
	}
	return NULL;
}

/**
 * @brief Creates a new NDArray with dimensions set by ndim, and size set by
 * size. Output pixel type is decided by ptype variable.
 *
 * @param dim size of image, in each dimension, number of dimensions decied by
 * length of size vector
 * @param ptype Pixel type npl::PixelT
 *
 * @return New image, default orientation
 */
shared_ptr<NDArray> createNDArray(const std::vector<size_t>& dim, PixelT ptype)
{
	return createNDArray(dim.size(), dim.data(), ptype);
}

/**
 * @brief Template helper for creating new images.
 *
 * @tparam T Type of voxels
 * @param ndim Length of input dimension array
 * @param dim Size of new image
 * @param ptr Pointer to data block
 * @param deleter function to delete data block
 *
 * @return New NDArray with defaults set
 */
template <typename T>
shared_ptr<NDArray> createNDArrayHelp(size_t ndim, const size_t* dim,
        void* ptr, std::function<void(void*)> deleter)
{
	switch(ndim) {
		case 1:
			return make_shared<NDArrayStore<1, T>>(ndim, dim, (T*)ptr, deleter);
		case 2:
			return make_shared<NDArrayStore<2, T>>(ndim, dim, (T*)ptr, deleter);
		case 3:
			return make_shared<NDArrayStore<3, T>>(ndim, dim, (T*)ptr, deleter);
		case 4:
			return make_shared<NDArrayStore<4, T>>(ndim, dim, (T*)ptr, deleter);
		case 5:
			return make_shared<NDArrayStore<5, T>>(ndim, dim, (T*)ptr, deleter);
		case 6:
			return make_shared<NDArrayStore<6, T>>(ndim, dim, (T*)ptr, deleter);
		case 7:
			return make_shared<NDArrayStore<7, T>>(ndim, dim, (T*)ptr, deleter);
		case 8:
			return make_shared<NDArrayStore<8, T>>(ndim, dim, (T*)ptr, deleter);
		default:
            throw std::invalid_argument("Unsupported len, dimension: " +
                    to_string(ndim) + " in\n" + __FUNCTION_STR__);
	}

	return NULL;
}

/**
 * @brief Creates a new NDArray with dimensions set by ndim, and size set by
 * size. Output pixel type is decided by ptype variable.
 *
 * @param ndim number of image dimensions
 * @param size size of image, in each dimension
 * @param ptype Pixel type npl::PixelT
 * @param ptr Pointer to data block
 * @param deleter function to delete data block
 *
 * @return New image, default orientation
 */
shared_ptr<NDArray> createNDArray(size_t ndim, const size_t* size, PixelT ptype,
        void* ptr, std::function<void(void*)> deleter)
{
	switch(ptype) {
         case UINT8:
			return createNDArrayHelp<uint8_t>(ndim, size, ptr, deleter);
        break;
         case INT16:
			return createNDArrayHelp<int16_t>(ndim, size, ptr, deleter);
        break;
         case INT32:
			return createNDArrayHelp<int32_t>(ndim, size, ptr, deleter);
        break;
         case FLOAT32:
			return createNDArrayHelp<float>(ndim, size, ptr, deleter);
        break;
         case COMPLEX64:
			return createNDArrayHelp<cfloat_t>(ndim, size, ptr, deleter);
        break;
         case FLOAT64:
			return createNDArrayHelp<double>(ndim, size, ptr, deleter);
        break;
         case RGB24:
			return createNDArrayHelp<rgb_t>(ndim, size, ptr, deleter);
        break;
         case INT8:
			return createNDArrayHelp<int8_t>(ndim, size, ptr, deleter);
        break;
         case UINT16:
			return createNDArrayHelp<uint16_t>(ndim, size, ptr, deleter);
        break;
         case UINT32:
			return createNDArrayHelp<uint32_t>(ndim, size, ptr, deleter);
        break;
         case INT64:
			return createNDArrayHelp<int64_t>(ndim, size, ptr, deleter);
        break;
         case UINT64:
			return createNDArrayHelp<uint64_t>(ndim, size, ptr, deleter);
        break;
         case FLOAT128:
			return createNDArrayHelp<long double>(ndim, size, ptr, deleter);
        break;
         case COMPLEX128:
			return createNDArrayHelp<cdouble_t>(ndim, size, ptr, deleter);
        break;
         case COMPLEX256:
			return createNDArrayHelp<cquad_t>(ndim, size, ptr, deleter);
        break;
         case RGBA32:
			return createNDArrayHelp<rgba_t>(ndim, size, ptr, deleter);
        break;
		 default:
            throw std::invalid_argument("Unsupported pixel type: " +
                    to_string(ptype) + " in\n" + __FUNCTION_STR__);
	}
	return NULL;
}

/**
 * @brief Creates a new NDArray with dimensions set by ndim, and size set by
 * size. Output pixel type is decided by ptype variable.
 *
 * @param dim size of image, in each dimension, number of dimensions decied by
 * length of size vector
 * @param ptype Pixel type npl::PixelT
 * @param ptr Pointer to data block
 * @param deleter function to delete data block
 *
 * @return New image, default orientation
 */
shared_ptr<NDArray> createNDArray(const std::vector<size_t>& dim, PixelT ptype,
        void* ptr, std::function<void(void*)> deleter)
{
	return createNDArray(dim.size(), dim.data(), ptype, ptr, deleter);
}

/**
 * @brief Helper function that casts all the elements as the given type then uses
 * the same type to set all the elements of the output array. Only overlapping
 * sections of the arrays are copied.
 *
 * @tparam T Type to cast to
 * @param in Input array to copy
 * @param out Output array to write to
 */
template <typename T>
void _copyCast_help(shared_ptr<const NDArray> in, shared_ptr<NDArray> out)
{

	// Set up slicers to iterate through the input and output arrays. Only
	// common dimensions are iterated over, and only the minimum of the two
	// sizes are used for ROI. so a 10x10x10 array cast to a 20x5 array will
	// iterator copy ROI 10x5x1
	OrderConstIter<T> iit(in);
	OrderIter<T> oit(out);
	
	std::vector<std::pair<int64_t,int64_t>> roi(max(out->ndim(), in->ndim()));
	for(size_t ii=0; ii<roi.size(); ii++) {
		if(ii < min(out->ndim(), in->ndim())) {
			roi[ii].first = 0;
			roi[ii].second = min(out->dim(ii), in->dim(ii))-1;
		} else {
			roi[ii].first = 0;
			roi[ii].second = 0;
		}
	}

	iit.setROI(roi);
	oit.setROI(roi);

	// use the larger of the two order vectors as the reference
	if(in->ndim() < out->ndim()) {
		iit.setOrder(oit.getOrder());
	} else {
		oit.setOrder(iit.getOrder());
	}

	// perform copy/cast
	for(iit.goBegin(), oit.goBegin(); !oit.eof() && !iit.eof(); ++oit, ++iit)
		oit.set(*iit);

}

/**
 * @brief Create a new array that is a copy of the input, possibly with new
 * dimensions and pixeltype. The new array will have all overlapping pixels
 * copied from the old array.
 *
 * @param in Input array, anything that can be copied will be
 * @param newdims Number of dimensions in output array
 * @param newsize Size of output array
 * @param newtype Type of pixels in output array
 *
 * @return Image with overlapping sections cast and copied from 'in'
 */
shared_ptr<NDArray> _copyCast(shared_ptr<const NDArray> in, size_t newdims,
		const size_t* newsize, PixelT newtype)
{
	auto out = createNDArray(newdims, newsize, newtype);

	switch(newtype) {
		case UINT8:
			_copyCast_help<uint8_t>(in, out);
			break;
		case INT16:
			_copyCast_help<int16_t>(in, out);
			break;
		case INT32:
			_copyCast_help<int32_t>(in, out);
			break;
		case FLOAT32:
			_copyCast_help<float>(in, out);
			break;
		case COMPLEX64:
			_copyCast_help<cfloat_t>(in, out);
			break;
		case FLOAT64:
			_copyCast_help<double>(in, out);
			break;
		case RGB24:
			_copyCast_help<rgb_t>(in, out);
			break;
		case INT8:
			_copyCast_help<int8_t>(in, out);
			break;
		case UINT16:
			_copyCast_help<uint16_t>(in, out);
			break;
		case UINT32:
			_copyCast_help<uint32_t>(in, out);
			break;
		case INT64:
			_copyCast_help<int64_t>(in, out);
			break;
		case UINT64:
			_copyCast_help<uint64_t>(in, out);
			break;
		case FLOAT128:
			_copyCast_help<long double>(in, out);
			break;
		case COMPLEX128:
			_copyCast_help<cdouble_t>(in, out);
			break;
		case COMPLEX256:
			_copyCast_help<cquad_t>(in, out);
			break;
		case RGBA32:
			_copyCast_help<rgba_t>(in, out);
			break;
		default:
			throw INVALID_ARGUMENT("Unsupported pixel type: " +
					to_string(newtype));
	}
	return out;
}

/**
 * @brief Create a new array that is a copy of the input, with same dimensions
 * but pxiels cast to newtype. The new array will have all overlapping pixels
 * copied from the old array.
 *
 * @param in Input array, anything that can be copied will be
 * @param newtype Type of pixels in output array
 *
 * @return Image with overlapping sections cast and copied from 'in'
 */
shared_ptr<NDArray> _copyCast(shared_ptr<const NDArray> in, PixelT newtype)
{
	return _copyCast(in, in->ndim(), in->dim(), newtype);
}

/**
 * @brief Create a new array that is a copy of the input, possibly with new
 * dimensions or size. The new array will have all overlapping pixels
 * copied from the old array. The new array will have the same pixel type as
 * the input array
 *
 * @param in Input array, anything that can be copied will be
 * @param newdims Number of dimensions in output array
 * @param newsize Size of output array
 *
 * @return Image with overlapping sections cast and copied from 'in'
 */
shared_ptr<NDArray> _copyCast(shared_ptr<const NDArray> in, size_t newdims,
		const size_t* newsize)
{
	return _copyCast(in, newdims, newsize, in->type());
}

/**
 * @brief Helper function that casts all the elements as the given type then uses
 * the same type to set all the elements of the output array. Only overlapping
 * sections of the arrays are copied.
 *
 * @tparam T Type to cast to
 * @param in Input array to copy
 * @param roi Region of interest in input image to copy
 * @param out Output array to write to
 */
template <typename T>
void copyROI_help(shared_ptr<const NDArray> in, const int64_t* inROIL, const
        int64_t* inROIU, shared_ptr<NDArray> out, const int64_t* oROIL, 
        const int64_t* oROIU)
{
    // Set up slicers to iterate through the input and output arrays. Only
    // common dimensions are iterated over, and only the minimum of the two
    // sizes are used for ROI. so a 10x10x10 array cast to a 20x5 array will
    // iterator copy ROI 10x5x1
    OrderConstIter<T> iit(in);
    OrderIter<T> oit(out);

    // perform copy/cast
    iit.setROI(in->ndim(), inROIL, inROIU);
    oit.setROI(out->ndim(), oROIL, oROIU);
    for(iit.goBegin(), oit.goBegin(); !oit.eof() && !iit.eof(); ++oit, ++iit)
        oit.set(*iit);

    if(!oit.eof() || !iit.eof())
        throw INVALID_ARGUMENT("Input image/target have differenct sizes");
}

/**
 * @Brief extracts a region of this image. Zeros in the size variable
 * indicate dimension to be removed.
 *
 * @param len     Length of index/size arrays
 * @param index   Index to start copying from.
 * @param size Size of output image. Note length 0 dimensions will be
 * removed, while length 1 dimensions will be left. 
 * @param newtype Pixel type of output image.
 *
 * @return Image with overlapping sections cast and copied from 'in'
 */


/**
 * @brief Copy an roi from one image to another image. ROI's must be the same
 * size. 
 *
 * @param in Input image (copy pixels from this image)
 * @param inROIL Input ROI, lower bound 
 * @param inROIU Input ROI, upper bound
 * @param out Copy to copy pixels to
 * @param oROIL Output ROI, lower bound
 * @param oROIU Output ROI, upper bound
 * @param newtype Type to cast pixels to during copy
 *
 */
void copyROI(shared_ptr<const NDArray> in, 
        const int64_t* inROIL, const int64_t* inROIU, shared_ptr<NDArray> out,
        const int64_t* oROIL, const int64_t* oROIU, PixelT newtype)
{
	switch(newtype) {
		case UINT8:
			copyROI_help<uint8_t>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		case INT16:
			copyROI_help<int16_t>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		case INT32:
			copyROI_help<int32_t>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		case FLOAT32:
			copyROI_help<float>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		case COMPLEX64:
			copyROI_help<cfloat_t>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		case FLOAT64:
			copyROI_help<double>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		case RGB24:
			copyROI_help<rgb_t>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		case INT8:
			copyROI_help<int8_t>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		case UINT16:
			copyROI_help<uint16_t>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		case UINT32:
			copyROI_help<uint32_t>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		case INT64:
			copyROI_help<int64_t>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		case UINT64:
			copyROI_help<uint64_t>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		case FLOAT128:
			copyROI_help<long double>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		case COMPLEX128:
			copyROI_help<cdouble_t>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		case COMPLEX256:
			copyROI_help<cquad_t>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		case RGBA32:
			copyROI_help<rgba_t>(in, inROIL, inROIU, out, oROIL, oROIU);
			break;
		default:
            throw INVALID_ARGUMENT("Unsupported pixel type: " +
                    to_string(newtype));
	}
}

}

#include "ndarray.txx"

namespace npl {
/******************************************************************************
 * Pre-Compile Array Types
 ******************************************************************************/

template class NDArrayStore<1, double>;
template class NDArrayStore<1, long double>;
template class NDArrayStore<1, cdouble_t>;
template class NDArrayStore<1, cquad_t>;
template class NDArrayStore<1, float>;
template class NDArrayStore<1, cfloat_t>;
template class NDArrayStore<1, int64_t>;
template class NDArrayStore<1, uint64_t>;
template class NDArrayStore<1, int32_t>;
template class NDArrayStore<1, uint32_t>;
template class NDArrayStore<1, int16_t>;
template class NDArrayStore<1, uint16_t>;
template class NDArrayStore<1, int8_t>;
template class NDArrayStore<1, uint8_t>;
template class NDArrayStore<1, rgba_t>;
template class NDArrayStore<1, rgb_t>;

template class NDArrayStore<2, double>;
template class NDArrayStore<2, long double>;
template class NDArrayStore<2, cdouble_t>;
template class NDArrayStore<2, cquad_t>;
template class NDArrayStore<2, float>;
template class NDArrayStore<2, cfloat_t>;
template class NDArrayStore<2, int64_t>;
template class NDArrayStore<2, uint64_t>;
template class NDArrayStore<2, int32_t>;
template class NDArrayStore<2, uint32_t>;
template class NDArrayStore<2, int16_t>;
template class NDArrayStore<2, uint16_t>;
template class NDArrayStore<2, int8_t>;
template class NDArrayStore<2, uint8_t>;
template class NDArrayStore<2, rgba_t>;
template class NDArrayStore<2, rgb_t>;

template class NDArrayStore<3, double>;
template class NDArrayStore<3, long double>;
template class NDArrayStore<3, cdouble_t>;
template class NDArrayStore<3, cquad_t>;
template class NDArrayStore<3, float>;
template class NDArrayStore<3, cfloat_t>;
template class NDArrayStore<3, int64_t>;
template class NDArrayStore<3, uint64_t>;
template class NDArrayStore<3, int32_t>;
template class NDArrayStore<3, uint32_t>;
template class NDArrayStore<3, int16_t>;
template class NDArrayStore<3, uint16_t>;
template class NDArrayStore<3, int8_t>;
template class NDArrayStore<3, uint8_t>;
template class NDArrayStore<3, rgba_t>;
template class NDArrayStore<3, rgb_t>;

template class NDArrayStore<4, double>;
template class NDArrayStore<4, long double>;
template class NDArrayStore<4, cdouble_t>;
template class NDArrayStore<4, cquad_t>;
template class NDArrayStore<4, float>;
template class NDArrayStore<4, cfloat_t>;
template class NDArrayStore<4, int64_t>;
template class NDArrayStore<4, uint64_t>;
template class NDArrayStore<4, int32_t>;
template class NDArrayStore<4, uint32_t>;
template class NDArrayStore<4, int16_t>;
template class NDArrayStore<4, uint16_t>;
template class NDArrayStore<4, int8_t>;
template class NDArrayStore<4, uint8_t>;
template class NDArrayStore<4, rgba_t>;
template class NDArrayStore<4, rgb_t>;

template class NDArrayStore<5, double>;
template class NDArrayStore<5, long double>;
template class NDArrayStore<5, cdouble_t>;
template class NDArrayStore<5, cquad_t>;
template class NDArrayStore<5, float>;
template class NDArrayStore<5, cfloat_t>;
template class NDArrayStore<5, int64_t>;
template class NDArrayStore<5, uint64_t>;
template class NDArrayStore<5, int32_t>;
template class NDArrayStore<5, uint32_t>;
template class NDArrayStore<5, int16_t>;
template class NDArrayStore<5, uint16_t>;
template class NDArrayStore<5, int8_t>;
template class NDArrayStore<5, uint8_t>;
template class NDArrayStore<5, rgba_t>;
template class NDArrayStore<5, rgb_t>;

template class NDArrayStore<6, double>;
template class NDArrayStore<6, long double>;
template class NDArrayStore<6, cdouble_t>;
template class NDArrayStore<6, cquad_t>;
template class NDArrayStore<6, float>;
template class NDArrayStore<6, cfloat_t>;
template class NDArrayStore<6, int64_t>;
template class NDArrayStore<6, uint64_t>;
template class NDArrayStore<6, int32_t>;
template class NDArrayStore<6, uint32_t>;
template class NDArrayStore<6, int16_t>;
template class NDArrayStore<6, uint16_t>;
template class NDArrayStore<6, int8_t>;
template class NDArrayStore<6, uint8_t>;
template class NDArrayStore<6, rgba_t>;
template class NDArrayStore<6, rgb_t>;

template class NDArrayStore<7, double>;
template class NDArrayStore<7, long double>;
template class NDArrayStore<7, cdouble_t>;
template class NDArrayStore<7, cquad_t>;
template class NDArrayStore<7, float>;
template class NDArrayStore<7, cfloat_t>;
template class NDArrayStore<7, int64_t>;
template class NDArrayStore<7, uint64_t>;
template class NDArrayStore<7, int32_t>;
template class NDArrayStore<7, uint32_t>;
template class NDArrayStore<7, int16_t>;
template class NDArrayStore<7, uint16_t>;
template class NDArrayStore<7, int8_t>;
template class NDArrayStore<7, uint8_t>;
template class NDArrayStore<7, rgba_t>;
template class NDArrayStore<7, rgb_t>;

template class NDArrayStore<8, double>;
template class NDArrayStore<8, long double>;
template class NDArrayStore<8, cdouble_t>;
template class NDArrayStore<8, cquad_t>;
template class NDArrayStore<8, float>;
template class NDArrayStore<8, cfloat_t>;
template class NDArrayStore<8, int64_t>;
template class NDArrayStore<8, uint64_t>;
template class NDArrayStore<8, int32_t>;
template class NDArrayStore<8, uint32_t>;
template class NDArrayStore<8, int16_t>;
template class NDArrayStore<8, uint16_t>;
template class NDArrayStore<8, int8_t>;
template class NDArrayStore<8, uint8_t>;
template class NDArrayStore<8, rgba_t>;
template class NDArrayStore<8, rgb_t>;


} // npl


