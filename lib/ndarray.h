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
 * @file ndarray.h
 *
 *****************************************************************************/

/******************************************************************************
 * @file ndarray.h
 * @brief This file contains the definition for NDarray and its derived types.
 * The derived types are templated over dimensionality and pixel type.
 ******************************************************************************/

#ifndef NDARRAY_H
#define NDARRAY_H

#include "npltypes.h"

#include <cstddef>
#include <cmath>
#include <initializer_list>
#include <vector>
#include <cstdint>
#include <complex>
#include <cassert>
#include <memory>


namespace npl {

using std::shared_ptr;

/******************************************************************************
 * Define Types
 *****************************************************************************/
enum PixelT {UNKNOWN_TYPE=0, UINT8=2, INT16=4, INT32=8, FLOAT32=16,
    COMPLEX64=32, CFLOAT=32, FLOAT64=64, RGB24=128, INT8=256, UINT16=512,
    UINT32=768, INT64=1024, UINT64=1280, FLOAT128=1536, CDOUBLE=1792,
    COMPLEX128=1792, CQUAD=2048, COMPLEX256=2048, RGBA32=2304 };

class NDArray;

/******************************************************************************
 * Basic Functions.
 ******************************************************************************/

/**
 * \addtogroup NDarrayUtilities 
 * @{
 */

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
shared_ptr<NDArray> createNDArray(size_t ndim, const size_t* size, PixelT ptype);

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
shared_ptr<NDArray> createNDArray(const std::vector<size_t>& dim, PixelT ptype);

/**
 * @brief Creates a new NDArray with dimensions set by ndim, and size set by
 * size. Output pixel type is decided by ptype variable.
 *
 * @param ndim number of image dimensions
 * @param size size of image, in each dimension
 * @param ptype Pixel type npl::PixelT
 * @param ptr Pointer to data to graft.
 * @param deleter Function to delete ptr
 *
 * @return New image, default orientation
 */
shared_ptr<NDArray> createNDArray(size_t ndim, const size_t* size, 
        PixelT ptype, void* ptr, std::function<void(void*)> deleter);

/**
 * @brief Creates a new NDArray with dimensions set by ndim, and size set by
 * size. Output pixel type is decided by ptype variable.
 *
 * @param dim size of image, in each dimension, number of dimensions decied by
 * length of size vector
 * @param ptype Pixel type npl::PixelT
 * @param ptr Pointer to data to graft.
 * @param deleter Function to delete ptr
 *
 * @return New image, default orientation
 */
shared_ptr<NDArray> createNDArray(const std::vector<size_t>& dim, 
        PixelT ptype, void* ptr, std::function<void(void*)> deleter);

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
        const int64_t* oROIL, const int64_t* oROIU, PixelT newtype);

/** @} NDArrayUtilities */

/******************************************************************************
 * Classes.
 ******************************************************************************/

/**
 * @brief Pure virtual interface to interact with an ND array
 */
class NDArray : public std::enable_shared_from_this<NDArray>
{
public:
	/*
	 * get / set functions
	 */
	// Get Address

	virtual size_t ndim() const = 0;
	virtual size_t bytes() const = 0;
	virtual size_t elements() const = 0;
	virtual size_t dim(size_t dir) const = 0;
	virtual const size_t* dim() const = 0;

	// return type of stored value
	virtual PixelT type() const = 0;
	
	shared_ptr<NDArray> getPtr()  {
		return shared_from_this();
	};
	
	shared_ptr<const NDArray> getConstPtr() const {
		return shared_from_this();
	};

	virtual void* data() = 0;
	virtual const void* data() const = 0;

	/**
	 * @brief Performs a deep copy of the entire array.
	 *
	 * @return Copied array.
	 */
	virtual shared_ptr<NDArray> copy() const = 0;
    
    /**
     * @brief Creates an identical array, but does not initialize pixel values.
	 *
	 * @return New array.
	 */
	virtual shared_ptr<NDArray> createAnother() const = 0;

	/**
	 * @brief Create a new array that is a copy of the input, possibly with new
	 * dimensions and pixeltype. The new array will have all overlapping pixels
	 * copied from the old array.
	 *
	 * @param newdims Number of dimensions in copied output
	 * @param newsize Size of output, this array should be of size newdims
	 * @param newtype Type of pixels in output array
	 *
	 * @return Image with overlapping sections cast and copied from 'in'
	 */
	virtual shared_ptr<NDArray> copyCast(size_t newdims, const size_t* newsize,
			PixelT newtype) const = 0;

