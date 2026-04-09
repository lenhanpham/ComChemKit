#ifndef IVCOORD_RUNNER_H
#define IVCOORD_RUNNER_H

#include "ivcoord/ivcoord_data.h"
#include <filesystem>
#include <string>

/**
 * @file ivcoord_runner.h
 * @brief Factory dispatcher, displacement math, and XYZ writer for ivcoord
 * @author Le Nhan Pham
 * @date 2026
 *
 * IVCoordRunner owns the parser registry. To add a new QC program:
 *   1. Implement IIVCoordParser in src/ivcoord/<program>_ivcoord_parser.{h,cpp}
 *   2. Include that header in ivcoord_runner.cpp
 *   3. Add an instance to the parsers vector in IVCoordRunner::extract()
 *   4. Add the .cpp to CMakeLists.txt SOURCES
 * No other files need to change.
 */

class IVCoordRunner
{
public:
    /**
     * @brief Try each registered parser in turn and return the parsed data.
     *
     * Returns an IVCoordData with parsed_ok==false when no parser recognises
     * the file format or when parsing fails.
     *
     * @param filepath Path to the QC output file
     */
    static IVCoordData extract(const std::string& filepath);

    /**
     * @brief Apply displacement to the geometry and write displaced XYZ file(s).
     *
     * new_coord[i] = coords[i] + direction * amp * disps[i]
     *
     * @param data     Parsed geometry and displacement data
     * @param amp      Displacement amplitude (e.g. 1.0)
     * @param direction +1 (plus only), -1 (minus only), or 0 (both)
     * @param out_dir  Directory to write output XYZ files into
     * @param stem     Base filename without extension (e.g. "to-10-TS_09R")
     */
    static void apply_displacement(const IVCoordData&           data,
                                   double                       amp,
                                   int                          direction,
                                   const std::filesystem::path& out_dir,
                                   const std::string&           stem);
};

#endif  // IVCOORD_RUNNER_H
