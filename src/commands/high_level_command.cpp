#include "commands/high_level_command.h"
#include "commands/signal_handler.h"
#include "utilities/config_manager.h"
#include "high_level/high_level_energy.h"
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

extern ConfigManager g_config_manager;

HighLevelCommand::HighLevelCommand(CommandType type, std::string name, std::string desc)
    : type(type), name(name), desc(desc) {
    if (g_config_manager.is_config_loaded()) {
        temp               = g_config_manager.get_default_temperature();
        concentration      = static_cast<int>(g_config_manager.get_default_concentration() * 1000);
        sort_column        = g_config_manager.get_int("default_sort_column");
        output_format      = g_config_manager.get_default_output_format();
        use_input_temp     = g_config_manager.get_bool("use_input_temp");
        memory_limit_mb    = g_config_manager.get_size_t("memory_limit_mb");
    }
}

std::string HighLevelCommand::get_name() const {
    return name;
}

std::string HighLevelCommand::get_description() const {
    return desc;
}

void HighLevelCommand::parse_args(int argc, char* argv[], int& i, CommandContext& context) {
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

int HighLevelCommand::execute(const CommandContext& context) {
    CommandContext ctx = context;
    ctx.command = type; 
    
    if (type == CommandType::HIGH_LEVEL_KJ) {
        return execute_kj(ctx);
    } else {
        return execute_au(ctx);
    }
}

int HighLevelCommand::execute_kj(const CommandContext& context)
{

    try
    {
        // Validate that we're in a high-level directory
        if (!HighLevelEnergyUtils::is_valid_high_level_directory(context.extension, context.max_file_size_mb))
        {
            std::cerr << "Error: This command must be run from a directory containing high-level .log files"
                      << std::endl;
            std::cerr << "       with a parent directory containing low-level thermal data." << std::endl;
            return 1;
        }

        // Find and count log files using batch processing if specified
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

            // Debug thread calculation with detailed reasoning
            unsigned int hardware_cores = std::thread::hardware_concurrency();
            std::cout << "System: " << hardware_cores << " cores detected" << std::endl;
            std::cout << "Requested: " << context.requested_threads << " threads";
            if (context.requested_threads == hardware_cores / 2)
            {
                std::cout << " (default: half cores)";
            }
            std::cout << std::endl;

            // Show environment
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

        // Create enhanced processing context
        double concentration_m = static_cast<double>(concentration) / 1000.0;

        // Determine optimal thread count for high-level processing
        unsigned int requested_threads =
            context.requested_threads > 0
                ? context.requested_threads
                : calculateSafeThreadCount(context.requested_threads, 100, context.job_resources);
        unsigned int thread_count = std::min(requested_threads, static_cast<unsigned int>(filtered_files.size()));

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

        // Create processing context with resource management using ALL CommandContext parameters
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

        auto processing_context = std::make_shared<ProcessingContext>(temp,
                                                                      pressure,
                                                                      concentration,
                                                                      use_input_temp,
                                                                      use_input_pressure,
                                                                      thread_count,
                                                                      context.extension,
                                                                      context.max_file_size_mb,
                                                                      job_resources);

        // Create enhanced high-level energy calculator (KJ format)
        HighLevelEnergyCalculator calculator(
            processing_context, temp, concentration_m, sort_column, false);

        // Use parallel processing for better performance
        std::vector<HighLevelEnergyData> results;
        if (thread_count > 1)
        {
            results = calculator.process_directory_parallel(context.extension, thread_count, context.quiet);
        }
        else
        {
            results = calculator.process_directory(context.extension);
        }

        // Check for errors during processing
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

        // Display warnings if any
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

        // Print results based on output format
        if (output_format == "csv")
        {
            calculator.print_gibbs_csv_format(results, context.quiet);
        }
        else
        {
            calculator.print_gibbs_format_dynamic(results, context.quiet);
        }

        // Save results to file
        std::string file_extension = (output_format == "csv") ? ".csv" : ".results";
        std::string output_filename =
            HighLevelEnergyUtils::get_current_directory_name() + "-highLevel-kJ" + file_extension;

        std::ofstream output_file(output_filename);
        if (output_file.is_open())
        {
            // Print results to file (include metadata)
            if (output_format == "csv")
            {
                calculator.print_gibbs_csv_format(results, false, &output_file);
            }
            else
            {
                calculator.print_gibbs_format_dynamic(results, false, &output_file);
            }
            output_file.close();

            if (!context.quiet)
            {
                std::cout << "\nResults saved to: " << output_filename << std::endl;

                // Print resource usage summary
                auto peak_memory = processing_context->memory_monitor->get_peak_usage();
                std::cout << "Peak memory usage: " << formatMemorySize(peak_memory) << std::endl;
            }
        }
        else
        {
            std::cerr << "Warning: Could not save results to " << output_filename << std::endl;
        }

        return processing_context->error_collector->has_errors() ? 1 : 0;
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

int HighLevelCommand::execute_au(const CommandContext& context)
{

    try
    {
        // Validate that we're in a high-level directory
        if (!HighLevelEnergyUtils::is_valid_high_level_directory(context.extension, context.max_file_size_mb))
        {
            std::cerr << "Error: This command must be run from a directory containing high-level .log files"
                      << std::endl;
            std::cerr << "       with a parent directory containing low-level thermal data." << std::endl;
            return 1;
        }

        // Find and count log files using batch processing if specified
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

            // Debug thread calculation with detailed reasoning
            unsigned int hardware_cores = std::thread::hardware_concurrency();
            std::cout << "System: " << hardware_cores << " cores detected" << std::endl;
            std::cout << "Requested: " << context.requested_threads << " threads";
            if (context.requested_threads == hardware_cores / 2)
            {
                std::cout << " (default: half cores)";
            }
            std::cout << std::endl;

            // Show environment
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

        // Create enhanced processing context
        double concentration_m = static_cast<double>(concentration) / 1000.0;

        // Determine optimal thread count for high-level processing
        unsigned int requested_threads =
            context.requested_threads > 0
                ? context.requested_threads
                : calculateSafeThreadCount(context.requested_threads, 100, context.job_resources);
        unsigned int thread_count = std::min(requested_threads, static_cast<unsigned int>(filtered_files.size()));

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

        // Create processing context with resource management using ALL CommandContext parameters
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

        // Create enhanced high-level energy calculator (AU format)
        HighLevelEnergyCalculator calculator(
            processing_context, temp, concentration_m, sort_column, true);

        // Use parallel processing for better performance
        std::vector<HighLevelEnergyData> results;
        if (thread_count > 1)
        {
            results = calculator.process_directory_parallel(context.extension, thread_count, context.quiet);
        }
        else
        {
            results = calculator.process_directory(context.extension);
        }

        // Check for errors during processing
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

        // Display warnings if any
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

        // Print results based on output format
        if (output_format == "csv")
        {
            calculator.print_components_csv_format(results, context.quiet);
        }
        else
        {
            calculator.print_components_format_dynamic(results, context.quiet);
        }

        // Save results to file
        std::string file_extension = (output_format == "csv") ? ".csv" : ".results";
        std::string output_filename =
            HighLevelEnergyUtils::get_current_directory_name() + "-highLevel-au" + file_extension;

        std::ofstream output_file(output_filename);
        if (output_file.is_open())
        {
            // Print results to file (include metadata)
            if (output_format == "csv")
            {
                calculator.print_components_csv_format(results, false, &output_file);
            }
            else
            {
                calculator.print_components_format_dynamic(results, false, &output_file);
            }
            output_file.close();

            if (!context.quiet)
            {
                std::cout << "\nResults saved to: " << output_filename << std::endl;

                // Print resource usage summary
                auto peak_memory = processing_context->memory_monitor->get_peak_usage();
                std::cout << "Peak memory usage: " << formatMemorySize(peak_memory) << std::endl;
            }
        }
        else
        {
            std::cerr << "Warning: Could not save results to " << output_filename << std::endl;
        }

        return processing_context->error_collector->has_errors() ? 1 : 0;
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

