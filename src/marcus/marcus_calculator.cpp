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
#include <cctype>
#include <algorithm>
#include <regex>
#include <filesystem>

namespace Marcus {

double calculate_phase_correction(double temp, double concentration_mol_m3);

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
                                          int occurrence = -1,
                                          bool* found = nullptr) {
    if (found) *found = false;
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
                double value = std::stod(field);
                if (!std::isfinite(value)) return 0.0;
                if (found) *found = true;
                return value;
            } catch (...) {
                return 0.0;
            }
        }
    }
    return 0.0;
}

// ---------------------------------------------------------------------------
// extract_electronic_energy (+ source variant)
// ---------------------------------------------------------------------------
static bool content_has_scrf(const std::string& content);

bool extract_electronic_energy_src(const std::string& log_path,
                                   double& energy, std::string& source_out,
                                   bool gas_phase, bool* pcm_present, bool* scrf_present) {
    std::string content = read_file_content(log_path);
    if (pcm_present) *pcm_present = content.find("After PCM corrections, the energy is") != std::string::npos;
    if (scrf_present) *scrf_present = content_has_scrf(content);
    if (content.empty()) return false;

    bool found = false;
    if (!gas_phase) {
        // Solution: prefer the last PCM-corrected energy, falling back to SCF.
        double scf_equi = extract_field_after_pattern(content,
                                                      "After PCM corrections, the energy is",
                                                      7, -1, &found);
        if (found) {
            energy = scf_equi;
            source_out = "After PCM";
            return true;
        }
    }

    // Gas: use only SCF Done. Solution: use it as the fallback.
    double scf_done = extract_field_after_pattern(content,
                                                  "SCF Done:",
                                                  5, -1, &found);
    if (found) {
        energy = scf_done;
        source_out = "SCF Done";
        return true;
    }

    return false;
}

bool extract_electronic_energy(const std::string& log_path, double& energy, bool gas_phase) {
    std::string src;
    return extract_electronic_energy_src(log_path, energy, src, gas_phase);
}

// Detect SCRF (case-insensitive "scrf"), mirroring high-au/high-kJ has_scrf.
static bool content_has_scrf(const std::string& content) {
    std::string lower = content;
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower.find("scrf") != std::string::npos;
}

// Lowest frequency: minimum value on any "Frequencies --" line.
static double extract_lowest_frequency_content(const std::string& content, bool* available = nullptr) {
    std::istringstream iss(content);
    std::string line;
    double best = 0.0;
    bool found = false;
    while (std::getline(iss, line)) {
        if (line.find("Frequencies --") == std::string::npos) continue;
        size_t p = line.find("--");
        if (p == std::string::npos) continue;
        std::istringstream ls(line.substr(p + 2));
        double v;
        while (ls >> v) {
            if (!found || v < best) { best = v; found = true; }
        }
    }
    if (available) *available = found;
    return found ? best : 0.0;
}

bool extract_full_thermal_step1(const std::string& log_path,
                                 double& e_low, double& zpe, double& tc_e,
                                 double& tc_h, double& tc_g,
                                 double& lowest_freq, double& temp_k) {
    std::string content = read_file_content(log_path);
    if (content.empty()) return false;
    e_low  = extract_field_after_pattern(content, "SCF Done:", 5, -1);
    zpe    = extract_field_after_pattern(content, "Zero-point correction", 3, -1);
    tc_e   = extract_field_after_pattern(content, "Thermal correction to Energy", 5, -1);
    tc_h   = extract_field_after_pattern(content, "Thermal correction to Enthalpy", 5, -1);
    bool found_g = false;
    tc_g   = extract_field_after_pattern(content,
                                          "Thermal correction to Gibbs Free Energy", 7, -1, &found_g);
    lowest_freq = extract_lowest_frequency_content(content);
    double t = extract_field_after_pattern(content, "Kelvin.", 2, -1);
    // "Temperature 298.150 Kelvin. Pressure ...": field layout puts T elsewhere;
    // fall back to the high-level pattern "Kelvin.  Pressure" field search via Temperature.
    if (t == 0.0) t = extract_field_after_pattern(content, "Temperature", 2, 0);
    temp_k = t;
    return found_g;
}

