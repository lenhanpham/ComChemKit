/**
 * @file geom.cpp
 * @brief Geometry implementation.
 * @author Le Nhan Pham
 * @date 2026
 */

#include "rmsd/geom.h"
#include "rmsd/linalg.h"
#include <cmath>
#include <stdexcept>

namespace Rmsd
{

namespace
{

Mat3 covariance_mat(const std::vector<Coord>& p, const std::vector<Coord>& q)
{
    Mat3 c{};
    for (size_t k = 0; k < p.size(); ++k)
    {
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                c[i][j] += p[k][i] * q[k][j];
            }
        }
    }
    return c;
}

Mat3 optimal_rotation_mat(Mat3 u, const Mat3& vt)
{
    double du = mat3_det(u);
    double dv = mat3_det(vt);
    if (du * dv < 0.0)
    {
        u[0][2] = -u[0][2];
        u[1][2] = -u[1][2];
        u[2][2] = -u[2][2];
    }
    return mat3_mul(u, vt);
}

Mat4 make_w(double r1, double r2, double r3, double r4)
{
    Mat4 m{};
    m[0][0] = r4;
    m[0][1] = r3;
    m[0][2] = -r2;
    m[0][3] = r1;
    m[1][0] = -r3;
    m[1][1] = r4;
    m[1][2] = r1;
    m[1][3] = r2;
    m[2][0] = r2;
    m[2][1] = -r1;
    m[2][2] = r4;
    m[2][3] = r3;
    m[3][0] = -r1;
    m[3][1] = -r2;
    m[3][2] = -r3;
    m[3][3] = r4;
    return m;
}

Mat4 make_q(double r1, double r2, double r3, double r4)
{
    Mat4 m{};
    m[0][0] = r4;
    m[0][1] = -r3;
    m[0][2] = r2;
    m[0][3] = r1;
    m[1][0] = r3;
    m[1][1] = r4;
    m[1][2] = -r1;
    m[1][3] = r2;
    m[2][0] = -r2;
    m[2][1] = r1;
    m[2][2] = r4;
    m[2][3] = r3;
    m[3][0] = -r1;
    m[3][1] = -r2;
    m[3][2] = -r3;
    m[3][3] = r4;
    return m;
}

Mat4 mat4_transpose(const Mat4& a)
{
    Mat4 t{};
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            t[i][j] = a[j][i];
        }
    }
    return t;
}

Mat4 mat4_mul(const Mat4& a, const Mat4& b)
{
    Mat4 c{};
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            c[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
        }
    }
    return c;
}

} // namespace

Coord centroid(const std::vector<Coord>& coords)
{
    if (coords.empty())
    {
        throw RmsdException(RmsdErrorCode::Args, "centroid of empty set");
    }
    Coord s{0.0, 0.0, 0.0};
    for (const auto& p : coords)
    {
        s[0] += p[0];
        s[1] += p[1];
        s[2] += p[2];
    }
    double n = static_cast<double>(coords.size());
    s[0] /= n;
    s[1] /= n;
    s[2] /= n;
    return s;
}

double rmsd_plain(const std::vector<Coord>& p, const std::vector<Coord>& q)
{
    if (p.size() != q.size() || p.empty())
    {
        throw RmsdException(RmsdErrorCode::SizeMismatch, "rmsd size mismatch or empty");
    }
    double acc = 0.0;
    for (size_t i = 0; i < p.size(); ++i)
    {
        double dx = p[i][0] - q[i][0];
        double dy = p[i][1] - q[i][1];
        double dz = p[i][2] - q[i][2];
        acc += dx * dx + dy * dy + dz * dz;
    }
    return std::sqrt(acc / static_cast<double>(p.size()));
}

Mat3 kabsch_matrix(const std::vector<Coord>& p, const std::vector<Coord>& q)
{
    Mat3 c = covariance_mat(p, q);
    Mat3 u, vt;
    double s[3];
    svd3(c, u, s, vt);
    return optimal_rotation_mat(u, vt);
}

