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
 * @file accessors.h Provides accessors to the NDArray data structure. Due to
 * the fact that dimensionality and pixel type are not carried around in the
 * container type, it is necessary to provide generic accessors that will cast
 * input data to the correct type and return it, or take input, cast it to the 
 * underlying type and set the internal pixel. While this might seem 
 * round-about, it allows you to write general purpose algorithms functions
 * without concerning with dimensionality or pixel type.
 *
 *****************************************************************************/

#ifndef ACCESSORS_H
#define ACCESSORS_H

#include <stdexcept>

#include "ndarray.h"
#include "mrimage.h"
#include "basic_functions.h"
#include "utility.h"

namespace npl {

/** \defgroup Accessors Accessors for NDArray/Image 
 *
 * Accessors are used to get and set pixel data. Since
 * the pixel type is hidden in images and arrays, accessors perform the
 * necessary casting. All Accessors have names that end with View or ConstView
 * Thus 
 *
 * \code{.cpp}
 * NDView<double> dacc(img);
 * double v = dacc[index];
 *
 * NDView<std::complex<double>> cacc(img);
 * std::complex<double> c = cacc[index];
 * \endcode
 *
 * Can both be used to access the same data. Of course upon writing or if
 * you need a particular precision you should cast the image to ensure 
 * that you aren't losing anything. While this may seem convoluted it allowes
 * for general purpose coding of functions without having to maintain the 
 * type in every single function declaration. This is what you would
 * effectively do in C if you had a void*.
 *
 * @{
 */

/**
 * @brief This is a basic accessor class, which allows for accessing
 * array data in the type specified by the template.
 *
 * @tparam T Value to return on access
 */
template<typename T>
class NDView
{
public:
	NDView(std::shared_ptr<NDArray> in) : parent(in)
	{
        setArray(in);
	};

    void setArray(ptr<NDArray> in) 
    {
        switch(in->type()) {
            case UINT8:
                castget = castgetStatic<uint8_t>;
                castset = castsetStatic<uint8_t>;
                break;
            case INT8:
                castget = castgetStatic<int8_t>;
                castset = castsetStatic<int8_t>;
                break;
            case UINT16:
                castget = castgetStatic<uint16_t>;
                castset = castsetStatic<uint16_t>;
                break;
            case INT16:
                castget = castgetStatic<int16_t>;
                castset = castsetStatic<int16_t>;
                break;
            case UINT32:
                castget = castgetStatic<uint32_t>;
                castset = castsetStatic<uint32_t>;
                break;
            case INT32:
                castget = castgetStatic<int32_t>;
                castset = castsetStatic<int32_t>;
                break;
            case UINT64:
                castget = castgetStatic<uint64_t>;
                castset = castsetStatic<uint64_t>;
                break;
            case INT64:
                castget = castgetStatic<int64_t>;
                castset = castsetStatic<int64_t>;
                break;
            case FLOAT32:
                castget = castgetStatic<float>;
                castset = castsetStatic<float>;
                break;
            case FLOAT64:
                castget = castgetStatic<double>;
                castset = castsetStatic<double>;
                break;
            case FLOAT128:
                castget = castgetStatic<long double>;
                castset = castsetStatic<long double>;
                break;
            case COMPLEX64:
                castget = castgetStatic<cfloat_t>;
                castset = castsetStatic<cfloat_t>;
                break;
            case COMPLEX128:
                castget = castgetStatic<cdouble_t>;
                castset = castsetStatic<cdouble_t>;
                break;
            case COMPLEX256:
                castget = castgetStatic<cquad_t>;
                castset = castsetStatic<cquad_t>;
                break;
            case RGB24:
                castget = castgetStatic<rgb_t>;
                castset = castsetStatic<rgb_t>;
                break;
            case RGBA32:
                castget = castgetStatic<rgba_t>;
                castset = castsetStatic<rgba_t>;
                break;
            default:
            case UNKNOWN_TYPE:
                castget = castgetStatic<uint8_t>;
                castset = castsetStatic<uint8_t>;
                throw std::invalid_argument("Unknown type to NDView");
                break;
        }
    }

