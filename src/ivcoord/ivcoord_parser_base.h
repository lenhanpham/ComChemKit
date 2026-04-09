#ifndef IVCOORD_PARSER_BASE_H
#define IVCOORD_PARSER_BASE_H

#include "ivcoord/ivcoord_data.h"
#include <string>

/**
 * @file ivcoord_parser_base.h
 * @brief Abstract interface for QC program-specific ivcoord parsers
 * @author Le Nhan Pham
 * @date 2026
 *
 * New QC program support (ORCA, Q-Chem, NWChem, …) is added by implementing
 * this interface in a new file inside src/ivcoord/, registering the parser in
 * IVCoordRunner::extract(), and adding the .cpp to CMakeLists.txt SOURCES.
 * No other files need to change.
 */

/**
 * @class IIVCoordParser
 * @brief Interface for parsing a QC output file to extract optimized geometry
 *        and displacement vectors for the first imaginary normal mode.
 *
 * Implementations must be stateless — a single instance may be called
 * multiple times for different files.
 */
class IIVCoordParser
{
public:
    virtual ~IIVCoordParser() = default;

    /**
     * @brief Detect whether this parser can handle the given file.
     *
     * Should be fast (read a small number of lines only). Must not throw.
     *
     * @param filepath Absolute or relative path to the output file.
     * @return true if the file belongs to the QC program this parser handles.
     */
    virtual bool can_parse(const std::string& filepath) = 0;

    /**
     * @brief Parse the file and return geometry + displacement data.
     *
     * Called only when can_parse() already returned true for this file.
     *
     * @param filepath Absolute or relative path to the output file.
     * @return Populated IVCoordData (check parsed_ok and error_message).
     */
    virtual IVCoordData parse(const std::string& filepath) = 0;
};

#endif  // IVCOORD_PARSER_BASE_H
