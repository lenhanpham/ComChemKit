/**
 * @file help_utils.cpp
 * @brief Implementation of help utility functions (thin hub).
 *
 * Each module owns its help text via ICommand::print_help().
 * This hub only aggregates (top-level list) and dispatches
 * (command-specific help) through the CommandRegistry, plus
 * shared blocks (common options, config help).
 */

#include "ui/help_utils.h"
#include "commands/command_registry.h"
#include "commands/icommand.h"
#include "commands/command_system.h"
#include "utilities/config_manager.h"
#include "utilities/version.h"
#include <iomanip>
#include <iostream>

using namespace ComChemKit;

namespace HelpUtils
{

    static std::string display_name(const std::string& registry_name)
    {
        // CLI keeps short aliases for checker family (pre-existing UX: `cck done`, not `cck check-done`).
        if (registry_name == "check-done") return "done";
        if (registry_name == "check-errors") return "errors";
        if (registry_name == "check-pcm") return "pcm";
        if (registry_name == "check-imaginary") return "imode";
        if (registry_name == "check-all") return "check";
        return registry_name;
    }

    void print_help(const std::string& program_name)
    {
        std::cout << "ComChemKit (Version " << ComChemKit::get_version_info() << ")\n\n";
        std::cout << "Usage: " << program_name << " [command] [options]\n\n";
        std::cout << "Commands:\n";
        auto all = CommandRegistry::get_instance().get_all_commands();
        // Curated order preserves the historic UX; new registry entries not in this
        // list are appended alphabetically so no hub edit is needed for new modules.
        const std::vector<std::string> order = {"extract",      "extract-au", "check-done", "check-errors",
                                                "check-pcm",    "check-imaginary", "check-all",  "high-kj",
                                                "high-au",      "xyz",        "ci",         "ivcoord",
                                                "rmsd",         "thermo",     "marcus"};
        for (const auto& name : order)
        {
            auto it = all.find(name);
            if (it != all.end())
            {
                std::cout << "  " << std::left << std::setw(18) << display_name(it->first)
                          << it->second->get_description() << "\n";
                all.erase(it);
            }
        }
        for (const auto& pair : all)
        {
            std::cout << "  " << std::left << std::setw(18) << display_name(pair.first)
                      << pair.second->get_description() << "\n";
        }
        std::cout << "\nOptions:\n";
        std::cout << "  -h, --help        Show this help message\n";
        std::cout << "  -v, --version     Show version information\n";
        std::cout << "  --config-help     Show configuration file help\n";
        std::cout << "  --create-config   Create a default configuration file\n";
        std::cout << "  --show-config     Show current configuration settings\n";
        std::cout << "\nRun '" << program_name << " <command> --help' for command-specific help.\n";
        std::cout << "\n";
    }

    bool print_command_help_by_name(const std::string& command_name, const std::string& program_name)
    {
        ICommand* cmd = CommandRegistry::get_instance().get_command(command_name);
        if (cmd != nullptr)
        {
            cmd->print_help(program_name);
            return true;
        }
        std::cerr << "Unknown command: " << command_name << "\n";
        std::cerr << "Run '" << program_name << " --help' for a list of available commands.\n";
        return false;
    }

    void print_command_help(CommandType command, const std::string& program_name, const CommandContext* context)
    {
        (void)context;  // Kept for backward compatibility; routing needs no context.
        std::string cmd_name = CommandParser::get_command_name(command);
        if (!print_command_help_by_name(cmd_name, program_name))
        {
            print_help(program_name);
        }
    }

    void print_common_options()
    {
        std::cout << "Options:\n";
        std::cout << "  -e, --ext <ext>       File extension: log|out (default: log)\n";
        std::cout << "  -nt, --threads <N>    Thread count: number|max|half (default: half)\n";
        std::cout << "  -q, --quiet           Quiet mode (minimal output)\n";
        std::cout << "  --max-file-size <MB>  Maximum file size in MB (default: 500)\n";
        std::cout << "  --batch-size <N>      Batch size for large directories (default: auto)\n";
    }

    void print_config_help()
    {
        std::cout << "ComChemKit Configuration System\n\n";
        std::cout << ConfigUtils::get_config_search_paths();
        std::cout << std::endl;

        std::cout << "Commands:\n";
        std::cout << "  --show-config     Show current configuration\n";
        std::cout << "  --create-config   Create default configuration file\n";
        std::cout << "  --config-help     Show this configuration help\n\n";

        std::cout << "Configuration file format:\n";
        std::cout << "  # Lines starting with # are comments\n";
        std::cout << "  key = value\n";
        std::cout << "  # Values can be quoted: key = \"value with spaces\"\n\n";

        std::cout << "Example configuration entries:\n";
        std::cout << "  default_temperature = 298.15\n";
        std::cout << "  default_concentration = 2.0\n";
        std::cout << "  output_extensions = .log,.out\n";
        std::cout << "  input_extensions = .com,.gjf,.gau\n";
        std::cout << "  default_threads = 4\n\n";
    }

    void create_default_config()
    {
        std::cout << "Creating default configuration file...\n";

        if (g_config_manager.create_default_config_file())
        {
            std::string home_dir = g_config_manager.get_user_home_directory();
            if (!home_dir.empty())
            {
                std::cout << "Configuration file created at: " << home_dir << "/.qc_extractor.conf" << std::endl;
            }
            else
            {
                std::cout << "Configuration file created at: ./.qc_extractor.conf" << std::endl;
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

}  // namespace HelpUtils