	/**
	 * @brief Gets value linear position in array, then casts to T
	 *
	 * @return value
	 */
	T operator[](int64_t index)
	{
		return castget(this->parent->__getAddr(index));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T get(const std::vector<int64_t>& index)
	{
		return castget(this->parent->__getAddr(index));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param len length of index array
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T get(size_t len, int64_t* index)
	{
		return castget(this->parent->__getAddr(len, index));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T operator[](const std::vector<int64_t>& index)
	{
		return castget(this->parent->__getAddr(index));
	};

	/**
	 * @brief Casts to the appropriate type then sets array at given index.
	 *
	 * @param v value to set at index
	 * @param index n-d index to access
	 *
	 * @return current value
	 */
	void set(const std::vector<int64_t>& index, T v)
	{
		return castset(this->parent->__getAddr(index), v);
	};

	/**
	 * @brief Casts to the appropriate type then sets array at given index.
	 *
	 * @param v value to set at index
	 * @param index n-d index to access
	 *
	 * @return current value
	 */
	void set(int64_t index, T v)
	{
		return castset(this->parent->__getAddr(index), v);
	};

	int64_t tlen() { return this->parent->tlen(); };

protected:


	/**
	 * @brief This is a wrapper function that will be called to safely cast
	 * from the underlying type.
	 *
	 * @tparam U Underlying type of pixel, figured out in the constructor
	 * @param ptr Pointer to memory where the pixel is.
	 *
	 * @return Correctly cast value
	 */
	template <typename U>
	static T castgetStatic(void* ptr)
	{
		return (T)(*(static_cast<U*>(ptr)));
	};

	/**
	 * @brief This is a wrapper function that will be called to safely cast
	 * to the underlying type.
	 *
	 * @tparam U Underlying type of pixel, figured out in the constructor
	 * @param ptr Pointer to memory where the pixel is.
	 * @param val new value to write
	 *
	 */
	template <typename U>
	static void castsetStatic(void* ptr, const T& val)
	{
		(*(static_cast<U*>(ptr))) = (U)val;
	};

	/**
	 * @brief Where to get the dat a from. Also the shared_ptr prevents dealloc
	 */
	std::shared_ptr<NDArray> parent;

	/**
	 * @brief Function pointer to the correct function for casting from the
	 * underlying type
	 *
	 * @param ptr location in memory where the pixel is stored
	 */
	T (*castget)(void* ptr);

	/**
	 * @brief Function pointer to the correct function for casting to the
	 * underlying type. This should be set during construction.
	 *
	 *
	 * @param ptr pointer to memory
	 * @param v value to cast and write
	 */
	void (*castset)(void* ptr, const T& v);

};

/**
 * @brief This is a basic accessor class, which allows for accessing
 * array data in the type specified by the template.
 *
 * @tparam T Value to return on access
 */
template<typename T>
class NDConstView
{
public:
	NDConstView(std::shared_ptr<const NDArray> in) : parent(in)
    {
        setArray(in);
    }

    void setArray(ptr<const NDArray> in)
	{
		switch(in->type()) {
			case UINT8:
				castget = castgetStatic<uint8_t>;
				break;
			case INT8:
				castget = castgetStatic<int8_t>;
				break;
			case UINT16:
				castget = castgetStatic<uint16_t>;
				break;
			case INT16:
				castget = castgetStatic<int16_t>;
				break;
			case UINT32:
				castget = castgetStatic<uint32_t>;
				break;
			case INT32:
				castget = castgetStatic<int32_t>;
				break;
			case UINT64:
				castget = castgetStatic<uint64_t>;
				break;
			case INT64:
				castget = castgetStatic<int64_t>;
				break;
			case FLOAT32:
				castget = castgetStatic<float>;
				break;
			case FLOAT64:
				castget = castgetStatic<double>;
				break;
			case FLOAT128:
				castget = castgetStatic<long double>;
				break;
			case COMPLEX64:
				castget = castgetStatic<cfloat_t>;
				break;
			case COMPLEX128:
				castget = castgetStatic<cdouble_t>;
				break;
			case COMPLEX256:
				castget = castgetStatic<cquad_t>;
				break;
			case RGB24:
				castget = castgetStatic<rgb_t>;
				break;
			case RGBA32:
				castget = castgetStatic<rgba_t>;
				break;
			default:
			case UNKNOWN_TYPE:
				castget = castgetStatic<uint8_t>;
				throw std::invalid_argument("Unknown type to NDConstView");
				break;
		}
	};

	/**
	 * @brief Gets value linear position in array, then casts to T
	 *
	 * @return value
	 */
	T operator[](int64_t index)
	{
		return castget(this->parent->__getAddr(index));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T get(const std::vector<int64_t>& index)
	{
		return castget(this->parent->__getAddr(index));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param len length of index array
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T get(size_t len, int64_t* index)
	{
		return castget(this->parent->__getAddr(len, index));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T operator[](const std::vector<int64_t>& index)
	{
		return castget(this->parent->__getAddr(index));
	};

	int64_t tlen() { return this->parent->tlen(); };

protected:


	/**
	 * @brief This is a wrapper function that will be called to safely cast
	 * from the underlying type.
	 *
	 * @tparam U Underlying type of pixel, figured out in the constructor
	 * @param ptr Pointer to memory where the pixel is.
	 *
	 * @return Correctly cast value
	 */
	template <typename U>
	static T castgetStatic(void* ptr)
	{
		return (T)(*(static_cast<U*>(ptr)));
	};

	/**
	 * @brief Where to get the dat a from. Also the shared_ptr prevents dealloc
	 */
	std::shared_ptr<const NDArray> parent;

	/**
	 * @brief Function pointer to the correct function for casting from the
	 * underlying type
	 *
	 * @param ptr location in memory where the pixel is stored
	 */
	T (*castget)(void* ptr);

};

/**
 * @brief The purpose of this class is to view an image as a 3D
 * image rather than a ND image. Therefore all dimensions above the third will
 * be ignored and index 0 will be used.
 *
 * @tparam T Type of value to cast and return
 */
template<typename T>
class Pixel3DView : public NDView<T>
{
public:
	Pixel3DView(std::shared_ptr<NDArray> in) : NDView<T>(in)
	{ };

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T operator()(int64_t x=0, int64_t y=0, int64_t z=0)
	{
		return this->castget(this->parent->__getAddr(x,y,z,0));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T get(int64_t x, int64_t y, int64_t z)
	{
		return this->castget(this->parent->__getAddr(x,y,z,0));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	void set(int64_t x, int64_t y, int64_t z, T v)
	{
		this->castset(this->parent->__getAddr(x,y,z,0), v);
	};

protected:

	// Remove functions that aren't relevent from NDView
	T operator[](int64_t i) { (void)(i); return T(); };
	T get(const std::vector<int64_t>& i) {  (void)(i); return T(); };
	T operator[](const std::vector<int64_t>& i) { (void)(i); return T(); };
};

/**
 * @brief The purpose of this class is to view an image as a 3D+vector dimension
 * image rather than a 4+D image. Therefore all dimensions above the third are
 * cast as a vector. If there is demand I may create a matrixx verion as well
 *
 * @tparam T Type of value to cast and return
 */
template<typename T>
class Vector3DConstView : public NDConstView<T>
{
public:
	Vector3DConstView(std::shared_ptr<const NDArray> in) : NDConstView<T>(in)
	{ };

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
    virtual
	T operator()(int64_t x=0, int64_t y=0, int64_t z=0, int64_t t=0)
	{
		return this->castget(this->parent->__getAddr(x,y,z,t));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
    virtual
	T get(int64_t x=0, int64_t y=0, int64_t z=0, int64_t t=0)
	{
		return this->castget(this->parent->__getAddr(x,y,z,t));
	};

private:
    //////////////////////////////////////////////////////
    // Hide Non-3D Functrions from NDConstView
    //////////////////////////////////////////////////////
	
    /**
	 * @brief Gets value linear position in array, then casts to T
	 *
	 * @return value
	 */
	T operator[](int64_t index)
	{
		return castget(this->parent->__getAddr(index));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T get(const std::vector<int64_t>& index)
	{
		return castget(this->parent->__getAddr(index));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param len length of index array
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T get(size_t len, int64_t* index)
	{
		return castget(this->parent->__getAddr(len, index));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T operator[](const std::vector<int64_t>& index)
	{
		return castget(this->parent->__getAddr(index));
	};

};

/**
 * @brief The purpose of this class is to view an image as a 3D+vector dimension
 * image rather than a 4+D image. Therefore all dimensions above the third are
 * cast as a vector. If there is demand I may create a matrixx verion as well
 *
 * @tparam T Type of value to cast and return
 */
template<typename T>
class Vector3DView : public NDView<T>
{
public:
	Vector3DView(std::shared_ptr<NDArray> in) : NDView<T>(in)
	{ };

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T operator()(int64_t x=0, int64_t y=0, int64_t z=0, int64_t t=0)
	{
		return this->castget(this->parent->__getAddr(x,y,z,t));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T get(int64_t x=0, int64_t y=0, int64_t z=0, int64_t t=0)
	{
		return this->castget(this->parent->__getAddr(x,y,z,t));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	void set(int64_t x, int64_t y, int64_t z, int64_t t, T v)
	{
		this->castset(this->parent->__getAddr(x,y,z,t), v);
	};
private:
    //////////////////////////////////////////////////////
    // Hide Non-3D Functrions from NDConstView
    //////////////////////////////////////////////////////
	
    /**
	 * @brief Gets value linear position in array, then casts to T
	 *
	 * @return value
	 */
	T operator[](int64_t index)
	{
		return castget(this->parent->__getAddr(index));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T get(const std::vector<int64_t>& index)
	{
		return castget(this->parent->__getAddr(index));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param len length of index array
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T get(size_t len, int64_t* index)
	{
		return castget(this->parent->__getAddr(len, index));
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T operator[](const std::vector<int64_t>& index)
	{
		return castget(this->parent->__getAddr(index));
	};

	/**
	 * @brief Casts to the appropriate type then sets array at given index.
	 *
	 * @param v value to set at index
	 * @param index n-d index to access
	 *
	 * @return current value
	 */
	void set(const std::vector<int64_t>& index, T v)
	{
		return castset(this->parent->__getAddr(index), v);
	};

	/**
	 * @brief Casts to the appropriate type then sets array at given index.
	 *
	 * @param v value to set at index
	 * @param index n-d index to access
	 *
	 * @return current value
	 */
	void set(int64_t index, T v)
	{
		return castset(this->parent->__getAddr(index), v);
	};

};

/****************************************
 * Interpolators
 ***************************************/

////////////////
//Linear
///////////////

/* Linear Kernel Sampling */
double linKern(double x)
{
	return fabs(1-fmin(1,fabs(x)));
}

/**
 * @brief The purpose of this class is to view an image as a continuous
 * ND image and to sample at a continuous ND-position within. 
 *
 * @tparam T Type of value to cast and return
 */
template<typename T>
class LinInterpNDView : public NDConstView<T>
{
public:
	LinInterpNDView(std::shared_ptr<const NDArray> in,
				BoundaryConditionT bound = ZEROFLUX)
				: NDConstView<T>(in), m_boundmethod(bound), m_ras(false)
	{ };

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param x	x-dimension 
	 * @param y	y-dimension 
	 * @param z	z-dimension 
	 * @param t	4th dimension 
	 * @param u	5th dimension 
	 * @param v	6th dimension 
	 * @param w	7th dimension 
	 * @param q	8th dimension 
	 *
	 * @return value Interpolated value at given position
	 */
	T operator()(double x=0, double y=0, double z=0, double t=0, double u = 0,
			double v = 0, double w = 0, double q = 0)
	{
		double tmp[8] = {x,y,z,t,u,v,w,q};
		return get(8, tmp);
	};


	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index  Position in ND-space to interpolate
	 *
	 * @return value Interpolated value at given position
	 */
	T operator()(const std::vector<float>& index)
	{
        assert(index.size() <= 8);
        double tmp[8];
        size_t ii=0;
        for(auto it = index.begin(); it != index.end() && ii<8; ++it, ++ii) 
            tmp[ii] = *it;
		return get(std::min(8UL, index.size()), tmp);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index  Position in ND-space to interpolate
	 *
	 * @return value Interpolated value at given position
	 */
	T operator()(const std::vector<double>& index)
	{
		return get(index.size(), index.data());
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index  Position in ND-space to interpolate
	 *
	 * @return value Interpolated value at given position
	 */
	T operator()(std::initializer_list<double> index)
	{
        assert(index.size() <= 8);
        double tmp[8];
        size_t ii=0;
        for(auto it = index.begin(); it != index.end() && ii<8; ++it, ++ii) 
            tmp[ii] = *it;
		return get(std::min(8UL, index.size()), tmp);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index  Position in ND-space to interpolate
	 *
	 * @return value Interpolated value at given position
	 */
	T operator()(std::initializer_list<float> index)
	{
        assert(index.size() <= 8);
        double tmp[8];
        size_t ii=0;
        for(auto it = index.begin(); it != index.end() && ii<8; ++it, ++ii) 
            tmp[ii] = *it;
		return get(std::min(8UL, index.size()), tmp);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T get(const vector<double>& cindex)
	{
        get(cindex.size(), cindex.data());
    }

    /**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T get(const std::vector<int64_t>& index)
	{
        assert(index.size() <= 8);
        double tmp[8];
        size_t ii=0;
        for(auto it = index.begin(); it != index.end() && ii<8; ++it, ++ii) 
            tmp[ii] = *it;
		return get(std::min(8UL, index.size()), tmp);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param len length of index array
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T get(size_t len, int64_t* index)
	{
        assert(len <= 8);
        double tmp[8];
        size_t ii=0;
        for(size_t ii=0; ii<len && ii<8; ii++) 
            tmp[ii] = index[ii];
		return get(std::min(8UL, len), tmp);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T operator[](const std::vector<int64_t>& index)
	{
        assert(index.size() <= 8);
        double tmp[8];
        size_t ii=0;
        for(auto it = index.begin(); it != index.end() && ii<8; ++it, ++ii) 
            tmp[ii] = *it;
		return get(std::min(8UL, index.size()), tmp);
	};

    /**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T get(size_t len, const double* incindex)
	{
		// initialize variables
		int ndim = this->parent->ndim();
        const size_t* dim = this->parent->dim();

        // convert RAS to index
        vector<double> cindex(incindex, incindex+len);
        if(m_ras) {
            auto tmp = dPtrCast<const MRImage>(this->parent);
            tmp->pointToIndex(len, cindex.data(), cindex.data());
        }

		vector<int64_t> index(ndim, 0);
		const int KPOINTS = 2;

		// 1D version of the weights and indices
		vector<vector<double>> karray(ndim, vector<double>(KPOINTS));
		vector<vector<int64_t>> indarray(ndim, vector<int64_t>(KPOINTS));

		for(int dd = 0; dd < ndim && dd<len; dd++) {
			indarray[dd][0] = floor(cindex[dd]);
			indarray[dd][1] = indarray[dd][0]+1; //make sure they aren't the same
			karray[dd][0] = linKern(indarray[dd][0]-cindex[dd]);
			karray[dd][1] = linKern(indarray[dd][1]-cindex[dd]);
		}
        // defualt missing values in cindex to 0
		for(int dd = 0; dd < ndim; dd++) {
			indarray[dd][0] = floor(0);
			indarray[dd][1] = indarray[dd][0]+1; //make sure they aren't the same
			karray[dd][0] = linKern(indarray[dd][0]-0);
			karray[dd][1] = linKern(indarray[dd][1]-0);
		}

		bool iioutside = false;
//		outside = false;

		// compute weighted pixval by iterating over neighbors, which are
		// combinations of KPOINTS
		T pixval = 0;
		double weight = 0;
		div_t result;
		for(int ii = 0 ; ii < pow(KPOINTS, ndim); ii++) {
			weight = 1;

			//set index
			result.quot = ii;
			iioutside = false;
			for(int dd = 0; dd < ndim; dd++) {
				result = std::div(result.quot, KPOINTS);
				weight *= karray[dd][result.rem];
				index[dd] = indarray[dd][result.rem];
				iioutside = iioutside || index[dd] < 0 || index[dd] >= dim[dd];
			}

            // might prevent optimization, but also helps with NAN's in 0
            // weighted regions
            if(weight == 0)
                continue;

			// if the current point maps outside, then we need to deal with it
//			outside = (weight != 0 && iioutside) || outside;
			if(iioutside) {
				if(m_boundmethod == ZEROFLUX) {
					// clamp
					for(size_t dd=0; dd<ndim; dd++)
						index[dd] = clamp<int64_t>(0, dim[dd]-1, index[dd]);
				} else if(m_boundmethod == WRAP) {
					// wrap
					for(size_t dd=0; dd<ndim; dd++)
						index[dd] = wrap<int64_t>(0, dim[dd]-1, index[dd]);
				} else {
					// set wieght to zero, then just clamp
					weight = 0;
					for(size_t dd=0; dd<ndim; dd++)
						index[dd] = clamp<int64_t>(0, dim[dd]-1, index[dd]);
				}
			}

			T v = this->castget(this->parent->__getAddr(index));
			pixval += weight*v;
		}

		return pixval;
	}


	BoundaryConditionT m_boundmethod;

    /**
     * @brief if true, then this assumes the inputs are RAS coordinates rather
     * than indexes. Default is false
     */
    bool m_ras;

protected:

	/**
	 * @brief Gets value at array index and then casts to T
     * doesn't make sense for interpolation
	 *
	 * @return value
	 */
	T operator[](int64_t i) { (void)(i); return T(); };
};

/**
 * @brief The purpose of this class is to view an image as a continuous
 * 3D+vector dimension image rather than a 4+D image. Therefore all dimensions
 * above the third are cast as a vector and interpolation is only performed
 * between 3D points, with the 4th dimension assumed to be non-spatial. The
 * would be applicable if the upper dimensions are of a different type
 * than the first 3.
 *
 * @tparam T Type of value to cast and return
 */
template<typename T>
class LinInterp3DView : public Vector3DConstView<T>
{
public:
	LinInterp3DView(std::shared_ptr<const NDArray> in,
				BoundaryConditionT bound = ZEROFLUX)
				: Vector3DConstView<T>(in), m_boundmethod(bound), m_ras(false)
	{ };

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T operator()(double x=0, double y=0, double z=0, int64_t t=0)
	{
		return get(x,y,z,t);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T get(double x=0, double y=0, double z=0, int64_t t=0)
	{
		// figure out size of dimensions in parent
		size_t dim[4];
		dim[0] = this->parent->dim(0);
		dim[1] = this->parent->ndim() > 1 ? this->parent->dim(1) : 1;
		dim[2] = this->parent->ndim() > 2 ? this->parent->dim(2) : 1;
		dim[3] = this->parent->tlen();

		// deal with t being outside bounds
		if(t < 0 || t >= dim[3]) {
			if(m_boundmethod == ZEROFLUX) {
				// clamp
				t = clamp<int64_t>(0, dim[3]-1, t);
			} else if(m_boundmethod == WRAP) {
				// wrap
				t = wrap<int64_t>(0, dim[3]-1, t);
			} else {
				return 0;
			}
		}

		// initialize variables
		double cindex[3] = {x,y,z};

        // convert RAS to cindex
        if(m_ras) {
            auto tmp = dPtrCast<const MRImage>(this->parent);
            tmp->pointToIndex(3, cindex, cindex);
        }

		int64_t index[3];
		const int KPOINTS = 2;
		const int DIM = 3;

		// 1D version of the weights and indices
		double karray[DIM][KPOINTS];
		int64_t indarray[DIM][KPOINTS];

		for(int dd = 0; dd < DIM; dd++) {
			indarray[dd][0] = floor(cindex[dd]);
			indarray[dd][1] = indarray[dd][0]+1; //make sure they aren't the same
			karray[dd][0] = linKern(indarray[dd][0]-cindex[dd]);
			karray[dd][1] = linKern(indarray[dd][1]-cindex[dd]);
		}

		bool iioutside = false;
//		outside = false;

		// compute weighted pixval by iterating over neighbors, which are
		// combinations of KPOINTS
		T pixval = 0;
		double weight = 0;
		div_t result;
		for(int ii = 0 ; ii < pow(KPOINTS, DIM); ii++) {
			weight = 1;

			//set index
			result.quot = ii;
			iioutside = false;
			for(int dd = 0; dd < DIM; dd++) {
				result = std::div(result.quot, KPOINTS);
				weight *= karray[dd][result.rem];
				index[dd] = indarray[dd][result.rem];
				iioutside = iioutside || index[dd] < 0 || index[dd] >= dim[dd];
			}

			// might prevent optimization
			//			if(weight == 0)
			//				continue;

			// if the current point maps outside, then we need to deal with it
//			outside = (weight != 0 && iioutside) || outside;
			if(iioutside) {
				if(m_boundmethod == ZEROFLUX) {
					// clamp
					for(size_t dd=0; dd<DIM; dd++)
						index[dd] = clamp<int64_t>(0, dim[dd]-1, index[dd]);
				} else if(m_boundmethod == WRAP) {
					// wrap
					for(size_t dd=0; dd<DIM; dd++)
						index[dd] = wrap<int64_t>(0, dim[dd]-1, index[dd]);
				} else {
					// set wieght to zero, then just clamp
					weight = 0;
					for(size_t dd=0; dd<DIM; dd++)
						index[dd] = clamp<int64_t>(0, dim[dd]-1, index[dd]);
				}
			}

			T v = this->castget(this->parent->__getAddr(index[0], index[1],index[2],t));
			pixval += weight*v;
		}

		return pixval;
	}
	
    /**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T operator()(int64_t x=0, int64_t y=0, int64_t z=0, int64_t t=0)
	{
		return get((double)x,(double)y,(double)z,t);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T get(int64_t x=0, int64_t y=0, int64_t z=0, int64_t t=0)
	{
		return get((double)x,(double)y,(double)z,t);
	};

	BoundaryConditionT m_boundmethod;

    /**
     * @brief if true, then this assumes the inputs are RAS coordinates rather
     * than indexes. Default is false
     */
    bool m_ras;

};

/**
 * @brief General purpose Nearest-Neighbor interpolator
 *
 * @tparam T Type of value to cast and return
 */
template<typename T>
class NNInterpNDView : public NDConstView<T>
{
public:
	NNInterpNDView(std::shared_ptr<const NDArray> in,
				BoundaryConditionT bound = ZEROFLUX)
				: NDConstView<T>(in), m_boundmethod(bound), m_ras(false)
	{ };

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param x	x-dimension 
	 * @param y	y-dimension 
	 * @param z	z-dimension 
	 * @param t	4th dimension 
	 * @param u	5th dimension 
	 * @param v	6th dimension 
	 * @param w	7th dimension 
	 * @param q	8th dimension 
	 *
	 * @return value Interpolated value at given position
	 */
	T operator()(double x=0, double y=0, double z=0, double t=0, double u = 0,
			double v = 0, double w = 0, double q = 0)
	{
		double tmp[8] = {x,y,z,t,u,v,w,q};
		return get(8, tmp);
	};


	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index  Position in ND-space to interpolate
	 *
	 * @return value Interpolated value at given position
	 */
	T operator()(const std::vector<float>& index)
	{
        assert(index.size() <= 8);
        double tmp[8];
        size_t ii=0;
        for(auto it = index.begin(); it != index.end() && ii<8; ++it, ++ii) 
            tmp[ii] = *it;
		return get(std::min(8UL, index.size()), tmp);
	};
	
    /**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T get(const std::vector<int64_t>& index)
	{
        assert(index.size() <= 8);
        double tmp[8];
        size_t ii=0;
        for(auto it = index.begin(); it != index.end() && ii<8; ++it, ++ii) 
            tmp[ii] = *it;
		return get(std::min(8UL, index.size()), tmp);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param len length of index array
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T get(size_t len, int64_t* index)
	{
        assert(len <= 8);
        double tmp[8];
        for(size_t ii=0; ii < len && ii<8; ++ii) 
            tmp[ii] = index[ii];
		return get(std::min(8UL, len), tmp);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T operator[](const std::vector<int64_t>& index)
	{
        assert(index.size() <= 8);
        double tmp[8];
        size_t ii=0;
        for(auto it = index.begin(); it != index.end() && ii<8; ++it, ++ii) 
            tmp[ii] = *it;
		return get(std::min(8UL, index.size()), tmp);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index  Position in ND-space to interpolate
	 *
	 * @return value Interpolated value at given position
	 */
	T operator()(const std::vector<double>& index)
	{
		return get(index.size(), index.data());
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index  Position in ND-space to interpolate
	 *
	 * @return value Interpolated value at given position
	 */
	T operator()(std::initializer_list<double> index)
	{
        assert(index.size() <= 8);
        double tmp[8];
        size_t ii=0;
        for(auto it = index.begin(); it != index.end() && ii<8; ++it, ++ii) 
            tmp[ii] = *it;
		return get(std::min(8UL, index.size()), tmp);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index  Position in ND-space to interpolate
	 *
	 * @return value Interpolated value at given position
	 */
	T operator()(std::initializer_list<float> index)
	{
        assert(index.size() <= 8);
        double tmp[8];
        size_t ii=0;
        for(auto it = index.begin(); it != index.end() && ii<8; ++it, ++ii) 
            tmp[ii] = *it;
		return get(std::min(8UL, index.size()), tmp);
	};

    /**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T get(size_t len, const double* incindex)
	{
        // convert RAS to index
        vector<double> cindex(incindex, incindex+len);
        if(m_ras) {
            auto tmp = dPtrCast<const MRImage>(this->parent);
            tmp->pointToIndex(len, cindex.data(), cindex.data());
        }


		// initialize variables
		size_t ndim = this->parent->ndim();
        const size_t* dim = this->parent->dim();

        // round values from cindex
		vector<int64_t> index(ndim,0);
        if(m_boundmethod == ZEROFLUX) {
            // clamp
            for(size_t dd=0; dd<ndim&& dd<len; dd++)
                index[dd] = clamp<int64_t>(0, dim[dd]-1, round(cindex[dd]));
        } else if(m_boundmethod == WRAP) {
            // wrap
            for(size_t dd=0; dd<ndim; dd++)
                index[dd] = wrap<int64_t>(0, dim[dd]-1, round(index[dd]));
        } else {
            for(size_t dd=0; dd<ndim; dd++) {
                index[dd] = round(cindex[dd]);
                if(index[dd] < 0 || index[dd] > dim[dd])
                    return 0;
            }
        }

        return this->castget(this->parent->__getAddr(index));
	}

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T get(const vector<double>& cindex)
	{
        get(cindex.size(), cindex.data());
    }

	BoundaryConditionT m_boundmethod;

    /**
     * @brief if true, then this assumes the inputs are RAS coordinates rather
     * than indexes. Default is false
     */
    bool m_ras;

private:
    ///////////////////////////////////////////////////////////
    // Hide Unused Functions From Parent
    ///////////////////////////////////////////////////////////
	
    /**
	 * @brief Gets value linear position in array, then casts to T
	 *
	 * @return value
	 */
	T operator[](int64_t index)
	{
		return castget(this->parent->__getAddr(index));
	};
};

/**
 * @brief The purpose of this class is to view an image as a continuous
 * 3D+vector dimension image rather than a 4+D image. Therefore all dimensions
 * above the third are cast as a vector and interpolation is only performed
 * between 3D points, with the 4th dimension assumed to be non-spatial. The
 * would be applicable if the upper dimensions are of a different type
 * than the first 3.
 *
 * @tparam T Type of value to cast and return
 */
template<typename T>
class NNInterp3DView : public Vector3DConstView<T>
{
public:
	NNInterp3DView(std::shared_ptr<NDArray> in,
				BoundaryConditionT bound = ZEROFLUX)
				: Vector3DConstView<T>(in), m_boundmethod(bound), m_ras(false)
	{ };

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T operator()(double x=0, double y=0, double z=0, int64_t t=0)
	{
		return get(x,y,z,t);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T get(double x=0, double y=0, double z=0, int64_t t=0)
	{
        // convert RAS to cindex
        if(m_ras) {
            double cindex[3] = {x,y,z};
            auto tmp = dPtrCast<const MRImage>(this->parent);
            tmp->pointToIndex(3, cindex, cindex);
            x = cindex[0];
            y = cindex[1];
            z = cindex[2];
        }


		// interpolate
		int64_t i = round(x);
		int64_t j = round(y);
		int64_t k = round(z);
		size_t xdim = this->parent->dim(0);
		size_t ydim = this->parent->ndim() > 1 ? this->parent->dim(1) : 1;
		size_t zdim = this->parent->ndim() > 2 ? this->parent->dim(2) : 1;
		size_t tdim = this->parent->tlen();

		bool xout = (i < 0 || i >= xdim);
		bool yout = (j < 0 || j >= ydim);
		bool zout = (k < 0 || k >= zdim);
		bool tout = (t < 0 || t >= tdim);

		if(xout || yout || zout || tout) {
//			outside = true;
			switch(m_boundmethod) {
				case ZEROFLUX:
					i = clamp<int64_t>(0, xdim-1, i);
					j = clamp<int64_t>(0, ydim-1, j);
					k = clamp<int64_t>(0, zdim-1, k);
					t = clamp<int64_t>(0, tdim-1, t);
					break;
				case WRAP:
					i = wrap<int64_t>(0, xdim-1, i);
					j = wrap<int64_t>(0, ydim-1, j);
					k = wrap<int64_t>(0, zdim-1, k);
					t = wrap<int64_t>(0, tdim-1, t);
					break;
				case CONSTZERO:
				default:
					return 0;
					break;
			}
		}

		return this->castget(this->parent->__getAddr(i,j,k,t));
	};
	
    /**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T operator()(int64_t x=0, int64_t y=0, int64_t z=0, int64_t t=0)
	{
		return get((double)x, (double)y, (double)z, t);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T get(int64_t x=0, int64_t y=0, int64_t z=0, int64_t t=0)
	{
		return get((double)x, (double)y, (double)z, t);
	};


	BoundaryConditionT m_boundmethod;;

    /**
     * @brief if true, then this assumes the inputs are RAS coordinates rather
     * than indexes. Default is false
     */
    bool m_ras;

private:

};

////////////////////////
// Lanczos
////////////////////////

/**
 * @brief The purpose of this class is to view an image as a continuous
 * 3D+vector dimension image rather than a 4+D image. Therefore all dimensions
 * above the third are cast as a vector and interpolation is only performed
 * between 3D points, with the 4th dimension assumed to be non-spatial. The
 * would be applicable if the upper dimensions are of a different type
 * than the first 3.
 *
 * @tparam T Type of value to cast and return
 */
template<typename T>
class LanczosInterpNDView : public NDConstView<T>
{
public:
	LanczosInterpNDView(std::shared_ptr<const NDArray> in,
				BoundaryConditionT bound = ZEROFLUX)
				: NDConstView<T>(in), m_boundmethod(bound), m_ras(false), 
                m_radius(2)
	{ };

	void setRadius(size_t rad) { m_radius = rad; };
	size_t getRadius() { return m_radius; };

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
    T operator()(double x=0, double y=0, double z=0, int64_t t=0, double u=0,
            double v=0, double w=0)
	{
		double tmp[8] = {x,y,z,t,u,v,w};
		return get(8, tmp);
	};
	
	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T get(const std::vector<int64_t>& index)
	{
        assert(index.size() <= 8);
        double tmp[8];
        size_t ii=0;
        for(auto it = index.begin(); it != index.end() && ii<8; ++it, ++ii) 
            tmp[ii] = *it;
		return get(std::min(8UL, index.size()), tmp);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param len length of index array
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T get(size_t len, int64_t* index)
	{
        assert(len <= 8);
        double tmp[8];
        size_t ii=0;
        for(size_t ii=0; ii<len && ii<8; ii++) 
            tmp[ii] = index[ii];
		return get(std::min(8UL, len), tmp);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @param index n-d index to access
	 *
	 * @return value
	 */
	T operator[](const std::vector<int64_t>& index)
	{
        assert(index.size() <= 8);
        double tmp[8];
        size_t ii=0;
        for(auto it = index.begin(); it != index.end() && ii<8; ++it, ++ii) 
            tmp[ii] = *it;
		return get(std::min(8UL, index.size()), tmp);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T get(size_t len, const double* incoord)
	{
		// figure out size of dimensions in parent
		const size_t* dim = this->parent->dim();
        size_t ndim = this->parent->ndim();

        // convert RAS to index
        vector<double> cindex(incoord, incoord+len);
        if(m_ras) {
            auto tmp = dPtrCast<const MRImage>(this->parent);
            tmp->pointToIndex(len, cindex.data(), cindex.data());
        }

		// initialize variables
		vector<int64_t> index(ndim, 0);
		int kpoints = 1+m_radius*2;

		// 1D version of the weights and indices
		vector<vector<double>> karray(ndim, vector<double>(kpoints, 0));
		vector<vector<int64_t>> indarray(ndim, vector<int64_t>(kpoints, 0));

		for(int dd = 0; dd < ndim; dd++) {
			for(int64_t ii=-m_radius; ii<=m_radius; ii++){
                double C = dd < len ? cindex[dd] : 0; 
				int64_t i = round(C)+ii;
				indarray[dd][ii+m_radius] = i;
				karray[dd][ii+m_radius] = lanczosKern(i-C, m_radius);
			}
		}

		bool iioutside = false;

		// compute weighted pixval by iterating over neighbors, which are
		// combinations of KPOINTS
		T pixval = 0;
		double weight = 0;
		div_t result;
		for(int ii = 0 ; ii < pow(kpoints, ndim); ii++) {
			weight = 1;

			//set index
			result.quot = ii;
			iioutside = false;
			for(int dd = 0; dd < ndim; dd++) {
				result = std::div(result.quot, kpoints);
				weight *= karray[dd][result.rem];
				index[dd] = indarray[dd][result.rem];
				iioutside = iioutside || index[dd] < 0 || index[dd] >= dim[dd];
			}

			if(iioutside) {
				if(m_boundmethod == ZEROFLUX) {
					// clamp
					for(size_t dd=0; dd<ndim; dd++)
						index[dd] = clamp<int64_t>(0, dim[dd]-1, index[dd]);
				} else if(m_boundmethod == WRAP) {
					// wrap
					for(size_t dd=0; dd<ndim; dd++)
						index[dd] = wrap<int64_t>(0, dim[dd]-1, index[dd]);
				} else { // zero outside
					// set wieght to zero, then just clamp
					weight = 0;
					for(size_t dd=0; dd<ndim; dd++)
						index[dd] = clamp<int64_t>(0, dim[dd]-1, index[dd]);
				}
			}

			T v = this->castget(this->parent->__getAddr(index));
			pixval += weight*v;
		}

		return pixval;
	}

	BoundaryConditionT m_boundmethod;

    /**
     * @brief if true, then this assumes the inputs are RAS coordinates rather
     * than indexes. Default is false
     */
    bool m_ras;

protected:

    /**
	 * @brief Gets value linear position in array, then casts to T
	 *
	 * @return value
	 */
	T operator[](int64_t index)
	{
		return castget(this->parent->__getAddr(index));
	};

	int64_t m_radius;
};


/**
 * @brief The purpose of this class is to view an image as a continuous
 * 3D+vector dimension image rather than a 4+D image. Therefore all dimensions
 * above the third are cast as a vector and interpolation is only performed
 * between 3D points, with the 4th dimension assumed to be non-spatial. The
 * would be applicable if the upper dimensions are of a different type
 * than the first 3.
 *
 * @tparam T Type of value to cast and return
 */
template<typename T>
class LanczosInterp3DView : public Vector3DConstView<T>
{
public:
	LanczosInterp3DView(std::shared_ptr<const NDArray> in,
				BoundaryConditionT bound = ZEROFLUX)
                : NDConstView<T>(in), m_boundmethod(bound), m_ras(false),
                m_radius(2)
	{ };

	void setRadius(size_t rad) { m_radius = rad; };
	size_t getRadius() { return m_radius; };

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T operator()(double x=0, double y=0, double z=0, int64_t t=0)
	{
		return get(x,y,z,t);
	};
	
    /**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T operator()(int64_t x=0, int64_t y=0, int64_t z=0, int64_t t=0)
	{
		return get((double)x,(double)y,(double)z,t);
	};

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T get(int64_t x=0, int64_t y=0, int64_t z=0, int64_t t=0)
	{
		return get((double)x,(double)y,(double)z,t);
	};


	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T get(double x=0, double y=0, double z=0, int64_t t=0)
	{
		// figure out size of dimensions in parent
		size_t dim[4];
		dim[0] = this->parent->dim(0);
		dim[1] = this->parent->ndim() > 1 ? this->parent->dim(1) : 1;
		dim[2] = this->parent->ndim() > 2 ? this->parent->dim(2) : 1;
		dim[3] = this->parent->tlen();

		// deal with t being outside bounds
		if(t < 0 || t >= dim[3]) {
			if(m_boundmethod == ZEROFLUX) {
				// clamp
				t = clamp<int64_t>(0, dim[3]-1, t);
			} else if(m_boundmethod == WRAP) {
				// wrap
				t = wrap<int64_t>(0, dim[3]-1, t);
			} else {
				return 0;
			}
		}

		// initialize variables

        double cindex[3] = {x,y,z};
        // convert RAS to cindex
        if(m_ras) {
            auto tmp = dPtrCast<const MRImage>(this->parent);
            tmp->pointToIndex(3, cindex, cindex);
        }

		int64_t index[3];
		const int KPOINTS = 1+m_radius*2;
		const int DIM = 3;

		// 1D version of the weights and indices
		double karray[DIM][KPOINTS];
		int64_t indarray[DIM][KPOINTS];

		for(int dd = 0; dd < DIM; dd++) {
			for(int64_t ii=-m_radius; ii<=m_radius; ii++){
				int64_t i = round(cindex[dd])+ii;
				indarray[dd][ii+m_radius] = i;
				karray[dd][ii+m_radius] = lanczosKern(i-cindex[dd], m_radius);
			}
		}

		bool iioutside = false;

		// compute weighted pixval by iterating over neighbors, which are
		// combinations of KPOINTS
		T pixval = 0;
		double weight = 0;
		div_t result;
		for(int ii = 0 ; ii < pow(KPOINTS, DIM); ii++) {
			weight = 1;

			//set index
			result.quot = ii;
			iioutside = false;
			for(int dd = 0; dd < DIM; dd++) {
				result = std::div(result.quot, KPOINTS);
				weight *= karray[dd][result.rem];
				index[dd] = indarray[dd][result.rem];
				iioutside = iioutside || index[dd] < 0 || index[dd] >= dim[dd];
			}

			// if the current point maps outside, then we need to deal with it
			if(iioutside) {
				if(m_boundmethod == ZEROFLUX) {
					// clamp
					for(size_t dd=0; dd<DIM; dd++)
						index[dd] = clamp<int64_t>(0, dim[dd]-1, index[dd]);
				} else if(m_boundmethod == WRAP) {
					// wrap
					for(size_t dd=0; dd<DIM; dd++)
						index[dd] = wrap<int64_t>(0, dim[dd]-1, index[dd]);
				} else {
					// set wieght to zero, then just clamp
					weight = 0;
					for(size_t dd=0; dd<DIM; dd++)
						index[dd] = clamp<int64_t>(0, dim[dd]-1, index[dd]);
				}
			}

			T v = this->castget(this->parent->__getAddr(index[0], index[1],index[2],t));
			pixval += weight*v;
		}

		return pixval;
	}

	BoundaryConditionT m_boundmethod;

    /**
     * @brief if true, then this assumes the inputs are RAS coordinates rather
     * than indexes. Default is false
     */
    bool m_ras;

protected:

	/**
	 * @brief Gets value at array index and then casts to T
	 *
	 * @return value
	 */
	T operator[](int64_t i) { (void)(i); return T(); };
	T get(const std::vector<int64_t>& i) { (void)(i); return T(); };
	T operator[](const std::vector<int64_t>& i) { (void)(i); return T(); };
	int64_t m_radius;
};

/**
 * @}
 */

} // namespace npl

#endif //ACCESSORS_H
