#include "commands/create_input_command.h"
#include "commands/signal_handler.h"
#include "utilities/utils.h"
#include "utilities/config_manager.h"
#include "input_gen/parameter_parser.h"
#include <filesystem>
#include <iostream>
#include <sstream>
#include "input_gen/create_input.h"
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <atomic>
#include <memory>

extern std::atomic<bool> g_shutdown_requested;

extern ConfigManager g_config_manager;

static std::string find_or_create_default_param_file()
{
    const std::string DEFAULT_PARAM_FILENAME = ".ci_parameters.params";
    const std::string ALT_PARAM_FILENAME     = "ci_parameters.params";

    std::vector<std::string> search_paths;

    // 1. Current directory
    search_paths.push_back("./" + DEFAULT_PARAM_FILENAME);
    search_paths.push_back("./" + ALT_PARAM_FILENAME);

    // 2. Executable directory
    std::string exe_dir = ConfigUtils::get_executable_directory();
    if (!exe_dir.empty())
    {
        search_paths.push_back(exe_dir + "/" + DEFAULT_PARAM_FILENAME);
        search_paths.push_back(exe_dir + "/" + ALT_PARAM_FILENAME);
    }

    // 3. User home directory
    std::string home_dir = g_config_manager.get_user_home_directory();
    if (!home_dir.empty())
    {
        search_paths.push_back(home_dir + "/" + DEFAULT_PARAM_FILENAME);
        search_paths.push_back(home_dir + "/" + ALT_PARAM_FILENAME);
    }

    // 4. System config directories (Unix-like systems)
#ifndef _WIN32
    search_paths.push_back("/etc/cck/" + ALT_PARAM_FILENAME);
    search_paths.push_back("/usr/local/etc/" + ALT_PARAM_FILENAME);
#endif

    // Search for existing parameter file
    for (const auto& path : search_paths)
    {
        if (std::filesystem::exists(path))
        {
            std::cout << "Found default parameter file: " << path << std::endl;
            return path;
        }
    }

    // No existing file found, create default in current directory
    std::string     default_path = "./" + DEFAULT_PARAM_FILENAME;
    ParameterParser parser;

    if (parser.generateTemplate("sp", default_path))
    {
        std::cout << "Created default parameter file: " << default_path << std::endl;
        std::cout << "Using default parameters from newly created file." << std::endl;
        return default_path;
    }
    else
    {
        std::cerr << "Error: Failed to create default parameter file: " << default_path << std::endl;
        return "";
    }
}

std::string CreateInputCommand::get_name() const {
    return "ci";
}

std::string CreateInputCommand::get_description() const {
    return "Create Gaussian input files from XYZ files";
}

