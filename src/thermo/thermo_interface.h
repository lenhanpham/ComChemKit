/**
 * @file thermo_interface.h
 * @brief Interface layer for integrating OpenThermo module with ComChemKit
 * @author Le Nhan Pham
 * @date 2025
 *
 * This header provides the interface layer that bridges ComChemKit's command system
 * with the OpenThermo module, enabling thermodynamic analysis for multiple
 * quantum chemistry programs.
 */

#ifndef THERMO_INTERFACE_H
#define THERMO_INTERFACE_H

#include <string>
#include <vector>
#include "utilities/command_system.h"

// Forward declarations
struct SystemData;

namespace ThermoInterface 
{
    /**
     * @brief Result structure for thermo operations
     */
    struct ThermoResult 
    {
        bool success = false;
        std::string error_message;
        std::vector<std::string> output_files;
        int exit_code = 0;
    };

    /**
     * @brief Process a single file with thermodynamic analysis
     * @param context Command context containing thermo parameters
     * @return ThermoResult with operation status and results
     */
    ThermoResult process_file(const CommandContext& context);

    /**
     * @brief Process multiple files with thermodynamic analysis
     * @param context Command context containing thermo parameters
     * @param files List of input files to process
     * @return ThermoResult with operation status and results
     */
    ThermoResult process_batch(const CommandContext& context, 
                              const std::vector<std::string>& files);

    /**
     * @brief Convert CommandContext to SystemData for thermo module
     * @param context ComChemKit command context
     * @param input_file Path to input file
     * @return SystemData structure for thermo calculations
     */
    void* create_system_data(const CommandContext& context, const std::string& input_file);

    /**
     * @brief Initialize thermo module with default settings
     */
    void initialize_thermo_module();

    /**
     * @brief Cleanup thermo module resources
     */
    void cleanup_thermo_module();
    
    /**
     * @brief Print OpenThermo program header and citation information
     */
    void print_program_header();
    
    /**
     * @brief Get input file path interactively from user
     * @return Input file path
     */
    std::string get_interactive_input();
    
    /**
     * @brief Display detailed molecular information
     * @param sys SystemData containing molecular information
     * @param nimag Number of imaginary frequencies
     */
    void display_molecular_info(const SystemData& sys, int nimag);
}

#endif // THERMO_INTERFACE_H
