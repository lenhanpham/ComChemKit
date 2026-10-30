/**
 * @file marcus_types.h
 * @brief Data structures for Marcus barrier calculations (OSET & EnT, four-point method).
 * 
 * Defines shared data types used across the Marcus input generation,
 * calculation, and I/O modules.
 */

#ifndef MARCUS_TYPES_H
#define MARCUS_TYPES_H

#include <string>
#include <vector>
#include <map>

namespace Marcus {

constexpr double HARTREE_TO_KCAL_MOL = 627.509;
constexpr double HARTREE_TO_KJ_MOL = 2625.5002;
constexpr double HARTREE_TO_EV = 27.211396641308;
constexpr double R_CONSTANT = 8.314462618;          ///< J/(K·mol)
constexpr double PO_CONSTANT = 101325.0;            ///< N/m² (1 atm)
constexpr double KB_CONSTANT = 0.000003166811563;   ///< Hartree/K
constexpr double PHASE_CORR_FACTOR = 0.0003808798033989866;

enum class CalcType {
    OSET,  ///< Outer Sphere Single Electron Transfer
    ENT    ///< Energy Transfer
};

/**
 * @enum Step
 * @brief Workflow step: 1=opt, 2=single point, 3=barrier.
 */
enum class Step {
    STEP_1_OPT,   ///< Geometry optimization input generation
    STEP_2_SP,    ///< Single point input generation
    STEP_3_BARRIER ///< Barrier calculation
};

/**
 * @struct MarcusState
 * @brief Represents one electronic state (initial or final) of one molecule.
 *
 * For OSET: state is defined by charge/multiplicity (e.g., donor neutral singlet,
 * donor anion doublet).
 * For EnT: state is defined by electronic state label (e.g., S0, T1) with
 * charge/multiplicity of that state.
 */
struct MarcusState {
    std::string name;          ///< Molecule name (e.g., "PC", "D1")
    std::string state_label;   ///< State label: "init" or "final" (or S0/T1 for EnT)
    int charge = 0;            ///< Molecular charge
    int mult = 1;              ///< Spin multiplicity
    std::string xyz_path;      ///< Path to XYZ coordinate file (step 1 only)
    std::string chk_file;      ///< Checkpoint filename (e.g., "PC-doublet.chk")
    std::string log_file;      ///< Gaussian output log filename
    // --- Two-level components (high-au style) ---
    double energy_elec = 0.0;  ///< Electronic energy from step 2 (Hartree, alias of e_high)
    double e_low = 0.0;        ///< Low-level SCF energy from step-1 log (Hartree)
    double e_high = 0.0;       ///< High-level electronic energy from step-2 log (Hartree)
    std::string e_high_source; ///< Which key won: "After PCM" (scfEqui) or "SCF Done"
    double zpe = 0.0;          ///< Zero-point correction from step 1 (Hartree)
    double tc_energy = 0.0;    ///< Thermal correction to Energy from step 1 (Hartree)
    double tc_enthalpy = 0.0;  ///< Thermal correction to Enthalpy from step 1 (Hartree)
    double tc_gibbs = 0.0;     ///< Thermal correction to Gibbs from step 1 (Hartree)
    double tc_only = 0.0;      ///< tc_energy - zpe (high-au "TC" column)
    double ts_value = 0.0;     ///< tc_enthalpy - tc_gibbs (high-au "TS" column)
    double h_state = 0.0;      ///< Enthalpy: E_high + TC_H (Hartree)
    double g_raw = 0.0;        ///< Gibbs before phase: E_high + TC_G (Hartree)
    double g_state = 0.0;      ///< Final Gibbs per state incl. phase (== g_corr, Hartree)
    double g_corr = 0.0;       ///< Gibbs with phase correction (Hartree)
    double phase_corr = 0.0;   ///< Per-state phase correction applied (Hartree)
    bool has_scrf = false;     ///< Whether SCRF detected in step-1/step-2 logs
    double lowest_freq = 0.0;  ///< Lowest vibrational frequency from step-1 log (cm^-1)
    double temperature_used = 298.15; ///< T used for thermal corrections (K)
    double phase_temperature_used = 298.15; ///< T used in the phase correction (K)
    double phase_concentration_used = 1.0; ///< Concentration used in phase correction (mol/L)
    bool has_energy = false;   ///< Whether electronic energy was parsed
    bool has_tc_gibbs = false; ///< Whether thermal correction was parsed
    bool has_full_thermal = false; ///< Whether full ZPE/TC_H/TC_E set was parsed
    bool has_e_low = false, has_zpe = false, has_tc_energy = false;
    bool has_tc_enthalpy = false, has_lowest_freq = false;
    bool thermo_recalculated = false; ///< Thermo module provided the corrections
    bool thermo_fallback = false;     ///< Recalculation failed; log values used
    bool temperature_from_log = false; ///< Log temperature used instead of input T
};

/**
 * @struct MarcusMolecule
 * @brief One molecule (donor or acceptor) with its initial and final states.
 */
struct MarcusMolecule {
    std::string name;          ///< Molecule name
    MarcusState init_state;    ///< Initial (reactant) state
    MarcusState final_state;   ///< Final (product) state
};

/**
 * @struct MarcusProject
 * @brief Complete project configuration: directories, molecules, theory params.
 */
struct MarcusProject {
    std::string name;                           ///< Project name
    CalcType calc_type = CalcType::OSET;        ///< OSET or EnT
    Step current_step = Step::STEP_1_OPT;       ///< Current workflow step
    std::vector<int> requested_steps = {1, 2}; ///< Requested steps from `step` key/CLI (default: 1,2)
    bool verbose_report = false;                 ///< Include detailed audit sections
    bool automatic_workflow = false;             ///< Run Gaussian jobs sequentially
    std::string qc_engine;                       ///< Gaussian executable for automatic mode

