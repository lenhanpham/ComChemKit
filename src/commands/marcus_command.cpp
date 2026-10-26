/**
 * @file marcus_command.cpp
 * @brief Implementation of MarcusCommand for Marcus theory barrier calculations.
 */

#include "commands/marcus_command.h"
#include "marcus/marcus_input_gen.h"
#include "marcus/marcus_calculator.h"
#include "marcus/marcus_io.h"
#include "input_gen/parameter_parser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

MarcusCommand::MarcusCommand() {}

std::string MarcusCommand::get_name() const {
    return "marcus";
}

std::string MarcusCommand::get_description() const {
    return "Marcus theory barrier calculations for OSET and EnT (four-point method)";
}

Marcus::CalcType MarcusCommand::parse_calc_type(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "oset") return Marcus::CalcType::OSET;
    if (lower == "ent")  return Marcus::CalcType::ENT;
    return Marcus::CalcType::OSET;  // default
}

Marcus::BarrierMode MarcusCommand::parse_barrier_mode(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "asymmetric" || lower == "asym") return Marcus::BarrierMode::ASYMMETRIC;
    return Marcus::BarrierMode::SYMMETRIC;  // default
}

void MarcusCommand::print_help() const {
    std::cout << "Usage: cck marcus -p <parameter_file> [-step <1|2|3|1,2>] [-mode <symmetric|asymmetric>] [-O <outfile>]\n";
    std::cout << "\n";
    std::cout << "Marcus Theory Barrier Calculations (OSET / EnT, Four-Point Method)\n";
    std::cout << "================================================================\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  -p <file>      Parameter file (required)\n";
    std::cout << "  -step <n>      Workflow step(s): 1=gen opt inputs, 2=gen sp inputs, 3=calc barriers\n";
    std::cout << "                 (default: params file `step` key, default `1,2` = generate both\n";
    std::cout << "                 input sets up front; submit step-1 jobs first, then step-2 jobs)\n";
    std::cout << "  -mode <m>      Barrier mode: symmetric (default) or asymmetric\n";
    std::cout << "  -O <file>      Output file for barrier report (step 3)\n";
    std::cout << "  -v             Verbose mode\n";
    std::cout << "\n";
    std::cout << "Workflow:\n";
    std::cout << "  Step 1: Generate geometry optimization inputs in <project>_step_1_opt/\n";
    std::cout << "          User submits to Gaussian, waits for completion.\n";
    std::cout << "  Step 2: Generate single point inputs in <project>_step_2_sp/\n";
    std::cout << "          (reads step-1 checkpoints via %OldChk + Geom=Check)\n";
    std::cout << "          User submits to Gaussian, waits for completion.\n";
    std::cout << "  Step 3: Calculate barriers in <project>_barrier/\n";
    std::cout << "\n";
    std::cout << "Parameter file uses ComChemKit key=value format.\n";
    std::cout << "See documentation for full parameter reference.\n";
}

