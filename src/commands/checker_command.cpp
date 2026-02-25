#include "commands/checker_command.h"
#include "commands/signal_handler.h"
#include "job_management/job_checker.h"
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

CheckerCommand::CheckerCommand(CommandType type, std::string name, std::string desc)
    : type(type), name(name), desc(desc) {}

std::string CheckerCommand::get_name() const {
    return name;
}

std::string CheckerCommand::get_description() const {
    return desc;
}

void CheckerCommand::parse_args(int argc, char* argv[], int& i, CommandContext& context) {
    std::string arg = argv[i];

    if (arg == "--target-dir")
    {
        if (++i < argc)
        {
            target_dir = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: Target directory name required after --target-dir.");
        }
    }
    else if (arg == "--dir-suffix")
    {
        if (++i < argc)
        {
            dir_suffix = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: Directory suffix required after --dir-suffix.");
        }
    }
    else if (arg == "--show-details")
    {
        show_error_details = true;
    }
    else if (!arg.empty() && arg.front() == '-')
    {
        context.warnings.push_back("Warning: Unknown argument '" + arg + "' ignored.");
    }
}

int CheckerCommand::execute(const CommandContext& context) {
    CommandContext ctx = context;
    ctx.command = type; // Ensure the correct command type is set
    
    switch(type) {
        case CommandType::CHECK_DONE: return execute_check_done(ctx);
        case CommandType::CHECK_ERRORS: return execute_check_errors(ctx);
        case CommandType::CHECK_PCM: return execute_check_pcm(ctx);
        case CommandType::CHECK_IMAGINARY: return execute_check_imaginary(ctx);
        case CommandType::CHECK_ALL: return execute_check_all(ctx);
        default: return 1;
    }
}

