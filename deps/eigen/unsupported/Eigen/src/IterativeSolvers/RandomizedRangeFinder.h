// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2014 Micah Chambers <micahc.vt@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_RANDOMIZED_RANGE_FINDER_H
#define EIGEN_RANDOMIZED_RANGE_FINDER_H

#include <Eigen/Eigenvalues>
#include <Eigen/SVD>
#include <Eigen/QR>
#include <limits>
#include <cmath>
#include <random>

//#define RANDOMIZED_RANGE_FINDER_DEBUG

#ifdef RANDOMIZED_RANGE_FINDER_DEBUG
#include <iostream>
using std::cerr;
using std::endl;
#endif //ifdef RANDOMIZED_RANGE_FINDER_DEBUG

namespace Eigen {

/**
 * @brief Produces a matrix, Q, whose range approximates that of A (the input)
 * From Q
 * It is possible to compute the SVD of A by computing the SVD of * B = Q*A
 * It is possible to compute the Eigenvalues/EigenVectors using the B = Q*AQ
 *
 */
template <typename _MatrixType>
class RandomizedRangeFinder
{
public:
    typedef _MatrixType MatrixType;
    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;
    typedef typename MatrixType::Index Index;
    enum {
        RowsAtCompileTime = MatrixType::RowsAtCompileTime,
        ColsAtCompileTime = MatrixType::ColsAtCompileTime,
        DiagSizeAtCompileTime = EIGEN_SIZE_MIN_PREFER_DYNAMIC(
                RowsAtCompileTime,ColsAtCompileTime),
        MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
        MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime,
        MaxDiagSizeAtCompileTime = EIGEN_SIZE_MIN_PREFER_FIXED(
                MaxRowsAtCompileTime,MaxColsAtCompileTime),
        MatrixOptions = MatrixType::Options
    };

    // Vector Type
    typedef Matrix<Scalar, RowsAtCompileTime, 1, MatrixOptions,
            MaxRowsAtCompileTime, 1> VectorType;

    // Similar Matrix Type
    typedef Matrix<Scalar, Dynamic, Dynamic, MatrixOptions,
            MaxRowsAtCompileTime, MaxDiagSizeAtCompileTime> ApproxType;

    // EigenVectors Type
    typedef Matrix<Scalar, RowsAtCompileTime, Dynamic, MatrixOptions,
            MaxRowsAtCompileTime, MaxDiagSizeAtCompileTime> EigenVectorType;

    // EigenValus Type
    typedef Matrix<Scalar, Dynamic, 1, MatrixOptions,
            MaxDiagSizeAtCompileTime> EigenValuesType;

    /**
     * @brief Basic constructor
     */
    RandomizedRangeFinder()
    {
        init();
    };

    /**
     * @brief Constructor that computes A
     *
     * @param A matrix to compute
     * @param poweriters Number of power iterations to perform.
     * @param rank rank of matrix approximation
     */
    RandomizedRangeFinder(const Ref<const MatrixType> A, size_t poweriters,
			int rank)
    {
        compute(A, poweriters, rank);
    };

    /**
     * @brief Constructor that computes A
     *
     * @param A matrix to compute
     * @param poweriters Number of power iterations to perform.
     * @param rank rank of matrix approximation
     */
    RandomizedRangeFinder(const Ref<const MatrixType> A, size_t poweriters,
			double tol, int minrank, int maxrank)
    {
        compute(A, poweriters, tol, minrank, maxrank);
    };

    /**
     * @brief Constructor that computes A
     *
     * @param A matrix to compute
     * @param poweriters Number of power iterations to perform.
     * @param rank rank of matrix approximation
     */
    void compute(const Ref<const MatrixType> A, size_t poweriters,
			int rank, bool transpose=false)
    {
        init();
		m_poweriters = poweriters;
		m_minrank = rank;
		m_maxrank = rank;

		if(transpose) {
			m_transpose = true;
			_compute(A.transpose());
		} else {
			m_transpose = false;
			_compute(A);
		}
    };

