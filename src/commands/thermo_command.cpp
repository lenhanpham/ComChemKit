#include "commands/thermo_command.h"
#include "commands/signal_handler.h"
#include "thermo/thermo.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <filesystem>
#include <atomic>
#include <memory>

extern std::atomic<bool> g_shutdown_requested;

std::string ThermoCommand::get_name() const {
    return "thermo";
}

std::string ThermoCommand::get_description() const {
    return "Advanced thermodynamic analysis for multiple quantum chemistry programs";
}

void ThermoCommand::parse_args(int argc, char* argv[], int& i, CommandContext& context) {
    std::string arg = argv[i];

    // CLI arguments that override settings.ini - these get passed to util::loadarguments
    if (arg == "-E" && i + 1 < argc)
    {
        settings.external_energy = std::stod(argv[++i]);
        settings.cli_args.push_back("-E");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-prtvib" && i + 1 < argc)
    {
        settings.print_vib = std::stoi(argv[++i]);
        settings.cli_args.push_back("-prtvib");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-T" && i + 1 < argc)
    {
        // Check if it's a scan (3 values) or single value
        if (i + 3 < argc)
        {
            try
            {
                double low  = std::stod(argv[i + 1]);
                double high = std::stod(argv[i + 2]);
                double step = std::stod(argv[i + 3]);
                // Valid scan
                settings.temp_low  = low;
                settings.temp_high = high;
                settings.temp_step = step;
                settings.cli_args.push_back("-T");
                settings.cli_args.push_back(argv[++i]);
                settings.cli_args.push_back(argv[++i]);
                settings.cli_args.push_back(argv[++i]);
            }
            catch (...)
            {
                // Single value
                settings.temperature = std::stod(argv[++i]);
                settings.cli_args.push_back("-T");
                settings.cli_args.push_back(argv[i]);
            }
        }
        else
        {
            // Single value
            settings.temperature = std::stod(argv[++i]);
            settings.cli_args.push_back("-T");
            settings.cli_args.push_back(argv[i]);
        }
    }
    else if (arg == "-P" && i + 1 < argc)
    {
        // Check if it's a scan (3 values) or single value
        if (i + 3 < argc)
        {
            try
            {
                double low  = std::stod(argv[i + 1]);
                double high = std::stod(argv[i + 2]);
                double step = std::stod(argv[i + 3]);
                // Valid scan
                settings.pressure_low  = low;
                settings.pressure_high = high;
                settings.pressure_step = step;
                settings.cli_args.push_back("-P");
                settings.cli_args.push_back(argv[++i]);
                settings.cli_args.push_back(argv[++i]);
                settings.cli_args.push_back(argv[++i]);
            }
            catch (...)
            {
                // Single value
                settings.pressure = std::stod(argv[++i]);
                settings.cli_args.push_back("-P");
                settings.cli_args.push_back(argv[i]);
            }
        }
        else
        {
            // Single value
            settings.pressure = std::stod(argv[++i]);
            settings.cli_args.push_back("-P");
            settings.cli_args.push_back(argv[i]);
        }
    }
    else if (arg == "-conc" && i + 1 < argc)
    {
        settings.concentration = argv[++i];
        settings.cli_args.push_back("-conc");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-sclZPE" && i + 1 < argc)
    {
        settings.scale_zpe = std::stod(argv[++i]);
        settings.cli_args.push_back("-sclZPE");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-sclheat" && i + 1 < argc)
    {
        settings.scale_heat = std::stod(argv[++i]);
        settings.cli_args.push_back("-sclheat");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-sclS" && i + 1 < argc)
    {
        settings.scale_entropy = std::stod(argv[++i]);
        settings.cli_args.push_back("-sclS");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-sclCV" && i + 1 < argc)
    {
        settings.scale_cv = std::stod(argv[++i]);
        settings.cli_args.push_back("-sclCV");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-lowvibmeth" && i + 1 < argc)
    {
        settings.low_vib_treatment = argv[++i];
        settings.cli_args.push_back("-lowvibmeth");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-ravib" && i + 1 < argc)
    {
        settings.raise_vib = std::stod(argv[++i]);
        settings.cli_args.push_back("-ravib");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-ipmode" && i + 1 < argc)
    {
        settings.ip_mode = std::stoi(argv[++i]);
        settings.cli_args.push_back("-ipmode");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-imagreal" && i + 1 < argc)
    {
        settings.imag_real = std::stod(argv[++i]);
        settings.cli_args.push_back("-imagreal");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-massmod" && i + 1 < argc)
    {
        settings.mass_mode = std::stoi(argv[++i]);
        settings.cli_args.push_back("-massmod");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-PGname" && i + 1 < argc)
    {
        settings.point_group = argv[++i];
        settings.cli_args.push_back("-PGname");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-noset")
    {
        settings.no_settings = true;
        settings.cli_args.push_back("-noset");
    }
    else if (arg == "-outotm" && i + 1 < argc)
    {
        settings.output_otm = (std::stoi(argv[++i]) != 0);
        settings.cli_args.push_back("-outotm");
        settings.cli_args.push_back(argv[i]);
    }
    // Long-form arguments (for user convenience)
    else if (arg == "-prtlevel" && i + 1 < argc)
    {
        settings.prt_level = std::stoi(argv[++i]);
        settings.cli_args.push_back("-prtlevel");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-hgEntropy" && i + 1 < argc)
    {
        settings.hg_entropy = (std::stoi(argv[++i]) != 0);
        settings.cli_args.push_back("-hgEntropy");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-bav" && i + 1 < argc)
    {
        settings.bav_preset = argv[++i];
        settings.cli_args.push_back("-bav");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "-omp-threads" && i + 1 < argc)
    {
        settings.omp_threads = std::stoi(argv[++i]);
        settings.cli_args.push_back("-omp-threads");
        settings.cli_args.push_back(argv[i]);
    }
    else if (arg == "--temp-scan" && i + 3 < argc)
    {
        settings.temp_low  = std::stod(argv[++i]);
        settings.temp_high = std::stod(argv[++i]);
        settings.temp_step = std::stod(argv[++i]);
        settings.cli_args.push_back("-T");
        settings.cli_args.push_back(std::to_string(settings.temp_low));
        settings.cli_args.push_back(std::to_string(settings.temp_high));
        settings.cli_args.push_back(std::to_string(settings.temp_step));
    }
    else if (arg == "--pressure-scan" && i + 3 < argc)
    {
        settings.pressure_low  = std::stod(argv[++i]);
        settings.pressure_high = std::stod(argv[++i]);
        settings.pressure_step = std::stod(argv[++i]);
        settings.cli_args.push_back("-P");
        settings.cli_args.push_back(std::to_string(settings.pressure_low));
        settings.cli_args.push_back(std::to_string(settings.pressure_high));
        settings.cli_args.push_back(std::to_string(settings.pressure_step));
    }
    else if (arg == "--temperature")
    {
        if (++i < argc)
        {
            settings.temperature = std::stod(argv[i]);
            settings.cli_args.push_back("-T");
            settings.cli_args.push_back(argv[i]);
        }
    }
    else if (arg == "--pressure")
    {
        if (++i < argc)
        {
            settings.pressure = std::stod(argv[i]);
            settings.cli_args.push_back("-P");
            settings.cli_args.push_back(argv[i]);
        }
    }
    else if (arg == "--low-vib-treatment")
    {
        if (++i < argc)
        {
            settings.low_vib_treatment = argv[i];
            settings.cli_args.push_back("-lowvibmeth");
            settings.cli_args.push_back(argv[i]);
        }
    }
    else if (arg == "--scale-zpe")
    {
        if (++i < argc)
        {
            settings.scale_zpe = std::stod(argv[i]);
            settings.cli_args.push_back("-sclZPE");
            settings.cli_args.push_back(argv[i]);
        }
    }
    else if (arg == "--scale-heat")
    {
        if (++i < argc)
        {
            settings.scale_heat = std::stod(argv[i]);
            settings.cli_args.push_back("-sclheat");
            settings.cli_args.push_back(argv[i]);
        }
    }
    else if (arg == "--scale-entropy")
    {
        if (++i < argc)
        {
            settings.scale_entropy = std::stod(argv[i]);
            settings.cli_args.push_back("-sclS");
            settings.cli_args.push_back(argv[i]);
        }
    }
    else if (arg == "--output-otm")
    {
        settings.output_otm = true;
        settings.cli_args.push_back("-outotm");
        settings.cli_args.push_back("1");
    }
    else if (arg == "--point-group")
    {
        if (++i < argc)
        {
            settings.point_group = argv[i];
            settings.cli_args.push_back("-PGname");
            settings.cli_args.push_back(argv[i]);
        }
    }
    else if (arg == "--help-input")
    {
        thermo_help_topic = "input";
    }
    else if (arg == "--help-output")
    {
        thermo_help_topic = "output";
    }
    else if (arg == "--help-settings")
    {
        thermo_help_topic = "settings";
    }
    else if (arg.substr(0, 7) == "--help-")
    {
        std::string option        = arg.substr(7);
        thermo_help_topic = option;
    }
    else
    {
        // Handle input files
        if (!arg.empty() && arg[0] != '-')
        {
            context.files.push_back(arg);
        }
        else
        {
            --i;  // Unrecognized option, let other parsers handle it
        }
    }
}



int ThermoCommand::execute(const CommandContext& context)
{

    try
    {
        if (!context.quiet)
        {
            std::cout << "Starting thermodynamic analysis using OpenThermo module..." << std::endl;
        }

        // Process files using thermo interface
        ThermoInterface::ThermoResult result;

        if (!context.files.empty())
        {
            // Process multiple files
            result = ThermoInterface::process_batch(settings, context, context.files);
        }
        else if (!settings.input_file.empty())
        {
            // Process single file
            result = ThermoInterface::process_file(settings, context);
        }
        else
        {
            // Auto-detect files in current directory
            std::vector<std::string> auto_files;
            for (const auto& entry : std::filesystem::directory_iterator("."))
            {
                if (entry.is_regular_file())
                {
                    std::string filename = entry.path().filename().string();
                    std::string ext      = entry.path().extension().string();

                    // Check for supported quantum chemistry output files
                    if (ext == ".log" || ext == ".out" || ext == ".LOG" || ext == ".OUT" || ext == ".output")
                    {
                        auto_files.push_back(filename);
                    }
                }
            }

            if (auto_files.empty())
            {
                std::cerr << "No suitable input files found in current directory." << std::endl;
                std::cerr << "Supported extensions: .log, .out" << std::endl;
                return 1;
            }

            if (!context.quiet)
            {
                std::cout << "Found " << auto_files.size() << " input files for processing." << std::endl;
            }
            result = ThermoInterface::process_batch(settings, context, auto_files);
        }

        // Report results
        if (result.success)
        {
            if (!context.quiet)
            {
                std::cout << "Thermodynamic analysis completed successfully." << std::endl;
                if (!result.output_files.empty())
                {
                    std::cout << "Output files generated:" << std::endl;
                    for (const auto& file : result.output_files)
                    {
                        std::cout << "  " << file << std::endl;
                    }
                }
            }
            return 0;
        }
        else
        {
            std::cerr << "Thermodynamic analysis failed: " << result.error_message << std::endl;
            return result.exit_code != 0 ? result.exit_code : 1;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error in thermo command: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Fatal error: Unknown exception occurred in thermo command" << std::endl;
        return 1;
    }
}

