#include "commands/command_system.h"
#include "utilities/config_manager.h"
#include "input_gen/parameter_parser.h"
#include "ui/help_utils.h"
#include "commands/command_registry.h"
#include "commands/icommand.h"
#include "utilities/utils.h"
#include "utilities/version.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

CommandContext CommandParser::parse(int argc, char* argv[])
{
    // Early check for version before any other processing
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--version" || arg == "-v")
        {
            std::cout << ComChemKit::get_version_info() << std::endl;
            std::exit(0);
        }
    }

    // Configuration is already loaded by main.cpp

    CommandContext context;
    // Apply configuration defaults after config is loaded
    apply_config_to_context(context);

    // Detect job scheduler resources early
    context.job_resources = JobSchedulerDetector::detect_job_resources();

    // If no arguments, default to EXTRACT
    if (argc == 1)
    {
        validate_context(context);
        return context;
    }

    // Scan all arguments to find a command (flexible positioning)
    CommandType found_command = CommandType::EXTRACT;
    int         command_index = -1;

    for (int i = 1; i < argc; ++i)
    {
        CommandType potential_command = parse_command(argv[i]);
        if (potential_command != CommandType::EXTRACT || std::string(argv[i]) == "extract")
        {
            found_command = potential_command;
            command_index = i;
            break;
        }
    }

    context.command = found_command;

    // Parse all arguments, skipping the command if found
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        // Skip the command argument itself
        if (i == command_index)
        {
            continue;
        }

        if (arg == "-h" || arg == "--help")
        {
            if (context.command == CommandType::EXTRACT)
            {
                HelpUtils::print_help();  // Use default parameter
            }
            else
            {
                HelpUtils::print_command_help(context.command, "cck", &context);
            }
            std::exit(0);
        }

        else if (arg == "--config-help")
        {
            HelpUtils::print_config_help();
            std::exit(0);
        }
        else if (arg == "--create-config")
        {
            HelpUtils::create_default_config();
            std::exit(0);
        }
        else if (arg == "--show-config")
        {
            g_config_manager.print_config_summary(true);
            std::exit(0);
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

        // Parse common options first
        int original_i = i;
        parse_common_options(context, i, argc, argv);

        // Delegate parsing of command-specific options to the appropriate ICommand implementation
        if (i == original_i)
        {
            std::string cmd_name = CommandParser::get_command_name(context.command);
            ICommand* cmd = CommandRegistry::get_instance().get_command(cmd_name);
            if (cmd)
            {
                cmd->parse_args(argc, argv, i, context);
            }
            else
            {
                context.warnings.push_back("Warning: Unknown command " + cmd_name + ", arguments may be ignored.");
            }
        }
    }



    validate_context(context);
    return context;
}

CommandType CommandParser::parse_command(const std::string& cmd)
{
    if (cmd == "extract")
        return CommandType::EXTRACT;
    if (cmd == "done")
        return CommandType::CHECK_DONE;
    if (cmd == "errors")
        return CommandType::CHECK_ERRORS;
    if (cmd == "pcm")
        return CommandType::CHECK_PCM;
    if (cmd == "imode" || cmd == "--imaginary")
        return CommandType::CHECK_IMAGINARY;
    if (cmd == "check")
        return CommandType::CHECK_ALL;
    if (cmd == "high-kj" || cmd == "--high-level-kj")
        return CommandType::HIGH_LEVEL_KJ;
    if (cmd == "high-au" || cmd == "--high-level-au")
        return CommandType::HIGH_LEVEL_AU;
    if (cmd == "xyz" || cmd == "--extract-coord")
        return CommandType::EXTRACT_COORDS;
    if (cmd == "ci" || cmd == "--create-input")
        return CommandType::CREATE_INPUT;
    if (cmd == "thermo" || cmd == "--thermo")
        return CommandType::THERMO;

    // If it starts with '-', it's probably an option, not a command
    if (!cmd.empty() && cmd.front() == '-')
        return CommandType::EXTRACT;

    // Default to extract for backward compatibility
    return CommandType::EXTRACT;
}

std::string CommandParser::get_command_name(CommandType command)
{
    switch (command)
    {
        case CommandType::EXTRACT:
            return std::string("extract");
        case CommandType::CHECK_DONE:
            return std::string("check-done");
        case CommandType::CHECK_ERRORS:
            return std::string("check-errors");
        case CommandType::CHECK_PCM:
            return std::string("check-pcm");
        case CommandType::CHECK_IMAGINARY:
            return std::string("check-imaginary");
        case CommandType::CHECK_ALL:
            return std::string("check-all");
        case CommandType::HIGH_LEVEL_KJ:
            return std::string("high-kj");
        case CommandType::HIGH_LEVEL_AU:
            return std::string("high-au");
        case CommandType::EXTRACT_COORDS:
            return std::string("xyz");
        case CommandType::CREATE_INPUT:
            return std::string("ci");
        case CommandType::THERMO:
            return std::string("thermo");
        default:
            return std::string("unknown");
    }
}

