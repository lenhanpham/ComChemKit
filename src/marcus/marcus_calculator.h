/**
 * @file marcus_calculator.h
 * @brief Energy extraction and Marcus barrier calculation.
 * 
 * Implements the high-kj/high-au two-level approach:
 *   G_state = E_step2_electronic + TC_G_step1 + optional solution phase correction
 *   E_step2: SCF Done in gas phase; After PCM preferred in solution.
 *   lambda from step-2 electronic energy differences (four-point method)
 *   Barriers: symmetric (lambda average) and asymmetric expressions.
 */

#ifndef MARCUS_CALCULATOR_H
#define MARCUS_CALCULATOR_H

#include "marcus/marcus_types.h"
#include <string>

namespace Marcus {

/**
 * @brief Extract electronic energy from a step-2 Gaussian log file.
 * 
 * Solution mode: "After PCM corrections, the energy is" -> field 7,
 * falling back to "SCF Done" -> field 5. Gas mode uses SCF Done only.
 * 
 * @param log_path Path to the .log file.
 * @param energy Output energy in Hartree.
 * @param gas_phase Use SCF Done only for gas-phase single points.
 * @return true on success.
 */
bool extract_electronic_energy(const std::string& log_path, double& energy, bool gas_phase = false);

/**
 * @brief Extract electronic energy plus which key produced it.
 * @param log_path Path to the .log file.
 * @param energy Output energy in Hartree.
 * @param source_out "After PCM" when scfEqui won, else "SCF Done".
 * @param gas_phase Use SCF Done only for gas-phase single points.
 * @param pcm_present Optional flag indicating a PCM correction line in the log.
 * @param scrf_present Optional flag indicating SCRF text in the log.
 * @return true on success.
 */
bool extract_electronic_energy_src(const std::string& log_path,
                                   double& energy, std::string& source_out,
                                   bool gas_phase = false, bool* pcm_present = nullptr,
                                   bool* scrf_present = nullptr);

/**
 * @brief Extract thermal correction to Gibbs free energy from a step-1 log file.
 * 
 * Pattern: "Thermal correction to Gibbs Free Energy" -> field 7.
 * 
 * @param log_path Path to the .log file.
 * @param tc_gibbs Output thermal correction in Hartree.
 * @return true on success.
 */
bool extract_thermal_correction_gibbs(const std::string& log_path, double& tc_gibbs);

/**
 * @brief Extract the full low-level thermal block from a step-1 log.
 * Reads SCF Done (E_low), ZPE, TC_E, TC_H, TC_G, lowest frequency and log temperature.
 * Mirrors HighLevelEnergyCalculator::extract_low_level_thermal_data.
 * @return true if at least TC_G was found.
 */
bool extract_full_thermal_step1(const std::string& log_path,
                                 double& e_low, double& zpe, double& tc_e,
                                 double& tc_h, double& tc_g,
                                 double& lowest_freq, double& temp_k);

/**
 * @brief Compute the Gibbs free energy for a state (high-kj/high-au).
 * 
 * G_state = E_elec (step 2) + TC_G (step 1)
 * 
 * Two pathways (mirrors high-kj/high-au):
 *   1) Thermo module: when -t or -c specified, recalculate TC_G at user T
 *   2) String-matching: read pre-calculated TC_G from log file
 * 
 * @param state The MarcusState (will be populated with g_state).
 * @param dir_step1 Step 1 directory (for TC_G log).
 * @param dir_step2 Step 2 directory (for electronic energy log).
 * @param use_thermo Whether to use thermo module pathway (-t/-c specified).
 * @param temperature Temperature in Kelvin.
 * @param pressure Pressure in atm.
 * @param concentration_m Concentration in mol/L.
 * @param low_vib_method Low-frequency vibrational treatment.
 * @param ravib Crossover frequency for quasi-RRHO (cm⁻¹).
 * @param scale_zpe ZPE scaling factor.
 * @param scale_heat Heat capacity scaling factor.
 * @param scale_entropy Entropy scaling factor.
 * @param scale_cv CV scaling factor.
 * @param interp_vib Vibrational interpolation parameter (cm⁻¹).
 * @param imag_real Imaginary frequency threshold.
 * @param external_energy External electronic energy (Hartree).
 * @param mass_mode Mass assignment mode.
 * @param prt_level Output verbosity.
 * @param print_vib Print vibration contributions.
 * @param ip_mode 0 for isolated, 1 for periodic.
 * @param point_group Point group label.
 * @param hg_entropy Enable entropy interpolation for Head-Gordon.
 * @param bav_preset Bav preset (qchem|grimme).
 * @param output_otm Output .otm file flag.
 * @param step2_gas Use SCF Done for step-2 electronic energies.
 * @param fully_gas Suppress the solution-phase correction.
 * @return true on success.
 */
bool compute_g_state(MarcusState& state,
                     const std::string& dir_step1,
                     const std::string& dir_step2,
                     bool use_thermo = false,
                     double temperature = 298.15,
                     double pressure = 1.0,
                     double concentration_m = 1.0,
                     const std::string& low_vib_method = "grimme",
                     double ravib = 100.0,
                     double scale_zpe = 1.0,
                     double scale_heat = 1.0,
                     double scale_entropy = 1.0,
                     double scale_cv = 1.0,
                     double interp_vib = 100.0,
                     double imag_real = 0.0,
                     double external_energy = 0.0,
                     int mass_mode = 3,
                     int prt_level = 1,
                     int print_vib = 0,
                     int ip_mode = 0,
                     const std::string& point_group = "",
                     bool hg_entropy = false,
                     const std::string& bav_preset = "",
                     bool output_otm = false,
                     bool step2_gas = false,
                     bool fully_gas = false);

/**
 * @brief Per-state phase correction (Hartree), high-au rule.
 * Returns 0 when no SCRF found in either log; fully gas-phase calculations
 * suppress the correction separately.
 */
double per_state_phase_correction(bool has_scrf, double temp_k, double concentration_m);

/**
 * @brief Compute the reaction free energy Delta G°r.
 * 
 * Delta G°r = [G(P_D) + G(P_A)] - [G(R_D) + G(R_A)]
 * 
 * @param project Project configuration.
 * @param dG_r_hartree Output in Hartree.
 * @param dG_r_kcal Output in kcal/mol.
 * @return true on success.
 */
bool compute_reaction_free_energy(MarcusProject& project,
                                  double& dG_r_hartree, double& dG_r_kcal);

/**
 * @brief Compute reorganization energies lambda_R and lambda_P (kcal/mol).
 * 
 * lambda_P = [E(P_A @ q_R_A) - E(P_A @ q_P_A)] + [E(P_D @ q_R_D) - E(P_D @ q_P_D)]
 * lambda_R = [E(R_A @ q_P_A) - E(R_A @ q_R_A)] + [E(R_D @ q_P_D) - E(R_D @ q_R_D)]
 * 
 * Electronic energies from step-2 logs only (no thermal corrections).
 * 
 * @param project Project configuration.
 * @param dir_step2 Step 2 directory (for log files).
 * @param lambda_R Output lambda_R in kcal/mol.
 * @param lambda_P Output lambda_P in kcal/mol.
 * @return true on success.
 */
bool compute_reorganization_energies(const MarcusProject& project,
                                     const std::string& dir_step2,
                                     double& lambda_R, double& lambda_P);

/**
 * @brief Compute Marcus barriers (symmetric and asymmetric).
 * 
 * Symmetric:  Delta G‡ = (lambda + Delta G°r)^2 / (4 lambda), lambda = (lambdaR + lambdaP)/2
 * Asymmetric: Delta G‡ = lambdaR * [(-lambdaP + sqrt(lambdaR*lambdaP + (lambdaR-lambdaP)*Delta G°r)) / (lambdaR - lambdaP)]^2
 * 
 * @param dG_r_kcal Reaction free energy (kcal/mol).
 * @param lambda_R_kcal Reorganization energy reactant side (kcal/mol).
 * @param lambda_P_kcal Reorganization energy product side (kcal/mol).
 * @param result Output result structure (barrier fields populated).
 * @return true on success.
 */
bool compute_barriers(double dG_r_kcal,
                      double lambda_R_kcal, double lambda_P_kcal,
                      MarcusResult& result);

/**
 * @brief Run the full step-3 barrier calculation.
 * 
 * Reads step-1 logs for TC_G, step-2 logs for electronic energies,
 * computes G_state, Delta G°r, lambda_R/P, and barriers.
 * 
 * @param project Project configuration.
 * @param result Output results.
 * @return true on success.
 */
bool calculate_barrier_step3(MarcusProject& project, MarcusResult& result);

}  // namespace Marcus

#endif  // MARCUS_CALCULATOR_H