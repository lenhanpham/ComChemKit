/**
 * @file elements.h
 * @brief Element Z/symbol/mass tables.
 * @author Le Nhan Pham
 * @date 2026
 */

#ifndef RMSD_ELEMENTS_H
#define RMSD_ELEMENTS_H

#include <cstdint>
#include <string>

namespace Rmsd
{

/**
 * @brief Atomic mass by atomic number (index 0 unused, gaps are 0.0).
 * @param z Atomic number.
 * @return Mass, or 0.0 when out of range.
 */
double element_mass(uint8_t z);

/**
 * @brief Symbol for atomic number.
 * @param z Atomic number.
 * @param out Receives symbol on success.
 * @return true on success, false for unknown number.
 */
bool element_symbol(uint8_t z, std::string& out);

/**
 * @brief Atomic number for symbol (trim + capitalize normalize).
 * @param sym Input symbol.
 * @param out Receives Z on success.
 * @return true on success, false for unknown symbol.
 */
bool element_atomic_number(const std::string& sym, uint8_t& out);

} // namespace Rmsd

#endif // RMSD_ELEMENTS_H
