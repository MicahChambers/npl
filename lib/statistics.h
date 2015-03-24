/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * NPL is free software: you can redistribute it and/or modify it under the
 * terms of the BSD 2-Clause License available in LICENSE or at
 * http://opensource.org/licenses/BSD-2-Clause
 *
 * @file statistics.h Tools for analyzing data, including PCA, ICA and
 * general linear modeling.
 *
 *****************************************************************************/

#ifndef STATISTICS_H
#define STATISTICS_H

#include <Eigen/Dense>
#include "npltypes.h"
#include "mrimage.h"

namespace npl {

/** \defgroup StatFunctions Statistical Functions
 *
 * @{
 */

/**
 * @brief Computes the statistical variance of a column vector
 *
 * @param vec Input samples
 *
 * @return Variance
 */
double sample_var(const Ref<const VectorXd> vec);

/**
 * @brief Removes the effects of X from signal (y). Note that this takes both X
 * and the pseudoinverse of X because the bulk of the computation will be on
 * the pseudoinverse.
 *
 * Each column of X should be a regressor, Xinv should be the pseudoinverse of X
 *
 * Beta in OLS may be computed with the pseudoinverse (P):
 * B = Py
 * where P is the pseudoinverse:
 * P = VE^{-1}U^*
 *
 * @param signal response term (y), will be modified to remove the effects of X
 * @param X Design matrix, or independent values in colums
 * @param Xinv the pseudoinverse of X
 */
inline
void regressOutLS(VectorXd& signal, const MatrixXd& X, const MatrixXd& covInv)
{
	VectorXd beta = covInv*X.transpose()*signal;
	signal -= X*beta;
}

template <typename T>
void fillGaussian(Ref<T> m)
{
	static std::random_device rd;
	static std::default_random_engine rng(rd());
	std::normal_distribution<double> rdist(0,1);

	for(size_t cc=0; cc<m.cols(); cc++) {
		for(size_t rr=0; rr<m.rows(); rr++) {
			m(rr, cc) = rdist(rng);
		}
	}
}


/******************************************
 * \defgroup Basic Stats Functions
 * @{
 *****************************************/

/**
 * @brief 1D Gaussian distribution function
 *
 * @param mean Mean of gaussian distribution
 * @param sd Standard deviation
 * @param x Position to get probability of
 *
 * @return Probability Density at Point
 */
double gaussianPDF(double mean, double sd, double x);

/**
 * @brief 1D Gaussian cumulative distribution function
 *
 * @param mean Mean of gaussian distribution
 * @param sd Standard deviation
 * @param x Position to get probability of
 *
 * @return Probability of value falling below x
 */
double gaussianCDF(double mean, double sd, double x);

/**
 * @brief PDF for the gamma distribution, if mean is negative then it is
 * assumed that x should be negated as well. Unlike gammaPDF, this takes
 * the mean and standard deviation (_MS)
 *
 * mean = k theta
 * var = k theta theta
 *
 * theta = var / mean
 * k = mean / theta
 *
 * prob(mu, sd, x) = x^{k-1}exp(-x/theta)/(gamma(k) theta^k)
 * log prob(mu, sd, x) = (k-1)log(x)-x/theta-log(gamma(k)) - k*log(theta)
 *
 * @param mean Mean to compute density of
 * @param sd Standard deviation of distribution
 * @param x X position to compute density of (<0 produces 0 probability unless
 * mu is negative
 *
 * @return Probability Density
 */
double gammaPDF_MS(double mean, double sd, double x);

/**
 * @brief Computes mutual information between signal a and signal b which
 * are of length len. Marginal bins used is mbin
 *
 * @param len Length of signal and and b
 * @param a Signal a
 * @param b Signal b
 * @param mbin Bins to use in marginal distribution (mbin*mbin) used in joint
 *
 * @return
 */
double mutualInformation(size_t len, double* a, double* b, size_t mbin);

/**
 * @brief Computes correlation between signal a and signal b which
 * are of length len.
 *
 * @param len Length of signal and and b
 * @param a Signal a
 * @param b Signal b
 *
 * @return
 */
double correlation(size_t len, double* a, double* b);

/**
 * @brief Takes a count, sum and sumsqr and returns the sample variance.
 * This is slightly different than the variance definition and I can
 * never remember the exact formulation.
 *
 * @param count Number of samples
 * @param sum sum of samples
 * @param sumsqr sum of square of the samples
 *
 * @return sample variance
 */
inline
double sample_var(int count, double sum, double sumsqr)
{
	return (sumsqr-sum*sum/count)/(count-1);
}

/**
 * @brief Computes the sample correlation.
 *
 * @param count 	Number of samples
 * @param sum1 		Sum of group 1
 * @param sum2		Sum of group 2
 * @param sumsq1	Sum of the sqaure of the elements of group1
 * @param sumsq2	Sum of the sqaure of the elements of group2
 * @param s1s2		Elements of group1*elements of group 2
 *
 * @return Sample Correlation
 */
inline
double sample_corr(int count, double sum1, double sum2,
		double sumsq1, double sumsq2, double s1s2)
{
	return (count*s1s2-sum1*sum2)/
			sqrt((count*sumsq1-sum1*sum1)*(count*sumsq2-sum2*sum2));
}

struct RegrResult
{
	/**
	 * @brief Predicted y values, based on estimate of Beta
	 */
	VectorXd yhat;

