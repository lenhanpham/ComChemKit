/**
 * @file main.cpp
 * @brief Main entry point for the ComChemKit application
 * @author Le Nhan Pham
 * @date 2025
 *
 * This file contains the main function and signal handling for the ComChemKit,
 * a high-performance tool for processing Gaussian computational chemistry log files.
 * The application supports various commands including extraction, job checking, and
 * high-level energy calculations.
 *
 * @section Safety, resouce management, and features
 * - Multi-threaded file processing with resource management
 * - Job scheduler integration (SLURM, PBS, SGE, LSF)
 * - Comprehensive error detection and job status checking
 * - High-level energy calculations with thermal corrections
 * - Configurable through configuration files and command-line options
 * - Graceful shutdown handling for long-running operations
 */

#include "extraction/qc_extractor.h"
#include "ui/interactive_mode.h"
#include "commands/command_system.h"
#include "utilities/config_manager.h"
#include "commands/signal_handler.h"
#include "utilities/version.h"
#include "commands/command_registry.h"
#include "commands/extract_command.h"
#include "commands/thermo_command.h"
#include "commands/checker_command.h"
#include "commands/high_level_command.h"
#include "commands/extract_coords_command.h"
#include "commands/create_input_command.h"
#include <atomic>
#include <csignal>
#include <iostream>


/**
 * @brief Global flag to indicate when a shutdown has been requested
 *
 * This atomic boolean is used to coordinate graceful shutdown across all threads
 * when a termination signal (SIGINT, SIGTERM) is received. All long-running
 * operations should periodically check this flag and terminate cleanly.
 */
std::atomic<bool> g_shutdown_requested{false};

/**
 * @brief Signal handler for graceful shutdown
 *
 * This function is called when the application receives termination signals
 * (SIGINT from Ctrl+C, SIGTERM from system shutdown, etc.). It sets the global
 * shutdown flag to coordinate clean termination of all threads and operations.
 *
 * @param signal The signal number that was received
 *
 * @note This function is signal-safe and only performs async-signal-safe operations
 * @see g_shutdown_requested
 */
void signalHandler(int signal)
{
    std::cerr << "\nReceived signal " << signal << ". Initiating graceful shutdown..." << std::endl;
    g_shutdown_requested.store(true);
}


/**
 * @brief Main entry point for the ComChemKit application
 *
 * The main function handles the complete application lifecycle:
 * 1. Sets up signal handlers for graceful shutdown
 * 2. Initializes the configuration system
 * 3. Parses command-line arguments and options
 * 4. Dispatches to the appropriate command handler
 * 5. Handles exceptions and provides appropriate exit codes
 *
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 *
 * @return Exit code: 0 for success, non-zero for various error conditions
 *
 * @section Exit Codes
 * - 0: Successful execution
 * - 1: General error (exceptions, unknown commands, etc.)
 * - Command-specific exit codes may also be returned
 *
 * @section Signal Handling
 * The application installs handlers for SIGINT and SIGTERM to enable graceful
 * shutdown of long-running operations. When these signals are received, the
 * global shutdown flag is set and all threads are notified to terminate cleanly.
 *
 * @section Configuration
 * The configuration system is initialized first, loading settings from:
 * - Default configuration file (.qc_extractor.conf)
 * - User-specified configuration file
 * - Command-line overrides
 *
 * Configuration errors are reported as warnings but don't prevent execution.
 *
 * @section Error Handling
 * All exceptions are caught at the top level to ensure clean termination:
 * - std::exception derived exceptions show the error message
 * - Unknown exceptions show a generic error message
 * - All exceptions result in exit code 1
 *
 * @note This function coordinates the entire application flow and ensures
 *       proper resource cleanup even in error conditions
 */
