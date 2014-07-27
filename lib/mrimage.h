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

#ifndef NDIMAGE_H
#define NDIMAGE_H

#include "ndarray.h"
#include "npltypes.h"
#include "matrix.h"

#include "zlib.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <memory>
#include <map>

namespace npl {

using std::vector;
using std::shared_ptr;

enum SliceOrderT {UNKNOWN_SLICE=0, SEQ=1, RSEQ=2, ALT=3, RALT=4, ALT_SHFT=5,
	RALT_SHFT=6};

enum BoundaryConditionT {ZEROFLUX=0, CONSTZERO=1, WRAP=2};

class MRImage;

/****************************************************************************** 
 * Basic Functions. 
 ******************************************************************************/

/**
 * @brief Creates a new MRImage with dimensions set by ndim, and size set by
 * size. Output pixel type is decided by ptype variable.
 *
 * @param ndim number of image dimensions
 * @param size size of image, in each dimension
 * @param ptype Pixel type npl::PixelT
 *
 * @return New image, default orientation
 */
shared_ptr<MRImage> createMRImage(size_t ndim, const size_t* size, PixelT ptype);

/**
 * @brief Creates a new MRImage with dimensions set by ndim, and size set by
 * size. Output pixel type is decided by ptype variable.
 *
 * @param size size of image, in each dimension, number of dimensions decied by
 * length of size vector
 * @param ptype Pixel type npl::PixelT
 *
 * @return New image, default orientation
 */
shared_ptr<MRImage> createMRImage(const std::vector<size_t>& dim, PixelT ptype);

/****************************************************************************** 
 * Classes. 
 ******************************************************************************/

/**
 * @brief MRImage can basically be used like an NDArray, with the addition
 * of orientation related additions.
 */
class MRImage : public virtual NDArray
{
public:
	MRImage() : m_freqdim(-1), m_phasedim(-1), m_slicedim(-1), 
				m_slice_duration(0), m_slice_start(-1), 
				m_slice_end(-1), m_slice_order(UNKNOWN_SLICE) {} ;
	
	shared_ptr<MRImage> getPtr()  {
		return std::dynamic_pointer_cast<MRImage>(shared_from_this());
	};
	
	shared_ptr<const MRImage> getConstPtr() const {
		return std::dynamic_pointer_cast<const MRImage>(shared_from_this());
	};


	virtual MatrixP& spacing() = 0;
	virtual MatrixP& origin() = 0;
	virtual MatrixP& direction() = 0;
	virtual const MatrixP& spacing() const = 0;
	virtual const MatrixP& origin() const  = 0;
	virtual const MatrixP& direction() const = 0;
	virtual const MatrixP& affine() const = 0;
	virtual const MatrixP& iaffine() const = 0;

	virtual void setOrient(const MatrixP& orig, const MatrixP& space, 
			const MatrixP& dir, bool reinit) = 0;
	virtual void setOrigin(const MatrixP& orig, bool reinit = false) = 0;
	virtual void setSpacing(const MatrixP& space, bool reinit = false) = 0;
	virtual void setDirection(const MatrixP& dir, bool reinit = false) = 0;
	
	virtual int write(std::string filename, double version = 1) const = 0;
	
	virtual std::shared_ptr<MRImage> cloneImage() const = 0;
	

	/**
	 * @brief Performs a deep copy of the entire image and all metadata.
	 *
	 * @return Copied image.
	 */
	virtual shared_ptr<NDArray> copy() const = 0;

	/**
	 * @brief Create a new image that is a copy of the input, possibly with new
	 * dimensions and pixeltype. The new image will have all overlapping pixels
	 * copied from the old image.
	 *
	 * This function just calls the outside copyCast, the reason for this 
	 * craziness is that making a template function nested in the already 
	 * huge number of templates I have kills the compiler, so we call an 
	 * outside function that calls templates that has all combinations of D,T.
	 *
	 * @param in Input image, anything that can be copied will be
	 * @param newdims Number of dimensions in output image
	 * @param newsize Size of output image
	 * @param newtype Type of pixels in output image
	 *
	 * @return Image with overlapping sections cast and copied from 'in' 
	 */
	virtual shared_ptr<NDArray> copyCast(size_t newdims, const size_t* newsize, 
			PixelT newtype) const = 0;