void MarcusCommand::parse_args(int argc, char* argv[], int& i, CommandContext& /*context*/) {
    std::string arg = argv[i];

    if (arg == "-p" || arg == "--param" || arg == "--parameter" || arg == "--param-file") {
        if (i + 1 < argc) {
            param_file = argv[++i];
        } else {
            std::cerr << "Error: -p requires a parameter file argument" << std::endl;
        }
    }
    else if (arg == "-step") {
        if (i + 1 < argc) {
            step_arg = argv[++i];
            step_cli_given = true;
        } else {
            std::cerr << "Error: -step requires an argument (1, 2, 3, or 1,2)" << std::endl;
        }
    }
    else if (arg == "-mode") {
        if (i + 1 < argc) {
            barrier_mode = parse_barrier_mode(argv[++i]);
        } else {
            std::cerr << "Error: -mode requires symmetric or asymmetric" << std::endl;
        }
    }
    else if (arg == "-O" || arg == "--out" || arg == "--output") {
        if (i + 1 < argc) {
            out_file = argv[++i];
        } else {
            std::cerr << "Error: -O requires an output file argument" << std::endl;
        }
    }
    else if (arg == "-v" || arg == "--verbose") {
        verbose = true;
    }
    else if (arg == "-t" || arg == "--temp") {
        if (i + 1 < argc) {
            project_temperature = std::stod(argv[++i]);
            use_input_temp = true;
        } else {
            std::cerr << "Error: -t requires a temperature argument" << std::endl;
        }
    }
    else if (arg == "-c" || arg == "-C" || arg == "--cm" || arg == "--conc") {
        if (i + 1 < argc) {
            project_concentration = std::stod(argv[++i]);
            use_input_concentration = true;
        } else {
            std::cerr << "Error: -c requires a concentration argument" << std::endl;
        }
    }
    else if (arg == "-p" || arg == "--pressure") {
        if (i + 1 < argc) {
            project_pressure = std::stod(argv[++i]);
            use_input_pressure = true;
        } else {
            std::cerr << "Error: -p requires a pressure argument" << std::endl;
        }
    }
    else if (arg == "--lowvibmeth") {
        if (i + 1 < argc) {
            project_low_vib_method = argv[++i];
        } else {
            std::cerr << "Error: --lowvibmeth requires a method argument" << std::endl;
        }
    }
    else if (arg == "--ravib") {
        if (i + 1 < argc) {
            project_ravib = std::stod(argv[++i]);
        } else {
            std::cerr << "Error: --ravib requires a frequency argument" << std::endl;
        }
    }
    else if (arg == "-h" || arg == "--help") {
        print_help();
        std::exit(0);
    }
    else if (arg == "--genoset-params") {
        generate_template_flag = true;
        template_calc_type = "oset";
    }
    else if (arg == "--genent-params") {
        generate_template_flag = true;
        template_calc_type = "ent";
    }
    else if (arg == "-o" || arg == "--output-template" || arg == "--template-out") {
        if (i + 1 < argc) {
            param_file = argv[++i];  // reuse param_file as output path
        } else {
            std::cerr << "Error: -o requires an output file argument" << std::endl;
        }
    }
    else {
        // Unrecognized option for marcus command — skip it
        // (the outer parse loop will advance past it)
    }
}

