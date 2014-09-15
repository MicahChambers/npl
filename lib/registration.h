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

#ifndef REGISTRATION_H
#define REGISTRATION_H

#include "mrimage.h"
#include "accessors.h"
#include "iterators.h"
#include <Eigen/Dense>

#include <memory>

#define VERYDEBUG 1

using std::shared_ptr;

namespace npl {

/** \defgroup ImageRegistration Image registration algorithms
 *
 * Registration functions are implemented with classes linked to optimization
 * functions. All registration algorithms ultimately may be performed with
 * a simple function call, but the Computer classes (of which there is
 * currently just RigidCorrComputer) are exposed in case you want to use them
 * for your own registration algorithms.
 *
 * A computer is needed for every pair of Metric and Transform type.
 *
 */
 
/** @{ */

/**
 * @brief The Rigid Corr Computer is used to compute the correlation
 * and gradient of correlation between two images. As the name implies, it 
 * is designed for 6 parameter rigid transforms.
 *
 * Note that it computes the NEGATIVE of the correlation, and gradient,
 * so that most gradient descent methods will work.
 */
class RigidCorrComputer
{
    public:

    /**
     * @brief Constructor for the rigid correlation class. Note that 
     * rigid rotation is assumed to be about the center of the fixed 
     * image space. If necessary the input moving image will be resampled.
     * To the same space as the fixed image.
     *
     * @param fixed Fixed image. A copy of this will be made.
     * @param moving Moving image. A copy of this will be made.
     * @param negate Whether to use negative correlation (for instance to
     * minimize negative correlation using a gradient descent).
     */
    RigidCorrComputer(shared_ptr<const MRImage> fixed,
            shared_ptr<const MRImage> moving, bool negate);

    /**
     * @brief Computes the gradient and value of the correlation. 
     *
     * @param params Paramters (Rx, Ry, Rz, Sx, Sy, Sz).
     * @param val Value at the given rotation
     * @param grad Gradient at the given rotation
     *
     * @return 0 if successful
     */
    int valueGrad(const Eigen::VectorXd& params, double& val, 
            Eigen::VectorXd& grad);
    
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
    int grad(const Eigen::VectorXd& params, Eigen::VectorXd& grad);

    /**
     * @brief Computes the correlation. 
     *
     * @param params Paramters (Rx, Ry, Rz, Sx, Sy, Sz).
     * @param value Value at the given rotation
     *
     * @return 0 if successful
     */
    int value(const Eigen::VectorXd& params, double& val);

    private:

    shared_ptr<MRImage> m_fixed;
    shared_ptr<MRImage> m_moving;
    shared_ptr<MRImage> m_dmoving;

    // for interpolating moving image, and iterating fixed
    LinInterp3DView<double> m_move_get;
    LinInterp3DView<double> m_dmove_get;
    NDConstIter<double> m_fit;

	double m_center[3];
    
#ifdef VERYDEBUG
    shared_ptr<MRImage> d_theta_x;
    shared_ptr<MRImage> d_theta_y;
    shared_ptr<MRImage> d_theta_z;
    shared_ptr<MRImage> d_shift_x;
    shared_ptr<MRImage> d_shift_y;
    shared_ptr<MRImage> d_shift_z;
    shared_ptr<MRImage> interpolated;
    int callcount;
#endif

    /**
     * @brief Negative of correlation (which will make it work with most
     * optimizers)
     */
    bool m_negate;

};

/**
 * @brief Struct for holding information about a rigid transform. Note that
 * rotation R = Rx*Ry*Rz, where Rx, Ry, and Rz are the rotations about x, y and
 * z aaxes, and the angles are stored (in radians) in the rotation member. 
 *
 * \f$ \hat y = R(\hat x- \hat c)+ \hat s+ \hat c \f$
 *
 */
struct Rigid3DTrans
{
    Vector3d rotation; //Rx, Ry, Rz
    Vector3d shift;
    Vector3d center;
   
    /**
     * @brief Indicates the stored transform is relative to physical coordintes
     * rather than index coordinates
     */
    bool ras_coord;