std::vector<Coord> kabsch_rotate(const std::vector<Coord>& p, const std::vector<Coord>& q)
{
    Mat3 u = kabsch_matrix(p, q);
    std::vector<Coord> out;
    out.reserve(p.size());
    for (const auto& v : p)
    {
        out.push_back(apply_mat_row(v, u));
    }
    return out;
}

std::vector<Coord> kabsch_fit(const std::vector<Coord>& p, const std::vector<Coord>& q)
{
    Coord pc = centroid(p);
    Coord qc = centroid(q);
    std::vector<Coord> pcc, qcc;
    pcc.reserve(p.size());
    qcc.reserve(q.size());
    for (const auto& v : p)
    {
        pcc.push_back({v[0] - pc[0], v[1] - pc[1], v[2] - pc[2]});
    }
    for (const auto& v : q)
    {
        qcc.push_back({v[0] - qc[0], v[1] - qc[1], v[2] - qc[2]});
    }
    Mat3 u = kabsch_matrix(pcc, qcc);
    std::vector<Coord> out;
    out.reserve(p.size());
    for (const auto& v : pcc)
    {
        Coord r = apply_mat_row(v, u);
        out.push_back({r[0] + qc[0], r[1] + qc[1], r[2] + qc[2]});
    }
    return out;
}

WeightedResult kabsch_weighted(const std::vector<Coord>& p, const std::vector<Coord>& q, const std::vector<double>* w)
{
    size_t n = p.size();
    if (n != q.size() || n == 0)
    {
        throw RmsdException(RmsdErrorCode::SizeMismatch, "weighted kabsch size mismatch");
    }
    std::vector<double> wvec;
    if (w != nullptr)
    {
        if (w->size() != n)
        {
            throw RmsdException(RmsdErrorCode::SizeMismatch, "weight size mismatch");
        }
        wvec = *w;
    }
    else
    {
        wvec.assign(n, 1.0 / static_cast<double>(n));
    }
    double wsum = 0.0;
    for (double v : wvec)
    {
        wsum += v;
    }
    double iw = 1.0 / wsum;
    double cmp[3] = {0, 0, 0};
    double cmq[3] = {0, 0, 0};
    Mat3 c{};
    double psq = 0.0;
    double qsq = 0.0;
    for (size_t k = 0; k < n; ++k)
    {
        double wi = wvec[k];
        cmp[0] += p[k][0] * wi;
        cmp[1] += p[k][1] * wi;
        cmp[2] += p[k][2] * wi;
        cmq[0] += q[k][0] * wi;
        cmq[1] += q[k][1] * wi;
        cmq[2] += q[k][2] * wi;
        psq += (p[k][0] * p[k][0] + p[k][1] * p[k][1] + p[k][2] * p[k][2]) * wi;
        qsq += (q[k][0] * q[k][0] + q[k][1] * q[k][1] + q[k][2] * q[k][2]) * wi;
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                c[i][j] += p[k][i] * wi * q[k][j];
            }
        }
    }
    double cmp_dot = cmp[0] * cmp[0] + cmp[1] * cmp[1] + cmp[2] * cmp[2];
    double cmq_dot = cmq[0] * cmq[0] + cmq[1] * cmq[1] + cmq[2] * cmq[2];
    psq -= cmp_dot * iw;
    qsq -= cmq_dot * iw;
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            c[i][j] = (c[i][j] - cmp[i] * cmq[j] * iw) * iw;
        }
    }
    Mat3 u, vt;
    double s[3];
    svd3(c, u, s, vt);
    double d = mat3_det(u) * mat3_det(vt);
    Mat3 rot = optimal_rotation_mat(u, vt);
    double s_sum = (d < 0.0) ? (s[0] + s[1] - s[2]) : (s[0] + s[1] + s[2]);
    double msd = (psq + qsq) * iw - 2.0 * s_sum;
    if (msd < 0.0)
    {
        msd = 0.0;
    }
    WeightedResult r;
    r.rot = rot;
    double rcmq[3];
    rcmq[0] = rot[0][0] * cmq[0] + rot[1][0] * cmq[1] + rot[2][0] * cmq[2];
    rcmq[1] = rot[0][1] * cmq[0] + rot[1][1] * cmq[1] + rot[2][1] * cmq[2];
    rcmq[2] = rot[0][2] * cmq[0] + rot[1][2] * cmq[1] + rot[2][2] * cmq[2];
    r.trans[0] = (cmp[0] - rcmq[0]) * iw;
    r.trans[1] = (cmp[1] - rcmq[1]) * iw;
    r.trans[2] = (cmp[2] - rcmq[2]) * iw;
    r.rmsd = std::sqrt(msd);
    return r;
}