bool MarcusCommand::load_parameters(const std::string& filename,
                                     Marcus::MarcusProject& project) {
    ParameterParser parser;
    if (!parser.loadFromFile(filename)) {
        std::cerr << "Error: Cannot load parameter file: " << filename << std::endl;
        return false;
    }

    // Project identification
    project.name = parser.getString("project", "marcus_project");
    project.calc_type = parse_calc_type(parser.getString("calc_type", "oset"));
    // Step selection: CLI -step overrides the params-file `step` key.
    // Default (key commented out / missing) generates both input sets: "1,2".
    std::string step_spec = step_cli_given ? step_arg : parser.getString("step", "1,2");
    std::vector<int> steps;
    {
        std::string tok;
        std::istringstream ss(step_spec);
        while (std::getline(ss, tok, ',')) {
            // trim whitespace
            size_t b = tok.find_first_not_of(" \t\r\n");
            if (b == std::string::npos) continue;
            size_t e = tok.find_last_not_of(" \t\r\n");
            std::string t = tok.substr(b, e - b + 1);
            try {
                int n = std::stoi(t);
                if (n < 1 || n > 3) {
                    std::cerr << "Error: Invalid step number '" << t
                              << "' (must be 1, 2, or 3)" << std::endl;
                    return false;
                }
                if (std::find(steps.begin(), steps.end(), n) == steps.end())
                    steps.push_back(n);
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid step spec '" << step_spec
                          << "' (expected e.g. 1, 2, 1,2, or 3)" << std::endl;
                return false;
            }
        }
    }
    if (steps.empty()) {
        std::cerr << "Error: No workflow step selected (step = '" << step_spec << "')" << std::endl;
        return false;
    }
    std::sort(steps.begin(), steps.end());
    project.requested_steps = steps;
    project.current_step = static_cast<Marcus::Step>(steps.front() - 1);
    project.barrier_mode = barrier_mode;
    project.skip_existing = parser.getBool("skip_existing", true);

    // Donor
    project.donor.name = parser.getString("donor_name", "");
    project.donor.init_state.name = project.donor.name;
    project.donor.init_state.state_label = "init";
    project.donor.init_state.charge = parser.getInt("donor_init_charge", 0);
    project.donor.init_state.mult = parser.getInt("donor_init_mult", 1);
    project.donor.init_state.xyz_path = parser.getString("donor_xyz", "");
    project.donor.init_state.log_file = project.donor.name + state_suffix(project.donor.init_state) + "_init.log";

    project.donor.final_state.name = project.donor.name;
    project.donor.final_state.state_label = "final";
    project.donor.final_state.charge = parser.getInt("donor_final_charge", 0);
    project.donor.final_state.mult = parser.getInt("donor_final_mult", 1);
    project.donor.final_state.xyz_path = parser.getString("donor_xyz", "");
    project.donor.final_state.log_file = project.donor.name + state_suffix(project.donor.final_state) + "_final.log";

    // Acceptor
    project.acceptor.name = parser.getString("acceptor_name", "");
    project.acceptor.init_state.name = project.acceptor.name;
    project.acceptor.init_state.state_label = "init";
    project.acceptor.init_state.charge = parser.getInt("acceptor_init_charge", 0);
    project.acceptor.init_state.mult = parser.getInt("acceptor_init_mult", 1);
    project.acceptor.init_state.xyz_path = parser.getString("acceptor_xyz", "");
    project.acceptor.init_state.log_file = project.acceptor.name + state_suffix(project.acceptor.init_state) + "_init.log";

    project.acceptor.final_state.name = project.acceptor.name;
    project.acceptor.final_state.state_label = "final";
    project.acceptor.final_state.charge = parser.getInt("acceptor_final_charge", 0);
    project.acceptor.final_state.mult = parser.getInt("acceptor_final_mult", 1);
    project.acceptor.final_state.xyz_path = parser.getString("acceptor_xyz", "");
    project.acceptor.final_state.log_file = project.acceptor.name + state_suffix(project.acceptor.final_state) + "_final.log";

    // Validate required fields
    std::vector<std::string> missing;
    if (project.donor.name.empty()) missing.push_back("donor_name");
    if (project.donor.init_state.xyz_path.empty()) missing.push_back("donor_xyz");
    if (project.acceptor.name.empty()) missing.push_back("acceptor_name");
    if (project.acceptor.init_state.xyz_path.empty()) missing.push_back("acceptor_xyz");

    if (!missing.empty()) {
        std::cerr << "Error: Missing required parameter(s):";
        for (const auto& m : missing) std::cerr << " " << m;
        std::cerr << std::endl;
        return false;
    }

    // Step 1 theory parameters
    project.step1_functional      = parser.getString("step1_functional", "uwb97xd");
    project.step1_basis           = parser.getString("step1_basis", "def2svpp");
    project.step1_basis_path      = parser.getString("step1_basis_path", "");
    project.step1_solvent_model   = parser.getString("step1_solvent_model", "smd");
    project.step1_solvent         = parser.getString("step1_solvent", "");
    project.step1_solvent_extra   = parser.getString("step1_solvent_extra", "");
    project.step1_scf_maxcycle    = parser.getInt("step1_scf_maxcycle", 500);
    project.step1_opt_maxcycles   = parser.getInt("step1_opt_maxcycles", 100);
    project.step1_opt_options     = parser.getString("step1_opt_options", "");
    project.step1_scf_options     = parser.getString("step1_scf_options", "");
    project.step1_opt_restart     = parser.getBool("step1_opt_restart", false);
    project.step1_scf_restart     = parser.getBool("step1_scf_restart", false);
    project.step1_mem             = parser.getString("step1_mem", "");
    project.step1_nprocshared     = parser.getString("step1_nprocshared", "");
    project.step1_cpu             = parser.getString("step1_cpu", "");
    project.step1_gpucpu          = parser.getString("step1_gpucpu", "");
    project.step1_print_level     = parser.getString("step1_print_level", "");
    project.step1_tail            = parser.getString("step1_tail", "");
    project.step1_route_extra_keywords = parser.getString("step1_route_extra_keywords", "");

    // Step 2 theory parameters
    project.step2_functional      = parser.getString("step2_functional", "uwb97xd");
    project.step2_basis           = parser.getString("step2_basis", "def2tzvp");
    project.step2_basis_path      = parser.getString("step2_basis_path", "");
    project.step2_solvent_model   = parser.getString("step2_solvent_model", "smd");
    project.step2_solvent         = parser.getString("step2_solvent", "");
    project.step2_solvent_extra   = parser.getString("step2_solvent_extra", "ExternalIteration,NonEq=read");
    project.step2_scf_maxcycle    = parser.getInt("step2_scf_maxcycle", 500);
    project.step2_mem             = parser.getString("step2_mem", "");
    project.step2_nprocshared     = parser.getString("step2_nprocshared", "");
    project.step2_cpu             = parser.getString("step2_cpu", "");
    project.step2_gpucpu          = parser.getString("step2_gpucpu", "");
    project.step2_print_level     = parser.getString("step2_print_level", "");
    project.step2_tail            = parser.getString("step2_tail", "");
    project.step2_route_extra_keywords = parser.getString("step2_route_extra_keywords", "");

    // Step 3 parameters
    // CLI options take precedence over parameter file values
    project.temperature = use_input_temp ? project_temperature : parser.getDouble("temperature", 298.15);
    project.concentration = use_input_concentration ? project_concentration : parser.getDouble("concentration", 1.0);
    project.pressure = use_input_pressure ? project_pressure : parser.getDouble("pressure", 1.0);
    project.low_vib_method = parser.getString("low_vib_method", "grimme");
    project.ravib = parser.getDouble("ravib", 100.0);
    project.scale_zpe = parser.getDouble("scale_zpe", 1.0);
    project.scale_heat = parser.getDouble("scale_heat", 1.0);
    project.scale_entropy = parser.getDouble("scale_entropy", 1.0);
    project.scale_cv = parser.getDouble("scale_cv", 1.0);
    project.interp_vib = parser.getDouble("interp_vib", 100.0);
    project.imag_real = parser.getDouble("imag_real", 0.0);
    project.external_energy = parser.getDouble("external_energy", 0.0);
    project.mass_mode = parser.getInt("mass_mode", 3);
    project.prt_level = parser.getInt("prt_level", 1);
    project.print_vib = parser.getInt("print_vib", 0);
    project.ip_mode = parser.getInt("ip_mode", 0);
    project.point_group = parser.getString("point_group", "");
    project.hg_entropy = parser.getBool("hg_entropy", false);
    project.bav_preset = parser.getString("bav_preset", "");
    project.output_otm = parser.getBool("output_otm", false);
    project.use_input_temp = use_input_temp || parser.getBool("use_input_temp", false);
    project.use_input_concentration = use_input_concentration || parser.getBool("use_input_concentration", false);
    project.use_input_pressure = use_input_pressure || parser.getBool("use_input_pressure", false);
    project.out_file = out_file.empty() ? parser.getString("out_file", "") : out_file;

    if (verbose) {
        std::cout << "Loaded parameters from: " << filename << std::endl;
        std::cout << "  Project: " << project.name << std::endl;
        std::cout << "  Calc type: " << (project.calc_type == Marcus::CalcType::OSET ? "OSET" : "EnT") << std::endl;
        std::cout << "  Donor: " << project.donor.name << " (init: " << project.donor.init_state.charge
                  << "," << project.donor.init_state.mult << "; final: "
                  << project.donor.final_state.charge << "," << project.donor.final_state.mult << ")" << std::endl;
        std::cout << "  Acceptor: " << project.acceptor.name << " (init: "
                  << project.acceptor.init_state.charge << "," << project.acceptor.init_state.mult
                  << "; final: " << project.acceptor.final_state.charge << ","
                  << project.acceptor.final_state.mult << ")" << std::endl;
        std::cout << "  Step1: " << project.step1_functional << "/" << project.step1_basis << std::endl;
        std::cout << "  Step2: " << project.step2_functional << "/" << project.step2_basis << std::endl;
    }

    return true;
}

