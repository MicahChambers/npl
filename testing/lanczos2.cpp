/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * NPL is free software: you can redistribute it and/or modify it under the
 * terms of the BSD 2-Clause License available in LICENSE or at
 * http://opensource.org/licenses/BSD-2-Clause
 *
 ******************************************************************************/

#include <iostream>
#include <vector>

#include <Eigen/Dense>
#include <unsupported/Eigen/IterativeSolvers>

using namespace std;

using Eigen::VectorXd;
using Eigen::MatrixXd;
using Eigen::BandLanczosSelfAdjointEigenSolver;

/**
 * @brief Fills a matrix with random (but symmetric, positive definite) values
 */
void createRandom(MatrixXd& tgt, size_t rank)
{
    assert(tgt.rows() == tgt.cols());
    VectorXd tmp(tgt.rows());
    tgt.setZero();

    for(size_t ii=0; ii<rank; ii++) {
        tmp.setRandom();
        tmp.normalize();
        tgt += tmp*tmp.transpose();
    }
}

int main(int argc, char** argv)
{
    // Size of Matrix to Compute Eigenvalues of
    size_t matsize = 250;

    // Number of orthogonal vectors to start with
    size_t nbasis = 10;

    // Rank of matrix to construct
    size_t nrank = 10;
    if(argc == 2) {
        matsize = atoi(argv[1]);
    } else if(argc == 3) {
        matsize = atoi(argv[1]);
        nbasis = atoi(argv[2]);
    } else if(argc == 4) {
        matsize = atoi(argv[1]);
        nbasis = atoi(argv[2]);
        nrank = atoi(argv[3]);
    } else {
        cerr << "Using default matsize, nbasis, rank (set with: " << argv[0]
            << " [matsize] [nbasis] [rank]" << endl;
    }

    MatrixXd A(matsize, matsize);
    createRandom(A, nrank);
    double trace = A.trace();
    cerr << "Trace=" << trace << endl;

    cerr << "Computing with Eigen::SelfAdjointEigenSolver";
    clock_t t = clock();
    Eigen::SelfAdjointEigenSolver<MatrixXd> egsolver(A);
    t = clock()-t;
    MatrixXd evecs = egsolver.eigenvectors();
    VectorXd evals = egsolver.eigenvalues();
    cerr << "Done ("<<t<<")"<<endl;

    BandLanczosSelfAdjointEigenSolver<MatrixXd> blsolver;
    cerr << "Computing with BandLanczosHermitian";
    t = clock();
    blsolver.compute(A, nbasis);
    t = clock()-t;
    MatrixXd bvecs = blsolver.eigenvectors();
    VectorXd bvals = blsolver.eigenvalues();
    cerr << "Done ("<<t<<")"<<endl;

    size_t egrank = evals.rows();
    size_t blrank = bvals.rows();

    cerr << "Comparing"<<endl;
    for(int ii=1; ii<=std::min(bvals.rows(), evals.rows()); ii++) {
        if(fabs(bvals[blrank-ii])/trace > .01) {
            if(fabs(bvals[blrank-ii] - evals[egrank-ii])/trace > 0.05) {
                cerr << "Difference in eigenvalues" << endl;
                cerr << bvals[blrank-ii] << " vs. " << evals[egrank-ii] << endl;
                return -1;
            }
        }
    }

    for(int ii=1; ii<=std::min(bvals.rows(), evals.rows()); ii++) {
        if(fabs(bvals[blrank-ii])/trace > .01) {
            double v = fabs(bvecs.col(blrank-ii).dot(evecs.col(egrank-ii)));
            cerr << ii << " dot prod = " << v << endl;
            if(fabs(v) < .95) {
                cerr << "Difference in eigenvector " << ii << endl;
                return -1;
            }
        }
    }
    cerr << "Success!"<<endl;

    return 0;
}

