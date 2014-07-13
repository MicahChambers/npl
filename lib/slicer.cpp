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

#include "slicer.h"

#include <vector>
#include <list>
#include <algorithm>
#include <cassert>

namespace npl {

/**
 * @brief Default Constructor, max a length 1, dimension 1 slicer
 */
Slicer::Slicer() 
{ 
	std::vector<size_t> tmp(1,1);
	updateDim(tmp);
};

/**
 * @brief Full Featured Constructor
 *
 * @param dim	size of ND array
 * @param order	order of iteration during ++, this doesn't affect step()
 * @param revorder	Reverse order, in which case the first element of order
 * 					will have the slowest iteration, and dimensions not
 * 					specified in order will be faster than those included.
 * @param roi	min/max, roi is pair<size_t,size_t> = [min,max] 
 */
Slicer::Slicer(const std::vector<size_t>& dim, const std::list<size_t>& order,
		bool revorder, const std::vector<std::pair<size_t,size_t>>& roi)
{
	updateDim(dim);
	setOrder(order, revorder);
	setROI(roi);
};

/**
 * @brief Simple (no ROI, no order) Constructor
 *
 * @param dim	size of ND array
 */
Slicer::Slicer(const std::vector<size_t>& dim) 
{
	updateDim(dim);
};

/**
 * @brief Constructor that takes a dimension and order of ++/-- iteration
 *
 * @param dim	size of ND array
 * @param order	iteration direction, steps will be fastest in the direction
 * 				of order[0] and slowest in order.back()
 * @param revorder	Reverse order, in which case the first element of order
 * 					will have the slowest iteration, and dimensions not
 * 					specified in order will be faster than those included.
 */
Slicer::Slicer(const std::vector<size_t>& dim, const std::list<size_t>& order,
		bool revorder)
{
	updateDim(dim);
	setOrder(order, revorder);
};

/**
 * @brief Constructor that takes a dimension and region of interest, which
 * is defined as min,max (inclusive)
 *
 * @param dim	size of ND array
 * @param roi	min/max, roi is pair<size_t,size_t> = [min,max] 
 */
Slicer::Slicer(const std::vector<size_t>& dim, const std::vector<std::pair<size_t,size_t>>& roi)
{
	updateDim(dim);
	setROI(roi);
};

/**
 * @brief Directional step, this will not step outside the region of 
 * interest. Useful for kernels (maybe)
 *
 * @param dd	dimension to step in
 * @param dist	distance to step (may be negative)
 *
 * @return new linear index
 */
size_t Slicer::step(size_t dim, int64_t dist, bool* outside)
{
	int64_t clamped = std::max<int64_t>(m_roi[dim].first, 
			std::min<int64_t>(m_roi[dim].second, (int64_t)m_pos[dim]+dist));
	
	if(outside) {
		if(clamped != (int64_t)m_pos[dim]+dist)
			*outside = true;
		else
			*outside = false;
	}

	m_linpos += (clamped-m_pos[dim])*m_strides[dim];
	m_pos[dim] = clamped;

	return m_linpos;
}

/**
 * @brief Get linear index at an offset location from the current, useful
 * for kernels.
 *
 * @param len 		Length of input array
 * @param dindex	Array offset from the current location 
 * @param outside 	Set to true if the point is outside the region of interest
 *
 * @return 		linear index
 */
size_t Slicer::offset(size_t len, const int64_t* off, bool* outside) const
{
	size_t ret = m_linpos;
	int64_t clamped;
	size_t ii = 0;
	
	if(outside)
		*outside = false;

	for(ii=0; ii<m_pos.size() && ii<len; ii++) {
		clamped = std::max<int64_t>(m_roi[ii].first, 
				std::min<int64_t>(m_roi[ii].second, m_pos[ii]+off[ii]));
		ret += (clamped-m_pos[ii])*m_strides[ii];
		
		// indicate that the value was outside the roi
		if(outside && clamped != (int64_t)m_pos[ii] + off[ii])
			*outside = true;
	}

	return ret;
}

/**
 * @brief Get linear index at an offset location from the current, useful
 * for kernels.
 *
 * @param dindex	Vector (offset) from the current location 
 *
 * @return 		linear index
 */
size_t Slicer::offset(std::initializer_list<int64_t> off, bool* outside) const
{
	size_t ret = m_linpos;
	int64_t clamped;
	size_t ii = 0;
			
	if(outside)
		*outside = false;

	for(auto it = off.begin() ; it != off.end() && ii<m_pos.size(); it++,ii++) {
		clamped = std::max<int64_t>(m_roi[ii].first, 
				std::min<int64_t>(m_roi[ii].second, m_pos[ii]+*it));
		ret += (clamped-m_pos[ii])*m_strides[ii];
		
		// indicate that the value was outside the roi
		if(outside && clamped != (int64_t)m_pos[ii] + *it)
			*outside = true;
	}
	return ret;
}

/**
 * @brief Postfix iterator. Iterates in the order dictatored by the dimension
 * order passsed during construction or by setOrder
 *
 * @param int	unused
 *
 * @return 	old value of linear position
 */
size_t Slicer::operator++(int)
{
	if(isEnd())
		return m_linpos;

	size_t ret = m_linpos;
	for(size_t ii=0; ii<m_order.size(); ii++){
		size_t dd = m_order[ii];
		if(m_pos[dd] < m_roi[dd].second) {
			m_pos[dd]++;
			m_linpos += m_strides[dd];
			break;
		} else if(ii != m_order.size()-1){
			// highest dimension doesn't roll over
			// reset dimension
			m_linpos -= (m_pos[dd]-m_roi[dd].first)*m_strides[dd];
			m_pos[dd] = m_roi[dd].first;
		} else {
			// we are willing to go 1 past the last
			m_pos[dd]++;
			m_linpos += m_strides[dd];
			m_end = true;
		}
	}

	return ret;
}

/**
 * @brief Prefix iterator. Iterates in the order dictatored by the dimension
 * order passsed during construction or by setOrder
 *
 * @return 	new value of linear position
 */
size_t Slicer::operator++() 
{
	if(isEnd())
		return m_linpos;

	for(size_t ii=0; ii<m_order.size(); ii++){
		size_t dd = m_order[ii];
		if(m_pos[dd] < m_roi[dd].second) {
			m_pos[dd]++;
			m_linpos += m_strides[dd];
			break;
		} else if(ii != m_order.size()-1){
			// rool over (unles we are the last dimension)
			m_linpos -= (m_pos[dd]-m_roi[dd].first)*m_strides[dd];
			m_pos[dd] = m_roi[dd].first;
		} else {
			// we are willing to go 1 past the last
			m_pos[dd]++;
			m_linpos += m_strides[dd];
			m_end = true;
		}
	}

	return m_linpos;
}

/**
 * @brief Postfix negative  iterator. Iterates in the order dictatored by
 * the dimension order passsed during construction or by setOrder
 *
 * @return 	new value of linear position
 */
size_t Slicer::operator--(int)
{
	if(isBegin())
		return m_linpos;

	m_end = false;
	size_t ret = m_linpos;
	for(size_t ii=0; ii<m_order.size(); ii++){
		size_t dd = m_order[ii];
		if(m_pos[dd] != m_roi[dd].first) {
			m_pos[dd]--;
			m_linpos -= m_strides[dd];
			break;
		} else if(ii != m_order.size()-1) {
			// jump forward in dd, (will pull back in next)
			m_linpos += m_roi[dd].second*m_strides[dd];
			m_pos[dd] = m_roi[dd].second;
		}
	}

	return ret;
};

/**
 * @brief Prefix negative  iterator. Iterates in the order dictatored by
 * the dimension order passsed during construction or by setOrder
 *
 * @return 	new value of linear position
 */
size_t Slicer::operator--() 
{
	if(isBegin())
		return m_linpos;

	m_end = false;
	for(size_t ii=0; ii<m_order.size(); ii++){
		size_t dd = m_order[ii];
		if(m_pos[dd] != m_roi[dd].first) {
			m_pos[dd]--;
			m_linpos -= m_strides[dd];
			break;
		} else if(ii != m_order.size()-1) {
			// jump forward in dd, (will pull back in next)
			m_linpos += m_roi[dd].second*m_strides[dd];
			m_pos[dd] = m_roi[dd].second;
		}
	}

	return m_linpos;
};

void Slicer::updateLinRange()
{
	m_total = 1;
	for(size_t ii=0; ii<m_sizes.size(); ii++)
		m_total *= m_sizes[ii];
	
	// all the dimensions should be at lower bound except lowest priority
	m_linfirst = 0;
	m_linlast = 0;
	for(size_t ii=0; ii<m_order.size(); ii++) {
		m_linfirst += m_strides[ii]*m_roi[ii].first;

		if(ii == m_order.size()-1) {
			m_linlast += m_strides[ii]*m_roi[ii].second;
		} else {
			m_linlast += m_strides[ii]*m_roi[ii].first;
		}
	}
};

/**
 * @brief Updates dimensions of target nd array
 *
 * @param dim	size of nd array, number of dimesions given by dim.size()
 */
void Slicer::updateDim(const std::vector<size_t>& dim)
{
	size_t ndim = dim.size();
	assert(ndim > 0);

	m_roi.resize(dim.size());
	m_sizes.assign(dim.begin(), dim.end());
	m_order.resize(ndim);
	m_pos.resize(ndim);
	m_strides.resize(ndim);

	// reset position
	std::fill(m_pos.begin(), m_pos.end(), 0);
	m_linpos = 0;

	// reset ROI
	for(size_t ii=0; ii<ndim; ii++) {
		m_roi[ii].first = 0;
		m_roi[ii].second = m_sizes[ii]-1;
	};

	// reset default order
	for(size_t ii=0 ; ii<ndim ; ii++)
		m_order[ii] = ndim-1-ii;

	// set up strides
	m_strides[ndim-1] = 1;
	for(int64_t ii=(int64_t)ndim-2; ii>=0; ii--) {
		m_strides[ii] = m_strides[ii+1]*dim[ii+1];
	}

	updateLinRange();
};

/**
 * @brief Sets the region of interest. During iteration or any motion the
 * position will not move outside the specified range. Invalidates position.
 *
 * @param roi	pair of [min,max] values in the desired hypercube
 */
void Slicer::setROI(const std::vector<std::pair<size_t, size_t>>& roi)
{
	for(size_t ii=0; ii<m_sizes.size(); ii++) {
		if(ii < roi.size()) {
			// clamp, to be <= 0...sizes[ii]-1
			m_roi[ii].first = std::min(roi[ii].first, m_sizes[ii]-1);
			m_roi[ii].second = std::min(roi[ii].second, m_sizes[ii]-1);
		} else {
			// no specification, just make it all
			m_roi[ii].first = 0;
			m_roi[ii].second= m_sizes[ii]-1;
		}
	}

	updateLinRange();
	goBegin();
}

/**
 * @brief Sets the order of iteration from ++/-- operators. Invalidates
 * position, because the location of the last pixel changes.
 *
 * @param order	vector of priorities, with first element being the fastest
 * iteration and last the slowest. All other dimensions not used will be 
 * slower than the last
 * @param revorder	Reverse order, in which case the first element of order
 * 					will have the slowest iteration, and dimensions not
 * 					specified in order will be faster than those included.
 */
void Slicer::setOrder(const std::list<size_t>& order, bool revorder)
{
	size_t ndim = m_sizes.size();
	m_order.clear();

	// need to ensure that all dimensions get covered
	std::list<size_t> avail;
	for(size_t ii=0 ; ii<ndim ; ii++) {
		avail.push_back(ii);
	}

	// add dimensions to internal order, but make sure there are 
	// no repeats
	for(auto ito=order.begin(); ito != order.end(); ito++) {

		// determine whether the given is available still
		auto it = std::find(avail.begin(), avail.end(), *ito);

		// should be available, although technically we could handle it, it
		// seems like a repeat would be a logic error
		assert(it != avail.end());
		if(it != avail.end()) {
			m_order.push_back(*it);
			avail.erase(it);
		}
	}

	// we would like the dimensions to be added so that steps are small, 
	// so in revorder case, add dimensions in decreasing order (since they will
	// be flipped), in normal case add in increasing order. 
	// so dimensions 0 3, 5 might be remaining, with order currently:
	// m_order = {1,4,2}, 
	// in the case of revorder we will add the remaining dimensions as
	// m_order = {1,4,2,0,3,5}, because once we flip it will be {5,3,0,2,4,1}
	if(revorder) {
		for(auto it=avail.begin(); it != avail.end(); ++it) 
			m_order.push_back(*it);
		// reverse 6D, {0,5},{1,4},{2,3}
		// reverse 5D, {0,4},{1,3}
		for(size_t ii=0; ii<ndim/2; ii++) 
			std::swap(m_order[ii],m_order[ndim-1-ii]);
	} else {
		for(auto it=avail.rbegin(); it != avail.rend(); ++it) 
			m_order.push_back(*it);
	}

	updateLinRange();
	goBegin();
};

/**
 * @brief Jump to the given position
 *
 * @param newpos	location to move to
 */
void Slicer::goIndex(size_t len, size_t* newpos, bool* outside)
{
	m_linpos = 0;
	size_t ii=0;
	size_t clamped = 0;

	if(outside)
		*outside = false;

	// copy the dimensions 
	for(ii = 0;  ii<len && ii<m_pos.size(); ii++) {
		// clamp to roi
		clamped = std::max(std::min(newpos[ii], m_roi[ii].second), 
				m_roi[ii].first);
		// if clamping had an effect, then set outside
		if(outside && clamped != newpos[ii])
			*outside = true;

		// set position
		m_pos[ii] = clamped;
		m_linpos += m_strides[ii]*m_pos[ii];
	}

	// set the unreferenced dimensions to 0
	for(;  ii<m_pos.size(); ii++) 
		m_pos[ii] = 0;

	m_end = false;
};

/**
 * @brief Jump to the given position
 *
 * @param newpos	location to move to
 */
void Slicer::goIndex(std::initializer_list<size_t> newpos, bool* outside)
{
	m_linpos = 0;
	size_t clamped;

	if(outside)
		*outside = false;

	// copy the dimensions 
	size_t ii=0;
	for(auto it=newpos.begin(); it != newpos.end() && ii<m_pos.size(); ii++) {
		// clamp value
		clamped = std::max(std::min(*it, m_roi[ii].second), 
				m_roi[ii].first);

		// if clamping had an effect, set outside
		if(outside && clamped != *it)
			*outside = true;

		// set position
		m_pos[ii] = clamped;
		m_linpos += m_strides[ii]*m_pos[ii];
	}

	// set the unreferenced dimensions to 0
	for(;  ii<m_pos.size(); ii++) 
		m_pos[ii] = 0;

	m_end = false;
};
	
	
/**
 * @brief Are we at the begining of iteration?
 *
 * @return true if we are at the begining
 */
void Slicer::goBegin()
{
	for(size_t ii=0; ii<m_sizes.size(); ii++)
		m_pos[ii] = m_roi[ii].first;
	m_linpos = m_linfirst;
	m_end = false;
}
	
void Slicer::goEnd()
{
	for(size_t ii=0; ii<m_sizes.size(); ii++)
		m_pos[ii] = m_roi[ii].second;
	m_linpos = m_linlast;
	m_end = true;
}

} //npl

