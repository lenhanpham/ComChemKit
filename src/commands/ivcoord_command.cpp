#include "commands/ivcoord_command.h"
#include "commands/signal_handler.h"
#include "input_gen/parameter_parser.h"
#include "utilities/config_manager.h"
#include "ivcoord/ivcoord_runner.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern std::atomic<bool> g_shutdown_requested;
extern ConfigManager     g_config_manager;

// ---------------------------------------------------------------------------
// Default parameter-file search (mirrors create_input_command.cpp)
// ---------------------------------------------------------------------------
static std::string find_default_ivcoord_param_file()
{
    const std::string DEFAULT_NAME = ".ivcoord_parameters.params";
    const std::string ALT_NAME     = "ivcoord_parameters.params";

    std::vector<std::string> candidates;

    candidates.push_back("./" + DEFAULT_NAME);
    candidates.push_back("./" + ALT_NAME);

    std::string exe_dir = ConfigUtils::get_executable_directory();
    if (!exe_dir.empty())
    {
        candidates.push_back(exe_dir + "/" + DEFAULT_NAME);
        candidates.push_back(exe_dir + "/" + ALT_NAME);
    }

    std::string home_dir = g_config_manager.get_user_home_directory();
    if (!home_dir.empty())
    {
        candidates.push_back(home_dir + "/" + DEFAULT_NAME);
        candidates.push_back(home_dir + "/" + ALT_NAME);
    }

#ifndef _WIN32
    candidates.push_back("/etc/cck/" + ALT_NAME);
    candidates.push_back("/usr/local/etc/" + ALT_NAME);
#endif

    for (const auto& p : candidates)
    {
        if (std::filesystem::exists(p))
            return p;
    }

    return "";  // Not found; caller will use hard-coded defaults
}

// ---------------------------------------------------------------------------
// ICommand interface
// ---------------------------------------------------------------------------
std::string IVCoordCommand::get_name() const
{
    return "ivcoord";
}

std::string IVCoordCommand::get_description() const
{
    return "Displace geometry along imaginary normal modes and write XYZ files";
}

// ---------------------------------------------------------------------------
// parse_args
// ---------------------------------------------------------------------------
void IVCoordCommand::parse_args(int argc, char* argv[], int& i, CommandContext& context)
{
    std::string arg = argv[i];

    if (arg == "--iamp")
    {
        if (++i < argc)
        {
            try
            {
                ivc_amp = std::stod(argv[i]);
            }
            catch (const std::exception&)
            {
                context.warnings.push_back("Error: --iamp requires a numeric value");
                --i;
            }
        }
        else
        {
            context.warnings.push_back("Error: --iamp requires a value");
        }
    }
    else if (arg == "--idirection")
    {
        if (++i < argc)
        {
            try
            {
                ivc_direction = std::stoi(argv[i]);
                if (ivc_direction < -1 || ivc_direction > 1)
                {
                    context.warnings.push_back("Warning: --idirection should be -1, 0, or +1; got " +
                                               std::string(argv[i]));
                    ivc_direction = 1;
                }
            }
            catch (const std::exception&)
            {
                context.warnings.push_back("Error: --idirection requires an integer value (-1, 0, or +1)");
                --i;
            }
        }
        else
        {
            context.warnings.push_back("Error: --idirection requires a value");
        }
    }
    else if (arg == "--param-file")
    {
        std::string param_path;

        if (++i < argc)
        {
            std::string next = argv[i];
            if (!next.empty() && next[0] == '-')
            {
                // Next token is another option — use default search
                param_path = find_default_ivcoord_param_file();
                --i;
            }
            else
            {
                param_path = next;
            }
        }
        else
        {
            param_path = find_default_ivcoord_param_file();
        }

        if (param_path.empty())
        {
            context.warnings.push_back(
                "Warning: No ivcoord parameter file found; using built-in defaults");
            return;
        }

        ParameterParser parser;
        if (parser.loadFromFile(param_path))
        {
            ivc_amp       = parser.getDouble("iamp",       ivc_amp);
            ivc_direction = parser.getInt   ("idirection", ivc_direction);

            if (ivc_direction < -1 || ivc_direction > 1)
            {
                context.warnings.push_back(
                    "Warning: idirection in param file should be -1, 0, or +1; reset to 1");
                ivc_direction = 1;
            }
        }
        else
        {
            context.warnings.push_back("Warning: Could not load parameter file: " + param_path);
        }
    }
    else if (arg == "--gen-ivcoord-params")
    {
        const std::string filename = "ivcoord_parameters.params";
        std::ofstream out(filename);
        if (out.is_open())
        {
            out << "# ivcoord parameter file\n";
            out << "# Displace geometry along imaginary normal modes\n\n";
            out << "# Displacement amplitude (default: 1.0)\n";
            out << "iamp = 1.0\n\n";
            out << "# Direction: +1 = plus only, -1 = minus only, 0 = both (default: 1)\n";
            out << "idirection = 1\n";
            out.close();
            std::cout << "Generated parameter template: " << filename << std::endl;
            std::cout << "Use with: cck ivcoord --param-file " << filename << std::endl;
        }
        else
        {
            std::cerr << "Error: Failed to create " << filename << std::endl;
        }
        exit(0);
    }
    else if (!arg.empty() && arg[0] != '-')
    {
        // Positional argument — treat as input file
        context.files.push_back(arg);
    }
}