	/**
	 * @brief Create a new image that is a copy of the input, with same dimensions
	 * but pxiels cast to newtype. The new image will have all overlapping pixels
	 * copied from the old image.
	 *
	 * This function just calls the outside copyCast, the reason for this 
	 * craziness is that making a template function nested in the already 
	 * huge number of templates I have kills the compiler, so we call an 
	 * outside function that calls templates that has all combinations of D,T.
	 *
	 * @param in Input image, anything that can be copied will be
	 * @param newtype Type of pixels in output image
	 *
	 * @return Image with overlapping sections cast and copied from 'in' 
	 */
	virtual shared_ptr<NDArray> copyCast(PixelT newtype) const = 0;

	/**
	 * @brief Create a new image that is a copy of the input, possibly with new
	 * dimensions or size. The new image will have all overlapping pixels
	 * copied from the old image. The new image will have the same pixel type as
	 * the input image
	 *
	 * This function just calls the outside copyCast, the reason for this 
	 * craziness is that making a template function nested in the already 
	 * huge number of templates I have kills the compiler, so we call an 
	 * outside function that calls templates that has all combinations of D,T.
	 *
	 * @param in Input image, anything that can be copied will be
	 * @param newdims Number of dimensions in output image
	 * @param newsize Size of output image
	 *
	 * @return Image with overlapping sections cast and copied from 'in' 
	 */
	virtual shared_ptr<NDArray> copyCast(size_t newdims, 
				const size_t* newsize) const = 0;

	//////////////////////////////////
	// coordinate system conversion 
	//////////////////////////////////
	
	/**
	 * @brief Converts an index in pixel space to RAS, physical/time coordinates.
	 * If len < dimensions, additional dimensions are assumed to be 0. If len >
	 * dimensions then additional values are ignored, and only the first DIM 
	 * values will be transformed and written to ras.
	 *
	 * @param len Length of xyz/ras arrays.
	 * @param xyz Array in xyz... coordinates (maybe as long as you want).
	 * @param ras Corresponding coordinate
	 *
	 * @return 0
	 */
	virtual int indexToPoint(size_t len, const int64_t* xyz, double* ras) const=0;
	
	/**
	 * @brief Converts an index in pixel space to RAS, physical/time coordinates.
	 * If len < dimensions, additional dimensions are assumed to be 0. If len >
	 * dimensions then additional values are ignored, and only the first DIM 
	 * values will be transformed and written to ras.
	 *
	 * @param len Length of xyz/ras arrays.
	 * @param xyz Array in xyz... coordinates (maybe as long as you want).
	 * @param ras Corresponding coordinate
	 *
	 * @return 0
	 */
	virtual int indexToPoint(size_t len, const double* xyz, double* ras) const=0;
	
	/**
	 * @brief Converts a point in RAS coordinate system to index.
	 * If len < dimensions, additional dimensions are assumed to be 0. If len >
	 * dimensions then additional values are ignored, and only the first DIM 
	 * values will be transformed and written to ras.
	 *
	 * @param len Length of xyz/ras arrays.
	 * @param ras Array in RAS... coordinates (may be as long as you want).
	 * @param xyz Corresponding coordinate
	 *
	 * @return 0
	 */
	virtual int pointToIndex(size_t len, const double* ras, double* xyz) const=0;
	
	/**
	 * @brief Converts a point in RAS coordinate system to index.
	 * If len < dimensions, additional dimensions are assumed to be 0. If len >
	 * dimensions then additional values are ignored, and only the first DIM 
	 * values will be transformed and written to ras.
	 *
	 * @param len Length of xyz/ras arrays.
	 * @param ras Array in RAS... coordinates (may be as long as you want).
	 * @param xyz Corresponding coordinate, rounded to nearest integer
	 *
	 * @return 0
	 */
	virtual int pointToIndex(size_t len, const double* ras, int64_t* index) const=0;
	
