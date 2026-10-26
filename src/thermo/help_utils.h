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
     * @brief Print general help information (owned by thermo module).
     * @param program_name Name of the program (default: "OpenThermo")
     */
    void print_help(const std::string& program_name = "OpenThermo");

    /**
     * @brief Print help for a thermo sub-topic.
     *
     * Handles --help-input / --help-output / --help-settings and
     * --help-<option> (e.g. --help-T, --help-lowvibmeth). Owned by thermo
     * module so new options never touch the global hub.
     */
    void print_topic(const std::string& topic, const std::string& program_name = "OpenThermo");

}  // namespace ThermoHelpUtils

#endif  // THERMO_HELP_UTILS_H