    // Directories
    std::string dir_step1_opt;                  ///< <project>_step_1/
    std::string dir_step2_sp;                   ///< <project>_step_2/

    // Molecules
    MarcusMolecule donor;
    MarcusMolecule acceptor;

    // Step 1 theory parameters (opt_freq)
    std::string step1_functional = "uwb97xd";
    std::string step1_basis = "def2svpp";
    std::string step1_basis_path = "";
    std::string step1_solvent_model = "smd";
    std::string step1_solvent = "";
    std::string step1_solvent_extra = "";
    int step1_scf_maxcycle = 300;
    int step1_opt_maxcycles = 300;
    std::string step1_opt_options = "";
    std::string step1_scf_options = "";
    bool step1_opt_restart = false;
    bool step1_scf_restart = false;
    std::string step1_mem = "";
    std::string step1_nprocshared = "";
    std::string step1_cpu = "";
    std::string step1_gpucpu = "";
    std::string step1_print_level = "";
    std::string step1_tail = "";
    std::string step1_route_extra_keywords = "";

    // Step 2 theory parameters (sp)
    std::string step2_functional = "uwb97xd";
    std::string step2_basis = "def2tzvp";
    std::string step2_basis_path = "";
    std::string step2_solvent_model = "smd";
    std::string step2_solvent = "";
    std::string step2_solvent_extra = "ExternalIteration,NonEq=read";
    int step2_scf_maxcycle = 500;
    std::string step2_mem = "";
    std::string step2_nprocshared = "";
    std::string step2_cpu = "";
    std::string step2_gpucpu = "";
    std::string step2_print_level = "";
    std::string step2_tail = "";
    std::string step2_route_extra_keywords = "Geom=Check Guess(Read)";

