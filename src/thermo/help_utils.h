/**
 * @file help_utils.h
 * @brief Help utility functions for OpenThermo program
 * @author Le Nhan Pham
 * @date 2025
 *
 * This file contains declarations for help-related utility functions
 * used throughout the OpenThermo molecular thermochemistry program.
 */

#ifndef THERMO_HELP_UTILS_H
#define THERMO_HELP_UTILS_H

#include <string>

namespace ThermoHelpUtils
{

    /**
     * @brief Print general help information
     * @param program_name Name of the program (default: "OpenThermo")
     */
    void print_help(const std::string& program_name = "OpenThermo");

}  // namespace ThermoHelpUtils

#endif  // THERMO_HELP_UTILS_H