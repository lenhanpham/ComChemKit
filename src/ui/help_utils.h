/**
 * @file help_utils.h
 * @brief Help utility functions for all commands
 * @author Le Nhan Pham
 * @date 2025
 *
 * This file contains declarations for help-related utility functions
 * used throughout the ComChemKit application.
 */

#ifndef HELP_UTILS_H
#define HELP_UTILS_H

#include "commands/command_system.h"  // For CommandType
#include <string>

namespace HelpUtils
{

    /**
     * @brief Print general help information (aggregated from registry).
     * @param program_name Name of the program (default: "cck")
     */
    void print_help(const std::string& program_name = "cck");

    /**
     * @brief Print command-specific help by dispatching to the owning ICommand.
     * @param command The command type to show help for
     * @param program_name Name of the program (default: "cck")
     *
     * Kept for backward compatibility. New code should prefer
     * print_command_help_by_name(). Each module owns its text via
     * ICommand::print_help(); this function only routes.
     */
    void print_command_help(CommandType           command,
                            const std::string&    program_name = "cck",
                            const CommandContext* context      = nullptr);

    /**
     * @brief Print help for a command by registry name (preferred).
     * @param command_name Registry name (e.g. "extract", "ci", "xyz", "rmsd", "marcus", "thermo").
     * @return true if a command was found and helped, false otherwise.
     */
    bool print_command_help_by_name(const std::string& command_name,
                                    const std::string& program_name = "cck");

    /**
     * @brief Print the 5 options common to every command.
     *
     * Called by each ICommand::print_help() so the hub stays thin and
     * the common block cannot drift between modules.
     */
    void print_common_options();

    /**
     * @brief Print configuration help information
     */
    void print_config_help();

    /**
     * @brief Create default configuration file
     */
    void create_default_config();

}  // namespace HelpUtils

#endif  // HELP_UTILS_H