	/**
	 * @brief Estimated Beta
	 */
	VectorXd bhat;

	/**
	 * @brief Sum of square of the residuals
	 */
	double ssres;

	/**
	 * @brief sigma hat - estimate standard deviation of noise
	 */
	double sigmahat;

	/**
	 * @brief Coefficient of determination (Rsqr)
	 */
	double rsqr;

	/**
	 * @brief Coefficient of determination, corrected for the number of
	 * regressors.
	 */
	double adj_rsqr;

	/**
	 * @brief Standard errors for each of the regressors
	 */
	VectorXd std_err;

	/**
	 * @brief Degrees of freedom in the regression
	 */
	double dof;

	/**
	 * @brief Students t score of each of the regressors
	 */
	VectorXd t;

	/**
	 * @brief Significance of each of the regressors.
	 */
	VectorXd p;
};

/**
 * @brief Student's T-distribution. A cache of the Probability Density Function and
 * cumulative density function is created using the analytical PDF.
 */
class StudentsT
{
public:
	/**
	 * @brief Defualt constructor takes the degrees of freedom (Nu), step size
	 * for numerical CDF computation and Maximum T to sum to for numerical CDF
	 *
	 * @param dof Degrees of freedom (shape parameter)
	 * @param dt step size in x or t to take
	 * @param tmax Maximum t to consider
	 */
	StudentsT(int dof = 2, double dt = 0.1, double tmax = 20);

	/**
	 * @brief Change the degress of freedom, update cache
	 *
	 * @param dof Shape parameter, higher values make the distribution more
	 * gaussian
	 */
	void setDOF(double dof);

	/**
	 * @brief Step in t to use for computing the CDF, smaller means more
	 * precision although in reality the distribution is quite smooth, and
	 * linear interpolation should be very good.
	 *
	 * @param dt Step size for numerical integration
	 */
	void setStepT(double dt);

	/**
	 * @brief Set the maximum t for numerical integration, and recompute the
	 * cdf/pdf caches.
	 *
	 * @param tmax CDF and PDF are stored as arrays, this is the maximum
	 * acceptable t value. Its RARE (like 10^-10 rare) to have a value higher
	 * than 20.
	 */
	void setMaxT(double tmax);

	/**
	 * @brief Get the cumulative probability at some t value.
	 *
	 * @param t T (or x, distance from center) value to query
	 *
	 * @return Cumulative probability (probability value of value < t)
	 */
	double cumulative(double t) const;

	/**
	 * @brief Get the cumulative probability at some t value.
	 *
	 * @param t T (or x, distance from center) value to query
	 *
	 * @return Cumulative probability (probability value of value < t)
	 */
	double cdf(double t) const { return cumulative(t); };

