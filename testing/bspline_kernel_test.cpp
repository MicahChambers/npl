/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * NPL is free software: you can redistribute it and/or modify it under the
 * terms of the BSD 2-Clause License available in LICENSE or at
 * http://opensource.org/licenses/BSD-2-Clause
 *
 * @file bspline_kernel_test.cpp Tests the spline kernel.
 *
 *****************************************************************************/

#include "basic_functions.h"
#include <iostream>
#include "basic_plot.h"

using namespace npl;
using namespace std;

int main()
{
    Plotter plt;
    plt.addFunc(B3kern);
    plt.addFunc(dB3kern);
    plt.addFunc(ddB3kern);
    plt.setXRange(-3, 3);
    plt.setYRange(-3.5, 2.5);
    plt.write("bspline.svg");

    Plotter plt2;
    plt2.addFunc([](double x) { return B3kern(x*2)*2; } );
    plt2.addFunc([](double x) { return dB3kern(x*2)*2; } );
    plt2.addFunc([](double x) { return ddB3kern(x*2)*2; } );
    plt2.setXRange(-3, 3);
    plt2.setYRange(-3.5, 2.5);
    plt2.write("bspline_rad1.svg");

    double sum = 0;

    cerr << "Radius 2: " << endl;
    sum = 0;
    for(int64_t ii=-4; ii<4; ii++) {
        cerr << ii << "," << B3kern(ii, 2)<< endl;
        sum += B3kern(ii, 2);
    }
    cerr << "Sum: " << sum << endl;
    if(fabs(sum - 1) > 0.000001)
        return -1;

    cerr << "Radius 4: " << endl;
    sum = 0;
    for(int64_t ii=-4; ii<4; ii++) {
        cerr << ii << "," << B3kern(ii, 4) << endl;
        sum += B3kern(ii, 4);
    }
    cerr << "Sum: " << sum << endl;
    if(fabs(sum - 1) > 0.000001)
        return -1;

    cerr << "Radius 8: " << endl;
    sum = 0;
    for(int64_t ii=-8; ii<8; ii++) {
        cerr << ii << "," << B3kern(ii, 8)<< endl;
        sum += B3kern(ii, 8);
    }
    cerr << "Sum: " << sum << endl;
    if(fabs(sum - 1) > 0.000001)
        return -1;

    cerr << "Radius 3: " << endl;
    sum = 0;
    for(int64_t ii=-8; ii<8; ii++) {
        cerr << ii << "," << B3kern(ii, 3)<< endl;
        sum += B3kern(ii, 3);
    }
    cerr << "Sum: " << sum << endl;
    if(fabs(sum - 1) > 0.01)
        return -1;

    cerr << "Radius 3.5: " << endl;
    sum = 0;
    for(int64_t ii=-8; ii<8; ii++) {
        cerr << ii << "," << B3kern(ii, 3.5)<< endl;
        sum += B3kern(ii, 3.5);
    }
    cerr << "Sum: " << sum << endl;
    if(fabs(sum - 1) > 0.01)
        return -1;
}