	/**
	 * @brief Returns true if the point is within the field of view of the 
	 * image. Note, like all coordinates pass to MRImage, if the array given
	 * differs from the dimensions of the image, then the result will either
	 * pad out zeros and ignore extra values in the input array.
	 *
	 * @param len Length of RAS array
	 * @param ras Array of Right-handed coordinates Right+, Anterior+, Superior+
	 *
	 * @return Whether the point would round to a voxel inside the image.
	 */
	virtual bool pointInsideFOV(size_t len, const double* ras) const=0;
	
	/**
	 * @brief Returns true if the constinuous index is within the field of 
	 * view of the image. Note, like all coordinates pass to MRImage, if the
	 * array given differs from the dimensions of the image, then the result
	 * will either pad out zeros and ignore extra values in the input array.

	 *
	 * @param len Length of xyz array
	 * @param xyz Array of continouos indices 
	 *
	 * @return Whether the index would round to a voxel inside the image.
	 */
	virtual bool indexInsideFOV(size_t len, const double* xyz) const=0;
	
	/**
	 * @brief Returns true if the constinuous index is within the field of 
	 * view of the image. Note, like all coordinates pass to MRImage, if the
	 * array given differs from the dimensions of the image, then the result
	 * will either pad out zeros and ignore extra values in the input array.
	 *
	 * @param len Length of xyz array
	 * @param xyz Array of indices 
	 *
	 * @return Whether the index is inside the image
	 */
	virtual bool indexInsideFOV(size_t len, const int64_t* xyz) const=0;

//	virtual int unary(double(*func)(double,double)) const = 0;
//	virtual int binOp(const MRImage* right, double(*func)(double,double), bool elevR) const = 0;

	/*
	 * medical image specific stuff, eventually these should be moved to a 
	 * medical image subclass
	 */
	
	// < 0 indicate unset variables
	int m_freqdim;
	int m_phasedim;
	int m_slicedim;

	/*
	 * nifti specific stuff, eventually these should be moved to a nifti 
	 * image subclass
	 */

	void updateSliceTiming(double duration, int start, int end, SliceOrderT order);

	// raw values for slice data, < 0 indicate unset
	double m_slice_duration;
	int m_slice_start;
	int m_slice_end;

	// SEQ, RSEQ, ALT, RALT, ALT_SHFT, RALT_SHFT
	// SEQ (sequential): 	slice_start .. slice_end
	// RSEQ (reverse seq): 	slice_end .. slice_start
	// ALT (alternated): 	slice_start, slice_start+2, .. slice_end|slice_end-1,
	// 						slice_start+1 .. slice_end|slice_end-1
	// RALT (reverse alt): 	slice_end, slice_end-2, .. slice_start|slice_start+1,
	// 						slice_end-1 .. slice_start|slice_start+1
	// ALT_SHFT (siemens alt):slice_start+1, slice_start+3, .. slice_end|slice_end-1,
	// 						slice_start .. slice_end|slice_end-1
	// RALT (reverse alt): 	slice_end-1, slice_end-3, .. slice_start|slice_start+1,
	// 						slice_end-2 .. slice_start|slice_start+1
	SliceOrderT m_slice_order;

	// each slice is given its relative time, with 0 as the first
	std::map<int64_t,double> m_slice_timing;

	friend std::ostream& operator<<(std::ostream &out, const MRImage& img);

protected:
	virtual int writeNifti1Image(gzFile file) const = 0;
	virtual int writeNifti2Image(gzFile file) const = 0;
	
	virtual void updateAffine() = 0;

	
	friend shared_ptr<MRImage> readNiftiImage(gzFile file, bool verbose);
};

/**
 * @brief MRImageStore is a version of NDArray that has an orientation matrix.
 * Right now it also has additional data that is unique to nifti. Eventually
 * this class will be forked into a subclass, and this will only have the 
 * orientation.
 *
 * @tparam D 	Number of dimensions
 * @tparam T	Pixel type
 */
template <size_t D, typename T>
class MRImageStore :  public virtual NDArrayStore<D,T>, public virtual MRImage
{

public:

	/**
	 * @brief Constructor with initializer list. Orientation will be default
	 * (direction = identity, spacing = 1, origin = 0). 
	 *
	 * @param a_args dimensions of input, the length of this initializer list
	 * may not be fully used if a_args is longer than D. If it is shorter
	 * then D then additional dimensions are left as size 1.
	 */
	MRImageStore(std::initializer_list<size_t> a_args);