	/**
	 * @brief Get the probability density at some t value.
	 *
	 * @param t T value to query
	 *
	 * @return Probability density at t.
	 */
	double density(double t) const;

	/**
	 * @brief Get the probability density at some t value.
	 *
	 * @param t T value to query
	 *
	 * @return Probability density at t.
	 */
	double pdf(double t) const { return density(t); };

	/**
	 * @brief Get the T-score that corresponds to a particular p-value.
	 * Alias for tthresh
	 *
	 * @param t t value to get the inverse CDF (area under the curve) for
	 *
	 * @return T value that matches p.
	 */
	double icdf(double t) const;

	/**
	 * @brief Get the T-score that corresponds to a particular p-value.
	 * Alias for icdf
	 *
	 * @param p probability value is less than this
	 *
	 * @return T value that matches p.
	 */
	double tthresh(double p) const { return icdf(p); };

private:
	void init();

	double m_dt;
	double m_tmax;
	int m_dof;
	std::vector<double> m_cdf;
	std::vector<double> m_pdf;
	std::vector<double> m_tvals;
};


/**
 * @brief Computes the Ordinary Least Square predictors, beta for
 *
 * \f$ y = \hat \beta X \f$
 *
 * Returning beta. This is the same as the other regress function, but allows
 * for cacheing of pseudoinverse of X
 *
 * @param out Struct with Regression Results.
 * @param y response variables
 * @param X independent variables
 * @param covInv Inverse of covariance matrix, to compute us pseudoinverse(X^TX)
 * @param Xinv Pseudo inverse of X. Compute with pseudoInverse(X)
 * @param distrib Pre-computed students' T distribution. Example:
 * auto v = students_t_cdf(X.rows()-1, .1, 1000);
 *
 * @return Struct with Regression Results.
 */
void regress(RegrResult* out,
		const Ref<const VectorXd> y,
		const Ref<const MatrixXd> X,
		const Ref<const VectorXd> covInv,
		const Ref<const MatrixXd> Xinv,
		const StudentsT& distrib);

/**
 * @brief Computes the Ordinary Least Square predictors, beta for
 *
 * \f$ y = \hat \beta X \f$
 *
 * Returning beta. This is the same as the other regress function, but allows
 * for cacheing of pseudoinverse of X
 *
 * @param out Struct with Regression Results.
 * @param y response variables
 * @param X independent variables
 *
 */
void regress(RegrResult* out,
		const Ref<const VectorXd> y,
		const Ref<const MatrixXd> X);

/******************************************
 * @}
 * \defgroup Matrix Decompositions
 * @{
 *****************************************/

/**
 * @brief
 *
 * @param A M x N
 * @param subsize Columns in projection matrix,
 * @param poweriters Number of power iterations to perform
 * @param U Output Left Singular Vectors
 * @param E OUtput Singular Values
 * @param V Output Right Singular Vectors
 */
void randomizePowerIterationSVD(const Ref<const MatrixXd> A,
		size_t subsize, size_t poweriters, MatrixXd& U, VectorXd& E,
		MatrixXd& V);

void randomizePowerIterationSVD(const Ref<const MatrixXd> A,
		double tol, size_t startrank, size_t maxrank, size_t poweriters,
		MatrixXd& U, VectorXd& E, MatrixXd& V);

/**
 * @brief Computes the Principal Components of input matrix X
 *
 * Outputs reduced dimension (fewer cols) in output. Note that prior to this,
 * the columns of X should be 0 mean otherwise the first component will
 * be the mean
 *
 * @param X 	RxC matrix where each column row is a sample, each column a
 * dimension (or feature). The number of columns in the output
 * will be fewer because there will be fewer features
 * @param varth Variance threshold. This is the ratio (0-1) of variance to
 * include in the output. This is used to determine the dimensionality of the
 * output. If this is 1 then all variance will be included. If this < 1 and
 * odim > 0 then whichever gives a larger output dimension will be selected.
 * @param odim Threshold for output dimensions. If this is <= 0 then it is
 * ignored, if it is > 0 then max(dim(varth), odim) is used as the output
 * dimension.
 *
 * @return 		RxP matrix, where P is the number of principal components
 */
MatrixXd pca(const Ref<const MatrixXd> X, double varth = 1, int odim = -1);

/**
 * @brief Computes the Principal Components of input matrix X using the
 * randomized power iteration SVD algorithm.
 *
 * Outputs reduced dimension (fewer cols) in output. Note that prior to this,
 * the columns of X should be 0 mean otherwise the first component will
 * be the mean
 *
 * @param X 	RxC matrix where each column row is a sample, each column a
 * dimension (or feature). The number of columns in the output
 * will be fewer because there will be fewer features
 * @param varth Variance threshold. This is the ratio (0-1) of variance to
 * include in the output. This is used to determine the dimensionality of the
 * output. If this is 1 then all variance will be included. If this < 1 and
 * odim > 0 then whichever gives a larger output dimension will be selected.
 * @param odim Threshold for output dimensions. If this is <= 0 then it is
 * ignored, if it is > 0 then max(dim(varth), odim) is used as the output
 * dimension.
 *
 * @return 		RxP matrix, where P is the number of principal components
 */
MatrixXd rpiPCA(const Ref<const MatrixXd> X, double varth, int odim);

/**
 * @brief Computes the Independent Components of input matrix X using symmetric
 * decorrlation. Note that you should run PCA on X before running ICA.
 *
 * Same number of cols as output
 *
 * Note that this whole problem is the transpose of the version listed on
 * wikipedia.
 *
 * In: I number of components
 * In: X RxC matrix with rows representing C-D samples
 * for p in 1 to I:
 *   wp = random weight
 *   while wp changes:
 *     wp = g(X wp)X/R - wp SUM(g'(X wp))/R
 *     wp = wp - SUM wp^T wj wj
 *     normalize(wp)
 *
 * Output: W = [w0 w1 w2 ... ]
 * Output: S = XW, where each column is a dimension, each row a sample
 *
 * @param Xin 	RxC matrix where each row is a sample, each column a
 * dimension (or feature). The number of columns in the output
 * will be fewer because there will be fewer features.
 * Columns should be zero-mean and uncorrelated with one another.
 *
 * @return 		RxP matrix, where P is the number of independent components
 */
MatrixXd symICA(const Ref<const MatrixXd> Xin, MatrixXd* unmix = NULL);

/**
 * @brief Computes the Independent Components of input matrix X using
 * sequential component extraction. Note that
 * you should run PCA on X before running ICA.
 *
 * Outputs reduced dimension (fewer cols) in output
 *
 * Note that this whole problem is the transpose of the version listed on
 * wikipedia.
 *
 * In: I number of components
 * In: X RxC matrix with rows representing C-D samples
 * for p in 1 to I:
 *   wp = random weight
 *   while wp changes:
 *     wp = g(X wp)X/R - wp SUM(g'(X wp))/R
 *     wp = wp - SUM wp^T wj wj
 *     normalize(wp)
 *
 * Output: W = [w0 w1 w2 ... ]
 * Output: S = XW, where each column is a dimension, each row a sample
 *
 * @param Xin 	RxC matrix where each row is a sample, each column a
 * dimension (or feature). The number of columns in the output
 * will be fewer because there will be fewer features.
 * Columns should be zero-mean and uncorrelated with one another.
 *
 * @return 		RxP matrix, where P is the number of independent components
 */
MatrixXd asymICA(const Ref<const MatrixXd> Xin, MatrixXd* unmix = NULL);

