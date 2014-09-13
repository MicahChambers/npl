/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file registration.h Tools for registering 2D or 3D images.
 *
 *****************************************************************************/

#include "registration.h"
#include "lbfgs.h"
#include "mrimage_utils.h"
#include "ndarray_utils.h"
#include "macros.h"

#include <memory>
#include <stdexcept>
#include <functional>

#include <Eigen/Dense>

using std::dynamic_pointer_cast;

using Eigen::VectorXd;
using Eigen::Vector3d;
using Eigen::Matrix3d;
using Eigen::Matrix4d;
using Eigen::AngleAxisd;

using std::cerr;
using std::endl;

#ifdef VERYDEBUG
#define DEBUGWRITE(FOO) FOO 
#else
#define DEBUGWRITE(FOO) 
#endif

namespace npl {

/**
 * @brief Performs motion correction on a set of volumes. Each 3D volume is
 * extracted and linearly registered with the ref volume.
 *
 * @param input 3+D volume (set of 3D volumes, all higher dimensions are
 *              treated equally as separate volumes).
 * @param ref   Reference t, all images will be registered to the specified
 *              timepoint
 *
 * @return      Motion corrected volume.
 */
shared_ptr<MRImage> motionCorrect(shared_ptr<const MRImage> input, size_t ref)
{
 
    (void)input;
    (void)ref;
    return NULL;
};


/*********************************************************************
 * Registration Class Implementations
 ********************************************************************/

/**
 * @brief Constructor for the rigid correlation class. Note that 
 * rigid rotation is assumed to be about the center of the fixed 
 * image space. If necessary the input moving image will be resampled.
 * To the same space as the fixed image.
 *
 * @param fixed Fixed image. A copy of this will be made.
 * @param moving Moving image. A copy of this will be made.
 */
RigidCorrComputer::RigidCorrComputer(
        shared_ptr<const MRImage> fixed, shared_ptr<const MRImage> moving) :
    m_fixed(dynamic_pointer_cast<MRImage>(fixed->copy())),
    m_moving(dynamic_pointer_cast<MRImage>(moving->copy())),
    m_dmoving(dynamic_pointer_cast<MRImage>(derivative(moving))),
    m_move_get(m_moving),
    m_dmove_get(m_dmoving),
    m_fit(m_fixed)
{
    if(fixed->ndim() != 3)
        throw INVALID_ARGUMENT("Fixed image is not 3D!");
    if(moving->ndim() != 3)
        throw INVALID_ARGUMENT("Moving image is not 3D!");

#ifdef VERYDEBUG
    d_theta_x = dynamic_pointer_cast<MRImage>(moving->copy());
    d_theta_y = dynamic_pointer_cast<MRImage>(moving->copy());
    d_theta_z = dynamic_pointer_cast<MRImage>(moving->copy());
    d_shift_x = dynamic_pointer_cast<MRImage>(moving->copy());
    d_shift_y = dynamic_pointer_cast<MRImage>(moving->copy());
    d_shift_z = dynamic_pointer_cast<MRImage>(moving->copy());
    interpolated = dynamic_pointer_cast<MRImage>(fixed->copy());
#endif
	
    for(size_t ii=0; ii<3 && ii<moving->ndim(); ii++) 
		m_center[ii] = (m_moving->dim(ii)-1)/2.;
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
int RigidCorrComputer::valueGrad(const VectorXd& params, 
        double& val, VectorXd& grad)
{
    double rx = params[0];
    double ry = params[1];
    double rz = params[2];
    double sx = params[3];
    double sy = params[4];
    double sz = params[5];

#ifdef VERYDEBUG
    Pixel3DView<double> d_ang_x(d_theta_x);
    Pixel3DView<double> d_ang_y(d_theta_y);
    Pixel3DView<double> d_ang_z(d_theta_z);
    Pixel3DView<double> d_shi_x(d_shift_x);
    Pixel3DView<double> d_shi_y(d_shift_y);
    Pixel3DView<double> d_shi_z(d_shift_z);
#endif

    // for computing roted indices
	double ind[3];
	double cind[3];

    //  Compute derivative Images (if debugging is enabled, otherwise just
    //  compute the gradient)
    grad.setZero();
    size_t count = 0;
    double mov_sum = 0;
    double fix_sum = 0;
    double mov_ss = 0;
    double fix_ss = 0;
    double corr = 0;
    for(m_fit.goBegin(); !m_fit.eof(); ++m_fit) {
		m_fit.index(3, ind);
        // u = c + R^-1(v - s - c)
        // where u is the output index, v the input, c the center of rotation
        // and s the shift
		// cind = center + rInv*(ind-shift-center); 
        
        double x = ind[0];
        double y = ind[1];
        double z = ind[2];

        double cx = m_center[0];
        double cy = m_center[1];
        double cz = m_center[2];
        
        cind[0] = cx - (cx + sx - x)*cos(ry)*cos(rz) + cos(rz)*((cz + sz -
                    z)*cos(rx) - (cy + sy - y)*sin(rx))*sin(ry) - ((cy + sy -
                    y)*cos(rx) + (cz + sz - z)*sin(rx))*sin(rz);
        cind[1] = cy + cos(rz)*((-cy - sy + y)*cos(rx) - (cz + sz - z)*sin(rx))
                    + (cx + sx - x)*cos(ry)*sin(rz) + ((-cz - sz + z)*cos(rx) +
                    (cy + sy - y)*sin(rx))*sin(ry)*sin(rz);
        cind[2] = cz - (cz + sz - z)*cos(rx)*cos(ry) + (cy + sy -
                    y)*cos(ry)*sin(rx) - (cx + sx - x)*sin(ry);

        // Here we compute dg(v(u,p))/dp, where g is the image, u is the
        // coordinate in the fixed image, and p is the param. 
        // dg/dp = SUM_i dg/dv_i dv_i/dp, where v is the rotated coordinate, so
        // dg/dv_i is the directional derivative in original space,
        // dv_i/dp is the derivative of the rotated coordinate system with
        // respect to a parameter
        double dg_dx = m_dmove_get(cind[0], cind[1], cind[2], 0);
        double dg_dy = m_dmove_get(cind[0], cind[1], cind[2], 1);
        double dg_dz = m_dmove_get(cind[0], cind[1], cind[2], 2);

        double dx_dRx = -(cos(rz)*((cy + sy - y)*cos(rx) + (cz + sz -
                    z)*sin(rx))*sin(ry)) + ((-cz - sz + z)*cos(rx) + (cy +
                    sy - y)*sin(rx))*sin(rz);
        double dy_dRx =  cos(rz)*((-cz - sz + z)*cos(rx) + (cy + sy -
                    y)*sin(rx)) + ((cy + sy - y)*cos(rx) + (cz + sz -
                    z)*sin(rx))*sin(ry)*sin(rz);
        double dz_dRx = cos(ry)*((cy + sy - y)*cos(rx) + (cz + sz -
                    z)*sin(rx));

        double dx_dRy = cos(rz)*((cz + sz - z)*cos(rx)*cos(ry) - (cy + sy -
                    y)*cos(ry)*sin(rx) + (cx + sx - x)*sin(ry));
        double dy_dRy = (-((cz + sz - z)*cos(rx)*cos(ry)) + (cy + sy -
                    y)*cos(ry)*sin(rx) - (cx + sx - x)*sin(ry))*sin(rz);
        double dz_dRy = (-cx - sx + x)*cos(ry) + ((cz + sz - z)*cos(rx) - (cy +
                    sy - y)*sin(rx))*sin(ry);

        double dx_dRz = (-cz - sz + z)*cos(rz)*sin(rx) + ((cx + sx - x)*cos(ry)
                    + (cy + sy - y)*sin(rx)*sin(ry))*sin(rz) - cos(rx)*((cy +
                    sy - y)*cos(rz) + (cz + sz - z)*sin(ry)*sin(rz));
        double dy_dRz = (cx + sx - x)*cos(ry)*cos(rz) + cos(rz)*((-cz - sz +
                    z)*cos(rx) + (cy + sy - y)*sin(rx))*sin(ry) + ((cy + sy -
                    y)*cos(rx) + (cz + sz - z)*sin(rx))*sin(rz);
        double dz_dRz = 0;

        // derivative of coordinate system due 
        double dx_dSx = -(cos(ry)*cos(rz));
        double dy_dSx = cos(ry)*sin(rz);
        double dz_dSx = -sin(ry);
        
        double dx_dSy = -(cos(rz)*sin(rx)*sin(ry)) - cos(rx)*sin(rz);
        double dy_dSy = -(cos(rx)*cos(rz)) + sin(rx)*sin(ry)*sin(rz);
        double dz_dSy = cos(ry)*sin(rx);
        
        double dx_dSz = cos(rx)*cos(rz)*sin(ry) - sin(rx)*sin(rz);
        double dy_dSz = -(cos(rz)*sin(rx)) - cos(rx)*sin(ry)*sin(rz);
        double dz_dSz = -(cos(rx)*cos(ry));

        // compute SUM_i dg/dv_i dv_i/dp
        double dCdRx = dg_dx*dx_dRx + dg_dy*dy_dRx + dg_dz*dz_dRx;
        double dCdRy = dg_dx*dx_dRy + dg_dy*dy_dRy + dg_dz*dz_dRy;
        double dCdRz = dg_dx*dx_dRz + dg_dy*dy_dRz + dg_dz*dz_dRz;

        double dCdSx = dg_dx*dx_dSx + dg_dy*dy_dSx + dg_dz*dz_dSx;
        double dCdSy = dg_dx*dx_dSy + dg_dy*dy_dSy + dg_dz*dz_dSy;
        double dCdSz = dg_dx*dx_dSz + dg_dy*dy_dSz + dg_dz*dz_dSz;
        
        // compute correlation, since it requires almost no additional work
        double g = m_move_get(cind[0], cind[1], cind[2]);
        double f = *m_fit;
        
        mov_sum += g;
        fix_sum += f;
        mov_ss += g*g;
        fix_ss += f*f;
        corr += g*f;

#ifdef VERYDEBUG
        d_ang_x.set(dCdRx, ind[0], ind[1], ind[2]);
        d_ang_y.set(dCdRy, ind[0], ind[1], ind[2]);
        d_ang_z.set(dCdRz, ind[0], ind[1], ind[2]);
        d_shi_x.set(dCdSx, ind[0], ind[1], ind[2]);
        d_shi_y.set(dCdSy, ind[0], ind[1], ind[2]);
        d_shi_z.set(dCdSz, ind[0], ind[1], ind[2]);
#endif
     
        grad[0] += (*m_fit)*dCdRx;
        grad[1] += (*m_fit)*dCdRy;
        grad[2] += (*m_fit)*dCdRz;
        grad[3] += (*m_fit)*dCdSx;
        grad[4] += (*m_fit)*dCdSy;
        grad[5] += (*m_fit)*dCdSz;
     
    }

    count = m_fixed->elements();
    val = sample_corr(count, mov_sum, fix_sum, mov_ss, fix_sum, corr);
    double sd1 = sqrt(sample_var(count, mov_sum, mov_ss));
    double sd2 = sqrt(sample_var(count, fix_sum, fix_ss));
    grad /= (count-1)*sd1*sd2;

#ifdef VERYDEBUG
    d_theta_x->write("d_theta_x.nii.gz");
    d_theta_y->write("d_theta_y.nii.gz");
    d_theta_z->write("d_theta_z.nii.gz");
    d_shift_x->write("d_shift_x.nii.gz");
    d_shift_y->write("d_shift_y.nii.gz");
    d_shift_z->write("d_shift_z.nii.gz");
#endif

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
int RigidCorrComputer::grad(const VectorXd& params, VectorXd& grad)
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
int RigidCorrComputer::value(const VectorXd& params, double& val)
{
#ifdef VERYDEBUG
    Pixel3DView<double> acc(interpolated);
#endif 

    assert(fixed->ndim() == 3);
    assert(moving->ndim() == 3);

    double rx = params[0];
    double ry = params[1];
    double rz = params[2];
    double sx = params[3];
    double sy = params[4];
    double sz = params[5];

	double ind[3];
	double cind[3];

    //  Resample Output and Compute Orientation. While actually resampling is
    //  optional, it helps with debugging
    double sum1 = 0;
    double sum2 = 0;
    double ss1 = 0;
    double ss2 = 0;
    double corr = 0;
	for(m_fit.goBegin(); !m_fit.eof(); ++m_fit) {
		m_fit.index(3, ind);

        // u = c + R^-1(v - s - c)
        // where u is the output index, v the input, c the center of rotation
        // and s the shift
        double x = ind[0];
        double y = ind[1];
        double z = ind[2];

        double cx = m_center[0];
        double cy = m_center[1];
        double cz = m_center[2];
        
        cind[0] = cx - (cx + sx - x)*cos(ry)*cos(rz) + cos(rz)*((cz + sz -
                    z)*cos(rx) - (cy + sy - y)*sin(rx))*sin(ry) - ((cy + sy -
                    y)*cos(rx) + (cz + sz - z)*sin(rx))*sin(rz);
        cind[1] = cy + cos(rz)*((-cy - sy + y)*cos(rx) - (cz + sz - z)*sin(rx))
                    + (cx + sx - x)*cos(ry)*sin(rz) + ((-cz - sz + z)*cos(rx) +
                    (cy + sy - y)*sin(rx))*sin(ry)*sin(rz);
        cind[2] = cz - (cz + sz - z)*cos(rx)*cos(ry) + (cy + sy -
                    y)*cos(ry)*sin(rx) - (cx + sx - x)*sin(ry);
        
        double a = m_move_get(cind[0], cind[1], cind[2]);
#ifdef VERYDEBUG
        acc.set(a, ind[0], ind[1], ind[2]);
#endif
        double b = *m_fit;
        sum1 += a;
        ss1 += a*a;
        sum2 += b;
        ss2 += b*b;
        corr += a*b;
    }

    val = sample_corr(m_fixed->elements(), sum1, sum2, ss1, ss2, corr);
    return 0;
};


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
    RigidCorrComputer comp(in1, in2);

    auto vfunc = std::bind(&RigidCorrComputer::value, &comp, _1, _2);
    auto gfunc = std::bind(&RigidCorrComputer::grad, &comp, _1, _2);

    double error = 0;
    VectorXd x = VectorXd::Ones(6);
    if(testgrad(error, x, step, tol, vfunc, gfunc) != 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief Performs correlation based registration between two 3D volumes. note
 * that the two volumes should have identical sampling and identical
 * orientation. If that is not the case, an exception will be thrown.
 *
 * \todo make it v = Ru + s, then u = INV(R)*(v - s)
 *
 * @param fixed     Image which will be the target of registration. 
 * @param moving    Image which will be rotated then shifted to match fixed.
 *
 * @return          4x4 Matrix, indicating rotation about the center then 
 *                  shift. Rotation matrix is the first 3x3 and shift is the
 *                  4th column.
 */
Eigen::Matrix4d corReg3D(shared_ptr<const MRImage> fixed, 
        shared_ptr<const MRImage> moving)
{
    using namespace std::placeholders;
    using std::bind;

    Matrix4d output;

    // make sure the input image has matching properties
    if(!fixed->matchingOrient(moving, true))
        throw std::invalid_argument("Input images have mismatching pixels "
                "in\n" + __FUNCTION_STR__);

    std::vector<double> sigma_schedule({3, 2, 1});
    for(size_t ii=0; ii<sigma_schedule.size(); ii++) {
        // smooth and downsample input images
        auto sm_fixed = smoothDownsample(fixed, sigma_schedule[ii]);
        auto sm_moving = smoothDownsample(moving, sigma_schedule[ii]);
        DEBUGWRITE(sm_fixed->write("smooth_fixed.nii.gz"));
        DEBUGWRITE(sm_moving->write("smooth_moving.nii.gz"));

        RigidCorrComputer comp(sm_fixed, sm_moving);
        
        // create value and gradient functions
        auto vfunc = bind(&RigidCorrComputer::value, &comp, _1, _2);
        auto vgfunc = bind(&RigidCorrComputer::valueGrad, &comp, _1, _2, _3);
        auto gfunc = bind(&RigidCorrComputer::grad, &comp, _1, _2);

        // initialize optimizer
        LBFGSOpt opt(6, vfunc, gfunc, vgfunc);
        opt.state_x = VectorXd::Zero(6);
        opt.stop_Its = 10000;
        opt.stop_X = 0;
        opt.stop_G = 0.0000000001;
        StopReason stopr = opt.optimize();
        cerr << Optimizer::explainStop(stopr) << endl;
    }

    // TODO set output
    output.setZero();
    return output; 
};

}

