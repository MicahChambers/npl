/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * NPL is free software: you can redistribute it and/or modify it under the
 * terms of the BSD 2-Clause License available in LICENSE or at
 * http://opensource.org/licenses/BSD-2-Clause
 *
 * @file array_test1.cpp
 *
 *****************************************************************************/

#include "ndarray.h"
#include "accessors.h"
#include <iostream>
#include <ctime>

using namespace std;
using namespace npl;

int main()
{
	std::vector<size_t> sz({100,78,83});
	auto test1 = make_shared<NDArrayStore<3, float>>(sz);
	cerr << "Bytes: " << test1->bytes() << endl;

	for(size_t ii = 0; ii < test1->elements(); ii++)
		(*test1)[ii] = ii;

	cerr << "Dimensions:" << test1->ndim() << endl;

	NDView<double> arr1(test1);

	double total = 0;
	std::vector<int64_t> index(3);
	for(int64_t xx=0, ii=0; xx < test1->dim(0); xx++) {
		for(int64_t yy=0; yy < test1->dim(1); yy++) {
			for(int64_t zz=0; zz < test1->dim(2); zz++, ii++) {
				index[0] = xx;
				index[1] = yy;
				index[2] = zz;
				if(arr1[index] != arr1[{xx,yy,zz}]) {
					cerr << "Difference in vector/initializer" << endl;
					return -1;
				}
				if((*test1)[ii] != arr1[{xx,yy,zz}]) {
					cerr << "Difference in flat/accessor" << endl;
					return -1;
				}
			}
		}
	}

	auto t = clock();
	for(int64_t xx=0, ii=0; xx < test1->dim(0); xx++) {
		for(int64_t yy=0; yy < test1->dim(1); yy++) {
			for(int64_t zz=0; zz < test1->dim(2); zz++, ii++) {
				index[0] = xx;
				index[1] = yy;
				index[2] = zz;
				total += arr1[index];
			}
		}
	}
	t = clock()-t;
	std::cout << "vector access: " << ((double)t)/CLOCKS_PER_SEC << " s.\n";

	t = clock();
	for(int64_t xx=0, ii=0; xx < test1->dim(0); xx++) {
		for(int64_t yy=0; yy < test1->dim(1); yy++) {
			for(int64_t zz=0; zz < test1->dim(2); zz++, ii++) {
				total += arr1[{xx,yy,zz}];
			}
		}
	}
	t = clock()-t;
	std::cout << "Implicit vector access: " << ((double)t)/CLOCKS_PER_SEC << " s.\n";

	t = clock();
	for(int64_t xx=0, ii=0; xx < test1->dim(0); xx++) {
		for(int64_t yy=0; yy < test1->dim(1); yy++) {
			for(int64_t zz=0; zz < test1->dim(2); zz++, ii++) {
				total += (*test1)[ii];
			}
		}
	}
	t = clock()-t;
	std::cout << "Linear access: " << ((double)t)/CLOCKS_PER_SEC << " s.\n";

	return 0;
}
