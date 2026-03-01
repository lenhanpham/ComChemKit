/**
 * @file create_input_command.h
 * @brief Defines the CreateInputCommand class for generating Gaussian input files.
 * @author Le Nhan Pham
 * @date 2026
 *
 * The CreateInputCommand bridges user-defined calculation parameters via the CLI
 * with the underlying CreateInput module. It parses parameters such as functional,
 * basis set, solvent model, and charge/multiplicity to automatically generate
 * ready-to-run `.com` files from molecular coordinates.
 */

#ifndef CREATE_INPUT_COMMAND_H
#define CREATE_INPUT_COMMAND_H

#include "commands/icommand.h"

/**
 * @class CreateInputCommand
 * @brief Command for generating Gaussian input (.com) files from coordinates or templates.
 *
 * This command parses calculation specification arguments (e.g., theory level, basis set,
 * solvent) and instructs the CreateInput module to generate runnable Gaussian inputs.
 */
class CreateInputCommand : public ICommand {
public:
    std::string get_name() const override;
    std::string get_description() const override;
    void parse_args(int argc, char* argv[], int& i, CommandContext& context) override;
    int execute(const CommandContext& context) override;

private:
    std::string ci_calc_type = "sp";
    std::string ci_functional = "UwB97XD";
    std::string ci_basis = "Def2SVPP";
    std::string ci_large_basis = "";
    std::string ci_solvent = "";
    std::string ci_solvent_model = "smd";
    std::string ci_solvent_extra = "";
    std::string ci_print_level = "";
    std::string ci_extra_keywords = "";
    std::string ci_extra_keyword_section = "";
    int         ci_charge = 0;
    int         ci_mult = 1;
    std::string ci_tail = "";
    std::string ci_modre = "";
    std::string ci_extension = ".gau";
    std::string ci_tschk_path = "";
    int         ci_freeze_atom1 = 0;
    int         ci_freeze_atom2 = 0;
    int         ci_scf_maxcycle = -1;
    int         ci_opt_maxcycles = -1;
    int         ci_opt_maxstep = -1;
    int         ci_irc_maxpoints = -1;
    int         ci_irc_recalc = -1;
    int         ci_irc_maxcycle = -1;
    int         ci_irc_stepsize = -1;
    std::string ci_tddft_method = "tda";
    std::string ci_tddft_states = "";
    int         ci_tddft_nstates = 15;
    std::string ci_tddft_extra = "";
};

#endif // CREATE_INPUT_COMMAND_H