int MarcusCommand::generate_template(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot create template file: " << filename << std::endl;
        return 1;
    }

    bool is_ent = (template_calc_type == "ent");

    file << "# ============================================================================\n";
    file << "# ComChemKit Marcus Barrier Parameter File\n";
    file << "# Calculation type: " << (is_ent ? "EnT" : "OSET") << "\n";
    file << "# Generated by: cck marcus --gen" << template_calc_type << "-params\n";
    file << "# Edit this file and use with: cck marcus -p " << filename << " -step <1|2|3>\n";
    file << "# ============================================================================\n";
    file << "\n";

    // Project identification
    file << "# ----------------------------------------------------------------------------\n";
    file << "# PROJECT IDENTIFICATION\n";
    file << "# ----------------------------------------------------------------------------\n";
    file << "project = my_marcus_project          # Project name; used for directory names\n";
    file << "calc_type = " << template_calc_type << "          # oset | ent\n";
    file << "step = 1,2                           # 1 = gen opt inputs, 2 = gen sp inputs, 3 = calc barriers\n";
    file << "                                     # (default 1,2: generate both input sets up front;\n";
    file << "                                     #  submit step-1 jobs first, then step-2 jobs)\n";
    file << "\n";

    // Donor / Acceptor definitions
    file << "# ----------------------------------------------------------------------------\n";
    file << "# DONOR / ACCEPTOR DEFINITIONS (shared by all steps)\n";
    file << "# ----------------------------------------------------------------------------\n";
    file << "# Donor\n";
    file << "donor_name        = PC\n";
    file << "donor_xyz         = PC.xyz\n";
    if (is_ent) {
        file << "# For EnT, donor states are electronic states (e.g., S0, T1);\n";
        file << "# charge/mult refer to that state's charge and multiplicity\n";
        file << "donor_init_state  = S0\n";
        file << "donor_final_state = T1\n";
    }
    file << "donor_init_charge = 0\n";
    file << "donor_init_mult   = 1\n";
    file << "donor_final_charge = 0\n";
    file << "donor_final_mult  = 1\n";
    file << "\n";
    file << "# Acceptor\n";
    file << "acceptor_name        = D1\n";
    file << "acceptor_xyz         = D1.xyz\n";
    if (is_ent) {
        file << "acceptor_init_state  = S0\n";
        file << "acceptor_final_state = T1\n";
    }
    file << "acceptor_init_charge = 0\n";
    file << "acceptor_init_mult   = 1\n";
    file << "acceptor_final_charge = 0\n";
    file << "acceptor_final_mult   = 1\n";
    file << "\n";

    // Step 1 theory
    file << "# ----------------------------------------------------------------------------\n";
    file << "# STEP 1 THEORY — Geometry optimization + Freq at LOW level (opt_freq)\n";
    file << "# All CreateInput opt_freq parameters supported, prefixed with step1_.\n";
    file << "# ----------------------------------------------------------------------------\n";
    file << "step1_functional      = uwb97xd\n";
    file << "step1_basis           = def2svpp\n";
    file << "step1_basis_path      = /path/to/basis/ma-def2svpp.gbs\n";
    file << "step1_solvent_model   = smd\n";
    file << "step1_solvent         = n,n-DiMethylAcetamide\n";
    file << "step1_scf_maxcycle    = 300\n";
    file << "step1_opt_maxcycles   = 300\n";
    file << "step1_opt_options     =\n";
    file << "step1_scf_options     =\n";
    file << "step1_opt_restart     = false\n";
    file << "step1_scf_restart     = false\n";
    file << "# step1_mem             = 20GB\n";
    file << "# step1_nprocshared     = 16\n";
    file << "# step1_cpu             =\n";
    file << "# step1_gpucpu          =\n";
    file << "# step1_print_level     = P\n";
    file << "# step1_tail            =\n";
    file << "# step1_route_extra_keywords =\n";
    file << "\n";

    // Step 2 theory
    file << "# ----------------------------------------------------------------------------\n";
    file << "# STEP 2 THEORY — Single point at HIGH level (sp)\n";
    file << "# All CreateInput sp parameters supported, prefixed with step2_.\n";
    file << "# ----------------------------------------------------------------------------\n";
    file << "step2_functional      = uwb97xd\n";
    file << "step2_basis           = def2tzvp        # typically higher level than step1\n";
    file << "step2_basis_path      = /path/to/basis/ma-def2tzvp.gbs\n";
    file << "step2_solvent_model   = smd\n";
    file << "step2_solvent         = n,n-DiMethylAcetamide\n";
    file << "step2_solvent_extra   = ExternalIteration,NonEq=read\n";
    file << "step2_scf_maxcycle    = 500\n";
    file << "# step2_mem             = 20GB\n";
    file << "# step2_nprocshared     = 16\n";
    file << "# step2_cpu             =\n";
    file << "# step2_gpucpu          =\n";
    file << "# step2_print_level     =\n";
    file << "# step2_tail            =\n";
    file << "# step2_route_extra_keywords =\n";
    file << "\n";

    // Step 3
    file << "# ----------------------------------------------------------------------------\n";
    file << "# STEP 3 — Barrier calculation\n";
    file << "# ----------------------------------------------------------------------------\n";
    file << "marcus_mode = symmetric       # symmetric | asymmetric\n";
    file << "skip_existing = true\n";
    file << "temperature = 298.15          # K (for thermal corrections & phase correction)\n";
    file << "concentration = 1.0           # mol/L (for phase correction)\n";
    file << "pressure = 1.0                # atm (for phase correction)\n";
    file << "low_vib_method = grimme       # harmonic|truhlar|grimme|minenkov|headgordon\n";
    file << "ravib = 100.0                 # cm^-1, crossover for quasi-RRHO\n";
    file << "# scale_zpe = 1.0             # ZPE scaling factor\n";
    file << "# scale_heat = 1.0            # Heat capacity scaling factor\n";
    file << "# scale_entropy = 1.0         # Entropy scaling factor\n";
    file << "# scale_cv = 1.0              # CV scaling factor\n";
    file << "# interp_vib = 100.0          # cm^-1, vibrational interpolation parameter\n";
    file << "# imag_real = 0.0             # Imaginary frequency threshold\n";
    file << "# external_energy = 0.0       # External electronic energy (Hartree)\n";
    file << "# mass_mode = 3               # Mass assignment mode\n";
    file << "# prt_level = 1               # Output verbosity: 0=minimal, 1=default, 2=verbose, 3=full\n";
    file << "# print_vib = 0               # Print vibration contributions\n";
    file << "# ip_mode = 0                 # 0 for isolated systems, 1 for periodic solid state\n";
    file << "# point_group =               # Point group label (e.g., C1, C2v, D2h)\n";
    file << "# hg_entropy = false          # Enable entropy interpolation for Head-Gordon method\n";
    file << "# bav_preset =                # qchem|grimme, Bav preset for free-rotor entropy\n";
    file << "# output_otm = false          # Output .otm file flag\n";
    file << "# use_input_temp = true       # uncomment to use -t pathway (thermo module)\n";
    file << "# use_input_concentration = true  # uncomment to use -c pathway\n";
    file << "# use_input_pressure = true   # uncomment to use -p pathway (phase correction)\n";
    file << "out_file =                   # optional; default: <project>_barrier/<project>_barrier_report.txt\n";
    file << "\n";

    file << "# ----------------------------------------------------------------------------\n";
    file << "# WORKFLOW INSTRUCTIONS\n";
    file << "# ----------------------------------------------------------------------------\n";
    file << "# Step 1: Generate geometry optimization inputs:\n";
    file << "#   cck marcus -p " << filename << " -step 1\n";
    file << "#   -> Creates .gau files in <project>_step_1_opt/\n";
    file << "#   Submit these to Gaussian, wait for completion.\n";
    file << "\n";
    file << "# Step 2: Generate single point inputs (reads step-1 checkpoints):\n";
    file << "#   cck marcus -p " << filename << " -step 2\n";
    file << "#   -> Creates .gau files in <project>_step_2_sp/\n";
    file << "#   Submit these to Gaussian, wait for completion.\n";
    file << "\n";
    file << "# Step 3: Calculate barriers:\n";
    file << "#   cck marcus -p " << filename << " -step 3 -mode symmetric\n";
    file << "#   cck marcus -p " << filename << " -step 3 -mode asymmetric\n";
    file << "#   -> Writes barrier report to <project>_barrier/\n";
    file << "\n";

    file.close();

    std::cout << "Template file created: " << filename << std::endl;
    std::cout << "Edit the parameters and use with:" << std::endl;
    std::cout << "  cck marcus -p " << filename << " -step 1" << std::endl;
    return 0;
}

