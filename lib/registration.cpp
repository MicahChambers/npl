/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * NPL is free software: you can redistribute it and/or modify it under the
 * terms of the BSD 2-Clause License available in LICENSE or at
 * http://opensource.org/licenses/BSD-2-Clause
 *
 * @file registration.h Tools for registering 2D or 3D images.
 *
 *****************************************************************************/

#include "registration.h"
#include "lbfgs.h"
#include "statistics.h"
#include "mrimage_utils.h"
#include "ndarray_utils.h"
#include "macros.h"

#include <memory>
#include <stdexcept>
#include <functional>
#include <algorithm>

#include <Eigen/Dense>
#include <Eigen/Geometry>

using Eigen::VectorXd;
using Eigen::Vector3d;
using Eigen::Matrix3d;
using Eigen::Matrix4d;
using Eigen::AngleAxisd;

using std::cerr;
using std::endl;

//#define CLOCK(S) S
#define CLOCK(S)


#ifdef VERYDEBUG
#define DEBUGWRITE(FOO) FOO
#else
#define DEBUGWRITE(FOO)
#endif

namespace npl {

/******************************************************************
 * Registration Functions
 *****************************************************************/
/**
 * @brief Performs correlation based registration between two 3D volumes. note
 * that the two volumes should have identical sampling and identical
 * orientation. If that is not the case, an exception will be thrown.
 *
 * \todo make it v = Ru + s, then u = INV(R)*(v - s)
 *
 * @param fixed     Image which will be the target of registration.
 * @param moving    Image which will be rotated then shifted to match fixed.
 * @param sigmas	Standard deviation of smoothing at each level
 *
 * @return Output rigid transform
 */
Rigid3DTrans corReg3D(shared_ptr<const MRImage> fixed,
		shared_ptr<const MRImage> moving, const std::vector<double>& sigmas)
{
	using namespace std::placeholders;
	using std::bind;

	// make sure the input image has matching properties
	if(!fixed->matchingOrient(moving, true, true))
		throw INVALID_ARGUMENT("Input images have mismatching pixels in");

	// create value and gradient functions from RigiCorrComputer
	RigidCorrComp comp(true);
	auto vfunc = bind(&RigidCorrComp::value, &comp, _1, _2);
	auto vgfunc = bind(&RigidCorrComp::valueGrad, &comp, _1, _2, _3);
	auto gfunc = bind(&RigidCorrComp::grad, &comp, _1, _2);

	Rigid3DTrans rigid;
	for(size_t ii=0; ii<sigmas.size(); ii++) {
		// smooth and downsample input images
		auto sm_fixed = smoothDownsample(fixed, sigmas[ii]);
		auto sm_moving = smoothDownsample(moving, sigmas[ii]);
		DEBUGWRITE(sm_fixed->write("smooth_fixed_"+to_string(ii)+".nii.gz"));
		DEBUGWRITE(sm_moving->write("smooth_moving_"+to_string(ii)+".nii.gz"));
		comp.setFixed(sm_fixed);
		comp.setMoving(sm_moving);

		// initialize optimizer
		LBFGSOpt opt(6, vfunc, gfunc, vgfunc);
		opt.stop_Its = 10000;
		opt.stop_X = 0.00001;
		opt.stop_G = 0;
		opt.stop_F = 0;

		// grab the parameters from the previous iteration (or initialized)
		rigid.toIndexCoords(sm_moving, true);
		for(size_t ii=0; ii<3; ii++) {
			opt.state_x[ii] = radToDeg(rigid.rotation[ii]);
			opt.state_x[ii+3] = rigid.shift[ii]*sm_moving->spacing(ii);
			assert(rigid.center[ii] == (sm_moving->dim(ii)-1.)/2.);
		}

		// run the optimizer
		StopReason stopr = opt.optimize();
		cerr << Optimizer::explainStop(stopr) << endl;

		// set values from parameters, and convert to RAS coordinate so that no
	// matter the sampling after smoothing the values remain
		for(size_t ii=0; ii<3; ii++) {
			rigid.rotation[ii] = degToRad(opt.state_x[ii]);
			rigid.shift[ii] = opt.state_x[ii+3]/sm_moving->spacing(ii);
			rigid.center[ii] = (sm_moving->dim(ii)-1)/2.;
		}

		rigid.toRASCoords(sm_moving);
	}

	return rigid;
};

/**
 * @brief Performs correlation based registration between two 3D volumes. note
 * that the two volumes should have identical sampling and identical
 * orientation. If that is not the case, an exception will be thrown.
 *
 * \todo make it v = Ru + s, then u = INV(R)*(v - s)
 *
 * @param fixed     Image which will be the target of registration.
 * @param moving    Image which will be rotated then shifted to match fixed.
 * @param sigmas array of standard deviations to use
 * @param nbins number of bins for estimation of marginal pdf's
 * @param binrarius Radius of kernel in pdf density estimation
 * @param metric metric to use (default is MI)
 * @param stopx Smallest step to take, stop after steps reach this size
 *
 * @return          4x4 Matrix, indicating rotation about the center then
 *                  shift. Rotation matrix is the first 3x3 and shift is the
 *                  4th column.
 */
Rigid3DTrans informationReg3D(shared_ptr<const MRImage> fixed,
		shared_ptr<const MRImage> moving, const std::vector<double>& sigmas,
		size_t nbins, size_t binradius, string metric, double stopx)
{
	using namespace std::placeholders;
	using std::bind;

	Rigid3DTrans rigid;

	// make sure the input image has matching properties
	if(!fixed->matchingOrient(moving, true, true))
		throw INVALID_ARGUMENT("Input images have mismatching pixels in");

	for(size_t ii=0; ii<sigmas.size(); ii++) {
		// smooth and downsample input images
		auto sm_fixed = smoothDownsample(fixed, sigmas[ii]);
		auto sm_moving = smoothDownsample(moving, sigmas[ii]);
		DEBUGWRITE(sm_fixed->write("smooth_fixed_"+to_string(ii)+".nii.gz"));
		DEBUGWRITE(sm_moving->write("smooth_moving_"+to_string(ii)+".nii.gz"));

		RigidInfoComp comp(true);
		comp.setBins(nbins, binradius);
		comp.setFixed(sm_fixed);
		comp.setMoving(sm_moving);

		if(metric == "MI")
			comp.m_metric = METRIC_MI;
		else if(metric == "NMI")
			comp.m_metric = METRIC_NMI;
		else if(metric == "VI") {
			comp.m_metric = METRIC_VI;
		}

		// create value and gradient functions
		auto vfunc = bind(&RigidInfoComp::value, &comp, _1, _2);
		auto vgfunc = bind(&RigidInfoComp::valueGrad, &comp, _1, _2, _3);
		auto gfunc = bind(&RigidInfoComp::grad, &comp, _1, _2);

		// initialize optimizer
		LBFGSOpt opt(6, vfunc, gfunc, vgfunc);
		opt.stop_Its = 10000;
		opt.stop_X = stopx;
		opt.stop_G = 0;
		opt.stop_F = 0;

		cerr << "Init Rigid: " << ii << endl;
		cerr << "Rotation: " << rigid.rotation.transpose() << endl;
		cerr << "Center : " << rigid.center.transpose() << endl;
		cerr << "Shift : " << rigid.shift.transpose() << endl;

		// grab the parameters from the previous iteration (or initialized)
		rigid.toIndexCoords(sm_moving, true);
		for(size_t ii=0; ii<3; ii++) {
			opt.state_x[ii] = radToDeg(rigid.rotation[ii]);
			opt.state_x[ii+3] = rigid.shift[ii]*sm_moving->spacing(ii);
			assert(rigid.center[ii] == (sm_moving->dim(ii)-1.)/2.);
		}

		cerr << "Init Rigid (Index Coord): " << ii << endl;
		cerr << "Rotation: " << rigid.rotation.transpose() << endl;
		cerr << "Center : " << rigid.center.transpose() << endl;
		cerr << "Shift : " << rigid.shift.transpose() << endl;

		// run the optimizer
		StopReason stopr = opt.optimize();
		cerr << Optimizer::explainStop(stopr) << endl;

		// set values from parameters, and convert to RAS coordinate so that no
		// matter the sampling after smoothing the values remain
		for(size_t ii=0; ii<3; ii++) {
			rigid.rotation[ii] = degToRad(opt.state_x[ii]);
			rigid.shift[ii] = opt.state_x[ii+3]/sm_moving->spacing(ii);
			rigid.center[ii] = (sm_moving->dim(ii)-1)/2.;
		}

		cerr << "Finished Rigid (Index Coord): " << ii << endl;
		cerr << "Rotation: " << rigid.rotation.transpose() << endl;
		cerr << "Center : " << rigid.center.transpose() << endl;
		cerr << "Shift : " << rigid.shift.transpose() << endl;

		rigid.toRASCoords(sm_moving);

		cerr << "Finished Rigid (RAS Coord): " << ii << endl;
		cerr << "Rotation: " << rigid.rotation.transpose() << endl;
		cerr << "Center : " << rigid.center.transpose() << endl;
		cerr << "Shift : " << rigid.shift.transpose() << endl;
	}

	return rigid;
};

/**
 * @brief Information based registration between two 3D volumes. note
 * that the two volumes should have identical sampling and identical
 * orientation.
 *
 * @param fixed Image which will be the target of registration.
 * @param moving Image which will be rotated then shifted to match fixed.
 * @param otsu perform otsu thresholding of the input images
 * @param dir direction/dimension of distortion
 * @param bspace Spacing of B-Spline knots (in mm)
 * @param jac jacobian regularizer weight
 * @param tps thin-plate-spline regularizer weight
 * @param sigmas Standard deviation of smoothing at each level
 * @param nbins Number of bins in marginal PDF
 * @param binradius radius of parzen window, to smooth pdf
 * @param metric Type of information based metric to use
 * @param hist length of history to keep
 * @param stopx Minimum step size before stopping
 * @param beta Fraction of previous step size to consider for next step size.
 * Essentially this decides how quickly to reduce step sizes (note 1 would mean
 * NO reduction and would continue forever, 0 would do 1 step then stop)
 *
 * @return parameters of bspline
 */
ptr<MRImage> infoDistCor(ptr<const MRImage> infixed, ptr<const MRImage> inmoving,
		bool otsu, int dir, double bspace, double jac, double tps,
		const std::vector<double>& sigmas,
		size_t nbins, size_t binradius, string metric,
		size_t hist, double stopx, double beta)
{
	using namespace std::placeholders;
	using std::bind;
	size_t pp;

	// make sure the input image has matching properties
	if(!infixed->matchingOrient(inmoving, true, true))
		throw INVALID_ARGUMENT("Input images have mismatching pixels in");

	// Create Distortion Correction Computer
	DistCorrInfoComp comp(true);
	comp.setBins(nbins, binradius);

	if(metric == "MI")
		comp.m_metric = METRIC_MI;
	else if(metric == "NMI")
		comp.m_metric = METRIC_NMI;
	else if(metric == "VI") {
		comp.m_metric = METRIC_VI;
	}

	// CREATE MASK

	ptr<MRImage> mmask, fmask;
	ptr<MRImage> moving, fixed;
	if(otsu) {
		double mthresh = otsuThresh(inmoving);
		mmask = dPtrCast<MRImage>(binarize(inmoving, mthresh));
		moving = dPtrCast<MRImage>(threshold(inmoving, mthresh));

		double fthresh = otsuThresh(infixed);
		fmask = dPtrCast<MRImage>(binarize(infixed, fthresh));
		fixed = dPtrCast<MRImage>(threshold(infixed, fthresh));
	} else {
		fixed = dPtrCast<MRImage>(infixed->copy());
		moving = dPtrCast<MRImage>(inmoving->copy());
	}

	comp.m_jac_reg = jac;
	comp.m_tps_reg = tps;

	// Need to provide gridding for bspline image
	comp.setFixed(fixed);
	comp.initializeKnots(bspace);

	// create value and gradient functions
	auto vfunc = bind(&DistCorrInfoComp::value, &comp, _1, _2);
	auto vgfunc = bind(&DistCorrInfoComp::valueGrad, &comp, _1, _2, _3);
	auto gfunc = bind(&DistCorrInfoComp::grad, &comp, _1, _2);

	for(size_t ii=0; ii<sigmas.size(); ii++) {
		// smooth and downsample input images
		cerr << "Sigma: " << sigmas[ii] << endl;
		auto sm_fixed = smoothDownsample(fixed, sigmas[ii], sigmas[ii]*2.355);
		auto sm_moving = smoothDownsample(moving, sigmas[ii], sigmas[ii]*2.355);

		// Threshold
		if(otsu) {
			auto sm_mmask = smoothDownsample(mmask, sigmas[ii], sigmas[ii]*2.355);
			auto sm_fmask = smoothDownsample(fmask, sigmas[ii], sigmas[ii]*2.355);
			FlatIter<double> mmaskit(sm_mmask);
			FlatIter<double> fmaskit(sm_fmask);
			FlatIter<double> mit(sm_moving);
			FlatIter<double> fit(sm_fixed);
			for(; !mit.eof(); ++mmaskit, ++fmaskit, ++mit, ++fit) {
				if(mmaskit.get() < 0.01)
					mit.set(0);
				if(fmaskit.get() < 0.01)
					fit.set(0);
			}
		}

		comp.setFixed(sm_fixed);
		comp.setMoving(sm_moving, dir);

		// initialize optimizer
		LBFGSOpt opt(comp.nparam(), vfunc, gfunc, vgfunc);
		opt.stop_Its = 10000;
		opt.stop_G = 0;
		opt.stop_F = 0;
		opt.stop_X = stopx;
		opt.opt_histsize = hist;
		opt.opt_ls_beta = beta;

		// grab the parameters from the previous iteration (or initialized)
		pp = 0;
		for(FlatConstIter<double> pit(comp.getDeform()); !pit.eof(); ++pit, ++pp)
			opt.state_x[pp] = *pit;

		// run the optimizer
		StopReason stopr = opt.optimize();
		cerr << Optimizer::explainStop(stopr) << endl;

		// set values from parameters, and convert to RAS coordinate so that no
		// matter the sampling after smoothing the values remain
		pp = 0;
		for(FlatIter<double> pit(comp.getDeform()); !pit.eof(); ++pit, ++pp)
			pit.set(opt.state_x[pp]);

	}

	return comp.getDeform();
};

/*****************************************************************************
 * Derivative Testers
 ****************************************************************************/

/**
 * @brief This function checks the validity of the derivative functions used
 * to optimize between-image corrlation.
 *
 * @param step Test step size
 * @param tol Tolerance in error between analytical and Numeric gratient
 * @param in1 Image 1
 * @param in2 Image 2
 *
 * @return 0 if success, -1 if failure
 */
int cor3DDerivTest(double step, double tol,
		shared_ptr<const MRImage> in1, shared_ptr<const MRImage> in2)
{
	using namespace std::placeholders;
	RigidCorrComp comp(false);
	comp.setFixed(in1);
	comp.setMoving(in2);

	auto vfunc = std::bind(&RigidCorrComp::value, &comp, _1, _2);
	auto vgfunc = std::bind(&RigidCorrComp::valueGrad, &comp, _1, _2, _3);

	double error = 0;
	VectorXd x = VectorXd::Ones(6);
	if(testgrad(error, x, step, tol, vfunc, vgfunc) != 0) {
		return -1;
	}

	return 0;
}

/**
 * @brief This function checks the validity of the derivative functions used
 * to optimize between-image corrlation.
 *
 * @param step Test step size
 * @param tol Tolerance in error between analytical and Numeric gratient
 * @param in1 Image 1
 * @param in2 Image 2
 *
 * @return 0 if success, -1 if failure
 */
int information3DDerivTest(double step, double tol,
		shared_ptr<const MRImage> in1, shared_ptr<const MRImage> in2)
{
	using namespace std::placeholders;
	RigidInfoComp comp(false);
	comp.setBins(128,4);
	comp.m_metric = METRIC_MI;
	comp.setFixed(in1);
	comp.setMoving(in2);

	auto vfunc = std::bind(&RigidInfoComp::value, &comp, _1, _2);
	auto vgfunc = std::bind(&RigidInfoComp::valueGrad, &comp, _1, _2, _3);

	double error = 0;
	VectorXd x = VectorXd::Ones(6);
	if(testgrad(error, x, step, tol, vfunc, vgfunc) != 0) {
		return -1;
	}

	return 0;
}

/**
 * @brief This function checks the validity of the derivative functions used
 * to optimize between-image corrlation.
 *
 * @param step Test step size
 * @param tol Tolerance in error between analytical and Numeric gratient
 * @param in1 Image 1
 * @param in2 Image 2
 * @param regj Jackobian weight
 * @param regt Thin-plate-spline weight
 *
 * @return 0 if success, -1 if failure
 */
int distcorProbDerivTest(double step, double tol,
		shared_ptr<const MRImage> in1, shared_ptr<const MRImage> in2,
		double regj, double regt)
{
	using namespace std::placeholders;
	ProbDistCorrInfoComp comp(false);
	comp.initialize(in1, in2, 8, 256, 20, 1);
	comp.m_metric = METRIC_MI;
	comp.m_jac_reg = regj;
	comp.m_tps_reg = regt;

	auto vfunc = std::bind(&ProbDistCorrInfoComp::value, &comp, _1, _2);
	auto vgfunc = std::bind(&ProbDistCorrInfoComp::valueGrad, &comp, _1, _2, _3);

	double error = 0;
	VectorXd x(comp.nparam());
	for(size_t ii=0; ii<x.rows(); ii++)
		x[ii] = 10*(rand()/(double)RAND_MAX-0.5);

	if(testgrad(error, x, step, tol, vfunc, vgfunc) != 0) {
		return -1;
	}

	return 0;
}


/**
 * @brief This function checks the validity of the derivative functions used
 * to optimize between-image corrlation.
 *
 * @param step Test step size
 * @param tol Tolerance in error between analytical and Numeric gratient
 * @param in1 Image 1
 * @param in2 Image 2
 * @param regj Jackobian weight
 * @param regt Thin-plate-spline weight
 *
 * @return 0 if success, -1 if failure
 */
int distcorDerivTest(double step, double tol,
		shared_ptr<const MRImage> in1, shared_ptr<const MRImage> in2,
		double regj, double regt)
{
	using namespace std::placeholders;
	DistCorrInfoComp comp(false);
	comp.setBins(256,8);
	comp.m_metric = METRIC_MI;
	comp.setFixed(in1);
	comp.setMoving(in2);
	comp.initializeKnots(20);
	comp.m_jac_reg = regj;
	comp.m_tps_reg = regt;

	auto vfunc = std::bind(&DistCorrInfoComp::value, &comp, _1, _2);
	auto vgfunc = std::bind(&DistCorrInfoComp::valueGrad, &comp, _1, _2, _3);

	double error = 0;
	VectorXd x(comp.nparam());
	for(size_t ii=0; ii<x.rows(); ii++)
		x[ii] = 10*(rand()/(double)RAND_MAX-0.5);

	if(testgrad(error, x, step, tol, vfunc, vgfunc) != 0) {
		return -1;
	}

	return 0;
}

/*********************************************************************
 * Registration Class Implementations
 ********************************************************************/

/*********************
 * Correlation
 ********************/

/**
 * @brief Constructor for the rigid correlation class. Note that
 * rigid rotation is assumed to be about the center of the fixed
 * image space. If you make any changes to the internal images call
 * reinit() to reinitialize the derivative.
 *
 * @param fixed Fixed image. A copy of this will be made.
 * @param moving Moving image. A copy of this will be made.
 * @param compdiff Negate correlation to compute a difference measure
 */
RigidCorrComp::RigidCorrComp(bool compdiff) : m_compdiff(compdiff)
{
}

/**
 * @brief Computes the gradient and value of the correlation.
 *
 * The basic gyst of this function is to compute dC/dP where p is a parameter.
 * dC/dP = f*dg/dP = f*dg/dx*dx/dP
 * where x is a 3d coordinate so that dg/dx is the gradient of the image
 * and dx/dP is the jacobian of the coordinate system with respect to a
 * change in parameter. For shift dx/dP is a diagonal matrix, for rotation
 * dx/dR = derivative of rotation matrix wrt to a paremeter * (x-c) where
 * x-c is the vector from the center of the image to the coodinate
 *
 * @param x Paramters (Rx, Ry, Rz, Sx, Sy, Sz).
 * @param v Value at the given rotation
 * @param g Gradient at the given rotation
 *
 * @return 0 if successful
 */
int RigidCorrComp::valueGrad(const VectorXd& params,
		double& val, VectorXd& grad)
{
	if(!m_fixed) throw INVALID_ARGUMENT("ERROR must set fixed image before "
				"computing value.");
	if(!m_moving) throw INVALID_ARGUMENT("ERROR must set moving image before "
				"computing value.");
	if(!m_moving->matchingOrient(m_fixed, true, true)) {
		throw INVALID_ARGUMENT("Moving and Fixed Images must have the same "
				"orientation and gred!");
	}

	// Degrees -> Radians, mm -> index
	double rx = params[0]*M_PI/180.;
	double ry = params[1]*M_PI/180.;
	double rz = params[2]*M_PI/180.;
	double sx = params[3]/m_moving->spacing(0);
	double sy = params[4]/m_moving->spacing(1);
	double sz = params[5]/m_moving->spacing(2);

//#if defined DEBUG || defined VERYDEBUG
	cerr << "VALGRAD Rotation: " << rx << ", " << ry << ", " << rz << ", Shift: "
		<< sx << ", " << sy << ", " << sz << endl;
//#endif

	// Compute derivative Images (if debugging is enabled, otherwise just
	// compute the gradient)
	grad.setZero();
	size_t count = 0;
	double mov_sum = 0;
	double fix_sum = 0;
	double mov_ss = 0;
	double fix_ss = 0;
	double corr = 0;

	// Update Transform Matrix
	Matrix3d Rinv;
	Rinv(0, 0) = cos(ry)*cos(rz);;
	Rinv(0, 1) = cos(rz)*sin(rx)*sin(ry)+cos(rx)*sin(rz);
	Rinv(0, 2) = -cos(rx)*cos(rz)*sin(ry)+sin(rx)*sin(rz);
	Rinv(1, 0) = -cos(ry)*sin(rz);
	Rinv(1, 1) = cos(rx)*cos(rz)-sin(rx)*sin(ry)*sin(rz);
	Rinv(1, 2) = cos(rz)*sin(rx)+cos(rx)*sin(ry)*sin(rz);
	Rinv(2, 0) = sin(ry);
	Rinv(2, 1) = -cos(ry)*sin(rx);
	Rinv(2, 2) = cos(rx)*cos(ry);

	Vector3d ind;
	Vector3d cind;
	Vector3d shift(sx, sy, sz);
	Eigen::Map<Vector3d> center(m_center);

	// dRigid/dRx, dRigid/dRy, dRigid/dRz = ddRx*(u-c), ddRy*(u-c) ...
	Matrix3d ddRx, ddRy, ddRz;
	ddRx <<0,0,0,cos(rx)*cos(rz)*sin(ry)-sin(rx)*sin(rz),
		-(cos(rz)*sin(rx))-cos(rx)*sin(ry)*sin(rz),
		-(cos(rx)*cos(ry)),cos(rz)*sin(rx)*sin(ry)+cos(rx)*sin(rz),
		cos(rx)*cos(rz)-sin(rx)*sin(ry)*sin(rz),-(cos(ry)*sin(rx));
	ddRy <<-(cos(rz)*sin(ry)),sin(ry)*sin(rz),cos(ry),cos(ry)*cos(rz)*sin(rx),
		-(cos(ry)*sin(rx)*sin(rz)),sin(rx)*sin(ry),-(cos(rx)*cos(ry)*cos(rz)),
		cos(rx)*cos(ry)*sin(rz),-(cos(rx)*sin(ry));
	ddRz <<-(cos(ry)*sin(rz)),-(cos(ry)*cos(rz)),0,
		cos(rx)*cos(rz)-sin(rx)*sin(ry)*sin(rz),
		-(cos(rz)*sin(rx)*sin(ry))-cos(rx)*sin(rz),0,
		cos(rz)*sin(rx)+cos(rx)*sin(ry)*sin(rz),
		cos(rx)*cos(rz)*sin(ry)-sin(rx)*sin(rz),0;

	Vector3d gradG; //dg/dx, dg/dy, dg/dz
	Matrix3d dR; //dg/dRx, dg/dRy, dg/dRz
	Vector3d dgdR;

	NDConstIter<double> mit(m_moving);
	Vector3DConstIter<double> dmit(m_dmoving);
	LinInterp3DView<double> fix_vw(m_fixed);
	for(; !mit.eof() && !dmit.eof(); ++mit, ++dmit) {

		mit.index(3, ind.array().data());

		// u = c + R^-1(v - s - c)
		// where u is the output index, v the input, c the center of rotation
		// and s the shift
		// cind = center + rInv*(ind-shift-center);
		cind = Rinv*(ind-shift-center) + center;

		// Here we compute dg(v(u,p))/dp, where g is the image, u is the
		// coordinate in the fixed image, and p is the param.
		// dg/dp = SUM_i dg/dv_i dv_i/dp, where v is the rotated coordinate, so
		// dg/dv_i is the directional derivative in original space,
		// dv_i/dp is the derivative of the rotated coordinate system with
		// respect to a parameter
		gradG[0] = dmit[0];
		gradG[1] = dmit[1];
		gradG[2] = dmit[2];

		dR.row(0) = ddRx*(cind-center);
		dR.row(1) = ddRy*(cind-center);
		dR.row(2) = ddRz*(cind-center);

		// compute SUM_i dg/dv_i dv_i/dp
		dgdR = dR*gradG;

		// compute correlation, since it requires almost no additional work
		double g = *mit;
		double f = fix_vw(cind[0], cind[1], cind[2]);

		mov_sum += g;
		fix_sum += f;
		mov_ss += g*g;
		fix_ss += f*f;
		corr += g*f;

		grad[0] += f*dgdR[0];
		grad[1] += f*dgdR[1];
		grad[2] += f*dgdR[2];
		grad[3] += f*gradG[0];
		grad[4] += f*gradG[1];
		grad[5] += f*gradG[2];

	}

	// Radians -> Degrees, index to mm
	grad[0] *= M_PI/180.;
	grad[1] *= M_PI/180.;
	grad[2] *= M_PI/180.;
	grad[3] /= m_moving->spacing(0);
	grad[4] /= m_moving->spacing(1);
	grad[5] /= m_moving->spacing(2);

	count = m_moving->elements();
	val = sample_corr(count, mov_sum, fix_sum, mov_ss, fix_ss, corr);
	double sd1 = sqrt(sample_var(count, mov_sum, mov_ss));
	double sd2 = sqrt(sample_var(count, fix_sum, fix_ss));
	grad /= (count-1)*sd1*sd2;

//#if defined VERYDEBUG || defined DEBUG
	cerr << "Value: " << val << endl;
	cerr << "Gradient: " << grad.transpose() << endl;
//#endif

	if(m_compdiff) {
		grad = -grad;
		val = -val;
	}

	return 0;
}

/**
 * @brief Computes the gradient of the correlation. Note that this
 * function just calls valueGrad because computing the
 * additional values are trivial
 *
 * @param params Paramters (Rx, Ry, Rz, Sx, Sy, Sz).
 * @param grad Gradient at the given rotation
 *
 * @return 0 if successful
 */
int RigidCorrComp::grad(const VectorXd& params, VectorXd& grad)
{
	double v = 0;
	return valueGrad(params, v, grad);
}

/**
 * @brief Computes the correlation.
 *
 * @param params Paramters (Rx, Ry, Rz, Sx, Sy, Sz).
 * @param val Value at the given rotation
 *
 * @return 0 if successful
 */
int RigidCorrComp::value(const VectorXd& params, double& val)
{
	if(!m_fixed) throw INVALID_ARGUMENT("ERROR must set fixed image before "
				"computing value.");
	if(!m_moving) throw INVALID_ARGUMENT("ERROR must set moving image before "
				"computing value.");
	if(!m_moving->matchingOrient(m_fixed, true, true)) {
		throw INVALID_ARGUMENT("Moving and Fixed Images must have the same "
				"orientation and gred!");
	}

	assert(m_fixed->ndim() == 3);
	assert(m_moving->ndim() == 3);

	// Degrees -> Radians, mm -> index
	double rx = params[0]*M_PI/180.;
	double ry = params[1]*M_PI/180.;
	double rz = params[2]*M_PI/180.;
	double sx = params[3]/m_moving->spacing(0);
	double sy = params[4]/m_moving->spacing(1);
	double sz = params[5]/m_moving->spacing(2);

	//#if defined DEBUG || defined VERYDEBUG
	cerr << "VAL() Rotation: " << rx << ", " << ry << ", " << rz << ", Shift: "
		<< sx << ", " << sy << ", " << sz << endl;
	//#endif
	Vector3d shift(sx, sy, sz);

	// Update Transform Matrix
	Matrix3d Rinv;
	Rinv(0, 0) = cos(ry)*cos(rz);;
	Rinv(0, 1) = cos(rz)*sin(rx)*sin(ry)+cos(rx)*sin(rz);
	Rinv(0, 2) = -cos(rx)*cos(rz)*sin(ry)+sin(rx)*sin(rz);
	Rinv(1, 0) = -cos(ry)*sin(rz);
	Rinv(1, 1) = cos(rx)*cos(rz)-sin(rx)*sin(ry)*sin(rz);
	Rinv(1, 2) = cos(rz)*sin(rx)+cos(rx)*sin(ry)*sin(rz);
	Rinv(2, 0) = sin(ry);
	Rinv(2, 1) = -cos(ry)*sin(rx);
	Rinv(2, 2) = cos(rx)*cos(ry);

	Eigen::Map<Vector3d> center(m_center);

	Vector3d ind;
	Vector3d cind;

	//  Resample Output and Compute Orientation. While actually resampling is
	//  optional, it helps with debugging
	double sum1 = 0;
	double sum2 = 0;
	double ss1 = 0;
	double ss2 = 0;
	double corr = 0;
	LinInterp3DView<double> fix_vw(m_fixed);
	Vector3DConstIter<double> mit(m_moving);
	for(; !mit.eof(); ++mit) {
		mit.index(3, ind.array().data());

		// u = c + R^-1(v - s - c)
		// where u is the output index, v the input, c the center of rotation
		// and s the shift
		cind = center + Rinv*(ind-center-shift);

		double a = mit[0];
		double b = fix_vw(cind[0], cind[1], cind[2]);
		sum1 += a;
		ss1 += a*a;
		sum2 += b;
		ss2 += b*b;
		corr += a*b;
	}

	val = sample_corr(m_moving->elements(), sum1, sum2, ss1, ss2, corr);

//#if defined VERYDEBUG || defined DEBUG
	cerr << "Value: " << val << endl;
//#endif

	if(m_compdiff)
		val = -val;
	return 0;
};

/**
 * @brief Set the fixed image for registration/comparison
 *
 * @param fixed Input fixed image (not modified)
 */
void RigidCorrComp::setFixed(ptr<const MRImage> newfix)
{
	if(newfix->ndim() != 3)
		throw INVALID_ARGUMENT("Fixed image is not 3D!");
	if(!newfix->isIsotropic(true, 0.1))
		throw INVALID_ARGUMENT("Fixed image is not isotropic!");

	m_fixed = newfix;
};

/**
 * @brief Set the moving image for comparison, note that setting this
 * triggers a derivative computation and so is slower than setFixed.
 *
 * Note that modification of the moving image outside this class without
 * re-calling setMoving is undefined and will result in an out-of-date
 * moving image derivative. THIS WILL BREAK GRADIENT CALCULATIONS.
 *
 * @param moving Input moving image (not modified)
 */
void RigidCorrComp::setMoving(ptr<const MRImage> newmove)
{
	if(newmove->ndim() != 3)
		throw INVALID_ARGUMENT("Moving image is not 3D!");
	if(!newmove->isIsotropic(true, 0.1))
		throw INVALID_ARGUMENT("Moving image is not isotropic!");

	m_moving = newmove;
	m_dmoving = dPtrCast<MRImage>(derivative(m_moving));

	for(size_t ii=0; ii<3 && ii<m_moving->ndim(); ii++)
		m_center[ii] = (m_moving->dim(ii)-1)/2.;
}

/****************************************************************************
 * Mutual Information/Normalize Mutual Information/Variation of Information
 ****************************************************************************/

/**
 * @brief Constructor for the rigid correlation class. Note that
 * rigid rotation is assumed to be about the center of the fixed
 * image space. If necessary the input moving image will be resampled.
 * To the same space as the fixed image.
 *
 * @param fixed Fixed image. A copy of this will be made.
 * @param moving Moving image. A copy of this will be made.
 * @param compdiff negate MI and NMI to create an effective distance
 */
RigidInfoComp::RigidInfoComp(bool compdiff) :
	m_compdiff(compdiff), m_metric(METRIC_MI), m_gradHjoint(6), m_gradHmove(6)
{
	setBins(128, 4);
}

/**
 * @brief Reallocates histograms and if m_fixed has been set, regenerates
 * histogram estimate of fixed pdf
 *
 * @param nbins Number of bins for marginal estimation
 * @param krad Number of bins in kernel radius
 */
void RigidInfoComp::setBins(size_t nbins, size_t krad)
{
	// set number of bins
	m_bins = nbins;
	m_krad = krad;

	// reallocate memory
	m_pdfmove.resize({nbins});
	m_pdffix.resize({nbins});
	m_pdfjoint.resize({nbins, nbins});
	m_dpdfjoint.resize({6, nbins, nbins});
	m_dpdfmove.resize({6, nbins});

	if(m_fixed) {
		// if fixed hsa been set, recompute fixed entropy
		setFixed(m_fixed);
	}
}

/**
 * @brief Sets the moving image for Information-Based comparison. This sets
 * m_moving, m_move_get, d_moving, m_dmove_get and m_rangemove
 *
 * @param newmove New moving image
 */
void RigidInfoComp::setMoving(ptr<const MRImage> newmove)
{
	if(newmove->ndim() != 3)
		throw INVALID_ARGUMENT("Moving image is not 3D!");
	if(!newmove->isIsotropic(true, 0.1))
		throw INVALID_ARGUMENT("Moving image is not isotropic!");

	//////////////////////////////////////
	// Setup accessors and Members Images
	//////////////////////////////////////
	m_moving = newmove;
	m_dmoving = dPtrCast<MRImage>(derivative(m_moving));

	m_move_get.setArray(m_moving);
	m_dmove_get.setArray(m_dmoving);
	for(size_t ii=0; ii<3 && ii<m_moving->ndim(); ii++)
		m_center[ii] = (m_moving->dim(ii)-1)/2.;

	// must include 0 because outside values get mapped to 0
	m_rangemove[0] = 0;
	m_rangemove[1] = 0;
	for(NDConstIter<double> it(m_moving); !it.eof(); ++it) {
		m_rangemove[0] = std::min(m_rangemove[0], *it);
		m_rangemove[1] = std::max(m_rangemove[1], *it);
	}
}

/**
 * @brief Sets the fixed image for Information-Based comparison. This sets
 * m_fixed, m_fit, m_rangefix, m_wfix, fills the histogram m_pdffix, and
 * computes m_Hfix
 *
 * @param newfixed New fixed image
 */
void RigidInfoComp::setFixed(ptr<const MRImage> newfixed)
{
	if(newfixed->ndim() != 3)
		throw INVALID_ARGUMENT("Fixed image is not 3D!");
	if(!newfixed->isIsotropic(true, 0.1))
		throw INVALID_ARGUMENT("Fixed image is not isotropic!");

	//////////////////////////////////////
	// Set up Members
	//////////////////////////////////////
	m_fixed = newfixed;
	m_fit.setArray(m_fixed);

	//////////////////////////////////////
	// compute ranges, and bin widths
	//////////////////////////////////////
	m_rangefix[0] = INFINITY;
	m_rangefix[1] = -INFINITY;
	for(NDConstIter<double> it(m_fixed); !it.eof(); ++it) {
		m_rangefix[0] = std::min(m_rangefix[0], *it);
		m_rangefix[1] = std::max(m_rangefix[1], *it);
	}
}

/**
 * @brief Computes the gradient and value of the correlation.
 *
 * @param x Paramters (Rx, Ry, Rz, Sx, Sy, Sz).
 * @param v Value at the given rotation
 * @param g Gradient at the given rotation
 *
 * @return 0 if successful
 */
int RigidInfoComp::valueGrad(const VectorXd& params,
		double& val, VectorXd& grad)
{
	if(!m_fixed) throw INVALID_ARGUMENT("ERROR must set fixed image before "
				"computing value.");
	if(!m_moving) throw INVALID_ARGUMENT("ERROR must set moving image before "
				"computing value.");
	if(!m_moving->matchingOrient(m_fixed, true, true)) {
		throw INVALID_ARGUMENT("Moving and Fixed Images must have the same "
				"orientation and gred!");
	}

	assert(m_pdfmove.dim(0) == m_bins);
	assert(m_pdffix.dim(0) == m_bins);
	assert(m_pdfjoint.dim(0) == m_bins);
	assert(m_pdfjoint.dim(1) == m_bins);

	assert(m_dpdfjoint.dim(0) == 6);
	assert(m_dpdfjoint.dim(1) == m_bins);
	assert(m_dpdfjoint.dim(1) == m_bins);

	assert(m_dpdfmove.dim(0) == 6);
	assert(m_dpdfmove.dim(1) == m_bins);

	// Degrees -> Radians, mm -> index
	double rx = params[0]*M_PI/180.;
	double ry = params[1]*M_PI/180.;
	double rz = params[2]*M_PI/180.;
	double sx = params[3]/m_moving->spacing(0);
	double sy = params[4]/m_moving->spacing(1);
	double sz = params[5]/m_moving->spacing(2);
//#if defined DEBUG || defined VERYDEBUG
	cerr << "Rotation: " << rx << ", " << ry << ", " << rz << ", Shift: "
		<< sx << ", " << sy << ", " << sz << endl;
//#endif

	// Update Transform Matrix
	Matrix3d rotation;
	rotation(0,0) = cos(ry)*cos(rz);
	rotation(0,1) = -cos(ry)*sin(rz);
	rotation(0,2) = sin(ry);
	rotation(1,0) = cos(rz)*sin(rx)*sin(ry)+cos(rx)*sin(rz);
	rotation(1,1) = cos(rx)*cos(rz) - sin(rx)*sin(ry)*sin(rz);
	rotation(1,2) = -cos(ry)*sin(rx);
	rotation(2,0) = -cos(rx)*cos(rz)*sin(ry)+sin(rx)*sin(rz);
	rotation(2,1) = cos(rz)*sin(rx)+cos(rx)*sin(ry)*sin(rz);
	rotation(2,2) = cos(rx)*cos(ry);

	Vector3d ind;
	Vector3d cind;
	Vector3d shift(sx, sy, sz);
	Eigen::Map<Vector3d> center(m_center);

	// dRigid/dRx, dRigid/dRy, dRigid/dRz = ddRx*(u-c), ddRy*(u-c) ...
	Matrix3d ddRx, ddRy, ddRz;
	ddRx <<0,0,0,cos(rx)*cos(rz)*sin(ry)-sin(rx)*sin(rz),
		-(cos(rz)*sin(rx))-cos(rx)*sin(ry)*sin(rz),
		-(cos(rx)*cos(ry)),cos(rz)*sin(rx)*sin(ry)+cos(rx)*sin(rz),
		cos(rx)*cos(rz)-sin(rx)*sin(ry)*sin(rz),-(cos(ry)*sin(rx));
	ddRy <<-(cos(rz)*sin(ry)),sin(ry)*sin(rz),cos(ry),cos(ry)*cos(rz)*sin(rx),
		-(cos(ry)*sin(rx)*sin(rz)),sin(rx)*sin(ry),-(cos(rx)*cos(ry)*cos(rz)),
		cos(rx)*cos(ry)*sin(rz),-(cos(rx)*sin(ry));
	ddRz <<-(cos(ry)*sin(rz)),-(cos(ry)*cos(rz)),0,
		cos(rx)*cos(rz)-sin(rx)*sin(ry)*sin(rz),
		-(cos(rz)*sin(rx)*sin(ry))-cos(rx)*sin(rz),0,
		cos(rz)*sin(rx)+cos(rx)*sin(ry)*sin(rz),
		cos(rx)*cos(rz)*sin(ry)-sin(rx)*sin(rz),0;

	Vector3d gradG; //dg/dx, dg/dy, dg/dz
	Matrix3d dR; //dg/dRx, dg/dRy, dg/dRz
	Vector3d dgdR;

	// Zero Everything
	m_pdfmove.zero();
	m_pdffix.zero();
	m_pdfjoint.zero();
	m_dpdfmove.zero();
	m_dpdfjoint.zero();

	// compute updated moving width
	m_wmove = (m_rangemove[1]-m_rangemove[0])/(m_bins-2*m_krad-1);
	m_wfix = (m_rangefix[1]-m_rangefix[0])/(m_bins-2*m_krad-1);

	// for computing rotated indices
	double cbinmove, cbinfix; //continuous
	double binmove, binfix; // nearest int
	double dgdPhi[6];

	// Compute Probabilities
	for(m_fit.goBegin(); !m_fit.eof(); ++m_fit) {
		m_fit.index(3, ind.array().data());

		// u = c + R^-1(v - s - c)
		// where u is the output index, v the input, c the center of rotation
		// and s the shift
		// cind = center + rInv*(ind-shift-center);
		cind = rotation*(ind-center) + center + shift;

		// Here we compute dg(v(u,p))/dp, where g is the image, u is the
		// coordinate in the fixed image, and p is the param.
		// dg/dp = SUM_i dg/dv_i dv_i/dp, where v is the rotated coordinate, so
		// dg/dv_i is the directional derivative in original space,
		// dv_i/dp is the derivative of the rotated coordinate system with
		// respect to a parameter
		gradG[0] = m_dmove_get(cind[0], cind[1], cind[2], 0);
		gradG[1] = m_dmove_get(cind[0], cind[1], cind[2], 1);
		gradG[2] = m_dmove_get(cind[0], cind[1], cind[2], 2);

		dR.row(0) = ddRx*(ind-center);
		dR.row(1) = ddRy*(ind-center);
		dR.row(2) = ddRz*(ind-center);

		// compute SUM_i dg/dv_i dv_i/dp
		dgdR = dR*gradG;
		dgdPhi[0] = dgdR[0];
		dgdPhi[1] = dgdR[1];
		dgdPhi[2] = dgdR[2];
		dgdPhi[3] = gradG[0];
		dgdPhi[4] = gradG[1];
		dgdPhi[5] = gradG[2];

		// get actual values
		double g = m_move_get(cind[0], cind[1], cind[2]);
		double f = *m_fit;

		// compute bins
		cbinfix = (f-m_rangefix[0])/m_wfix + m_krad;
		cbinmove = (g-m_rangemove[0])/m_wmove + m_krad;
		binfix = round(cbinfix);
		binmove = round(cbinmove);

		assert(binfix+m_krad < m_bins);
		assert(binmove+m_krad < m_bins);
		assert(binfix-m_krad >= 0);
		assert(binmove-m_krad >= 0);

		for(int ii = binfix-m_krad; ii <= binfix+m_krad; ii++)
			for(int jj = binmove-m_krad; jj <= binmove+m_krad; jj++) {
				m_pdfjoint[{ii,jj}] += B3kern(ii-cbinfix, m_krad)*
					B3kern(jj-cbinmove, m_krad);
		}

		for(int phi = 0; phi < 6; phi++) {
			for(int ii = binfix-m_krad; ii <= binfix+m_krad; ii++) {
				for(int jj = binmove-m_krad; jj <= binmove+m_krad; jj++) {
					m_dpdfjoint[{phi,ii,jj}] += B3kern(ii-cbinfix, m_krad)*
						dB3kern(jj-cbinmove, m_krad)*dgdPhi[phi];
				}
			}
		}
	}

	///////////////////////
	// Update Entropies
	///////////////////////

	// Scale
	size_t tbins = m_bins*m_bins;
	double scale = 0;
	for(size_t ii=0; ii<tbins; ii++)
		scale += m_pdfjoint[ii];
	scale = 1./scale;
	for(size_t ii=0; ii<tbins; ii++)
		m_pdfjoint[ii] *= scale;

	// marginals
	for(int64_t ii=0, bb=0; ii<m_bins; ii++) {
		for(int64_t jj=0; jj<m_bins; jj++, bb++) {
			m_pdffix[ii] += m_pdfjoint[bb];
			m_pdfmove[jj] += m_pdfjoint[bb];
		}
	}

	// update m_Hmove, m_Hfix
	m_Hmove = 0;
	m_Hfix= 0;
	for(int ii=0; ii<m_bins; ii++) {
		m_Hmove -= m_pdfmove[ii] > 0 ? m_pdfmove[ii]*log(m_pdfmove[ii]) : 0;
		m_Hfix -= m_pdffix[ii] > 0 ? m_pdffix[ii]*log(m_pdffix[ii]) : 0;
	}

	// update m_Hjoint
	m_Hjoint = 0;
	for(int ii=0; ii<tbins; ii++)
		m_Hjoint -= m_pdfjoint[ii] > 0 ? m_pdfjoint[ii]*log(m_pdfjoint[ii]) : 0;

	//////////////////////////////
	// Update Gradient Entropies
	//////////////////////////////
	scale = -scale/m_wmove;
	size_t dpjsz = m_dpdfjoint.elements();
	for(size_t ii=0; ii<dpjsz; ii++)
		m_dpdfjoint[ii] *= scale;

	size_t nparams = m_gradHmove.size();
	for(size_t pp=0; pp < nparams; pp++) {
		size_t jstart = pp*tbins;
		size_t mstart = pp*m_bins;
		for(int64_t ii=0, bb=0; ii<m_bins; ii++) {
			for(int64_t jj=0; jj<m_bins; jj++, bb++) {
				m_dpdfmove[mstart+jj] += m_dpdfjoint[jstart+bb];
			}
		}
	}

	// Compute Gradient Marginal Entropy
	for(size_t pp=0, dd=0; pp<nparams; pp++) {
		m_gradHmove[pp] = 0;
		for(size_t ii=0; ii<m_bins; ii++, dd++) {
			double p = m_pdfmove[ii];
			double dp = m_dpdfmove[dd];
			double v = p > 0 ? dp*(log(p)+1) : 0;
			m_gradHmove[pp] -= v;
		}
	}

	// Compute Gradient Joint Entropy
	for(size_t pp=0, dd=0; pp<nparams; pp++) {
		m_gradHjoint[pp] = 0;
		for(size_t bb=0; bb<tbins; bb++, dd++) {
			double p = m_pdfjoint[bb];
			double dp = m_dpdfjoint[dd];
			double v = p > 0 ? dp*(log(p)+1) : 0;
			m_gradHjoint[pp] -= v;
		}
	}

	// update value and grad
	if(m_metric == METRIC_MI) {
		val = m_Hfix+m_Hmove-m_Hjoint;
		for(size_t ii=0; ii<6; ii++)
			grad[ii] = m_gradHmove[ii]-m_gradHjoint[ii];

	} else if(m_metric == METRIC_VI) {
		val = 2*m_Hjoint-m_Hfix-m_Hmove;
		for(size_t ii=0; ii<6; ii++)
			grad[ii] = 2*m_gradHjoint[ii] - m_gradHmove[ii];

	} else if(m_metric == METRIC_NMI) {
		val =  (m_Hfix+m_Hmove)/m_Hjoint;
		for(size_t ii=0; ii<6; ii++)
			grad[ii] = m_gradHmove[ii]/m_Hjoint -
				m_gradHjoint[ii]*(m_Hfix+m_Hmove)/(m_Hjoint*m_Hjoint);
	}

	// We scale by this on input, so we need to scale gradient by it ...
	grad[0] *= M_PI/180.;
	grad[1] *= M_PI/180.;
	grad[2] *= M_PI/180.;
	grad[3] /= m_moving->spacing(0);
	grad[4] /= m_moving->spacing(1);
	grad[5] /= m_moving->spacing(2);

//#if defined DEBUG || defined VERYDEBUG
	cerr << "ValueGrad() = " << val << " / " << grad.transpose() << endl;
//#endif

	// negate if we are using a similarity measure
	if(m_compdiff && (m_metric == METRIC_NMI || m_metric == METRIC_MI)) {
		grad = -grad;
		val = -val;
	}

	return 0;
}

/**
 * @brief Computes the gradient of the correlation. Note that this
 * function just calls valueGrad because computing the
 * additional values are trivial
 *
 * @param params Paramters (Rx, Ry, Rz, Sx, Sy, Sz).
 * @param grad Gradient at the given rotation
 *
 * @return 0 if successful
 */
int RigidInfoComp::grad(const VectorXd& params, VectorXd& grad)
{
	double v = 0;
	return valueGrad(params, v, grad);
}

/**
 * @brief Computes the correlation.
 *
 * @param params Paramters (Rx, Ry, Rz, Sx, Sy, Sz).
 * @param val Value at the given rotation
 *
 * @return 0 if successful
 */
int RigidInfoComp::value(const VectorXd& params, double& val)
{
	if(!m_fixed)
		throw INVALID_ARGUMENT("ERROR must set fixed image before "
				"computing value.");
	if(!m_moving)
		throw INVALID_ARGUMENT("ERROR must set moving image before "
				"computing value.");
	if(!m_moving->matchingOrient(m_fixed, true, true)) {
		throw INVALID_ARGUMENT("Moving and Fixed Images must have the same "
				"orientation and gred!");
	}

	assert(m_pdfmove.dim(0) == m_bins);
	assert(m_pdffix.dim(0) == m_bins);
	assert(m_pdfjoint.dim(0) == m_bins);
	assert(m_pdfjoint.dim(1) == m_bins);

	assert(m_dpdfjoint.dim(0) == 6);
	assert(m_dpdfjoint.dim(1) == m_bins);
	assert(m_dpdfjoint.dim(1) == m_bins);

	assert(m_dpdfmove.dim(0) == 6);
	assert(m_dpdfmove.dim(1) == m_bins);

	double rx = params[0]*M_PI/180.;
	double ry = params[1]*M_PI/180.;
	double rz = params[2]*M_PI/180.;
	double sx = params[3]/m_moving->spacing(0);
	double sy = params[4]/m_moving->spacing(1);
	double sz = params[5]/m_moving->spacing(2);
//#if defined DEBUG || defined VERYDEBUG
	cerr << "Rotation: " << rx << ", " << ry << ", " << rz << ", Shift: "
		<< sx << ", " << sy << ", " << sz << endl;
//#endif

	// Update Transform Matrix
	Matrix3d rotation;
	rotation(0,0) = cos(ry)*cos(rz);
	rotation(0,1) = -cos(ry)*sin(rz);
	rotation(0,2) = sin(ry);
	rotation(1,0) = cos(rz)*sin(rx)*sin(ry)+cos(rx)*sin(rz);
	rotation(1,1) = cos(rx)*cos(rz) - sin(rx)*sin(ry)*sin(rz);
	rotation(1,2) = -cos(ry)*sin(rx);
	rotation(2,0) = -cos(rx)*cos(rz)*sin(ry)+sin(rx)*sin(rz);
	rotation(2,1) = cos(rz)*sin(rx)+cos(rx)*sin(ry)*sin(rz);
	rotation(2,2) = cos(rx)*cos(ry);

	Vector3d shift(sx, sy, sz);
	Eigen::Map<Vector3d> center(m_center);
	Vector3d ind;
	Vector3d cind;

	// Zero
	m_pdfmove.zero();
	m_pdffix.zero();
	m_pdfjoint.zero();

	m_wmove = (m_rangemove[1]-m_rangemove[0])/(m_bins-2*m_krad-1);
	m_wfix = (m_rangefix[1]-m_rangefix[0])/(m_bins-2*m_krad-1);

	// for computing rotated indices
	double cbinmove, cbinfix; //continuous
	double binmove, binfix; // nearest int

	// Compute Probabilities
	for(m_fit.goBegin(); !m_fit.eof(); ++m_fit) {
		m_fit.index(3, ind.array().data());

		// u = c + R^-1(v - s - c)
		// where u is the output index, v the input, c the center of rotation
		// and s the shift
		cind = rotation*(ind-center)+center+shift;

		// get actual values
		double g = m_move_get(cind[0], cind[1], cind[2]);
		double f = *m_fit;

		// compute bins
		cbinfix = (f-m_rangefix[0])/m_wfix + m_krad;
		cbinmove = (g-m_rangemove[0])/m_wmove + m_krad;
		binfix = round(cbinfix);
		binmove = round(cbinmove);

		assert(binfix+m_krad < m_bins);
		assert(binmove+m_krad < m_bins);
		assert(binfix-m_krad >= 0);
		assert(binmove-m_krad >= 0);

		for(int ii = binfix-m_krad; ii <= binfix+m_krad; ii++) {
			for(int jj = binmove-m_krad; jj <= binmove+m_krad; jj++)
				m_pdfjoint[{ii,jj}] += B3kern(ii-cbinfix, m_krad)*
					B3kern(jj-cbinmove, m_krad);
		}
	}

	// Scale
	size_t tbins = m_bins*m_bins;
	double scale = 0;
	for(size_t ii=0; ii<tbins; ii++)
		scale += m_pdfjoint[ii];
	scale = 1./scale;
	for(size_t ii=0; ii<tbins; ii++)
		m_pdfjoint[ii] *= scale;

	// marginals
	for(int64_t ii=0, bb=0; ii<m_bins; ii++) {
		for(int64_t jj=0; jj<m_bins; jj++, bb++) {
			m_pdffix[ii] += m_pdfjoint[bb];
			m_pdfmove[jj] += m_pdfjoint[bb];
		}
	}

	// update m_Hmove
	m_Hmove = 0;
	m_Hfix = 0;
	for(int ii=0; ii<m_bins; ii++) {
		m_Hmove -= m_pdfmove[ii] > 0 ? m_pdfmove[ii]*log(m_pdfmove[ii]) : 0;
		m_Hfix -= m_pdffix[ii] > 0 ? m_pdffix[ii]*log(m_pdffix[ii]) : 0;
	}

	// update m_Hjoint
	m_Hjoint = 0;
	for(int ii=0; ii<tbins; ii++)
		m_Hjoint -= m_pdfjoint[ii] > 0 ? m_pdfjoint[ii]*log(m_pdfjoint[ii]) : 0;

	// update value
	if(m_metric == METRIC_MI) {
		val = m_Hfix+m_Hmove-m_Hjoint;
	} else if(m_metric == METRIC_VI) {
		val = 2*m_Hjoint-m_Hfix-m_Hmove;
	} else if(m_metric == METRIC_NMI) {
		val =  (m_Hfix+m_Hmove)/m_Hjoint;
	}

//#if defined DEBUG || defined VERYDEBUG
	cerr << "Value() = " << val << endl;
//#endif

	// negate if we are using a similarity measure
	if(m_compdiff && (m_metric == METRIC_NMI || m_metric == METRIC_MI)) {
		val = -val;
	}

	return 0;
};

/****************************************************************************
 * Mutual Information/Normalize Mutual Information/Variation of Information
 * Distortion Correction Based on a Probability Map
 ****************************************************************************/

/**
 * @brief Constructor for the rigid correlation class. Note that
 * rigid rotation is assumed to be about the center of the fixed
 * image space. If necessary the input moving image will be resampled.
 * To the same space as the fixed image.
 *
 * @param fixed Fixed image. A copy of this will be made.
 * @param moving Moving image. A copy of this will be made.
 * @param compdiff negate MI and NMI to create an effective distance
 */
ProbDistCorrInfoComp::ProbDistCorrInfoComp(bool compdiff) :
			m_compdiff(compdiff), m_metric(METRIC_MI)
{
	m_bins = 128;
	m_krad = 4;
	m_dir = 1;
	m_tps_reg = 0;
	m_jac_reg = 0;
}

/**
 * @brief Initializes knot spacing and if m_fixed has been set, then
 * initializes the m_deform image.
 *
 * Note that modification of the moving image outside this class without
 * re-calling setMoving is undefined and will result in an out-of-date
 * moving image derivative. THIS WILL BREAK GRADIENT CALCULATIONS.
 *
 * @param newfixed New fixed image
 * @param moving Input moving image, should be 4D with each volume containing
 * a probability map (not modified)
 * @param dir Direction of distortion (the dimension, must be >= 0)
 * @param space Spacing between knots, in physical coordinates
 */
void ProbDistCorrInfoComp::initialize(ptr<const MRImage> fixed,
		ptr<const MRImage> moving, size_t krad, size_t nbins, double space,
		int dir)
{
	if(moving->ndim() != 4)
		throw INVALID_ARGUMENT("Moving image is not 4D!");
	if(fixed->ndim() != 3)
		throw INVALID_ARGUMENT("Fixed image is not 3D!");
	if(m_dir >= 3)
		throw INVALID_ARGUMENT("Error direction should in [0,3)");
	if(nbins <= krad*2)
		throw INVALID_ARGUMENT("m_bins must be > m_krad*2");

	// Set Direction
	if(dir >= 0)
		m_dir = dir;
	else if(m_moving->m_phasedim >= 0)
		m_dir = m_moving->m_phasedim;
	else
		m_dir = 1;

	m_fixed = fixed;
	m_moving = moving;
	m_dmoving = dPtrCast<MRImage>(derivative(m_moving, m_dir));
	m_bins = nbins;
	m_krad = krad;

	size_t movbins = m_moving->tlen();
	// allocate PDFs
	m_pdffix.resize({nbins});
	m_pdfmove.resize({movbins});
	m_pdfjoint.resize({movbins, nbins});

	/************************
	 * Create Deform
	 ************************/
	VectorXd spacing(3);
	VectorXd origin(3);
	vector<size_t> osize(5, 0);

	// get spacing and size
	for(size_t dd=0; dd<3; ++dd) {
		osize[dd] = 4+ceil(m_fixed->dim(dd)*m_fixed->spacing(dd)/space);
		spacing[dd] = space;
	}
	osize[3] = movbins;
	osize[4] = m_bins;

	m_deform = createMRImage(3, osize.data(), FLOAT64);
	m_deform->setDirection(m_fixed->getDirection(), false);
	m_deform->setSpacing(spacing, false);

	// compute center of input
	VectorXd indc(3);
	for(size_t dd=0; dd<3; dd++)
		indc[dd] = (m_fixed->dim(dd)-1.)/2.;
	VectorXd ptc(3); // point center
	m_fixed->indexToPoint(3, indc.array().data(), ptc.array().data());

	// compute origin from center index (x_c) and center of input (c):
	// o = c-R(sx_c)
	for(size_t dd=0; dd<3; dd++)
		indc[dd] = (osize[dd]-1.)/2.;
	origin = ptc - m_fixed->getDirection()*(spacing.asDiagonal()*indc);
	m_deform->setOrigin(origin, false);

	for(FlatIter<double> it(m_deform); !it.eof(); ++it)
		it.set(0);

	m_deform->m_phasedim = m_dir;

	/*************************************
	 * Create buffers for gradient
	 *************************************/
	gradbuff.resize(m_deform->elements());
	m_gradHjoint.resize(osize.data()); //3D
	m_gradHmove.resize(osize.data()); //3D

	// Create Derivates of Individual Bins
	m_dpdfmove.resize(osize.data()); // 4D
	m_dpdfjoint.resize(osize.data()); // 5D
}

/**
 * @brief Updates m_move_cache, m_dmove_cache, m_corr_cache and m_rangemove
 */
void ProbDistCorrInfoComp::updateCaches()
{
	NDConstView<double> fixed_vw(m_fixed);
	BSplineView<double> bsp_vw(m_deform);
	bsp_vw.m_boundmethod = ZEROFLUX;
	bsp_vw.m_ras = true;

	// Compute Probabilities
	size_t dirlen = m_fixed_cache->dim(m_dir);
	double dcind[3]; // index in distortion image
	int64_t mind[3] ;
	double pt[3]; // point
	double Fm = 0;
	for(NDIter<double> mit(m_fixed_cache); !mit.eof(); ++mit) {

		// Compute Continuous Index of Point in Deform Image
		mit.index(3, mind);
		m_fixed_cache->indexToPoint(3, mind, pt);
		m_deform->pointToIndex(3, pt, dcind);

		// Sample B-Spline Value and Derivative at Current Position
		double def = 0, ddef = 0;
		bsp_vw.get(3, pt, m_dir, def, ddef);

		// get linear index
		double cind = mind[m_dir] + def/m_fixed->spacing(m_dir);
		int64_t below = (int64_t)floor(cind);
		int64_t above = below + 1;
		Fm = 0;

		// get values
		if(below >= 0 && below < dirlen) {
			mind[m_dir] = below;
			Fm += fixed_vw.get(3, mind)*linKern(below-cind);
		}
		if(above >= 0 && above < dirlen) {
			mind[m_dir] = above;
			Fm += fixed_vw.get(3, mind)*linKern(above-cind);
		}

		mit.set(Fm);
		m_rangefix[0] = std::min(m_rangefix[0], Fm);
		m_rangefix[1] = std::max(m_rangefix[1], Fm);
	}
}

/**
 * @brief Computes the metric value and gradient
 *
 * @param val Output Value
 * @param grad Output Gradient, in index coordinates (change of variables
 * handled outside this function)
 */
int ProbDistCorrInfoComp::metric(double& val, VectorXd& grad)
{
	CLOCK(clock_t c = clock());

	//Zero Inputs
	m_pdfmove.zero();
	m_pdffix.zero();
	m_pdfjoint.zero();
	m_dpdfmove.zero();
	m_dpdfjoint.zero();

	// for computing distorted indices
	int binfix; // nearest int
	double fcind[3]; // Continuous index in fixed image
	double dcind[3]; // Continuous index in deformation
	int64_t dnind[3]; // Nearest knot to dcind
	int64_t dind[5]; // last two dimensions are for bins
	double pt[3];

	// probweight cached derivative of p wrt to the center parameter
	vector<double> fixweight(2*m_krad+1);
	vector<double> invspace(m_deform->ndim());
	for(size_t dd=0; dd<m_deform->ndim(); dd++)
		invspace[dd] = 1./m_deform->spacing(dd);

	Counter<> neighbors(3);
	for(size_t dd=0; dd<3; dd++) neighbors.sz[dd] = 5;

	// Compute Updated Version of Distortion-Corrected Image
	CLOCK(c = clock());
	updateCaches();
	CLOCK(c = clock() - c);
	CLOCK(cerr << "Apply() Time:" << c << endl);
	CLOCK(c = clock());

	// Create Iterators/ Viewers
	Vector3DConstIter<double> mit(m_moving);
	Vector3DConstIter<double> dmit(m_dmoving);
	NDConstIter<double> fit(m_fixed_cache);

	BSplineView<double> bsp_vw(m_deform);
	bsp_vw.m_boundmethod = ZEROFLUX;
	bsp_vw.m_ras = true;

	// compute updated moving width
	m_wfix = (m_rangefix[1]-m_rangefix[0])/(m_bins-1);
	size_t movbins = m_moving->tlen();

	// Compute Probabilities
	for(; !fit.eof(); ++mit, ++fit, ++dmit) {

		// Compute Continuous Index of Point in Deform Image
		fit.index(3, fcind);
		m_fixed_cache->indexToPoint(3, fcind, pt);
		m_deform->pointToIndex(3, pt, dcind);

		// compute bins
		binfix = round((fit.get()-m_rangefix[0])/m_wfix);

		/**************************************************************
		 * Compute Marginal and Joint PDF's
		 **************************************************************/
		// Sum up Bins for Value Comp
		for(int ii = 0; ii < movbins; ii++) {
			m_pdfjoint[{ii,binfix}] += mit[ii];
		}

		/**************************************************************
		 * Compute Derivative of Marginal and Joint PDFs
		 **************************************************************/

		// Find center
		for(size_t dd=0; dd<3; dd++)
			dnind[dd] = round(dcind[dd]);

		/********************************************************
		 * Add to Derivatives of Bins Within Reach of the Point
		 *******************************************************/
		for(dind[0] = max<int64_t>(0,dnind[0]-2); dind[0] <
				min<int64_t>(m_deform->dim(0),dnind[0]+2); dind[0]++) {
		for(dind[1] = max<int64_t>(0,dnind[1]-2); dind[1] <
				min<int64_t>(m_deform->dim(1),dnind[1]+2); dind[1]++) {
		for(dind[2] = max<int64_t>(0,dnind[2]-2); dind[2] <
				min<int64_t>(m_deform->dim(2),dnind[2]+2); dind[2]++) {

			double dPHI_dphi = 1;
			for(int ii = 0; ii < 3; ii++)
				dPHI_dphi *= B3kern(dind[ii]-dcind[ii]);

			double dPHI_dydphi = 1;
			for(int ii = 0; ii < 3; ii++) {
				if(ii == m_dir)
					dPHI_dydphi *= -invspace[ii]*dB3kern(dind[ii]-dcind[ii]);
				else
					dPHI_dydphi *= B3kern(dind[ii]-dcind[ii]);
			}

			for(int ii = 0; ii < movbins; ii++) {
				dind[3] = ii;
				dind[4] = binfix;
				m_dpdfjoint[dind] += dmit[ii]*dPHI_dydphi;
			}
		}
		}
		}
	}

	///////////////////////
	// Update Entropies
	///////////////////////

	// scale
	size_t tbins = movbins*m_bins;
	double scale = 0;
	for(size_t ii=0; ii<tbins; ii++)
		scale += m_pdfjoint[ii];
	scale = 1./scale;
	for(size_t ii=0; ii<tbins; ii++)
		m_pdfjoint[ii] *= scale;

	// marginals
	for(int64_t ii=0, bb=0; ii<m_bins; ii++) {
		for(int64_t jj=0; jj<m_bins; jj++, bb++) {
			m_pdffix[ii] += m_pdfjoint[bb];
			m_pdfmove[jj] += m_pdfjoint[bb];
		}
	}

	//////////////////////////////
	// Update Gradient Entropies
	//////////////////////////////

	size_t dpjsz = m_dpdfjoint.elements();
	for(size_t ii=0; ii<dpjsz; ii++)
		m_dpdfjoint[ii] *= -scale;
	size_t dpmsz = m_dpdfmove.elements();
	for(size_t ii=0; ii<dpmsz; ii++)
		m_dpdfmove[ii] *= -scale;

	size_t nparams = m_gradHmove.elements();

	CLOCK(c = clock() - c);
	CLOCK(cerr << "dPDF() Time: " << c << endl);
	CLOCK(c = clock());

	// update m_Hmove
	m_Hmove = 0;
	m_Hfix = 0;
	for(int ii=0; ii<m_bins; ii++) {
		m_Hmove -= m_pdfmove[ii] > 0 ? m_pdfmove[ii]*log(m_pdfmove[ii]) : 0;
		m_Hfix -= m_pdffix[ii] > 0 ? m_pdffix[ii]*log(m_pdffix[ii]) : 0;
	}

	// update m_Hjoint
	m_Hjoint = 0;
	for(int ii=0; ii<tbins; ii++)
		m_Hjoint -= m_pdfjoint[ii] > 0 ? m_pdfjoint[ii]*log(m_pdfjoint[ii]) : 0;


	// Compute Gradient Marginal Entropy
	m_gradHmove.zero();
	for(int64_t ii=0; ii<m_bins; ii++) {
		double p = m_pdfmove[ii];
		if(p <= 0)
			continue;
		double lp = (log(p)+1);
		for(int64_t pp=0; pp<nparams; pp++)
			m_gradHmove[pp] -= lp*m_dpdfmove[pp*m_bins + ii];
	}

	// Compute Gradient Joint Entropy
	m_gradHjoint.zero();
	for(size_t ii=0; ii<tbins; ii++) {
		double p = m_pdfjoint[ii];
		if(p <= 0)
			continue;
		double lp = (log(p)+1);
		for(size_t pp=0; pp<nparams; pp++)
			m_gradHjoint[pp] -= lp*m_dpdfjoint[pp*tbins + ii];
	}

	CLOCK(c = clock() - c);
	CLOCK(cerr << "dH() Time: " << c << endl);
	CLOCK(c = clock());

	// update value and grad
	if(m_metric == METRIC_MI) {
		size_t elements = m_deform->elements();
		val = m_Hfix+m_Hmove-m_Hjoint;
		// Fill Gradient From GradH Images
		for(size_t ii=0; ii<elements; ii++)
			grad[ii] = (m_gradHmove[ii]-m_gradHjoint[ii])*
				m_moving->spacing(m_dir);

	} else if(m_metric == METRIC_VI) {
		size_t elements = m_deform->elements();
		val = 2*m_Hjoint-m_Hfix-m_Hmove;
		for(size_t ii=0; ii<elements; ii++)
			grad[ii] = (2*m_gradHjoint[ii] - m_gradHmove[ii])*
				m_moving->spacing(m_dir);

	} else if(m_metric == METRIC_NMI) {
		size_t elements = m_deform->elements();
		val =  (m_Hfix+m_Hmove)/m_Hjoint;
		for(size_t ii=0; ii<elements; ii++)
			grad[ii] = m_moving->spacing(m_dir)*(m_gradHmove[ii]/m_Hjoint -
				m_gradHjoint[ii]*(m_Hfix+m_Hmove)/(m_Hjoint*m_Hjoint));
	}

	// negate if we are using a similarity measure
	if(m_compdiff && (m_metric == METRIC_NMI || m_metric == METRIC_MI)) {
		grad = -grad;
		val = -val;
	}

	return 0;
}

/**
 * @brief Computes the metric value
 *
 * @param val Output Value
 */
int ProbDistCorrInfoComp::metric(double& val)
{
	// Views
	BSplineView<double> bsp_vw(m_deform);
	bsp_vw.m_boundmethod = ZEROFLUX;
	bsp_vw.m_ras = true;

	//Zero Inputs
	m_pdfmove.zero();
	m_pdffix.zero();
	m_pdfjoint.zero();

	// for computing distorted indices
	int binfix; // nearest int

	// Compute Updated Version of Distortion-Corrected Image
	CLOCK(auto c = clock());
	updateCaches();
	CLOCK(c = clock() - c);
	CLOCK(cerr << "Apply() Time: " << c << endl);
	CLOCK(c = clock());
	size_t movbins = m_moving->tlen();

	// Create Iterators/ Viewers
	Vector3DConstIter<double> mit(m_moving);
	NDConstIter<double> fit(m_fixed_cache);

	// compute updated moving width
	m_wfix = (m_rangefix[1]-m_rangefix[0])/(m_bins-1);

	// Compute Probabilities
	for(; !fit.eof(); ++mit, ++fit) {
		// compute bins
		binfix = round((fit.get()-m_rangefix[0])/m_wfix);

		/**************************************************************
		 * Compute Joint PDF
		 **************************************************************/
		// Sum up Bins for Value Comp
		for(int ii = 0; ii < movbins; ii++)
			m_pdfjoint[{ii,binfix}] += mit[ii];
	}

	///////////////////////
	// Update Entropies
	///////////////////////

	// Scale
	size_t tbins = m_bins*m_bins;
	double scale = 0;
	for(size_t ii=0; ii<tbins; ii++)
		scale += m_pdfjoint[ii];
	scale = 1./scale;
	for(size_t ii=0; ii<tbins; ii++)
		m_pdfjoint[ii] *= scale;

	// marginals
	for(int64_t ii=0, bb=0; ii<m_bins; ii++) {
		for(int64_t jj=0; jj<m_bins; jj++, bb++) {
			m_pdffix[ii] += m_pdfjoint[bb];
			m_pdfmove[jj] += m_pdfjoint[bb];
		}
	}

	// Scale Joint
	CLOCK(c = clock() - c);
	CLOCK(cerr << "PDF() Time: " << c << endl);
	CLOCK(c = clock());

	// Compute Marginal Entropy
	m_Hmove = 0;
	m_Hfix = 0;
	for(int ii=0; ii<m_bins; ii++) {
		m_Hmove -= m_pdfmove[ii] > 0 ? m_pdfmove[ii]*log(m_pdfmove[ii]) : 0;
		m_Hfix -= m_pdffix[ii] > 0 ? m_pdffix[ii]*log(m_pdffix[ii]) : 0;
	}

	// Compute Joint Entropy
	m_Hjoint = 0;
	size_t elem = m_pdfjoint.elements();
	for(int ii=0; ii<elem; ii++)
		m_Hjoint -= m_pdfjoint[ii] > 0 ? m_pdfjoint[ii]*log(m_pdfjoint[ii]) : 0;

	// update value and grad
	if(m_metric == METRIC_MI) {
		val = m_Hfix+m_Hmove-m_Hjoint;
	} else if(m_metric == METRIC_VI) {
		val = 2*m_Hjoint-m_Hfix-m_Hmove;
	} else if(m_metric == METRIC_NMI) {
		val =  (m_Hfix+m_Hmove)/m_Hjoint;
	}

	CLOCK(c = clock() - c);
	CLOCK(cerr << "H() Time: " << c << endl);
	CLOCK(c = clock());

	// negate if we are using a similarity measure
	if(m_compdiff && (m_metric == METRIC_NMI || m_metric == METRIC_MI))
		val = -val;

	return 0;
}

/**
 * @brief Computes the gradient and value of the correlation.
 *
 * @param x Paramters (Rx, Ry, Rz, Sx, Sy, Sz).
 * @param v Value at the given rotation
 * @param g Gradient at the given rotation
 *
 * @return 0 if successful
 */
int ProbDistCorrInfoComp::valueGrad(const VectorXd& params, double& val,
		VectorXd& grad)
{
	/*
	 * Check That Everything Is Properly Initialized
	 */
	if(!m_fixed) throw INVALID_ARGUMENT("ERROR must set fixed image before "
				"computing value.");
	if(!m_moving) throw INVALID_ARGUMENT("ERROR must set moving image before "
				"computing value.");
	if(!m_moving->matchingOrient(m_fixed, true, true))
		throw INVALID_ARGUMENT("ERROR input images must be in same index "
				"space");

	assert(m_bins > m_krad*2);

	assert(m_pdfmove.dim(0) == m_bins);
	assert(m_pdfjoint.dim(0) == m_bins); // Moving
	assert(m_pdfjoint.dim(1) == m_bins); // Fixed

	assert(m_dpdfjoint.dim(0) == m_deform->dim(0));
	assert(m_dpdfjoint.dim(1) == m_deform->dim(1));
	assert(m_dpdfjoint.dim(2) == m_deform->dim(2));
	assert(m_dpdfjoint.dim(3) == m_bins); // Moving PDF
	assert(m_dpdfjoint.dim(4) == m_bins); // Fixed PDF

	assert(m_dpdfmove.dim(0) == m_deform->dim(0));
	assert(m_dpdfmove.dim(1) == m_deform->dim(1));
	assert(m_dpdfmove.dim(2) == m_deform->dim(2));
	assert(m_dpdfmove.dim(3) == m_bins); // Moving

	assert(m_deform->elements() == gradbuff.rows());

	// Fill Deform Image from params
	FlatIter<double> dit(m_deform);
	for(size_t ii=0; !dit.eof(); ++dit, ++ii)
		dit.set(params[ii]);

	/*
	 * Compute the Gradient and Value
	 */
	val  = 0;
	size_t nparam = m_deform->elements();
	grad.setZero();
	BSplineView<double> b_vw(m_deform);
	b_vw.m_boundmethod = ZEROFLUX;

	// Compute and add Thin Plate Spline
	if(m_tps_reg > 0) {
		val += m_tps_reg*b_vw.thinPlateEnergy(nparam,gradbuff.array().data());
		grad += m_tps_reg*gradbuff;
	}

	// Compute and add Jacobian
	if(m_jac_reg > 0) {
		val += m_jac_reg*b_vw.jacobianDet(m_dir,nparam,gradbuff.array().data());
		grad += m_jac_reg*gradbuff;
	}

	// Compute and add Metric
	double tmp = 0;
	if(metric(tmp, gradbuff) != 0) return -1;
	val += tmp;
	grad += gradbuff;

//#if defined DEBUG || defined VERYDEBUG
	cerr << "ValueGrad() = " << val << " / " << grad.norm() << endl;
//#endif

	return 0;
}


/**
 * @brief Computes the gradient of the correlation. Note that this
 * function just calls valueGrad because computing the
 * additional values are trivial
 *
 * @param params Paramters (Rx, Ry, Rz, Sx, Sy, Sz).
 * @param grad Gradient at the given rotation
 *
 * @return 0 if successful
 */
int ProbDistCorrInfoComp::grad(const VectorXd& params, VectorXd& grad)
{
	double v = 0;
	return valueGrad(params, v, grad);
}

/**
 * @brief Computes the correlation.
 *
 * @param params Paramters (Rx, Ry, Rz, Sx, Sy, Sz).
 * @param val Value at the given rotation
 *
 * @return 0 if successful
 */
int ProbDistCorrInfoComp::value(const VectorXd& params, double& val)
{
	/*************************************************************************
	 * Check State
	 ************************************************************************/
	if(!m_fixed) throw INVALID_ARGUMENT("ERROR must set fixed image before "
				"computing value.");
	if(!m_moving) throw INVALID_ARGUMENT("ERROR must set moving image before "
				"computing value.");
	if(!m_moving->matchingOrient(m_fixed, true, true))
		throw INVALID_ARGUMENT("ERROR input images must be in same index "
				"space");

	assert(m_bins > m_krad*2);

	assert(m_pdfmove.dim(0) == m_bins);
	assert(m_pdfjoint.dim(0) == m_bins); // Moving
	assert(m_pdfjoint.dim(1) == m_bins); // Fixed

	// Fill Parameter Image
	FlatIter<double> dit(m_deform);
	for(size_t ii=0; !dit.eof(); ++dit, ++ii)
		dit.set(params[ii]);

	/*
	 * Compute Value
	 */
	val = 0;
	BSplineView<double> b_vw(m_deform);
	b_vw.m_boundmethod = ZEROFLUX;

	// Compute and add Thin Plate Spline
	if(m_tps_reg > 0) {
		val += b_vw.thinPlateEnergy()*m_tps_reg;
	}

	// Compute and add Jacobian
	if(m_jac_reg > 0) {
		val += b_vw.jacobianDet(m_dir)*m_jac_reg;
	}

	// Compute and add Metric
	double tmp = 0;
	if(metric(tmp) != 0) return -1;
	val += tmp;

//#if defined DEBUG || defined VERYDEBUG
	cerr << "Value() = " << val << endl;
//#endif

	return 0;
};

/****************************************************************************
 * Mutual Information/Normalize Mutual Information/Variation of Information
 * Distortion Correction
 ****************************************************************************/

/**
 * @brief Constructor for the rigid correlation class. Note that
 * rigid rotation is assumed to be about the center of the fixed
 * image space. If necessary the input moving image will be resampled.
 * To the same space as the fixed image.
 *
 * @param fixed Fixed image. A copy of this will be made.
 * @param moving Moving image. A copy of this will be made.
 * @param compdiff negate MI and NMI to create an effective distance
 */
DistCorrInfoComp::DistCorrInfoComp(bool compdiff) :
	m_compdiff(compdiff), m_metric(METRIC_MI)
{
	m_knotspace = 10;
	setBins(128, 4);
	m_dir = 1;
	m_tps_reg = 0;
	m_jac_reg = 0;
}

/**
 * @brief Initializes knot spacing and if m_fixed has been set, then
 * initializes the m_deform image.
 *
 * @param space Spacing between knots, in physical coordinates
 */
void DistCorrInfoComp::initializeKnots(double space)
{
	m_knotspace = space;
	if(!m_fixed) {
		throw INVALID_ARGUMENT("Error, cannot set knot spacing without a "
			"fixed image for reference! Set fixed image template before "
			"initializeKnots. Later changes to fixed image will not affect "
			"the knot image.");
	}

	size_t ndim = m_fixed->ndim();

	VectorXd spacing(ndim);
	VectorXd origin(ndim);
	vector<size_t> osize(ndim+2, 0);

	// get spacing and size
	for(size_t dd=0; dd<ndim; ++dd) {
		osize[dd] = 4+ceil(m_fixed->dim(dd)*m_fixed->spacing(dd)/space);
		spacing[dd] = space;
	}
	osize[m_fixed->ndim()] = m_bins;
	osize[m_fixed->ndim()+1] = m_bins;

	/************************
	 * Create Deform
	 ************************/
	m_deform = createMRImage(ndim, osize.data(), FLOAT64);
	m_deform->setDirection(m_fixed->getDirection(), false);
	m_deform->setSpacing(spacing, false);

	// compute center of input
	VectorXd indc(ndim);
	for(size_t dd=0; dd<ndim; dd++)
		indc[dd] = (m_fixed->dim(dd)-1.)/2.;
	VectorXd ptc(ndim); // point center
	m_fixed->indexToPoint(ndim, indc.array().data(), ptc.array().data());

	// compute origin from center index (x_c) and center of input (c):
	// o = c-R(sx_c)
	for(size_t dd=0; dd<ndim; dd++)
		indc[dd] = (osize[dd]-1.)/2.;
	origin = ptc - m_fixed->getDirection()*(spacing.asDiagonal()*indc);
	m_deform->setOrigin(origin, false);

	for(FlatIter<double> it(m_deform); !it.eof(); ++it)
		it.set(0);

	/*************************************
	 * Create buffers for gradient
	 *************************************/
	gradbuff.resize(m_deform->elements());
	m_gradHjoint.resize(osize.data());
	m_gradHmove.resize(osize.data());

	// Create Derivates of Individual Bins
	m_dpdfmove.resize(osize.data());
	m_dpdfjoint.resize(osize.data());

	m_deform->m_phasedim = m_dir;
}

/**
 * @brief Reallocates histograms and if m_fixed has been set, regenerates
 * histogram estimate of fixed pdf
 *
 * @param nbins Number of bins for marginal estimation
 * @param krad Number of bins in kernel radius
 */
void DistCorrInfoComp::setBins(size_t nbins, size_t krad)
{
	if(nbins <= krad*2)
		throw INVALID_ARGUMENT("m_bins must be > m_krad*2");

	// set number of bins
	m_bins = nbins;
	m_krad = krad;

	// reallocate memory
	m_pdfmove.resize({nbins});
	m_pdffix.resize({nbins});
	m_pdfjoint.resize({nbins, nbins});

	if(m_fixed) {
		// if fixed hsa been set, recompute fixed entropy, and adjust field
		setFixed(m_fixed);
	}
}

/**
 * @brief Sets the moving image for Information-Based comparison. This sets
 * m_moving, m_move_get, d_moving, m_dmove_get and m_rangemove
 *
 * @param newmove New moving image
 */
void DistCorrInfoComp::setMoving(ptr<const MRImage> newmove, int dir)
{
	if(newmove->ndim() != 3)
		throw INVALID_ARGUMENT("Moving image is not 3D!");

	//////////////////////////////////////
	// Setup accessors and Members Images
	//////////////////////////////////////
	m_moving = newmove;

	if(dir >= 0)
		m_dir = dir;
	else if(m_moving->m_phasedim >= 0)
		m_dir = m_moving->m_phasedim;
	else
		m_dir = 1;

	if(m_deform) m_deform->m_phasedim = m_dir;

	m_dmoving = dPtrCast<MRImage>(derivative(m_moving, m_dir));

	// must include 0 because outside values get mapped to 0
	m_rangemove[0] = 0;
	m_rangemove[1] = 0;
	for(NDConstIter<double> it(m_moving); !it.eof(); ++it) {
		m_rangemove[0] = std::min(m_rangemove[0], *it);
		m_rangemove[1] = std::max(m_rangemove[1], *it);
	}

	m_move_cache = dPtrCast<MRImage>(m_moving->createAnother());
	m_corr_cache = dPtrCast<MRImage>(m_moving->createAnother()); // DYING HERE
	m_dmove_cache = dPtrCast<MRImage>(m_dmoving->createAnother());
}

/**
 * @brief Sets the fixed image for Information-Based comparison. This sets
 * m_fixed, m_fit, m_rangefix, m_wfix, fills the histogram m_pdffix, and
 * computes m_Hfix
 *
 * @param newfixed New fixed image
 */
void DistCorrInfoComp::setFixed(ptr<const MRImage> newfixed)
{
	if(newfixed->ndim() != 3)
		throw INVALID_ARGUMENT("Fixed image is not 3D!");

	m_fixed = newfixed;

	// Compute Range of Values
	m_rangefix[0] = INFINITY;
	m_rangefix[1] = -INFINITY;
	for(NDConstIter<double> it(m_fixed); !it.eof(); ++it) {
		m_rangefix[0] = std::min(m_rangefix[0], *it);
		m_rangefix[1] = std::max(m_rangefix[1], *it);
	}
}

/**
 * @brief Updates m_move_cache, m_dmove_cache, m_corr_cache and m_rangemove
 */
void DistCorrInfoComp::updateCaches()
{
	m_rangemove[0] = 0;
	m_rangemove[1] = 0;
	NDConstView<double> move_vw(m_moving);
	NDConstView<double> dmove_vw(m_dmoving);
	BSplineView<double> bsp_vw(m_deform);
	bsp_vw.m_boundmethod = ZEROFLUX;
	bsp_vw.m_ras = true;

	// Compute Probabilities
	size_t dirlen = m_corr_cache->dim(m_dir);
	double dcind[3]; // index in distortion image
	int64_t mind[3] ;
	double pt[3]; // point
	double Fm = 0;
	double dFm = 0;
	for(NDIter<double> dmit(m_dmove_cache), mit(m_move_cache), cit(m_corr_cache);
			!mit.eof(); ++dmit, ++mit, ++cit) {

		// Compute Continuous Index of Point in Deform Image
		dmit.index(3, mind);
		m_move_cache->indexToPoint(3, mind, pt);
		m_deform->pointToIndex(3, pt, dcind);

		// Sample B-Spline Value and Derivative at Current Position
		double def = 0, ddef = 0;
		bsp_vw.get(3, pt, m_dir, def, ddef);

		// get linear index
		double cind = mind[m_dir] + def/m_moving->spacing(m_dir);
		int64_t below = (int64_t)floor(cind);
		int64_t above = below + 1;
		Fm = 0;
		dFm = 0;

		// get values
		if(below >= 0 && below < dirlen) {
			mind[m_dir] = below;
			dFm += dmove_vw.get(3, mind)*linKern(below-cind);
			Fm += move_vw.get(3, mind)*linKern(below-cind);
		}
		if(above >= 0 && above < dirlen) {
			mind[m_dir] = above;
			dFm += dmove_vw.get(3, mind)*linKern(above-cind);
			Fm += move_vw.get(3, mind)*linKern(above-cind);
		}

		if(Fm < 1e-10 || ddef < -1) Fm = 0;
		mit.set(Fm);
		cit.set(Fm*(1+ddef));
		dmit.set(dFm);

		m_rangemove[0] = std::min(m_rangemove[0], cit.get());
		m_rangemove[1] = std::max(m_rangemove[1], cit.get());
	}
}

/**
 * @brief Computes the metric value and gradient
 *
 * @param val Output Value
 * @param grad Output Gradient, in index coordinates (change of variables
 * handled outside this function)
 */
int DistCorrInfoComp::metric(double& val, VectorXd& grad)
{
	CLOCK(clock_t c = clock());
	//Zero Inputs
	m_pdfmove.zero();
	m_pdffix.zero();
	m_pdfjoint.zero();
	m_dpdfmove.zero();
	m_dpdfjoint.zero();

	// for computing distorted indices
	double cbinmove, cbinfix; //continuous
	int binmove, binfix; // nearest int
	double fcind[3]; // Continuous index in fixed image
	double dcind[3]; // Continuous index in deformation
	int64_t dnind[3]; // Nearest knot to dcind
	int64_t dind[5]; // last two dimensions are for bins
	double pt[3];
	double Fm;   /** Moving Value */
	double dFm;  /** Derivative Moving Value */
	double Fc;   /** Intensity Corrected Moving Value */
	double Ff;   /** Fixed Value Value */

	// probweight cached derivative of p wrt to the center parameter
	vector<double> dmovweight(2*m_krad+1);
	vector<double> movweight(2*m_krad+1);
	vector<double> fixweight(2*m_krad+1);
	vector<double> invspace(m_deform->ndim());
	for(size_t dd=0; dd<m_deform->ndim(); dd++)
		invspace[dd] = 1./m_deform->spacing(dd);

	Counter<> neighbors(3);
	for(size_t dd=0; dd<3; dd++) neighbors.sz[dd] = 5;

	// Compute Updated Version of Distortion-Corrected Image
	CLOCK(c = clock());
	updateCaches();
	CLOCK(c = clock() - c);
	CLOCK(cerr << "Apply() Time:" << c << endl);
	CLOCK(c = clock());

	// Create Iterators/ Viewers
	NDConstIter<double> cit(m_corr_cache);
	NDConstIter<double> mit(m_move_cache);
	NDConstIter<double> dmit(m_dmove_cache);
	NDConstIter<double> fit(m_fixed);

	BSplineView<double> bsp_vw(m_deform);
	bsp_vw.m_boundmethod = ZEROFLUX;
	bsp_vw.m_ras = true;

	// compute updated moving width
	m_wmove = (m_rangemove[1]-m_rangemove[0])/(m_bins-2*m_krad-1);
	m_wfix = (m_rangefix[1]-m_rangefix[0])/(m_bins-2*m_krad-1);

	// Compute Probabilities
	for(cit.goBegin(), dmit.goBegin(), mit.goBegin(), fit.goBegin();
			!fit.eof(); ++cit, ++mit, ++fit, ++dmit) {

		// Compute Continuous Index of Point in Deform Image
		fit.index(3, fcind);
		m_fixed->indexToPoint(3, fcind, pt);
		m_deform->pointToIndex(3, pt, dcind);

		// get actual values
		Fm = mit.get();
		Fc = cit.get();
		dFm = dmit.get();
		Ff = fit.get();

		// compute bins
		cbinfix = (Ff-m_rangefix[0])/m_wfix + m_krad;
		binfix = round(cbinfix);
		cbinmove = (Fc-m_rangemove[0])/m_wmove + m_krad;
		binmove = round(cbinmove);

		/**************************************************************
		 * Compute Marginal and Joint PDF's
		 **************************************************************/

		for(int jj = binmove-m_krad; jj <= binmove+m_krad; jj++)
			dmovweight[jj-binmove+m_krad] = dB3kern(jj-cbinmove, m_krad);
		for(int jj = binmove-m_krad; jj <= binmove+m_krad; jj++)
			movweight[jj-binmove+m_krad] = B3kern(jj-cbinmove, m_krad);
		for(int ii = binfix-m_krad; ii <= binfix+m_krad; ii++)
			fixweight[ii-binfix+m_krad] = B3kern(ii-cbinfix, m_krad);

		// Sum up Bins for Value Comp
		for(int ii = binfix-m_krad; ii <= binfix+m_krad; ii++) {
			for(int jj = binmove-m_krad; jj <= binmove+m_krad; jj++) {
				m_pdfjoint[{ii,jj}] += movweight[jj-binmove+m_krad]*
					fixweight[ii-binfix+m_krad];
			}
		}

		/**************************************************************
		 * Compute Derivative of Marginal and Joint PDFs
		 **************************************************************/
		if(dFm == 0 && Fm == 0)
			continue;

		assert(binfix+m_krad < m_bins);
		assert(binmove+m_krad < m_bins);
		assert(binfix-m_krad >= 0);
		assert(binmove-m_krad >= 0);

		// Find center
		for(size_t dd=0; dd<3; dd++)
			dnind[dd] = round(dcind[dd]);

		/********************************************************
		 * Add to Derivatives of Bins Within Reach of the Point
		 *******************************************************/
		for(dind[0] = max<int64_t>(0,dnind[0]-2); dind[0] <
				min<int64_t>(m_deform->dim(0),dnind[0]+2); dind[0]++) {
		for(dind[1] = max<int64_t>(0,dnind[1]-2); dind[1] <
				min<int64_t>(m_deform->dim(1),dnind[1]+2); dind[1]++) {
		for(dind[2] = max<int64_t>(0,dnind[2]-2); dind[2] <
				min<int64_t>(m_deform->dim(2),dnind[2]+2); dind[2]++) {

			double dPHI_dphi = 1;
			for(int ii = 0; ii < 3; ii++)
				dPHI_dphi *= B3kern(dind[ii]-dcind[ii]);

			double dPHI_dydphi = 1;
			for(int ii = 0; ii < 3; ii++) {
				if(ii == m_dir)
					dPHI_dydphi *= -invspace[ii]*dB3kern(dind[ii]-dcind[ii]);
				else
					dPHI_dydphi *= B3kern(dind[ii]-dcind[ii]);
			}

			double dg_dphi;
			if(Fm <= 0) // 0/0 => 1
				dg_dphi = dFm*dPHI_dphi;
			else
				dg_dphi = dFm*dPHI_dphi*Fc/Fm + Fm*dPHI_dydphi;

			assert(dg_dphi == dg_dphi);
			for(int jj = binmove-m_krad; jj <= binmove+m_krad; jj++) {
				double tmp = dg_dphi*dmovweight[jj-binmove+m_krad];
				dind[3] = jj;
				m_dpdfmove[dind] += tmp;
				for(int ii = binfix-m_krad; ii <= binfix+m_krad; ii++) {
					dind[3] = ii;
					dind[4] = jj;
					m_dpdfjoint[dind] += tmp*fixweight[ii-binfix+m_krad];

				}
			}
		}
		}
		}
	}

	///////////////////////
	// Update Entropies
	///////////////////////

	// scale
	size_t tbins = m_bins*m_bins;
	double scale = 0;
	for(size_t ii=0; ii<tbins; ii++)
		scale += m_pdfjoint[ii];
	scale = 1./scale;
	for(size_t ii=0; ii<tbins; ii++)
		m_pdfjoint[ii] *= scale;

	// marginals
	for(int64_t ii=0, bb=0; ii<m_bins; ii++) {
		for(int64_t jj=0; jj<m_bins; jj++, bb++) {
			m_pdffix[ii] += m_pdfjoint[bb];
			m_pdfmove[jj] += m_pdfjoint[bb];
		}
	}

	//////////////////////////////
	// Update Gradient Entropies
	//////////////////////////////

	scale = -scale/m_wmove;
	size_t dpjsz = m_dpdfjoint.elements();
	for(size_t ii=0; ii<dpjsz; ii++)
		m_dpdfjoint[ii] *= scale;
	size_t dpmsz = m_dpdfmove.elements();
	for(size_t ii=0; ii<dpmsz; ii++)
		m_dpdfmove[ii] *= scale;

	size_t nparams = m_gradHmove.elements();

	CLOCK(c = clock() - c);
	CLOCK(cerr << "dPDF() Time: " << c << endl);
	CLOCK(c = clock());

	// update m_Hmove
	m_Hmove = 0;
	m_Hfix = 0;
	for(int ii=0; ii<m_bins; ii++) {
		m_Hmove -= m_pdfmove[ii] > 0 ? m_pdfmove[ii]*log(m_pdfmove[ii]) : 0;
		m_Hfix -= m_pdffix[ii] > 0 ? m_pdffix[ii]*log(m_pdffix[ii]) : 0;
	}

	// update m_Hjoint
	m_Hjoint = 0;
	for(int ii=0; ii<tbins; ii++)
		m_Hjoint -= m_pdfjoint[ii] > 0 ? m_pdfjoint[ii]*log(m_pdfjoint[ii]) : 0;


	// Compute Gradient Marginal Entropy
	m_gradHmove.zero();
	for(int64_t ii=0; ii<m_bins; ii++) {
		double p = m_pdfmove[ii];
		if(p <= 0)
			continue;
		double lp = (log(p)+1);
		for(int64_t pp=0; pp<nparams; pp++)
			m_gradHmove[pp] -= lp*m_dpdfmove[pp*m_bins + ii];
	}

	// Compute Gradient Joint Entropy
	m_gradHjoint.zero();
	for(size_t ii=0; ii<tbins; ii++) {
		double p = m_pdfjoint[ii];
		if(p <= 0)
			continue;
		double lp = (log(p)+1);
		for(size_t pp=0; pp<nparams; pp++)
			m_gradHjoint[pp] -= lp*m_dpdfjoint[pp*tbins + ii];
	}

	CLOCK(c = clock() - c);
	CLOCK(cerr << "dH() Time: " << c << endl);
	CLOCK(c = clock());

	// update value and grad
	if(m_metric == METRIC_MI) {
		size_t elements = m_deform->elements();
		val = m_Hfix+m_Hmove-m_Hjoint;
		// Fill Gradient From GradH Images
		for(size_t ii=0; ii<elements; ii++)
			grad[ii] = (m_gradHmove[ii]-m_gradHjoint[ii])*
				m_moving->spacing(m_dir);

	} else if(m_metric == METRIC_VI) {
		size_t elements = m_deform->elements();
		val = 2*m_Hjoint-m_Hfix-m_Hmove;
		for(size_t ii=0; ii<elements; ii++)
			grad[ii] = (2*m_gradHjoint[ii] - m_gradHmove[ii])*
				m_moving->spacing(m_dir);

	} else if(m_metric == METRIC_NMI) {
		size_t elements = m_deform->elements();
		val =  (m_Hfix+m_Hmove)/m_Hjoint;
		for(size_t ii=0; ii<elements; ii++)
			grad[ii] = m_moving->spacing(m_dir)*(m_gradHmove[ii]/m_Hjoint -
				m_gradHjoint[ii]*(m_Hfix+m_Hmove)/(m_Hjoint*m_Hjoint));
	}

	// negate if we are using a similarity measure
	if(m_compdiff && (m_metric == METRIC_NMI || m_metric == METRIC_MI)) {
		grad = -grad;
		val = -val;
	}

	return 0;
}

/**
 * @brief Computes the metric value
 *
 * @param val Output Value
 */
int DistCorrInfoComp::metric(double& val)
{
	// Views
	BSplineView<double> bsp_vw(m_deform);
	bsp_vw.m_boundmethod = ZEROFLUX;
	bsp_vw.m_ras = true;

	//Zero Inputs
	m_pdfmove.zero();
	m_pdffix.zero();
	m_pdfjoint.zero();

	// for computing distorted indices
	double cbinmove, cbinfix; //continuous
	int binmove, binfix; // nearest int
	double Fc;   /** Intensity Corrected Moving Value */
	double Ff;   /** Fixed Value Value */

	// Compute Updated Version of Distortion-Corrected Image
	CLOCK(auto c = clock());
	updateCaches();
	CLOCK(c = clock() - c);
	CLOCK(cerr << "Apply() Time: " << c << endl);
	CLOCK(c = clock());

	// Create Iterators/ Viewers
	NDConstIter<double> cit(m_corr_cache);
	NDConstIter<double> mit(m_move_cache);
	NDConstIter<double> fit(m_fixed);

	// compute updated moving width
	m_wmove = (m_rangemove[1]-m_rangemove[0])/(m_bins-2*m_krad-1);
	m_wfix = (m_rangefix[1]-m_rangefix[0])/(m_bins-2*m_krad-1);

	// Compute Probabilities
	for(cit.goBegin(), mit.goBegin(), fit.goBegin();
			!fit.eof(); ++cit, ++mit, ++fit) {
		// get actual values
		Fc = cit.get();
		Ff = fit.get();

		// compute bins
		cbinfix = (Ff-m_rangefix[0])/m_wfix + m_krad;
		binfix = round(cbinfix);
		cbinmove = (Fc-m_rangemove[0])/m_wmove + m_krad;
		binmove = round(cbinmove);

		/**************************************************************
		 * Compute Joint PDF
		 **************************************************************/
		// Sum up Bins for Value Comp
		for(int ii = binfix-m_krad; ii <= binfix+m_krad; ii++) {
			for(int jj = binmove-m_krad; jj <= binmove+m_krad; jj++) {
				m_pdfjoint[{ii,jj}] += B3kern(jj-cbinmove, m_krad)*
					B3kern(ii-cbinfix, m_krad);
			}
		}
	}

	///////////////////////
	// Update Entropies
	///////////////////////

	// Scale
	size_t tbins = m_bins*m_bins;
	double scale = 0;
	for(size_t ii=0; ii<tbins; ii++)
		scale += m_pdfjoint[ii];
	scale = 1./scale;
	for(size_t ii=0; ii<tbins; ii++)
		m_pdfjoint[ii] *= scale;

	// marginals
	for(int64_t ii=0, bb=0; ii<m_bins; ii++) {
		for(int64_t jj=0; jj<m_bins; jj++, bb++) {
			m_pdffix[ii] += m_pdfjoint[bb];
			m_pdfmove[jj] += m_pdfjoint[bb];
		}
	}

	// Scale Joint
	CLOCK(c = clock() - c);
	CLOCK(cerr << "PDF() Time: " << c << endl);
	CLOCK(c = clock());

	// Compute Marginal Entropy
	m_Hmove = 0;
	m_Hfix = 0;
	for(int ii=0; ii<m_bins; ii++) {
		m_Hmove -= m_pdfmove[ii] > 0 ? m_pdfmove[ii]*log(m_pdfmove[ii]) : 0;
		m_Hfix -= m_pdffix[ii] > 0 ? m_pdffix[ii]*log(m_pdffix[ii]) : 0;
	}

	// Compute Joint Entropy
	m_Hjoint = 0;
	size_t elem = m_pdfjoint.elements();
	for(int ii=0; ii<elem; ii++)
		m_Hjoint -= m_pdfjoint[ii] > 0 ? m_pdfjoint[ii]*log(m_pdfjoint[ii]) : 0;

	// update value and grad
	if(m_metric == METRIC_MI) {
		val = m_Hfix+m_Hmove-m_Hjoint;
	} else if(m_metric == METRIC_VI) {
		val = 2*m_Hjoint-m_Hfix-m_Hmove;
	} else if(m_metric == METRIC_NMI) {
		val =  (m_Hfix+m_Hmove)/m_Hjoint;
	}

	CLOCK(c = clock() - c);
	CLOCK(cerr << "H() Time: " << c << endl);
	CLOCK(c = clock());

	// negate if we are using a similarity measure
	if(m_compdiff && (m_metric == METRIC_NMI || m_metric == METRIC_MI))
		val = -val;

	return 0;
}


/**
 * @brief Computes the gradient and value of the correlation.
 *
 * @param x Paramters (Rx, Ry, Rz, Sx, Sy, Sz).
 * @param v Value at the given rotation
 * @param g Gradient at the given rotation
 *
 * @return 0 if successful
 */
int DistCorrInfoComp::valueGrad(const VectorXd& params,
		double& val, VectorXd& grad)
{
	/*
	 * Check That Everything Is Properly Initialized
	 */
	if(!m_fixed) throw INVALID_ARGUMENT("ERROR must set fixed image before "
				"computing value.");
	if(!m_moving) throw INVALID_ARGUMENT("ERROR must set moving image before "
				"computing value.");
	if(!m_moving->matchingOrient(m_fixed, true, true))
		throw INVALID_ARGUMENT("ERROR input images must be in same index "
				"space");

	assert(m_bins > m_krad*2);

	assert(m_pdfmove.dim(0) == m_bins);
	assert(m_pdfjoint.dim(0) == m_bins); // Moving
	assert(m_pdfjoint.dim(1) == m_bins); // Fixed

	assert(m_dpdfjoint.dim(0) == m_deform->dim(0));
	assert(m_dpdfjoint.dim(1) == m_deform->dim(1));
	assert(m_dpdfjoint.dim(2) == m_deform->dim(2));
	assert(m_dpdfjoint.dim(3) == m_bins); // Moving PDF
	assert(m_dpdfjoint.dim(4) == m_bins); // Fixed PDF

	assert(m_dpdfmove.dim(0) == m_deform->dim(0));
	assert(m_dpdfmove.dim(1) == m_deform->dim(1));
	assert(m_dpdfmove.dim(2) == m_deform->dim(2));
	assert(m_dpdfmove.dim(3) == m_bins); // Moving

	assert(m_deform->elements() == gradbuff.rows());

	// Fill Deform Image from params
	FlatIter<double> dit(m_deform);
	for(size_t ii=0; !dit.eof(); ++dit, ++ii)
		dit.set(params[ii]);

	/*
	 * Compute the Gradient and Value
	 */
	val  = 0;
	size_t nparam = m_deform->elements();
	grad.setZero();
	BSplineView<double> b_vw(m_deform);
	b_vw.m_boundmethod = ZEROFLUX;

	// Compute and add Thin Plate Spline
	if(m_tps_reg > 0) {
		val += m_tps_reg*b_vw.thinPlateEnergy(nparam,gradbuff.array().data());
		grad += m_tps_reg*gradbuff;
	}

	// Compute and add Jacobian
	if(m_jac_reg > 0) {
		val += m_jac_reg*b_vw.jacobianDet(m_dir,nparam,gradbuff.array().data());
		grad += m_jac_reg*gradbuff;
	}

	// Compute and add Metric
	double tmp = 0;
	if(metric(tmp, gradbuff) != 0) return -1;
	val += tmp;
	grad += gradbuff;

//#if defined DEBUG || defined VERYDEBUG
	cerr << "ValueGrad() = " << val << " / " << grad.norm() << endl;
//#endif

	return 0;
}


/**
 * @brief Computes the gradient of the correlation. Note that this
 * function just calls valueGrad because computing the
 * additional values are trivial
 *
 * @param params Paramters (Rx, Ry, Rz, Sx, Sy, Sz).
 * @param grad Gradient at the given rotation
 *
 * @return 0 if successful
 */
int DistCorrInfoComp::grad(const VectorXd& params,
		VectorXd& grad)
{
	double v = 0;
	return valueGrad(params, v, grad);
}

/**
 * @brief Computes the correlation.
 *
 * @param params Paramters (Rx, Ry, Rz, Sx, Sy, Sz).
 * @param val Value at the given rotation
 *
 * @return 0 if successful
 */
int DistCorrInfoComp::value(const VectorXd& params,
		double& val)
{
	/*************************************************************************
	 * Check State
	 ************************************************************************/
	if(!m_fixed) throw INVALID_ARGUMENT("ERROR must set fixed image before "
				"computing value.");
	if(!m_moving) throw INVALID_ARGUMENT("ERROR must set moving image before "
				"computing value.");
	if(!m_moving->matchingOrient(m_fixed, true, true))
		throw INVALID_ARGUMENT("ERROR input images must be in same index "
				"space");

	assert(m_bins > m_krad*2);

	assert(m_pdfmove.dim(0) == m_bins);
	assert(m_pdfjoint.dim(0) == m_bins); // Moving
	assert(m_pdfjoint.dim(1) == m_bins); // Fixed

	// Fill Parameter Image
	FlatIter<double> dit(m_deform);
	for(size_t ii=0; !dit.eof(); ++dit, ++ii)
		dit.set(params[ii]);

	/*
	 * Compute Value
	 */
	val = 0;
	BSplineView<double> b_vw(m_deform);
	b_vw.m_boundmethod = ZEROFLUX;

	// Compute and add Thin Plate Spline
	if(m_tps_reg > 0) {
		val += b_vw.thinPlateEnergy()*m_tps_reg;
	}

	// Compute and add Jacobian
	if(m_jac_reg > 0) {
		val += b_vw.jacobianDet(m_dir)*m_jac_reg;
	}

	// Compute and add Metric
	double tmp = 0;
	if(metric(tmp) != 0) return -1;
	val += tmp;

//#if defined DEBUG || defined VERYDEBUG
	cerr << "Value() = " << val << endl;
//#endif

	return 0;
};


/*********************************************************************
 * Rigid Transform Struct
 *********************************************************************/

/**
 * @brief Inverts rigid transform, where:
 *
 * Original:
 * y = R(x-c)+s+c
 *
 * Inverse:
 * x = R^-1(y - s - c) + c
 * x = R^-1(y - c) - R^-1 s + c
 *
 * So the new parameters, interms of the old are:
 * New c = c
 * New s = -R^-1 s
 * New R = R^-1
 *
 */
void Rigid3DTrans::invert()
{
	Matrix3d Q = rotMatrix().inverse();

	auto tmp_shift = shift;
	shift = -Q*tmp_shift;

	rotation = Q.eulerAngles(0,1,2);
}

/**
 * @brief Constructs and returns rotation Matrix.
 *
 * @return Rotation matrix
 */
Matrix3d Rigid3DTrans::rotMatrix()
{
	Matrix3d ret;
	ret = AngleAxisd(rotation[0], Vector3d::UnitX())*
		AngleAxisd(rotation[1], Vector3d::UnitY())*
		AngleAxisd(rotation[2], Vector3d::UnitZ());
	return ret;
};

/**
 * @brief Constructs rotation vector from rotation matrix
 *
 * @param rot Rotation matrix
 */
void Rigid3DTrans::setRotation(const Matrix3d& rot)
{
	rotation = rot.eulerAngles(0, 1, 2);
}

/**
 * @brief Converts to world coordinates based on the orientation stored in
 * input image.
 *
 * The image center is just converted from index space to RAS space.
 *
 * For a rotation in RAS coordinates, with orientation \f$A\f$, origin \f$b\f$
 * rotation \f$Q\f$, shift \f$t\f$ and center \f$d\f$:
 *
 * \f[
 *   \hat u = Q(A\hat x + \hat b - \hat d) + \hat t + \hat d
 * \f]
 *
 * \f{eqnarray*}{
 *      R &=& Rotation matrix in index space \\
 * \hat c &=& center of rotation in index space \\
 * \hat s &=& shift vector in index space \\
 *      Q &=& Rotation matrix in RAS (physical) space \\
 * \hat d &=& center of rotation in RAS (physical) space \\
 * \hat t &=& shift in index space \\
 *      A &=& Direction matrix of input image * spacing \\
 *      b &=& Origin of input image.
 * }
 *
 * From a rotation in index space
 * \f{eqnarray*}{
 *  Q &=& ARA^{-1} \\
 *  \hat t &=& -Q\hat b + Q\hat d - \hat d - AR\hat c + A\hat s + A\hat c + \hat b \\
 *  \f}
 *
 * @param in Source of index->world transform
 */
void Rigid3DTrans::toRASCoords(shared_ptr<const MRImage> in)
{
	if(ras_coord) {
		return;
	}

	ras_coord = true;

	Matrix3d R, Q, A;
	Vector3d c, s, d, t, b;

	////////////////////
  // Orientation
  ////////////////////
	A = in->getDirection(); // direction*spacing
	// premultiply direction matrix with spacing
	for(size_t rr=0; rr<A.rows(); rr++) {
		for(size_t cc=0; cc<A.cols(); cc++) {
			A(rr,cc) *= in->spacing(cc);
		}
	}
	b = in->getOrigin(); // origin

	////////////////////////
  // Index Space Rotation
  ////////////////////////
	R = AngleAxisd(rotation[0], Vector3d::UnitX())*
		AngleAxisd(rotation[1], Vector3d::UnitY())*
		AngleAxisd(rotation[2], Vector3d::UnitZ());
	s = shift; // shift in index space
	c = center; // center in index space

	////////////////////////
  // RAS Space Rotation
  ////////////////////////
	in->indexToPoint(3, c.data(), d.data()); // center
	Q = A*R*A.inverse(); // rotation in RAS

	// compute shift in RAS
	t = Q*(d-b)+A*(s+c-R*c)+b-d;

	shift = t;
	center = d;
	rotation[1] = asin(Q(0,2));
	rotation[2] = -Q(0,1)/cos(rotation[1]);
	rotation[0] = -Q(1,2)/cos(rotation[1]);
};

/**
 * @brief Converts from world coordinates to index coordinates based on the
 * orientation stored in input image.
 *
 * The image center is either converted from RAS space to index space or, if
 * forcegridcenter is true, then the center of rotation is set to the center of
 * the grid ((SIZE-1)/2 in each dimension).
 *
 * \f{eqnarray*}{
 *      R &=& Rotation matrix in index space \\
 * \hat c &=& center of rotation in index space \\
 * \hat s &=& shift vector in index space \\
 *      Q &=& Rotation matrix in RAS (physical) space \\
 * \hat d &=& center of rotation in RAS (physical) space \\
 * \hat t &=& shift in index space \\
 *      A &=& Direction matrix of input image * spacing \\
 *      b &=& Origin of input image.
 * }
 *
 * The Rotation (\f$R\f$) and Shift(\f$ \hat s \f$) are given by:
 * \f{eqnarray*}{
 * R &=& A^{-1}QA \\
 * \hat s &=& R\hat c -\hat c - A^{-1}(\hat b + Q\hat b - Q\hat d + \hat t + \hat d )
 * \f}
 *
 * where \f$A\f$ is the rotation of the grid, \f$\hat c\f$ is the center of the
 * grid, \f$\hat b \f$ is the origin of the grid, \f$ Q \f$ is the rotation
 * matrix in RAS coordinate space, \f$ \hat d \f$ is the given center of roation
 * (in RAS coordinates), and \f$ \hat t \f$ is the original shift in RAS
 * coordinates.
 *
 * @param in Source of index->world transform
 * @param forcegridcenter Force the center to be the center of the grid rather
 * than using the location corresponding to the current center
 */
void Rigid3DTrans::toIndexCoords(shared_ptr<const MRImage> in,
		bool forcegridcenter)
{
	if(!ras_coord) {
		throw INVALID_ARGUMENT("Rigid3DTrans is already in Index Coordinates");
		return;
	}

	ras_coord = false;

	Matrix3d R, Q, A;
	Vector3d c, s, d, t, b;

	////////////////////
  // Orientation
  ////////////////////
	A = in->getDirection(); // direction*spacing
	// premultiply direction matrix with spacing
	for(size_t rr=0; rr<A.rows(); rr++) {
		for(size_t cc=0; cc<A.cols(); cc++) {
			A(rr,cc) *= in->spacing(cc);
		}
	}
	b = in->getOrigin(); // origin

	////////////////////////
  // RAS Space Rotation
  ////////////////////////
	Q = AngleAxisd(rotation[0], Vector3d::UnitX())*
		AngleAxisd(rotation[1], Vector3d::UnitY())*
		AngleAxisd(rotation[2], Vector3d::UnitZ());
	t = shift; // shift in ras space
	d = center; // center in ras space

	////////////////////////
  // Index Space Rotation
  ////////////////////////
	if(forcegridcenter) {
		// make center the image center
		for(size_t dd=0; dd < 3; dd++)
			c[dd] = (in->dim(dd)-1.)/2.;
	} else {
		// change center to index space
		in->pointToIndex(3, d.data(), c.data());
	}

	R = A.inverse()*Q*A; // rotation in RAS

	// compute shift in RAS
	s = A.inverse()*(Q*(b+A*c-d) + t+d-b) - c;

	shift = s;
	center = c;
	rotation[1] = asin(R(0,2));
	rotation[2] = -R(0,1)/cos(rotation[1]);
	rotation[0] = -R(1,2)/cos(rotation[1]);
	//	rotation = R.eulerAngles(0,1,2);
};

}

