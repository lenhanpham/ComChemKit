/**
 * @file linalg.cpp
 * @brief Minimal linear algebra: Jacobi eigen + one-sided Jacobi SVD3.
 * @author Le Nhan Pham
 * @date 2026
 */

#include "rmsd/linalg.h"
#include <algorithm>
#include <cmath>

namespace Rmsd
{

Mat3 mat3_identity()
{
    Mat3 m{};
    m[0][0] = 1.0;
    m[1][1] = 1.0;
    m[2][2] = 1.0;
    return m;
}

Mat3 mat3_transpose(const Mat3& a)
{
    Mat3 t{};
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            t[i][j] = a[j][i];
        }
    }
    return t;
}

Mat3 mat3_mul(const Mat3& a, const Mat3& b)
{
    Mat3 c{};
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            c[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
        }
    }
    return c;
}

double mat3_det(const Mat3& a)
{
    return a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
         - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
         + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
}

double mat3_trace(const Mat3& a)
{
    return a[0][0] + a[1][1] + a[2][2];
}

void jacobi_eigen3(const Mat3& a, Mat3& v, double w[3])
{
    Mat3 d = a;
    v = mat3_identity();
    for (int sweep = 0; sweep < 50; ++sweep)
    {
        double off = d[0][1] * d[0][1] + d[0][2] * d[0][2] + d[1][2] * d[1][2];
        if (off < 1e-30)
        {
            break;
        }
        for (int p = 0; p < 2; ++p)
        {
            for (int q = p + 1; q < 3; ++q)
            {
                double apq = d[p][q];
                if (std::fabs(apq) < 1e-18)
                {
                    continue;
                }
                double app = d[p][p];
                double aqq = d[q][q];
                double theta = 0.5 * (aqq - app) / apq;
                double t = (theta >= 0 ? 1.0 : -1.0) / (std::fabs(theta) + std::sqrt(1.0 + theta * theta));
                double c = 1.0 / std::sqrt(1.0 + t * t);
                double s = c * t;
                for (int k = 0; k < 3; ++k)
                {
                    double dkp = d[k][p];
                    double dkq = d[k][q];
                    d[k][p] = c * dkp - s * dkq;
                    d[k][q] = s * dkp + c * dkq;
                }
                for (int k = 0; k < 3; ++k)
                {
                    double dpk = d[p][k];
                    double dqk = d[q][k];
                    d[p][k] = c * dpk - s * dqk;
                    d[q][k] = s * dpk + c * dqk;
                }
                for (int k = 0; k < 3; ++k)
                {
                    double vkp = v[k][p];
                    double vkq = v[k][q];
                    v[k][p] = c * vkp - s * vkq;
                    v[k][q] = s * vkp + c * vkq;
                }
            }
        }
    }
    w[0] = d[0][0];
    w[1] = d[1][1];
    w[2] = d[2][2];
}

void jacobi_eigen4(const Mat4& a, Mat4& v, double w[4])
{
    Mat4 d = a;
    v = {};
    for (int i = 0; i < 4; ++i)
    {
        v[i][i] = 1.0;
    }
    for (int sweep = 0; sweep < 60; ++sweep)
    {
        double off = 0.0;
        for (int p = 0; p < 4; ++p)
        {
            for (int q = p + 1; q < 4; ++q)
            {
                off += d[p][q] * d[p][q];
            }
        }
        if (off < 1e-28)
        {
            break;
        }
        for (int p = 0; p < 3; ++p)
        {
            for (int q = p + 1; q < 4; ++q)
            {
                double apq = d[p][q];
                if (std::fabs(apq) < 1e-18)
                {
                    continue;
                }
                double app = d[p][p];
                double aqq = d[q][q];
                double theta = 0.5 * (aqq - app) / apq;
                double t = (theta >= 0 ? 1.0 : -1.0) / (std::fabs(theta) + std::sqrt(1.0 + theta * theta));
                double c = 1.0 / std::sqrt(1.0 + t * t);
                double s = c * t;
                for (int k = 0; k < 4; ++k)
                {
                    double dkp = d[k][p];
                    double dkq = d[k][q];
                    d[k][p] = c * dkp - s * dkq;
                    d[k][q] = s * dkp + c * dkq;
                }
                for (int k = 0; k < 4; ++k)
                {
                    double dpk = d[p][k];
                    double dqk = d[q][k];
                    d[p][k] = c * dpk - s * dqk;
                    d[q][k] = s * dpk + c * dqk;
                }
                for (int k = 0; k < 4; ++k)
                {
                    double vkp = v[k][p];
                    double vkq = v[k][q];
                    v[k][p] = c * vkp - s * vkq;
                    v[k][q] = s * vkp + c * vkq;
                }
            }
        }
    }
    for (int i = 0; i < 4; ++i)
    {
        w[i] = d[i][i];
    }
}

