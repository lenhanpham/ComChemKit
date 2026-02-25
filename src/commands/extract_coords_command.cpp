#include "commands/extract_coords_command.h"
#include "commands/signal_handler.h"
#include <sstream>
#include <filesystem>
#include <iostream>
#include "extraction/coord_extractor.h"
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <atomic>
#include <memory>

extern std::atomic<bool> g_shutdown_requested;

std::string ExtractCoordsCommand::get_name() const {
    return "xyz";
}

std::string ExtractCoordsCommand::get_description() const {
    return "Extract coordinates from log files and organize XYZ files";
}

void ExtractCoordsCommand::parse_args(int argc, char* argv[], int& i, CommandContext& context) {
    std::string arg = argv[i];

    if (arg == "-f" || arg == "--files")
    {
        bool files_found = false;
        // Keep consuming arguments until we hit another option or the end
        while (++i < argc)
        {
            std::string file_arg = argv[i];

            // Check if the argument is another option
            if (file_arg.length() > 1 && file_arg[0] == '-')
            {
                // It's another option, so we're done with files.
                // Decrement i so the main loop can process this new option.
                i--;
                break;
            }

            files_found = true;

            // Process the argument, which may contain multiple filenames separated by commas
            std::replace(file_arg.begin(), file_arg.end(), ',', ' ');
            std::istringstream iss(file_arg);
            std::string        file;
            while (iss >> file)
            {
                if (!file.empty())
                {
                    // Trim whitespace (should be handled by stringstream, but good practice)
                    file.erase(0, file.find_first_not_of(" "));
                    file.erase(file.find_last_not_of(" ") + 1);

                    if (file.empty())
                        continue;

                    bool has_valid_extension = false;
                    for (const auto& ext : context.valid_extensions)
                    {
                        if (file.size() >= ext.size() && file.compare(file.size() - ext.size(), ext.size(), ext) == 0)
                        {
                            has_valid_extension = true;
                            break;
                        }
                    }

                    if (!has_valid_extension)
                    {
                        file += context.extension;
                    }

                    if (!std::filesystem::exists(file))
                    {
                        context.warnings.push_back("Specified file does not exist: " + file);
                    }

                    specific_files.push_back(file);
                }
            }
        }

        if (!files_found)
        {
            context.warnings.push_back("--files requires a filename or list of filenames");
        }
    }
}



int ExtractCoordsCommand::execute(const CommandContext& context)
{

    try
    {
        std::vector<std::string> log_files;
        if (!specific_files.empty())
        {
            // Use specified files
            log_files = specific_files;
            // Filter out invalid files and ensure they exist
            log_files.erase(std::remove_if(log_files.begin(),
                                           log_files.end(),
                                           [&](const std::string& file) {
                                               bool exists = std::filesystem::exists(file);
                                               if (!exists && !context.quiet)
                                               {
                                                   std::cerr << "Warning: File not found: " << file << std::endl;
                                               }
                                               return !exists || !JobCheckerUtils::is_valid_log_file(
                                                                     file, context.max_file_size_mb);
                                           }),
                            log_files.end());
        }
        else
        {
            // Find all log files
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
        }

        if (log_files.empty())
        {
            if (!context.quiet)
            {
                std::cout << "No valid " << context.extension << " files found." << std::endl;
            }
            return 0;
        }

        auto processing_context = std::make_shared<ProcessingContext>(298.15,  // Default temp, not used
                                                                      1.0,     // Default pressure, not used
                                                                      1000,    // Default conc, not used
                                                                      false,   // use_input_temp not relevant
                                                                      false,   // use_input_pressure not relevant
                                                                      context.requested_threads,
                                                                      context.extension,
                                                                      context.max_file_size_mb,
                                                                      context.job_resources);

        if (0 > 0)
        {
            processing_context->memory_monitor->set_memory_limit(0);
        }

        CoordExtractor extractor(processing_context, context.quiet);

        ExtractSummary summary = extractor.extract_coordinates(log_files);

        extractor.print_summary(summary, "Coordinate extraction");

        if (!context.quiet)
        {
            auto errors = processing_context->error_collector->get_errors();
            if (!errors.empty())
            {
                std::cout << "\nErrors encountered:" << std::endl;
                for (const auto& err : errors)
                {
                    std::cout << "  " << err << std::endl;
                }
            }
        }

        return summary.failed_files > 0 || !processing_context->error_collector->get_errors().empty() ? 1 : 0;
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