    Rigid3DTrans() {
        ras_coord = false;
        rotation.setZero();
        shift.setZero();
        center.setZero();
    };

    /**
     * @brief Inverts rigid transform, where: 
     *
     * Original:
     * \f$ \hat y = R(\hat x- \hat c)+ \hat s+ \hat c \f$
     *
     * Inverse:
     * \f$ \hat x = R^{-1}(\hat y - \hat s - \hat c) + \hat c \f$
     *
     * So the new parameters, interms of the old are:
     * \f[ \hat c' = \hat s+ \hat c \f]
     * \f[ \hat s' = -\hat s \f]
     * \f[ \hat R' = R^{-1} \f]
     * 
     */
    void invert();

    /**
     * @brief Constructs and returns rotation Matrix.
     *
     * @return Rotation matrix
     */
    Matrix3d rotMatrix();

    /**
     * @brief Converts to world coordinates based on the orientation stored in
     * input image.
     *
     * For a rotation in RAS coordinates, with rotation \f$Q\f$, shift \f$t\f$ and
     * center \f$d\f$:
     *
     * \f[
     *   \hat u = Q(A\hat x + \hat b - \hat d) + \hat t + \hat d 
     * \f]
     *
     * From a rotation in index space with rotation \f$R\f$, shift \f$s\f$ and
     * center \f$c\f$:
     * \f{eqnarray*}{
     *  Q &=& A^{-1}AR \\
     *  \hat t &=& -Q\hat b + Q\hat d - \hat d - AR\hat c + A\hat s + A\hat c + \hat b \\
     * \f}
     *
     * @param in Source of index->world transform
     */
    void toRASCoords(shared_ptr<const MRImage> in);
    
    /**
     * @brief Converts from world coordinates to index coordinates based on the
     * orientation stored in input image.
     *
     * The center of rotation is assumed to be the center of the grid which is 
     * (SIZE-1)/2 in each dimension.
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
     * @param forcegridcenter Force the center to be the center of the grid
     * rather than using the location corresponding to the current center 
     */
    void toIndexCoords(shared_ptr<const MRImage> in, bool forcegridcenter);

};

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
shared_ptr<MRImage> motionCorrect(shared_ptr<const MRImage> input, size_t ref);

/**
 * @brief Performs correlation based registration between two 3D volumes. note
 * that the two volumes should have identical sampling and identical
 * orientation. If that is not the case, an exception will be thrown.
 *
 * @param fixed     Image which will be the target of registration. 
 * @param moving    Image which will be rotated then shifted to match fixed.
 * @param sigmas    Standard deviation of smoothing kernel at each level
 *
 * @return          Rigid transform.
 */
Rigid3DTrans corReg3D(shared_ptr<const MRImage> fixed, 
        shared_ptr<const MRImage> moving, const std::vector<double>& sigmas);


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
int cor3DDerivTest(double step, double tol, shared_ptr<const MRImage> in1,
        shared_ptr<const MRImage> in2);


/**
 * @brief Prints a rigid transform
 *
 * @param stream Output stream
 * @param rigid Rigid transform
 *
 * @return after this is inserted, stream
 */
std::ostream& operator<< (std::ostream& stream, const Rigid3DTrans& rigid)
{
    stream << "Rigid3DTransform ";
    if(rigid.ras_coord)
        stream << "(In RAS)\n";
    else 
        stream << "(In Index)\n";

    stream << "Rotation: ";
    for(size_t ii=0; ii<2; ii++)
        stream << rigid.rotation[ii] << ", ";
    stream << rigid.rotation[2] << "\n";

    stream << "Center: ";
    for(size_t ii=0; ii<2; ii++)
        stream << rigid.center[ii] << ", ";
    stream << rigid.center[2] << "\n";

    stream << "Shift : ";
    for(size_t ii=0; ii<2; ii++)
        stream << rigid.shift[ii] << ", ";
    stream << rigid.shift[2] << "\n";

    return stream;
}

/** @} */

} // npl
#endif  //REGISTRATION_H