	/**
	 * @brief Constructor with vector. Orientation will be default
	 * (direction = identity, spacing = 1, origin = 0). 
	 *
	 * @param a_args dimensions of input, the length of this initializer list
	 * may not be fully used if a_args is longer than D. If it is shorter
	 * then D then additional dimensions are left as size 1.
	 */
	MRImageStore(const std::vector<size_t>& a_args);
	
	/**
	 * @brief Constructor with array of length len, Orientation will be default
	 * (direction = identity, spacing = 1, origin = 0). 
	 *
	 * @param len Length of array 'size'
	 * @param size dimensions of input, the length of this initializer list
	 * may not be fully used if a_args is longer than D. If it is shorter
	 * then D then additional dimensions are left as size 1.
	 */
	MRImageStore(size_t len, const size_t* size);
	
	/**
	 * @brief Constructor which uses a preexsting array, to graft into the
	 * image. No new allocation will be performed, however ownership of the
	 * array will be taken, meaning it could be deleted anytime after this 
	 * constructor completes.
	 *
	 * @param len Length of array 'size'
	 * @param size dimensions of input, the length of this initializer list
	 * may not be fully used if a_args is longer than D. If it is shorter
	 * then D then additional dimensions are left as size 1.
	 * @param ptr Pointer to data array, should be allocated with new, and 
	 * size should be exactly sizeof(T)*size[0]*size[1]*...*size[len-1]
	 */
	MRImageStore(size_t len, const size_t* size, T* ptr);
	
	/**
	 * @brief Performs a deep copy of the entire image and all metadata.
	 *
	 * @return Copied image.
	 */
	virtual shared_ptr<NDArray> copy() const;

	/**
	 * @brief Create a new image that is a copy of the input, possibly with new
	 * dimensions and pixeltype. The new image will have all overlapping pixels
	 * copied from the old image.
	 *
	 * This function just calls the outside copyCast, the reason for this 
	 * craziness is that making a template function nested in the already 
	 * huge number of templates I have kills the compiler, so we call an 
	 * outside function that calls templates that has all combinations of D,T.
	 *
	 * @param in Input image, anything that can be copied will be
	 * @param newdims Number of dimensions in output image
	 * @param newsize Size of output image
	 * @param newtype Type of pixels in output image
	 *
	 * @return Image with overlapping sections cast and copied from 'in' 
	 */
	virtual shared_ptr<NDArray> copyCast(size_t newdims, const size_t* newsize, 
			PixelT newtype) const;

	/**
	 * @brief Create a new image that is a copy of the input, with same dimensions
	 * but pxiels cast to newtype. The new image will have all overlapping pixels
	 * copied from the old image.
	 *
	 * This function just calls the outside copyCast, the reason for this 
	 * craziness is that making a template function nested in the already 
	 * huge number of templates I have kills the compiler, so we call an 
	 * outside function that calls templates that has all combinations of D,T.
	 *
	 * @param in Input image, anything that can be copied will be
	 * @param newtype Type of pixels in output image
	 *
	 * @return Image with overlapping sections cast and copied from 'in' 
	 */
	virtual shared_ptr<NDArray> copyCast(PixelT newtype) const;

	/**
	 * @brief Create a new image that is a copy of the input, possibly with new
	 * dimensions or size. The new image will have all overlapping pixels
	 * copied from the old image. The new image will have the same pixel type as
	 * the input image
	 *
	 * This function just calls the outside copyCast, the reason for this 
	 * craziness is that making a template function nested in the already 
	 * huge number of templates I have kills the compiler, so we call an 
	 * outside function that calls templates that has all combinations of D,T.
	 *
	 * @param in Input image, anything that can be copied will be
	 * @param newdims Number of dimensions in output image
	 * @param newsize Size of output image
	 *
	 * @return Image with overlapping sections cast and copied from 'in' 
	 */
	virtual shared_ptr<NDArray> copyCast(size_t newdims, const size_t* newsize) const;

