/**
 * @file thermo.cpp
 * @brief Implementation of thermo interface layer
 * @author Le Nhan Pham
 * @date 2025
 */

#include "thermo.h"
#include "chemsys.h"
#include "util.h"
#include "utilities/loadfile.h"
#include "calc.h"
#include "atommass.h"
#include "symmetry.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

// Use constants from chemsys.h
using namespace std;

namespace ThermoInterface 
{
    ThermoResult process_file(const CommandContext& context) 
    {
        ThermoResult result;
        
        try {
            // Print program header if not in quiet mode
            if (!context.quiet) {
                print_program_header();
            }
            
            // Initialize thermo module
            initialize_thermo_module();
            
            // Determine input file
            std::string input_file;
            if (!context.thermo_input_file.empty()) {
                input_file = context.thermo_input_file;
            } else if (!context.files.empty()) {
                input_file = context.files[0];
            } else {
                // Interactive input mode
                input_file = get_interactive_input();
                if (input_file.empty()) {
                    result.error_message = "No input file specified for thermo analysis";
                    return result;
                }
            }
            
            // Check if file exists
            if (!std::filesystem::exists(input_file)) {
                result.error_message = "Input file not found: " + input_file;
                return result;
            }
            
            // Print start processing message
            if (!context.quiet) {
                auto start_now = std::chrono::system_clock::now();
                std::time_t start_now_time = std::chrono::system_clock::to_time_t(start_now);
                std::cout << "                      -------- End of Summary --------\n";
                std::cout << "\n";
                std::cout << "OpenThermo started to process " << input_file << " at "
                          << std::ctime(&start_now_time);
            }
            
            // Create SystemData from context
            SystemData* sys = static_cast<SystemData*>(create_system_data(context, input_file));
            if (!sys) {
                result.error_message = "Failed to create system data";
                return result;
            }
            
            // Initialize isotope mass table
            atommass::initmass(*sys);
            
            // Load settings unless disabled
            if (!context.thermo_no_settings) {
                util::loadsettings(*sys);
            }
            
            // Process command-line arguments (simulate argc/argv from context)
            std::vector<std::string> args = {"cck", "thermo"};
            if (!input_file.empty()) {
                args.push_back(input_file);
            }
            util::loadarguments(*sys, args.size(), args);
            
            // Check if input file is a list file (.list or .txt)
            bool is_list_file = (input_file.find(".list") != std::string::npos) ||
                               (input_file.find(".txt") != std::string::npos);
            
            if (is_list_file) {
                // Process batch file
                std::vector<std::string> filelist;
                std::ifstream listfile(input_file);
                if (!listfile.is_open()) {
                    result.error_message = "Unable to open list file: " + input_file;
                    delete sys;
                    return result;
                }
                
                std::string line;
                while (std::getline(listfile, line)) {
                    // Trim whitespace
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
                
                // Process batch using calc::ensemble
                size_t nfile = filelist.size();
                std::vector<double> Elist(nfile), Ulist(nfile), Hlist(nfile), Glist(nfile), 
                                   Slist(nfile), CVlist(nfile), CPlist(nfile), QVlist(nfile), Qbotlist(nfile);
                
                calc::ensemble(*sys, filelist, Elist, Ulist, Hlist, Glist, Slist, CVlist, CPlist, QVlist, Qbotlist);
                
                result.success = true;
                delete sys;
                return result;
            }
            
            // Check if input file is an OTM file
            if (input_file.find(".otm") != std::string::npos) {
                LoadFile::loadotm(*sys);
            } else {
                util::QuantumChemistryProgram prog = util::deterprog(*sys);
            sys->isys = static_cast<int>(prog);
            
            if (prog == util::QuantumChemistryProgram::Unknown) {
                result.error_message = "Unsupported quantum chemistry program";
                delete sys;
                return result;
            }
            
            // Display mass mode information
            if (!context.quiet) {
                std::cout << "\n";
                if (sys->massmod == 1)
                    std::cout << " Atomic masses used: Element\n";
                if (sys->massmod == 2)
                    std::cout << " Atomic masses used: Most abundant isotope\n";
                if (sys->massmod == 3)
                    std::cout << " Atomic masses used: Read from quantum chemical output\n";
            }
            
            // Load data from file using LoadFile class
            LoadFile loader;
            
            try {
                switch (prog) {
                    case util::QuantumChemistryProgram::Gaussian:
                        if (!context.quiet) std::cout << "Processing Gaussian output file...\n";
                        loader.loadgau(*sys);
                        break;
                    case util::QuantumChemistryProgram::Orca:
                        if (!context.quiet) std::cout << "Processing ORCA output file...\n";
                        loader.loadorca(*sys);
                        break;
                    case util::QuantumChemistryProgram::Gamess:
                        if (!context.quiet) std::cout << "Processing GAMESS-US output file...\n";
                        loader.loadgms(*sys);
                        break;
                    case util::QuantumChemistryProgram::Nwchem:
                        if (!context.quiet) std::cout << "Processing NWChem output file...\n";
                        loader.loadnw(*sys);
                        break;
                    case util::QuantumChemistryProgram::Cp2k:
                        if (!context.quiet) {
                            std::cout << "Processing CP2K output file...\n";
                            if (sys->ipmode == 0) {
                                std::cout << " Note: If your system is not isolated (periodic crystals, slabs or adsorbate on "
                                             "surface), \n"
                                             "you may want to set"
                                             "\"ipmode\" = 1 settings.ini in order to ignore translation and rotation "
                                             "contributions. \n"
                                          << "This is typical for condensed materials calculations with CP2K and VASP \n\n";
                            }
                        }
                        loader.loadCP2K(*sys);
                        break;
                    case util::QuantumChemistryProgram::Vasp:
                        if (!context.quiet) std::cout << "Processing VASP output file...\n";
                        loader.loadvasp(*sys);
                        break;
                    case util::QuantumChemistryProgram::Xtb:
                        if (!context.quiet) std::cout << "Processing xtb g98.out file...\n";
                        loader.loadxtb(*sys);
                        break;
                    default:
                        result.error_message = "Unknown program type";
                        delete sys;
                        return result;
                }
            } catch (const std::exception& e) {
                result.error_message = "Failed to load data from input file: " + std::string(e.what());
                delete sys;
                return result;
            }
            
            // Process mass modifications and electronic levels
            util::modmass(*sys);
            sys->nelevel = 1;
            sys->elevel = {0.0};
            sys->edegen = {sys->spinmult};
            if (!sys->edegen.empty() && sys->edegen[0] <= 0) {
                sys->edegen[0] = 1;
            }
            
            // Generate OTM file if requested
            if (sys->outotm == 1) {
                util::outotmfile(*sys);
            }
            } // End of else block for non-OTM files
            
            // Handle external energy override
            if (sys->Eexter != 0.0) {
                sys->E = sys->Eexter;
                if (!context.quiet) {
                    std::cout << "Note: The electronic energy specified by \"E\" parameter will be used\n";
                }
            } else if (sys->E != 0.0 && !context.quiet) {
                std::cout << "Note: The electronic energy extracted from input file will be used\n";
            }
            
            // Handle imaginary frequency treatment
            if (sys->imagreal != 0.0) {
                for (int ifreq = 0; ifreq < sys->nfreq; ++ifreq) {
                    if (sys->wavenum[ifreq] < 0 && std::abs(sys->wavenum[ifreq]) < sys->imagreal) {
                        sys->wavenum[ifreq] = std::abs(sys->wavenum[ifreq]);
                        if (!context.quiet) {
                            std::cout << " Note: Imaginary frequency " << std::fixed << std::setprecision(2)
                                      << sys->wavenum[ifreq] << " cm^-1 has been set to real frequency!\n";
                        }
                    }
                }
            }
            
            // Print running parameters summary
            if (!context.quiet) {
                std::cout << "\n                   --- Summary of Current Parameters ---\n\nRunning parameters:\n";
                
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
                }
                
                if (sys->lowVibTreatment == LowVibTreatment::Grimme || sys->lowVibTreatment == LowVibTreatment::Minenkov) {
                    std::cout << " Vibrational frequency threshold used in the interpolation is " << std::fixed
                              << std::setprecision(2) << sys->intpvib << " cm^-1\n";
                }
                
                if (sys->imagreal != 0.0) {
                    std::cout << " Imaginary frequencies with norm < " << std::fixed << std::setprecision(2) << sys->imagreal
                              << " cm^-1 will be treated as real frequencies\n";
                }
                
                std::cout << "                      -------- End of Summary --------\n\n";
            }
            
            // Perform thermodynamic calculations
            std::filesystem::path input_path(input_file);
            std::string basename = input_path.stem().string();
            
            // Calculate total mass
            sys->totmass = 0.0;
            for (const auto& atom : sys->a) {
                sys->totmass += atom.mass;
            }
            
            // Calculate inertia and detect linearity
            calc::calcinertia(*sys);
            sys->ilinear = 0;
            for (double inert : sys->inert) {
                if (inert < 0.001) {
                    sys->ilinear = 1;
                    break;
                }
            }
            
            // Symmetry detection
            if (!context.quiet) {
                std::cout << "Number of atoms loaded: " << sys->a.size() << "\n";
            }
            if (sys->a.empty()) {
                result.error_message = "No atoms loaded from input file!";
                delete sys;
                return result;
            }
            
            symmetry::SymmetryDetector symDetector;
            symDetector.PGnameinit = sys->PGname;
            symDetector.ncenter = sys->a.size();
            symDetector.a = sys->a;
            symDetector.a_index.resize(sys->a.size());
            for (size_t i = 0; i < sys->a.size(); ++i) {
                symDetector.a_index[i] = i;
            }
            symDetector.detectPG(1);
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
            if (nimag > 0 && !context.quiet) {
                std::cout << " Note: There are " << nimag
                          << " imaginary frequencies, they will be ignored in the calculation\n";
            }
            
            // Display molecular information
            if (!context.quiet) {
                display_molecular_info(*sys, nimag);
            }
            
            // Check for scanning mode
            bool is_scanning = (sys->Tstep != 0.0) || (sys->Pstep != 0.0);
            
            if (is_scanning) {
                // Perform temperature/pressure scanning
                double P1 = sys->P, P2 = sys->P, Ps = 1.0;
                double T1 = sys->T, T2 = sys->T, Ts = 1.0;
                
                if (sys->Tstep != 0.0) {
                    T1 = sys->Tlow;
                    T2 = sys->Thigh;
                    Ts = sys->Tstep;
                }
                if (sys->Pstep != 0.0) {
                    P1 = sys->Plow;
                    P2 = sys->Phigh;
                    Ps = sys->Pstep;
                }
                
                // Generate output files
                std::string uhg_filename = basename + ".UHG";
                std::string scq_filename = basename + ".SCq";
                
                std::ofstream file_UHG(uhg_filename);
                std::ofstream file_SCq(scq_filename);
                
                if (!file_UHG.is_open() || !file_SCq.is_open()) {
                    result.error_message = "Failed to create scanning output files";
                    delete sys;
                    return result;
                }
                
                file_UHG << "Ucorr, Hcorr and Gcorr are in kcal/mol; U, H and G are in a.u.\n\n"
                         << "     T(K)      P(atm)  Ucorr     Hcorr     Gcorr            U                H                G\n";
                file_SCq << "S, CV and CP are in cal/mol/K; q(V=0)/NA and q(bot)/NA are unitless\n\n"
                         << "    T(K)       P(atm)    S         CV        CP        q(V=0)/NA      q(bot)/NA\n";
                
                if (Ts > 0 && Ps > 0) {
                    const int num_step_T = static_cast<int>((T2 - T1) / Ts) + 1;
                    
                    for (int i = 0; i < num_step_T; ++i) {
                        const double T = T1 + i * Ts;
                        const int num_step_P = static_cast<int>((P2 - P1) / Ps) + 1;
                        
                        for (int j = 0; j < num_step_P; ++j) {
                            const double P = P1 + j * Ps;
                            
                            double corrU, corrH, corrG, S, Cv, Cp, Qv, Qbot;
                            calc::calcthermo(*sys, T, P, corrU, corrH, corrG, S, Cv, Cp, Qv, Qbot);
                            
                            file_UHG << std::fixed << std::setprecision(3) << std::setw(10) << T << std::setw(10) << P
                                     << std::setprecision(3) << std::setw(10) << corrU / cal2J << std::setw(10)
                                     << corrH / cal2J << std::setw(10) << corrG / cal2J << std::setprecision(6)
                                     << std::setw(17) << corrU / au2kJ_mol + sys->E << std::setw(17)
                                     << corrH / au2kJ_mol + sys->E << std::setw(17) << corrG / au2kJ_mol + sys->E << "\n";
                            file_SCq << std::fixed << std::setprecision(3) << std::setw(10) << T << std::setw(10) << P
                                     << std::setprecision(3) << std::setw(10) << S / cal2J << std::setw(10)
                                     << Cv / cal2J << std::setw(10) << Cp / cal2J << std::scientific
                                     << std::setprecision(6) << std::setw(16) << Qv / NA << std::setw(16) << Qbot / NA << "\n";
                        }
                    }
                }
                
                file_UHG.close();
                file_SCq.close();
                
                if (!context.quiet) {
                    std::cout << "\n Congratulation! Thermochemical properties at various temperatures/pressures were calculated " << "\n"
                              << " " << "All data were exported to " << uhg_filename << " and " << scq_filename << "\n" 
                              << " " << uhg_filename << " contains thermal correction to U, H and G, and sum of electronic energy and corresponding corrections\n"
                              << " " << scq_filename << " contains S, CV, CP, q(V=0) and q(bot)\n";
                }
                
                result.output_files.push_back(uhg_filename);
                result.output_files.push_back(scq_filename);
                
            } else {
                // Single point calculation
                calc::showthermo(*sys);
            }
            
            // Generate .otm file if requested
            if (sys->outotm) {
                util::outotmfile(*sys);
                std::string otm_filename = basename + ".otm";
                result.output_files.push_back(otm_filename);
            }
            
            result.success = true;
            result.exit_code = 0;
            
            // Print completion message
            if (!context.quiet) {
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
    
    ThermoResult process_batch(const CommandContext& context, 
                              const std::vector<std::string>& files) 
    {
        ThermoResult result;
        result.success = true;
        
        for (const auto& file : files) {
            CommandContext file_context = context;
            file_context.thermo_input_file = file;
            
            ThermoResult file_result = process_file(file_context);
            
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
    
    void* create_system_data(const CommandContext& context, const std::string& input_file) 
    {
        SystemData* sys = new SystemData();
        
        // Initialize with defaults
        sys->inputfile = input_file;
        sys->T = context.thermo_temperature;
        sys->P = context.thermo_pressure;
        sys->Tlow = context.thermo_temp_low;
        sys->Thigh = context.thermo_temp_high;
        sys->Tstep = context.thermo_temp_step;
        sys->Plow = context.thermo_pressure_low;
        sys->Phigh = context.thermo_pressure_high;
        sys->Pstep = context.thermo_pressure_step;
        sys->concstr = context.thermo_concentration;
        sys->prtvib = context.thermo_print_vib;
        sys->massmod = context.thermo_mass_mode;
        sys->ipmode = context.thermo_ip_mode;
        sys->sclZPE = context.thermo_scale_zpe;
        sys->sclheat = context.thermo_scale_heat;
        sys->sclS = context.thermo_scale_entropy;
        sys->sclCV = context.thermo_scale_cv;
        sys->ravib = context.thermo_raise_vib;
        sys->intpvib = context.thermo_interp_vib;
        sys->imagreal = context.thermo_imag_real;
        sys->Eexter = context.thermo_external_energy;
        sys->outotm = context.thermo_output_otm;
        sys->inoset = !context.thermo_no_settings;
        sys->PGname = context.thermo_point_group;
        
        // Set low vibration treatment
        if (context.thermo_low_vib_treatment == "harmonic") {
            sys->lowVibTreatment = LowVibTreatment::Harmonic;
        } else if (context.thermo_low_vib_treatment == "truhlar") {
            sys->lowVibTreatment = LowVibTreatment::Truhlar;
        } else if (context.thermo_low_vib_treatment == "grimme") {
            sys->lowVibTreatment = LowVibTreatment::Grimme;
        } else if (context.thermo_low_vib_treatment == "minenkov") {
            sys->lowVibTreatment = LowVibTreatment::Minenkov;
        } else {
            sys->lowVibTreatment = LowVibTreatment::Harmonic;
        }
        
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
        std::cout << "  " << "                                                                                     \n"
                  << "  " << "   ***********************************************************************     " << " \n"
                  << "  " << "                                OPENTHERMO                                     " << " \n"
                  << "  " << "   ***********************************************************************     " << " \n"
                  << "# " << "-------------------------------------------------------------------------------" << "#\n"
                  << "# " << "Version 0.001.1  Release date: 2025                                            " << "#\n"
                  << "# " << "Developer: Le Nhan Pham                                                        " << "#\n"
                  << "# " << "https://github.com/lenhanpham/openthermo                                       " << "#\n"
                  << "# " << "-------------------------------------------------------------------------------" << "#\n";

        std::cout << "  " << "                                                                                     \n"
                  << "  " << "                                                                               " << " \n"
                  << "  " << "Please cite this preprint if you use OpenThermo for your research              " << " \n"
                  << "  " << "                                                                               " << " \n"
                  << "# " << "-------------------------------------------------------------------------------" << "#\n"
                  << "# " << "L.N Pham, \"OpenThermo A Comprehensive C++ Program for Calculation of           " << "#\n"
                  << "# " << "Thermochemical Properties\" 2025, http://dx.doi.org/10.13140/RG.2.2.22380.63363 " << "#\n"
                  << "# " << "-------------------------------------------------------------------------------" << "#\n";
    }
    
    std::string get_interactive_input() 
    {
        std::cout << "\nInput file path, e.g. D:\\your_dir\\your_calc.log\n"
                  << " OpenThermo supports Gaussian, ORCA, GAMESS-US, NWChem, CP2K, and VASP "
                     "\n";
        std::string input_file;
        while (true) {
            std::getline(std::cin, input_file);
            input_file.erase(std::remove(input_file.begin(), input_file.end(), '"'), input_file.end());
            std::ifstream check(input_file);
            bool file_exists = check.good();
            check.close();
            if (file_exists) {
                break;
            }
            std::cout << "Cannot find the file, input again!\n";
        }
        return input_file;
    }
    
    void display_molecular_info(const SystemData& sys, int nimag) 
    {
        std::cout << "\n"
                  << "                      -------- Chemical System Data -------\n"
                  << "                      -------------------------------------\n"
                  << " Electronic energy: " << std::fixed << std::setprecision(8) << std::setw(18) << sys.E
                  << " a.u.\n";
        
        if (sys.spinmult != 0) {
            std::cout << " Spin multiplicity: " << std::setw(3) << sys.spinmult << "\n";
        } else {
            for (int ie = 0; ie < sys.nelevel; ++ie) {
                std::cout << " Electronic energy level " << ie + 1 << "     E= " << std::fixed
                          << std::setprecision(6) << std::setw(12) << sys.elevel[ie]
                          << " eV     Degeneracy= " << std::setw(3) << sys.edegen[ie] << "\n";
            }
        }
        
        for (int iatm = 0; iatm < sys.ncenter; ++iatm) {
            std::cout << " Atom " << std::setw(5) << iatm + 1 << " (" << ind2name[sys.a[iatm].index]
                      << ")   Mass: " << std::fixed << std::setprecision(6) << std::setw(12) << sys.a[iatm].mass
                      << " amu\n";
        }
        
        std::cout << " Total mass: " << std::fixed << std::setprecision(6) << std::setw(16) << sys.totmass
                  << " amu\n\n"
                  << " Point group: " << sys.PGname << "\n";
        
        if (sys.ipmode == 0) {
            std::cout << " Rotational symmetry number: " << std::setw(3) << sys.rotsym << "\n";
            
            std::array<double, 3> sorted_inert = sys.inert;
            std::sort(sorted_inert.begin(), sorted_inert.end());
            
            std::cout << " Principal moments of inertia (amu*Bohr^2):\n";
            for (double i : sorted_inert) {
                std::cout << std::fixed << std::setprecision(6) << std::setw(16) << i << "\n";
            }
            
            double inert_sum = sorted_inert[0] + sorted_inert[1] + sorted_inert[2];
            if (inert_sum < 1e-10) {
                std::cout << "This is a single atom system, rotational constant is zero\n";
            } else if (sys.ilinear == 1) {
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
                    rotcst[i] = h / (8.0 * pi * pi * sys.inert[i] * amu2kg * (b2a * 1e-10) * (b2a * 1e-10));
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
        } else {
            std::cout << "Rotation information is not shown here since ipmode=1\n";
        }
        
        if (sys.nfreq > 0) {
            std::cout << "\n There are " << sys.nfreq << " frequencies (cm^-1):\n";
            for (int ifreq = 0; ifreq < sys.nfreq; ++ifreq) {
                std::cout << std::fixed << std::setprecision(1) << std::setw(8) << sys.wavenum[ifreq];
                if ((ifreq + 1) % 9 == 0 || ifreq == sys.nfreq - 1)
                    std::cout << "\n";
            }
        }
        
        if (nimag > 0) {
            std::cout << " Note: There are " << nimag
                      << " imaginary frequencies, they will be ignored in the calculation\n";
        }
    }
}