int main(int argc, char* argv[])
{
    // Initialize configuration system FIRST so commands can access preferences
    if (!g_config_manager.load_config())
    {
        // Configuration loaded with warnings/errors - continue with defaults
        auto errors = g_config_manager.get_load_errors();
        if (!errors.empty())
        {
            std::cerr << "Configuration warnings:" << std::endl;
            for (const auto& error : errors)
            {
                std::cerr << "  " << error << std::endl;
            }
            std::cerr << std::endl;
        }
    }

    // Bootstrap Command Registry
    auto& registry = CommandRegistry::get_instance();
    registry.register_command(std::make_unique<ExtractCommand>());
    registry.register_command(std::make_unique<ThermoCommand>());
    registry.register_command(std::make_unique<CheckerCommand>(CommandType::CHECK_DONE, "check-done", "Check and organize completed calculations"));
    registry.register_command(std::make_unique<CheckerCommand>(CommandType::CHECK_ERRORS, "check-errors", "Check and organize failed calculations"));
    registry.register_command(std::make_unique<CheckerCommand>(CommandType::CHECK_PCM, "check-pcm", "Check and organize PCM failures"));
    registry.register_command(std::make_unique<CheckerCommand>(CommandType::CHECK_IMAGINARY, "check-imaginary", "Check and organize jobs with imaginary frequencies"));
    registry.register_command(std::make_unique<CheckerCommand>(CommandType::CHECK_ALL, "check-all", "Run comprehensive checks for all job types"));
    registry.register_command(std::make_unique<HighLevelCommand>(CommandType::HIGH_LEVEL_KJ, "high-kj", "High-level energies in kJ/mol"));
    registry.register_command(std::make_unique<HighLevelCommand>(CommandType::HIGH_LEVEL_AU, "high-au", "High-level energies in atomic units"));
    registry.register_command(std::make_unique<ExtractCoordsCommand>());
    registry.register_command(std::make_unique<CreateInputCommand>());

    // Install signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try
    {
        // Configuration already loaded at startup

        // Check if running without arguments
        bool no_arguments = (argc == 1);


        if (no_arguments)
        {
#ifdef _WIN32
            // On Windows, no arguments means double-clicked - show intro and enter interactive mode
            std::cout << std::endl;
            std::cout << "==================================================" << std::endl;
            std::cout << ComChemKit::get_version_info() << std::endl;
            std::cout << "==================================================" << std::endl;
            std::cout << std::endl;

            std::cout << "Welcome to CCK interactive mode!" << std::endl;
            std::cout << std::endl;
            std::cout << "This tool helps you play with computational chemistry using Gaussian:" << std::endl;
            std::cout << "> High-performance multi-threaded extraction of thermodynamic data and energy components"
                      << std::endl;
            std::cout << "> Job status checking and error detection" << std::endl;
            std::cout << "> High-level theory Gibbs free energy calculations with thermal corrections " << std::endl;
            std::cout << "> Coordinate extraction and Gaussian input file generation" << std::endl;
            std::cout << std::endl;
            std::cout << "For help and available commands, type 'help' in interactive mode." << std::endl;
            std::cout << "Type 'help <command>' for command-specific help, e.g. 'help ci' for input creation."
                      << std::endl;
            std::cout << "To exit, type 'exit' or 'quit'." << std::endl;
            std::cout << std::endl;

            // Enter interactive mode for Windows users
            return run_interactive_loop();
#else
            // On Linux/macOS, no arguments means run default extract command and exit
            std::cout << "Running default EXTRACT command..." << std::endl;
            CommandContext extract_context = CommandParser::parse(1, argv);

            // Show warnings if any
            if (!extract_context.warnings.empty() && !extract_context.quiet)
            {
                for (const auto& warning : extract_context.warnings)
                {
                    std::cerr << warning << std::endl;
                }
                std::cerr << std::endl;
            }

            // Execute EXTRACT and exit
            ICommand* cmd = CommandRegistry::get_instance().get_command("extract");
            int extract_result = cmd ? cmd->execute(extract_context) : 1;

            // On Linux, exit immediately after command execution
            return extract_result;
#endif
        }
        else
        {
            // Parse command and context (will use configuration defaults)
            CommandContext context = CommandParser::parse(argc, argv);

            // Show warnings if any and not in quiet mode
            if (!context.warnings.empty() && !context.quiet)
            {
                for (const auto& warning : context.warnings)
                {
                    std::cerr << warning << std::endl;
                }
                std::cerr << std::endl;
            }

            // Execute based on command type dispatcher
            int command_result = 1;
            std::string cmd_name = CommandParser::get_command_name(context.command);
            ICommand* cmd = CommandRegistry::get_instance().get_command(cmd_name);
            
            if (cmd) {
                command_result = cmd->execute(context);
            } else {
                std::cerr << "Error: Unknown or unregistered command type: " << cmd_name << std::endl;
            }

            return command_result;
        }
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

    return 0;
}