	/**
 * @brief Computes the pseudoinverse of the input matrix
 *
 * \f$ P = UE^-1V^* \f$
 *
 * @return Psueodinverse
 */
MatrixXd pseudoInverse(const Ref<const MatrixXd> X);

/**
 * @brief Performs LASSO regression using the 'shooting' algorithm of
 *
 * Fu, W. J. (1998). Penalized Regressions: The Bridge versus the Lasso.
 * Journal of Computational and Graphical Statistics, 7(3), 397.
 * doi:10.2307/1390712
 *
 * Essentially solves the equation:
 *
 * y = X * beta
 *
 * where beta is mostly 0's
 *
 * @param X Design matrix
 * @param y Measured value
 * @param gamma Weight of regularization (larger values forces sparser model)
 *
 * @return Beta vector
 */
VectorXd shootingRegr(const Ref<const MatrixXd> X,
		const Ref<const VectorXd> y, double gamma);

/**
 * @brief Performs LASSO regression using the 'activeShooting' algorithm of
 *
 * Peng, J., Wang, P., Zhou, N., & Zhu, J. (2009). Partial Correlation
 * Estimation by Joint Sparse Regression Models. Journal of the American
 * Statistical Association, 104(486), 735–746. doi:10.1198/jasa.2009.0126
 *
 * Essentially solves the equation:
 *
 * y = X * beta
 *
 * where beta is mostly 0's
 *
 * @param X Design matrix
 * @param y Measured value
 * @param gamma Weight of regularization (larger values forces sparser model)
 *
 * @return Beta vector
 */
VectorXd activeShootingRegr(const Ref<const MatrixXd> X,
		const Ref<const VectorXd> y, double gamma);


/**
 * @brief Computes the Principal Components of input matrix X using the
 * covariance matrix.
 *
 * Outputs projections in the columns of a matrix.
 *
 * @param cov 	Covariance matrix of XtX
 * @param varth Variance threshold. Don't include dimensions after this percent
 * of the variance has been explained.
 *
 * @return 	RxP matrix, where P is the number of principal components
 */
MatrixXd pcacov(const Ref<const MatrixXd> cov, double varth);

/* @}
 * \defgroup Clustering algorithms
 * @{
 */

/**
 * @brief Computes the mean and standard deviation of multiple distributions
 * based on 1D data. The probability distribution functions should be passed
 * in through a vector of function objects (pdfs) taking mu/sd/x
 *
 * @param data Data points to fit
 * @param pdfs Probability distribution functions of the form pdf(mu, sd, x)
 * and returning the probability density at x
 * @param mean Output mean of each distribution, value when called will be used
 * for initialization so it should be pre-set and pre-allocated
 * @param sd Output standard deviation of each distribution, value when called
 * will be used for initialization so it should be pre-set and pre-allocated
 * @param prior Output prior probability of each distribution, initial value is
 * not used but it should be pre-allocated
 * @param plotfile file to plot histogram in (svg)
 */
void expMax1D(const Ref<const VectorXd> data,
		vector<std::function<double(double,double,double)>> pdfs,
		Ref<VectorXd> mean, Ref<VectorXd> sd, Ref<VectorXd> prior,
		std::string plotfile = "");

/**
 * @brief Computes the mean and standard deviation of multiple distributions
 * based on 1D data. This is a special version in which a negative gamma, a
 * gaussian and positive gamma. The means of the negative and positive gamma
 * are relative to the center of the gaussian
 *
 * @param data Data points to fit
 * @param pdfs Probability distribution functions of the form pdf(mu, sd, x)
 * and returning the probability density at x
 * @param mean Output mean of each distribution, value when called will be used
 * for initialization so it should be pre-set and pre-allocated
 * @param sd Output standard deviation of each distribution, value when called
 * will be used for initialization so it should be pre-set and pre-allocated
 * @param prior Output prior probability of each distribution, initial value is
 * not used but it should be pre-allocated
 * @param plotfile file to plot histogram in (svg)
 */
void gaussGammaMixtureModel(const Ref<const VectorXd> data,
		Ref<VectorXd> mu, Ref<VectorXd> sd, Ref<VectorXd> prior,
		std::string plotfile);

/**
 * @brief Approximates k-means using the algorithm of:
 *
 * 'Fast Approximate k-Means via Cluster Closures' by Wang et al
 *
 * This is not really meant to be used by outside users, but as an initializer
 *
 * @param samples Matrix of samples, one sample per row,
 * @param nclass Number of classes to break samples up into
 * @param means Estimated mid-points/means
 */
void approxKMeans(const Ref<const MatrixXd> samples,
		size_t nclass, MatrixXd& means);

/**
 * @brief Base class for all ND classifiers.
 */
class Classifier
{
public:
	/**
	 * @brief Initializes the classifier
	 *
	 * @param rank Number of dimensions of samples
	 */
	Classifier(size_t rank) : ndim(rank), maxit(-1), m_valid(false) {};

