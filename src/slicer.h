#ifndef SLICER_H
#define SLICER_H

#include <vector>
#include <list>
#include <cstdint>

using namespace std;

/**
 * @brief This class is used to slice an image in along a dimension, and to 
 * step an arbitrary direction in an image. Order may be any size from 0 to the
 * number of dimensions. The first member of order will be the fastest moving,
 * and the last will be the slowest. Any not dimensions not included in the order
 * vector will be slower than the last member of order
 */
class Slicer 
{
public:
	
	/****************************************
	 *
	 * Constructors
	 *
	 ****************************************/

	/**
	 * @brief Default Constructor, max a length 1, dimension 1 slicer
	 */
	Slicer();
	

	/**
	 * @brief Full Featured Constructor
	 *
	 * @param dim	size of ND array
	 * @param order	order of iteration during ++, this doesn't affect step()
	 * @param roi	min/max, roi is pair<size_t,size_t> = [min,max] 
	 */
	Slicer(const std::vector<size_t>& dim, const std::list<size_t>& order,
			const std::vector<std::pair<size_t,size_t>>& roi);

	/**
	 * @brief Simple (no ROI, no order) Constructor
	 *
	 * @param dim	size of ND array
	 */
	Slicer(const std::vector<size_t>& dim);
	
	/**
	 * @brief Constructor that takes a dimension and order of ++/-- iteration
	 *
	 * @param dim	size of ND array
	 * @param order	iteration direction, steps will be fastest in the direction
	 * 				of order[0] and slowest in order.back()
	 */
	Slicer(const std::vector<size_t>& dim, const std::list<size_t>& order);

	/**
	 * @brief Constructor that takes a dimension and region of interest, which
	 * is defined as min,max (inclusive)
	 *
	 * @param dim	size of ND array
	 * @param roi	min/max, roi is pair<size_t,size_t> = [min,max] 
	 */
	Slicer(const std::vector<size_t>& dim, const std::vector<std::pair<size_t,size_t>>& roi);

	/**
	 * @brief Directional step, this will not step outside the region of 
	 * interest. Useful for kernels (maybe)
	 *
	 * @param dd	dimension to step in
	 * @param dist	distance to step (may be negative)
	 *
	 * @return new linear index
	 */
	size_t step(size_t dim, int64_t dist = 1);
	
	/******************************************
	 *
	 * Offset, useful to kernel processing
	 *
	 ******************************************/

	/**
	 * @brief Get linear index at an offset location from the current, useful
	 * for kernels.
	 *
	 * @param dindex	Vector (offset) from the current location 
	 *
	 * @return 		linear index
	 */
	size_t offset(int64_t* dindex) const;
	
	/**
	 * @brief Get linear index at an offset location from the current, useful
	 * for kernels.
	 *
	 * @param dindex	Vector (offset) from the current location 
	 *
	 * @return 		linear index
	 */
	size_t offset(const vector<int64_t>& dindex) const;
	
	/**
	 * @brief Get linear index at an offset location from the current, useful
	 * for kernels.
	 *
	 * @param dindex	Vector (offset) from the current location 
	 *
	 * @return 		linear index
	 */
	size_t offset(std::initializer_list<size_t> dindex) const;

	/****************************************
	 *
	 * Query Location
	 *
	 ****************************************/

	/**
	 * @brief Are we at the end in a particular dimension
	 *
	 * @param dim	dimension to check
	 *
	 * @return whether we are at the tail end of the particular dimension
	 */
	bool isEnd(size_t dim) const { return m_pos[dim] == m_roi[dim].second; };
	
	/**
	 * @brief Are we at the begin in a particular dimension
	 *
	 * @param dim	dimension to check
	 *
	 * @return whether we are at the start of the particular dimension
	 */
	bool isBegin(size_t dim) const { return m_pos[dim] == m_roi[dim].first; };
	
	/**
	 * @brief Are we at the begining of iteration?
	 *
	 * @return true if we are at the begining
	 */
	bool isBegin() const { return m_linpos==m_linfirst; };