	/**
	 * @brief Create a new array that is a copy of the input, with same dimensions
	 * but pxiels cast to newtype. The new array will have all overlapping pixels
	 * copied from the old array.
	 *
	 * @param newtype Type of pixels in output array
	 *
	 * @return Image with overlapping sections cast and copied from 'in'
	 */
	virtual shared_ptr<NDArray> copyCast(PixelT newtype) const = 0;

	/**
	 * @brief Create a new array that is a copy of the input, possibly with new
	 * dimensions or size. The new array will have all overlapping pixels
	 * copied from the old array. The new array will have the same pixel type as
	 * the input array
	 *
	 * @param newdims Number of dimensions in output array
     * @param newsize Input array of length newdims that gives the size of
     *                output array,
	 *
	 * @return Image with overlapping sections cast and copied from 'in'
	 */
	virtual shared_ptr<NDArray> copyCast(size_t newdims,
				const size_t* newsize) const = 0;

    /**
     * @Brief extracts a region of this image. Zeros in the size variable
     * indicate dimension to be removed.
     *
     * @param len     Length of index/newsize arrays
     * @param index   Index to start copying from.
     * @param size Size of output image. Note length 0 dimensions will be
     * removed, while length 1 dimensions will be left. 
     *
     * @return Image with overlapping sections cast and copied from 'in'
     */
    virtual shared_ptr<NDArray> extractCast(size_t len, const int64_t* index,
            const size_t* size) const = 0;

    /**
     * @Brief extracts a region of this image. Zeros in the size variable
     * indicate dimension to be removed.
     *
     * @param len     Length of index/size arrays
     * @param size Size of output image. Note length 0 dimensions will be
     * removed, while length 1 dimensions will be left. 
     *
     * @return Image with overlapping sections cast and copied from 'in'
     */
    virtual shared_ptr<NDArray> extractCast(size_t len, const size_t* size) const = 0;

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
    virtual shared_ptr<NDArray> extractCast(size_t len,
            const int64_t* index, const size_t* size, PixelT newtype) const = 0;

    /**
     * @Brief extracts a region of this image. Zeros in the size variable
     * indicate dimension to be removed.
     *
     * @param len     Length of index/size arrays
     * @param size Size of output image. Note length 0 dimensions will be
     * removed, while length 1 dimensions will be left. 
     * @param newtype Pixel type of output image.
     *
     * @return Image with overlapping sections cast and copied from 'in'
     */
    virtual shared_ptr<NDArray> extractCast(size_t len, const size_t* size, 
            PixelT newtype) const = 0;

	
//	virtual int opself(const NDArray* right, double(*func)(double,double),
//			bool elevR) = 0;
//	virtual std::shared_ptr<NDArray> opnew(const NDArray* right,
//			double(*func)(double,double), bool elevR) = 0;

	virtual void* __getAddr(std::initializer_list<int64_t> index) const = 0;
	virtual void* __getAddr(size_t len, const int64_t* index) const = 0;
	virtual void* __getAddr(const std::vector<int64_t>& index) const = 0;
	virtual void* __getAddr(int64_t i) const = 0;
	virtual void* __getAddr(int64_t x, int64_t y, int64_t z, int64_t t) const = 0;
	
	virtual int64_t getLinIndex(std::initializer_list<int64_t> index) const = 0;
	virtual int64_t getLinIndex(size_t len, const int64_t* index) const = 0;
	virtual int64_t getLinIndex(const std::vector<int64_t>& index) const = 0;
	virtual int64_t getLinIndex(int64_t x, int64_t y, int64_t z, int64_t t) const = 0;
	
	/**
	 * @brief This function just returns the number of elements in a theoretical
	 * fourth dimension (ignoring orgnaization of higher dimensions)
	 *
	 * @return number of elements in the 4th or greater dimensions
	 */
	virtual int64_t tlen() const = 0;

protected:
	NDArray() {} ;

    /**
     * @brief The function which should be called when deleting data. By
     * default this will just be delete[], but if data is grafted or if
     */
    std::function<void(void*)> m_freefunc;
};


/**
 * @brief Basic storage unity for ND array. Creates a big chunk of memory.
 *
 * @tparam D dimension of array
 * @tparam T type of sample
 */
template <size_t D, typename T>
class NDArrayStore : public virtual NDArray
{
public:
	/**
	 * @brief Constructor with initializer list. Orientation will be default
	 * (direction = identity, spacing = 1, origin = 0).
	 *
	 * @param dim dimensions of input, the length of this initializer list
	 * may not be fully used if a_args is longer than D. If it is shorter
	 * then D then additional dimensions are left as size 1.
	 */
	NDArrayStore(const std::initializer_list<size_t>& dim);