	/*************************************************************************
	 * Coordinate Transform Functions
	 ************************************************************************/
	void orientDefault();
	void updateAffine();
	void setOrient(const MatrixP& orig, const MatrixP& space, 
			const MatrixP& dir, bool reinit);
	void setOrigin(const MatrixP& orig, bool reinit = false);
	void setSpacing(const MatrixP& space, bool reinit = false);
	void setDirection(const MatrixP& dir, bool reinit = false);
	
	void printSelf();
	
	/**
	 * @brief Converts an index in pixel space to RAS, physical/time coordinates.
	 * If len < dimensions, additional dimensions are assumed to be 0. If len >
	 * dimensions then additional values are ignored, and only the first DIM 
	 * values will be transformed and written to ras.
	 *
	 * @param len Length of xyz/ras arrays.
	 * @param xyz Array in xyz... coordinates (maybe as long as you want).
	 * @param ras Corresponding coordinate
	 *
	 * @return 0
	 */
	virtual int indexToPoint(size_t len, const int64_t* xyz, double* ras) const;
	
	/**
	 * @brief Converts an index in pixel space to RAS, physical/time coordinates.
	 * If len < dimensions, additional dimensions are assumed to be 0. If len >
	 * dimensions then additional values are ignored, and only the first DIM 
	 * values will be transformed and written to ras.
	 *
	 * @param len Length of xyz/ras arrays.
	 * @param xyz Array in xyz... coordinates (maybe as long as you want).
	 * @param ras Corresponding coordinate
	 *
	 * @return 0
	 */
	virtual int indexToPoint(size_t len, const double* xyz, double* ras) const;
	
	/**
	 * @brief Converts a point in RAS coordinate system to index.
	 * If len < dimensions, additional dimensions are assumed to be 0. If len >
	 * dimensions then additional values are ignored, and only the first DIM 
	 * values will be transformed and written to ras.
	 *
	 * @param len Length of xyz/ras arrays.
	 * @param ras Array in RAS... coordinates (may be as long as you want).
	 * @param xyz Corresponding coordinate
	 *
	 * @return 0
	 */
	virtual int pointToIndex(size_t len, const double* ras, double* xyz) const;
	
	/**
	 * @brief Converts a point in RAS coordinate system to index.
	 * If len < dimensions, additional dimensions are assumed to be 0. If len >
	 * dimensions then additional values are ignored, and only the first DIM 
	 * values will be transformed and written to ras.
	 *
	 * @param len Length of xyz/ras arrays.
	 * @param ras Array in RAS... coordinates (may be as long as you want).
	 * @param xyz Corresponding coordinate, rounded to nearest integer
	 *
	 * @return 0
	 */
	virtual int pointToIndex(size_t len, const double* ras, int64_t* index) const;
	
	/**
	 * @brief Returns true if the point is within the field of view of the 
	 * image. Note, like all coordinates pass to MRImage, if the array given
	 * differs from the dimensions of the image, then the result will either
	 * pad out zeros and ignore extra values in the input array.
	 *
	 * @param len Length of RAS array
	 * @param ras Array of Right-handed coordinates Right+, Anterior+, Superior+
	 *
	 * @return Whether the point would round to a voxel inside the image.
	 */
	virtual bool pointInsideFOV(size_t len, const double* ras) const;
	
	/**
	 * @brief Returns true if the constinuous index is within the field of 
	 * view of the image. Note, like all coordinates pass to MRImage, if the
	 * array given differs from the dimensions of the image, then the result
	 * will either pad out zeros and ignore extra values in the input array.

	 *
	 * @param len Length of xyz array
	 * @param xyz Array of continouos indices 
	 *
	 * @return Whether the index would round to a voxel inside the image.
	 */
	virtual bool indexInsideFOV(size_t len, const double* xyz) const;
	
	/**
	 * @brief Returns true if the constinuous index is within the field of 
	 * view of the image. Note, like all coordinates pass to MRImage, if the
	 * array given differs from the dimensions of the image, then the result
	 * will either pad out zeros and ignore extra values in the input array.
	 *
	 * @param len Length of xyz array
	 * @param xyz Array of indices 
	 *
	 * @return Whether the index is inside the image
	 */
	virtual bool indexInsideFOV(size_t len, const int64_t* xyz) const;