int MarcusCommand::run_step1(const Marcus::MarcusProject& project) {
    std::cout << "=== Step 1: Geometry Optimization Input Generation ===" << std::endl;
    std::cout << "Project: " << project.name << std::endl;
    std::cout << "Output directory: " << project.name << "_step_1_opt/" << std::endl;

    if (!generate_step1_inputs(project)) {
        std::cerr << "Error: Step 1 input generation failed." << std::endl;
        return 1;
    }

    std::cout << "\nStep 1 complete." << std::endl;
    std::cout << "Next: Submit the .gau files in " << project.name << "_step_1_opt/ to Gaussian." << std::endl;
    std::cout << "      After completion, run: cck marcus -p <param> -step 2" << std::endl;
    return 0;
}

int MarcusCommand::run_step2(const Marcus::MarcusProject& project) {
    std::cout << "=== Step 2: Single Point Input Generation ===" << std::endl;
    std::cout << "Project: " << project.name << std::endl;
    std::cout << "Output directory: " << project.name << "_step_2_sp/" << std::endl;
    std::cout << "Non-cross inputs read step-1 checkpoints via absolute path." << std::endl;

    if (!generate_step2_inputs(project)) {
        std::cerr << "Error: Step 2 input generation failed." << std::endl;
        return 1;
    }

    std::cout << "\nStep 2 complete." << std::endl;
    std::cout << "Next: In " << project.name << "_step_2_sp/, first submit the non-cross" << std::endl;
    std::cout << "      (<mol>-<state>_{init,final}.gau) jobs, then the cross" << std::endl;
    std::cout << "      (*-from-*.gau) jobs (they read the non-cross .chk files)." << std::endl;
    std::cout << "      After completion, run: cck marcus -p <param> -step 3" << std::endl;
    return 0;
}

