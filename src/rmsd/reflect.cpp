/**
 * @file reflect.cpp
 * @brief Reflection search (port of Rust reflect.rs).
 * @author Le Nhan Pham
 * @date 2026
 */

#include "rmsd/reflect.h"
#include <limits>

namespace Rmsd
{

const std::array<std::array<size_t, 3>, 6> AXIS_SWAPS = {{{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 1, 0}, {2, 0, 1}}};
const std::array<std::array<double, 3>, 8> AXIS_REFLECTIONS = {{{1, 1, 1},
    {-1, 1, 1},
    {1, -1, 1},
    {1, 1, -1},
    {-1, -1, 1},
    {-1, 1, -1},
    {1, -1, -1},
    {-1, -1, -1}}};

ReflectResult check_reflections(const std::vector<uint8_t>& p_atoms, const std::vector<uint8_t>& q_atoms,
    const std::vector<Coord>& p_coord, const std::vector<Coord>& q_coord, const ReorderFn& reorder, bool has_reorder,
    const RmsdFn& rmsd_fn, bool keep_stereo)
{
    const int SWAP_MASK[6] = {1, -1, -1, 1, -1, 1};
    const int REFL_MASK[8] = {1, -1, -1, -1, 1, 1, 1, -1};
    ReflectResult best;
    best.rmsd = std::numeric_limits<double>::infinity();
    best.swap = {0, 1, 2};
    best.reflection = {1, 1, 1};
    best.review.resize(p_atoms.size());
    for (size_t i = 0; i < best.review.size(); ++i)
    {
        best.review[i] = i;
    }
    auto centroid_of = [](const std::vector<Coord>& v) {
        Coord s{0, 0, 0};
        for (const auto& p : v)
        {
            s[0] += p[0];
            s[1] += p[1];
            s[2] += p[2];
        }
        double n = static_cast<double>(v.size());
        s[0] /= n;
        s[1] /= n;
        s[2] /= n;
        return s;
    };
    for (size_t si = 0; si < 6; ++si)
    {
        for (size_t rj = 0; rj < 8; ++rj)
        {
            if (keep_stereo && SWAP_MASK[si] * REFL_MASK[rj] == -1)
            {
                continue;
            }
            const auto& swap = AXIS_SWAPS[si];
            const auto& refl = AXIS_REFLECTIONS[rj];
            std::vector<Coord> tmp;
            tmp.reserve(q_coord.size());
            for (const auto& q : q_coord)
            {
                tmp.push_back({q[swap[0]] * refl[0], q[swap[1]] * refl[1], q[swap[2]] * refl[2]});
            }
            Coord c = centroid_of(tmp);
            for (auto& v : tmp)
            {
                v[0] -= c[0];
                v[1] -= c[1];
                v[2] -= c[2];
            }
            std::vector<size_t> review(tmp.size());
            for (size_t i = 0; i < review.size(); ++i)
            {
                review[i] = i;
            }
            std::vector<Coord> trial;
            if (has_reorder)
            {
                review = reorder(p_atoms, q_atoms, p_coord, tmp);
                for (size_t i : review)
                {
                    trial.push_back(tmp[i]);
                }
            }
            else
            {
                trial = tmp;
            }
            double r = rmsd_fn(p_coord, trial);
            if (r < best.rmsd)
            {
                best.rmsd = r;
                best.swap = swap;
                best.reflection = refl;
                best.review = review;
            }
        }
    }
    return best;
}

} // namespace Rmsd