void svd3(const Mat3& c, Mat3& u, double s[3], Mat3& vt)
{
    // SVD via eigendecomposition of symmetric C^T*C (robust for rank-deficient
    // inputs such as 2 collinear atoms, where one-sided Jacobi leaves a
    // non-orthonormal U). V columns are orthonormal eigenvectors; U columns
    // follow as C*v/s with cross-product completion of the null space.
    Mat3 ct{};
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            ct[i][j] = c[0][i] * c[0][j] + c[1][i] * c[1][j] + c[2][i] * c[2][j];
        }
    }
    Mat3 v;
    double w[3];
    jacobi_eigen3(ct, v, w);
    for (int i = 0; i < 3; ++i)
    {
        if (w[i] < 0.0 && w[i] > -1e-18)
        {
            w[i] = 0.0;
        }
        if (w[i] < 0.0)
        {
            w[i] = 0.0;
        }
    }
    int idx[3] = {0, 1, 2};
    std::sort(idx, idx + 3, [&](int a, int b) { return w[a] > w[b]; });
    Mat3 vsorted{};
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            vsorted[i][j] = v[i][idx[j]];
        }
    }
    for (int j = 0; j < 3; ++j)
    {
        s[j] = std::sqrt(w[idx[j]]);
    }
    double s0 = s[0];
    double tol = 1e-12 * (s0 > 1.0 ? s0 : 1.0);
    bool valid[3] = {s[0] > tol, s[1] > tol, s[2] > tol};
    u = {};
    for (int j = 0; j < 3; ++j)
    {
        if (!valid[j])
        {
            continue;
        }
        double ux = c[0][0] * vsorted[0][j] + c[0][1] * vsorted[1][j] + c[0][2] * vsorted[2][j];
        double uy = c[1][0] * vsorted[0][j] + c[1][1] * vsorted[1][j] + c[1][2] * vsorted[2][j];
        double uz = c[2][0] * vsorted[0][j] + c[2][1] * vsorted[1][j] + c[2][2] * vsorted[2][j];
        u[0][j] = ux / s[j];
        u[1][j] = uy / s[j];
        u[2][j] = uz / s[j];
    }
    int nvalid = (valid[0] ? 1 : 0) + (valid[1] ? 1 : 0) + (valid[2] ? 1 : 0);    auto cross = [](const double a[3], const double b[3], double o[3]) {
        o[0] = a[1] * b[2] - a[2] * b[1];
        o[1] = a[2] * b[0] - a[0] * b[2];
        o[2] = a[0] * b[1] - a[1] * b[0];
    };
    auto norm3 = [](double a[3]) { return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); };
    if (nvalid == 0)
    {
        u = mat3_identity();
    }
    else if (nvalid == 1)
    {
        int g = valid[0] ? 0 : (valid[1] ? 1 : 2);
        double a0[3] = {u[0][g], u[1][g], u[2][g]};
        double e[3] = {1, 0, 0};
        double ax = std::fabs(a0[0]);
        if (std::fabs(a0[1]) > ax)
        {
            ax = std::fabs(a0[1]);
            e[0] = 0;
            e[1] = 1;
            e[2] = 0;
        }
        if (std::fabs(a0[2]) > ax)
        {
            e[0] = 0;
            e[1] = 0;
            e[2] = 1;
        }
        double d = e[0] * a0[0] + e[1] * a0[1] + e[2] * a0[2];
        double t[3] = {e[0] - d * a0[0], e[1] - d * a0[1], e[2] - d * a0[2]};
        double n = norm3(t);
        t[0] /= n;
        t[1] /= n;
        t[2] /= n;
        double wv[3];
        cross(a0, t, wv);
        int o1 = (g + 1) % 3, o2 = (g + 2) % 3;
        u[0][o1] = t[0];
        u[1][o1] = t[1];
        u[2][o1] = t[2];
        u[0][o2] = wv[0];
        u[1][o2] = wv[1];
        u[2][o2] = wv[2];
        s[o1] = 0.0;
        s[o2] = 0.0;
    }
    else if (nvalid == 2)
    {
        int m = !valid[0] ? 0 : (!valid[1] ? 1 : 2);
        int a = (m + 1) % 3, b = (m + 2) % 3;
        double va[3] = {u[0][a], u[1][a], u[2][a]};
        double vb[3] = {u[0][b], u[1][b], u[2][b]};
        double wv[3];
        cross(va, vb, wv);
        double n = norm3(wv);
        if (n < 1e-18)
        {
            u[0][m] = (m == 0);
            u[1][m] = (m == 1);
            u[2][m] = (m == 2);
        }
        else
        {
            u[0][m] = wv[0] / n;
            u[1][m] = wv[1] / n;
            u[2][m] = wv[2] / n;
        }
        s[m] = 0.0;
    }
    else if (nvalid == 3 && s[0] > 0.0 && s[2] / s[0] < 1e-6)
    {
        // Polish only ill-conditioned cases: the column from a tiny singular
        // value loses accuracy through C*v/s (e.g. s=2.8e-06 in the ALA PDB
        // pair). Rebuild it from the two accurate columns; the later
        // det-correction fixes the sign. Well-conditioned inputs keep pure
        // C*v/s so last-ulp tie-breaking is undisturbed.
        double d01 = u[0][0] * u[0][1] + u[1][0] * u[1][1] + u[2][0] * u[2][1];
        double t1[3] = {u[0][1] - d01 * u[0][0], u[1][1] - d01 * u[1][0], u[2][1] - d01 * u[2][0]};
        double n1 = norm3(t1);
        if (n1 > 1e-18)
        {
            u[0][1] = t1[0] / n1;
            u[1][1] = t1[1] / n1;
            u[2][1] = t1[2] / n1;
        }
        double va[3] = {u[0][0], u[1][0], u[2][0]};
        double vb[3] = {u[0][1], u[1][1], u[2][1]};
        double wv[3];
        cross(va, vb, wv);
        double n2 = norm3(wv);
        if (n2 > 1e-18)
        {
            u[0][2] = wv[0] / n2;
            u[1][2] = wv[1] / n2;
            u[2][2] = wv[2] / n2;
        }
    }
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            vt[i][j] = vsorted[j][i];
        }
    }
}

Mat3 sorted_eigenvectors3(const Mat3& m)
{
    Mat3 v;
    double w[3];
    jacobi_eigen3(m, v, w);
    int idx[3] = {0, 1, 2};
    std::sort(idx, idx + 3, [&](int a, int b) { return w[a] < w[b]; });
    Mat3 out{};
    for (int col = 0; col < 3; ++col)
    {
        for (int row = 0; row < 3; ++row)
        {
            out[row][col] = v[row][idx[col]];
        }
    }
    return out;
}

} // namespace Rmsd