	/**
	 * @brief Returns a matrix (single column) with spacing information. The 
	 * number of rows is equal to the number of dimensions in the image.
	 *
	 * @return Reference to spacing matrix
	 */
	MatrixP& spacing() {return *((MatrixP*)&m_space); };
	
	/**
	 * @brief Returns a matrix (single column) with origin (RAS coordinate of 
	 * index 0,0,0). The number of rows is equal to the number of dimensions in
	 * the image.
	 *
	 * @return Reference to origin matrix
	 */
	MatrixP& origin() {return *((MatrixP*)&m_origin); };
	
	/**
	 * @brief Returns a square matrix with direction, which is the rotation off
	 * the indices to +R +A +S. 
	 *
	 * @return Reference to direction matrix
	 */
	MatrixP& direction() {return *((MatrixP*)&m_dir); };


//	MatrixP& affine() {return *((MatrixP*)&m_affine); };
//	MatrixP& iaffine() {return *((MatrixP*)&m_inv_affine); };
	
	/**
	 * @brief Returns a matrix (single column) with spacing information. The 
	 * number of rows is equal to the number of dimensions in the image.
	 *
	 * @return Reference to spacing matrix
	 */
	const MatrixP& spacing() const {return *((MatrixP*)&m_space); };
	
	/**
	 * @brief Returns a matrix (single column) with origin (RAS coordinate of 
	 * index 0,0,0). The number of rows is equal to the number of dimensions in
	 * the image.
	 *
	 * @return Reference to origin matrix
	 */
	const MatrixP& origin() const {return *((MatrixP*)&m_origin); };
	
	/**
	 * @brief Returns a square matrix with direction, which is the rotation off
	 * the indices to +R +A +S. 
	 *
	 * @return Reference to direction matrix
	 */
	const MatrixP& direction() const {return *((MatrixP*)&m_dir); };
	
	/**
	 * @brief Returns a square matrix that may be used to convert an index to 
	 * a point in the coordinate system of real RAS space (rather than index
	 * space).
	 *
	 * @return Reference to affine matrix
	 */
	const MatrixP& affine() const {return *((MatrixP*)&m_affine); };

	/**
	 * @brief Returns a square matrix that may be used to convert a point in
	 * the coordinate system of real RAS space to index space.
	 *
	 * @return Reference to inverse affine matrix
	 */
	const MatrixP& iaffine() const {return *((MatrixP*)&m_inv_affine); };

	/**
	 * @brief Returns units of given dimension, note that this is prior to the
	 * direction matrix, so if there is oblique orientation you are really
	 * looking at a mix of units.
	 *
	 * @param d Dimension
	 *
	 * @return String describing units.
	 */
	std::string getUnits(size_t d) { return m_units[d]; };

	/**
	 * @brief Write out nifti image with the current images data.
	 *
	 * @param filename
	 * @param version > 2 or < 2 to indicate whether to use nifti version 2
	 * or nifti version 1.
	 *
	 * @return Success if 0
	 */
	int write(std::string filename, double version) const;
	
	/**
	 * @brief Create an exact copy of the current image object, and return
	 * a pointer to it.
	 *
	 * @return Pointer to exact duplicate of current image.
	 */
	std::shared_ptr<MRImage> cloneImage() const;
protected:
	// used to transform index to RAS (Right Handed Coordinate System)

	/**
	 * @brief Raw direction matrix
	 */
	Matrix<D,D> m_dir;

	/**
	 * @brief Raw spacing matrix
	 */
	Matrix<D,1> m_space;

	/**
	 * @brief Raw origin
	 */
	Matrix<D,1> m_origin;

	/**
	 * @brief String indicating units
	 */
	std::string m_units[D];
	
	/**
	 * @brief Matrix which transforms an index vector into a point vector
	 */
	Matrix<D+1,D+1> m_affine;


	/**
	 * @brief Matrix which transforms a space vector to an index vector.
	 */
	Matrix<D+1,D+1> m_inv_affine;
	
	int writeNifti1Image(gzFile file) const;
	int writeNifti2Image(gzFile file) const;
	int writeNifti1Header(gzFile file) const;
	int writeNifti2Header(gzFile file) const;
	int writePixels(gzFile file) const;
};

} // npl
#endif 
