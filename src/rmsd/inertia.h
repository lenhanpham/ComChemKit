/**
 * @file inertia.h
 * @brief Center of mass, inertia tensor.
 * @author Le Nhan Pham
 * @date 2026
 */

#ifndef RMSD_INERTIA_H
#define RMSD_INERTIA_H

#include "rmsd/rmsd_types.h"
#include <cstdint>
#include <vector>

namespace Rmsd
{

Coord center_of_mass(const std::vector<uint8_t>& atoms, const std::vector<Coord>& coords);
Mat3 inertia_tensor(const std::vector<uint8_t>& atoms, const std::vector<Coord>& coords);
Coord principal_axis(const std::vector<uint8_t>& atoms, const std::vector<Coord>& coords);

} // namespace Rmsd

#endif // RMSD_INERTIA_H