    /**
     * @brief Constructor that computes A
     *
     * @param A matrix to compute
     * @param poweriters Number of power iterations to perform.
     * @param rank rank of matrix approximation
     */
    void compute(const Ref<const MatrixType> A, size_t poweriters,
			double tol, int minrank, int maxrank, bool transpose=false)
    {
        init();
		m_poweriters = poweriters;
		m_tol = tol;
		m_minrank = minrank;
		m_maxrank = maxrank;

		if(transpose) {
			m_transpose = true;
			_compute(A.transpose());
		} else {
			m_transpose = false;
			_compute(A);
		}
    };

    /**
     * @brief Solves A with the initial projection (Krylov
     * basis vectors) set to the random vectors of dimension set by
     * randbasis.
     *
     * @param A matrix to compute the eigensystem of
     * @param randbasis size of random basis vectors to build Krylov basis
     * from, this should be roughly the number of clustered large eigenvalues.
     * If results are not sufficiently good, it may be worth increasing this
     */
    void _compute(const Ref<const MatrixType> A);

protected:

	size_t m_poweriters;
	int m_minrank;
	int m_maxrank;
	double m_tol;
	MatrixType m_Q;
	bool m_transpose;

    void init()
    {
		m_poweriters = 0;
		m_minrank = -1;
		m_maxrank = -1;
		m_tol = 0.01;
    };
};

template <typename T>
void RandomizedRangeFinder<T>::_compute(const Ref<const MatrixType> A)
{
	size_t MINCSIZE = 5;
	MatrixType Yc;
	MatrixType Yhc;
	MatrixType Qtmp;
	MatrixType Qhat;
	MatrixType Qc;
	MatrixType Omega;
	VectorType norms;
	Eigen::HouseholderQR<MatrixType> qr; // QR = Y
	Eigen::HouseholderQR<MatrixType> qrh; // QR = \hat Y
	m_Q.resize(0,0);

	size_t curank, maxrank;
	if(m_minrank <= 1)
		curank = std::ceil(std::log2(std::min(A.cols(), A.rows())));
	else
		curank = m_minrank;
	curank = std::min((size_t)std::min(A.rows(), A.cols()), std::max(MINCSIZE, curank));

	if(m_maxrank <= 1)
		maxrank = std::min(A.cols(), A.rows());
	else
		maxrank = m_maxrank;
	maxrank = std::min<size_t>(maxrank, std::min<size_t>(A.cols(), A.rows()));

	std::random_device rd;
	std::default_random_engine rng(rd());
	std::normal_distribution<double> rdist(0,1);
	do {
		size_t nextsize = std::min(curank, maxrank-curank);

		// Fill With Gaussian Random Numbes
		Omega.resize(A.cols(), nextsize);
		for(size_t cc=0; cc<Omega.cols(); cc++) {
			for(size_t rr=0; rr<Omega.rows(); rr++) {
				Omega(rr, cc) = rdist(rng);
			}
		}
		Yc = A*Omega;

		// Perform Power Iteration set number of times
		qr.compute(Yc);
		Qtmp = qr.householderQ()*MatrixType::Identity(A.rows(), nextsize);
		for(size_t ii=0; ii<m_poweriters; ii++) {
			Yhc = A.transpose()*Qtmp;
			qrh.compute(Yhc);
			Qhat = qrh.householderQ()*MatrixType::Identity(A.cols(), nextsize);
			Yc = A*Qhat;
			qr.compute(Yc);
			Qtmp = qr.householderQ()*MatrixType::Identity(A.rows(), nextsize);
		}

		/*
		 * Orthogonalize new basis with the current basis (Q) and then append
		 */
		if(m_Q.rows() > 0) {
			// Orthogonalize the additional Q vectors Q with respect to the
			// current Q vectors
			Qc = Qtmp - m_Q*(m_Q.transpose()*Qtmp).eval();

			// After orthogonalizing wrt to Q, reorthogonalize wrt each other
			std::list<int> keep;
			size_t csize = 0;
			for(size_t cc=0; cc<Qc.cols(); cc++) {
				for(std::list<int>::iterator it=keep.begin(); it!=keep.end(); ++it)
					Qc.col(cc) -= Qc.col(*it).dot(Qc.col(cc))*Qc.col(*it);
				double norm = Qc.col(cc).norm();
				if(norm > m_tol) {
					keep.push_back(cc);
					Qc.col(cc) /= norm;
					csize = 0;
				} else
					csize++;
				// Check if we have reached the threshold to stop based on run
				// of small values
				if(csize >= MINCSIZE)
					break;
			}

			if(keep.empty()) {
#ifdef RANDOMIZED_RANGE_FINDER_DEBUG
				cerr << "Keep Empty, Breaking"<<endl;
#endif //RANDOMIZED_RANGE_FINDER_DEBUG
				break;
			}

			// Append Orthogonalized Basis, might want to remove the Q vectors
			// that are past the threshold for stopping (ie 0...bestcstart-1)
#ifdef RANDOMIZED_RANGE_FINDER_DEBUG
			cerr<<"Keeping "<<keep.size()<<endl;
#endif //RANDOMIZED_RANGE_FINDER_DEBUG
			m_Q.conservativeResize(Qc.rows(), m_Q.cols()+keep.size());
			size_t ii=curank;
			for(std::list<int>::iterator it = keep.begin(); it != keep.end(); ++it)
				m_Q.col(ii++) = Qc.col(*it);

			// Break Because the probability is really low that there is now
			// information
			if(csize >= MINCSIZE )
				break;
		} else {
			m_Q = Qtmp;
		}

		curank = m_Q.cols();
#ifdef RANDOMIZED_RANGE_FINDER_DEBUG
		cerr << "Currank: " << curank <<endl;
#endif //RANDOMIZED_RANGE_FINDER_DEBUG
	} while(curank < maxrank);
#ifdef RANDOMIZED_RANGE_FINDER_DEBUG
	cerr << "Q: " << endl << m_Q << endl;
#endif //RANDOMIZED_RANGE_FINDER_DEBUG
}

/**
 * @brief Performs Randomized Range Calculation on input matrix the uses that
 * the similar matrix to perform SVD Decomposition
 *
 */
template <typename _MatrixType>
class RandomRangeSVD : RandomizedRangeFinder<_MatrixType>
{
public:
    typedef _MatrixType MatrixType;
    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;
    typedef typename MatrixType::Index Index;
    enum {
        RowsAtCompileTime = MatrixType::RowsAtCompileTime,
        ColsAtCompileTime = MatrixType::ColsAtCompileTime,
        DiagSizeAtCompileTime = EIGEN_SIZE_MIN_PREFER_DYNAMIC(
                RowsAtCompileTime,ColsAtCompileTime),
        MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
        MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime,
        MaxDiagSizeAtCompileTime = EIGEN_SIZE_MIN_PREFER_FIXED(
                MaxRowsAtCompileTime,MaxColsAtCompileTime),
        MatrixOptions = MatrixType::Options
    };

