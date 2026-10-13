#include "commands/extract_au_command.h"
#include "high_level/high_level_energy.h"
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

ExtractAuCommand::ExtractAuCommand() {
    if (g_config_manager.is_config_loaded()) {
        temp               = g_config_manager.get_default_temperature();
        concentration      = static_cast<int>(g_config_manager.get_default_concentration() * 1000);
        sort_column        = g_config_manager.get_int("default_sort_column");
        output_format      = g_config_manager.get_default_output_format();
        use_input_temp     = g_config_manager.get_bool("use_input_temp");
        memory_limit_mb    = g_config_manager.get_size_t("memory_limit_mb");
    }
}

std::string ExtractAuCommand::get_name() const {
    return "extract-au";
}

std::string ExtractAuCommand::get_description() const {
    return "Extract single-file energies with component breakdown in atomic units";
}

void ExtractAuCommand::parse_args(int argc, char* argv[], int& i, CommandContext& context) {
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
    else if (arg == "-c" || arg == "-C" || arg == "--cm" || arg == "--conc")
    {
        if (++i < argc)
        {
            try
            {
                double conc = std::stod(argv[i]);
                if (conc <= 0)
                {
                    context.warnings.push_back("Error: Concentration must be positive. Using configured default.");
                    concentration = static_cast<int>(g_config_manager.get_default_concentration() * 1000);
                }
                else
                {
                    concentration = static_cast<int>(conc * 1000);
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
                if (col >= 1 && col <= 9)
                {
                    sort_column = col;
                }
                else
                {
                    context.warnings.push_back("Error: Column must be between 1-9. Using default column 2.");
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
    else if (arg == "-lowvibmeth")
    {
        if (++i < argc)
        {
            std::string method = argv[i];
            static const std::vector<std::string> valid_methods = {
                "harmonic", "truhlar", "grimme", "minenkov", "headgordon"
            };
            if (std::find(valid_methods.begin(), valid_methods.end(), method) != valid_methods.end())
            {
                low_vib_method = method;
            }
            else
            {
                context.warnings.push_back(
                    "Warning: Unknown low-vib method '" + method +
                    "'. Valid options: harmonic|truhlar|grimme|minenkov|headgordon. Using default 'grimme'.");
                low_vib_method = "grimme";
            }
        }
        else
        {
            context.warnings.push_back("Error: Method name required after -lowvibmeth.");
        }
    }
    else if (arg == "-ravib")
    {
        if (++i < argc)
        {
            try
            {
                double rv = std::stod(argv[i]);
                if (rv <= 0)
                {
                    context.warnings.push_back("Warning: Crossover frequency must be positive. Using default 100.0 cm-1.");
                }
                else
                {
                    ravib = rv;
                }
            }
            catch (const std::exception& e)
            {
                context.warnings.push_back("Error: Invalid crossover frequency format. Using default 100.0 cm-1.");
            }
        }
        else
        {
            context.warnings.push_back("Error: Frequency value required after -ravib.");
        }
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



int ExtractAuCommand::execute(const CommandContext& context)
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
        // Find log files in the current directory (single-file mode: no parent lookup).
        std::vector<std::string> log_files;
        if (context.batch_size > 0)
        {
            log_files = findLogFiles(context.extension, context.max_file_size_mb, context.batch_size);
        }
        else
        {
            log_files = findLogFiles(context.extension, context.max_file_size_mb);
        }
        std::vector<std::string> filtered_files;
        std::copy_if(log_files.begin(),
                     log_files.end(),
                     std::back_inserter(filtered_files),
                     [&context](const std::string& file) {
                         return file.find(context.extension) != std::string::npos;
                     });

        if (!context.quiet)
        {
            std::cout << "Found " << filtered_files.size() << " " << context.extension << " files" << std::endl;

            unsigned int hardware_cores = std::thread::hardware_concurrency();
            std::cout << "System: " << hardware_cores << " cores detected" << std::endl;
            std::cout << "Requested: " << context.requested_threads << " threads";
            if (context.requested_threads == hardware_cores / 2)
            {
                std::cout << " (default: half cores)";
            }
            std::cout << std::endl;

            if (context.job_resources.scheduler_type != SchedulerType::NONE)
            {
                std::cout << "Environment: "
                          << JobSchedulerDetector::scheduler_name(context.job_resources.scheduler_type)
                          << " job execution" << std::endl;
            }
            else
            {
                std::cout << "Environment: Interactive/local execution" << std::endl;
            }
        }

        double concentration_m = static_cast<double>(concentration) / 1000.0;

        unsigned int requested_threads =
            context.requested_threads > 0
                ? context.requested_threads
                : calculateSafeThreadCount(context.requested_threads, 100, context.job_resources);
        unsigned int thread_count =
            filtered_files.empty()
                ? requested_threads
                : std::min(requested_threads, static_cast<unsigned int>(filtered_files.size()));

        if (!context.quiet)
        {
            std::cout << "Using: " << thread_count << " threads";
            if (thread_count < requested_threads)
            {
                std::cout << " (reduced for safety)";
            }
            std::cout << std::endl;
            std::cout << "Max file size limit: " << context.max_file_size_mb << " MB" << std::endl;
        }

        JobResources job_resources;
        job_resources.allocated_memory_mb = memory_limit_mb > 0
                                                ? memory_limit_mb
                                                : calculateSafeMemoryLimit(0, thread_count, context.job_resources);
        job_resources.allocated_cpus      = thread_count;

        if (!context.quiet)
        {
            std::cout << "Memory limit: " << formatMemorySize(job_resources.allocated_memory_mb * 1024 * 1024)
                      << std::endl;
        }

        auto processing_context = std::make_shared<ProcessingContext>(temp, pressure, concentration, use_input_temp, use_input_pressure,
                                                                      thread_count,
                                                                      context.extension,
                                                                      context.max_file_size_mb,
                                                                      job_resources);

        // Single-file calculator in extract-au mode (9-column layout, no Status column).
        HighLevelEnergyCalculator calculator(
            processing_context, temp, concentration_m, sort_column, true);
        calculator.set_extract_au_format(true);
        calculator.set_use_input_temp(use_input_temp);
        calculator.set_use_input_pressure(use_input_pressure);
        calculator.set_use_input_concentration(use_input_concentration);
        calculator.set_low_vib_method(low_vib_method);
        calculator.set_ravib(ravib);

        // Process each file with the single-file engine (sequential or parallel).
        std::vector<HighLevelEnergyData> results;
        if (thread_count > 1 && filtered_files.size() > 1)
        {
            results = calculator.process_files_with_limits(filtered_files, thread_count, memory_limit_mb, context.quiet);
        }
        else
        {
            results.reserve(filtered_files.size());
            for (const auto& file : filtered_files)
            {
                if (g_shutdown_requested.load())
                    break;
                results.push_back(calculator.calculate_single_file_energy(file));
            }
            std::sort(results.begin(), results.end(), [&](const HighLevelEnergyData& a, const HighLevelEnergyData& b) {
                // 9-column extract-au mapping via the calculator flag.
                HighLevelEnergyCalculator& nc = const_cast<HighLevelEnergyCalculator&>(calculator);
                (void)nc;
                return calculator.compare_results_public(a, b, sort_column);
            });
        }

        if (processing_context->error_collector->has_errors())
        {
            auto errors = processing_context->error_collector->get_errors();
            if (!context.quiet)
            {
                std::cerr << "Errors encountered during processing:" << std::endl;
                for (const auto& error : errors)
                {
                    std::cerr << "  " << error << std::endl;
                }
            }
        }

        auto warnings = processing_context->error_collector->get_warnings();
        if (!warnings.empty() && !context.quiet)
        {
            std::cout << "Warnings:" << std::endl;
            for (const auto& warning : warnings)
            {
                std::cout << "  " << warning << std::endl;
            }
        }

        if (results.empty())
        {
            if (!context.quiet)
            {
                std::cout << "No valid " << context.extension << " files processed." << std::endl;
            }
            return processing_context->error_collector->has_errors() ? 1 : 0;
        }

        if (!context.quiet)
        {
            std::cout << "Successfully processed " << results.size() << "/" << filtered_files.size() << " files."
                      << std::endl;
        }

        // Print results based on output format (extract-au table has no Status column).
        if (output_format == "csv")
        {
            calculator.print_extract_au_csv_format(results, context.quiet);
        }
        else
        {
            calculator.print_extract_au_format_dynamic(results, context.quiet);
        }

        // Save results to file
        std::string file_extension = (output_format == "csv") ? ".csv" : ".results";
        std::string output_filename =
            HighLevelEnergyUtils::get_current_directory_name() + "-extract-au" + file_extension;

        std::ofstream output_file(output_filename);
        if (output_file.is_open())
        {
            if (output_format == "csv")
            {
                calculator.print_extract_au_csv_format(results, false, &output_file);
            }
            else
            {
                calculator.print_extract_au_format_dynamic(results, false, &output_file);
            }
            output_file.close();

            if (!context.quiet)
            {
                std::cout << "\nResults saved to: " << output_filename << std::endl;

                auto peak_memory = processing_context->memory_monitor->get_peak_usage();
                std::cout << "Peak memory usage: " << formatMemorySize(peak_memory) << std::endl;
            }
        }
        else
        {
            std::cerr << "Warning: Could not save results to " << output_filename << std::endl;
        }

        // Exit code from internal status bookkeeping (never printed as a column).
        bool has_error = processing_context->error_collector->has_errors();
        for (const auto& r : results)
        {
            if (r.status == "ERROR")
            {
                has_error = true;
                break;
            }
        }
        return has_error ? 1 : 0;
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