void CreateInputCommand::parse_args(int argc, char* argv[], int& i, CommandContext& context) {
    std::string arg = argv[i];

    if (arg == "--calc-type")
    {
        if (++i < argc)
        {
            ci_calc_type = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: calc-type requires a value");
        }
    }
    else if (arg == "--functional")
    {
        if (++i < argc)
        {
            ci_functional = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: functional requires a value");
        }
    }
    else if (arg == "--basis")
    {
        if (++i < argc)
        {
            ci_basis = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: basis requires a value");
        }
    }
    else if (arg == "--large-basis")
    {
        if (++i < argc)
        {
            ci_large_basis = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: large-basis requires a value");
        }
    }
    else if (arg == "--solvent")
    {
        if (++i < argc)
        {
            ci_solvent = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: solvent requires a value");
        }
    }
    else if (arg == "--solvent-model")
    {
        if (++i < argc)
        {
            ci_solvent_model = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: solvent-model requires a value");
        }
    }
    else if (arg == "--solvent-extra")
    {
        if (++i < argc)
        {
            ci_solvent_extra = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: solvent-extra requires a value");
        }
    }
    else if (arg == "--tddft-method")
    {
        if (++i < argc)
        {
            ci_tddft_method = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: tddft-method requires a value (td or tda)");
        }
    }
    else if (arg == "--tddft-states")
    {
        if (++i < argc)
        {
            ci_tddft_states = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: tddft-states requires a value (singlets, triplets, or 50-50)");
        }
    }
    else if (arg == "--tddft-nstates")
    {
        if (++i < argc)
        {
            try
            {
                ci_tddft_nstates = std::stoi(argv[i]);
            }
            catch (const std::exception&)
            {
                context.warnings.push_back("Error: tddft-nstates must be an integer");
            }
        }
        else
        {
            context.warnings.push_back("Error: tddft-nstates requires a value");
        }
    }
    else if (arg == "--tddft-extra")
    {
        if (++i < argc)
        {
            ci_tddft_extra = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: tddft-extra requires a value");
        }
    }
    else if (arg == "--charge")
    {
        if (++i < argc)
        {
            try
            {
                ci_charge = std::stoi(argv[i]);
            }
            catch (const std::exception& e)
            {
                context.warnings.push_back("Error: invalid charge value");
            }
        }
        else
        {
            context.warnings.push_back("Error: charge requires a value");
        }
    }
    else if (arg == "--mult")
    {
        if (++i < argc)
        {
            try
            {
                ci_mult = std::stoi(argv[i]);
            }
            catch (const std::exception& e)
            {
                context.warnings.push_back("Error: invalid multiplicity value");
            }
        }
        else
        {
            context.warnings.push_back("Error: mult requires a value");
        }
    }
    else if (arg == "--print-level")
    {
        if (++i < argc)
        {
            ci_print_level = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: print-level requires a value");
        }
    }
    else if (arg == "--extra-keywords")
    {
        if (++i < argc)
        {
            ci_extra_keywords = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: extra-keywords requires a value");
        }
    }
    else if (arg == "--tail")
    {
        if (++i < argc)
        {
            ci_tail = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: tail requires a value");
        }
    }
    else if (arg == "--extension")
    {
        if (++i < argc)
        {
            ci_extension = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: extension requires a value");
        }
    }
    else if (arg == "--tschk-path")
    {
        if (++i < argc)
        {
            ci_tschk_path = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: tschk-path requires a value");
        }
    }
    else if (arg == "--freeze-atoms")
    {
        if (++i < argc)
        {
            try
            {
                ci_freeze_atom1 = std::stoi(argv[i]);
                if (++i < argc)
                {
                    ci_freeze_atom2 = std::stoi(argv[i]);
                }
                else
                {
                    context.warnings.push_back("Error: freeze-atoms requires two values");
                }
            }
            catch (const std::exception& e)
            {
                context.warnings.push_back("Error: freeze-atoms requires integer values");
            }
        }
        else
        {
            context.warnings.push_back("Error: freeze-atoms requires values");
        }
    }
    else if (arg == "--genci-params")
    {
        std::string template_type = "";   // Empty means general template
        std::string directory     = ".";  // Default to current directory
        std::string filename;
        bool        is_general_template = true;

        if (++i < argc)
        {
            std::string first_arg = argv[i];

            if (first_arg[0] == '-')
            {
                // Next argument is another option, use defaults (general template)
                --i;
            }
            else if (first_arg.find('/') != std::string::npos || first_arg.find('\\') != std::string::npos ||
                     first_arg.find('.') == 0 || std::filesystem::exists(first_arg))
            {
                // Argument looks like a directory path
                directory = first_arg;

                // Check if there's a calculation type argument after the directory
                if (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    template_type       = argv[++i];
                    is_general_template = false;
                }
                // If no calc_type after directory, generate general template in that directory
            }
            else
            {
                // Assume it's a calculation type
                template_type       = first_arg;
                is_general_template = false;

                // Check if there's an optional directory argument
                if (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    directory = argv[++i];
                }
            }
        }
        else
        {
            // No arguments provided, use defaults (general template)
            --i;
        }

        // Ensure directory exists or create it
        std::filesystem::path dir_path(directory);
        if (!std::filesystem::exists(dir_path))
        {
            try
            {
                std::filesystem::create_directories(dir_path);
            }
            catch (const std::filesystem::filesystem_error& e)
            {
                std::cerr << "Error: Cannot create directory " << directory << ": " << e.what() << std::endl;
                exit(1);
            }
        }

        ParameterParser parser;
        bool            success;

        if (is_general_template)
        {
            std::filesystem::path base_path  = std::filesystem::path(directory) / "ci_parameters.params";
            std::filesystem::path final_path = Utils::generate_unique_filename(base_path);
            filename                         = final_path.string();
            success                          = parser.generateGeneralTemplate(filename);
        }
        else
        {
            std::filesystem::path base_path  = std::filesystem::path(directory) / (template_type + ".params");
            std::filesystem::path final_path = Utils::generate_unique_filename(base_path);
            filename                         = final_path.string();
            success                          = parser.generateTemplate(template_type, filename);
        }

        if (success)
        {
            std::cout << "Template generated successfully: " << filename << std::endl;
            if (is_general_template)
            {
                std::cout << "This is a general parameter file containing all possible parameters." << std::endl;
                std::cout << "Edit the calc_type and uncomment relevant parameters as needed." << std::endl;
            }
            std::cout << "Use with: cck ci --param-file " << filename << std::endl;
            exit(0);  // Exit after generating template
        }
        else
        {
            if (is_general_template)
            {
                std::cerr << "Failed to generate general template" << std::endl;
            }
            else
            {
                std::cerr << "Failed to generate template for: " << template_type << std::endl;
            }
            exit(1);
        }
    }
    else if (arg == "--genci-all-params")
    {
        std::string directory = ".";  // Default to current directory

        // Check if there's an optional directory argument
        if (++i < argc && argv[i][0] != '-')
        {
            directory = argv[i];
        }
        else
        {
            // No directory provided, use current directory
            --i;  // Reset index since we didn't consume an argument
        }

        // Ensure directory path is absolute for clear feedback
        std::filesystem::path dir_path(directory);
        std::filesystem::path abs_dir_path = std::filesystem::absolute(dir_path);

        ParameterParser parser;
        if (parser.generateAllTemplates(directory))
        {
            exit(0);  // Exit after generating templates
        }
        else
        {
            std::cerr << "Failed to generate templates in: " << abs_dir_path.string() << std::endl;
            exit(1);
        }
    }
    else if (arg == "--param-file")
    {
        std::string param_file;
        std::string detected_calc_type;

        if (++i < argc)
        {
            std::string next_arg = argv[i];
            if (next_arg[0] == '-')
            {
                // Next is another option, use default param
                param_file = find_or_create_default_param_file();
                if (param_file.empty())
                {
                    context.warnings.push_back("Error: Could not find or create default parameter file");
                    return;
                }
                --i;  // Don't consume
            }
            else if (next_arg.size() >= 4 && next_arg.substr(next_arg.size() - 4) == ".xyz")
            {
                // Looks like a xyz file, use default param, and let it be processed as positional later
                param_file = find_or_create_default_param_file();
                if (param_file.empty())
                {
                    context.warnings.push_back("Error: Could not find or create default parameter file");
                    return;
                }
                --i;  // Don't consume
            }
            else
            {
                // Check if it's a known calc_type keyword
                std::string lower_arg = next_arg;
                std::transform(lower_arg.begin(), lower_arg.end(), lower_arg.begin(), ::tolower);
                if (lower_arg == "sp" || lower_arg == "opt_freq" || lower_arg == "ts_freq" ||
                    lower_arg == "modre_opt" || lower_arg == "oss_ts_freq" || lower_arg == "modre_ts_freq" ||
                    lower_arg == "oss_check_sp" || lower_arg == "high_sp" || lower_arg == "irc_forward" ||
                    lower_arg == "irc_reverse" || lower_arg == "irc" || lower_arg == "tddft")
                {
                    // It's a calc_type, use default param and set calc_type
                    param_file = find_or_create_default_param_file();
                    if (param_file.empty())
                    {
                        context.warnings.push_back("Error: Could not find or create default parameter file");
                        return;
                    }
                    detected_calc_type = next_arg;
                    // i already incremented, so consumed
                }
                else
                {
                    // Assume it's a param file
                    param_file = next_arg;
                }
            }
        }
        else
        {
            // No more args, use default
            param_file = find_or_create_default_param_file();
            if (param_file.empty())
            {
                context.warnings.push_back("Error: Could not find or create default parameter file");
                return;
            }
        }

        ParameterParser parser;
        if (parser.loadFromFile(param_file))
        {
            // Apply loaded parameters to context
            ci_calc_type     = parser.getString("calc_type", ci_calc_type);
            ci_functional    = parser.getString("functional", ci_functional);
            ci_basis         = parser.getString("basis", ci_basis);
            ci_large_basis   = parser.getString("large_basis", ci_large_basis);
            ci_solvent       = parser.getString("solvent", ci_solvent);
            ci_solvent_model = parser.getString("solvent_model", ci_solvent_model);
            ci_solvent_extra = parser.getString("solvent_extra", ci_solvent_extra);

            // Convert functional and basis to uppercase for better readability
            std::transform(
                ci_functional.begin(), ci_functional.end(), ci_functional.begin(), ::toupper);
            std::transform(ci_basis.begin(), ci_basis.end(), ci_basis.begin(), ::toupper);
            std::transform(ci_large_basis.begin(),
                           ci_large_basis.end(),
                           ci_large_basis.begin(),
                           ::toupper);
            ci_print_level = parser.getString("print_level", ci_print_level);
            ci_extra_keywords =
                Utils::parseExtraKeywords(parser.getString("route_extra_keywords", ci_extra_keywords));
            std::string extra_options_value = parser.getString("extra_options", "DEFAULT_VALUE");
            std::cout << "DEBUG: extra_options loaded: '" << extra_options_value
                      << "' (length: " << extra_options_value.length() << ")" << std::endl;
            if (extra_options_value != "DEFAULT_VALUE")
            {
                ci_extra_keyword_section = extra_options_value;
            }
            ci_charge     = parser.getInt("charge", ci_charge);
            ci_mult       = parser.getInt("mult", ci_mult);
            ci_tail       = parser.getString("tail", ci_tail);
            ci_modre      = parser.getString("modre", ci_modre);
            ci_extension  = parser.getString("extension", ci_extension);
            ci_tschk_path = parser.getString("tschk_path", ci_tschk_path);
            // Handle freeze atoms - try freeze_atoms first, then fall back to separate parameters
            std::string freeze_atoms_str = parser.getString("freeze_atoms", "");
            if (!freeze_atoms_str.empty())
            {
                // Parse freeze_atoms string (comma or space separated)
                std::vector<int> atoms;
                std::string      cleaned_str = freeze_atoms_str;
                // Remove leading/trailing whitespace
                cleaned_str.erase(cleaned_str.begin(),
                                  std::find_if(cleaned_str.begin(), cleaned_str.end(), [](unsigned char ch) {
                                      return !std::isspace(ch);
                                  }));
                cleaned_str.erase(std::find_if(cleaned_str.rbegin(),
                                               cleaned_str.rend(),
                                               [](unsigned char ch) {
                                                   return !std::isspace(ch);
                                               })
                                      .base(),
                                  cleaned_str.end());

                // Handle comma-separated format: "1,2"
                if (cleaned_str.find(',') != std::string::npos)
                {
                    std::stringstream ss(cleaned_str);
                    std::string       token;
                    while (std::getline(ss, token, ','))
                    {
                        token.erase(token.begin(), std::find_if(token.begin(), token.end(), [](unsigned char ch) {
                                        return !std::isspace(ch);
                                    }));
                        token.erase(std::find_if(token.rbegin(),
                                                 token.rend(),
                                                 [](unsigned char ch) {
                                                     return !std::isspace(ch);
                                                 })
                                        .base(),
                                    token.end());

                        try
                        {
                            int atom = std::stoi(token);
                            atoms.push_back(atom);
                        }
                        catch (const std::exception&)
                        {
                            // Skip invalid tokens
                        }
                    }
                }
                // Handle space-separated format: "1 2"
                else
                {
                    std::stringstream ss(cleaned_str);
                    int               atom;
                    while (ss >> atom)
                    {
                        atoms.push_back(atom);
                    }
                }

                if (atoms.size() >= 2)
                {
                    ci_freeze_atom1 = atoms[0];
                    ci_freeze_atom2 = atoms[1];
                }
            }
            else
            {
                // Fall back to separate freeze_atom1 and freeze_atom2 parameters
                ci_freeze_atom1 = parser.getInt("freeze_atom1", ci_freeze_atom1);
                ci_freeze_atom2 = parser.getInt("freeze_atom2", ci_freeze_atom2);
            }

            // Load custom cycle and optimization parameters (always loaded)
            ci_scf_maxcycle  = parser.getInt("scf_maxcycle", ci_scf_maxcycle);
            ci_opt_maxcycles = parser.getInt("opt_maxcycles", ci_opt_maxcycles);
            ci_opt_maxstep   = parser.getInt("opt_maxstep", ci_opt_maxstep);
            ci_irc_maxpoints = parser.getInt("irc_maxpoints", ci_irc_maxpoints);
            ci_irc_recalc    = parser.getInt("irc_recalc", ci_irc_recalc);
            ci_irc_maxcycle  = parser.getInt("irc_maxcycle", ci_irc_maxcycle);
            ci_irc_stepsize  = parser.getInt("irc_stepsize", ci_irc_stepsize);

            // Load TD-DFT parameters
            ci_tddft_method  = parser.getString("tddft_method", ci_tddft_method);
            ci_tddft_states  = parser.getString("tddft_states", ci_tddft_states);
            ci_tddft_nstates = parser.getInt("tddft_nstates", ci_tddft_nstates);
            ci_tddft_extra   = parser.getString("tddft_extra", ci_tddft_extra);

            std::cout << "Parameters loaded from: " << param_file << std::endl;
            if (!detected_calc_type.empty())
            {
                ci_calc_type = detected_calc_type;
            }
        }
        else
        {
            context.warnings.push_back("Error: Failed to load parameter file: " + param_file);
        }
    }
    else if (!arg.empty() && arg.front() == '-')
    {
        context.warnings.push_back("Warning: Unknown argument '" + arg + "' ignored.");
    }
    else
    {
        // Treat as positional argument (file or comma-separated files)
        // Parse comma-separated filenames and handle whitespace
        std::string file_arg = arg;
        std::replace(file_arg.begin(), file_arg.end(), ',', ' ');

        std::istringstream iss(file_arg);
        std::string        filename;
        while (iss >> filename)
        {
            if (!filename.empty())
            {
                // Trim whitespace from both ends
                filename.erase(0, filename.find_first_not_of(" \t"));
                filename.erase(filename.find_last_not_of(" \t") + 1);

                if (!filename.empty())
                {
                    context.files.push_back(filename);
                }
            }
        }
    }
}



