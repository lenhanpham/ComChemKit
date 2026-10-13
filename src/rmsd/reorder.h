/**
 * @file reorder.h
 * @brief Atom reordering (no QML).
 * @author Le Nhan Pham
 * @date 2026
 */

#ifndef RMSD_REORDER_H
#define RMSD_REORDER_H

#include "rmsd/rmsd_types.h"
#include <cstdint>
#include <vector>

namespace Rmsd
{

std::vector<size_t> hungarian_assign(const std::vector<std::vector<double>>& cost);
std::vector<size_t> reorder_hungarian(const std::vector<uint8_t>& p_atoms, const std::vector<uint8_t>& q_atoms,
    const std::vector<Coord>& p_coord, const std::vector<Coord>& q_coord);
std::vector<size_t> reorder_distance(const std::vector<uint8_t>& p_atoms, const std::vector<uint8_t>& q_atoms,
    const std::vector<Coord>& p_coord, const std::vector<Coord>& q_coord);
std::vector<size_t> reorder_inertia_hungarian(const std::vector<uint8_t>& p_atoms,
    const std::vector<uint8_t>& q_atoms, const std::vector<Coord>& p_coord, const std::vector<Coord>& q_coord);
std::vector<size_t> reorder_brute(const std::vector<uint8_t>& p_atoms, const std::vector<uint8_t>& q_atoms,
    const std::vector<Coord>& p_coord, const std::vector<Coord>& q_coord);

/**
 * @brief Dispatch by method; empty vector means no reorder (None).
 */
std::vector<size_t> reorder_dispatch(ReorderMethod method, const std::vector<uint8_t>& p_atoms,
    const std::vector<uint8_t>& q_atoms, const std::vector<Coord>& p_coord, const std::vector<Coord>& q_coord,
    bool& applied);

} // namespace Rmsd

#endif // RMSD_REORDER_H