	/**
	 * @brief Given a matrix of samples (Samples x Dims, sample on each row),
	 * apply the classifier to each sample and return a vector of the classes.
	 *
	 * @param samples Set of samples, 1 per row
	 *
	 * @return Vector of classes, rows match up with input sample rows
	 */
	virtual
	Eigen::VectorXi classify(const Ref<const MatrixXd> samples) = 0;

	/**
	 * @brief Given a matrix of samples (Samples x Dims, sample on each row),
	 * apply the classifier to each sample and return a vector of the classes.
	 *
	 * @param samples Set of samples, 1 per row
	 * @param oclass Output classes. This vector will be resized to have the
	 * same number of rows as samples matrix.
	 *
	 * @return the number of changed classifications
	 */
	virtual
	size_t classify(const Ref<const MatrixXd> samples, Ref<VectorXi> oclass) = 0;

	/**
	 * @brief Updates the classifier with new samples, if reinit is true then
	 * no prior information will be used. If reinit is false then any existing
	 * information will be left intact. In Kmeans that would mean that the
	 * means will be left at their previous state.
	 *
	 * @param samples Samples, S x D matrix with S is the number of samples and
	 * D is the dimensionality. This must match the internal dimension count.
	 * @param reinit whether to reinitialize the  classifier before updating
	 *
	 * return -1 if maximum number of iterations hit, 0 otherwise (converged)
	 */
	virtual
		int update(const Ref<const MatrixXd> samples, bool reinit = false) = 0;