int CreateInputCommand::execute(const CommandContext& context)
{

    try
    {
        std::vector<std::string> xyz_files;
        if (!context.files.empty())
        {
            // Use specific files provided
            for (const auto& file : context.files)
            {
                if (std::filesystem::exists(file) && std::filesystem::is_regular_file(file))
                {
                    xyz_files.push_back(file);
                }
                else
                {
                    std::cerr << "Warning: Specified file '" << file << "' does not exist or is not a regular file."
                              << std::endl;
                }
            }
        }
        else
        {
            // Find all XYZ files in current directory
            for (const auto& entry : std::filesystem::directory_iterator("."))
            {
                if (entry.is_regular_file())
                {
                    std::string extension = entry.path().extension().string();
                    if (extension == ".xyz")
                    {
                        xyz_files.push_back(entry.path().string());
                    }
                }
            }
        }

        if (xyz_files.empty())
        {
            if (!context.quiet)
            {
                std::cout << "No valid .xyz files found." << std::endl;
            }
            return 0;
        }

        auto processing_context = std::make_shared<ProcessingContext>(298.15,  // Default temp, not used
                                                                      1.0,     // Default pressure, not used
                                                                      1000,    // Default conc, not used
                                                                      false,   // use_input_temp not relevant
                                                                      false,   // use_input_pressure not relevant
                                                                      context.requested_threads,
                                                                      ".xyz",  // extension for XYZ files
                                                                      context.max_file_size_mb,
                                                                      context.job_resources);

        // Create CreateInput instance with default parameters
        CreateInput creator(processing_context, context.quiet);

        // Configure calculation parameters from context
        // Convert string calc_type to CalculationType enum
        CalculationType calc_type = CalculationType::SP;  // Default
        if (ci_calc_type == "opt_freq")
        {
            calc_type = CalculationType::OPT_FREQ;
        }
        else if (ci_calc_type == "ts_freq")
        {
            calc_type = CalculationType::TS_FREQ;
        }
        else if (ci_calc_type == "modre_opt")
        {
            calc_type = CalculationType::MODRE_OPT;
        }
        else if (ci_calc_type == "oss_ts_freq")
        {
            calc_type = CalculationType::OSS_TS_FREQ;
        }
        else if (ci_calc_type == "modre_ts_freq")
        {
            calc_type = CalculationType::MODRE_TS_FREQ;
        }
        else if (ci_calc_type == "oss_check_sp")
        {
            calc_type = CalculationType::OSS_CHECK_SP;
        }
        else if (ci_calc_type == "high_sp")
        {
            calc_type = CalculationType::HIGH_SP;
        }
        else if (ci_calc_type == "irc_forward")
        {
            calc_type = CalculationType::IRC_FORWARD;
        }
        else if (ci_calc_type == "irc_reverse")
        {
            calc_type = CalculationType::IRC_REVERSE;
        }
        else if (ci_calc_type == "irc")
        {
            calc_type = CalculationType::IRC;
        }
        else if (ci_calc_type == "tddft")
        {
            calc_type = CalculationType::TDDFT;
        }
        // Default is SP for any other value

        // Validate freeze atoms or modre for OSS_TS_FREQ and MODRE_TS_FREQ
        if (calc_type == CalculationType::OSS_TS_FREQ || calc_type == CalculationType::MODRE_TS_FREQ)
        {
            bool has_freeze_atoms = (ci_freeze_atom1 != 0 && ci_freeze_atom2 != 0);
            bool has_modre        = !ci_modre.empty();

            if (!has_freeze_atoms && !has_modre)
            {
                std::string calc_type_name =
                    (calc_type == CalculationType::OSS_TS_FREQ) ? "oss_ts_freq" : "modre_ts_freq";
                std::cerr << "Error: --freeze-atoms or modre parameter is required for " << calc_type_name
                          << " calculation type." << std::endl;
                std::cerr << "Please specify --freeze-atoms 1 2 or provide modre in the parameter file." << std::endl;
                return 1;
            }
        }

        creator.set_calculation_type(calc_type);
        creator.set_functional(ci_functional);
        creator.set_basis(ci_basis);
        if (!ci_large_basis.empty())
        {
            creator.set_large_basis(ci_large_basis);
        }
        if (!ci_solvent.empty())
        {
            creator.set_solvent(ci_solvent, ci_solvent_model, ci_solvent_extra);
        }
        creator.set_print_level(ci_print_level);
        creator.set_extra_keywords(ci_extra_keywords);
        creator.set_extra_keyword_section(ci_extra_keyword_section);
        creator.set_molecular_specs(ci_charge, ci_mult);
        creator.set_tail(ci_tail);
        creator.set_modre(ci_modre);
        creator.set_extension(ci_extension);
        creator.set_tschk_path(ci_tschk_path);
        if (ci_freeze_atom1 != 0 && ci_freeze_atom2 != 0)
        {
            creator.set_freeze_atoms(ci_freeze_atom1, ci_freeze_atom2);
        }
        creator.set_scf_maxcycle(ci_scf_maxcycle);
        creator.set_opt_maxcycles(ci_opt_maxcycles);
        creator.set_opt_maxstep(ci_opt_maxstep);
        creator.set_irc_maxpoints(ci_irc_maxpoints);
        creator.set_irc_recalc(ci_irc_recalc);
        creator.set_irc_maxcycle(ci_irc_maxcycle);
        creator.set_irc_stepsize(ci_irc_stepsize);
        creator.set_tddft_params(ci_tddft_method, ci_tddft_states, ci_tddft_nstates,
                                  ci_tddft_extra);

        // Execute the creation (with batch processing if enabled)
        CreateSummary total_summary;
        if (context.batch_size > 0 && xyz_files.size() > context.batch_size)
        {
            // Process files in batches
            size_t total_files       = xyz_files.size();
            size_t processed_batches = 0;

            if (!context.quiet)
            {
                std::cout << "Processing " << total_files << " files in batches of " << context.batch_size << std::endl;
            }

            for (size_t i = 0; i < total_files; i += context.batch_size)
            {
                size_t                   batch_end = std::min(i + context.batch_size, total_files);
                std::vector<std::string> batch(xyz_files.begin() + i, xyz_files.begin() + batch_end);

                if (!context.quiet)
                {
                    std::cout << "Processing batch " << (processed_batches + 1) << " (files " << (i + 1) << "-"
                              << batch_end << ")" << std::endl;
                }

                CreateSummary batch_summary = creator.create_inputs(batch);

                // Accumulate results
                total_summary.total_files += batch_summary.total_files;
                total_summary.processed_files += batch_summary.processed_files;
                total_summary.created_files += batch_summary.created_files;
                total_summary.failed_files += batch_summary.failed_files;
                total_summary.skipped_files += batch_summary.skipped_files;
                total_summary.execution_time += batch_summary.execution_time;

                processed_batches++;

                // Check for shutdown signal
                if (g_shutdown_requested.load())
                {
                    if (!context.quiet)
                    {
                        std::cout << "Shutdown requested, stopping batch processing..." << std::endl;
                    }
                    break;
                }
            }

            if (!context.quiet)
            {
                std::cout << "Completed processing " << processed_batches << " batches" << std::endl;
            }
        }
        else
        {
            // Process all files at once (original behavior)
            total_summary = creator.create_inputs(xyz_files);
        }

        // Print summary
        if (!context.quiet)
        {
            creator.print_summary(total_summary, "Input file creation");
        }

        // Check for errors
        if (!processing_context->error_collector->get_errors().empty())
        {
            if (!context.quiet)
            {
                std::cout << "\nProcessing errors:" << std::endl;
                for (const auto& error : processing_context->error_collector->get_errors())
                {
                    std::cout << "  " << error << std::endl;
                }
            }
            return 1;
        }

        return total_summary.failed_files > 0 ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Fatal error: Unknown exception occurred" << std::endl;
        return 1;
    }
}

