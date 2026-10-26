/**
 * @file marcus_input_gen.cpp
 * @brief Implementation of Gaussian input generation for Marcus barrier calculations.
 */

#include "marcus/marcus_input_gen.h"
#include "marcus/marcus_io.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>

namespace Marcus {

// ---------------------------------------------------------------------------
// Helper: state suffix from charge/multiplicity
// ---------------------------------------------------------------------------
std::string state_suffix(const MarcusState& state) {
    // Multiplicity name (spectroscopic notation)
    std::string mult_name;
    switch (state.mult) {
        case 1: mult_name = "singlet"; break;
        case 2: mult_name = "doublet"; break;
        case 3: mult_name = "triplet"; break;
        case 4: mult_name = "quartet"; break;
        case 5: mult_name = "quintet"; break;
        default: mult_name = "mult" + std::to_string(state.mult); break;
    }

    // Charge descriptor: neutral/plus/minus for consistency
    std::string charge_desc;
    if (state.charge == 0) {
        charge_desc = "neutral";
    } else if (state.charge > 0) {
        charge_desc = (state.charge == 1) ? "plus"
                                          : (std::to_string(state.charge) + "plus");
    } else {
        charge_desc = (state.charge == -1) ? "minus"
                                           : (std::to_string(-state.charge) + "minus");
    }

    // Always <spin>-<charge> order (consistent for all charge/mult combos)
    return "-" + mult_name + "-" + charge_desc;
}

// ---------------------------------------------------------------------------
// Helper: Link0 section
// ---------------------------------------------------------------------------
std::string build_link0_section(const std::string& mem,
                                const std::string& nprocshared,
                                const std::string& cpu,
                                const std::string& gpucpu,
                                const std::string& print_level) {
    std::ostringstream oss;
    std::string pound = print_level.empty() ? "#"
                     : (print_level == "N" || print_level == "P" || print_level == "T")
                         ? "#" + print_level
                         : "#" + print_level + " ";
    if (!mem.empty())       oss << "%mem=" << mem << "\n";
    if (!cpu.empty())       oss << "%cpu=" << cpu << "\n";
    if (!nprocshared.empty()) oss << "%nprocshared=" << nprocshared << "\n";
    if (!gpucpu.empty())    oss << "%gpucpu=" << gpucpu << "\n";
    return oss.str();
}

// ---------------------------------------------------------------------------
// Helper: SCRF fragments
// ---------------------------------------------------------------------------
std::string build_step1_scrf(const MarcusProject& project) {
    std::ostringstream oss;
    oss << "scrf=(" << project.step1_solvent_model << ",solvent=" << project.step1_solvent;
    if (!project.step1_solvent_extra.empty()) {
        oss << "," << project.step1_solvent_extra;
    }
    oss << ")";
    return oss.str();
}

std::string build_step2_scrf_equilibrium(const MarcusProject& project) {
    // Non-cross (equilibrium) jobs always write the solvent reaction field.
    std::ostringstream oss;
    oss << "scrf=(" << project.step2_solvent_model << ",solvent=" << project.step2_solvent
        << ",NonEq=write)";
    return oss.str();
}

std::string build_step2_scrf(const MarcusProject& project) {
    // Cross (non-equilibrium) jobs read the field written by the non-cross jobs.
    std::ostringstream oss;
    oss << "scrf=(" << project.step2_solvent_model << ",solvent=" << project.step2_solvent;
    if (!project.step2_solvent_extra.empty()) {
        oss << "," << project.step2_solvent_extra;
    }
    oss << ")";
    return oss.str();
}

// ---------------------------------------------------------------------------
// Helper: basis-set include line for step-2 inputs.
// For GEN/GENECP bases the external file reference needs the "/N" suffix
// (e.g. "@/path/to/ma-def2-TZVP.gbs/N"); an explicit step2_tail wins as-is.
// ---------------------------------------------------------------------------
static std::string step2_basis_include(const MarcusProject& project) {
    if (!project.step2_tail.empty()) {
        return project.step2_tail;
    }
    if (project.step2_basis_path.empty()) {
        return "";
    }
    std::string ref = "@" + project.step2_basis_path;
    std::string upper = project.step2_basis;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    if ((upper == "GEN" || upper == "GENECP") &&
        (ref.size() < 2 || ref.compare(ref.size() - 2, 2, "/N") != 0)) {
        ref += "/N";
    }
    return ref;
}

// ---------------------------------------------------------------------------
// Helper: read XYZ coordinates
// ---------------------------------------------------------------------------
std::string read_xyz_coordinates(const std::string& xyz_path) {
    std::ifstream file(xyz_path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open XYZ file: " << xyz_path << std::endl;
        return "";
    }
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    if (lines.size() < 3) {
        std::cerr << "Error: XYZ file too short: " << xyz_path << std::endl;
        return "";
    }
    std::ostringstream coords;
    // Skip first two lines (atom count and comment)
    for (size_t i = 2; i < lines.size(); ++i) {
        coords << lines[i] << "\n";
    }
    return coords.str();
}

// ---------------------------------------------------------------------------
// Step 1 route builder
// ---------------------------------------------------------------------------
std::string build_step1_route(const MarcusProject& project, const MarcusState& /*state*/) {
    std::ostringstream oss;
    int scf_mc = (project.step1_scf_maxcycle != -1) ? project.step1_scf_maxcycle : 300;
    int opt_mc = (project.step1_opt_maxcycles != -1) ? project.step1_opt_maxcycles : 300;

    oss << "opt(maxcycles=" << opt_mc;
    if (!project.step1_opt_options.empty()) {
        oss << "," << project.step1_opt_options;
    }
    oss << ") freq scf(maxcycle=" << scf_mc << ",xqc";
    if (!project.step1_scf_options.empty()) {
        oss << "," << project.step1_scf_options;
    }
    oss << ") " << project.step1_functional << "/" << project.step1_basis;

    // Only add SCRF if a solvent is specified (matches ci module opt_freq behavior)
    if (!project.step1_solvent.empty()) {
        oss << " " << build_step1_scrf(project);
    }

    if (!project.step1_route_extra_keywords.empty()) {
        oss << " " << project.step1_route_extra_keywords;
    }
    return oss.str();
}

// ---------------------------------------------------------------------------
// Step 2 route builder
// ---------------------------------------------------------------------------
std::string build_step2_route(const MarcusProject& project, bool is_equilibrium) {
    std::ostringstream oss;
    int scf_mc = (project.step2_scf_maxcycle != -1) ? project.step2_scf_maxcycle : 500;

    // Match the reference inputs: equilibrium jobs use "#P ... Geom(Check)",
    // cross jobs use "# ... Geom=Check".
    oss << (is_equilibrium ? "#P " : "# ");
    oss << "scf(maxcycle=" << scf_mc << ",xqc) " << project.step2_functional << "/"
        << project.step2_basis << " ";
    if (is_equilibrium) {
        oss << build_step2_scrf_equilibrium(project) << " Geom(Check) Guess(read) nosymm";
    } else {
        oss << build_step2_scrf(project) << " Geom=Check Guess(read) nosymm";
    }
    if (!project.step2_route_extra_keywords.empty()) {
        oss << " " << project.step2_route_extra_keywords;
    }
    return oss.str();
}

// ---------------------------------------------------------------------------
// Step 1: generate geometry optimization inputs
// ---------------------------------------------------------------------------
bool generate_step1_inputs(const MarcusProject& project) {
    std::string dir_step1, dir_step2, dir_barrier;
    if (!create_project_directories(project.name, dir_step1, dir_step2, dir_barrier)) {
        return false;
    }

    // Build the 4 states to optimize
    std::vector<MarcusState> states = {
        project.donor.init_state,
        project.donor.final_state,
        project.acceptor.init_state,
        project.acceptor.final_state
    };

    std::map<std::string, std::string> manifest;

    for (const auto& state : states) {
        std::string suffix = state_suffix(state);
        std::string base_name = state.name + suffix;
        std::string state_tag = (state.state_label == "init") ? "_init" : "_final";
        std::string gau_file = base_name + state_tag + ".gau";
        std::string chk_file = base_name + state_tag + ".chk";
        std::string log_file = base_name + state_tag + ".log";

        // Store in manifest
        std::string manifest_key = state.name + "-" + state.state_label;
        manifest[manifest_key] = chk_file;

        // Build full content
        std::ostringstream content;

        // Link0 section
        content << build_link0_section(project.step1_mem, project.step1_nprocshared,
                                       project.step1_cpu, project.step1_gpucpu,
                                       project.step1_print_level);
        content << "%chk=" << chk_file << "\n";

        // Route
        content << build_step1_route(project, state) << "\n";
        content << "\n";

        // Title
        content << "Title: Geometrical optimization and frequency calculation\n";
        content << "\n";

        // Charge/multiplicity
        content << state.charge << " " << state.mult << "\n";

        // Coordinates from XYZ
        std::string coords = read_xyz_coordinates(state.xyz_path);
        if (coords.empty()) {
            std::cerr << "Error: Failed to read coordinates for " << state.name
                      << " from " << state.xyz_path << std::endl;
            return false;
        }
        content << coords;
        content << "\n";

        // Basis set tail (for GEN/GENECP basis)
        if (!project.step1_tail.empty()) {
            content << project.step1_tail << "\n";
        } else if (!project.step1_basis_path.empty()) {
            content << "@" << project.step1_basis_path << "\n";
        }
        content << "\n";

        // Write file
        std::string full_path = dir_step1 + "/" + gau_file;
        std::ofstream out(full_path);
        if (!out.is_open()) {
            std::cerr << "Error: Cannot write input file: " << full_path << std::endl;
            return false;
        }
        out << content.str();
        out.close();

        std::cout << "Created: " << full_path << std::endl;
    }

    // Write manifest
    if (!write_step1_manifest(dir_step1, manifest)) {
        return false;
    }

    std::cout << "Step 1: Generated " << states.size() << " optimization inputs in "
              << dir_step1 << std::endl;
    return true;
}

// ---------------------------------------------------------------------------
// Step 2: generate single point inputs reading step-1 checkpoints
// ---------------------------------------------------------------------------
bool generate_step2_inputs(const MarcusProject& project) {
    std::string dir_step1, dir_step2, dir_barrier;
    if (!create_project_directories(project.name, dir_step1, dir_step2, dir_barrier)) {
        return false;
    }

    // Read step-1 manifest to locate checkpoints
    auto manifest = read_step1_manifest(dir_step1);
    if (manifest.empty()) {
        std::cerr << "Error: No step-1 manifest found in " << dir_step1
                  << ". Run step 1 first." << std::endl;
        return false;
    }

    // Helper to get checkpoint filename for a state
    auto get_chk = [&](const MarcusMolecule& mol, bool is_final) -> std::string {
        std::string key = mol.name + "-" + (is_final ? mol.final_state.state_label : mol.init_state.state_label);
        auto it = manifest.find(key);
        if (it == manifest.end()) {
            std::cerr << "Error: Checkpoint not found in manifest for " << key << std::endl;
            return "";
        }
        return it->second;
    };

    std::string chk_D_init = get_chk(project.donor, false);
    std::string chk_D_final = get_chk(project.donor, true);
    std::string chk_A_init = get_chk(project.acceptor, false);
    std::string chk_A_final = get_chk(project.acceptor, true);

    if (chk_D_init.empty() || chk_D_final.empty() ||
        chk_A_init.empty() || chk_A_final.empty()) {
        return false;
    }

    std::map<std::string, std::string> manifest2;

    // Absolute path of the step-1 directory: non-cross jobs read the
    // step-1 checkpoints from there. Cross jobs instead read the local
    // equilibrium checkpoints produced by the non-cross jobs in this
    // (step-2) directory, so they use a plain filename.
    std::string abs_dir_step1;
    try {
        abs_dir_step1 = std::filesystem::absolute(dir_step1).string();
    } catch (const std::exception& e) {
        std::cerr << "Error: Cannot resolve absolute path of " << dir_step1
                  << ": " << e.what() << std::endl;
        return false;
    }

    // Lambda to write a single-point input file.
    // oldchk_ref is the full %OldChk reference (absolute step-1 path for
    // non-cross jobs, plain local filename for cross jobs).
    auto write_sp_input = [&](const std::string& filename,
                              const std::string& oldchk_ref,
                              const std::string& chk_name,
                              int charge, int mult,
                              bool is_equilibrium) -> bool {
        std::ostringstream content;

        // Link0 section
        content << build_link0_section(project.step2_mem, project.step2_nprocshared,
                                       project.step2_cpu, project.step2_gpucpu,
                                       project.step2_print_level);
        content << (is_equilibrium ? "%OldChk=" : "%Oldchk=") << oldchk_ref << "\n";
        content << "%chk=" << chk_name << "\n";

        // Route
        content << build_step2_route(project, is_equilibrium) << "\n";
        content << "\n";

        // Title
        content << "Second step\n";
        content << "\n";

        // Charge/multiplicity
        content << charge << " " << mult << "\n";

        // No coordinates - geometry is read from checkpoint
        content << "\n";

        // Basis set tail (for GEN/GENECP basis)
        std::string basis_include = step2_basis_include(project);
        if (!basis_include.empty()) {
            content << basis_include << "\n";
        }
        content << "\n";

        std::string full_path = dir_step2 + "/" + filename;
        std::ofstream out(full_path);
        if (!out.is_open()) {
            std::cerr << "Error: Cannot write input file: " << full_path << std::endl;
            return false;
        }
        out << content.str();
        out.close();

        std::cout << "Created: " << full_path << std::endl;

        return true;
    };

    // === Donor: 4 single points ===
    // Equilibrium: D-init reads D-init.chk, D-final reads D-final.chk
    // Cross: D-final-from-D-init reads D-init.chk (charge/mult of final)
    // Cross: D-init-from-D-final reads D-final.chk (charge/mult of init)

    // D-init equilibrium (non-cross: reads step-1 chk via absolute path)
    if (!write_sp_input(project.donor.name + state_suffix(project.donor.init_state) + "_init.gau",
                        abs_dir_step1 + "/" + chk_D_init, chk_D_init,
                        project.donor.init_state.charge, project.donor.init_state.mult,
                        true)) return false;
    manifest2[project.donor.name + "-" + project.donor.init_state.state_label + "-eq"] =
        project.donor.name + state_suffix(project.donor.init_state) + "_init.gau";

    // D-final equilibrium (non-cross: reads step-1 chk via absolute path)
    if (!write_sp_input(project.donor.name + state_suffix(project.donor.final_state) + "_final.gau",
                        abs_dir_step1 + "/" + chk_D_final, chk_D_final,
                        project.donor.final_state.charge, project.donor.final_state.mult,
                        true)) return false;
    manifest2[project.donor.name + "-" + project.donor.final_state.state_label + "-eq"] =
        project.donor.name + state_suffix(project.donor.final_state) + "_final.gau";

    // Helper: cross .chk name follows the cross .gau basename.
    auto cross_chk = [](const std::string& cross_gau) {
        std::string c = cross_gau;
        size_t dot = c.rfind(".gau");
        if (dot != std::string::npos) c.replace(dot, 4, ".chk");
        return c;
    };

    // D-final-from-D-init (cross, product state at reactant geometry)
    std::string cross_D_fi = project.donor.name + state_suffix(project.donor.final_state) + "_final-from-"
                           + project.donor.init_state.name + state_suffix(project.donor.init_state) + "_init.gau";
    if (!write_sp_input(cross_D_fi, chk_D_init, cross_chk(cross_D_fi),
                        project.donor.final_state.charge, project.donor.final_state.mult,
                        false)) return false;
    manifest2[project.donor.name + "-" + project.donor.final_state.state_label
              + "-from-" + project.donor.init_state.state_label] = cross_D_fi;

    // D-init-from-D-final (cross, reactant state at product geometry)
    std::string cross_D_if = project.donor.name + state_suffix(project.donor.init_state) + "_init-from-"
                           + project.donor.final_state.name + state_suffix(project.donor.final_state) + "_final.gau";
    if (!write_sp_input(cross_D_if, chk_D_final, cross_chk(cross_D_if),
                        project.donor.init_state.charge, project.donor.init_state.mult,
                        false)) return false;
    manifest2[project.donor.name + "-" + project.donor.init_state.state_label
              + "-from-" + project.donor.final_state.state_label] = cross_D_if;

    // === Acceptor: 4 single points ===
    // A-init equilibrium (non-cross: reads step-1 chk via absolute path)
    if (!write_sp_input(project.acceptor.name + state_suffix(project.acceptor.init_state) + "_init.gau",
                        abs_dir_step1 + "/" + chk_A_init, chk_A_init,
                        project.acceptor.init_state.charge, project.acceptor.init_state.mult,
                        true)) return false;
    manifest2[project.acceptor.name + "-" + project.acceptor.init_state.state_label + "-eq"] =
        project.acceptor.name + state_suffix(project.acceptor.init_state) + "_init.gau";

    // A-final equilibrium (non-cross: reads step-1 chk via absolute path)
    if (!write_sp_input(project.acceptor.name + state_suffix(project.acceptor.final_state) + "_final.gau",
                        abs_dir_step1 + "/" + chk_A_final, chk_A_final,
                        project.acceptor.final_state.charge, project.acceptor.final_state.mult,
                        true)) return false;
    manifest2[project.acceptor.name + "-" + project.acceptor.final_state.state_label + "-eq"] =
        project.acceptor.name + state_suffix(project.acceptor.final_state) + "_final.gau";

    // A-final-from-A-init (cross)
    std::string cross_A_fi = project.acceptor.final_state.name + state_suffix(project.acceptor.final_state) + "_final-from-"
                           + project.acceptor.init_state.name + state_suffix(project.acceptor.init_state) + "_init.gau";
    if (!write_sp_input(cross_A_fi, chk_A_init, cross_chk(cross_A_fi),
                        project.acceptor.final_state.charge, project.acceptor.final_state.mult,
                        false)) return false;
    manifest2[project.acceptor.name + "-" + project.acceptor.final_state.state_label
              + "-from-" + project.acceptor.init_state.state_label] = cross_A_fi;

    // A-init-from-A-final (cross)
    std::string cross_A_if = project.acceptor.init_state.name + state_suffix(project.acceptor.init_state) + "_init-from-"
                           + project.acceptor.final_state.name + state_suffix(project.acceptor.final_state) + "_final.gau";
    if (!write_sp_input(cross_A_if, chk_A_final, cross_chk(cross_A_if),
                        project.acceptor.init_state.charge, project.acceptor.init_state.mult,
                        false)) return false;
    manifest2[project.acceptor.name + "-" + project.acceptor.init_state.state_label
              + "-from-" + project.acceptor.final_state.state_label] = cross_A_if;

    // Write manifest
    if (!write_step2_manifest(dir_step2, manifest2)) {
        return false;
    }

    std::cout << "Step 2: Generated 8 single point inputs in " << dir_step2 << std::endl;
    return true;
}

}  // namespace Marcus