	/**
	 * @brief Constructor with vector. Orientation will be default
	 * (direction = identity, spacing = 1, origin = 0).
	 *
	 * @param dim dimensions of input, the length of this initializer list
	 * may not be fully used if a_args is longer than D. If it is shorter
	 * then D then additional dimensions are left as size 1.
	 */
	NDArrayStore(const std::vector<size_t>& dim);
	
	/**
	 * @brief Constructor with array of length len, Orientation will be default
	 * (direction = identity, spacing = 1, origin = 0).
	 *
	 * @param len Length of array 'size'
	 * @param dim dimensions of input, the length of this initializer list
	 * may not be fully used if a_args is longer than D. If it is shorter
	 * then D then additional dimensions are left as size 1.
	 */
	NDArrayStore(size_t len, const size_t* dim);
	
	/**
	 * @brief Constructor which uses a preexsting array, to graft into the
	 * array. No new allocation will be performed, however ownership of the
	 * array will be taken, meaning it could be deleted anytime after this
	 * constructor completes.
	 *
	 * @param len Length of array 'size'
	 * @param dim dimensions of input, the length of this initializer list
	 * may not be fully used if a_args is longer than D. If it is shorter
	 * then D then additional dimensions are left as size 1.
	 * @param ptr Pointer to data array, should be allocated with new, and
	 * size should be exactly sizeof(T)*size[0]*size[1]*...*size[len-1]
	 * @param deleter Function which should be used to delete ptr
	 */
	NDArrayStore(size_t len, const size_t* dim, T* ptr, 
            const std::function<void(void*)>& deleter);
	
	~NDArrayStore() { m_freefunc(_m_data); };

	/*
	 * get / set functions
	 */
	T& operator[](const std::vector<int64_t>& index);
	T& operator[](std::initializer_list<int64_t> index);
	T& operator[](int64_t pixel);
	
	const T& operator[](const std::vector<int64_t>& index) const;
	const T& operator[](std::initializer_list<int64_t> index) const;
	const T& operator[](int64_t pixel) const;

	/*
	 * General Information
	 */
	virtual size_t ndim() const;
	virtual size_t bytes() const;
	virtual size_t elements() const;
	virtual size_t dim(size_t dir) const;
	virtual const size_t* dim() const;

	virtual void resize(const size_t dim[D]);

	// return the pixel type
	virtual PixelT type() const;
	
	/**
	 * @brief Returns a pointer to the data array. Be careful
	 *
	 * @return Pointer to data
	 */
	void* data() {return _m_data; };
	
	/**
	 * @brief Returns a pointer to the data array. Be careful
	 *
	 * @return Pointer to data
	 */
	const void* data() const { return _m_data; };

	/**
	 * @brief Grafts data of the given dimensions into the image, effectively
	 * changing the image size.
	 *
	 * @param dim Dimensions of image
	 * @param ptr Pointer to data which we will take control of
	 * @param deleter Function which can be called on ptr to delete it.
	 */
	void graft(const size_t dim[D], T* ptr, const
            std::function<void(void*)>& deleter);
	
	/**************************************************************************
	 * Duplication Functions
	 *************************************************************************/
	
	/**
	 * @brief Performs a deep copy of the entire array
	 *
	 * @return Copied array.
	 */
	virtual shared_ptr<NDArray> copy() const;
	
    /**
     * @brief Creates an identical array, but does not initialize pixel values.
	 *
	 * @return New array.
	 */
	virtual shared_ptr<NDArray> createAnother() const;

	/**
	 * @brief Create a new array that is a copy of the input, possibly with new
	 * dimensions and pixeltype. The new array will have all overlapping pixels
	 * copied from the old array.
	 *
	 * @param newdims Number of dimensions in output array
     * @param newsize Input array sized 'newdims', indicating size of output
     *                array
	 * @param newtype Type of pixels in output array
	 *
	 * @return Image with overlapping sections cast and copied from 'in'
	 */
	virtual shared_ptr<NDArray> copyCast(size_t newdims, const size_t* newsize,
			PixelT newtype) const;

	/**
	 * @brief Create a new array that is a copy of the input, with same dimensions
	 * but pxiels cast to newtype. The new array will have all overlapping pixels
	 * copied from the old array.
	 *
	 * @param newtype Type of pixels in output array
	 *
	 * @return Image with overlapping sections cast and copied from 'in'
	 */
	virtual shared_ptr<NDArray> copyCast(PixelT newtype) const;

	/**
	 * @brief Create a new array that is a copy of the input, possibly with new
	 * dimensions or size. The new array will have all overlapping pixels
	 * copied from the old array. The new array will have the same pixel type as
	 * the input array
	 *
	 * @param newdims Number of dimensions in output array
	 * @param newsize Size of output array
	 *
	 * @return Image with overlapping sections cast and copied from 'in'
	 */
	virtual shared_ptr<NDArray> copyCast(size_t newdims, const size_t* newsize) const;

