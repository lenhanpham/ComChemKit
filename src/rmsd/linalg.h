/**
 * @file linalg.h
 * @brief Minimal 3x3/4x4 linear algebra replacing nalgebra.
 * @author Le Nhan Pham
 * @date 2026
 */

#ifndef RMSD_LINALG_H
#define RMSD_LINALG_H

#include "rmsd/rmsd_types.h"

namespace Rmsd
{

Mat3 mat3_identity();
Mat3 mat3_transpose(const Mat3& a);
Mat3 mat3_mul(const Mat3& a, const Mat3& b);
double mat3_det(const Mat3& a);
double mat3_trace(const Mat3& a);

/**
 * @brief Cyclic Jacobi eigendecomposition of symmetric 3x3.
 * @param a Input symmetric matrix (not modified).
 * @param v Output eigenvectors as columns.
 * @param w Output eigenvalues.
 */
void jacobi_eigen3(const Mat3& a, Mat3& v, double w[3]);

/**
 * @brief Cyclic Jacobi eigendecomposition of symmetric 4x4.
 */
void jacobi_eigen4(const Mat4& a, Mat4& v, double w[4]);

/**
 * @brief 3x3 SVD via one-sided Jacobi: C = U * diag(S) * Vt.
 * @param c Input matrix.
 * @param u Output left singular vectors (columns).
 * @param s Output singular values (>= 0).
 * @param vt Output V-transpose.
 */
void svd3(const Mat3& c, Mat3& u, double s[3], Mat3& vt);

/** @brief Eigenvectors of symmetric 3x3, columns ascending by eigenvalue. */
Mat3 sorted_eigenvectors3(const Mat3& m);

/** @brief Apply row-vector rotation p @ u. */
inline Coord apply_mat_row(const Coord& p, const Mat3& u)
{
    Coord r;
    r[0] = p[0] * u[0][0] + p[1] * u[1][0] + p[2] * u[2][0];
    r[1] = p[0] * u[0][1] + p[1] * u[1][1] + p[2] * u[2][1];
    r[2] = p[0] * u[0][2] + p[1] * u[1][2] + p[2] * u[2][2];
    return r;
}

} // namespace Rmsd

#endif // RMSD_LINALG_H