	/**
	 * @brief Alias for updateClasses with reinit = true. This will perform
	 * a classification scheme on all the input samples.
	 *
	 * @param samples Samples, S x D matrix with S is the number of samples and
	 * D is the dimensionality. This must match the internal dimension count.
	 */
	inline
	void compute(const Ref<const MatrixXd> samples)
	{
		update(samples, true);
	};

	/**
	 * @brief Number of dimensions, must be set at construction. This is the
	 * number of columns in input samples.
	 */
	const int ndim;

	/**
	 * @brief Maximum number of iterations. Set below 0 for infinite.
	 */
	int maxit;
protected:
	/**
	 * @brief Whether the classifier has been initialized yet
	 */
	bool m_valid;

};

/**
 * @brief K-means classifier.
 */
class KMeans : public Classifier
{
public:
	/**
	 * @brief Constructor for k-means class
	 *
	 * @param rank Number of dimensions in input samples.
	 * @param k Number of groups to classify samples into
	 */
	KMeans(size_t rank, size_t k = 2);

	/**
	 * @brief Update the number of groups. Note that this invalidates any
	 * current information
	 *
	 * @param ngroups Number of groups to classify
	 */
	void setk(size_t ngroups);

	/**
	 * @brief Sets the mean matrix. Each row of the matrix is a ND-mean, where
	 * N is the number of columns.
	 *
	 * @param newmeans Matrix with new mean
	 */
	void updateMeans(const Ref<const MatrixXd> newmeans);

	/**
	 * @brief Updates the mean coordinates by providing a set of labeled samples.
	 *
	 * @param samples Matrix of samples, where each row is an ND-sample.
	 * @param classes Classes, where rows match the rows of the samples matrix.
	 * Classes should be integers 0 <= c < K where K is the number of classes
	 * in this.
	 */
	void updateMeans(const Ref<const MatrixXd> samples,
			const Ref<const Eigen::VectorXi> classes);

	/**
	 * @brief Given a matrix of samples (Samples x Dims, sample on each row),
	 * apply the classifier to each sample and return a vector of the classes.
	 *
	 * @param samples Set of samples, 1 per row
	 *
	 * @return Vector of classes, rows match up with input sample rows
	 */
	VectorXi classify(const Ref<const MatrixXd> samples);

