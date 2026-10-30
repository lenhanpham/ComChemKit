/**
 * @file marcus_command.cpp
 * @brief Implementation of MarcusCommand for Marcus theory barrier calculations.
 */

#include "commands/marcus_command.h"
#include "ui/help_utils.h"
#include "marcus/marcus_input_gen.h"
#include "marcus/marcus_calculator.h"
#include "marcus/marcus_io.h"
#include "marcus/marcus_auto.h"
#include "input_gen/parameter_parser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <regex>

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

void MarcusCommand::print_help(const std::string& program_name) const {
    std::string cmd_name = get_name();
    std::cout << "Help for command: " << cmd_name << "\n\n";
    std::cout << "Usage: " << program_name << " marcus <input_file> [-step <1|2|3>]\n";
    std::cout << "       [-O <outfile>] [-v] [thermo options]\n\n";
    std::cout << "Marcus Theory Barrier Calculations (OSET / EnT, Four-Point Method)\n";
    std::cout << "================================================================\n\n";
    std::cout << "Calculates Marcus free-energy barriers for Outer Sphere Single\n";
    std::cout << "Electron Transfer (OSET) and Energy Transfer (EnT) using the\n";
    std::cout << "explicit four-point reorganization energy method.\n\n";
    std::cout << "Manual workflow (user submits Gaussian jobs between steps):\n";
    std::cout << "  Step 1: Generate geometry optimization inputs in <project>_step_1/\n";
    std::cout << "          User submits to Gaussian, waits for completion.\n";
    std::cout << "  Step 2: Generate single point inputs in <project>_step_2/\n";
    std::cout << "          (reads step-1 checkpoints via %OldChk + Geom=Check)\n";
    std::cout << "          User submits to Gaussian, waits for completion.\n";
    std::cout << "  Step 3: Calculate symmetric and asymmetric barriers in <project>.log\n";
    std::cout << "  Leave step1_solvent and step2_solvent blank for gas-phase calculations.\n";
    std::cout << "Automatic: set workflow = auto and qc_engine = g16 (or executable path).\n";
    std::cout << "  Runs 4 optimization, 4 equilibrium and 4 cross jobs sequentially.\n";
    std::cout << "  Input step is ignored in auto mode; CLI -step is not allowed.\n\n";
    std::cout << "Options:\n";
    std::cout << "  <input_file>   Input file in ComChemKit key=value format (any extension)\n";
    std::cout << "                 Project defaults to the input filename without its last extension\n";
    std::cout << "  -step <n>      Workflow step: 1=opt inputs, 2=sp inputs, 3=barriers\n";
    std::cout << "  -O <file>      Output file for barrier report (step 3)\n";
    std::cout << "  -v, --verbose  Verbose report and console diagnostics\n";
    std::cout << "                 Input key report_level = default|verbose also selects report detail\n\n";
    std::cout << "Thermo options (mirror high-kj/high-au):\n";
    std::cout << "  -t <K>         Temperature in Kelvin for thermal corrections\n";
    std::cout << "  -c <M>         Concentration in mol/L for phase correction\n";
    std::cout << "  --pressure <atm> Pressure for thermo recalculation (default: 1.0)\n";
    std::cout << "  --lowvibmeth <m> Low-freq vibrational treatment (default: grimme)\n";
    std::cout << "                   Options: harmonic|truhlar|grimme|minenkov|headgordon\n";
    std::cout << "  --ravib <cm-1> Crossover frequency for quasi-RRHO (default: 100.0)\n\n";
    std::cout << "Input template generation:\n";
    std::cout << "  --genoset-input  Generate OSET input template\n";
    std::cout << "  --genent-input   Generate EnT input template\n";
    std::cout << "  -o <file>      Output file for template (default: marcus_<type>.inp)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " marcus reaction.inp -step 1\n";
    std::cout << "  " << program_name << " marcus reaction.inp -step 2\n";
    std::cout << "  " << program_name << " marcus reaction.inp -step 3\n";
    std::cout << "  " << program_name << " marcus reaction.inp -step 3 -v\n";
    HelpUtils::print_common_options();
    std::cout << "  -h, --help            Show this help message\n";
    std::cout << "  --version             Show version information\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " " << cmd_name << " reaction.inp -q           # Quiet mode\n";
    std::cout << "  " << program_name << " " << cmd_name << " reaction.inp -nt 4        # Use 4 threads\n";
    std::cout << "\n";
}