int CheckerCommand::execute_check_done(const CommandContext& context)
{

    try
    {
        // Find log files using batch processing if specified
        std::vector<std::string> log_files;

        // If using default extension (.log), search for both .log and .out files (case-insensitive)
        bool is_log_ext = (context.extension.length() == 4 && std::tolower(context.extension[1]) == 'l' &&
                           std::tolower(context.extension[2]) == 'o' && std::tolower(context.extension[3]) == 'g');
        if (is_log_ext)
        {
            std::vector<std::string> extensions = {".log", ".out"};
            if (context.batch_size > 0)
            {
                log_files = findLogFiles(extensions, context.max_file_size_mb, context.batch_size);
            }
            else
            {
                log_files = findLogFiles(extensions, context.max_file_size_mb);
            }
        }
        else
        {
            if (context.batch_size > 0)
            {
                log_files = findLogFiles(context.extension, context.max_file_size_mb, context.batch_size);
            }
            else
            {
                log_files = findLogFiles(context.extension, context.max_file_size_mb);
            }
        }

        if (log_files.empty())
        {
            if (!context.quiet)
            {
                if (context.extension == ".log")
                {
                    std::cout << "No .log or .out files found in current directory." << std::endl;
                }
                else
                {
                    std::cout << "No " << context.extension << " files found in current directory." << std::endl;
                }
            }
            return 0;
        }

        // Create processing context for resource management
        auto processing_context =
            std::make_shared<ProcessingContext>(298.15,  // temp
                                                1.0,     // pressure
                                                1000,    // concentration
                                                false,
                                                false,
                                                context.requested_threads,
                                                context.extension,
                                                DEFAULT_MAX_FILE_SIZE_MB,  // Default max file size
                                                context.job_resources);

        // Set memory limit if specified
        if (0 > 0)
        {
            processing_context->memory_monitor->set_memory_limit(0);
        }

        // Create job checker
        JobChecker checker(processing_context, context.quiet, false);

        // Determine target directory suffix
        std::string current_dir_suffix = dir_suffix;
        if (!target_dir.empty())
        {
            // If custom target directory is specified, use it as full name
            current_dir_suffix = target_dir;
        }

        // Check completed jobs
        CheckSummary summary = checker.check_completed_jobs(log_files, current_dir_suffix);

        // Print summary if not quiet
        if (!context.quiet)
        {
            checker.print_summary(summary, "Job completion check");
        }

        // Print any resource usage information
        if (!context.quiet)
        {
            printResourceUsage(*processing_context, context.quiet);
        }

        return (summary.errors.empty()) ? 0 : 1;
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

int CheckerCommand::execute_check_errors(const CommandContext& context)
{

    try
    {
        // Find log files using batch processing if specified
        std::vector<std::string> log_files;

        // If using default extension (.log), search for both .log and .out files (case-insensitive)
        bool is_log_ext = (context.extension.length() == 4 && std::tolower(context.extension[1]) == 'l' &&
                           std::tolower(context.extension[2]) == 'o' && std::tolower(context.extension[3]) == 'g');
        if (is_log_ext)
        {
            std::vector<std::string> extensions = {".log", ".out", ".LOG", ".OUT", ".Log", ".Out"};
            if (context.batch_size > 0)
            {
                log_files = findLogFiles(extensions, context.max_file_size_mb, context.batch_size);
            }
            else
            {
                log_files = findLogFiles(extensions, context.max_file_size_mb);
            }
        }
        else
        {
            if (context.batch_size > 0)
            {
                log_files = findLogFiles(context.extension, context.max_file_size_mb, context.batch_size);
            }
            else
            {
                log_files = findLogFiles(context.extension, context.max_file_size_mb);
            }
        }

        if (log_files.empty())
        {
            if (!context.quiet)
            {
                if (context.extension == ".log")
                {
                    std::cout << "No .log or .out files found in current directory." << std::endl;
                }
                else
                {
                    std::cout << "No " << context.extension << " files found in current directory." << std::endl;
                }
            }
            return 0;
        }

        // Create processing context for resource management
        auto processing_context =
            std::make_shared<ProcessingContext>(298.15,  // temp
                                                1.0,     // pressure
                                                1000,    // concentration
                                                false,
                                                false,
                                                context.requested_threads,
                                                context.extension,
                                                DEFAULT_MAX_FILE_SIZE_MB,  // Default max file size
                                                context.job_resources);

        // Set memory limit if specified
        if (0 > 0)
        {
            processing_context->memory_monitor->set_memory_limit(0);
        }

        // Create job checker
        JobChecker checker(processing_context, context.quiet, show_error_details);

        // Determine target directory
        std::string current_target_dir = "errorJobs";
        if (!target_dir.empty())
        {
            current_target_dir = target_dir;
        }

        // Check error jobs
        CheckSummary summary = checker.check_error_jobs(log_files, current_target_dir);

        // Print summary if not quiet
        if (!context.quiet)
        {
            checker.print_summary(summary, "Error job check");
        }

        // Print any resource usage information
        if (!context.quiet)
        {
            printResourceUsage(*processing_context, context.quiet);
        }

        return (summary.errors.empty()) ? 0 : 1;
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

int CheckerCommand::execute_check_pcm(const CommandContext& context)
{

    try
    {
        // Find log files using batch processing if specified
        std::vector<std::string> log_files;

        // If using default extension (.log), search for both .log and .out files (case-insensitive)
        bool is_log_ext = (context.extension.length() == 4 && std::tolower(context.extension[1]) == 'l' &&
                           std::tolower(context.extension[2]) == 'o' && std::tolower(context.extension[3]) == 'g');
        if (is_log_ext)
        {
            std::vector<std::string> extensions = {".log", ".out", ".LOG", ".OUT", ".Log", ".Out"};
            if (context.batch_size > 0)
            {
                log_files = findLogFiles(extensions, context.max_file_size_mb, context.batch_size);
            }
            else
            {
                log_files = findLogFiles(extensions, context.max_file_size_mb);
            }
        }
        else
        {
            if (context.batch_size > 0)
            {
                log_files = findLogFiles(context.extension, context.max_file_size_mb, context.batch_size);
            }
            else
            {
                log_files = findLogFiles(context.extension, context.max_file_size_mb);
            }
        }

        if (log_files.empty())
        {
            if (!context.quiet)
            {
                if (context.extension == ".log")
                {
                    std::cout << "No .log or .out files found in current directory." << std::endl;
                }
                else
                {
                    std::cout << "No " << context.extension << " files found in current directory." << std::endl;
                }
            }
            return 0;
        }

        // Create processing context for resource management
        auto processing_context =
            std::make_shared<ProcessingContext>(298.15,  // temp
                                                1.0,     // pressure
                                                1000,    // concentration
                                                false,
                                                false,
                                                context.requested_threads,
                                                context.extension,
                                                DEFAULT_MAX_FILE_SIZE_MB,  // Default max file size
                                                context.job_resources);

        // Set memory limit if specified
        if (0 > 0)
        {
            processing_context->memory_monitor->set_memory_limit(0);
        }

        // Create job checker
        JobChecker checker(processing_context, context.quiet, false);

        // Determine target directory
        std::string current_target_dir = "PCMMkU";
        if (!target_dir.empty())
        {
            current_target_dir = target_dir;
        }

        // Check PCM failed jobs
        CheckSummary summary = checker.check_pcm_failures(log_files, current_target_dir);

        // Print summary if not quiet
        if (!context.quiet)
        {
            checker.print_summary(summary, "PCM failure check");
        }

        // Print any resource usage information
        if (!context.quiet)
        {
            printResourceUsage(*processing_context, context.quiet);
        }

        return (summary.errors.empty()) ? 0 : 1;
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

int CheckerCommand::execute_check_all(const CommandContext& context)
{

    try
    {
        // Find log files using batch processing if specified
        std::vector<std::string> log_files;

        // If using default extension (.log), search for both .log and .out files (case-insensitive)
        bool is_log_ext = (context.extension.length() == 4 && std::tolower(context.extension[1]) == 'l' &&
                           std::tolower(context.extension[2]) == 'o' && std::tolower(context.extension[3]) == 'g');
        if (is_log_ext)
        {
            std::vector<std::string> extensions = {".log", ".out", ".LOG", ".OUT", ".Log", ".Out"};
            if (context.batch_size > 0)
            {
                log_files = findLogFiles(extensions, context.max_file_size_mb, context.batch_size);
            }
            else
            {
                log_files = findLogFiles(extensions, context.max_file_size_mb);
            }
        }
        else
        {
            if (context.batch_size > 0)
            {
                log_files = findLogFiles(context.extension, context.max_file_size_mb, context.batch_size);
            }
            else
            {
                log_files = findLogFiles(context.extension, context.max_file_size_mb);
            }
        }

        if (log_files.empty())
        {
            if (!context.quiet)
            {
                if (context.extension == ".log")
                {
                    std::cout << "No .log or .out files found in current directory." << std::endl;
                }
                else
                {
                    std::cout << "No " << context.extension << " files found in current directory." << std::endl;
                }
            }
            return 0;
        }

        // Create processing context for resource management
        auto processing_context =
            std::make_shared<ProcessingContext>(298.15,  // temp
                                                1.0,     // pressure
                                                1000,    // concentration
                                                false,
                                                false,
                                                context.requested_threads,
                                                context.extension,
                                                DEFAULT_MAX_FILE_SIZE_MB,  // Default max file size
                                                context.job_resources);

        // Set memory limit if specified
        if (0 > 0)
        {
            processing_context->memory_monitor->set_memory_limit(0);
        }

        // Create job checker
        JobChecker checker(processing_context, context.quiet, show_error_details);

        // Run all checks
        CheckSummary summary = checker.check_all_job_types(log_files);

        // Print any resource usage information
        if (!context.quiet)
        {
            printResourceUsage(*processing_context, context.quiet);
        }

        return (summary.errors.empty()) ? 0 : 1;
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

int CheckerCommand::execute_check_imaginary(const CommandContext& context)
{

    try
    {
        std::vector<std::string> log_files;

        // If using default extension (.log), search for both .log and .out files (case-insensitive)
        bool is_log_ext = (context.extension.length() == 4 && std::tolower(context.extension[1]) == 'l' &&
                           std::tolower(context.extension[2]) == 'o' && std::tolower(context.extension[3]) == 'g');
        if (is_log_ext)
        {
            std::vector<std::string> extensions = {".log", ".out"};
            if (context.batch_size > 0)
            {
                log_files = findLogFiles(extensions, context.max_file_size_mb, context.batch_size);
            }
            else
            {
                log_files = findLogFiles(extensions, context.max_file_size_mb);
            }
        }
        else
        {
            if (context.batch_size > 0)
            {
                log_files = findLogFiles(context.extension, context.max_file_size_mb, context.batch_size);
            }
            else
            {
                log_files = findLogFiles(context.extension, context.max_file_size_mb);
            }
        }

        if (log_files.empty())
        {
            if (!context.quiet)
            {
                if (context.extension == ".log")
                {
                    std::cout << "No .log or .out files found in current directory." << std::endl;
                }
                else
                {
                    std::cout << "No " << context.extension << " files found in current directory." << std::endl;
                }
            }
            return 0;
        }

        auto processing_context = std::make_shared<ProcessingContext>(298.15,
                                                                      1.0,
                                                                      1000,
                                                                      false,
                                                                      false,
                                                                      context.requested_threads,
                                                                      context.extension,
                                                                      DEFAULT_MAX_FILE_SIZE_MB,
                                                                      context.job_resources);

        if (0 > 0)
        {
            processing_context->memory_monitor->set_memory_limit(0);
        }

        JobChecker checker(processing_context, context.quiet, false);

        std::string target_dir_suffix = "imaginary_freqs";
        if (!target_dir.empty())
        {
            target_dir_suffix = target_dir;
        }

        CheckSummary summary = checker.check_imaginary_frequencies(log_files, target_dir_suffix);

        if (!context.quiet)
        {
            checker.print_summary(summary, "Imaginary frequency check");
        }

        if (!context.quiet)
        {
            printResourceUsage(*processing_context, context.quiet);
        }

        return (summary.errors.empty()) ? 0 : 1;
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

