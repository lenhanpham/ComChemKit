/**
 * @file marcus_io.cpp
 * @brief Implementation of I/O utilities for Marcus barrier calculations.
 */

#include "marcus/marcus_io.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace Marcus {

bool create_project_directories(const std::string& project_name,
                                std::string& dir_step1,
                                std::string& dir_step2,
                                std::string& dir_barrier) {
    try {
        dir_step1  = project_name + "_step_1_opt";
        dir_step2  = project_name + "_step_2_sp";
        dir_barrier = project_name + "_barrier";

        std::filesystem::create_directories(dir_step1);
        std::filesystem::create_directories(dir_step2);
        std::filesystem::create_directories(dir_barrier);

        std::cout << "Created project directories:" << std::endl;
        std::cout << "  Step 1 (opt):  " << dir_step1 << std::endl;
        std::cout << "  Step 2 (sp):   " << dir_step2 << std::endl;
        std::cout << "  Step 3 (barrier): " << dir_barrier << std::endl;

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

bool write_barrier_report(const std::string& dir_barrier,
                          const MarcusProject& project,
                          const MarcusResult& result,
                          const std::string& out_file) {
    std::string report_path;
    if (!out_file.empty()) {
        report_path = out_file;
    } else {
        report_path = dir_barrier + "/" + project.name + "_barrier_report.txt";
    }

    std::ofstream file(report_path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot write barrier report: " << report_path << std::endl;
        return false;
    }

    const std::string calc_type_str =
        (project.calc_type == CalcType::OSET) ? "OSET" : "EnT";
    const std::string mode_str =
        (project.barrier_mode == BarrierMode::SYMMETRIC) ? "symmetric" : "asymmetric";

    file << "Marcus Theory Barrier Calculation (Four-Point Method)\n";
    file << "======================================================\n";
    file << "Project: " << project.name << "\n";
    file << "Calculation type: " << calc_type_str << "\n";
    file << "Barrier mode: " << mode_str << "\n";
    file << "\n";

    file << "Step 1 directory: " << project.dir_step1_opt << "\n";
    file << "Step 2 directory: " << project.dir_step2_sp << "\n";
    file << "\n";

    bool thermo_path = project.use_input_temp || project.use_input_concentration ||
                         project.use_input_pressure;
    file << "Thermo pathway: "
         << (thermo_path ? "thermo module (recalculated at user T/C/P)"
                         : "string-matching from step-1 logs (log T)") << "\n";
    file << "Temperature: " << project.temperature << " K (params/CLI)\n";
    file << "Concentration: " << project.concentration << " mol/L\n";
    file << "Pressure: " << project.pressure << " atm\n";
    file << "Low-vib method: " << project.low_vib_method
         << "  ravib: " << project.ravib << " cm^-1\n";
    file << "\n";

    auto basename = [](const std::string& p) {
        size_t s = p.find_last_of("/\\");
        return (s == std::string::npos) ? p : p.substr(s + 1);
    };

    // ---- Section A: per-state Gibbs breakdown (high-au style) ----
    file << "Section A: Gibbs Free Energy per State (E_high step-2 + thermal step-1, high-au style)\n";
    file << "--------------------------------------------------------------------------------------\n";
    file << "All energies in Hartree unless noted. H = E_high + TC_H, G_raw = E_high + TC_G,\n";
    file << "G_corr = G_raw + phase (compare G_corr to high-kJ G a.u.; H to high-au H a.u.).\n";
    file << std::left << std::setw(22) << "State"
         << std::right << std::setw(13) << "E_low"
         << std::setw(13) << "E_high"
         << std::setw(11) << "ZPE"
         << std::setw(11) << "TC_E"
         << std::setw(11) << "TC_H"
         << std::setw(11) << "TC_G"
         << std::setw(13) << "H"
         << std::setw(13) << "G_raw"
         << std::setw(10) << "phase"
         << std::setw(13) << "G_corr"
         << std::setw(9) << "LowFQ" << "\n";
    file << "Key: E_low = SCF Done step-1; E_high = "
            "After PCM (scfEqui) else SCF Done step-2; "
            "TC_* from step-1 freq block.\n";

    auto fmtA = [&file](const std::string& label, const MarcusState& st) {
        file << std::left << std::setw(22) << label << std::right;
        if (st.has_energy && st.has_tc_gibbs) {
            file << std::fixed << std::setprecision(6)
                 << std::setw(13) << st.e_low
                 << std::setw(13) << st.e_high
                 << std::setw(11) << st.zpe
                 << std::setw(11) << st.tc_energy
                 << std::setw(11) << st.tc_enthalpy
                 << std::setw(11) << st.tc_gibbs
                 << std::setw(13) << st.h_state
                 << std::setw(13) << st.g_raw
                 << std::setw(10) << st.phase_corr
                 << std::setw(13) << st.g_corr
                 << std::setw(9) << std::setprecision(2) << st.lowest_freq;
            file << "  [" << st.e_high_source
                 << (st.has_scrf ? ",scrf" : ",no-scrf")
                 << ",T=" << std::fixed << std::setprecision(2) << st.temperature_used << "K]";
            if (!st.has_full_thermal) file << "  (partial thermal)";
            file << "\n";
        } else {
            file << "  N/A (missing energy or TC_G)\n";
        }
        file << std::setprecision(6);
    };
    fmtA(project.donor.name + "-" + project.donor.init_state.state_label,
         project.donor.init_state);
    fmtA(project.donor.name + "-" + project.donor.final_state.state_label,
         project.donor.final_state);
    fmtA(project.acceptor.name + "-" + project.acceptor.init_state.state_label,
         project.acceptor.init_state);
    fmtA(project.acceptor.name + "-" + project.acceptor.final_state.state_label,
         project.acceptor.final_state);
    file << "Derived: TC_only = TC_E - ZPE (= high-au TC); TS = TC_H - TC_G (= high-au TS).\n";
    auto fmtDerived = [&file](const std::string& label, const MarcusState& st) {
        file << "  " << std::left << std::setw(20) << label << std::right
             << " TC_only=" << std::fixed << std::setprecision(6) << st.tc_only
             << "  TS=" << st.ts_value << "\n";
    };
    fmtDerived(project.donor.name + "-" + project.donor.init_state.state_label,
               project.donor.init_state);
    fmtDerived(project.donor.name + "-" + project.donor.final_state.state_label,
               project.donor.final_state);
    fmtDerived(project.acceptor.name + "-" + project.acceptor.init_state.state_label,
               project.acceptor.init_state);
    fmtDerived(project.acceptor.name + "-" + project.acceptor.final_state.state_label,
               project.acceptor.final_state);
    file << "Sources (step-1 log + step-2 log per state):\n";
    file << "  D-init: " << project.donor.init_state.log_file << " + "
         << project.donor.init_state.log_file << "\n";
    file << "  D-final: " << project.donor.final_state.log_file << " + "
         << project.donor.final_state.log_file << "\n";
    file << "  A-init: " << project.acceptor.init_state.log_file << " + "
         << project.acceptor.init_state.log_file << "\n";
    file << "  A-final: " << project.acceptor.final_state.log_file << " + "
         << project.acceptor.final_state.log_file << "\n";
    file << "\n";

    // ---- Section B: electronic energies ----
    file << "Section B: Electronic Energies for Reorganization (step-2 only, no thermal)\n";
    file << "--------------------------------------------------------------------------\n";
    if (result.success || result.elec.success) {
        const auto& e = result.elec;
        file << std::left << std::setw(16) << "Term"
             << std::setw(52) << "Step-2 log file" << std::right
             << std::setw(15) << "E (Hartree)" << "\n";
        auto row = [&file, &basename](const char* lab, const std::string& f, double v) {
            file << std::left << std::setw(16) << lab << std::setw(52) << basename(f)
                 << std::right << std::fixed << std::setprecision(6) << std::setw(15) << v << "\n";
        };
        row("E(R_D@qR_D)", e.f_R_D_qR_D, e.E_R_D_qR_D);
        row("E(P_D@qP_D)", e.f_P_D_qP_D, e.E_P_D_qP_D);
        row("E(R_A@qR_A)", e.f_R_A_qR_A, e.E_R_A_qR_A);
        row("E(P_A@qP_A)", e.f_P_A_qP_A, e.E_P_A_qP_A);
        row("E(P_D@qR_D)", e.f_P_D_qR_D, e.E_P_D_qR_D);
        row("E(R_D@qP_D)", e.f_R_D_qP_D, e.E_R_D_qP_D);
        row("E(P_A@qR_A)", e.f_P_A_qR_A, e.E_P_A_qR_A);
        row("E(R_A@qP_A)", e.f_R_A_qP_A, e.E_R_A_qP_A);
        file << "Cross terms use 'After PCM corrections, the energy is' (NonEq read); "
                "equilibrium terms use SCF Done unless PCM line present.\n";
        double dD_R = e.E_R_D_qP_D - e.E_R_D_qR_D;
        double dA_R = e.E_R_A_qP_A - e.E_R_A_qR_A;
        double dD_P = e.E_P_D_qR_D - e.E_P_D_qP_D;
        double dA_P = e.E_P_A_qR_A - e.E_P_A_qP_A;
        file << std::fixed << std::setprecision(6)
             << "lambda_R donor part  = E(R_D@qP_D)-E(R_D@qR_D) = " << dD_R << " H = "
             << dD_R * HARTREE_TO_KCAL_MOL << " kcal/mol\n"
             << "lambda_R acceptor part= E(R_A@qP_A)-E(R_A@qR_A) = " << dA_R << " H = "
             << dA_R * HARTREE_TO_KCAL_MOL << " kcal/mol\n"
             << "lambda_P donor part  = E(P_D@qR_D)-E(P_D@qP_D) = " << dD_P << " H = "
             << dD_P * HARTREE_TO_KCAL_MOL << " kcal/mol\n"
             << "lambda_P acceptor part= E(P_A@qR_A)-E(P_A@qP_A) = " << dA_P << " H = "
             << dA_P * HARTREE_TO_KCAL_MOL << " kcal/mol\n";
    } else {
        file << "Not available: " << result.error_message << "\n";
    }
    file << "\n";

    file << "Section C: Reaction Free Energy\n";
    file << "-------------------------------\n";
    file << "dG_r = [G(P_D)+G(P_A)] - [G(R_D)+G(R_A)], using Section A G values.\n";
    if (result.success) {
        auto trio = [&file](const char* lab, double h) {
            file << std::fixed << std::setprecision(6)
                 << lab << " = " << h << " Hartree  = "
                 << std::setprecision(4) << h * HARTREE_TO_KCAL_MOL << " kcal/mol  = "
                 << h * HARTREE_TO_KJ_MOL << " kJ/mol  = "
                 << h * HARTREE_TO_EV << " eV\n";
        };
        trio("dG_r (raw, no phase) ", result.dG_r_hartree_raw);
        trio("dG_r (corr, solution)", result.dG_r_hartree);
        file << std::fixed << std::setprecision(6)
             << "Net phase on dG_r = " << result.phase_correction
             << " Hartree (sums cancel when all states share T/C/SCRF).\n";
        file << "G(P_D)=" << project.donor.final_state.g_corr
             << " G(P_A)=" << project.acceptor.final_state.g_corr
             << " G(R_D)=" << project.donor.init_state.g_corr
             << " G(R_A)=" << project.acceptor.init_state.g_corr << " (Hartree, corr)\n";
    } else {
        file << "Not available: " << result.error_message << "\n";
    }
    file << "\n";

    file << "Section D: Reorganization Energies and Barriers\n";
    file << "-----------------------------------------------\n";
    if (result.success) {
        file << std::fixed << std::setprecision(6)
             << "lambda R = " << result.lambda_R_hartree << " Hartree = "
             << std::setprecision(4) << result.lambda_R_kcal << " kcal/mol\n";
        file << std::fixed << std::setprecision(6)
             << "lambda P = " << result.lambda_P_hartree << " Hartree = "
             << std::setprecision(4) << result.lambda_P_kcal << " kcal/mol\n";
        file << std::fixed << std::setprecision(4)
             << "lambda avg = " << result.lambda_avg_kcal << " kcal/mol\n";
    } else {
        file << "Not available: " << result.error_message << "\n";
    }
    file << "\n";

    file << "Free Energy Barrier:\n";
    file << "--------------------\n";
    if (result.success) {
        if (project.barrier_mode == BarrierMode::SYMMETRIC ||
            project.barrier_mode == BarrierMode::ASYMMETRIC) {
            file << std::fixed << std::setprecision(4)
                 << "Delta G‡ (symmetric):  " << result.barrier_sym_kcal << " kcal/mol\n";
        }
        if (project.barrier_mode == BarrierMode::ASYMMETRIC) {
            file << std::fixed << std::setprecision(4)
                 << "Delta G‡ (asymmetric): " << result.barrier_asym_kcal << " kcal/mol\n";
        }
    } else {
        file << "Not available: " << result.error_message << "\n";
    }
    file << "\n";

    file << "Notes:\n";
    file << "  lambda R: reorganization energy measured on the reactants parabola\n";
    file << "  lambda P: reorganization energy measured on the products parabola\n";
    file << "  lambda avg: (lambda R + lambda P) / 2 (symmetric approximation)\n";
    file << "  Per-state: H = E_high(step-2) + TC_H(step-1); "
            "G_raw = E_high + TC_G; G_corr = G_raw + phase.\n";
    file << "  Phase per state when SCRF present: RT*ln(C*RT/Po)*FACTOR; "
            "default 1M/298.15K = +0.003019 au.\n";
    file << "  Compare Section A E_high/H/G_corr with high-au/high-kJ tables; "
            "Section B with step_2_sp SCFE / After-PCM energies.\n";

    file.close();

    std::cout << "Barrier report written to: " << report_path << std::endl;
    return true;
}

std::string step1_path(const std::string& dir_step1, const std::string& filename) {
    return dir_step1 + "/" + filename;
}

std::string step2_path(const std::string& dir_step2, const std::string& filename) {
    return dir_step2 + "/" + filename;
}

std::string barrier_path(const std::string& dir_barrier, const std::string& filename) {
    return dir_barrier + "/" + filename;
}

}  // namespace Marcus