// ---------------------------------------------------------------------------
// execute
// ---------------------------------------------------------------------------
int IVCoordCommand::execute(const CommandContext& context)
{
    // -----------------------------------------------------------------------
    // Build input file list
    // -----------------------------------------------------------------------
    std::vector<std::string> log_files;

    if (!context.files.empty())
    {
        for (const auto& f : context.files)
        {
            if (std::filesystem::exists(f))
                log_files.push_back(f);
            else
                std::cerr << "[WARN]  File not found: " << f << std::endl;
        }
    }
    else
    {
        // Auto-glob current directory
        for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::current_path()))
        {
            if (!entry.is_regular_file()) continue;
            const std::string ext = entry.path().extension().string();
            if (ext == ".log" || ext == ".out" || ext == ".LOG" || ext == ".OUT")
                log_files.push_back(entry.path().string());
        }
        std::sort(log_files.begin(), log_files.end());
    }

    if (log_files.empty())
    {
        std::cerr << "No input files found." << std::endl;
        return 1;
    }

    // -----------------------------------------------------------------------
    // Print any parse-time warnings
    // -----------------------------------------------------------------------
    if (!context.quiet)
    {
        for (const auto& w : context.warnings)
            std::cerr << w << std::endl;
    }

    // -----------------------------------------------------------------------
    // Thread pool (same lock-free pattern as CoordExtractor)
    // -----------------------------------------------------------------------
    std::atomic<size_t> file_index{0};
    std::mutex          console_mutex;
    std::atomic<int>    exit_code{0};

    unsigned int num_threads = context.requested_threads > 0
                                   ? context.requested_threads
                                   : std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 1;
    if (num_threads > static_cast<unsigned int>(log_files.size()))
        num_threads = static_cast<unsigned int>(log_files.size());

    if (!context.quiet)
    {
        std::cout << "Using: " << num_threads << " thread(s)"
                  << " to process " << log_files.size() << " file(s)"
                  << "  amp=" << ivc_amp
                  << "  direction=" << ivc_direction << std::endl;
    }

    // Capture command parameters for lambda (cannot capture `this` by copy)
    const double amp       = ivc_amp;
    const int    direction = ivc_direction;

    auto worker = [&]()
    {
        size_t idx;
        while ((idx = file_index.fetch_add(1)) < log_files.size())
        {
            if (g_shutdown_requested.load()) break;

            const std::string& file = log_files[idx];
            std::filesystem::path fp(file);

            // Determine output directory
            std::filesystem::path parent = std::filesystem::absolute(fp).parent_path();
            std::string           parent_name = parent.filename().string();
            if (parent_name.empty() || parent_name == ".")
                parent_name = std::filesystem::current_path().filename().string();

            std::filesystem::path out_dir = parent / (parent_name + "_ivcoord");

            std::string stem = fp.stem().string();

            // Parse
            IVCoordData data = IVCoordRunner::extract(file);

            if (!data.parsed_ok)
            {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cerr << "[WARN]  " << std::left << std::setw(40) << fp.filename().string()
                          << " -> " << data.error_message << std::endl;
                exit_code.store(1);
                continue;
            }

            // Ensure output directory exists
            try
            {
                std::filesystem::create_directories(out_dir);
            }
            catch (const std::exception& e)
            {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cerr << "[WARN]  " << fp.filename().string()
                          << " -> Failed to create output directory: " << e.what() << std::endl;
                exit_code.store(1);
                continue;
            }

            // Apply displacement and write XYZ file(s)
            try
            {
                IVCoordRunner::apply_displacement(data, amp, direction, out_dir, stem);

                std::lock_guard<std::mutex> lock(console_mutex);
                const std::string rel_dir = out_dir.filename().string();

                if (direction == 0)
                {
                    std::cout << "[+- OK] " << std::left << std::setw(40) << fp.filename().string()
                              << " ->  " << rel_dir << "/" << stem << "_p.xyz"
                              << "  (freq: " << std::fixed << std::setprecision(2)
                              << data.im_freq << " cm\u207b\u00b9"
                              << ", amp: " << amp << ")" << std::endl;
                    std::cout << "[+- OK] " << std::left << std::setw(40) << fp.filename().string()
                              << " ->  " << rel_dir << "/" << stem << "_m.xyz" << std::endl;
                }
                else
                {
                    const std::string tag = (direction > 0) ? "[+ OK]  " : "[- OK]  ";
                    std::cout << tag << std::left << std::setw(40) << fp.filename().string()
                              << " ->  " << rel_dir << "/" << stem << ".xyz"
                              << "  (freq: " << std::fixed << std::setprecision(2)
                              << data.im_freq << " cm\u207b\u00b9"
                              << ", amp: " << amp << ")" << std::endl;
                }
            }
            catch (const std::exception& e)
            {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cerr << "[FAIL]  " << fp.filename().string()
                          << " -> " << e.what() << std::endl;
                exit_code.store(1);
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (unsigned int t = 0; t < num_threads; ++t)
        threads.emplace_back(worker);

    for (auto& t : threads)
        t.join();

    return exit_code.load();
}