double per_state_phase_correction(bool has_scrf, double temp_k, double concentration_m) {
    if (!has_scrf) return 0.0;
    if (temp_k <= 0.0) temp_k = 298.15;
    if (concentration_m <= 0.0) concentration_m = 1.0;
    return calculate_phase_correction(temp_k, concentration_m * 1000.0);
}

// ---------------------------------------------------------------------------
// extract_thermal_correction_gibbs
// ---------------------------------------------------------------------------
bool extract_thermal_correction_gibbs(const std::string& log_path, double& tc_gibbs) {
    std::string content = read_file_content(log_path);
    if (content.empty()) return false;

    bool found = false;
    tc_gibbs = extract_field_after_pattern(content,
                                           "Thermal correction to Gibbs Free Energy",
                                           7, -1, &found);
    return found;
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
    // Matches HighLevelEnergyCalculator: RT ln(C RT/Po), J/mol -> Hartree (/1000 for kJ).
    return rt * std::log(ratio) * PHASE_CORR_FACTOR / 1000.0;
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
                     bool output_otm,
                     bool step2_gas,
                     bool fully_gas) {
    std::string step1_log = step1_path(dir_step1, state.log_file);
    std::string step2_log = step2_path(dir_step2, state.log_file);

    // --- High-level electronic energy from step-2 log ---
    double e_high = 0.0;
    std::string e_src;
    if (!extract_electronic_energy_src(step2_log, e_high, e_src, step2_gas)) {
        std::cerr << "Error: Could not extract " << (step2_gas ? "SCF Done energy" : "electronic energy")
                  << " from " << step2_log << std::endl;
        state.has_energy = false;
        return false;
    }
    state.has_energy = true;
    state.e_high = e_high;
    state.energy_elec = e_high;  // compat alias
    state.e_high_source = e_src;

    std::string c1 = read_file_content(step1_log);
    std::string c2 = read_file_content(step2_log);
    state.has_scrf = content_has_scrf(c1) || content_has_scrf(c2);

    // --- Low-level thermal block from step-1 log ---
    double e_low = 0.0, zpe = 0.0, tc_e = 0.0, tc_h = 0.0, tc_g = 0.0;
    double lf = 0.0, log_t = 0.0;
    bool have_string = extract_full_thermal_step1(step1_log, e_low, zpe, tc_e, tc_h, tc_g, lf, log_t);
    extract_field_after_pattern(c1, "SCF Done:", 5, -1, &state.has_e_low);
    extract_field_after_pattern(c1, "Zero-point correction", 3, -1, &state.has_zpe);
    extract_field_after_pattern(c1, "Thermal correction to Energy", 5, -1, &state.has_tc_energy);
    extract_field_after_pattern(c1, "Thermal correction to Enthalpy", 5, -1, &state.has_tc_enthalpy);
    extract_lowest_frequency_content(c1, &state.has_lowest_freq);
    state.thermo_fallback = false;
    state.thermo_recalculated = false;
    state.temperature_from_log = false;

    double T_used = temperature;
    bool thermo_used = false;
    if (use_thermo) {
        double tc_g_th = 0.0, tc_h_th = 0.0, zpe_th = 0.0;
        bool thermo_ok = false;
        if (extract_thermal_correction_gibbs_thermo(step1_log, temperature, pressure,
                                                    concentration_m, low_vib_method, ravib,
                                                    scale_zpe, scale_heat, scale_entropy, scale_cv,
                                                    interp_vib, imag_real, external_energy,
                                                    mass_mode, prt_level, print_vib, ip_mode,
                                                    point_group, hg_entropy, bav_preset, output_otm,
                                                    tc_g_th, tc_h_th, zpe_th, thermo_ok) && thermo_ok) {
            // Thermo module gives ZPE/corrG/corrH at user T; derive TC_E as H_corr - RT.
            double rt_au = (R_CONSTANT * temperature) / (HARTREE_TO_KJ_MOL * 1000.0);
            tc_g = tc_g_th;
            tc_h = tc_h_th;
            zpe = zpe_th;
            tc_e = tc_h_th - rt_au;
            // lf from thermo when available: re-read via basic properties is inside
            // extract_thermal_correction_gibbs_thermo; keep string lf as fallback.
            T_used = temperature;
            thermo_used = true;
            state.thermo_recalculated = true;
            state.has_zpe = state.has_tc_energy = state.has_tc_enthalpy = true;
            have_string = true;
        } else {
            state.thermo_fallback = true;
            std::cerr << "Warning: Thermo-module recalculation failed for " << step1_log
                      << "; falling back to values read from file" << std::endl;
        }
    }
    if (!thermo_used) {
        state.temperature_from_log = log_t > 0.0;
        T_used = state.temperature_from_log ? log_t : temperature;
    }

    if (!have_string) {
        std::cerr << "Warning: Could not extract TC_G from " << step1_log
                  << "; Gibbs energy and barrier are unavailable" << std::endl;
        state.tc_gibbs = 0.0;
        state.has_tc_gibbs = false;
    } else {
        state.has_tc_gibbs = true;
    }
    state.e_low = e_low;
    state.zpe = zpe;
    state.tc_energy = tc_e;
    state.tc_enthalpy = tc_h;
    state.tc_gibbs = tc_g;
    state.tc_only = tc_e - zpe;
    state.ts_value = tc_h - tc_g;
    state.lowest_freq = lf;
    state.temperature_used = T_used;
    state.has_full_thermal = state.has_zpe && state.has_tc_energy &&
                             state.has_tc_enthalpy && state.has_tc_gibbs;

    // High-au identities: H = E_high + TC_H, G_raw = E_high + TC_G.
    state.h_state = e_high + tc_h;
    state.g_raw = e_high + tc_g;
    state.phase_temperature_used = T_used > 0.0 ? T_used : 298.15;
    state.phase_concentration_used = concentration_m > 0.0 ? concentration_m : 1.0;
    // A configured fully gas-phase calculation has no solution standard-state
    // correction. Any conflicting SCRF text is reported separately.
    state.phase_corr = fully_gas ? 0.0 : per_state_phase_correction(state.has_scrf, T_used, concentration_m);
    state.g_corr = state.g_raw + state.phase_corr;
    state.g_state = state.g_corr;  // final solution-phase Gibbs (matches high-kJ)
    return true;
}

