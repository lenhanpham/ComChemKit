#ifndef IVCOORD_DATA_H
#define IVCOORD_DATA_H

#include <array>
#include <string>
#include <vector>

/**
 * @file ivcoord_data.h
 * @brief Shared data structures for the ivcoord module
 * @author Le Nhan Pham
 * @date 2026
 *
 * Contains the IVCoordData struct used by all QC program parsers and the
 * IVCoordRunner to transfer parsed geometry and normal-mode displacement data.
 */

/**
 * @struct IVCoordData
 * @brief Holds the optimized geometry and imaginary-mode displacement vectors
 *        parsed from a quantum chemistry frequency output file.
 *
 * The displacement vectors (disps) are the raw mass-weighted normalized
 * Cartesian eigenvectors as printed by the QC program. They are applied
 * directly as a Cartesian delta (no additional mass-weighting) in accordance
 * with the established QRC convention.
 */
struct IVCoordData
{
    int                                       natom;        ///< Number of atoms
    std::vector<std::string>                  elements;     ///< Element symbols (size natom)
    std::vector<std::array<double, 3>>        coords;       ///< Optimized coordinates, Angstrom (size natom)
    std::vector<std::array<double, 3>>        disps;        ///< Displacement eigenvectors for 1st imaginary mode (size natom)
    double                                    im_freq;      ///< Imaginary frequency value (cm⁻¹, negative)
    int                                       mode_index;   ///< 0-based mode index in the output file
    bool                                      parsed_ok;    ///< True if parsing succeeded
    std::string                               error_message; ///< Human-readable error when parsed_ok == false

    IVCoordData()
        : natom(0), im_freq(0.0), mode_index(-1), parsed_ok(false)
    {}
};

#endif  // IVCOORD_DATA_H
