/**
 * @file thermo.cpp
 * @brief Implementation of thermo interface layer
 * @author Le Nhan Pham
 * @date 2025
 */

#include "thermo/thermo.h"
#include "thermo/chemsys.h"
#include "thermo/util.h"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include "thermo/loadfile.h"
#include "thermo/calc.h"
#include "thermo/atommass.h"
#include "thermo/symmetry.h"
#include "thermo/omp_config.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <map>
#include <stdexcept>

using namespace std;

namespace ThermoInterface 
{
    // Helper: extract basename without extension
    static std::string get_basename_without_extension(const std::string& filepath)
    {
        size_t last_slash = filepath.find_last_of("/\\");
        std::string filename = (last_slash != std::string::npos) ? filepath.substr(last_slash + 1) : filepath;
        size_t last_dot = filename.rfind('.');
        return (last_dot != std::string::npos) ? filename.substr(0, last_dot) : filename;
    }

    ThermoResult process_file(const ThermoSettings& settings, const CommandContext& context) 
    {
        ThermoResult result;
        
        try {
            // Initialize thermo module
            initialize_thermo_module();
            
            // Determine input file
            std::string input_file;
            if (!settings.input_file.empty()) {
                input_file = settings.input_file;
            } else if (!context.files.empty()) {
                input_file = context.files[0];
            } else {
                result.error_message = "No input file specified for thermo analysis";
                return result;
            }
            
            // Check if file exists
            if (!std::filesystem::exists(input_file)) {
                result.error_message = "Input file not found: " + input_file;
                return result;
            }
            
            // Create SystemData from context
            SystemData* sys = static_cast<SystemData*>(create_system_data(settings, context, input_file));
            if (!sys) {
                result.error_message = "Failed to create system data";
                return result;
            }
            
            // Initialize isotope mass table
            atommass::initmass(*sys);
            
            // Load settings unless disabled
            if (!settings.no_settings) {
                util::loadsettings(*sys);
            } else if (sys->prtlevel >= 1) {
                std::cout << "\"-noset\" is set: Setting parameters from settings.ini are ignored\n";
            }

            // Process CLI arguments via util::loadarguments so settings.ini overrides work
            // util::loadarguments expects argv[0] = program name, argv[1] = input file, argv[2...] = options
            std::vector<std::string> args = {"cck thermo"};
            if (!input_file.empty()) {
                args.push_back(input_file);
            } else {
                args.push_back("");
            }
            if (!settings.cli_args.empty()) {
                args.insert(args.end(), settings.cli_args.begin(), settings.cli_args.end());
            }
            util::loadarguments(*sys, static_cast<int>(args.size()), args);

            // --- Apply method-dependent Bav ---
            if (sys->lowVibTreatment == LowVibTreatment::HeadGordon) {
                if (!sys->bavUserOverride) {
                    // HeadGordon defaults to Q-Chem's Bav
                    sys->bavPreset = BavPreset::QChem;
                    sys->Bav       = bavPresetValue(BavPreset::QChem);
                }
            } else {
                // Grimme/Minenkov/Truhlar/Harmonic: always use Grimme's Bav; warn if user tried to override
                if (sys->bavUserOverride && sys->bavPreset != BavPreset::Grimme) {
                    std::cerr << "Warning: -bav option is only applicable to HeadGordon method. "
                              << "Ignoring -bav " << bavPresetName(sys->bavPreset)
                              << "; using grimme (1e-44 kg m^2).\n";
                }
                sys->bavPreset = BavPreset::Grimme;
                sys->Bav       = bavPresetValue(BavPreset::Grimme);
            }

            // --- OpenMP thread detection and configuration ---
            sys->exec.physical_cores_detected = detect_physical_cores();
            sys->exec.scheduler_cpus_detected = detect_scheduler_cpus();
            std::string thread_notification = validate_thread_count(
                sys->exec.omp_threads_requested,
                sys->exec.physical_cores_detected,
                sys->exec.scheduler_cpus_detected,
                sys->exec.omp_threads_actual,
                sys->exec.omp_user_override
            );
            configure_openmp(sys->exec.omp_threads_actual);

            if (sys->prtlevel >= 1 && !thread_notification.empty()) {
                std::cout << "\n" << thread_notification << "\n";
            }

            // If prtlevel=3, auto-enable per-mode vibration output unless user explicitly set prtvib
            if (sys->prtlevel >= 3 && sys->prtvib == 0) {
                sys->prtvib = 1;
            }

            // Print running parameters
            if (sys->prtlevel >= 1) {
                std::cout << "\n                   --- Summary of Current Parameters ---\n\nRunning parameters:\n";
                std::cout << " Print level: " << sys->prtlevel << " (0=minimal, 1=default, 2=verbose, 3=full)\n";
                if (sys->prtvib == 1) {
                    std::cout << "Printing individual contribution of vibration modes: Yes\n";
                } else if (sys->prtvib == -1) {
                    std::cout << "Printing individual contribution of vibration modes: Yes, to <basename>.vibcon file\n";
                } else {
                    std::cout << "Printing individual contribution of vibration modes: No\n";
                }
                if (sys->Tstep == 0.0) {
                    std::cout << " Temperature:     " << std::fixed << std::setprecision(3) << std::setw(12) << sys->T << " K\n";
                } else {
                    std::cout << " Temperature scan, from " << std::fixed << std::setprecision(3) << std::setw(10) << sys->Tlow
                              << " to " << std::setw(10) << sys->Thigh << ", step: " << std::setw(8) << sys->Tstep << " K\n";
                }
                if (sys->Pstep == 0.0) {
                    std::cout << " Pressure:      " << std::fixed << std::setprecision(3) << std::setw(12) << sys->P << " atm\n";
                } else {
                    std::cout << " Pressure scan, from " << std::fixed << std::setprecision(3) << std::setw(10) << sys->Plow
                              << " to " << std::setw(10) << sys->Phigh << ", step: " << std::setw(8) << sys->Pstep << " atm\n";
                }
                if (sys->concstr != "0") {
                    std::cout << " Concentration: " << std::fixed << std::setprecision(3) << std::setw(12)
                              << std::stod(sys->concstr) << " mol/L\n";
                }
                std::cout << " Scaling factor of vibrational frequencies for ZPE:       " << std::fixed << std::setprecision(4)
                          << std::setw(8) << sys->sclZPE << "\n"
                          << " Scaling factor of vibrational frequencies for U(T)-U(0): " << std::setw(8) << sys->sclheat << "\n"
                          << " Scaling factor of vibrational frequencies for S(T):      " << std::setw(8) << sys->sclS << "\n"
                          << " Scaling factor of vibrational frequencies for CV:        " << std::setw(8) << sys->sclCV << "\n";
                if (sys->lowVibTreatment == LowVibTreatment::Harmonic) {
                    std::cout << "Low frequencies treatment: Harmonic approximation\n";
                } else if (sys->lowVibTreatment == LowVibTreatment::Truhlar) {
                    std::cout << " Low frequencies treatment: Raising low frequencies (Truhlar's treatment)\n"
                              << " Lower frequencies will be raised to " << std::fixed << std::setprecision(2) << sys->ravib
                              << " cm^-1 during calculating S, U(T)-U(0), CV and q\n";
                } else if (sys->lowVibTreatment == LowVibTreatment::Grimme) {
                    std::cout << " Low frequencies treatment: Grimme's interpolation for entropy\n";
                } else if (sys->lowVibTreatment == LowVibTreatment::Minenkov) {
                    std::cout << " Low frequencies treatment: Minenkov's interpolation for entropy and internal energy\n";
                } else if (sys->lowVibTreatment == LowVibTreatment::HeadGordon) {
                    std::cout << " Low frequencies treatment: Head-Gordon's interpolation for energy";
                    if (sys->hgEntropy)
                        std::cout << " and entropy";
                    std::cout << "\n";
                }
                if (sys->lowVibTreatment == LowVibTreatment::Grimme
                    || sys->lowVibTreatment == LowVibTreatment::Minenkov
                    || sys->lowVibTreatment == LowVibTreatment::HeadGordon) {
                    std::cout << " Vibrational frequency threshold used in the interpolation is " << std::fixed
                              << std::setprecision(2) << sys->intpvib << " cm^-1\n";
                }
                if (sys->lowVibTreatment == LowVibTreatment::HeadGordon) {
                    std::cout << " Average moment of inertia (Bav): " << bavPresetName(sys->bavPreset)
                              << " (" << std::scientific << std::setprecision(2) << sys->Bav << " kg m^2)\n"
                              << std::fixed;
                }
                if (sys->imagreal != 0.0) {
                    std::cout << " Imaginary frequencies with norm < " << std::fixed << std::setprecision(2) << sys->imagreal
                              << " cm^-1 will be treated as real frequencies\n";
                }
            } // end prtlevel >= 1 summary block

            // Print start processing message
            if (sys->prtlevel >= 1) {
                auto start_now = std::chrono::system_clock::now();
                std::time_t start_now_time = std::chrono::system_clock::to_time_t(start_now);
                std::cout << "                      -------- End of Summary --------\n\n";
                std::cout << "OpenThermo started to process " << input_file << " at "
                          << std::ctime(&start_now_time);
            }
            
            // Check if input file is a list file (.list or .txt)
            bool is_list_file = (input_file.find(".list") != std::string::npos) ||
                               (input_file.find(".txt") != std::string::npos);
            
            if (is_list_file) {
                std::cout << "Processing list file...\n";
                std::vector<std::string> filelist;
                std::ifstream listfile(input_file);
                if (!listfile.is_open()) {
                    result.error_message = "Unable to open list file: " + input_file;
                    delete sys;
                    return result;
                }
                std::string line;
                while (std::getline(listfile, line)) {
                    line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](unsigned char ch) {
                        return !std::isspace(ch);
                    }));
                    line.erase(std::find_if(line.rbegin(), line.rend(), [](unsigned char ch) {
                        return !std::isspace(ch);
                    }).base(), line.end());
                    if (!line.empty()) {
                        filelist.push_back(line);
                    }
                }
                listfile.close();
                if (filelist.empty()) {
                    result.error_message = "List file is empty or contains no valid file paths";
                    delete sys;
                    return result;
                }
                size_t nfile = filelist.size();
                std::vector<double> Elist(nfile), Ulist(nfile), Hlist(nfile), Glist(nfile), 
                                   Slist(nfile), CVlist(nfile), CPlist(nfile), QVlist(nfile), Qbotlist(nfile);
                calc::ensemble(*sys, filelist, Elist, Ulist, Hlist, Glist, Slist, CVlist, CPlist, QVlist, Qbotlist);
                result.success = true;
                delete sys;
                cleanup_thermo_module();
                return result;
            }
            
            // Process input file: OTM or quantum chemistry output
            if (input_file.find(".otm") != std::string::npos) {
                if (sys->prtlevel >= 2) {
                    std::cout << "\n Processing data from " << input_file << "\n";
                    std::cout << " Atomic masses used: Read from OTM file\n";
                }
                LoadFile::loadotm(*sys);
            } else {
                util::QuantumChemistryProgram prog = util::deterprog(*sys);
                sys->isys = static_cast<int>(prog);

                if (prog != util::QuantumChemistryProgram::Unknown) {
                    if (sys->prtlevel >= 2) {
                        std::cout << "\n";
                        if (sys->massmod == 1)
                            std::cout << " Atomic masses used: Element\n";
                        if (sys->massmod == 2)
                            std::cout << " Atomic masses used: Most abundant isotope\n";
                        if (sys->massmod == 3)
                            std::cout << " Atomic masses used: Read from quantum chemical output\n";
                    }

                    try {
                        if (prog == util::QuantumChemistryProgram::Gaussian) {
                            if (sys->prtlevel >= 2) std::cout << "Processing Gaussian output file...\n";
                            LoadFile::loadgau(*sys);
                        } else if (prog == util::QuantumChemistryProgram::Orca) {
                            if (sys->prtlevel >= 2) std::cout << "Processing ORCA output file...\n";
                            LoadFile::loadorca(*sys);
                        } else if (prog == util::QuantumChemistryProgram::Gamess) {
                            if (sys->prtlevel >= 2) std::cout << "Processing GAMESS-US output file...\n";
                            LoadFile::loadgms(*sys);
                        } else if (prog == util::QuantumChemistryProgram::Nwchem) {
                            if (sys->prtlevel >= 2) std::cout << "Processing NWChem output file...\n";
                            LoadFile::loadnw(*sys);
                        } else if (prog == util::QuantumChemistryProgram::Cp2k) {
                            if (sys->prtlevel >= 2) std::cout << "Processing CP2K output file...\n";
                            LoadFile::loadCP2K(*sys);
                            if (sys->ipmode == 0) {
                                std::cout << " Note: If your system is not isolated (periodic crystals, slabs or adsorbate on "
                                             "surface), \nyou may want to set"
                                             "\"ipmode\" = 1 settings.ini in order to ignore translation and rotation "
                                             "contributions. \n"
                                          << "This is typical for condensed materials calculations with CP2K and VASP \n\n";
                            }
                        } else if (prog == util::QuantumChemistryProgram::Vasp) {
                            if (sys->prtlevel >= 2) std::cout << "Processing VASP output file...\n";
                            LoadFile::loadvasp(*sys);
                        } else if (prog == util::QuantumChemistryProgram::Xtb) {
                            if (sys->prtlevel >= 2) std::cout << "Processing xtb g98.out file...\n";
                            LoadFile::loadxtb(*sys);
                        } else if (prog == util::QuantumChemistryProgram::QChem) {
                            if (sys->prtlevel >= 2) std::cout << "Processing Q-Chem output file...\n";
                            LoadFile::loadqchem(*sys);
                        } else {
                            result.error_message = "Unknown program type";
                            delete sys;
                            return result;
                        }
                    } catch (const std::exception& e) {
                        result.error_message = "Failed to load data from input file: " + std::string(e.what());
                        delete sys;
                        return result;
                    }

                    util::modmass(*sys);
                    sys->nelevel = 1;
                    sys->elevel  = {0.0};
                    sys->edegen  = {sys->spinmult};
                    if (!sys->edegen.empty() && sys->edegen[0] <= 0) {
                        sys->edegen[0] = 1;
                    }
                    if (sys->outotm == 1) {
                        util::outotmfile(*sys);
                    }
                } else {
                    // Unknown program — not a batch file either
                    std::cerr << "Error: Unable to identify the quantum chemical program that generated this file.\n";
                    std::cerr << "Supported programs: Gaussian, ORCA, GAMESS-US, NWChem, CP2K, VASP, Q-Chem, xTB, and "
                                 "OpenThermo (.otm)\n";
                    std::cerr << "For batch processing, use a list file with .list or .txt extension containing file paths.\n";
                    result.error_message = "Unknown file format";
                    delete sys;
                    return result;
                }
            }

            // Handle external energy override
            if (sys->Eexter != 0.0) {
                sys->E = sys->Eexter;
                if (sys->prtlevel >= 1)
                    std::cout << "Note: The electronic energy specified by \"E\" parameter will be used\n";
            } else if (sys->E != 0.0) {
                if (sys->prtlevel >= 2)
                    std::cout << "Note: The electronic energy extracted from input file will be used\n";
            }

            // Handle imaginary frequency treatment
            if (sys->imagreal != 0.0) {
                for (int ifreq = 0; ifreq < sys->nfreq; ++ifreq) {
                    if (sys->wavenum[ifreq] < 0 && std::abs(sys->wavenum[ifreq]) < sys->imagreal) {
                        sys->wavenum[ifreq] = std::abs(sys->wavenum[ifreq]);
                        std::cout << " Note: Imaginary frequency " << std::fixed << std::setprecision(2)
                                  << sys->wavenum[ifreq] << " cm^-1 has been set to real frequency!\n";
                    }
                }
            }

            // Calculate total mass
            sys->totmass = 0.0;
            for (const auto& atom : sys->a) {
                sys->totmass += atom.mass;
            }

            // Calculate inertia and detect linearity
            calc::calcinertia(*sys);
            sys->ilinear = 0;
            for (double in : sys->inert) {
                if (in < 0.001) {
                    sys->ilinear = 1;
                    break;
                }
            }

            // Update atom count
            sys->ncenter = static_cast<int>(sys->a.size());

            // Symmetry detection
            if (sys->prtlevel >= 2)
                std::cout << "Number of atoms loaded: " << sys->a.size() << "\n";
            if (sys->a.empty()) {
                std::cerr << "Error: No atoms loaded from input file!\n";
                result.error_message = "No atoms loaded from input file!";
                delete sys;
                return result;
            }

            symmetry::SymmetryDetector symDetector;
            symDetector.PGnameinit = sys->PGnameinit;  // Use PGnameinit for detection
            symDetector.ncenter    = sys->a.size();
            symDetector.a          = sys->a;
            symDetector.a_index.resize(sys->a.size());
            for (size_t i = 0; i < sys->a.size(); ++i) {
                symDetector.a_index[i] = i;
            }
            symDetector.detectPG((sys->prtlevel >= 2) ? 1 : 0);
            sys->rotsym = symDetector.rotsym;
            sys->PGname = symDetector.PGname;

            // Convert wavenumbers to frequencies
            sys->freq.resize(sys->nfreq);
            for (int i = 0; i < sys->nfreq; ++i) {
                sys->freq[i] = sys->wavenum[i] * wave2freq;
            }

            // Count imaginary frequencies
            int nimag = 0;
            for (double f : sys->freq) {
                if (f < 0) ++nimag;
            }
            if (nimag > 0) {
                std::cout << " Note: There are " << nimag
                          << " imaginary frequencies, they will be ignored in the calculation\n";
            }

            // Print molecular information (tiered by prtlevel)
            if (sys->prtlevel >= 1) {
                std::cout << "\n"
                          << "                      -------- Chemical System Data -------\n"
                          << "                      -------------------------------------\n"
                          << " Electronic energy: " << std::fixed << std::setprecision(8) << std::setw(18) << sys->E
                          << " a.u.\n";
                if (sys->spinmult != 0) {
                    std::cout << " Spin multiplicity: " << std::setw(3) << sys->spinmult << "\n";
                } else {
                    for (int ie = 0; ie < sys->nelevel; ++ie) {
                        std::cout << " Electronic energy level " << ie + 1 << "     E = " << std::fixed
                                  << std::setprecision(6) << std::setw(12) << sys->elevel[ie]
                                  << " eV     Degeneracy = " << std::setw(3) << sys->edegen[ie] << "\n";
                    }
                }
            }

            if (sys->prtlevel >= 2) {
                // Level 2+: full per-atom listing
                for (int iatm = 0; iatm < sys->ncenter; ++iatm) {
                    std::cout << " Atom " << std::setw(5) << iatm + 1 << " (" << ind2name[sys->a[iatm].index]
                              << ")   Mass: " << std::fixed << std::setprecision(6) << std::setw(12) << sys->a[iatm].mass
                              << " amu\n";
                }
                std::cout << " Total mass: " << std::fixed << std::setprecision(6) << std::setw(16) << sys->totmass
                          << " amu\n\n";
            } else if (sys->prtlevel == 1) {
                // Level 1: compact element-count summary
                std::map<int, int> elem_count;
                for (int iatm = 0; iatm < sys->ncenter; ++iatm) {
                    elem_count[sys->a[iatm].index]++;
                }
                std::cout << " Atoms: " << sys->ncenter << " (";
                bool first = true;
                for (const auto& [idx, count] : elem_count) {
                    if (!first) std::cout << ", ";
                    std::cout << count << " " << ind2name[idx];
                    first = false;
                }
                std::cout << ")  Total mass: " << std::fixed << std::setprecision(6) << sys->totmass << " amu\n";
            }

            if (sys->prtlevel >= 1) {
                std::cout << " Point group: " << sys->PGname;
                if (sys->ipmode == 0)
                    std::cout << "   Rotational symmetry number: " << std::setw(3) << sys->rotsym;
                std::cout << "\n";
            }

            if (sys->prtlevel >= 2 && sys->ipmode == 0) {
                std::array<double, 3> sorted_inert = sys->inert;
                std::sort(sorted_inert.begin(), sorted_inert.end());

                std::cout << " Principal moments of inertia (amu*Bohr^2):\n";
                for (double inval : sorted_inert) {
                    std::cout << std::fixed << std::setprecision(6) << std::setw(16) << inval << "\n";
                }

                double inert_sum = sorted_inert[0] + sorted_inert[1] + sorted_inert[2];
                if (inert_sum < 1e-10) {
                    std::cout << "This is a single atom system, rotational constant is zero\n";
                } else if (sys->ilinear == 1) {
                    double largest_inert = sorted_inert[2];
                    double rotcst1 = h / (8.0 * pi * pi * largest_inert * amu2kg * (b2a * 1e-10) * (b2a * 1e-10));
                    std::cout << " Rotational constant (GHz): " << std::fixed << std::setprecision(6) << std::setw(14)
                              << rotcst1 / 1e9 << "\n"
                              << " Rotational temperature (K): " << std::fixed << std::setprecision(6) << std::setw(12)
                              << rotcst1 * h / kb << "\n"
                              << "This is a linear molecule\n";
                } else {
                    std::array<double, 3> rotcst;
                    for (int i = 0; i < 3; ++i) {
                        rotcst[i] = h / (8.0 * pi * pi * sys->inert[i] * amu2kg * (b2a * 1e-10) * (b2a * 1e-10));
                    }
                    std::cout << " Rotational constants relative to principal axes (GHz):\n";
                    for (double r : rotcst) {
                        std::cout << std::fixed << std::setprecision(6) << std::setw(14) << r / 1e9 << "\n";
                    }
                    std::cout << " Rotational temperatures (K):";
                    for (double r : rotcst) {
                        std::cout << std::fixed << std::setprecision(6) << std::setw(12) << r * h / kb;
                    }
                    std::cout << "\nThis is not a linear molecule\n";
                }
            } else if (sys->prtlevel >= 2 && sys->ipmode == 1) {
                std::cout << "Rotation information is not shown here since ipmode=1\n";
            }

            if (sys->nfreq > 0) {
                if (sys->prtlevel >= 2) {
                    std::cout << "\n There are " << sys->nfreq << " frequencies (cm^-1):\n";
                    for (int ifreq = 0; ifreq < sys->nfreq; ++ifreq) {
                        std::cout << std::fixed << std::setprecision(1) << std::setw(8) << sys->wavenum[ifreq];
                        if ((ifreq + 1) % 9 == 0 || ifreq == sys->nfreq - 1)
                            std::cout << "\n";
                    }
                } else if (sys->prtlevel == 1) {
                    double wmin = sys->wavenum[0], wmax = sys->wavenum[0];
                    for (int ifreq = 1; ifreq < sys->nfreq; ++ifreq) {
                        if (sys->wavenum[ifreq] < wmin) wmin = sys->wavenum[ifreq];
                        if (sys->wavenum[ifreq] > wmax) wmax = sys->wavenum[ifreq];
                    }
                    std::cout << " Frequencies: " << sys->nfreq << " (range: " << std::fixed << std::setprecision(1)
                              << wmin << " -- " << wmax << " cm^-1)\n";
                }
            }

            if (nimag > 0) {
                std::cout << " Note: There are " << nimag
                          << " imaginary frequencies, they will be ignored in the calculation\n";
            }

            // Check for scanning mode
            bool is_scanning = (sys->Tstep != 0.0) || (sys->Pstep != 0.0);

            if (!is_scanning) {
                // Single T/P point
                sys->exec.omp_strategy = static_cast<int>(
                    select_strategy(1, sys->nfreq, sys->exec.omp_threads_actual));
                if (sys->prtlevel >= 2) {
                    std::cout << strategy_description(
                        static_cast<OMPStrategy>(sys->exec.omp_strategy), 1, sys->nfreq) << "\n";
                }
                calc::showthermo(*sys);
            } else {
                // Temperature / pressure scanning
                std::cout << "\nPerforming scan of temperature/pressure...\n";
                double P1 = sys->P, P2 = sys->P, Ps = 1.0;
                double T1 = sys->T, T2 = sys->T, Ts = 1.0;
                if (sys->Tstep != 0.0) {
                    T1 = sys->Tlow;  T2 = sys->Thigh;  Ts = sys->Tstep;
                }
                if (sys->Pstep != 0.0) {
                    P1 = sys->Plow;  P2 = sys->Phigh;  Ps = sys->Pstep;
                }

                std::string basename     = get_basename_without_extension(input_file);
                std::string uhg_filename = basename + ".UHG";
                std::string scq_filename = basename + ".SCq";

                std::ofstream file_UHG(uhg_filename, std::ios::out);
                if (!file_UHG.is_open()) {
                    result.error_message = "Failed to create output file: " + uhg_filename;
                    delete sys;
                    return result;
                }
                file_UHG << "Ucorr, Hcorr and Gcorr are in kcal/mol; U, H and G are in a.u.\n\n"
                         << "     T(K)      P(atm)  Ucorr     Hcorr     Gcorr            U                H                G\n";

                std::ofstream file_SCq(scq_filename, std::ios::out);
                if (!file_SCq.is_open()) {
                    file_UHG.close();
                    result.error_message = "Failed to create output file: " + scq_filename;
                    delete sys;
                    return result;
                }
                file_SCq << "S, CV and CP are in cal/mol/K; q(V=0)/NA and q(bot)/NA are unitless\n\n"
                         << "    T(K)       P(atm)    S         CV        CP        q(V=0)/NA      q(bot)/NA\n";

                if (Ts > 0 && Ps > 0) {
                    const int num_step_T  = static_cast<int>((T2 - T1) / Ts) + 1;
                    const int num_step_P  = static_cast<int>((P2 - P1) / Ps) + 1;
                    const int total_points = num_step_T * num_step_P;

                    // Auto-select parallelization strategy
                    OMPStrategy strategy = select_strategy(total_points, sys->nfreq, sys->exec.omp_threads_actual);
                    sys->exec.omp_strategy = static_cast<int>(strategy);
                    if (sys->prtlevel >= 2) {
                        std::cout << strategy_description(strategy, total_points, sys->nfreq) << "\n";
                    }

                    struct ScanResult {
                        double T, P;
                        double corrU, corrH, corrG, S, CV, CP, QV, Qbot;
                    };
                    std::vector<ScanResult> scan_results(total_points);

                    if (strategy == OMPStrategy::Outer) {
                        // Outer: parallelize T/P scan
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
                        for (int idx = 0; idx < total_points; ++idx) {
                            int i = idx / num_step_P;
                            int j = idx % num_step_P;
                            auto& r = scan_results[idx];
                            r.T = T1 + i * Ts;
                            r.P = P1 + j * Ps;
                            calc::calcthermo(*sys, r.T, r.P, r.corrU, r.corrH, r.corrG,
                                             r.S, r.CV, r.CP, r.QV, r.Qbot);
                        }
                    } else {
                        // Inner: serial T/P, vibrational loop parallelized in calc.cpp
                        for (int idx = 0; idx < total_points; ++idx) {
                            int i = idx / num_step_P;
                            int j = idx % num_step_P;
                            auto& r = scan_results[idx];
                            r.T = T1 + i * Ts;
                            r.P = P1 + j * Ps;
                            calc::calcthermo(*sys, r.T, r.P, r.corrU, r.corrH, r.corrG,
                                             r.S, r.CV, r.CP, r.QV, r.Qbot);
                        }
                    }

                    // Write results sequentially
                    for (int idx = 0; idx < total_points; ++idx) {
                        const auto& r = scan_results[idx];
                        file_UHG << std::fixed << std::setprecision(3) << std::setw(10) << r.T << std::setw(10) << r.P
                                 << std::setprecision(3) << std::setw(10) << r.corrU / cal2J << std::setw(10)
                                 << r.corrH / cal2J << std::setw(10) << r.corrG / cal2J << std::setprecision(6)
                                 << std::setw(17) << r.corrU / au2kJ_mol + sys->E << std::setw(17)
                                 << r.corrH / au2kJ_mol + sys->E << std::setw(17) << r.corrG / au2kJ_mol + sys->E << "\n";
                        file_SCq << std::fixed << std::setprecision(3) << std::setw(10) << r.T << std::setw(10) << r.P
                                 << std::setprecision(3) << std::setw(10) << r.S / cal2J << std::setw(10)
                                 << r.CV / cal2J << std::setw(10) << r.CP / cal2J << std::scientific
                                 << std::setprecision(6) << std::setw(16) << r.QV / NA << std::setw(16) << r.Qbot / NA
                                 << "\n";
                    }
                }

                file_UHG.close();
                file_SCq.close();

                std::cout << "\n Congratulation! Thermochemical properties at various temperatures/pressures were calculated\n"
                          << " All data were exported to " << uhg_filename << " and " << scq_filename << "\n"
                          << " " << uhg_filename << " contains thermal correction to U, H and G, and sum of electronic energy and corresponding corrections\n"
                          << " " << scq_filename << " contains S, CV, CP, q(V=0) and q(bot)\n";

                result.output_files.push_back(uhg_filename);
                result.output_files.push_back(scq_filename);
            }
            
            // Generate .otm file if requested
            if (sys->outotm && input_file.find(".otm") == std::string::npos) {
                util::outotmfile(*sys);
                std::string otm_filename = get_basename_without_extension(input_file) + ".otm";
                result.output_files.push_back(otm_filename);
            }
            
            result.success  = true;
            result.exit_code = 0;
            
            // Print completion message
            if (sys->prtlevel >= 1) {
                auto now = std::chrono::system_clock::now();
                std::time_t now_time = std::chrono::system_clock::to_time_t(now);
                std::cout << "\nCalculation completed at: " << std::ctime(&now_time);
                std::cout << "\n"
                          << "                    ---------- Happy calculation ----------" << "\n"
                          << "                    ---- OpenThermo normally terminated ---" << "\n";
            }
            
            delete sys;
            cleanup_thermo_module();
            
        } catch (const std::exception& e) {
            result.error_message = "Exception in thermo processing: " + std::string(e.what());
            result.success = false;
        }
        
        return result;
    }
    
    ThermoResult process_batch(const ThermoSettings& settings, const CommandContext& context,
                              const std::vector<std::string>& files)
    {
        ThermoResult result;
        result.success = true;

        if (files.empty()) {
            result.success = false;
            result.error_message = "No files specified for thermo analysis";
            return result;
        }

        for (const auto& file : files) {
            ThermoSettings file_settings = settings;
            file_settings.input_file = file;
            
            ThermoResult file_result = process_file(file_settings, context);
            
            if (!file_result.success) {
                result.success = false;
                result.error_message += "File " + file + ": " + file_result.error_message + "\n";
            } else {
                result.output_files.insert(result.output_files.end(), 
                                         file_result.output_files.begin(), 
                                         file_result.output_files.end());
            }
        }
        
        return result;
    }
    
    std::string identify_program(const std::string& file)
    {
        SystemData sys;
        sys.inputfile = file;
        util::QuantumChemistryProgram prog = util::deterprog(sys);
        
        switch(prog) {
            case util::QuantumChemistryProgram::Gaussian: return "Gaussian";
            case util::QuantumChemistryProgram::Orca: return "ORCA";
            case util::QuantumChemistryProgram::Gamess: return "GAMESS-US";
            case util::QuantumChemistryProgram::Nwchem: return "NWChem";
            case util::QuantumChemistryProgram::Cp2k: return "CP2K";
            case util::QuantumChemistryProgram::Vasp: return "VASP";
            case util::QuantumChemistryProgram::Xtb: return "xTB";
            case util::QuantumChemistryProgram::QChem: return "Q-Chem";
            default: return "Unknown";
        }
    }

    bool extract_basic_properties(const std::string& file, double T, double P, 
                                  double& scf_au, double& corrG_au, double& corrH_au, double& zpe_au, double& lf_cm, int& nfreq, std::string& prog_name)
    {
        SystemData* sys = new SystemData();
        sys->inputfile = file;
        sys->T = T;
        sys->P = P;
        sys->prtlevel = 0; // Quiet parsing
        
        util::QuantumChemistryProgram prog = util::deterprog(*sys);
        sys->isys = static_cast<int>(prog);
        
        switch(prog) {
            case util::QuantumChemistryProgram::Gaussian: prog_name = "Gaussian"; break;
            case util::QuantumChemistryProgram::Orca: prog_name = "ORCA"; break;
            case util::QuantumChemistryProgram::Gamess: prog_name = "GAMESS-US"; break;
            case util::QuantumChemistryProgram::Nwchem: prog_name = "NWChem"; break;
            case util::QuantumChemistryProgram::Cp2k: prog_name = "CP2K"; break;
            case util::QuantumChemistryProgram::Vasp: prog_name = "VASP"; break;
            case util::QuantumChemistryProgram::Xtb: prog_name = "xTB"; break;
            case util::QuantumChemistryProgram::QChem: prog_name = "Q-Chem"; break;
            default: prog_name = "Unknown"; break;
        }

        if (prog == util::QuantumChemistryProgram::Unknown) {
            delete sys;
            return false;
        }

        bool success = false;
        try {
            if (prog == util::QuantumChemistryProgram::Gaussian) {
                LoadFile::loadgau(*sys);
            } else if (prog == util::QuantumChemistryProgram::Orca) {
                LoadFile::loadorca(*sys);
            } else if (prog == util::QuantumChemistryProgram::Gamess) {
                LoadFile::loadgms(*sys);
            } else if (prog == util::QuantumChemistryProgram::Nwchem) {
                LoadFile::loadnw(*sys);
            } else if (prog == util::QuantumChemistryProgram::Cp2k) {
                LoadFile::loadCP2K(*sys);
            } else if (prog == util::QuantumChemistryProgram::Vasp) {
                LoadFile::loadvasp(*sys);
            } else if (prog == util::QuantumChemistryProgram::QChem) {
                LoadFile::loadqchem(*sys);
            }
            success = true;
        } catch (...) {
            // Full load failed.  For ORCA this typically means the file has no
            // frequency data (energy-only run).  Attempt a lighter load that
            // just retrieves the SCF energy and geometry so the caller still
            // gets a useful result with nfreq = 0.
            if (prog == util::QuantumChemistryProgram::Orca) {
                try {
                    sys->nfreq = 0;
                    sys->wavenum.clear();
                    sys->E = 0.0;

                    // Scan the file line by line for "FINAL SINGLE POINT ENERGY" and
                    // keep the last occurrence (same logic as loadorca internally).
                    // This avoids calling any private LoadFile members.
                    const std::string energy_label = "FINAL SINGLE POINT ENERGY";
                    std::ifstream ef(sys->inputfile);
                    if (ef.is_open()) {
                        std::string eline;
                        std::string last_energy_line;
                        while (std::getline(ef, eline)) {
                            if (eline.find(energy_label) != std::string::npos) {
                                last_energy_line = eline;
                            }
                        }
                        ef.close();
                        if (!last_energy_line.empty()) {
                            // Energy value is the last whitespace-separated token on the line
                            std::istringstream eiss(last_energy_line);
                            std::string etok;
                            while (eiss >> etok) { /* consume all tokens */ }
                            try { sys->E = std::stod(etok); } catch (...) {}
                        }
                        success = (sys->E != 0.0);
                    }
                } catch (...) {
                    success = false;
                }
            }
            // For non-ORCA programs a full-load failure is a real error.
        }

        if (success) {
            scf_au = sys->E;
            nfreq  = sys->nfreq;

            // find lowest frequency
            if (nfreq > 0 && !sys->wavenum.empty()) {
                lf_cm = sys->wavenum[0];
                for (int i = 1; i < nfreq; ++i) {
                    if (sys->wavenum[i] < lf_cm && sys->wavenum[i] >= 0) {
                        lf_cm = sys->wavenum[i];
                    } else if (lf_cm < 0 && sys->wavenum[i] < 0 && sys->wavenum[i] < lf_cm) {
                        lf_cm = sys->wavenum[i];
                    }
                }
            } else {
                lf_cm = 0.0;
            }

            if (nfreq > 0) {
                try {
                    sys->totmass = 0.0;
                    for (const auto& atom : sys->a) {
                        sys->totmass += atom.mass;
                    }
                    calc::calcinertia(*sys);
                    sys->ilinear = 0;
                    for (double in : sys->inert) {
                        if (in < 0.001) {
                            sys->ilinear = 1;
                            break;
                        }
                    }
                    sys->ncenter = static_cast<int>(sys->a.size());

                    symmetry::SymmetryDetector symDetector;
                    symDetector.PGnameinit = sys->PGnameinit;
                    symDetector.ncenter    = sys->a.size();
                    symDetector.a          = sys->a;
                    symDetector.a_index.resize(sys->a.size());
                    for (size_t i = 0; i < sys->a.size(); ++i) {
                        symDetector.a_index[i] = i;
                    }
                    symDetector.detectPG(0);
                    sys->rotsym = symDetector.rotsym;
                    sys->PGname = symDetector.PGname;

                    sys->freq.resize(sys->nfreq);
                    for (int i = 0; i < sys->nfreq; ++i) {
                        sys->freq[i] = sys->wavenum[i] * wave2freq;
                    }
                    // Ensure elecontri has at least one electronic level.
                    // Many loaders (loadgau, loadorca, …) don't populate nelevel/elevel/edegen.
                    // Physical default: ground state only (E=0 eV), degeneracy = spin multiplicity.
                    if (sys->nelevel <= 0 || sys->elevel.empty() || sys->edegen.empty()) {
                        sys->nelevel = 1;
                        sys->elevel  = { 0.0 };                       // ground state, 0 eV excitation
                        sys->edegen  = { std::max(1, sys->spinmult) }; // degeneracy from spinmult
                    }
                    calc::ThermoResult tr = calc::calcthermo(*sys, T, P);
                    corrG_au = tr.corrG / au2kJ_mol;
                    corrH_au = tr.corrH / au2kJ_mol;
                    zpe_au   = tr.ZPE / au2kJ_mol;
                } catch (...) {
                    // Thermal property calculation failed (e.g. insufficient electronic
                    // level data for partition function).  Fall back to SCF-energy only.
                    nfreq    = 0;
                    corrG_au = 0.0;
                    corrH_au = 0.0;
                    zpe_au   = 0.0;
                }
            }
        }
        
        delete sys;
        return success;
    }

    bool calculate_thermal_corrections(const std::string& file, double T, double P, 
                                       double& corrG_au, double& corrH_au, double& zpe_au, int& nfreq)
    {
        double scf_au, lf_cm;
        std::string prog_name;
        return extract_basic_properties(file, T, P, scf_au, corrG_au, corrH_au, zpe_au, lf_cm, nfreq, prog_name);
    }

    void* create_system_data(const ThermoSettings& settings, const CommandContext& context, const std::string& input_file) 
    {
        SystemData* sys = new SystemData();
        
        sys->inputfile       = input_file;
        sys->T               = settings.temperature;
        sys->P               = settings.pressure;
        sys->Tlow            = settings.temp_low;
        sys->Thigh           = settings.temp_high;
        sys->Tstep           = settings.temp_step;
        sys->Plow            = settings.pressure_low;
        sys->Phigh           = settings.pressure_high;
        sys->Pstep           = settings.pressure_step;
        sys->concstr         = settings.concentration;
        sys->prtvib          = settings.print_vib;
        sys->massmod         = settings.mass_mode;
        sys->ipmode          = settings.ip_mode;
        sys->sclZPE          = settings.scale_zpe;
        sys->sclheat         = settings.scale_heat;
        sys->sclS            = settings.scale_entropy;
        sys->sclCV           = settings.scale_cv;
        sys->ravib           = settings.raise_vib;
        sys->intpvib         = settings.interp_vib;
        sys->imagreal        = settings.imag_real;
        sys->Eexter          = settings.external_energy;
        sys->outotm          = settings.output_otm ? 1 : 0;
        sys->inoset          = settings.no_settings ? 1 : 0;
        sys->PGnameinit      = settings.point_group;  // UserPG goes into PGnameinit
        sys->prtlevel        = settings.prt_level;
        sys->hgEntropy       = settings.hg_entropy;

        // Map quiet → prtlevel=0 override
        if (context.quiet && sys->prtlevel > 0) {
            sys->prtlevel = 0;
        }
        
        // Set low vibration treatment
        if (settings.low_vib_treatment == "harmonic") {
            sys->lowVibTreatment = LowVibTreatment::Harmonic;
        } else if (settings.low_vib_treatment == "truhlar") {
            sys->lowVibTreatment = LowVibTreatment::Truhlar;
        } else if (settings.low_vib_treatment == "grimme") {
            sys->lowVibTreatment = LowVibTreatment::Grimme;
        } else if (settings.low_vib_treatment == "minenkov") {
            sys->lowVibTreatment = LowVibTreatment::Minenkov;
        } else if (settings.low_vib_treatment == "headgordon") {
            sys->lowVibTreatment = LowVibTreatment::HeadGordon;
        } else {
            sys->lowVibTreatment = LowVibTreatment::Grimme;
        }

        // Set Bav preset from context
        if (!settings.bav_preset.empty()) {
            if (settings.bav_preset == "qchem") {
                sys->bavPreset      = BavPreset::QChem;
                sys->Bav            = bavPresetValue(BavPreset::QChem);
                sys->bavUserOverride = true;
            } else if (settings.bav_preset == "grimme") {
                sys->bavPreset      = BavPreset::Grimme;
                sys->Bav            = bavPresetValue(BavPreset::Grimme);
                sys->bavUserOverride = true;
            }
        }

        // Set OMP thread request
        sys->exec.omp_threads_requested = settings.omp_threads;
        
        return sys;
    }
    
    void initialize_thermo_module() 
    {
        // Initialize any global thermo module state if needed
    }
    
    void cleanup_thermo_module() 
    {
        // Cleanup any global thermo module state if needed
    }

    void print_program_header()
    {
        // Banner is printed by HelpUtils in ComChemKit; this is a no-op in integrated mode.
    }

    void display_molecular_info(const SystemData& sys, int nimag)
    {
        // This helper is now handled inline in process_file() with prtlevel awareness.
        // Kept for API compatibility.
        (void)sys;
        (void)nimag;
    }
}