void CommandParser::parse_common_options(CommandContext& context, int& i, int argc, char* argv[])
{
    std::string arg = argv[i];

    if (arg == "-q" || arg == "--quiet")
    {
        context.quiet = true;
    }
    else if (arg == "-e" || arg == "--ext")
    {
        if (++i < argc)
        {
            std::string ext      = argv[i];
            std::string full_ext = (ext[0] == '.') ? ext : ("." + ext);
            if (full_ext == ".log" || full_ext == ".out")
            {
                context.extension = full_ext;
            }
            else
            {
                add_warning(context,
                            "Error: Extension '" + ext + "' not in configured output extensions. Using default.");
                context.extension = g_config_manager.get_default_output_extension();
            }
        }
        else
        {
            add_warning(context, "Error: Extension value required after -e/--ext.");
        }
    }
    else if (arg == "-nt" || arg == "--threads")
    {
        if (++i < argc)
        {
            std::string  threads_arg    = argv[i];
            unsigned int hardware_cores = std::thread::hardware_concurrency();
            if (hardware_cores == 0)
                hardware_cores = 4;

            if (threads_arg == "max")
            {
                context.requested_threads = hardware_cores;
            }
            else if (threads_arg == "half")
            {
                context.requested_threads = std::max(1u, hardware_cores / 2);
            }
            else
            {
                try
                {
                    unsigned int req_threads = std::stoul(threads_arg);
                    if (req_threads == 0)
                    {
                        add_warning(context, "Error: Thread count must be at least 1. Using configured default.");
                        context.requested_threads = g_config_manager.get_default_threads();
                    }
                    else
                    {
                        context.requested_threads = req_threads;
                    }
                }


                catch (const std::exception& e)
                {
                    add_warning(context, "Error: Invalid thread count format. Using configured default.");
                    context.requested_threads = g_config_manager.get_default_threads();
                }
            }
        }
        else
        {
            add_warning(context, "Error: Thread count required after -nt/--threads.");
        }
    }
    else if (arg == "--max-file-size")
    {
        if (++i < argc)
        {
            try
            {
                int size = std::stoi(argv[i]);
                if (size <= 0)
                {
                    add_warning(context, "Error: Max file size must be positive. Using default 100MB.");
                }
                else
                {
                    context.max_file_size_mb = static_cast<size_t>(size);
                }
            }
            catch (const std::exception& e)
            {
                add_warning(context, "Error: Invalid max file size format. Using default 100MB.");
            }
        }
        else
        {
            add_warning(context, "Error: Max file size value required after --max-file-size.");
        }
    }
    else if (arg == "--batch-size")
    {
        if (++i < argc)
        {
            try
            {
                int size = std::stoi(argv[i]);
                if (size <= 0)
                {
                    add_warning(context, "Error: Batch size must be positive. Using default (auto-detect).");
                }
                else
                {
                    context.batch_size = static_cast<size_t>(size);
                }
            }
            catch (const std::exception& e)
            {
                add_warning(context, "Error: Invalid batch size format. Using default (auto-detect).");
            }
        }
        else
        {
            add_warning(context, "Error: Batch size value required after --batch-size.");
        }
    }
}



// In command_system.cpp, modify parse_xyz_options:

void CommandParser::add_warning(CommandContext& context, const std::string& warning)
{
    context.warnings.push_back(warning);
}


void CommandParser::validate_context(CommandContext& context)
{
    // Set default threads if not specified
    if (context.requested_threads == 0)
    {
        context.requested_threads = g_config_manager.get_default_threads();
    }

    // Validate file size limits
    if (context.max_file_size_mb == 0)
    {
        context.max_file_size_mb = g_config_manager.get_default_max_file_size();
    }
}

void CommandParser::apply_config_to_context(CommandContext& context)
{
    // Apply configuration defaults to context
    if (!g_config_manager.is_config_loaded())
    {
        return;  // Keep built-in defaults if config not loaded
    }

    // Apply configuration values
    context.quiet              = g_config_manager.get_bool("quiet_mode");
    context.requested_threads  = g_config_manager.get_default_threads();
    context.max_file_size_mb   = g_config_manager.get_default_max_file_size();
    context.extension          = g_config_manager.get_default_output_extension();
    context.valid_extensions   = ConfigUtils::split_string(g_config_manager.get_string("output_extensions"), ',');
}

void CommandParser::load_configuration()
{
    // Try to load configuration file
    if (!g_config_manager.is_config_loaded())
    {
        g_config_manager.load_config();
    }
}


void CommandParser::create_default_config()
{
    std::cout << "Creating default configuration file...\n";

    if (g_config_manager.create_default_config_file())
    {
        std::string home_dir = g_config_manager.get_user_home_directory();
        if (!home_dir.empty())
        {
            std::cout << "Configuration file created at: " << home_dir << "/.cck.conf" << std::endl;
        }
        else
        {
            std::cout << "Configuration file created at: ./.cck.conf" << std::endl;
        }
        std::cout << "Edit this file to customize your default settings.\n";
    }
    else
    {
        std::cout << "Failed to create configuration file.\n";
        std::cout << "You can create it manually using the template below:\n\n";
        g_config_manager.print_config_file_template();
    }
}

std::unordered_map<std::string, std::string> CommandParser::extract_config_overrides(int argc, char* argv[])
{
    std::unordered_map<std::string, std::string> overrides;

    for (int i = 1; i < argc - 1; ++i)
    {
        std::string arg = argv[i];
        if (arg.substr(0, 9) == "--config-")
        {
            std::string key   = arg.substr(9);
            std::string value = argv[i + 1];
            overrides[key]    = value;
            ++i;  // Skip the value argument
        }
    }

    return overrides;
}