    // Step 3 parameters
    double temperature = 298.15;                ///< K, for thermal corrections & phase correction
    double concentration = 1.0;                 ///< mol/L, for phase correction
    double pressure = 1.0;                      ///< atm, for thermo recalculation
    std::string low_vib_method = "grimme";      ///< harmonic|truhlar|grimme|minenkov|headgordon
    double ravib = 100.0;                       ///< cm⁻¹, crossover for quasi-RRHO
    double scale_zpe = 1.0;                     ///< ZPE scaling factor
    double scale_heat = 1.0;                    ///< Heat capacity scaling factor
    double scale_entropy = 1.0;                 ///< Entropy scaling factor
    double scale_cv = 1.0;                      ///< CV scaling factor
    double interp_vib = 100.0;                  ///< cm⁻¹, vibrational interpolation parameter
    double imag_real = 0.0;                     ///< Imaginary frequency threshold
    double external_energy = 0.0;               ///< External electronic energy (Hartree)
    int mass_mode = 3;                          ///< Mass assignment mode
    int prt_level = 1;                          ///< Output verbosity: 0=minimal, 1=default, 2=verbose, 3=full
    int print_vib = 0;                          ///< Print vibration contributions
    int ip_mode = 0;                            ///< 0 for isolated systems, 1 for periodic solid state
    std::string point_group = "";               ///< Point group label
    bool hg_entropy = false;                    ///< Enable entropy interpolation for Head-Gordon method
    std::string bav_preset = "";                ///< qchem|grimme, Bav preset for free-rotor entropy
    bool output_otm = false;                    ///< Output .otm file flag
    bool use_input_temp = false;                ///< Whether -t was specified
    bool use_input_concentration = false;       ///< Whether -c was specified
    bool use_input_pressure = false;            ///< Whether pressure override was specified
    std::string out_file = "";                  ///< Optional output file path
};

/**
 * @brief Generate a state suffix string from charge/multiplicity.
 * @param state The MarcusState.
 * @return State suffix (e.g., "-doublet", "-singlet").
 */
std::string state_suffix(const MarcusState& state);

/**
 * @struct MarcusElectronicSet
 * @brief All 8 step-2 electronic energies for the four-point method.
 */
struct MarcusElectronicSet {
    // Equilibrium (own geometry)
    double E_R_D_qR_D = 0.0;
    double E_P_D_qP_D = 0.0;
    double E_R_A_qR_A = 0.0;
    double E_P_A_qP_A = 0.0;
    // Cross (other geometry, NonEq read)
    double E_P_D_qR_D = 0.0;
    double E_R_D_qP_D = 0.0;
    double E_P_A_qR_A = 0.0;
    double E_R_A_qP_A = 0.0;
    std::string f_R_D_qR_D, f_P_D_qP_D, f_R_A_qR_A, f_P_A_qP_A;
    std::string f_P_D_qR_D, f_R_D_qP_D, f_P_A_qR_A, f_R_A_qP_A;
    std::string s_R_D_qR_D, s_P_D_qP_D, s_R_A_qR_A, s_P_A_qP_A;
    std::string s_P_D_qR_D, s_R_D_qP_D, s_P_A_qR_A, s_R_A_qP_A;
    std::vector<std::string> pcm_lines_in_gas_logs; ///< Step-2 logs with PCM energy lines in gas mode
    std::vector<std::string> scrf_text_in_gas_logs; ///< Step-2 logs with SCRF text in gas mode
    bool success = false;
};

/**
 * @struct MarcusResult
 * @brief Results of the barrier calculation.
 */
struct MarcusResult {
    double dG_r_hartree = 0.0;      ///< Reaction free energy, phase-corrected (Hartree)
    double dG_r_kcal = 0.0;         ///< Reaction free energy, phase-corrected (kcal/mol)
    double dG_r_hartree_raw = 0.0;  ///< Reaction free energy before phase (Hartree)
    double dG_r_kcal_raw = 0.0;     ///< Reaction free energy before phase (kcal/mol)
    double lambda_R_kcal = 0.0;     ///< Reorganization energy reactant side (kcal/mol)
    double lambda_P_kcal = 0.0;     ///< Reorganization energy product side (kcal/mol)
    double lambda_avg_kcal = 0.0;   ///< Average reorganization energy (kcal/mol)
    double lambda_R_hartree = 0.0;  ///< Reactant-side reorganization (Hartree)
    double lambda_P_hartree = 0.0;  ///< Product-side reorganization (Hartree)
    double barrier_sym_kcal = 0.0;  ///< Symmetric barrier (kcal/mol)
    double barrier_asym_kcal = 0.0; ///< Asymmetric barrier (kcal/mol)
    double phase_correction = 0.0;  ///< Net phase on dG_r (= sum products - sum reactants, Hartree)
    double gibbs_hartree_corrected = 0.0;  ///< == dG_r_hartree (kept for compat)
    bool phase_corr_applied = false;  ///< Whether any per-state phase was applied
    bool gibbs_valid = false;         ///< All Gibbs corrections are available
    bool barrier_sym_valid = false;
    bool barrier_asym_valid = false;
    MarcusElectronicSet elec;       ///< Full electronic audit trail
    bool success = false;           ///< Whether calculation succeeded
    std::string error_message;      ///< Error message if failed
};

}  // namespace Marcus

#endif  // MARCUS_TYPES_H