    // Vector Type
    typedef Matrix<Scalar, RowsAtCompileTime, 1, MatrixOptions,
            MaxRowsAtCompileTime, 1> VectorType;

    /**
     * @brief Basic constructor
     */
    RandomRangeSVD()
    {
		init();
    };

    /**
     * @brief Constructor that computes A
     *
     * @param A matrix to compute
     * @param poweriters Number of power iterations to perform.
     * @param rank rank of matrix approximation
     */
	RandomRangeSVD(const Ref<const MatrixType> A, size_t poweriters,
			size_t rank, unsigned int computationOptions)
    {
		compute(A, poweriters, rank, computationOptions);
    };

    /**
     * @brief Constructor that computes A
     *
     * @param A matrix to compute
     * @param poweriters Number of power iterations to perform.
     * @param rank rank of matrix approximation
     */
    RandomRangeSVD(const Ref<const MatrixType> A, size_t poweriters,
			double tol, int minrank, int maxrank,
			unsigned int computationOptions)
    {
		compute(A, poweriters, tol, minrank, maxrank, computationOptions);
    };

    /**
     * @brief Constructor that computes A
     *
     * @param A matrix to compute
     * @param poweriters Number of power iterations to perform.
     * @param rank rank of matrix approximation
     */
	void compute(const Ref<const MatrixType> A, size_t poweriters,
			size_t rank, unsigned int computationOptions)
    {
		m_compU = (computationOptions & Eigen::ComputeThinU);
		m_compV = (computationOptions & Eigen::ComputeThinV);

		RandomizedRangeFinder<_MatrixType>::compute(A, poweriters, rank);
        _compute(A);
    };

