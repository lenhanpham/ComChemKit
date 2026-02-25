#include "commands/extract_command.h"
#include "commands/signal_handler.h"
#include "utilities/config_manager.h"
#include "extraction/qc_extractor.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <filesystem>
#include <atomic>
#include <memory>

extern std::atomic<bool> g_shutdown_requested;

extern ConfigManager g_config_manager;

ExtractCommand::ExtractCommand() {
    if (g_config_manager.is_config_loaded()) {
        temp               = g_config_manager.get_default_temperature();
        concentration      = static_cast<int>(g_config_manager.get_default_concentration() * 1000);
        sort_column        = g_config_manager.get_int("default_sort_column");
        output_format      = g_config_manager.get_default_output_format();
        use_input_temp     = g_config_manager.get_bool("use_input_temp");
        memory_limit_mb    = g_config_manager.get_size_t("memory_limit_mb");
    }
}

std::string ExtractCommand::get_name() const {
    return "extract";
}

std::string ExtractCommand::get_description() const {
    return "Process Gaussian log files and extract thermodynamic data";
}

void ExtractCommand::parse_args(int argc, char* argv[], int& i, CommandContext& context) {
    std::string arg = argv[i];

    if (arg == "-t" || arg == "--temp")
    {
        if (++i < argc)
        {
            try
            {
                temp = std::stod(argv[i]);
                if (temp <= 0)
                {
                    context.warnings.push_back("Warning: Temperature must be positive. Using default 298.15 K.");
                    temp = 298.15;
                }
                else
                {
                    use_input_temp = true;
                }
            }
            catch (const std::exception& e)
            {
                context.warnings.push_back("Error: Invalid temperature format. Using default 298.15 K.");
                temp = 298.15;
            }
        }
        else
        {
            context.warnings.push_back("Error: Temperature value required after -t/--temp.");
        }
    }
    else if (arg == "-p" || arg == "--pressure")
    {
        if (++i < argc)
        {
            try
            {
                pressure = std::stod(argv[i]);
                if (pressure <= 0)
                {
                    context.warnings.push_back("Warning: Pressure must be positive. Using default 1.0 atm.");
                    pressure = 1.0;
                }
                else
                {
                    use_input_pressure = true;
                }
            }
            catch (const std::exception& e)
            {
                context.warnings.push_back("Error: Invalid pressure format. Using default 1.0 atm.");
                pressure = 1.0;
            }
        }
        else
        {
            context.warnings.push_back("Error: Pressure value required after -p/--pressure.");
        }
    }
    else if (arg == "-c" || arg == "--cm")
    {
        if (++i < argc)
        {
            try
            {
                int conc = std::stoi(argv[i]);
                if (conc <= 0)
                {
                    context.warnings.push_back("Error: Concentration must be positive. Using configured default.");
                    concentration = static_cast<int>(g_config_manager.get_default_concentration() * 1000);
                }
                else
                {
                    concentration = conc * 1000;
                    use_input_concentration = true;
                }
            }
            catch (const std::exception& e)
            {
                context.warnings.push_back("Error: Invalid concentration format. Using configured default.");
                concentration = static_cast<int>(g_config_manager.get_default_concentration() * 1000);
            }
        }
        else
        {
            context.warnings.push_back("Error: Concentration value required after -c/--cm.");
        }
    }
    else if (arg == "-col" || arg == "--column")
    {
        if (++i < argc)
        {
            try
            {
                int col = std::stoi(argv[i]);
                if (col >= 1 && col <= 10)
                {
                    sort_column = col;
                }
                else
                {
                    context.warnings.push_back("Error: Column must be between 1-10. Using default column 2.");
                }
            }
            catch (const std::exception& e)
            {
                context.warnings.push_back("Error: Invalid column format. Using default column 2.");
            }
        }
        else
        {
            context.warnings.push_back("Error: Column value required after -col/--column.");
        }
    }
    else if (arg == "-f" || arg == "--format")
    {
        if (++i < argc)
        {
            std::string fmt = argv[i];
            if (fmt == "text" || fmt == "csv")
            {
                output_format = fmt;
            }
            else
            {
                context.warnings.push_back("Error: Format must be 'text' or 'csv'. Using default 'text'.");
                output_format = "text";
            }
        }
        else
        {
            context.warnings.push_back("Error: Format value required after -f/--format.");
        }
    }
    else if (arg == "--memory-limit")
    {
        if (++i < argc)
        {
            try
            {
                int size = std::stoi(argv[i]);
                if (size <= 0)
                {
                    context.warnings.push_back("Error: Memory limit must be positive. Using auto-calculated limit.");
                }
                else
                {
                    memory_limit_mb = static_cast<size_t>(size);
                }
            }
            catch (const std::exception& e)
            {
                context.warnings.push_back("Error: Invalid memory limit format. Using auto-calculated limit.");
            }
        }
        else
        {
            context.warnings.push_back("Error: Memory limit value required after --memory-limit.");
        }
    }
    else if (arg == "--resource-info")
    {
        show_resource_info = true;
    }
    else if (!arg.empty() && arg.front() == '-')
    {
        context.warnings.push_back("Warning: Unknown argument '" + arg + "' ignored.");
    }
    else if (!arg.empty())
    {
        context.files.push_back(arg);
    }
}



