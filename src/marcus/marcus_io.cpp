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

    file << "Gibbs Free Energy per State (High-level electronic + Low-level thermal):\n";
    file << "------------------------------------------------------------------\n";
    file << "State                  G (Hartree)      Source\n";
    file << "------------------------------------------------------------------\n";

    auto fmt_state = [&file](const std::string& label,
                             const MarcusState& st,
                             const std::string& step1_log,
                             const std::string& step2_log) {
        file << std::left << std::setw(24) << label;
        if (st.has_energy && st.has_tc_gibbs) {
            file << std::fixed << std::setprecision(8) << std::setw(16) << st.g_state;
        } else {
            file << std::setw(16) << "N/A";
        }
        file << "  " << step1_log << " + " << step2_log << "\n";
    };

    fmt_state(project.donor.name + "-" + project.donor.init_state.state_label,
              project.donor.init_state,
              project.donor.init_state.log_file,
              project.donor.init_state.log_file);
    fmt_state(project.donor.name + "-" + project.donor.final_state.state_label,
              project.donor.final_state,
              project.donor.final_state.log_file,
              project.donor.final_state.log_file);
    fmt_state(project.acceptor.name + "-" + project.acceptor.init_state.state_label,
              project.acceptor.init_state,
              project.acceptor.init_state.log_file,
              project.acceptor.init_state.log_file);
    fmt_state(project.acceptor.name + "-" + project.acceptor.final_state.state_label,
              project.acceptor.final_state,
              project.acceptor.final_state.log_file,
              project.acceptor.final_state.log_file);

    file << "\n";

    file << "Reaction Free Energy:\n";
    file << "---------------------\n";
    if (result.success) {
        file << std::fixed << std::setprecision(6)
             << "Delta G°r = " << result.dG_r_hartree << " Hartree\n";
        file << std::fixed << std::setprecision(4)
             << "          = " << result.dG_r_kcal << " kcal/mol\n";
        if (result.phase_corr_applied) {
            file << std::fixed << std::setprecision(6)
                 << "Phase correction = " << result.phase_correction << " Hartree\n";
            file << std::fixed << std::setprecision(4)
                 << "Delta G°r (corrected) = " << result.gibbs_hartree_corrected * HARTREE_TO_KCAL_MOL
                 << " kcal/mol\n";
        }
    } else {
        file << "Not available: " << result.error_message << "\n";
    }
    file << "\n";

    file << "Reorganization Energies (electronic, four-point method):\n";
    file << "---------------------------------------------------------\n";
    if (result.success) {
        file << std::fixed << std::setprecision(4)
             << "lambda R (reactant side): " << result.lambda_R_kcal << " kcal/mol\n";
        file << std::fixed << std::setprecision(4)
             << "lambda P (product side):  " << result.lambda_P_kcal << " kcal/mol\n";
        file << std::fixed << std::setprecision(4)
             << "lambda avg:               " << result.lambda_avg_kcal << " kcal/mol\n";
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
    file << "  G_state = E_step2_electronic (scfEqui) + TC_G_step1 (thermal correction to Gibbs)\n";

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