void MarcusCommand::parse_args(int argc, char* argv[], int& i, CommandContext& /*context*/) {
    std::string arg = argv[i];

    if (arg == "-step") {
        if (i + 1 < argc) {
            step_arg = argv[++i];
            step_cli_given = true;
        } else {
            std::cerr << "Error: -step requires an argument (1, 2, 3, or 1,2)" << std::endl;
            parse_error = true;
        }
    }
    else if (arg == "-O" || arg == "--out" || arg == "--output") {
        if (i + 1 < argc) {
            out_file = argv[++i];
        } else {
            std::cerr << "Error: -O requires an output file argument" << std::endl;
            parse_error = true;
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
            parse_error = true;
        }
    }
    else if (arg == "-c" || arg == "-C" || arg == "--cm" || arg == "--conc") {
        if (i + 1 < argc) {
            project_concentration = std::stod(argv[++i]);
            use_input_concentration = true;
        } else {
            std::cerr << "Error: -c requires a concentration argument" << std::endl;
            parse_error = true;
        }
    }
    else if (arg == "--pressure") {
        if (i + 1 < argc) {
            project_pressure = std::stod(argv[++i]);
            use_input_pressure = true;
        } else {
            std::cerr << "Error: --pressure requires a pressure argument" << std::endl;
            parse_error = true;
        }
    }
    else if (arg == "--lowvibmeth") {
        if (i + 1 < argc) {
            project_low_vib_method = argv[++i];
            low_vib_cli_given = true;
        } else {
            std::cerr << "Error: --lowvibmeth requires a method argument" << std::endl;
            parse_error = true;
        }
    }
    else if (arg == "--ravib") {
        if (i + 1 < argc) {
            project_ravib = std::stod(argv[++i]);
            ravib_cli_given = true;
        } else {
            std::cerr << "Error: --ravib requires a frequency argument" << std::endl;
            parse_error = true;
        }
    }
    else if (arg == "-h" || arg == "--help") {
        print_help("cck");
        std::exit(0);
    }
    else if (arg == "--genoset-input") {
        generate_template_flag = true;
        template_calc_type = "oset";
    }
    else if (arg == "--genent-input") {
        generate_template_flag = true;
        template_calc_type = "ent";
    }
    else if (arg == "-o" || arg == "--output-template" || arg == "--template-out") {
        if (i + 1 < argc) {
            template_file = argv[++i];
        } else {
            std::cerr << "Error: -o requires an output file argument" << std::endl;
            parse_error = true;
        }
    }
    else if (!arg.empty() && arg[0] != '-') {
        if (!input_file.empty()) {
            std::cerr << "Error: Only one Marcus input file may be specified" << std::endl;
            parse_error = true;
        } else {
            input_file = arg;
        }
    }
    else if (arg != "-q" && arg != "--quiet") {
        // The outer parser handles common options; flag-only quiet is also passed here.
        std::cerr << "Error: Unknown Marcus option: " << arg << std::endl;
        parse_error = true;
    }
}

