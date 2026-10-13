/**
 * @file reflect.h
 * @brief Reflection/axis-swap search (port of Rust reflect.rs).
 * @author Le Nhan Pham
 * @date 2026
 */

#ifndef RMSD_REFLECT_H
#define RMSD_REFLECT_H

#include "rmsd/rmsd_types.h"
#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace Rmsd
{

extern const std::array<std::array<size_t, 3>, 6> AXIS_SWAPS;
extern const std::array<std::array<double, 3>, 8> AXIS_REFLECTIONS;

struct ReflectResult
{
    double rmsd;
    std::array<size_t, 3> swap;
    std::array<double, 3> reflection;
    std::vector<size_t> review;
};

using ReorderFn = std::function<std::vector<size_t>(
    const std::vector<uint8_t>&, const std::vector<uint8_t>&, const std::vector<Coord>&, const std::vector<Coord>&)>;
using RmsdFn = std::function<double(const std::vector<Coord>&, const std::vector<Coord>&)>;

ReflectResult check_reflections(const std::vector<uint8_t>& p_atoms, const std::vector<uint8_t>& q_atoms,
    const std::vector<Coord>& p_coord, const std::vector<Coord>& q_coord, const ReorderFn& reorder, bool has_reorder,
    const RmsdFn& rmsd_fn, bool keep_stereo);

} // namespace Rmsd

#endif // RMSD_REFLECT_H
