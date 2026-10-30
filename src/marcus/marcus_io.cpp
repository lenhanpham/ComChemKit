/**
 * @file marcus_io.cpp
 * @brief Implementation of I/O utilities for Marcus barrier calculations.
 */

#include "marcus/marcus_io.h"
#include "utilities/metadata.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <array>
#include <vector>

namespace Marcus {

bool create_project_directories(const std::string& project_name,
                                std::string& dir_step1,
                                std::string& dir_step2) {
    try {
        dir_step1  = project_name + "_step_1";
        dir_step2  = project_name + "_step_2";

        std::filesystem::create_directories(dir_step1);
        std::filesystem::create_directories(dir_step2);

        std::cout << "Created project directories:" << std::endl;
        std::cout << "  Step 1 (opt):  " << dir_step1 << std::endl;
        std::cout << "  Step 2 (sp):   " << dir_step2 << std::endl;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating project directories: " << e.what() << std::endl;
        return false;
    }
}

bool write_step1_manifest(const std::string& dir_step1,
                          const std::map<std::string, std::string>& states) {
    std::string manifest_path = dir_step1 + "/.marcus_manifest_step1";
    std::ofstream file(manifest_path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot write step-1 manifest: " << manifest_path << std::endl;
        return false;
    }
    for (const auto& [key, chk] : states) {
        file << key << "=" << chk << "\n";
    }
    file.close();
    return true;
}

std::map<std::string, std::string> read_step1_manifest(const std::string& dir_step1) {
    std::map<std::string, std::string> states;
    std::string manifest_path = dir_step1 + "/.marcus_manifest_step1";
    std::ifstream file(manifest_path);
    if (!file.is_open()) {
        return states;  // Empty if no manifest yet
    }
    std::string line;
    while (std::getline(file, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            states[key] = val;
        }
    }
    file.close();
    return states;
}

bool write_step2_manifest(const std::string& dir_step2,
                          const std::map<std::string, std::string>& inputs) {
    std::string manifest_path = dir_step2 + "/.marcus_manifest_step2";
    std::ofstream file(manifest_path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot write step-2 manifest: " << manifest_path << std::endl;
        return false;
    }
    for (const auto& [key, fname] : inputs) {
        file << key << "=" << fname << "\n";
    }
    file.close();
    return true;
}

std::map<std::string, std::string> read_step2_manifest(const std::string& dir_step2) {
    std::map<std::string, std::string> inputs;
    std::string manifest_path = dir_step2 + "/.marcus_manifest_step2";
    std::ifstream file(manifest_path);
    if (!file.is_open()) {
        return inputs;
    }
    std::string line;
    while (std::getline(file, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            inputs[key] = val;
        }
    }
    file.close();
    return inputs;
}

bool step1_checkpoint_exists(const std::string& dir_step1,
                             const std::string& chk_file) {
    return std::filesystem::exists(dir_step1 + "/" + chk_file);
}

bool step2_log_exists(const std::string& dir_step2,
                      const std::string& log_file) {
    return std::filesystem::exists(dir_step2 + "/" + log_file);
}

bool step1_log_exists(const std::string& dir_step1,
                      const std::string& log_file) {
    return std::filesystem::exists(dir_step1 + "/" + log_file);
}

// Use a local tm rather than ctime's shared buffer; preserve ctime-style spacing.
static std::string format_completion_footer() {
    const std::time_t now = std::time(nullptr);
    std::tm local_time{};
#ifdef _WIN32
    bool have_time = localtime_s(&local_time, &now) == 0;
#else
    bool have_time = localtime_r(&now, &local_time) != nullptr;
#endif
    std::ostringstream out;
    out << "\nCalculation completed at: ";
    if (have_time) {
        out << std::put_time(&local_time, "%a %b ") << std::setw(2) << local_time.tm_mday
            << std::put_time(&local_time, " %H:%M:%S %Y");
    } else {
        out << "time unavailable";
    }
    out << "\n\n                    ---------- Happy calculation ----------\n"
        << "                    ---- ComChemKit normally terminated ---\n";
    return out.str();
}

bool write_barrier_report(const MarcusProject& project,
                          const MarcusResult& result,
                          const std::string& out_file,
                          std::string* completion_footer) {
    const std::string report_path = out_file.empty()
        ? std::filesystem::path(project.name).filename().string() + ".log"
        : out_file;
    std::ofstream file(report_path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot write barrier report: " << report_path << std::endl;
        return false;
    }

    struct StateRow { const char* id; const char* role; const MarcusState* state; };
    const StateRow states[] = {
        {"D-R", "Donor initial", &project.donor.init_state},
        {"D-P", "Donor final", &project.donor.final_state},
        {"A-R", "Acceptor initial", &project.acceptor.init_state},
        {"A-P", "Acceptor final", &project.acceptor.final_state}
    };
    const auto& e = result.elec;
    auto section = [&file](const char* title) {
        file << "\n" << title << "\n"
             << "--------------------------------------------------------------------------\n";
    };
    auto text_or_none = [](const std::string& s) { return s.empty() ? std::string("(not specified)") : s; };
    auto number = [](double value, bool available, int precision = 9) {
        if (!available) return std::string("N/A");
        std::ostringstream s;
        s << std::fixed << std::setprecision(precision) << value;
        return s.str();
    };
    auto basis_name = [](const std::string& path) {
        // Gaussian basis paths can end in /N, which is an include directive.
        std::filesystem::path p(path);
        if (p.filename() == "N") return p.parent_path().filename().string() + "/N";
        return p.filename().string();
    };
    bool thermo_requested = project.use_input_temp || project.use_input_concentration || project.use_input_pressure;
    bool step1_gas = project.step1_solvent.empty();
    bool step2_gas = project.step2_solvent.empty();
    bool fully_gas = step1_gas && step2_gas;
    bool mixed_phase = step1_gas != step2_gas;
    bool any_thermo = false, any_log = false, any_fallback = false;
    for (const auto& row : states) {
        any_thermo |= row.state->thermo_recalculated;
        any_log |= !row.state->thermo_recalculated;
        any_fallback |= row.state->thermo_fallback;
    }

    file << Metadata::header();
    file << "Marcus Theory Barrier Calculation (Four-Point Method)\n"
         << "==========================================================================\n"
         << "Project          : " << project.name << "\n"
         << "Calculation type : " << (project.calc_type == CalcType::OSET ? "OSET" : "EnT") << "\n"
         << "Configured phase : " << (fully_gas ? "gas phase" : mixed_phase ? "mixed gas/solution" : "solution phase") << "\n"
         << "Barrier models   : symmetric and asymmetric\n"
         << "Report level     : " << (project.verbose_report ? "verbose" : "default") << "\n"
         << "Step 1 directory: " << project.dir_step1_opt << "\n"
         << "Step 2 directory: " << project.dir_step2_sp << "\n";

    section("LEVELS OF THEORY AND THERMOCHEMISTRY");
    file << "Theory settings below are configured in the input; log routes are not verified.\n\n";
    auto theory = [&file, &text_or_none, &basis_name](const char* title,
                   const std::string& functional, const std::string& basis,
                   const std::string& path, const std::string& tail,
                   const std::string& model, const std::string& solvent) {
        file << title << "\n"
             << "  Functional    : " << text_or_none(functional) << "\n"
             << "  Basis set     : " << text_or_none(basis) << "\n";
        if (!tail.empty()) file << "  Basis source  : configured tail (use -v for details)\n";
        else if (!path.empty()) file << "  Basis file    : " << basis_name(path) << "\n";
        file << "  Solvent model : " << (solvent.empty() ? "none (gas phase)" : text_or_none(model)) << "\n"
             << "  Solvent       : " << (solvent.empty() ? "none (gas phase)" : solvent) << "\n";
    };
    theory("Step 1 — optimization and frequencies", project.step1_functional, project.step1_basis,
           project.step1_basis_path, project.step1_tail, project.step1_solvent_model, project.step1_solvent);
    file << "\n";
    theory("Step 2 — single-point electronic energies", project.step2_functional, project.step2_basis,
           project.step2_basis_path, project.step2_tail, project.step2_solvent_model, project.step2_solvent);
    file << "  Job types     : equilibrium and cross (configured routes)\n"
         << "  Energy source : " << (step2_gas ? "SCF Done (gas phase)" : "After PCM, falling back to SCF Done") << "\n\n";
    file << "Thermochemistry\n"
         << "  Requested     : " << (thermo_requested ? "thermo module" : "step-1 log values") << "\n"
         << "  Actual source : " << (any_thermo && any_log ? "mixed; see state provenance below" :
                                      any_thermo ? "thermo module" : "step-1 log values") << "\n"
         << std::fixed << std::setprecision(2)
         << "  Input T       : " << project.temperature << " K\n"
         << std::setprecision(3)
         << "  Concentration : " << project.concentration
         << (fully_gas ? " mol/L (no solution-phase correction)\n" : " mol/L (phase correction)\n")
         << "  Input pressure: " << project.pressure << " atm (thermo recalculation only)\n"
         << "  Phase standard: " << (fully_gas ? "gas phase; no solution-phase correction" : "1 atm (fixed)") << "\n"
         << "  Low-vib method: " << project.low_vib_method << "; ravib=" << project.ravib << " cm^-1\n"
         << "  Scale factors : ZPE=" << project.scale_zpe << ", heat=" << project.scale_heat
         << ", entropy=" << project.scale_entropy << ", Cv=" << project.scale_cv << "\n"
         << "  Interpolation : " << project.interp_vib << " cm^-1; imaginary threshold="
         << project.imag_real << " cm^-1\n"
         << "  Other settings: mass mode=" << project.mass_mode << ", phase mode=" << project.ip_mode
         << ", external energy=" << std::setprecision(9) << project.external_energy << " Hartree\n"
         << "  Point group   : " << text_or_none(project.point_group)
         << "; HG entropy=" << (project.hg_entropy ? "on" : "off")
         << "; Bav=" << text_or_none(project.bav_preset) << "\n"
         << "  Low-vib, scaling and other thermo settings apply only to recalculated states.\n";

    section("RESULTS");
    file << (fully_gas ? "All reaction and activation energies use gas-phase Gibbs energies.\n"
                       : "All reaction and activation energies use phase-corrected Gibbs energies.\n")
         << "Atomic units (au) are Hartree.\n";
    struct ResultRow {
        const char* label;
        std::array<std::string, 4> values;
    };
    std::vector<ResultRow> rows;
    auto add_result = [&rows, &number](const char* label, double hartree, bool valid) {
        rows.push_back({label, {number(hartree * HARTREE_TO_KJ_MOL, valid, 4),
                                number(hartree * HARTREE_TO_KCAL_MOL, valid, 4),
                                number(hartree, valid, 9),
                                number(hartree * HARTREE_TO_EV, valid, 4)}});
    };
    add_result("Reaction free energy, Delta G°", result.dG_r_hartree, result.gibbs_valid);
    add_result("Reorganization energy, lambda R", result.lambda_R_hartree, result.elec.success);
    add_result("Reorganization energy, lambda P", result.lambda_P_hartree, result.elec.success);
    add_result("Average reorganization, lambda",
               (result.lambda_R_hartree + result.lambda_P_hartree) / 2.0, result.elec.success);
    add_result("Delta G‡ (symmetric)", result.barrier_sym_kcal / HARTREE_TO_KCAL_MOL, result.barrier_sym_valid);
    add_result("Delta G‡ (asymmetric)", result.barrier_asym_kcal / HARTREE_TO_KCAL_MOL, result.barrier_asym_valid);

    std::array<size_t, 4> widths{};
    for (const auto& row : rows)
        for (size_t i = 0; i < widths.size(); ++i)
            widths[i] = std::max(widths[i], row.values[i].size());
    const std::array<const char*, 4> units = {"kJ/mol", "kcal/mol", "au", "eV"};
    for (size_t row_index = 0; row_index < rows.size(); ++row_index) {
        if (row_index == 4) file << "\n";
        const auto& row = rows[row_index];
        std::string label(row.label);
        size_t label_width = 0;
        for (unsigned char c : label) if ((c & 0xC0) != 0x80) ++label_width;
        file << "  " << label << std::string(label_width < 34 ? 34 - label_width : 1, ' ') << std::right;
        for (size_t i = 0; i < units.size(); ++i) {
            if (i > 0) file << "   ";
            file << std::setw(static_cast<int>(widths[i])) << row.values[i] << " " << units[i];
        }
        file << "\n";
    }

    section("STATE ASSIGNMENTS AND THERMAL PROVENANCE");
    file << "R = initial/reactant state; P = final/product state.\n";
    for (const auto& row : states) {
        const auto& st = *row.state;
        file << "  " << row.id << "  " << row.role << ": " << st.name
             << " (charge " << std::showpos << st.charge << std::noshowpos
             << ", multiplicity " << st.mult << ")\n"
             << "       Thermal: " << (st.thermo_recalculated ? "thermo module" :
                    st.thermo_fallback ? "step-1 log (thermo fallback)" : "step-1 log");
        if (st.thermo_recalculated || st.temperature_from_log)
            file << "; T=" << std::fixed << std::setprecision(2) << st.temperature_used << " K ("
                 << (st.thermo_recalculated ? "input" : "log") << ")\n";
        else if (fully_gas)
            file << "; log T unavailable (input T used as fallback)\n";
        else
            file << "; log T unavailable; phase T=" << std::fixed << std::setprecision(2)
                 << st.phase_temperature_used << " K (fallback if SCRF)\n";
    }

    section("EQUILIBRIUM ELECTRONIC AND GIBBS ENERGIES (Hartree)");
    file << "G = E(equilibrium) + thermal Gibbs correction + phase correction.\n"
         << std::left << std::setw(7) << "State" << std::right << std::setw(18) << "E(equilibrium)"
         << std::setw(16) << "Thermal G" << std::setw(16) << "Phase" << std::setw(18) << "G corrected" << "\n";
    for (const auto& row : states) {
        const auto& st = *row.state;
        file << std::left << std::setw(7) << row.id << std::right
             << std::setw(18) << number(st.e_high, st.has_energy)
             << std::setw(16) << number(st.tc_gibbs, st.has_tc_gibbs)
             << std::setw(16) << number(st.phase_corr, true)
             << std::setw(18) << number(st.g_corr, st.has_energy && st.has_tc_gibbs) << "\n";
    }

    section("CROSS ELECTRONIC ENERGIES (Hartree)");
    file << "Electronic state evaluated at the other state's geometry.\n"
         << std::left << std::setw(14) << "Molecule" << std::setw(19) << "State" << std::setw(19) << "Geometry"
         << std::right << std::setw(18) << "Energy" << "\n";
    auto cross = [&file, &number](const char* molecule, const char* state, const char* geometry, double value, bool valid) {
        file << std::left << std::setw(14) << molecule << std::setw(19) << state << std::setw(19) << geometry
             << std::right << std::setw(18) << number(value, valid) << "\n";
    };
    cross("Donor", "final (D-P)", "initial (D-R)", e.E_P_D_qR_D, e.success);
    cross("Donor", "initial (D-R)", "final (D-P)", e.E_R_D_qP_D, e.success);
    cross("Acceptor", "final (A-P)", "initial (A-R)", e.E_P_A_qR_A, e.success);
    cross("Acceptor", "initial (A-R)", "final (A-P)", e.E_R_A_qP_A, e.success);

    section("REORGANIZATION ENERGY COMPONENTS (kcal/mol)");
    file << "lambda R: initial state; lambda P: final state. Total = donor + acceptor.\n"
         << std::left << std::setw(18) << "" << std::right << std::setw(15) << "Donor"
         << std::setw(15) << "Acceptor" << std::setw(15) << "Total" << "\n";
    auto lambda_row = [&file](const char* name, double donor, double acceptor, double total) {
        file << std::left << std::setw(18) << name << std::right << std::fixed << std::setprecision(4)
             << std::setw(15) << donor << std::setw(15) << acceptor << std::setw(15) << total << "\n";
    };
    if (e.success) {
        lambda_row("lambda R", (e.E_R_D_qP_D - e.E_R_D_qR_D) * HARTREE_TO_KCAL_MOL,
                   (e.E_R_A_qP_A - e.E_R_A_qR_A) * HARTREE_TO_KCAL_MOL, result.lambda_R_kcal);
        lambda_row("lambda P", (e.E_P_D_qR_D - e.E_P_D_qP_D) * HARTREE_TO_KCAL_MOL,
                   (e.E_P_A_qR_A - e.E_P_A_qP_A) * HARTREE_TO_KCAL_MOL, result.lambda_P_kcal);
    } else file << "  N/A (electronic energies unavailable)\n";

    section("DEFINITIONS");
    file << "  Delta G° = G(D-P) + G(A-P) - G(D-R) - G(A-R)\n"
         << "  lambda R = [E(D-R at D-P) - E(D-R at D-R)]\n"
         << "           + [E(A-R at A-P) - E(A-R at A-R)]\n"
         << "  lambda P = [E(D-P at D-R) - E(D-P at D-P)]\n"
         << "           + [E(A-P at A-R) - E(A-P at A-P)]\n"
         << "  lambda = (lambda R + lambda P) / 2\n"
         << "  Delta G‡ (symmetric) = (lambda + Delta G°)^2 / (4 lambda)\n"
         << "  Delta G‡ (asymmetric) = lambda R * [(-lambda P +\n"
         << "    sqrt(lambda R * lambda P + (lambda R - lambda P) * Delta G°))\n"
         << "    / (lambda R - lambda P)]^2; equal lambdas use the symmetric limit.\n";

    bool any_scrf = false;
    for (const auto& row : states) any_scrf |= row.state->has_scrf;
    bool warning = !result.gibbs_valid || !result.barrier_sym_valid || !result.barrier_asym_valid ||
                   any_fallback || mixed_phase || !e.pcm_lines_in_gas_logs.empty() ||
                   !e.scrf_text_in_gas_logs.empty() || (fully_gas && any_scrf) ||
                   (!fully_gas && any_scrf && project.concentration <= 0.0);
    for (const auto& row : states)
        warning |= !row.state->has_full_thermal || (!row.state->thermo_recalculated && !row.state->temperature_from_log) ||
                   (!fully_gas && row.state->has_scrf && row.state->temperature_used <= 0.0);
    for (const auto& row : states)
        warning |= std::fabs(row.state->temperature_used - states[0].state->temperature_used) > 1e-6;
    if (warning) {
        section("WARNINGS");
        if (!result.gibbs_valid) file << "  Missing thermal Gibbs corrections: reaction energy and barriers are unavailable.\n";
        if (result.gibbs_valid && !result.barrier_sym_valid)
            file << "  Symmetric barrier unavailable: non-positive or invalid reorganization energies.\n";
        if (result.gibbs_valid && !result.barrier_asym_valid)
            file << "  Asymmetric barrier unavailable: invalid reorganization energies or negative discriminant.\n";
        if (mixed_phase)
            file << "  Step 1 and step 2 use mixed gas/solution settings; review thermochemistry and phase corrections.\n";
        if (fully_gas && any_scrf) {
            file << "  SCRF text detected in equilibrium logs despite configured gas phase; verify input logs:\n";
            for (const auto& row : states) {
                if (row.state->has_scrf)
                    file << "    " << row.id << ": " << step1_path(project.dir_step1_opt, row.state->log_file)
                         << " and " << step2_path(project.dir_step2_sp, row.state->log_file) << "\n";
            }
        }
        if (!e.pcm_lines_in_gas_logs.empty()) {
            file << "  PCM correction lines detected in configured gas-phase step-2 logs; SCF Done was used.\n"
                 << "  Verify these logs belong to the intended gas-phase calculations:\n";
            for (const auto& path : e.pcm_lines_in_gas_logs) file << "    " << path << "\n";
        }
        if (!e.scrf_text_in_gas_logs.empty()) {
            file << "  SCRF text detected in configured gas-phase step-2 logs; verify these logs:\n";
            for (const auto& path : e.scrf_text_in_gas_logs) file << "    " << path << "\n";
        }
        if (!fully_gas && any_scrf && project.concentration <= 0.0)
            file << "  Non-positive concentration: phase correction uses the 1 mol/L fallback.\n";
        for (const auto& row : states) {
            if (row.state->thermo_fallback) file << "  " << row.id << ": thermo recalculation failed; using log values if available.\n";
            if (!row.state->has_full_thermal) file << "  " << row.id << ": incomplete thermal data.\n";
            if (!row.state->thermo_recalculated && !row.state->temperature_from_log)
                file << "  " << row.id << ": log temperature unavailable; input temperature used as fallback.\n";
            if (!fully_gas && row.state->has_scrf && row.state->temperature_used <= 0.0)
                file << "  " << row.id << ": non-positive temperature; phase correction uses 298.15 K.\n";
        }
        bool different_t = false;
        for (const auto& row : states)
            different_t |= std::fabs(row.state->temperature_used - states[0].state->temperature_used) > 1e-6;
        if (different_t) file << "  States use different temperatures; review thermal and phase corrections.\n";
    }

    if (project.verbose_report) {
        section("VERBOSE: THERMAL BREAKDOWN BY STATE");
        file << "All energies in Hartree; lowest frequency in cm^-1.\n";
        for (const auto& row : states) {
            const auto& st = *row.state;
            file << "\n  " << row.id << " — " << row.role << ": " << st.name << "\n"
                 << "    E(low)          : " << number(st.e_low, st.has_e_low) << "\n"
                 << "    E(high)         : " << number(st.e_high, st.has_energy) << " (" << st.e_high_source << ")\n"
                 << "    ZPE             : " << number(st.zpe, st.has_zpe) << "\n"
                 << "    TC(E)           : " << number(st.tc_energy, st.has_tc_energy)
                 << (st.thermo_recalculated ? " (derived from TC(H) - RT)" : "") << "\n"
                 << "    TC(H)           : " << number(st.tc_enthalpy, st.has_tc_enthalpy) << "\n"
                 << "    TC(G)           : " << number(st.tc_gibbs, st.has_tc_gibbs) << "\n"
                 << "    TC(E) - ZPE     : " << number(st.tc_only, st.has_tc_energy && st.has_zpe) << "\n"
                 << "    TS = TC(H)-TC(G): " << number(st.ts_value, st.has_tc_enthalpy && st.has_tc_gibbs) << "\n"
                 << "    H               : " << number(st.h_state, st.has_energy && st.has_tc_enthalpy) << "\n"
                 << "    G(raw)          : " << number(st.g_raw, st.has_energy && st.has_tc_gibbs) << "\n"
                 << "    Phase correction: " << number(st.phase_corr, true) << "\n"
                 << "    G(corrected)    : " << number(st.g_corr, st.has_energy && st.has_tc_gibbs) << "\n"
                 << "    Lowest frequency: " << number(st.lowest_freq, st.has_lowest_freq, 2) << "\n"
                 << "    SCRF detected   : " << (st.has_scrf ? "yes" : "no") << "\n"
                 << "    Phase T / C     : ";
            if (fully_gas) file << "N/A (gas phase; no solution-phase correction)\n";
            else if (st.has_scrf)
                file << number(st.phase_temperature_used, true, 2) << " K / "
                     << number(st.phase_concentration_used, true, 3) << " mol/L\n";
            else file << "N/A (no SCRF detected)\n";
            file << "    Step 1 log      : " << step1_path(project.dir_step1_opt, st.log_file) << "\n"
                 << "    Step 2 log      : " << step2_path(project.dir_step2_sp, st.log_file) << "\n";
        }

        section("VERBOSE: ELECTRONIC ENERGY SOURCES");
        file << "Hartree; R and P identify the electronic state and geometry independently.\n";
        auto source = [&file, &number](const char* label, double value, const std::string& src, const std::string& path) {
            file << "  " << label << " = " << number(value, true) << " (" << src << ")\n"
                 << "    " << path << "\n";
        };
        if (e.success) {
            source("E(D-R at D-R)", e.E_R_D_qR_D, e.s_R_D_qR_D, e.f_R_D_qR_D);
            source("E(D-P at D-P)", e.E_P_D_qP_D, e.s_P_D_qP_D, e.f_P_D_qP_D);
            source("E(A-R at A-R)", e.E_R_A_qR_A, e.s_R_A_qR_A, e.f_R_A_qR_A);
            source("E(A-P at A-P)", e.E_P_A_qP_A, e.s_P_A_qP_A, e.f_P_A_qP_A);
            source("E(D-P at D-R)", e.E_P_D_qR_D, e.s_P_D_qR_D, e.f_P_D_qR_D);
            source("E(D-R at D-P)", e.E_R_D_qP_D, e.s_R_D_qP_D, e.f_R_D_qP_D);
            source("E(A-P at A-R)", e.E_P_A_qR_A, e.s_P_A_qR_A, e.f_P_A_qR_A);
            source("E(A-R at A-P)", e.E_R_A_qP_A, e.s_R_A_qP_A, e.f_R_A_qP_A);
        } else file << "  N/A (electronic energies unavailable)\n";

        section("VERBOSE: REACTION AND REORGANIZATION AUDIT");
        file << "  Delta G° raw       : " << number(result.dG_r_hartree_raw, result.gibbs_valid) << " Hartree\n"
             << "  Net phase on Delta G°: " << number(result.phase_correction, result.gibbs_valid) << " Hartree\n"
             << "  Delta G° corrected : " << number(result.dG_r_hartree, result.gibbs_valid) << " Hartree\n";
        if (e.success) {
            auto part = [&file, &number](const char* name, double cross_e, double eq_e) {
                double d = cross_e - eq_e;
                file << "  " << name << ": cross " << number(cross_e, true)
                     << "; equilibrium " << number(eq_e, true) << " Hartree\n"
                     << "    Difference: " << number(d, true) << " Hartree = "
                     << number(d * HARTREE_TO_KCAL_MOL, true, 4) << " kcal/mol\n";
            };
            part("lambda R donor   ", e.E_R_D_qP_D, e.E_R_D_qR_D);
            part("lambda R acceptor", e.E_R_A_qP_A, e.E_R_A_qR_A);
            part("lambda P donor   ", e.E_P_D_qR_D, e.E_P_D_qP_D);
            part("lambda P acceptor", e.E_P_A_qR_A, e.E_P_A_qP_A);
        }
        file << "  lambda R           : " << number(result.lambda_R_hartree, e.success) << " Hartree\n"
             << "  lambda P           : " << number(result.lambda_P_hartree, e.success) << " Hartree\n";
        if (result.gibbs_valid)
            file << "  Delta G° corrected : " << number(result.dG_r_hartree * HARTREE_TO_KJ_MOL, true, 4)
                 << " kJ/mol; " << number(result.dG_r_hartree * HARTREE_TO_EV, true, 4) << " eV\n";
        auto barrier_units = [&file, &number](const char* label, double kcal, bool valid) {
            if (!valid) { file << "  " << label << ": N/A\n"; return; }
            double h = kcal / HARTREE_TO_KCAL_MOL;
            file << "  " << label << ": " << number(h, true) << " Hartree; "
                 << number(h * HARTREE_TO_KJ_MOL, true, 4) << " kJ/mol; "
                 << number(h * HARTREE_TO_EV, true, 4) << " eV\n";
        };
        barrier_units("Symmetric barrier", result.barrier_sym_kcal, result.barrier_sym_valid);
        barrier_units("Asymmetric barrier", result.barrier_asym_kcal, result.barrier_asym_valid);
        file << (fully_gas ? "  Gas phase: no solution-phase standard-state correction.\n"
                           : "  Phase per SCRF state: RT ln(C RT/P°); P° = 1 atm, C in mol/m^3.\n")
             << "  Conversion factors: " << HARTREE_TO_KCAL_MOL << " kcal/mol per Hartree; "
             << HARTREE_TO_KJ_MOL << " kJ/mol per Hartree; " << HARTREE_TO_EV << " eV per Hartree.\n";

        section("VERBOSE: CONFIGURED THEORY AND THERMO OPTIONS");
        file << "  Step 1 basis path    : " << text_or_none(project.step1_basis_path) << "\n"
             << "  Step 1 basis tail    : " << text_or_none(project.step1_tail) << "\n"
             << "  Step 1 solvent extra : " << text_or_none(project.step1_solvent_extra)
             << (step1_gas ? " (ignored in gas phase)" : "") << "\n"
             << "  Step 1 route extra   : " << text_or_none(project.step1_route_extra_keywords) << "\n"
             << "  Step 1 opt options   : " << text_or_none(project.step1_opt_options) << "\n"
             << "  Step 1 SCF options   : " << text_or_none(project.step1_scf_options) << "\n"
             << "  Step 1 cycles        : SCF " << project.step1_scf_maxcycle << ", opt " << project.step1_opt_maxcycles << "\n"
             << "  Step 2 basis path    : " << text_or_none(project.step2_basis_path) << "\n"
             << "  Step 2 basis tail    : " << text_or_none(project.step2_tail) << "\n"
             << "  Step 2 solvent extra : " << text_or_none(project.step2_solvent_extra)
             << (step2_gas ? " (ignored in gas phase)" : "") << "\n"
             << "  Step 2 route extra   : " << text_or_none(project.step2_route_extra_keywords) << "\n"
             << "  Step 2 SCF cycles    : " << project.step2_scf_maxcycle << "\n"
             << "  Thermo overrides     : T=" << (project.use_input_temp ? "yes" : "no")
             << ", C=" << (project.use_input_concentration ? "yes" : "no")
             << ", P=" << (project.use_input_pressure ? "yes" : "no") << "\n"
             << "  Thermo print level   : " << project.prt_level << "; vibrations=" << project.print_vib
             << "; output OTM=" << (project.output_otm ? "yes" : "no") << "\n";
    }

    std::string footer;
    if (result.success && result.gibbs_valid && result.barrier_sym_valid) {
        footer = format_completion_footer();
        file << footer;
    }
    file.close();
    if (!file) {
        std::cerr << "Error: Cannot finish writing barrier report: " << report_path << std::endl;
        return false;
    }
    if (completion_footer) *completion_footer = footer;
    std::cout << "Barrier report written to: " << report_path << std::endl;
    return true;
}

std::string step1_path(const std::string& dir_step1, const std::string& filename) {
    return dir_step1 + "/" + filename;
}

std::string step2_path(const std::string& dir_step2, const std::string& filename) {
    return dir_step2 + "/" + filename;
}

}  // namespace Marcus