    /**
     * @brief Constructor that computes A
     *
     * @param A matrix to compute
     * @param poweriters Number of power iterations to perform.
     * @param rank rank of matrix approximation
     */
    void compute(const Ref<const MatrixType> A, size_t poweriters,
			double tol, int minrank, int maxrank,
			unsigned int computationOptions)
    {
		m_compU = (computationOptions & Eigen::ComputeThinU);
		m_compV = (computationOptions & Eigen::ComputeThinV);

		RandomizedRangeFinder<_MatrixType>::compute(A, poweriters, tol,
				minrank, maxrank);
        _compute(A);
    };

    /**
     * @brief Compute the U,V,E matrices
     */
    void _compute(const Ref<const MatrixType> A)
	{
		if(this->m_transpose) {
			// computed B = Q*A* = UEV*, A = VEU*Q*, U_A=V, U_E=E, V_A=QU
			// U*Q = V_A*
			MatrixType B = this->m_Q.transpose()*A.transpose();
			JacobiSVD<MatrixType> svd(B, ComputeThinV | ComputeThinU);
			m_V = this->m_Q*svd.matrixU();
			m_U = svd.matrixV();
			m_E = svd.singularValues();
		} else {
			// computed B = Q*A = UEV*, A = QUEV*, U_A=QU, U_E=E, V_A=V
			// U*Q = V_A*
			MatrixType B = this->m_Q.transpose()*A;
			JacobiSVD<MatrixType> svd(B, ComputeThinU | ComputeThinV);
			m_U = this->m_Q*svd.matrixU();
			m_V = svd.matrixV();
			m_E = svd.singularValues();
		}
	};

	const Ref<const MatrixType> singularValues()
	{
		return m_E;
	}

	const Ref<const MatrixType> matrixU()
	{
		eigen_assert(m_compU && "Error must set ComputeThinU prior to "
				"calling matrixU");
		return m_U;
	}

	const Ref<const MatrixType> matrixV()
	{
		eigen_assert(m_compV && "Error must set ComputeThinV prior to "
				"calling matrixV");
		return m_V;
	}
private:

	void init()
	{
		m_compU = false;
		m_compV = false;
	};

	bool m_compV;
	bool m_compU;

	MatrixType m_V;
	MatrixType m_U;
	VectorType m_E;
};

/**
 * @brief Performs Randomized Range Calculation on input matrix the uses that
 * the similar matrix to perform SVD Decomposition
 *
 */
template <typename _MatrixType>
class RandomRangeSelfAdjointEigenSolver : RandomizedRangeFinder<_MatrixType>
{
public:
    typedef _MatrixType MatrixType;
    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;
    typedef typename MatrixType::Index Index;
    enum {
        RowsAtCompileTime = MatrixType::RowsAtCompileTime,
        ColsAtCompileTime = MatrixType::ColsAtCompileTime,
        DiagSizeAtCompileTime = EIGEN_SIZE_MIN_PREFER_DYNAMIC(
                RowsAtCompileTime,ColsAtCompileTime),
        MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
        MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime,
        MaxDiagSizeAtCompileTime = EIGEN_SIZE_MIN_PREFER_FIXED(
                MaxRowsAtCompileTime,MaxColsAtCompileTime),
        MatrixOptions = MatrixType::Options
    };

