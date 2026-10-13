/**
 * @file extract_au_command.h
 * @brief Defines the ExtractAuCommand class (single-file component breakdown).
 * @author Le Nhan Pham
 * @date 2026
 *
 * This file contains the `extract-au` command. It behaves like the default
 * `extract` command (single-file: electronic energy and thermal corrections
 * are both read from each log file in the current directory) but prints a
 * `high-au`-style energy component breakdown in atomic units:
 * Output name | E a.u | ZPE a.u | TC a.u | TS a.u | H a.u | G a.u | LowFQ | PhaseCorr
 *
 * There is intentionally NO Status column: extract-au prepares a full table
 * of data for reporting. Status is still computed internally and drives the
 * process exit code, but it is never printed.
 */

#ifndef EXTRACT_AU_COMMAND_H
#define EXTRACT_AU_COMMAND_H

#include "commands/icommand.h"

/**
 * @class ExtractAuCommand
 * @brief Single-file energy component breakdown in atomic units.
 *
 * Reads electronic energies and thermal corrections from the SAME file
 * (extract behaviour) and decomposes the energy into components
 * (high-au behaviour: E, ZPE, TC, TS, H, G, LowFQ, PhaseCorr).
 */
class ExtractAuCommand : public ICommand {
public:
    /**
     * @brief Constructs an ExtractAuCommand with default configuration.
     */
    ExtractAuCommand();
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
    std::string low_vib_method = "grimme";  ///< Low-frequency vibrational treatment method
    double      ravib = 100.0;              ///< Crossover frequency for low-vib treatment (cm-1)
};

#endif // EXTRACT_AU_COMMAND_H
