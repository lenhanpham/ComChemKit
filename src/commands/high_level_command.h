/**
 * @file high_level_command.h
 * @brief Defines the HighLevelCommand class for high-accuracy energy calculations.
 * @author Le Nhan Pham
 * @date 2026
 *
 * This command handles the processing of log files generated from high-level
 * composite methods (e.g., CBS-QB3, G4, W1BD). It extracts the constituent
 * energies, applies thermal corrections, and outputs the final tabulated data
 * in either Hatree or kJ/mol.
 */

#ifndef HIGH_LEVEL_COMMAND_H
#define HIGH_LEVEL_COMMAND_H

#include "commands/icommand.h"
#include "commands/command_system.h"

/**
 * @class HighLevelCommand
 * @brief Command for processing high-level thermochemistry calculations (e.g., CBS-QB3, W1U).
 *
 * This command targets the extraction and tabulation of highly accurate energy methods,
 * typically converting results to kJ/mol or reporting in Hartrees based on the CommandType.
 */
class HighLevelCommand : public ICommand {
    CommandType type;
    std::string name;
    std::string desc;
public:
    /**
     * @brief Constructs a HighLevelCommand for a specific unit type.
     * 
     * @param type The specific CommandType (e.g., HIGH_LEVEL_KJ, HIGH_LEVEL_AU).
     * @param name The CLI string used to invoke this command.
     * @param desc A brief description of this specific command variant.
     */
    HighLevelCommand(CommandType type, std::string name, std::string desc);
    
    std::string get_name() const override;
    std::string get_description() const override;
    void parse_args(int argc, char* argv[], int& i, CommandContext& context) override;
    int execute(const CommandContext& context) override;
    
    // Mapped methods implemented in module_executor.cpp
    int execute_kj(const CommandContext& context);
    int execute_au(const CommandContext& context);

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

#endif // HIGH_LEVEL_COMMAND_H