	/**
	 * @brief Are we at the end of iteration? Note that this will be 1 past the
	 * end, as typically is done in c++
	 *
	 * @return true if we are at the end
	 */
	bool isEnd() const { return m_end; };


	/*************************************
	 * Movement
	 ***********************************/

	/**
	 * @brief Postfix iterator. Iterates in the order dictatored by the dimension
	 * order passsed during construction or by setOrder
	 *
	 * @param int	unused
	 *
	 * @return 	old value of linear position
	 */
	size_t operator++(int);
	

	/**
	 * @brief Prefix iterator. Iterates in the order dictatored by the dimension
	 * order passsed during construction or by setOrder
	 *
	 * @return 	new value of linear position
	 */
	size_t operator++();
	
	/**
	 * @brief Postfix negative  iterator. Iterates in the order dictatored by
	 * the dimension order passsed during construction or by setOrder
	 *
	 * @return 	old value of linear position
	 */
	size_t operator--(int);
	
	/**
	 * @brief Prefix negative  iterator. Iterates in the order dictatored by
	 * the dimension order passsed during construction or by setOrder
	 *
	 * @return 	new value of linear position
	 */
	size_t operator--();

	/**
	 * @brief Are we at the begining of iteration?
	 *
	 */
	void gotoBegin();

	/**
	 * @brief Jump to the end of iteration.
	 *
	 */
	void gotoEnd();

	/**
	 * @brief Jump to the given position
	 *
	 * @param newpos	location to move to
	 */
	void gotoIndex(const std::vector<size_t>& newpos);

	/**
	 * @brief Jump to the given position
	 *
	 * @param newpos	location to move to
	 */
	void gotoIndex(size_t* newpos);
	
	/**
	 * @brief Jump to the given position
	 *
	 * @param newpos	location to move to
	 */
	void gotoIndex(std::initializer_list<size_t> newpos);

	/****************************************
	 *
	 * Actually get the linear location
	 *
	 ***************************************/

	/**
	 * @brief dereference operator. Returns the linear position in the array 
	 * given the n-dimensional position.
	 *
	 * @return 
	 */
	inline
	size_t operator*() const { return m_linpos; };
	
	/**
	 * @brief Same as dereference operator. Returns the linear position in the 
	 * array given the n-dimensional position.
	 *
	 * @return 
	 */
	inline
	size_t get() const { return m_linpos; };

	/**
	 * @brief Get both ND position and linear position
	 *
	 * @param ndpos	Output, ND position
	 *
	 * @return linear position
	 */
	size_t get(std::vector<size_t>& ndpos) const
	{
		ndpos.assign(m_pos.begin(), m_pos.end());
		return m_linpos;
	};

	/***********************************************
	 *
	 * Modification
	 *
	 **********************************************/

	/**
	 * @brief Updates dimensions of target nd array
	 *
	 * @param dim	size of nd array, number of dimesions given by dim.size()
	 */
	void updateDim(const std::vector<size_t>& dim);

	/**
	 * @brief Sets the region of interest. During iteration or any motion the
	 * position will not move outside the specified range
	 *
	 * @param roi	pair of [min,max] values in the desired hypercube
	 */
	void setROI(const std::vector<std::pair<size_t, size_t>>& roi);

	/**
	 * @brief Sets the order of iteration from ++/-- operators
	 *
	 * @param order	vector of priorities, with first element being the fastest
	 * iteration and last the slowest. All other dimensions not used will be 
	 * slower than the last
	 */
	void setOrder(const std::list<size_t>& order);



private:

	size_t m_linpos;
	size_t m_linfirst;
	size_t m_linlast;
	bool m_end;
	std::vector<size_t> m_order;
	std::vector<size_t> m_pos;
	std::vector<std::pair<size_t,size_t>> m_roi;

	// these might benefit from being constant
	std::vector<size_t> m_sizes;
	std::vector<size_t> m_strides;
	size_t m_total;

	void updateLinRange();
};

#endif //SLICER_H
