#ifndef GAUSSIAN_IVCOORD_PARSER_H
#define GAUSSIAN_IVCOORD_PARSER_H

#include "ivcoord/ivcoord_parser_base.h"
#include "ivcoord/ivcoord_data.h"
#include <string>

/**
 * @file gaussian_ivcoord_parser.h
 * @brief Gaussian-specific parser for ivcoord displacement data
 * @author Le Nhan Pham
 * @date 2026
 *
 * Handles Gaussian 09/16 frequency output files. Parses:
 *  - Last "Standard orientation:" (or "Input orientation:") block for geometry
 *  - First "Frequencies --" block containing a negative value for displacements
 */

/**
 * @class GaussianIVCoordParser
 * @brief Implements IIVCoordParser for Gaussian output files (.log / .out)
 */
class GaussianIVCoordParser : public IIVCoordParser
{
public:
    bool        can_parse(const std::string& filepath) override;
    IVCoordData parse(const std::string& filepath) override;

private:
    /// Convert atomic number to element symbol (internal utility)
    static std::string atomic_symbol(int z);
};

#endif  // GAUSSIAN_IVCOORD_PARSER_H