    // Vector Type
    typedef Matrix<Scalar, RowsAtCompileTime, 1, MatrixOptions,
            MaxRowsAtCompileTime, 1> VectorType;

    // Similar Matrix Type
    typedef Matrix<Scalar, Dynamic, Dynamic, MatrixOptions,
            MaxRowsAtCompileTime, MaxDiagSizeAtCompileTime> ApproxType;

    // EigenVectors Type
    typedef Matrix<Scalar, RowsAtCompileTime, Dynamic, MatrixOptions,
            MaxRowsAtCompileTime, MaxDiagSizeAtCompileTime> EigenVectorType;

    // EigenValus Type
    typedef Matrix<Scalar, Dynamic, 1, MatrixOptions,
            MaxDiagSizeAtCompileTime> EigenValuesType;

    /**
     * @brief Basic constructor
     */
    RandomRangeSelfAdjointEigenSolver()
    {
		m_comp_evecs = false;
    };

    /**
     * @brief Constructor that computes A
     *
     * @param A matrix to compute
     * @param poweriters Number of power iterations to perform.
     * @param rank rank of matrix approximation
     */
	RandomRangeSelfAdjointEigenSolver(const Ref<const MatrixType> A,
			size_t poweriters, size_t rank, bool computeEigenvectors)
    {
		compute(A, poweriters, rank, computeEigenvectors);
    };

    /**
     * @brief Constructor that computes A
     *
     * @param A matrix to compute
     * @param poweriters Number of power iterations to perform.
     * @param rank rank of matrix approximation
     */
	RandomRangeSelfAdjointEigenSolver(const Ref<const MatrixType> A,
			size_t poweriters, double tol, int minrank, int maxrank,
			bool computeEigenvectors)
    {
		compute(A, poweriters, tol, minrank, maxrank, computeEigenvectors);
    };

    /**
     * @brief Constructor that computes A
     *
     * @param A matrix to compute
     * @param poweriters Number of power iterations to perform.
     * @param rank rank of matrix approximation
     */
	void compute(const Ref<const MatrixType> A, size_t poweriters,
			int rank, bool computeEigenvectors)
    {
		m_comp_evecs = computeEigenvectors;
		RandomizedRangeFinder<_MatrixType>::compute(A, poweriters, rank);
        _compute(A);
    };

    /**
     * @brief Constructor that computes A
     *
     * @param A matrix to compute
     * @param poweriters Number of power iterations to perform.
     * @param rank rank of matrix approximation
     */
    void compute(const Ref<const MatrixType> A, size_t poweriters,
			double tol, int minrank, int maxrank,
			bool computeEigenvectors)
    {
		m_comp_evecs = computeEigenvectors;
		RandomizedRangeFinder<_MatrixType>::compute(A, poweriters, tol, minrank, maxrank);
        _compute(A);
    };

    /**
     * @brief Compute the U,V,E matrices
     */
    void _compute(const Ref<const MatrixType> A)
	{
		MatrixType B = this->m_Q.transpose()*A*this->m_Q;
		SelfAdjointEigenSolver<MatrixType> eig(B);
		m_evecs = this->m_Q*eig.eigenvectors();
		m_evals = eig.eigenvalues();
	};

private:

	bool m_comp_evecs;
	MatrixType m_evecs;
	VectorType m_evals;
};


} // end namespace Eigen

#endif // EIGEN_RANDOMIZED_RANGE_FINDER_H