    /**
     * @brief Create a new array that is a copy of the input, possibly with new
     * dimensions or size. The new array will have all overlapping pixels
     * copied from the old array. The new array will have the same pixel type as
     * the input array
     *
     * @param len     Length of index/newsize arrays
     * @param index   Index to start copying from.
     * @param size Size of output image. Note length 0 dimensions will be
     * removed, while length 1 dimensions will be left. 
     *
     * @return Image with overlapping sections cast and copied from 'in'
     */
    virtual shared_ptr<NDArray> extractCast(size_t len, const int64_t* index,
            const size_t* size) const;

    /**
     * @brief Create a new array that is a copy of the input, possibly with new
     * dimensions or size. The new array will have all overlapping pixels
     * copied from the old array. The new array will have the same pixel type as
     * the input array. Index assumed to be [0,0,...], so the output image will 
     * start at the origin of this image.
     *
     * @param len     Length of index/size arrays
     * @param size Size of output image. Note length 0 dimensions will be
     * removed, while length 1 dimensions will be left. 
     *
     * @return Image with overlapping sections cast and copied from 'in'
     */
    virtual shared_ptr<NDArray> extractCast(size_t len, const size_t* size) const;

    /**
     * @brief Create a new array that is a copy of the input, possibly with new
     * dimensions or size. The new array will have all overlapping pixels
     * copied from the old array. The new array will have the same pixel type as
     * the input array
     *
     * @param len     Length of index/size arrays
     * @param index   Index to start copying from.
     * @param size Size of output image. Note length 0 dimensions will be
     * removed, while length 1 dimensions will be left. 
     * @param newtype Pixel type of output image.
     *
     * @return Image with overlapping sections cast and copied from 'in'
     */

    virtual shared_ptr<NDArray> extractCast(size_t len,
            const int64_t* index, const size_t* size, PixelT newtype) const;

    /**
     * @brief Create a new array that is a copy of the input, possibly with new
     * dimensions or size. The new array will have all overlapping pixels
     * copied from the old array. The new array will have the same pixel type as
     * the input array. Index assumed to be [0,0,...], so the output image will 
     * start at the origin of this image.
     *
     * @param len     Length of index/size arrays
     * @param size Size of output image. Note length 0 dimensions will be
     * removed, while length 1 dimensions will be left. 
     * @param newtype Pixel type of output image.
     *
     * @return Image with overlapping sections cast and copied from 'in'
     */
    virtual shared_ptr<NDArray> extractCast(size_t len, const size_t* size, 
            PixelT newtype) const;
	/*
	 * Higher Level Operations
	 */
//	virtual int opself(const NDArray* right, double(*func)(double,double),
//			bool elevR);
//	virtual std::shared_ptr<NDArray> opnew(const NDArray* right,
//			double(*func)(double,double), bool elevR);
	
	inline virtual void* __getAddr(std::initializer_list<int64_t> index) const
	{
		return &_m_data[getLinIndex(index)];
	};
	
	inline virtual void* __getAddr(size_t len, const int64_t* index) const
	{
		return &_m_data[getLinIndex(len, index)];
	};

	inline virtual void* __getAddr(const std::vector<int64_t>& index) const
	{
		return &_m_data[getLinIndex(index)];
	};

	inline virtual void* __getAddr(int64_t i) const
	{
		return &_m_data[i];
	};
	inline virtual void* __getAddr(int64_t x, int64_t y, int64_t z, int64_t t) const
	{
		return &_m_data[getLinIndex(x,y,z,t)];
	};

	virtual int64_t getLinIndex(std::initializer_list<int64_t> index) const;
	virtual int64_t getLinIndex(size_t len, const int64_t* index) const;
	virtual int64_t getLinIndex(const std::vector<int64_t>& index) const;
	virtual int64_t getLinIndex(int64_t x, int64_t y, int64_t z, int64_t t) const;

	/**
	 * @brief This function just returns the number of elements in a theoretical
	 * fourth dimension (ignoring orgnaization of higher dimensions)
	 *
	 * @return number of elements in the 4th or greater dimensions
	 */
	virtual int64_t tlen() const {
		if(D >= 3)
			return _m_stride[2];
		else
			return 1;
	};
	
	T* _m_data;
	size_t _m_stride[D]; // steps between pixels
	size_t _m_dim[D];	// overall image dimension

	protected:

	void updateStrides();
	
};


#undef VIRTGETSET
#undef GETSET

} //npl

#endif
