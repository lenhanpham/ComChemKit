/**
 * @file help_utils.cpp
 * @brief Implementation of help utility functions for OpenThermo
 * @author Le Nhan Pham
 * @date 2025
 *
 * This file contains implementations for help-related utility functions
 * used throughout the OpenThermo molecular thermochemistry program.
 */

#include "thermo/help_utils.h"
#include "utilities/version.h"
#include <iostream>
#include <string>

namespace ThermoHelpUtils
{

    void print_help(const std::string& program_name)
    {
        std::cout << "ComChemKit thermo module: Calculation of Thermochemical Properties\n"
                  << "Version " << ComChemKit::get_full_version() << "\n"
                  << "Developer: " << COMCHEMKIT_AUTHOR << "\n\n";
        std::cout << "Usage: " << program_name << " thermo [input_file] [options]\n\n";
        std::cout << "Description:\n";
        std::cout << "  OpenThermo calculates thermochemical properties from quantum chemistry output files.\n";
        std::cout << "  It supports various input formats and provides comprehensive thermodynamic analysis\n";
        std::cout
            << "  including Gibbs free energy, enthalpy, entropy, heat capacity, and vibrational corrections.\n\n";
        std::cout << "Input Files:\n";
        std::cout << "  input_file    Path to input file (.otm format or quantum chemistry output)\n";
        std::cout << "                Supported formats: Gaussian, ORCA, GAMESS-US, NWChem, CP2K, VASP, Q-Chem\n";
        std::cout << "                If no file specified, program will prompt for input\n\n";
        std::cout << "Options:\n";
        std::cout << "  -E <value>           Electronic energy in a.u. (overrides file value)\n";
        std::cout << "  -T <T>               Temperature in K (default: 298.15)\n";
        std::cout << "  -T <T1 T2 step>      Temperature scan from T1 to T2 with step\n";
        std::cout << "  -P <P>               Pressure in atm (default: 1.0)\n";
        std::cout << "  -P <P1 P2 step>      Pressure scan from P1 to P2 with step\n";
        std::cout << "  -sclZPE <factor>     Scale factor for ZPE frequencies (default: 1.0)\n";
        std::cout << "  -sclheat <factor>    Scale factor for thermal energy frequencies (default: 1.0)\n";
        std::cout << "  -sclS <factor>       Scale factor for entropy frequencies (default: 1.0)\n";
        std::cout << "  -sclCV <factor>      Scale factor for heat capacity frequencies (default: 1.0)\n";
        std::cout << "  -lowvibmeth <mode>   Low frequency treatment: 0/Harmonic, 1/Truhlar, 2/Grimme, 3/Minenkov, 4/HeadGordon\n";
        std::cout << "  -ravib <value>       Raising value for low frequencies in cm^-1 (default: 100.0)\n";
        std::cout << "  -intpvib <value>     Interpolation frequency threshold in cm^-1 (default: 100.0)\n";
        std::cout << "  -hg_entropy <bool>   Entropy interpolation for Head-Gordon: true/false (default: true)\n";
        std::cout << "  -bav <preset>        Bav for HeadGordon method: grimme, qchem (default for HeadGordon)\n";
        std::cout << "  -ipmode <mode>       Calculation mode: 0=gas phase, 1=condensed phase\n";
        std::cout << "  -imagreal <value>    Treat imaginary freq < value as real (default: 0.0)\n";
        std::cout << "  -conc <string>       Concentration string for phase correction\n";
        std::cout << "  -massmod <type>      Default mass type: 1=element, 2=most abundant isotope, 3=file\n";
        std::cout << "  -PGname <name>       Force point group symmetry\n";
        std::cout << "  -prtvib <mode>       Print vibration contributions: 0=no, 1=yes, -1=to file\n";
        std::cout << "  -prtlevel <level>    Output verbosity: 0=minimal, 1=default, 2=verbose, 3=full\n";
        std::cout << "  -outotm <mode>       Output .otm file: 0=no, 1=yes\n";
        std::cout << "  -omp-threads <N>     OpenMP thread count (default: half physical cores)\n";
        std::cout << "  -noset               Don't load settings from settings.ini\n";
        std::cout << "  --help               Show this help message\n";
        std::cout << "  --version, -v        Show version, authors, and citation\n";
        std::cout << "  --create-config      Create a default settings.ini file\n";
        std::cout << "  --help-input         Show input file format help\n";
        std::cout << "  --help-output        Show output format help\n";
        std::cout << "  --help-settings      Show settings file help\n";
        std::cout << "  --help-<option>      Show help for specific option (e.g., --help-T)\n\n";
        std::cout << "Settings File:\n";
        std::cout << "  Parameters can also be set in settings.ini file in current directory\n";
        std::cout << "  or in the directory specified by OPENTHERMOPATH environment variable\n\n";
        std::cout << "Output Files:\n";
        std::cout << "  <basename>.UHG       Thermodynamic quantities vs T/P (if scan performed)\n";
        std::cout << "  <basename>.SCq       Entropy, heat capacities, partition functions\n";
        std::cout << "  <basename>.vibcon    Individual vibration contributions (if requested)\n";
        std::cout << "  *.otm                OpenThermo format file (if -outotm 1)\n\n";
        std::cout << "Examples:\n";
        std::cout << "  " << program_name << " thermo molecule.log\n";
        std::cout << "  " << program_name << " thermo molecule.otm -T 300 -P 2.0\n";
        std::cout << "  " << program_name << " thermo molecule.out -T 273 373 10 -lowvibmeth 2\n";
        std::cout << "  " << program_name << " thermo --help-input\n";
        std::cout << "  " << program_name << " thermo --help-T\n\n";
        std::cout << "For more detailed help on specific topics, use --help-<topic>\n";
    }

}  // namespace ThermoHelpUtils