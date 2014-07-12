/*******************************************************************************
This file is part of Neural Program Library (NPL), 

Written and Copyrighted by by Micah C. Chambers (micahc.vt@gmail.com)

The Neural Program Library is free software: you can redistribute it and/or
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

#include "ndarray.h"
#include <iostream>
#include <set>
#include <map>

using namespace std;
using namespace npl;

inline
int64_t clamp(int64_t ii, int64_t low, int64_t high)
{
	return std::min<int64_t>(high, std::max<int64_t>(ii,low));
}


int main(int argc, char** argv)
{
	if(argc != 2) {
		cerr << "Need one argument (the size of clusters)" << endl;
		return -1;
	}

	//size_t csize = atoi(argv[1]);
	// need to produce a cache based on this

	////////////////////////////////////////////////////////////////////////////////////
	cerr << "4D Test" << endl;
	NDArrayStore<4, float> array1({500, 500, 1, 1});
	NDArrayStore<4, float> array2({500, 500, 1, 1});

	cerr << "Filling..." << endl;
	auto t = clock();
	for(size_t ii = 0; ii < array1._m_dim[0]; ii++) {
		for(size_t jj = 0; jj < array1._m_dim[1]; jj++) {
			for(size_t kk = 0; kk < array1._m_dim[2]; kk++) {
				for(size_t tt = 0; tt < array1._m_dim[3]; tt++) {
					double val = rand()/(double)RAND_MAX;
					array1.dbl({ii, jj, kk, tt}, val);
				}
			}
		}
	}
	t = clock() - t;
	printf("Fill: %li clicks (%f seconds)\n",t,((float)t)/CLOCKS_PER_SEC);
	
	int64_t radius = 4;
	t = clock();
	for(size_t tt = 0; tt < array1._m_dim[3]; tt++) {
		for(size_t kk = 0; kk < array1._m_dim[2]; kk++) {
			for(size_t jj = 0; jj < array1._m_dim[1]; jj++) {
				for(size_t ii = 0; ii < array1._m_dim[0]; ii++) {
					double sum = 0;
					double n = 0;
					for(int64_t xx=-radius; xx<=radius ; xx++) {
						for(int64_t yy=-radius; yy<=radius ; yy++) {
							for(int64_t zz=-radius; zz<=radius ; zz++) {
								for(int64_t rr=-radius; rr<=radius ; rr++) {
									size_t ix = clamp(ii+xx, 0, array1._m_dim[0]-1);
									size_t jy = clamp(jj+yy, 0, array1._m_dim[1]-1);
									size_t kz = clamp(kk+zz, 0, array1._m_dim[2]-1);
									size_t tr = clamp(tt+rr, 0, array1._m_dim[2]-1);
									sum += array1.dbl({ix, jy, kz, tr});
									n++;
								}
							}
						}
					}
					array2.dbl({ii, jj, kk, tt}, sum/n);
				}
			}
		}
	}
	t = clock() - t;
	printf("Radius %li Kernel: %li clicks (%f seconds)\n",radius, t,((float)t)/CLOCKS_PER_SEC);
}



