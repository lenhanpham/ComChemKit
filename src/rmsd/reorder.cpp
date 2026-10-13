/**
 * @file reorder.cpp
 * @brief Reorder implementation (port of Rust reorder.rs).
 * @author Le Nhan Pham
 * @date 2026
 */

#include "rmsd/reorder.h"
#include "rmsd/geom.h"
#include "rmsd/inertia.h"
#include "rmsd/linalg.h"
#include "rmsd/reflect.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace Rmsd
{

namespace
{

std::vector<uint8_t> unique_atoms(const std::vector<uint8_t>& p)
{
    std::map<uint8_t, bool> seen;
    for (uint8_t a : p)
    {
        seen[a] = true;
    }
    std::vector<uint8_t> out;
    for (const auto& kv : seen)
    {
        out.push_back(kv.first);
    }
    return out;
}

std::vector<size_t> indices_of(const std::vector<uint8_t>& atoms, uint8_t z)
{
    std::vector<size_t> out;
    for (size_t i = 0; i < atoms.size(); ++i)
    {
        if (atoms[i] == z)
        {
            out.push_back(i);
        }
    }
    return out;
}

std::vector<std::vector<double>> euclidean_cost(const std::vector<Coord>& a, const std::vector<Coord>& b)
{
    std::vector<std::vector<double>> cost(a.size(), std::vector<double>(b.size()));
    for (size_t i = 0; i < a.size(); ++i)
    {
        for (size_t j = 0; j < b.size(); ++j)
        {
            double dx = a[i][0] - b[j][0];
            double dy = a[i][1] - b[j][1];
            double dz = a[i][2] - b[j][2];
            cost[i][j] = std::sqrt(dx * dx + dy * dy + dz * dz);
        }
    }
    return cost;
}

} // namespace

std::vector<size_t> hungarian_assign(const std::vector<std::vector<double>>& cost)
{
    size_t n = cost.size();
    std::vector<std::vector<double>> c = cost;
    for (auto& row : c)
    {
        double m = *std::min_element(row.begin(), row.end());
        for (auto& v : row)
        {
            v -= m;
        }
    }
    std::vector<double> col_min(n, std::numeric_limits<double>::infinity());
    for (const auto& row : c)
    {
        for (size_t j = 0; j < n; ++j)
        {
            col_min[j] = std::min(col_min[j], row[j]);
        }
    }
    for (auto& row : c)
    {
        for (size_t j = 0; j < n; ++j)
        {
            row[j] -= col_min[j];
        }
    }
    std::vector<std::vector<char>> star(n, std::vector<char>(n, 0));
    std::vector<std::vector<char>> prime(n, std::vector<char>(n, 0));
    std::vector<char> row_cov(n, 0), col_cov(n, 0);
    for (size_t i = 0; i < n; ++i)
    {
        for (size_t j = 0; j < n; ++j)
        {
            if (std::fabs(c[i][j]) < 1e-12 && !row_cov[i] && !col_cov[j])
            {
                star[i][j] = 1;
                row_cov[i] = 1;
                col_cov[j] = 1;
            }
        }
    }
    std::fill(row_cov.begin(), row_cov.end(), 0);
    std::fill(col_cov.begin(), col_cov.end(), 0);
    for (size_t i = 0; i < n; ++i)
    {
        for (size_t j = 0; j < n; ++j)
        {
            if (star[i][j])
            {
                col_cov[j] = 1;
            }
        }
    }
    auto covered_cols = [&]() {
        size_t k = 0;
        for (char v : col_cov)
        {
            k += v ? 1 : 0;
        }
        return k;
    };
    while (covered_cols() != n)
    {
        while (true)
        {
            int zi = -1, zj = -1;
            for (size_t i = 0; i < n && zi < 0; ++i)
            {
                if (row_cov[i])
                {
                    continue;
                }
                for (size_t j = 0; j < n; ++j)
                {
                    if (col_cov[j])
                    {
                        continue;
                    }
                    if (std::fabs(c[i][j]) < 1e-10)
                    {
                        zi = static_cast<int>(i);
                        zj = static_cast<int>(j);
                        break;
                    }
                }
            }
            if (zi < 0)
            {
                double m = std::numeric_limits<double>::infinity();
                for (size_t i = 0; i < n; ++i)
                {
                    if (row_cov[i])
                    {
                        continue;
                    }
                    for (size_t j = 0; j < n; ++j)
                    {
                        if (col_cov[j])
                        {
                            continue;
                        }
                        m = std::min(m, c[i][j]);
                    }
                }
                for (size_t i = 0; i < n; ++i)
                {
                    for (size_t j = 0; j < n; ++j)
                    {
                        if (row_cov[i])
                        {
                            c[i][j] += m;
                        }
                        if (!col_cov[j])
                        {
                            c[i][j] -= m;
                        }
                    }
                }
                continue;
            }
            prime[zi][zj] = 1;
            int sj = -1;
            for (size_t j = 0; j < n; ++j)
            {
                if (star[zi][j])
                {
                    sj = static_cast<int>(j);
                    break;
                }
            }
            if (sj >= 0)
            {
                row_cov[zi] = 1;
                col_cov[sj] = 0;
            }
            else
            {
                std::vector<std::pair<size_t, size_t>> path;
                path.emplace_back(zi, zj);
                while (true)
                {
                    size_t lj = path.back().second;
                    int r = -1;
                    for (size_t i = 0; i < n; ++i)
                    {
                        if (star[i][lj])
                        {
                            r = static_cast<int>(i);
                            break;
                        }
                    }
                    if (r < 0)
                    {
                        break;
                    }
                    path.emplace_back(r, lj);
                    int pc = -1;
                    for (size_t j = 0; j < n; ++j)
                    {
                        if (prime[r][j])
                        {
                            pc = static_cast<int>(j);
                            break;
                        }
                    }
                    path.emplace_back(r, pc);
                }
                for (size_t k = 0; k < path.size(); ++k)
                {
                    star[path[k].first][path[k].second] = (k % 2 == 0) ? 1 : 0;
                }
                for (auto& row : prime)
                {
                    std::fill(row.begin(), row.end(), 0);
                }
                std::fill(row_cov.begin(), row_cov.end(), 0);
                std::fill(col_cov.begin(), col_cov.end(), 0);
                for (size_t i = 0; i < n; ++i)
                {
                    for (size_t j = 0; j < n; ++j)
                    {
                        if (star[i][j])
                        {
                            col_cov[j] = 1;
                        }
                    }
                }
                break;
            }
        }
    }
    std::vector<size_t> assign(n, 0);
    for (size_t i = 0; i < n; ++i)
    {
        for (size_t j = 0; j < n; ++j)
        {
            if (star[i][j])
            {
                assign[i] = j;
                break;
            }
        }
    }
    return assign;
}

std::vector<size_t> reorder_hungarian(const std::vector<uint8_t>& p_atoms, const std::vector<uint8_t>& q_atoms,
    const std::vector<Coord>& p_coord, const std::vector<Coord>& q_coord)
{
    std::vector<size_t> view(q_atoms.size(), 0);
    for (uint8_t z : unique_atoms(p_atoms))
    {
        std::vector<size_t> pi = indices_of(p_atoms, z);
        std::vector<size_t> qi = indices_of(q_atoms, z);
        std::vector<Coord> a, b;
        for (size_t i : pi)
        {
            a.push_back(p_coord[i]);
        }
        for (size_t i : qi)
        {
            b.push_back(q_coord[i]);
        }
        std::vector<size_t> cols = hungarian_assign(euclidean_cost(a, b));
        for (size_t k = 0; k < pi.size(); ++k)
        {
            view[pi[k]] = qi[cols[k]];
        }
    }
    return view;
}

std::vector<size_t> reorder_distance(const std::vector<uint8_t>& p_atoms, const std::vector<uint8_t>& q_atoms,
    const std::vector<Coord>& p_coord, const std::vector<Coord>& q_coord)
{
    std::vector<size_t> view(q_atoms.size(), 0);
    for (uint8_t z : unique_atoms(p_atoms))
    {
        std::vector<size_t> pi = indices_of(p_atoms, z);
        std::vector<size_t> qi = indices_of(q_atoms, z);
        std::vector<std::pair<double, size_t>> pa, qb;
        for (size_t i : pi)
        {
            double d = p_coord[i][0] * p_coord[i][0] + p_coord[i][1] * p_coord[i][1] + p_coord[i][2] * p_coord[i][2];
            pa.emplace_back(d, i);
        }
        for (size_t i : qi)
        {
            double d = q_coord[i][0] * q_coord[i][0] + q_coord[i][1] * q_coord[i][1] + q_coord[i][2] * q_coord[i][2];
            qb.emplace_back(d, i);
        }
        std::sort(pa.begin(), pa.end());
        std::sort(qb.begin(), qb.end());
        for (size_t k = 0; k < pa.size(); ++k)
        {
            view[pa[k].second] = qb[k].second;
        }
    }
    return view;
}

std::vector<size_t> reorder_inertia_hungarian(const std::vector<uint8_t>& p_atoms,
    const std::vector<uint8_t>& q_atoms, const std::vector<Coord>& p_coord, const std::vector<Coord>& q_coord)
{
    std::vector<Coord> pc = p_coord, qc = q_coord;
    Coord pcm = center_of_mass(p_atoms, pc);
    Coord qcm = center_of_mass(q_atoms, qc);
    for (auto& v : pc)
    {
        v[0] -= pcm[0];
        v[1] -= pcm[1];
        v[2] -= pcm[2];
    }
    for (auto& v : qc)
    {
        v[0] -= qcm[0];
        v[1] -= qcm[1];
        v[2] -= qcm[2];
    }
    Mat3 ip = inertia_tensor(p_atoms, pc);
    Mat3 iq = inertia_tensor(q_atoms, qc);
    Mat3 ep = sorted_eigenvectors3(ip);
    Mat3 eq = sorted_eigenvectors3(iq);
    std::vector<Coord> pa;
    for (const auto& v : pc)
    {
        pa.push_back(apply_mat_row(v, ep));
    }
    double best_rmsd = std::numeric_limits<double>::infinity();
    std::vector<size_t> best(p_atoms.size());
    for (size_t i = 0; i < best.size(); ++i)
    {
        best[i] = i;
    }
    for (const auto& mirror : AXIS_REFLECTIONS)
    {
        Mat3 tmp = eq;
        for (int j = 0; j < 3; ++j)
        {
            tmp[0][j] *= mirror[j];
            tmp[1][j] *= mirror[j];
            tmp[2][j] *= mirror[j];
        }
        std::vector<Coord> qa;
        for (const auto& v : qc)
        {
            qa.push_back(apply_mat_row(v, tmp));
        }
        std::vector<size_t> review = reorder_hungarian(p_atoms, q_atoms, pa, qa);
        std::vector<Coord> qa_re;
        for (size_t i : review)
        {
            qa_re.push_back(qa[i]);
        }
        double r = kabsch_rmsd(qa_re, pa, nullptr, false);
        if (r < best_rmsd)
        {
            best_rmsd = r;
            best = review;
        }
    }
    return best;
}

namespace
{

void heaps_perm(std::vector<size_t>& a, size_t n, std::vector<size_t>& best, double& best_rmsd,
    const std::vector<Coord>& ref, const std::vector<Coord>& mob)
{
    // Iterative Heap's, exact port of Rust heaps(): identity scored by the
    // caller first, then n!-1 further permutations in identical visit order
    // (tie-breaking among symmetric perms depends on it).
    auto score = [&](const std::vector<size_t>& perm) {
        std::vector<Coord> re;
        for (size_t i : perm)
        {
            re.push_back(mob[i]);
        }
        double r = kabsch_rmsd(ref, re, nullptr, false);
        if (r < best_rmsd)
        {
            best_rmsd = r;
            best = perm;
        }
    };
    (void)n;
    size_t len = a.size();
    std::vector<size_t> c(len, 0);
    size_t i = 0;
    while (i < len)
    {
        if (c[i] < i)
        {
            if (i % 2 == 0)
            {
                std::swap(a[0], a[i]);
            }
            else
            {
                std::swap(a[c[i]], a[i]);
            }
            score(a);
            c[i] += 1;
            i = 0;
        }
        else
        {
            c[i] = 0;
            i += 1;
        }
    }
}

std::vector<size_t> brute_permutation(const std::vector<Coord>& a, const std::vector<Coord>& b)
{
    size_t n = a.size();
    std::vector<size_t> order(n), best(n);
    for (size_t i = 0; i < n; ++i)
    {
        order[i] = i;
        best[i] = i;
    }
    {
        std::vector<Coord> re;
        for (size_t i : order)
        {
            re.push_back(b[i]);
        }
        double r = kabsch_rmsd(a, re, nullptr, false);
        double best_rmsd = r;
        heaps_perm(order, n, best, best_rmsd, a, b);
    }
    return best;
}

} // namespace

std::vector<size_t> reorder_brute(const std::vector<uint8_t>& p_atoms, const std::vector<uint8_t>& q_atoms,
    const std::vector<Coord>& p_coord, const std::vector<Coord>& q_coord)
{
    std::vector<size_t> view(q_atoms.size(), 0);
    for (uint8_t z : unique_atoms(p_atoms))
    {
        std::vector<size_t> pi = indices_of(p_atoms, z);
        std::vector<size_t> qi = indices_of(q_atoms, z);
        if (pi.size() > 10)
        {
            throw RmsdException(RmsdErrorCode::Args, "brute reorder refused: >10 same-element atoms");
        }
        std::vector<Coord> a, b;
        for (size_t i : pi)
        {
            a.push_back(p_coord[i]);
        }
        for (size_t i : qi)
        {
            b.push_back(q_coord[i]);
        }
        std::vector<size_t> cols = brute_permutation(a, b);
        for (size_t k = 0; k < pi.size(); ++k)
        {
            view[pi[k]] = qi[cols[k]];
        }
    }
    return view;
}

std::vector<size_t> reorder_dispatch(ReorderMethod method, const std::vector<uint8_t>& p_atoms,
    const std::vector<uint8_t>& q_atoms, const std::vector<Coord>& p_coord, const std::vector<Coord>& q_coord,
    bool& applied)
{
    applied = true;
    switch (method)
    {
        case ReorderMethod::None:
            applied = false;
            return {};
        case ReorderMethod::Hungarian:
            return reorder_hungarian(p_atoms, q_atoms, p_coord, q_coord);
        case ReorderMethod::InertiaHungarian:
            return reorder_inertia_hungarian(p_atoms, q_atoms, p_coord, q_coord);
        case ReorderMethod::Distance:
            return reorder_distance(p_atoms, q_atoms, p_coord, q_coord);
        case ReorderMethod::Brute:
            return reorder_brute(p_atoms, q_atoms, p_coord, q_coord);
    }
    applied = false;
    return {};
}

} // namespace Rmsd