	/**
	 * @brief Given a matrix of samples (Samples x Dims, sample on each row),
	 * apply the classifier to each sample and return a vector of the classes.
	 *
	 * @param samples Set of samples, 1 per row
	 * @param oclass Output classes. This vector will be resized to have the
	 * same number of rows as samples matrix.
	 */
	size_t classify(const Ref<const MatrixXd> samples, Ref<VectorXi> oclass);

	/**
	 * @brief Updates the classifier with new samples, if reinit is true then
	 * no prior information will be used. If reinit is false then any existing
	 * information will be left intact. In Kmeans that would mean that the
	 * means will be left at their previous state.
	 *
	 * @param samples Samples, S x D matrix with S is the number of samples and
	 * D is the dimensionality. This must match the internal dimension count.
	 * @param reinit whether to reinitialize the  classifier before updating
	 *
	 * @return -1 if maximum number of iterations hit, 0 otherwise
	 */
	int update(const Ref<const MatrixXd> samples, bool reinit = false);

	/**
	 * @brief Returns the current mean matrix
	 *
	 * @return The current mean matrix
	 */
	const MatrixXd& getMeans() { return m_mu; };

private:
	/**
	 * @brief Number of groups to classify samples into
	 */
	size_t m_k;

	/**
	 * @brief The means of each group, K x D, where each row is an N-D mean.
	 */
	MatrixXd m_mu;
};

/**
 * @brief Expectation Maximization With Gaussian Mixture Model
 */
class ExpMax : public Classifier
{
public:
	/**
	 * @brief Constructor for k-means class
	 *
	 * @param rank Number of dimensions in input samples.
	 * @param k Number of groups to classify samples into
	 */
	ExpMax(size_t rank, size_t k = 2);

	/**
	 * @brief Update the number of groups. Note that this invalidates any
	 * current information
	 *
	 * @param ngroups Number of groups to classify
	 */
	void setk(size_t ngroups);

	/**
	 * @brief Sets the mean matrix. Each row of the matrix is a ND-mean, where
	 * N is the number of columns.
	 *
	 * @param newmeans Matrix with new mean, means are stacked so that each row
	 * represents a group mean
	 * @param newmeans Matrix with new coviance matrices. Covariance matrices
	 * are stacked so that row ndim*k gets the first element of the k'th
	 * covance matrix.
	 * @param newcovs the new covariance matrices to set in the classifier
	 * @param tau the prior probaibilities of each of the mixture gaussians
	 */
	void updateMeanCovTau(const Ref<const MatrixXd> newmeans, const Ref<const MatrixXd> newcovs,
			const Ref<const VectorXd> tau);

	/**
	 * @brief Updates the mean/cov/tau coordinates by using the weighted class
	 * estimates (rather than hard classification, like the previous)
	 *
	 * @param samples Matrix of samples, where each row is an ND-sample.
	 * @param prob Probability that each sample is in a given distribution
	 */
	void updateMeanCovTau(const Ref<const MatrixXd> samples, Ref<MatrixXd> prob);

	/**
	 * @brief Given a matrix of samples (Samples x Dims, sample on each row),
	 * apply the classifier to each sample and return a vector of the classes.
	 *
	 * @param samples Set of samples, 1 per row
	 * @param prob Class probability of each sample, for each of the potential
	 * distributions
	 *
	 * @return Change in summed likelihood
	 */
	double expectation(const Ref<const MatrixXd> samples, Ref<MatrixXd> prob);

	/**
	 * @brief Given a matrix of samples (Samples x Dims, sample on each row),
	 * apply the classifier to each sample and return a vector of the classes.
	 *
	 * @param samples Set of samples, 1 per row
	 *
	 * @return Vector of classes, rows match up with input sample rows
	 */
	Eigen::VectorXi classify(const Ref<const MatrixXd> samples);

	/**
	 * @brief Given a matrix of samples (Samples x Dims, sample on each row),
	 * apply the classifier to each sample and return a vector of the classes.
	 *
	 * @param samples Set of samples, 1 per row
	 * @param oclass Output classes. This vector will be resized to have the
	 * same number of rows as samples matrix.
	 */
	size_t classify(const Ref<const MatrixXd> samples, Ref<VectorXi> oclass);

