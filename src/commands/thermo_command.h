/**
 * @file thermo_command.h
 * @brief Defines the ThermoCommand class for advanced thermodynamic analysis.
 * @author Le Nhan Pham
 * @date 2026
 *
 * The ThermoCommand integrates the OpenThermo module logic, enabling users to
 * perform post-calculation thermodynamic adjustments out of the standard Gaussian
 * workflow. This includes scaling vibrational frequencies, changing temperatures/pressures,
 * and swapping isotopes.
 */

#ifndef THERMO_COMMAND_H
#define THERMO_COMMAND_H

#include "commands/icommand.h"
#include "thermo/thermo.h"

/**
 * @class ThermoCommand
 * @brief Command for performing localized thermochemistry calculations using OpenThermo.
 *
 * This command handles the `--thermo` options including pressure/temperature scans,
 * isotopic substitutions, and scaling factors for vibrational frequencies.
 */
class ThermoCommand : public ICommand {
public:
    std::string get_name() const override;
    std::string get_description() const override;
    void parse_args(int argc, char* argv[], int& i, CommandContext& context) override;
    int execute(const CommandContext& context) override;

private:
    ThermoInterface::ThermoSettings settings;
    std::string thermo_help_topic = "";
};

#endif // THERMO_COMMAND_H
