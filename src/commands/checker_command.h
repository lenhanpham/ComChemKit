/**
 * @file checker_command.h
 * @brief Defines the CheckerCommand class for job status verification.
 * @author Le Nhan Pham
 * @date 2026
 *
 * This file handles the implementation for the suite of utility commands
 * (e.g., `check-done`, `check-errors`, `check-all`, `check-pcm`). These
 * commands quickly scan large directories of output files to identify
 * completed jobs, failed jobs, or jobs with specific calculation failures.
 */

#ifndef CHECKER_COMMAND_H
#define CHECKER_COMMAND_H

#include "commands/icommand.h"
#include "commands/command_system.h" 

/**
 * @class CheckerCommand
 * @brief Command for checking the status and integrity of computational chemistry jobs.
 *
 * This class handles multiple sub-commands (e.g., check done, check errors, check imaginary
 * frequencies) based on the CommandType provided during instantiation.
 */
class CheckerCommand : public ICommand {
    CommandType type;
    std::string name;
    std::string desc;
public:
    /**
     * @brief Constructs a CheckerCommand for a specific check type.
     * 
     * @param type The specific CommandType (e.g., CHECK_DONE, CHECK_ERRORS).
     * @param name The CLI string used to invoke this command.
     * @param desc A brief description of this specific checker variant.
     */
    CheckerCommand(CommandType type, std::string name, std::string desc);
    
    std::string get_name() const override;
    std::string get_description() const override;
    void parse_args(int argc, char* argv[], int& i, CommandContext& context) override;
    int execute(const CommandContext& context) override;
    
    // Mapped methods implemented in module_executor.cpp
    int execute_check_done(const CommandContext& context);
    int execute_check_errors(const CommandContext& context);
    int execute_check_pcm(const CommandContext& context);
    int execute_check_imaginary(const CommandContext& context);
    int execute_check_all(const CommandContext& context);

private:
    std::string target_dir = "";
    bool        show_error_details = false;
    std::string dir_suffix = "done";
};

#endif // CHECKER_COMMAND_H
