/**
 * @file icommand.h
 * @brief Defines the ICommand interface for the Command Pattern.
 * @author Le Nhan Pham
 * @date 2026
 * 
 * This file contains the abstract base class ICommand, which all concrete
 * commands in the ComChemKit application must implement. It dictates how
 * commands are named, described, parsed from the command line, and executed.
 * It serves as the foundation for the application's CLI routing and modularity.
 */

#ifndef ICOMMAND_H
#define ICOMMAND_H

#include "commands/command_system.h"
#include <string>

/**
 * @class ICommand
 * @brief Interface for actionable commands in the application.
 *
 * The ICommand interface follows the Command Pattern to decouple command
 * parsing and execution from the main application logic. Each derived class
 * represents a specific feature or module that can be invoked via the CLI.
 */
class ICommand {
public:
    /**
     * @brief Virtual destructor for proper cleanup of derived classes.
     */
    virtual ~ICommand() = default;
    
    /**
     * @brief Retrieves the name of the command.
     * 
     * This name corresponds to the keyword used in the CLI to invoke the command.
     * 
     * @return std::string The command name (e.g., "thermo", "extract").
     */
    virtual std::string get_name() const = 0;
    
    /**
     * @brief Retrieves a short description of the command.
     * 
     * This description is displayed in the application's help menu.
     * 
     * @return std::string A brief description of what the command does.
     */
    virtual std::string get_description() const = 0;
    
    /**
     * @brief Parses command-specific arguments from the command line.
     * 
     * @param argc The total number of command-line arguments.
     * @param argv Array of command-line argument strings.
     * @param i The current index in argv. Passed by reference to allow the command to consume arguments.
     * @param context The CommandContext used to store application-level options and warnings.
     */
    virtual void parse_args(int argc, char* argv[], int& i, CommandContext& context) = 0;
    
    /**
     * @brief Executes the command.
     * 
     * @param context The CommandContext containing shared configuration.
     * @return int The exit code of the execution (0 for success, non-zero for failure).
     */
    virtual int execute(const CommandContext& context) = 0;
};

#endif // ICOMMAND_H
