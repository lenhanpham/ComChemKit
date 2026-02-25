/**
 * @file extract_command.h
 * @brief Defines the ExtractCommand class for thermodynamic data extraction.
 * @author Le Nhan Pham
 * @date 2026
 *
 * This file contains the implementation for the default `extract` command.
 * It interfaces directly with the QCExtractor module to pull thermodynamic
 * properties and basic molecular information from Gaussian output logs.
 */

#ifndef EXTRACT_COMMAND_H
#define EXTRACT_COMMAND_H

#include "commands/icommand.h"

/**
 * @class ExtractCommand
 * @brief Command for extracting thermodynamic data and energy components from log files.
 *
 * This command parses extraction-specific arguments and delegates processing to the
 * QCExtractor module to gather properties like SCF energy, ZPE, enthalpy, and free energy.
 */
class ExtractCommand : public ICommand {
public:
    /**
     * @brief Constructs an ExtractCommand with default configuration.
     */
    ExtractCommand();
    std::string get_name() const override;
    std::string get_description() const override;
    void parse_args(int argc, char* argv[], int& i, CommandContext& context) override;
    int execute(const CommandContext& context) override;

private:
    double      temp = 298.15;
    double      pressure = 1.0;
    int         concentration = 1000;
    int         sort_column = 2;
    std::string output_format = "text";
    bool        use_input_temp = false;
    bool        use_input_pressure = false;
    bool        use_input_concentration = false;
    size_t      memory_limit_mb = 0;
    bool        show_resource_info = false;
};

#endif // EXTRACT_COMMAND_H