int MarcusCommand::run_step3(const Marcus::MarcusProject& project) {
    std::cout << "=== Step 3: Marcus Barrier Calculation ===" << std::endl;
    std::cout << "Project: " << project.name << std::endl;
    std::cout << "Mode: " << (project.barrier_mode == Marcus::BarrierMode::SYMMETRIC ? "symmetric" : "asymmetric") << std::endl;

    // Set directory paths on the project (needed by calculator)
    Marcus::MarcusProject proj = project;
    std::string dir_step1, dir_step2, dir_barrier;
    Marcus::create_project_directories(proj.name, dir_step1, dir_step2, dir_barrier);
    proj.dir_step1_opt = dir_step1;
    proj.dir_step2_sp = dir_step2;
    proj.dir_barrier = dir_barrier;

    Marcus::MarcusResult result;
    if (!calculate_barrier_step3(proj, result)) {
        std::cerr << "Error: Barrier calculation failed: " << result.error_message << std::endl;
        return 1;
    }

    // Print results to console
    std::cout << "\n";
    std::cout << "Results:" << std::endl;
    std::cout << "--------" << std::endl;
    std::cout << "Thermo pathway: "
              << (proj.use_input_temp || proj.use_input_concentration
                  ? "thermo module (T=" + std::to_string(proj.temperature) + " K, C="
                    + std::to_string(proj.concentration) + " M)"
                  : "string-matching from log files") << std::endl;
    if (result.phase_corr_applied) {
        std::cout << "Phase correction: " << result.phase_correction << " Hartree (applied)" << std::endl;
    }
    std::cout << "Delta G°r = " << result.dG_r_kcal << " kcal/mol (" << result.dG_r_hartree << " Hartree)" << std::endl;
    std::cout << "lambda R  = " << result.lambda_R_kcal << " kcal/mol" << std::endl;
    std::cout << "lambda P  = " << result.lambda_P_kcal << " kcal/mol" << std::endl;
    std::cout << "lambda    = " << result.lambda_avg_kcal << " kcal/mol" << std::endl;
    std::cout << "Delta G‡  = " << result.barrier_sym_kcal << " kcal/mol (symmetric)" << std::endl;
    if (project.barrier_mode == Marcus::BarrierMode::ASYMMETRIC) {
        std::cout << "Delta G‡  = " << result.barrier_asym_kcal << " kcal/mol (asymmetric)" << std::endl;
    }

    // Write report
    Marcus::write_barrier_report(proj.dir_barrier, proj, result, proj.out_file);

    std::cout << "\nStep 3 complete. Report written to " << proj.name << "_barrier/" << std::endl;
    return 0;
}

