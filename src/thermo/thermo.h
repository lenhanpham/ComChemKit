/**
 * @file thermo.h
 * @brief Interface layer for integrating OpenThermo module with ComChemKit
 * @author Le Nhan Pham
 * @date 2025
 *
 * This header provides the interface layer that bridges ComChemKit's command system
 * with the OpenThermo module, enabling thermodynamic analysis for multiple
 * quantum chemistry programs.
 */

#ifndef THERMO_H
#define THERMO_H

#include <string>
#include <vector>
#include "commands/command_system.h"

// Forward declarations
struct SystemData;

namespace ThermoInterface 
{
    struct ThermoSettings {
        std::string input_file;
        double temperature = 298.15;
        double pressure = 1.0;
        double temp_low = 0.0;
        double temp_high = 0.0;
        double temp_step = 0.0;
        double pressure_low = 0.0;
        double pressure_high = 0.0;
        double pressure_step = 0.0;
        std::string concentration = "0";
        int print_vib = 0;
        int mass_mode = 1;
        int ip_mode = 0;
        std::string low_vib_treatment = "harmonic";
        double scale_zpe = 1.0;
        double scale_heat = 1.0;
        double scale_entropy = 1.0;
        double scale_cv = 1.0;
        double raise_vib = 100.0;
        double interp_vib = 100.0;
        double imag_real = 0.0;
        double external_energy = 0.0;
        bool output_otm = false;
        bool no_settings = false;
        std::string point_group = "";
        int prt_level = 1;
        bool hg_entropy = false;
        std::string bav_preset = "";
        int omp_threads = 0;
        std::vector<std::string> cli_args;
    };

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
    ThermoResult process_file(const ThermoSettings& settings, const CommandContext& context);

    /**
     * @brief Process multiple files with thermodynamic analysis
     * @param context Command context containing thermo parameters
     * @param files List of input files to process
     * @return ThermoResult with operation status and results
     */
    ThermoResult process_batch(const ThermoSettings& settings, const CommandContext& context, 
                              const std::vector<std::string>& files);

    /**
     * @brief Extract thermal corrections dynamically from an output file
     * @param file Path to the input quantum chemistry file
     * @param T Temperature in Kelvin
     * @param P Pressure in atm
     * @param corrG_au [out] Calculated Thermal Correction to Gibbs Free Energy in au
     * @param corrH_au [out] Calculated Thermal Correction to Enthalpy in au
     * @param zpe_au [out] Calculated Zero Point Energy in au
     * @param nfreq [out] Number of parsed vibrational frequencies
     * @return true if successful and frequencies were extracted, false otherwise
     */
    bool calculate_thermal_corrections(const std::string& file, double T, double P, 
                                       double& corrG_au, double& corrH_au, double& zpe_au, int& nfreq);

    /**
     * @brief Extract basic properties using Thermo module regardless of program
     * @param file Path to the input quantum chemistry file
     * @param T Temperature in Kelvin
     * @param P Pressure in atm
     * @param scf_au [out] Calculated electronic energy in au
     * @param corrG_au [out] Calculated Thermal Correction to Gibbs Free Energy in au
     * @param corrH_au [out] Calculated Thermal Correction to Enthalpy in au
     * @param zpe_au [out] Calculated Zero Point Energy in au
     * @param nfreq [out] Number of parsed vibrational frequencies
     * @param prog_name [out] Name of detected program
     * @return true if successful
     */
    bool extract_basic_properties(const std::string& file, double T, double P, 
                                  double& scf_au, double& corrG_au, double& corrH_au, double& zpe_au, double& lf_cm, int& nfreq, std::string& prog_name);

    /**
     * @brief Identify the quantum chemistry program that generated the output file
     * @param file Path to the input quantum chemistry file
     * @return String representation of the program (e.g. "Gaussian", "ORCA", etc.)
     */
    std::string identify_program(const std::string& file);

    /**
     * @brief Convert CommandContext to SystemData for thermo module
     * @param context ComChemKit command context
     * @param input_file Path to input file
     * @return SystemData structure for thermo calculations
     */
    void* create_system_data(const ThermoSettings& settings, const CommandContext& context, const std::string& input_file);

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
     * @brief Display detailed molecular information
     * @param sys SystemData containing molecular information
     * @param nimag Number of imaginary frequencies
     */
    void display_molecular_info(const SystemData& sys, int nimag);
}

#endif // THERMO_H