int ExtractCommand::execute(const CommandContext& context)
{

    // Show warnings if any
    if (!context.warnings.empty() && !context.quiet)
    {
        for (const auto& warning : context.warnings)
        {
            std::cerr << warning << std::endl;
        }
        std::cerr << std::endl;
    }

    // Show resource information if requested
    if (show_resource_info)
    {
        JobResources job_resources  = context.job_resources;
        unsigned int hardware_cores = std::thread::hardware_concurrency();

        std::cout << "\n=== System Resource Information ===\n";
        std::cout << "Hardware cores detected: " << hardware_cores << "\n";
        std::cout << "System memory: " << MemoryMonitor::get_system_memory_mb() << " MB\n";
        std::cout << "Requested threads: " << context.requested_threads << "\n";
        if (memory_limit_mb > 0)
        {
            std::cout << "Memory limit: " << memory_limit_mb << " MB (user-specified)\n";
        }
        else
        {
            std::cout << "Memory limit: Auto-calculated based on threads and system memory\n";
        }

        // Job scheduler information
        if (job_resources.scheduler_type != SchedulerType::NONE)
        {
            std::cout << "\n=== Job Scheduler Information ===\n";
            std::cout << "Scheduler: " << JobSchedulerDetector::scheduler_name(job_resources.scheduler_type) << "\n";
            std::cout << "Job ID: " << job_resources.job_id << "\n";
            if (job_resources.has_cpu_limit)
            {
                std::cout << "Job allocated CPUs: " << job_resources.allocated_cpus << "\n";
            }
            if (job_resources.has_memory_limit)
            {
                std::cout << "Job allocated memory: " << job_resources.allocated_memory_mb << " MB\n";
            }
            if (!job_resources.partition.empty())
            {
                std::cout << "Partition/Queue: " << job_resources.partition << "\n";
            }
        }
        else
        {
            std::cout << "Job scheduler: None detected\n";
        }

        std::cout << "=====================================\n\n";
    }

    try
    {
        // Call the existing processAndOutputResults function
        processAndOutputResults(temp,
                                pressure,
                                concentration,
                                sort_column,
                                context.extension,
                                context.quiet,
                                output_format,
                                use_input_temp,
                                use_input_pressure,
                                use_input_concentration,
                                context.requested_threads,
                                context.max_file_size_mb,
                                memory_limit_mb,
                                context.warnings,
                                context.job_resources,
                                context.batch_size);

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Fatal error: Unknown exception occurred" << std::endl;
        return 1;
    }
}