int MarcusCommand::execute(const CommandContext& /*context*/) {
    // Template generation mode: generate a parameter template and exit
    if (generate_template_flag) {
        std::string output_file = param_file.empty() 
            ? ("marcus_" + template_calc_type + ".params") 
            : param_file;
        return generate_template(output_file);
    }

    if (param_file.empty()) {
        std::cerr << "Error: No parameter file specified. Use -p <file>." << std::endl;
        print_help();
        return 1;
    }

    Marcus::MarcusProject project;
    if (!load_parameters(param_file, project)) {
        return 1;
    }

    // Run the requested steps in order (default: 1,2 generates both input sets
    // up front; submit step-1 jobs first, then step-2 jobs).
    for (int n : project.requested_steps) {
        Marcus::MarcusProject proj = project;
        proj.current_step = static_cast<Marcus::Step>(n - 1);
        int rc = 0;
        switch (proj.current_step) {
            case Marcus::Step::STEP_1_OPT:
                rc = run_step1(proj);
                break;
            case Marcus::Step::STEP_2_SP:
                rc = run_step2(proj);
                break;
            case Marcus::Step::STEP_3_BARRIER:
                rc = run_step3(proj);
                break;
            default:
                std::cerr << "Error: Unknown step." << std::endl;
                return 1;
        }
        if (rc != 0) return rc;
    }
    return 0;
}