double kabsch_weighted_rmsd(const std::vector<Coord>& p, const std::vector<Coord>& q, const std::vector<double>* w)
{
    return kabsch_weighted(p, q, w).rmsd;
}

double kabsch_rmsd(const std::vector<Coord>& p, const std::vector<Coord>& q, const std::vector<double>* w, bool translate)
{
    if (translate)
    {
        Coord pc = centroid(p);
        Coord qc = centroid(q);
        std::vector<Coord> pp, qq;
        pp.reserve(p.size());
        qq.reserve(q.size());
        for (const auto& v : p)
        {
            pp.push_back({v[0] - pc[0], v[1] - pc[1], v[2] - pc[2]});
        }
        for (const auto& v : q)
        {
            qq.push_back({v[0] - qc[0], v[1] - qc[1], v[2] - qc[2]});
        }
        if (w != nullptr)
        {
            return kabsch_weighted_rmsd(pp, qq, w);
        }
        std::vector<Coord> rot = kabsch_rotate(pp, qq);
        return rmsd_plain(rot, qq);
    }
    if (w != nullptr)
    {
        return kabsch_weighted_rmsd(p, q, w);
    }
    std::vector<Coord> rot = kabsch_rotate(p, q);
    return rmsd_plain(rot, q);
}

Mat3 quaternion_transform(const std::array<double, 4>& r)
{
    Mat4 w = mat4_transpose(make_w(r[0], r[1], r[2], r[3]));
    Mat4 q = make_q(r[0], r[1], r[2], r[3]);
    Mat4 rot4 = mat4_mul(w, q);
    Mat3 m{};
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            m[i][j] = rot4[i][j];
        }
    }
    return m;
}

Mat3 quaternion_rotate(const std::vector<Coord>& x, const std::vector<Coord>& y)
{
    Mat4 a{};
    for (size_t k = 0; k < x.size(); ++k)
    {
        Mat4 q = make_q(x[k][0], x[k][1], x[k][2], 0.0);
        Mat4 w = make_w(y[k][0], y[k][1], y[k][2], 0.0);
        Mat4 qt = mat4_transpose(q);
        Mat4 prod = mat4_mul(qt, w);
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                a[i][j] += prod[i][j];
            }
        }
    }
    Mat4 v;
    double e[4];
    jacobi_eigen4(a, v, e);
    int best = 0;
    for (int i = 1; i < 4; ++i)
    {
        if (e[i] > e[best])
        {
            best = i;
        }
    }
    std::array<double, 4> quat{v[0][best], v[1][best], v[2][best], v[3][best]};
    return quaternion_transform(quat);
}

double quaternion_rmsd(const std::vector<Coord>& p, const std::vector<Coord>& q)
{
    Mat3 rot = quaternion_rotate(p, q);
    std::vector<Coord> pr;
    pr.reserve(p.size());
    for (const auto& v : p)
    {
        pr.push_back(apply_mat_row(v, rot));
    }
    return rmsd_plain(pr, q);
}

} // namespace Rmsd