// ---------------------------------------------------------------------------
// compute_reaction_free_energy
// ---------------------------------------------------------------------------
bool compute_reaction_free_energy(MarcusProject& project,
                                  double& dG_r_hartree, double& dG_r_kcal) {
    // Thermo recalc when user overrides T/C/P (mirrors high-au single-file rule).
    bool use_thermo = project.use_input_temp || project.use_input_concentration ||
                       project.use_input_pressure;

    // Use the same electronic-energy policy and phase convention for every state.
    bool step2_gas = project.step2_solvent.empty();
    bool fully_gas = project.step1_solvent.empty() && step2_gas;
    auto compute_state = [&](MarcusState& state) {
        return compute_g_state(state, project.dir_step1_opt, project.dir_step2_sp,
                               use_thermo, project.temperature, project.pressure,
                               project.concentration, project.low_vib_method, project.ravib,
                               project.scale_zpe, project.scale_heat, project.scale_entropy, project.scale_cv,
                               project.interp_vib, project.imag_real, project.external_energy,
                               project.mass_mode, project.prt_level, project.print_vib, project.ip_mode,
                               project.point_group, project.hg_entropy, project.bav_preset, project.output_otm,
                               step2_gas, fully_gas);
    };
    if (!compute_state(project.donor.init_state) || !compute_state(project.donor.final_state) ||
        !compute_state(project.acceptor.init_state) || !compute_state(project.acceptor.final_state))
        return false;

    // Delta G°r from phase-corrected G (= g_state/g_corr, matches high-kJ).
    // g_state is the solution-phase Gibbs; g_raw is kept for audit.
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
    if (!extract_electronic_energy(step2_path(dir_step2, project.donor.init_state.log_file), E_R_D_qR_D, project.step2_solvent.empty())) {
        std::cerr << "Error: Missing electronic energy for D-init equilibrium" << std::endl;
        return false;
    }
    if (!extract_electronic_energy(step2_path(dir_step2, project.donor.final_state.log_file), E_P_D_qP_D, project.step2_solvent.empty())) {
        std::cerr << "Error: Missing electronic energy for D-final equilibrium" << std::endl;
        return false;
    }

    // Acceptor equilibrium energies
    if (!extract_electronic_energy(step2_path(dir_step2, project.acceptor.init_state.log_file), E_R_A_qR_A, project.step2_solvent.empty())) {
        std::cerr << "Error: Missing electronic energy for A-init equilibrium" << std::endl;
        return false;
    }
    if (!extract_electronic_energy(step2_path(dir_step2, project.acceptor.final_state.log_file), E_P_A_qP_A, project.step2_solvent.empty())) {
        std::cerr << "Error: Missing electronic energy for A-final equilibrium" << std::endl;
        return false;
    }

    // Donor cross energies
    std::string cross_D_fi = project.donor.name + state_suffix(project.donor.final_state) + "_final-from-"
                           + project.donor.init_state.name + state_suffix(project.donor.init_state) + "_init.log";
    if (!extract_electronic_energy(step2_path(dir_step2, cross_D_fi), E_P_D_qR_D, project.step2_solvent.empty())) {
        std::cerr << "Error: Missing electronic energy for " << cross_D_fi << std::endl;
        return false;
    }

    std::string cross_D_if = project.donor.name + state_suffix(project.donor.init_state) + "_init-from-"
                           + project.donor.final_state.name + state_suffix(project.donor.final_state) + "_final.log";
    if (!extract_electronic_energy(step2_path(dir_step2, cross_D_if), E_R_D_qP_D, project.step2_solvent.empty())) {
        std::cerr << "Error: Missing electronic energy for " << cross_D_if << std::endl;
        return false;
    }

    // Acceptor cross energies
    std::string cross_A_fi = project.acceptor.name + state_suffix(project.acceptor.final_state) + "_final-from-"
                           + project.acceptor.init_state.name + state_suffix(project.acceptor.init_state) + "_init.log";
    if (!extract_electronic_energy(step2_path(dir_step2, cross_A_fi), E_P_A_qR_A, project.step2_solvent.empty())) {
        std::cerr << "Error: Missing electronic energy for " << cross_A_fi << std::endl;
        return false;
    }

    std::string cross_A_if = project.acceptor.name + state_suffix(project.acceptor.init_state) + "_init-from-"
                           + project.acceptor.final_state.name + state_suffix(project.acceptor.final_state) + "_final.log";
    if (!extract_electronic_energy(step2_path(dir_step2, cross_A_if), E_R_A_qP_A, project.step2_solvent.empty())) {
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

    // Audit trail is filled by calculate_barrier_step3; keep this helper lean.
    (void)E_R_D_qR_D; (void)E_P_D_qP_D; (void)E_R_A_qR_A; (void)E_P_A_qP_A;
    return true;
}

// Full electronic audit trail for Section B of the report.
static bool collect_electronic_set(const MarcusProject& project,
                                   const std::string& dir_step2,
                                   MarcusElectronicSet& out) {
    out.f_R_D_qR_D = step2_path(dir_step2, project.donor.init_state.log_file);
    out.f_P_D_qP_D = step2_path(dir_step2, project.donor.final_state.log_file);
    out.f_R_A_qR_A = step2_path(dir_step2, project.acceptor.init_state.log_file);
    out.f_P_A_qP_A = step2_path(dir_step2, project.acceptor.final_state.log_file);
    out.f_P_D_qR_D = step2_path(dir_step2, project.donor.name +
        state_suffix(project.donor.final_state) + "_final-from-" +
        project.donor.init_state.name + state_suffix(project.donor.init_state) + "_init.log");
    out.f_R_D_qP_D = step2_path(dir_step2, project.donor.name +
        state_suffix(project.donor.init_state) + "_init-from-" +
        project.donor.final_state.name + state_suffix(project.donor.final_state) + "_final.log");
    out.f_P_A_qR_A = step2_path(dir_step2, project.acceptor.name +
        state_suffix(project.acceptor.final_state) + "_final-from-" +
        project.acceptor.init_state.name + state_suffix(project.acceptor.init_state) + "_init.log");
    out.f_R_A_qP_A = step2_path(dir_step2, project.acceptor.name +
        state_suffix(project.acceptor.init_state) + "_init-from-" +
        project.acceptor.final_state.name + state_suffix(project.acceptor.final_state) + "_final.log");
    bool gas_phase = project.step2_solvent.empty();
    out.pcm_lines_in_gas_logs.clear();
    out.scrf_text_in_gas_logs.clear();
    auto collect = [&](const std::string& path, double& value, std::string& source) {
        bool pcm_present = false, scrf_present = false;
        bool found = extract_electronic_energy_src(path, value, source, gas_phase, &pcm_present,
                                                   gas_phase ? &scrf_present : nullptr);
        if (gas_phase && pcm_present) out.pcm_lines_in_gas_logs.push_back(path);
        if (gas_phase && scrf_present) out.scrf_text_in_gas_logs.push_back(path);
        if (!found)
            std::cerr << "Error: Could not extract " << (gas_phase ? "SCF Done energy" : "electronic energy")
                      << " from " << path << std::endl;
        return found;
    };
    bool ok = true;
    ok &= collect(out.f_R_D_qR_D, out.E_R_D_qR_D, out.s_R_D_qR_D);
    ok &= collect(out.f_P_D_qP_D, out.E_P_D_qP_D, out.s_P_D_qP_D);
    ok &= collect(out.f_R_A_qR_A, out.E_R_A_qR_A, out.s_R_A_qR_A);
    ok &= collect(out.f_P_A_qP_A, out.E_P_A_qP_A, out.s_P_A_qP_A);
    ok &= collect(out.f_P_D_qR_D, out.E_P_D_qR_D, out.s_P_D_qR_D);
    ok &= collect(out.f_R_D_qP_D, out.E_R_D_qP_D, out.s_R_D_qP_D);
    ok &= collect(out.f_P_A_qR_A, out.E_P_A_qR_A, out.s_P_A_qR_A);
    ok &= collect(out.f_R_A_qP_A, out.E_R_A_qP_A, out.s_R_A_qP_A);
    out.success = ok;
    return ok;
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
    result.barrier_sym_valid = false;
    result.barrier_asym_valid = false;

    double lambda = result.lambda_avg_kcal;

    // Symmetric barrier: (lambda + dG)^2 / (4 lambda)
    if (lambda_R_kcal > 0.0 && lambda_P_kcal > 0.0 && std::isfinite(lambda) && std::isfinite(dG_r_kcal)) {
        result.barrier_sym_kcal = (lambda + dG_r_kcal) * (lambda + dG_r_kcal) / (4.0 * lambda);
        result.barrier_sym_valid = std::isfinite(result.barrier_sym_kcal);
    } else {
        result.barrier_sym_kcal = 0.0;
        std::cerr << "Warning: non-positive or invalid reorganization energy; symmetric barrier unavailable" << std::endl;
    }

    // Asymmetric barrier:
    // Delta G‡ = lambdaR * [(-lambdaP + sqrt(lambdaR*lambdaP + (lambdaR-lambdaP)*dG)) / (lambdaR - lambdaP)]^2
    double diff = lambda_R_kcal - lambda_P_kcal;
    if (!result.barrier_sym_valid) {
        result.barrier_asym_kcal = 0.0;
    } else if (std::fabs(diff) > 1e-10) {
        double inner = lambda_R_kcal * lambda_P_kcal + diff * dG_r_kcal;
        if (inner >= 0.0) {
            double sqrt_inner = std::sqrt(inner);
            double numerator = -lambda_P_kcal + sqrt_inner;
            double ratio = numerator / diff;
            result.barrier_asym_kcal = lambda_R_kcal * ratio * ratio;
            result.barrier_asym_valid = std::isfinite(result.barrier_asym_kcal);
        } else {
            result.barrier_asym_kcal = 0.0;
            std::cerr << "Warning: asymmetric barrier inner term negative, not computable" << std::endl;
        }
    } else {
        // lambdaR == lambdaP, falls back to symmetric
        result.barrier_asym_kcal = result.barrier_sym_kcal;
        result.barrier_asym_valid = result.barrier_sym_valid;
    }

    result.success = true;
    return true;
}

// ---------------------------------------------------------------------------
// calculate_barrier_step3
// ---------------------------------------------------------------------------
bool calculate_barrier_step3(MarcusProject& project, MarcusResult& result) {
    // Per-state Gibbs already includes phase (high-au rule), so dG here is corrected.
    double dG_corr_h = 0.0, dG_corr_kcal = 0.0;
    if (!compute_reaction_free_energy(project, dG_corr_h, dG_corr_kcal)) {
        result.success = false;
        result.error_message = "Failed to compute reaction free energy";
        return false;
    }
    result.gibbs_valid = project.donor.init_state.has_tc_gibbs && project.donor.final_state.has_tc_gibbs &&
                         project.acceptor.init_state.has_tc_gibbs && project.acceptor.final_state.has_tc_gibbs;
    // Raw dG for audit: sum of g_raw instead of g_state.
    double dG_raw_h = (project.donor.final_state.g_raw + project.acceptor.final_state.g_raw) -
                      (project.donor.init_state.g_raw + project.acceptor.init_state.g_raw);
    result.dG_r_hartree_raw = dG_raw_h;
    result.dG_r_kcal_raw = dG_raw_h * HARTREE_TO_KCAL_MOL;
    result.dG_r_hartree = dG_corr_h;
    result.dG_r_kcal = dG_corr_kcal;
    result.gibbs_hartree_corrected = dG_corr_h;
    // Net phase on dG (cancels to ~0 when all 4 states share T/C/SCRF).
    result.phase_correction = dG_corr_h - dG_raw_h;
    result.phase_corr_applied =
        (project.donor.init_state.phase_corr != 0.0 ||
         project.donor.final_state.phase_corr != 0.0 ||
         project.acceptor.init_state.phase_corr != 0.0 ||
         project.acceptor.final_state.phase_corr != 0.0);

    // Full electronic audit trail (Section B).
    if (!collect_electronic_set(project, project.dir_step2_sp, result.elec)) {
        result.success = false;
        result.error_message = "Failed to collect electronic energies";
        return false;
    }

    // Compute reorganization energies
    double lambda_R = 0.0, lambda_P = 0.0;
    if (!compute_reorganization_energies(project, project.dir_step2_sp, lambda_R, lambda_P)) {
        result.success = false;
        result.error_message = "Failed to compute reorganization energies";
        return false;
    }
    result.lambda_R_hartree = lambda_R / HARTREE_TO_KCAL_MOL;
    result.lambda_P_hartree = lambda_P / HARTREE_TO_KCAL_MOL;

    // Barriers use the corrected (solution-phase) dG, matching high-kJ Gibbs.
    if (!compute_barriers(dG_corr_kcal, lambda_R, lambda_P, result)) {
        result.success = false;
        result.error_message = "Failed to compute barriers";
        return false;
    }
    if (!result.gibbs_valid) {
        result.barrier_sym_valid = false;
        result.barrier_asym_valid = false;
    }
    // compute_barriers overwrote dG fields; restore both raw and corrected.
    result.dG_r_hartree = dG_corr_h;
    result.dG_r_kcal = dG_corr_kcal;

    return true;
}

}  // namespace Marcus