bool MarcusCommand::load_input(const std::string& filename,
                                     Marcus::MarcusProject& project) {
    ParameterParser parser;
    if (!parser.loadFromFile(filename)) {
        std::cerr << "Error: Cannot load input file: " << filename << std::endl;
        return false;
    }

    if (parser.hasParameter("marcus_mode")) {
        std::cerr << "Error: marcus_mode is no longer supported; remove it from the input. "
                     "Both barriers are calculated automatically." << std::endl;
        return false;
    }

    // Project identification
    project.name = parser.getString("project", "");
    if (project.name.empty()) {
        project.name = std::filesystem::path(filename).stem().string();
    }
    if (project.name.empty()) {
        std::cerr << "Error: Cannot derive project name from input file: " << filename << std::endl;
        return false;
    }
    project.calc_type = parse_calc_type(parser.getString("calc_type", "oset"));
    std::string workflow = parser.getString("workflow", "manual");
    std::transform(workflow.begin(), workflow.end(), workflow.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (workflow != "manual" && workflow != "auto") {
        std::cerr << "Error: workflow must be manual or auto" << std::endl;
        return false;
    }
    project.automatic_workflow = workflow == "auto";
    project.qc_engine = parser.getString("qc_engine", "");
    if (project.automatic_workflow) {
        if (step_cli_given) {
            std::cerr << "Error: -step cannot be combined with workflow = auto; automatic mode runs all stages."
                      << std::endl;
            return false;
        }
        if (project.qc_engine.empty()) {
            std::cerr << "Error: workflow = auto requires qc_engine (e.g. qc_engine = g16)." << std::endl;
            return false;
        }
        project.requested_steps = {1, 2, 3};
        project.current_step = Marcus::Step::STEP_1_OPT;
    } else {
        // Step selection: CLI -step overrides the input-file `step` key.
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
    }

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

    // Solvent route keywords cannot be reconciled with a blank solvent. Do not
    // silently remove user-supplied route extras; solvent_extra is simply unused.
    const std::regex solvent_keyword(R"(\b(scrf|noneq|externaliteration)\b)", std::regex::icase);
    if (project.step1_solvent.empty() &&
        std::regex_search(project.step1_route_extra_keywords, solvent_keyword)) {
        std::cerr << "Error: step1_route_extra_keywords contains a solvent keyword but step1_solvent is blank."
                  << std::endl;
        return false;
    }
    if (project.step2_solvent.empty() &&
        std::regex_search(project.step2_route_extra_keywords, solvent_keyword)) {
        std::cerr << "Error: step2_route_extra_keywords contains a solvent keyword but step2_solvent is blank."
                  << std::endl;
        return false;
    }

    // Step 3 parameters
    // CLI options take precedence over parameter file values
    project.temperature = use_input_temp ? project_temperature : parser.getDouble("temperature", 298.15);
    project.concentration = use_input_concentration ? project_concentration : parser.getDouble("concentration", 1.0);
    project.pressure = use_input_pressure ? project_pressure : parser.getDouble("pressure", 1.0);
    project.low_vib_method = low_vib_cli_given ? project_low_vib_method : parser.getString("low_vib_method", "grimme");
    project.ravib = ravib_cli_given ? project_ravib : parser.getDouble("ravib", 100.0);
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
    std::string report_level = parser.getString("report_level", "default");
    std::transform(report_level.begin(), report_level.end(), report_level.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (report_level != "default" && report_level != "verbose") {
        std::cerr << "Error: report_level must be default or verbose" << std::endl;
        return false;
    }
    project.verbose_report = verbose || report_level == "verbose";

    if (verbose) {
        std::cout << "Loaded input from: " << filename << std::endl;
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
    file << "# ComChemKit Marcus Barrier Input File\n";
    file << "# Calculation type: " << (is_ent ? "EnT" : "OSET") << "\n";
    file << "# Generated by: cck marcus --gen" << template_calc_type << "-input\n";
    file << "# Edit this file and use with: cck marcus \"" << filename << "\" -step <1|2|3>\n";
    file << "# ============================================================================\n";
    file << "\n";

    // Project identification
    file << "# ----------------------------------------------------------------------------\n";
    file << "# PROJECT IDENTIFICATION\n";
    file << "# ----------------------------------------------------------------------------\n";
    file << "# project = my_marcus_project        # Optional; defaults to input filename without extension\n";
    file << "calc_type = " << template_calc_type << "                   # oset | ent\n";
    file << "workflow = manual              # manual | auto (auto runs all stages sequentially)\n";
    file << "# qc_engine = g16              # required for auto; executable name or path\n";
    file << "step = 1,2                     # manual: 1 = opt, 2 = sp, 3 = barriers; ignored in auto\n";
    file << "                               # (default 1,2: generate both input sets up front;\n";
    file << "                               #  submit step-1 jobs first, then step-2 jobs)\n";
    file << "\n";

    // Donor / Acceptor definitions
    file << "# ----------------------------------------------------------------------------\n";
    file << "# DONOR / ACCEPTOR DEFINITIONS (shared by all steps)\n";
    file << "# ----------------------------------------------------------------------------\n";
    file << "# Donor\n";
    file << "donor_name        = Donor\n";
    file << "donor_xyz         = Donor.xyz\n";
    if (is_ent) {
        file << "# For EnT, donor states are electronic states (e.g., S0, T1);\n";
        file << "# charge/mult refer to that state's charge and multiplicity\n";
        file << "donor_init_state  = S0\n";
        file << "donor_final_state = T1\n";
    }
    file << "donor_init_charge = 0\n";
    file << "donor_init_mult   = 3\n";
    file << "donor_final_charge = 1\n";
    file << "donor_final_mult  = 2\n";
    file << "\n";
    file << "# Acceptor\n";
    file << "acceptor_name        = Acceptor\n";
    file << "acceptor_xyz         = Acceptor.xyz\n";
    if (is_ent) {
        file << "acceptor_init_state  = S0\n";
        file << "acceptor_final_state = T1\n";
    }
    file << "acceptor_init_charge = 0\n";
    file << "acceptor_init_mult   = 1\n";
    file << "acceptor_final_charge = -1\n";
    file << "acceptor_final_mult   = 2\n";
    file << "\n";

    // Step 1 theory
    file << "# ----------------------------------------------------------------------------\n";
    file << "# STEP 1 THEORY — Geometry optimization + Freq at LOW level (opt_freq)\n";
    file << "# All CreateInput opt_freq parameters supported, prefixed with step1_.\n";
    file << "# ----------------------------------------------------------------------------\n";
    file << "step1_functional      = uwb97xd\n";
    file << "step1_basis           = def2svpp\n";
    file << "#step1_basis_path      = /path/to/basis/ma-def2svpp.gbs\n";
    file << "step1_solvent_model   = smd\n";
    file << "step1_solvent         = water          # leave blank for gas phase\n";
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
    file << "# step2_basis_path    = /path/to/basis/ma-def2tzvp.gbs  # for a custom basis\n";
    file << "step2_solvent_model   = smd\n";
    file << "step2_solvent         = water          # leave blank for gas phase\n";
    file << "step2_solvent_extra   = ExternalIteration,NonEq=read  # ignored in gas phase\n";
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
    file << "report_level = default       # default | verbose; -v also enables verbose reporting\n";
    file << "out_file =                   # progress/report path; default: <project>.log in current directory\n";
    file << "\n";

    file << "# ----------------------------------------------------------------------------\n";
    file << "# Thermochemistry settings for Barrier calculation\n";
    file << "# ----------------------------------------------------------------------------\n";
    file << "#temperature = 298.15          # K (for thermal corrections & phase correction)\n";
    file << "#concentration = 1.0           # mol/L (for phase correction)\n";
    file << "#pressure = 1.0                # atm (thermo recalculation; phase standard is fixed at 1 atm)\n";
    file << "#low_vib_method = grimme       # harmonic|truhlar|grimme|minenkov|headgordon\n";
    file << "#ravib = 100.0                 # cm^-1, crossover for quasi-RRHO\n";
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
    file << "# use_input_pressure = true   # uncomment to use thermo-module pressure pathway\n";
    file << "\n";

    file << "# ----------------------------------------------------------------------------\n";
    file << "# WORKFLOW INSTRUCTIONS\n";
    file << "# ----------------------------------------------------------------------------\n";
    file << "# Step 1: Generate geometry optimization inputs:\n";
    file << "#   cck marcus \"" << filename << "\" -step 1\n";
    file << "#   -> Creates .gau files in <project>_step_1/\n";
    file << "#   Submit these to Gaussian, wait for completion.\n";
    file << "\n";
    file << "# Step 2: Generate single point inputs (reads step-1 checkpoints):\n";
    file << "#   cck marcus \"" << filename << "\" -step 2\n";
    file << "#   -> Creates .gau files in <project>_step_2/\n";
    file << "#   Submit these to Gaussian, wait for completion.\n";
    file << "\n";
    file << "# Step 3: Calculate barriers:\n";
    file << "#   cck marcus \"" << filename << "\" -step 3\n";
    file << "#   -> Calculates both symmetric and asymmetric barriers\n";
    file << "#   Gas phase: leave both step1_solvent and step2_solvent blank.\n";
    file << "# Automatic: set workflow = auto and qc_engine = g16, then run without -step.\n";
    file << "#   -> Writes <project>.log in the current directory\n";
    file << "\n";

    file.close();

    std::cout << "Template file created: " << filename << std::endl;
    std::cout << "Edit the input file and use with:" << std::endl;
    std::cout << "  cck marcus \"" << filename << "\" -step 1" << std::endl;
    return 0;
}

int MarcusCommand::run_step1(const Marcus::MarcusProject& project) {
    std::cout << "=== Step 1: Geometry Optimization Input Generation ===" << std::endl;
    std::cout << "Project: " << project.name << std::endl;
    std::cout << "Output directory: " << project.name << "_step_1/" << std::endl;

    if (!generate_step1_inputs(project)) {
        std::cerr << "Error: Step 1 input generation failed." << std::endl;
        return 1;
    }

    std::cout << "\nStep 1 complete." << std::endl;
    std::cout << "Next: Submit the .gau files in " << project.name << "_step_1/ to Gaussian." << std::endl;
    std::cout << "      After completion, run: cck marcus <input_file> -step 2" << std::endl;
    return 0;
}

int MarcusCommand::run_step2(const Marcus::MarcusProject& project) {
    std::cout << "=== Step 2: Single Point Input Generation ===" << std::endl;
    std::cout << "Project: " << project.name << std::endl;
    std::cout << "Output directory: " << project.name << "_step_2/" << std::endl;
    std::cout << "Non-cross inputs read step-1 checkpoints via absolute path." << std::endl;

    if (!generate_step2_inputs(project)) {
        std::cerr << "Error: Step 2 input generation failed." << std::endl;
        return 1;
    }

    std::cout << "\nStep 2 complete." << std::endl;
    std::cout << "Next: In " << project.name << "_step_2/, first submit the non-cross" << std::endl;
    std::cout << "      (<mol>-<state>_{init,final}.gau) jobs, then the cross" << std::endl;
    std::cout << "      (*-from-*.gau) jobs (they read the non-cross .chk files)." << std::endl;
    std::cout << "      After completion, run: cck marcus <input_file> -step 3" << std::endl;
    return 0;
}

int MarcusCommand::run_step3(const Marcus::MarcusProject& project) {
    std::cout << "=== Step 3: Marcus Barrier Calculation ===" << std::endl;
    std::cout << "Project: " << project.name << std::endl;
    std::cout << "Calculating symmetric and asymmetric barriers." << std::endl;

    // Set directory paths on the project (needed by calculator)
    Marcus::MarcusProject proj = project;
    proj.dir_step1_opt = proj.name + "_step_1";
    proj.dir_step2_sp = proj.name + "_step_2";

    // A .log input (or explicit output path) must never be overwritten by the report.
    std::filesystem::path report_path = proj.out_file.empty()
        ? std::filesystem::path(proj.name).filename().string() + ".log"
        : proj.out_file;
    std::error_code ec;
    if (std::filesystem::equivalent(input_file, report_path, ec)) {
        std::cerr << "Error: Report path would overwrite the input file; choose a different -O path." << std::endl;
        return 1;
    }

    Marcus::MarcusResult result;
    if (!calculate_barrier_step3(proj, result)) {
        if (!result.elec.success) {
            // The collector already printed the complete step-2 diagnostic.
            std::cerr << "  Once the jobs are complete, re-run from the same project directory,\n"
                      << "  retaining any other options. For example:\n"
                      << "    cck marcus \"" << input_file << "\" -step 3" << std::endl;
        } else {
            std::cerr << "Error: Barrier calculation failed: " << result.error_message << std::endl;
        }
        return 1;
    }
    bool step1_gas = proj.step1_solvent.empty();
    bool step2_gas = proj.step2_solvent.empty();
    if (step1_gas != step2_gas)
        std::cerr << "Warning: Mixed gas/solution settings; review thermochemistry and phase corrections in the report."
                  << std::endl;
    if (!result.elec.pcm_lines_in_gas_logs.empty() || !result.elec.scrf_text_in_gas_logs.empty() ||
        (step1_gas && step2_gas && (proj.donor.init_state.has_scrf || proj.donor.final_state.has_scrf ||
                                    proj.acceptor.init_state.has_scrf || proj.acceptor.final_state.has_scrf)))
        std::cerr << "Warning: Solvent-related text detected in logs for a gas-phase configuration; "
                     "verify the logs listed in the report." << std::endl;

    // Print results to console
    std::cout << "\n";
    std::cout << "Results:" << std::endl;
    std::cout << "--------" << std::endl;
    std::cout << "Thermo pathway: "
              << (proj.use_input_temp || proj.use_input_concentration || proj.use_input_pressure
                  ? "thermo module requested (see report for per-state provenance)"
                  : "values read from step-1 logs") << std::endl;
    if (result.phase_corr_applied) {
        std::cout << "Per-state phase: " << proj.donor.init_state.phase_corr
                  << " Hartree (SCRF, T=" << proj.donor.init_state.temperature_used
                  << " K, C=" << proj.concentration << " M); net on dG_r = "
                  << result.phase_correction << " Hartree" << std::endl;
    }
    if (result.gibbs_valid)
        std::cout << "Delta G°r = " << result.dG_r_kcal << " kcal/mol (" << result.dG_r_hartree << " Hartree)" << std::endl;
    else
        std::cout << "Delta G°r = N/A (missing Gibbs corrections)" << std::endl;
    std::cout << "lambda R  = " << result.lambda_R_kcal << " kcal/mol" << std::endl;
    std::cout << "lambda P  = " << result.lambda_P_kcal << " kcal/mol" << std::endl;
    std::cout << "lambda    = " << result.lambda_avg_kcal << " kcal/mol" << std::endl;
    if (result.barrier_sym_valid)
        std::cout << "Delta G‡  = " << result.barrier_sym_kcal << " kcal/mol (symmetric)" << std::endl;
    else
        std::cout << "Delta G‡  = N/A (symmetric barrier unavailable)" << std::endl;
    if (result.barrier_asym_valid)
        std::cout << "Delta G‡  = " << result.barrier_asym_kcal << " kcal/mol (asymmetric)" << std::endl;
    else
        std::cout << "Delta G‡  = N/A (asymmetric barrier unavailable; see report)" << std::endl;

    // Write report; use the same completion timestamp in the console.
    std::string completion_footer;
    if (!Marcus::write_barrier_report(proj, result, proj.out_file, &completion_footer)) {
        std::cerr << "Error: Failed to write barrier report." << std::endl;
        return 1;
    }

    if (!result.gibbs_valid || !result.barrier_sym_valid) {
        std::cerr << "Error: Barrier calculation is incomplete; see report for details." << std::endl;
        return 1;
    }
    std::cout << "\nStep 3 complete.\n" << completion_footer;
    return 0;
}

int MarcusCommand::execute(const CommandContext& context) {
    if (parse_error) return 1;

    if (generate_template_flag) {
        if (!input_file.empty()) {
            std::cerr << "Error: Input file cannot be combined with template generation; use -o for its output." << std::endl;
            return 1;
        }
        std::string output_file = template_file.empty()
            ? ("marcus_" + template_calc_type + ".inp")
            : template_file;
        return generate_template(output_file);
    }

    if (!template_file.empty()) {
        std::cerr << "Error: -o is only for input template generation; use -O for reports." << std::endl;
        return 1;
    }
    if (input_file.empty()) {
        std::cerr << "Error: No input file specified. Use cck marcus <input_file>." << std::endl;
        print_help("cck");
        return 1;
    }

    Marcus::MarcusProject project;
    if (!load_input(input_file, project)) {
        return 1;
    }

    if (project.automatic_workflow)
        return Marcus::run_automatic(project, input_file, context.job_resources);

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