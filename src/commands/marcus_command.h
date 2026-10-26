/**
 * @file marcus_command.h
 * @brief Defines the MarcusCommand class for Marcus theory barrier calculations
 * (OSET & EnT, four-point method).
 * 
 * This command implements a 3-step workflow:
 *   Step 1: Generate geometry optimization inputs (<project>_step_1_opt/)
 *   Step 2: Generate single point inputs reading step-1 checkpoints (<project>_step_2_sp/)
 *   Step 3: Calculate barriers from collected outputs (<project>_barrier/)
 * 
 * All user parameters are specified in a parameter file (ComChemKit ParameterParser format).
 */

#ifndef MARCUS_COMMAND_H
#define MARCUS_COMMAND_H

#include "commands/icommand.h"
#include "marcus/marcus_types.h"
#include <string>

class MarcusCommand : public ICommand {
public:
    MarcusCommand();

    std::string get_name() const override;
    std::string get_description() const override;
    void print_help(const std::string& program_name = "cck") const override;
    void parse_args(int argc, char* argv[], int& i, CommandContext& context) override;
    int execute(const CommandContext& context) override;

private:
    std::string param_file;
    std::string step_arg;              // raw -step value, e.g. "1", "2", "1,2" (empty = not given on CLI)
    bool        step_cli_given = false;
    Marcus::BarrierMode barrier_mode = Marcus::BarrierMode::SYMMETRIC;
    std::string out_file;
    bool        verbose = false;
    bool        generate_template_flag = false;   // Whether to generate a template
    std::string template_calc_type = "oset";      // "oset" or "ent"

    // Thermo options (mirror high-kj/high-au)
    double      project_temperature = 298.15;
    double      project_concentration = 1.0;
    double      project_pressure = 1.0;
    std::string project_low_vib_method = "grimme";
    double      project_ravib = 100.0;
    bool        use_input_temp = false;
    bool        use_input_concentration = false;
    bool        use_input_pressure = false;

    // Parameter file parsing
    bool load_parameters(const std::string& filename, Marcus::MarcusProject& project);

    // Step dispatch
    int run_step1(const Marcus::MarcusProject& project);
    int run_step2(const Marcus::MarcusProject& project);
    int run_step3(const Marcus::MarcusProject& project);

    // Template generation
    int generate_template(const std::string& filename);

    // Helpers
    Marcus::CalcType parse_calc_type(const std::string& s);
    Marcus::BarrierMode parse_barrier_mode(const std::string& s);
};

#endif  // MARCUS_COMMAND_H