	/**
	 * @brief Updates the classifier with new samples, if reinit is true then
	 * no prior information will be used. If reinit is false then any existing
	 * information will be left intact. In Kmeans that would mean that the
	 * means will be left at their previous state.
	 *
	 * @param samples Samples, S x D matrix with S is the number of samples and
	 * D is the dimensionality. This must match the internal dimension count.
	 * @param reinit whether to reinitialize the  classifier before updating
	 *
	 * return 0 if converged, -1 otherwise
	 */
	int update(const Ref<const MatrixXd> samples, bool reinit = false);


	/**
	 * @brief Returns the current mean matrix
	 *
	 * @return The current mean matrix
	 */
	const MatrixXd& getMeans() { return m_mu; };

	/**
	 * @brief Returns the current mean matrix
	 *
	 * @return The current covariance matrix, with each covariance matrix
	 * stacked on top of the next.
	 */
	const MatrixXd& getCovs() { return m_cov; };
private:
	/**
	 * @brief Number of groups to classify samples into
	 */
	size_t m_k;

	/**
	 * @brief The means of each group, K x D, where each row is an N-D mean.
	 */
	MatrixXd m_mu;

	/**
	 * @brief The covariance of each group, Covariances are stacked vertically,
	 * with covariance c starting at (ndim*c, 0) and running to
	 * (ndim*(c+1)-1, ndim-1).
	 */
	MatrixXd m_cov;

	/**
	 * @brief Inverse of covariance matrix
	 */
	MatrixXd m_covinv;

	/**
	 * @brief Prior probabilities of each distribution based on percent of
	 * points that lie within the group
	 */
	VectorXd m_tau;

	/**
	 * @brief Current log likelihood
	 */
	double m_ll;
};

/**
 * @brief Algorithm of unsupervised learning (clustering) based on density.
 *
 * see "Clustering by fast search and find of density peaks"
 * by Rodriguez, a.  Laio, A.
 *
 * @param samples Samples, S x D matrix with S is the number of samples and
 * D is the dimensionality. This must match the internal dimension count.
 * @param thresh Threshold distance for density calculation
 * @param outthresh threshold for outlier, ratio of standard devation. Should
 * be > 2 because you want to be well outside the center of the distribution.
 * @param classes Output classes
 * @param brute whether ther use slower brute force method for density
 * calculation (for testing purposes only)
 *
 * return -1 if maximum number of iterations hit, 0 otherwise (converged)
 */
int fastSearchFindDP(const Eigen::MatrixXf& samples, double thresh,
		double outthresh, Eigen::VectorXi& classes, bool brute = false);

/**
 * @brief Computes Density and Peak computation for Fast Search and Find of
 * Density Peaks algorithm.
 *
 * @param samples Samples, S x D matrix with S is the number of samples and
 * D is the dimensionality. This must match the internal dimension count.
 * @param thresh Threshold for density calculation
 * @param rho Point densities
 * @param delta Distance to nearest peak
 * @param parent Index (point) that is the nearest peak
 *
 * @return 0 if successful
 */
int findDensityPeaks(const Eigen::MatrixXf& samples, double thresh,
		Eigen::VectorXf& rho, Eigen::VectorXf& delta,
		Eigen::VectorXi& parent);

/**
 * @brief Computes Density and Peak computation for Fast Search and Find of
 * Density Peaks algorithm. This is a slower, non-bin based version
 *
 * @param samples Samples, S x D matrix with S is the number of samples and
 * D is the dimensionality. This must match the internal dimension count.
 * @param thresh Threshold for density calculation
 * @param rho Point densities
 * @param delta Distance to nearest peak
 * @param parent Index (point) that is the nearest peak
 *
 * @return 0 if successful
 */
int findDensityPeaks_brute(const Eigen::MatrixXf& samples, double thresh,
		Eigen::VectorXf& rho, Eigen::VectorXf& delta,
		Eigen::VectorXi& parent);

/** @} */

/** @} */

}

#endif // STATISTICS_H
