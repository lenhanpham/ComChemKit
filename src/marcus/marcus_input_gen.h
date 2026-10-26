/**
 * @file marcus_input_gen.h
 * @brief Gaussian input file generation for Marcus barrier calculations.
 * 
 * Step 1: Geometry optimization + frequency inputs (opt_freq procedures).
 * Step 2: Single point inputs reading step-1 checkpoints via %OldChk + Geom=Check.
 */

#ifndef MARCUS_INPUT_GEN_H
#define MARCUS_INPUT_GEN_H

#include "marcus/marcus_types.h"
#include <string>
#include <vector>

namespace Marcus {

/**
 * @brief Generate step-1 geometry optimization Gaussian inputs.
 * 
 * Creates 4 .gau files in <project>_step_1_opt/:
 *   <donor>-<init>.gau, <donor>-<final>.gau,
 *   <acceptor>-<init>.gau, <acceptor>-<final>.gau
 * 
 * Each uses opt_freq procedures with step1_* theory parameters.
 * Writes a manifest recording checkpoint filenames.
 * 
 * @param project Project configuration (must have step1 theory params set).
 * @return true on success, false on failure.
 */
bool generate_step1_inputs(const MarcusProject& project);

/**
 * @brief Generate step-2 single point Gaussian inputs.
 *
 * Creates 8 .gau files in <project>_step_2_sp/ (4 per molecule):
 *   2 non-cross (equilibrium): <mol>-<state>.gau
 *     %OldChk=<abs_step1_dir>/<own>.chk (absolute path into step-1 dir)
 *     route: #P ... scrf=(...,NonEq=write) Geom(Check) Guess(read) nosymm
 *   2 cross (non-equilibrium): <final>-from-<init>.gau / <init>-from-<final>.gau
 *     %Oldchk=<local equilibrium .chk> (current directory: run the two
 *     non-cross jobs first, then the cross jobs read their .chk files)
 *     route: # ... scrf=(...,ExternalIteration,NonEq=read) Geom=Check Guess(read) nosymm
 *
 * Each uses sp procedures with step2_* theory parameters.
 *
 * Reads step-1 manifest to locate checkpoint files.
 * Writes a manifest recording all single-point input paths.
 *
 * @param project Project configuration (must have step2 theory params set).
 * @return true on success, false on failure.
 */
bool generate_step2_inputs(const MarcusProject& project);

/**
 * @brief Generate the route section for a step-1 (opt+freq) Gaussian input.
 * @param project Project configuration.
 * @param state The MarcusState (for charge/mult, not used in route directly).
 * @return Route section string (without leading #).
 */
std::string build_step1_route(const MarcusProject& project, const MarcusState& state);

/**
 * @brief Generate the route section for a step-2 (single point) Gaussian input.
 * @param project Project configuration.
 * @param is_equilibrium true for non-cross (equilibrium) jobs:
 *        "#P ... NonEq=write Geom(Check) Guess(read) nosymm";
 *        false for cross (non-equilibrium) jobs:
 *        "# ... ExternalIteration,NonEq=read Geom=Check Guess(read) nosymm".
 * @return Route section string (with leading #P / #).
 */
std::string build_step2_route(const MarcusProject& project, bool is_equilibrium);

/**
 * @brief Generate Link0 computing resource directives.
 * @param mem Memory (e.g. "20GB"), empty to omit.
 * @param nprocshared NProcShared value, empty to omit.
 * @param cpu CPU value, empty to omit.
 * @param gpucpu GPUCPU value, empty to omit.
 * @param print_level Print level ("", "P", "T"), empty for default "#".
 * @return Link0 section string.
 */
std::string build_link0_section(const std::string& mem,
                                const std::string& nprocshared,
                                const std::string& cpu,
                                const std::string& gpucpu,
                                const std::string& print_level);

/**
 * @brief Generate the SCRF keyword for step 1 (NonEq=write).
 * @param project Project configuration.
 * @return SCRF route fragment.
 */
std::string build_step1_scrf(const MarcusProject& project);

/**
 * @brief Generate the SCRF keyword for a step-2 non-cross (equilibrium) job.
 * Always uses NonEq=write so the solvent reaction field is written.
 * @param project Project configuration.
 * @return SCRF route fragment.
 */
std::string build_step2_scrf_equilibrium(const MarcusProject& project);

/**
 * @brief Generate the SCRF keyword for a step-2 cross (non-equilibrium) job.
 * Uses step2_solvent_extra (default ExternalIteration,NonEq=read).
 * @param project Project configuration.
 * @return SCRF route fragment.
 */
std::string build_step2_scrf(const MarcusProject& project);

/**
 * @brief Read coordinates from an XYZ file (skip first 2 lines).
 * @param xyz_path Path to XYZ file.
 * @return Coordinate block string, or empty on failure.
 */
std::string read_xyz_coordinates(const std::string& xyz_path);

/**
 * @brief Get the state suffix for a MarcusState (e.g., "-doublet", "-singlet").
 * @param state The MarcusState.
 * @return State suffix string based on charge and multiplicity.
 */
std::string state_suffix(const MarcusState& state);

}  // namespace Marcus

#endif  // MARCUS_INPUT_GEN_H