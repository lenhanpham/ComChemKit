/**
 * @file marcus_calculator.cpp
 * @brief Implementation of energy extraction and Marcus barrier calculation.
 */

#include "marcus/marcus_calculator.h"
#include "marcus/marcus_io.h"
#include "thermo/thermo.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cmath>
#include <regex>
#include <filesystem>

namespace Marcus {

// ---------------------------------------------------------------------------
// Helper: read entire file content
// ---------------------------------------------------------------------------
static std::string read_file_content(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

// ---------------------------------------------------------------------------
// Extract a floating-point value after a pattern, from a specific field
// ---------------------------------------------------------------------------
static double extract_field_after_pattern(const std::string& content,
                                          const std::string& pattern,
                                          int field_index_1based,
                                          int occurrence = -1) {
    // Find all line positions containing the pattern
    std::vector<size_t> line_starts;
    size_t pos = 0;
    while ((pos = content.find(pattern, pos)) != std::string::npos) {
        // Find start of line
        size_t line_start = pos;
        while (line_start > 0 && content[line_start - 1] != '\n') --line_start;
        line_starts.push_back(line_start);
        pos += pattern.length();
    }

    if (line_starts.empty()) return 0.0;

    // Determine which occurrence to use
    size_t idx;
    if (occurrence == -1) {
        idx = line_starts.size() - 1;  // last occurrence
    } else {
        idx = static_cast<size_t>(occurrence);
        if (idx >= line_starts.size()) return 0.0;
    }

    // Find end of line
    size_t line_start = line_starts[idx];
    size_t line_end = content.find('\n', line_start);
    if (line_end == std::string::npos) line_end = content.length();
    std::string line = content.substr(line_start, line_end - line_start);

    // Split into whitespace-delimited fields
    std::istringstream iss(line);
    std::string field;
    int field_count = 0;
    while (iss >> field) {
        ++field_count;
        if (field_count == field_index_1based) {
            // Replace D/d with E for Fortran-style floats
            std::replace(field.begin(), field.end(), 'D', 'E');
            std::replace(field.begin(), field.end(), 'd', 'e');
            try {
                return std::stod(field);
            } catch (...) {
                return 0.0;
            }
        }
    }
    return 0.0;
}

// ---------------------------------------------------------------------------
// extract_electronic_energy
// ---------------------------------------------------------------------------
bool extract_electronic_energy(const std::string& log_path, double& energy) {
    std::string content = read_file_content(log_path);
    if (content.empty()) return false;

    // Priority 1: "After PCM corrections, the energy is" -> field 7
    double scf_equi = extract_field_after_pattern(content,
                                                  "After PCM corrections, the energy is",
                                                  7, -1);
    if (scf_equi != 0.0) {
        energy = scf_equi;
        return true;
    }

    // Priority 2: "SCF Done:" -> field 5
    double scf_done = extract_field_after_pattern(content,
                                                  "SCF Done:",
                                                  5, -1);
    if (scf_done != 0.0) {
        energy = scf_done;
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// extract_thermal_correction_gibbs
// ---------------------------------------------------------------------------
bool extract_thermal_correction_gibbs(const std::string& log_path, double& tc_gibbs) {
    std::string content = read_file_content(log_path);
    if (content.empty()) return false;

    tc_gibbs = extract_field_after_pattern(content,
                                           "Thermal correction to Gibbs Free Energy",
                                           7, -1);
    if (tc_gibbs == 0.0) {
        // Try fallback: "Thermal correction to Gibbs Free Energy=" 
        tc_gibbs = extract_field_after_pattern(content,
                                               "Thermal correction to Gibbs Free Energy=",
                                               1, -1);
    }
    return tc_gibbs != 0.0;
}

/**
 * @brief Extract thermal corrections via the thermo module (pathway used when
 *        user specifies -t or -c). Falls back to string-matching if thermo
 *        module cannot extract frequency data.
 * Mirrors HighLevelEnergyCalculator::extract_low_level_thermal_data_thermo.
 */
bool extract_thermal_correction_gibbs_thermo(const std::string& log_path,
                                             double temperature,
                                             double pressure,
                                             double concentration_m,
                                             const std::string& low_vib_method,
                                             double ravib,
                                             double scale_zpe,
                                             double scale_heat,
                                             double scale_entropy,
                                             double scale_cv,
                                             double interp_vib,
                                             double imag_real,
                                             double external_energy,
                                             int mass_mode,
                                             int prt_level,
                                             int print_vib,
                                             int ip_mode,
                                             const std::string& point_group,
                                             bool hg_entropy,
                                             const std::string& bav_preset,
                                             bool output_otm,
                                             double& tc_gibbs_out,
                                             double& tc_enthalpy_out,
                                             double& zpe_out,
                                             bool& success) {
    // Build full ThermoSettings from all thermo parameters
    ThermoInterface::ThermoSettings settings;
    settings.temperature = temperature;
    settings.pressure = pressure;
    settings.concentration = std::to_string(concentration_m);
    settings.low_vib_treatment = low_vib_method;
    settings.raise_vib = ravib;
    settings.scale_zpe = scale_zpe;
    settings.scale_heat = scale_heat;
    settings.scale_entropy = scale_entropy;
    settings.scale_cv = scale_cv;
    settings.interp_vib = interp_vib;
    settings.imag_real = imag_real;
    settings.external_energy = external_energy;
    settings.mass_mode = mass_mode;
    settings.prt_level = prt_level;
    settings.print_vib = print_vib;
    settings.ip_mode = ip_mode;
    settings.point_group = point_group;
    settings.hg_entropy = hg_entropy;
    settings.bav_preset = bav_preset;
    settings.output_otm = output_otm;

    double scf_au = 0.0, corrG_au = 0.0, corrH_au = 0.0, zpe_au = 0.0, lf_cm = 0.0;
    int    nfreq = 0;
    std::string prog_name;

    bool ok = ThermoInterface::extract_basic_properties(
        log_path, scf_au, corrG_au, corrH_au, zpe_au, lf_cm, nfreq, prog_name, settings);

    if (!ok || nfreq == 0) {
        // Fall back to string-matching from the log file
        tc_gibbs_out = 0.0;
        tc_enthalpy_out = 0.0;
        zpe_out = 0.0;
        success = false;
        return false;
    }

    tc_gibbs_out = corrG_au;
    tc_enthalpy_out = corrH_au;
    zpe_out = zpe_au;
    success = true;
    return true;
}

/**
 * @brief Calculate phase correction for concentration effects (Hartree).
 * Mirrors HighLevelEnergyCalculator::calculate_phase_correction.
 * Delta_G_corr = RT * ln(C * RT / Po) * PHASE_CORR_FACTOR
 * where C is in mol/m³, RT in J/mol, Po in N/m².
 */
double calculate_phase_correction(double temp, double concentration_mol_m3) {
    double rt = R_CONSTANT * temp;
    double ratio = concentration_mol_m3 * rt / PO_CONSTANT;
    if (ratio <= 0.0) return 0.0;
    return rt * std::log(ratio) * PHASE_CORR_FACTOR;
}

// ---------------------------------------------------------------------------
// compute_g_state
// ---------------------------------------------------------------------------
bool compute_g_state(MarcusState& state,
                     const std::string& dir_step1,
                     const std::string& dir_step2,
                     bool use_thermo,
                     double temperature,
                     double pressure,
                     double concentration_m,
                     const std::string& low_vib_method,
                     double ravib,
                     double scale_zpe,
                     double scale_heat,
                     double scale_entropy,
                     double scale_cv,
                     double interp_vib,
                     double imag_real,
                     double external_energy,
                     int mass_mode,
                     int prt_level,
                     int print_vib,
                     int ip_mode,
                     const std::string& point_group,
                     bool hg_entropy,
                     const std::string& bav_preset,
                     bool output_otm) {
    std::string step1_log = step1_path(dir_step1, state.log_file);
    std::string step2_log = step2_path(dir_step2, state.log_file);

    // Extract electronic energy from step-2 log
    if (!extract_electronic_energy(step2_log, state.energy_elec)) {
        std::cerr << "Error: Could not extract electronic energy from " << step2_log << std::endl;
        state.has_energy = false;
        return false;
    }
    state.has_energy = true;

    // Extract thermal correction to Gibbs from step-1 log
    // Two pathways (mirrors high-kj/high-au):
    // 1) Thermo module pathway: when -t or -c specified, recalculate at user T
    // 2) String-matching pathway: read pre-calculated TC_G from log file
    if (use_thermo) {
        double tc_gibbs_thermo = 0.0, tc_enthalpy_thermo = 0.0, zpe_thermo = 0.0;
        bool thermo_ok = false;
        if (extract_thermal_correction_gibbs_thermo(step1_log, temperature, pressure,
                                                    concentration_m, low_vib_method, ravib,
                                                    scale_zpe, scale_heat, scale_entropy, scale_cv,
                                                    interp_vib, imag_real, external_energy,
                                                    mass_mode, prt_level, print_vib, ip_mode,
                                                    point_group, hg_entropy, bav_preset, output_otm,
                                                    tc_gibbs_thermo, tc_enthalpy_thermo,
                                                    zpe_thermo, thermo_ok) && thermo_ok) {
            state.tc_gibbs = tc_gibbs_thermo;
            state.has_tc_gibbs = true;
        } else {
            // Fall back to string-matching
            std::cerr << "Warning: Thermo-module recalculation failed for " << step1_log
                      << "; falling back to values read from file" << std::endl;
            if (!extract_thermal_correction_gibbs(step1_log, state.tc_gibbs)) {
                std::cerr << "Warning: Could not extract TC_G from " << step1_log
                          << " (will use 0)" << std::endl;
                state.tc_gibbs = 0.0;
                state.has_tc_gibbs = false;
            } else {
                state.has_tc_gibbs = true;
            }
        }
    } else {
        if (!extract_thermal_correction_gibbs(step1_log, state.tc_gibbs)) {
            std::cerr << "Warning: Could not extract TC_G from " << step1_log
                      << " (will use 0)" << std::endl;
            state.tc_gibbs = 0.0;
            state.has_tc_gibbs = false;
        } else {
            state.has_tc_gibbs = true;
        }
    }

    // G_state = E_elec + TC_G
    state.g_state = state.energy_elec + state.tc_gibbs;
    return true;
}

// ---------------------------------------------------------------------------
// compute_reaction_free_energy
// ---------------------------------------------------------------------------
bool compute_reaction_free_energy(const MarcusProject& project,
                                  double& dG_r_hartree, double& dG_r_kcal) {
    // Determine whether to use thermo module pathway (-t or -c specified)
    bool use_thermo = project.use_input_temp || project.use_input_concentration;

    // Ensure all 4 states have G values
    if (!compute_g_state(const_cast<MarcusState&>(project.donor.init_state),
                         project.dir_step1_opt, project.dir_step2_sp,
                         use_thermo, project.temperature, project.pressure,
                         project.concentration, project.low_vib_method, project.ravib,
                         project.scale_zpe, project.scale_heat, project.scale_entropy, project.scale_cv,
                         project.interp_vib, project.imag_real, project.external_energy,
                         project.mass_mode, project.prt_level, project.print_vib, project.ip_mode,
                         project.point_group, project.hg_entropy, project.bav_preset, project.output_otm))
        return false;
    if (!compute_g_state(const_cast<MarcusState&>(project.donor.final_state),
                         project.dir_step1_opt, project.dir_step2_sp,
                         use_thermo, project.temperature, project.pressure,
                         project.concentration, project.low_vib_method, project.ravib,
                         project.scale_zpe, project.scale_heat, project.scale_entropy, project.scale_cv,
                         project.interp_vib, project.imag_real, project.external_energy,
                         project.mass_mode, project.prt_level, project.print_vib, project.ip_mode,
                         project.point_group, project.hg_entropy, project.bav_preset, project.output_otm))
        return false;
    if (!compute_g_state(const_cast<MarcusState&>(project.acceptor.init_state),
                         project.dir_step1_opt, project.dir_step2_sp,
                         use_thermo, project.temperature, project.pressure,
                         project.concentration, project.low_vib_method, project.ravib,
                         project.scale_zpe, project.scale_heat, project.scale_entropy, project.scale_cv,
                         project.interp_vib, project.imag_real, project.external_energy,
                         project.mass_mode, project.prt_level, project.print_vib, project.ip_mode,
                         project.point_group, project.hg_entropy, project.bav_preset, project.output_otm))
        return false;
    if (!compute_g_state(const_cast<MarcusState&>(project.acceptor.final_state),
                         project.dir_step1_opt, project.dir_step2_sp,
                         use_thermo, project.temperature, project.pressure,
                         project.concentration, project.low_vib_method, project.ravib,
                         project.scale_zpe, project.scale_heat, project.scale_entropy, project.scale_cv,
                         project.interp_vib, project.imag_real, project.external_energy,
                         project.mass_mode, project.prt_level, project.print_vib, project.ip_mode,
                         project.point_group, project.hg_entropy, project.bav_preset, project.output_otm))
        return false;

    // Delta G°r = [G(P_D) + G(P_A)] - [G(R_D) + G(R_A)]
    double G_P_D = project.donor.final_state.g_state;
    double G_P_A = project.acceptor.final_state.g_state;
    double G_R_D = project.donor.init_state.g_state;
    double G_R_A = project.acceptor.init_state.g_state;

    dG_r_hartree = (G_P_D + G_P_A) - (G_R_D + G_R_A);
    dG_r_kcal = dG_r_hartree * HARTREE_TO_KCAL_MOL;

    return true;
}

// ---------------------------------------------------------------------------
// compute_reorganization_energies
// ---------------------------------------------------------------------------
bool compute_reorganization_energies(const MarcusProject& project,
                                     const std::string& dir_step2,
                                     double& lambda_R_kcal, double& lambda_P_kcal) {
    // Donor energies:
    // E(R_D @ q_R_D) = equilibrium D-init log
    // E(P_D @ q_R_D) = cross D-final-from-D-init log
    // E(R_D @ q_P_D) = cross D-init-from-D-final log
    // E(P_D @ q_P_D) = equilibrium D-final log
    double E_R_D_qR_D, E_P_D_qR_D, E_R_D_qP_D, E_P_D_qP_D;
    double E_R_A_qR_A, E_P_A_qR_A, E_R_A_qP_A, E_P_A_qP_A;

    // Donor equilibrium energies
    if (!extract_electronic_energy(step2_path(dir_step2, project.donor.init_state.log_file), E_R_D_qR_D)) {
        std::cerr << "Error: Missing electronic energy for D-init equilibrium" << std::endl;
        return false;
    }
    if (!extract_electronic_energy(step2_path(dir_step2, project.donor.final_state.log_file), E_P_D_qP_D)) {
        std::cerr << "Error: Missing electronic energy for D-final equilibrium" << std::endl;
        return false;
    }

    // Acceptor equilibrium energies
    if (!extract_electronic_energy(step2_path(dir_step2, project.acceptor.init_state.log_file), E_R_A_qR_A)) {
        std::cerr << "Error: Missing electronic energy for A-init equilibrium" << std::endl;
        return false;
    }
    if (!extract_electronic_energy(step2_path(dir_step2, project.acceptor.final_state.log_file), E_P_A_qP_A)) {
        std::cerr << "Error: Missing electronic energy for A-final equilibrium" << std::endl;
        return false;
    }

    // Donor cross energies
    std::string cross_D_fi = project.donor.name + state_suffix(project.donor.final_state) + "_final-from-"
                           + project.donor.init_state.name + state_suffix(project.donor.init_state) + "_init.log";
    if (!extract_electronic_energy(step2_path(dir_step2, cross_D_fi), E_P_D_qR_D)) {
        std::cerr << "Error: Missing electronic energy for " << cross_D_fi << std::endl;
        return false;
    }

    std::string cross_D_if = project.donor.name + state_suffix(project.donor.init_state) + "_init-from-"
                           + project.donor.final_state.name + state_suffix(project.donor.final_state) + "_final.log";
    if (!extract_electronic_energy(step2_path(dir_step2, cross_D_if), E_R_D_qP_D)) {
        std::cerr << "Error: Missing electronic energy for " << cross_D_if << std::endl;
        return false;
    }

    // Acceptor cross energies
    std::string cross_A_fi = project.acceptor.name + state_suffix(project.acceptor.final_state) + "_final-from-"
                           + project.acceptor.init_state.name + state_suffix(project.acceptor.init_state) + "_init.log";
    if (!extract_electronic_energy(step2_path(dir_step2, cross_A_fi), E_P_A_qR_A)) {
        std::cerr << "Error: Missing electronic energy for " << cross_A_fi << std::endl;
        return false;
    }

    std::string cross_A_if = project.acceptor.name + state_suffix(project.acceptor.init_state) + "_init-from-"
                           + project.acceptor.final_state.name + state_suffix(project.acceptor.final_state) + "_final.log";
    if (!extract_electronic_energy(step2_path(dir_step2, cross_A_if), E_R_A_qP_A)) {
        std::cerr << "Error: Missing electronic energy for " << cross_A_if << std::endl;
        return false;
    }

    // lambda_P (product side): E(P at reactant geom) - E(P at product geom)
    //   = [E(P_D @ q_R_D) - E(P_D @ q_P_D)] + [E(P_A @ q_R_A) - E(P_A @ q_P_A)]
    double lambda_P_hartree = (E_P_D_qR_D - E_P_D_qP_D) + (E_P_A_qR_A - E_P_A_qP_A);

    // lambda_R (reactant side): E(R at product geom) - E(R at reactant geom)
    //   = [E(R_D @ q_P_D) - E(R_D @ q_R_D)] + [E(R_A @ q_P_A) - E(R_A @ q_R_A)]
    double lambda_R_hartree = (E_R_D_qP_D - E_R_D_qR_D) + (E_R_A_qP_A - E_R_A_qR_A);

    lambda_P_kcal = lambda_P_hartree * HARTREE_TO_KCAL_MOL;
    lambda_R_kcal = lambda_R_hartree * HARTREE_TO_KCAL_MOL;

    return true;
}

// ---------------------------------------------------------------------------
// compute_barriers
// ---------------------------------------------------------------------------
bool compute_barriers(double dG_r_kcal,
                      double lambda_R_kcal, double lambda_P_kcal,
                      MarcusResult& result) {
    result.dG_r_kcal = dG_r_kcal;
    result.lambda_R_kcal = lambda_R_kcal;
    result.lambda_P_kcal = lambda_P_kcal;
    result.lambda_avg_kcal = (lambda_R_kcal + lambda_P_kcal) / 2.0;

    double lambda = result.lambda_avg_kcal;

    // Symmetric barrier: (lambda + dG)^2 / (4 lambda)
    if (lambda > 0.0) {
        result.barrier_sym_kcal = (lambda + dG_r_kcal) * (lambda + dG_r_kcal) / (4.0 * lambda);
    } else {
        result.barrier_sym_kcal = 0.0;
        std::cerr << "Warning: lambda <= 0, symmetric barrier not computable" << std::endl;
    }

    // Asymmetric barrier:
    // Delta G‡ = lambdaR * [(-lambdaP + sqrt(lambdaR*lambdaP + (lambdaR-lambdaP)*dG)) / (lambdaR - lambdaP)]^2
    double diff = lambda_R_kcal - lambda_P_kcal;
    if (std::fabs(diff) > 1e-10) {
        double inner = lambda_R_kcal * lambda_P_kcal + diff * dG_r_kcal;
        if (inner >= 0.0) {
            double sqrt_inner = std::sqrt(inner);
            double numerator = -lambda_P_kcal + sqrt_inner;
            double ratio = numerator / diff;
            result.barrier_asym_kcal = lambda_R_kcal * ratio * ratio;
        } else {
            result.barrier_asym_kcal = 0.0;
            std::cerr << "Warning: asymmetric barrier inner term negative, not computable" << std::endl;
        }
    } else {
        // lambdaR == lambdaP, falls back to symmetric
        result.barrier_asym_kcal = result.barrier_sym_kcal;
    }

    result.success = true;
    return true;
}

// ---------------------------------------------------------------------------
// calculate_barrier_step3
// ---------------------------------------------------------------------------
bool calculate_barrier_step3(const MarcusProject& project, MarcusResult& result) {
    // Compute reaction free energy (with thermo pathway if -t/-c specified)
    double dG_r_hartree = 0.0, dG_r_kcal = 0.0;
    if (!compute_reaction_free_energy(project, dG_r_hartree, dG_r_kcal)) {
        result.success = false;
        result.error_message = "Failed to compute reaction free energy";
        return false;
    }
    result.dG_r_hartree = dG_r_hartree;

    // Apply phase correction when SCRF is used (all Marcus step-1 calcs use SCRF)
    // Phase correction: Delta_G_corr = RT * ln(C * RT / Po) * PHASE_CORR_FACTOR
    // Only meaningful when concentration/temperature are specified
    if (project.use_input_concentration || project.use_input_temp) {
        double concentration_mol_m3 = project.concentration * 1000.0;
        result.phase_correction = calculate_phase_correction(project.temperature, concentration_mol_m3);
        result.gibbs_hartree_corrected = dG_r_hartree + result.phase_correction;
        result.phase_corr_applied = true;
        dG_r_kcal = result.gibbs_hartree_corrected * HARTREE_TO_KCAL_MOL;
    } else {
        result.phase_correction = 0.0;
        result.gibbs_hartree_corrected = dG_r_hartree;
        result.phase_corr_applied = false;
    }

    // Compute reorganization energies
    double lambda_R = 0.0, lambda_P = 0.0;
    if (!compute_reorganization_energies(project, project.dir_step2_sp, lambda_R, lambda_P)) {
        result.success = false;
        result.error_message = "Failed to compute reorganization energies";
        return false;
    }

    // Compute barriers (using phase-corrected dG_r_kcal if applicable)
    if (!compute_barriers(dG_r_kcal, lambda_R, lambda_P, result)) {
        result.success = false;
        result.error_message = "Failed to compute barriers";
        return false;
    }

    return true;
}

}  // namespace Marcus