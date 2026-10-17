/**
 * @file rmsd_command.cpp
 * @brief RmsdCommand implementation (port of Rust bin/rmsd.rs).
 * @author Le Nhan Pham
 * @date 2026
 */

#include "commands/rmsd_command.h"
#include "extraction/qc_extractor.h"
#include "rmsd/matrix.h"
#include "rmsd/rmsd_io.h"
#include "rmsd/struct_list.h"
#include "utilities/config_manager.h"
#include <atomic>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#ifndef _WIN32
#include <unistd.h>
#endif

extern std::atomic<bool> g_shutdown_requested;
extern ConfigManager g_config_manager;

namespace fs = std::filesystem;

RmsdCommand::RmsdCommand()
{
    memory_limit_mb_ = g_config_manager.get_size_t("memory_limit_mb");
}

std::string RmsdCommand::get_name() const
{
    return "rmsd";
}

std::string RmsdCommand::get_description() const
{
    return "Calculate RMSD between molecular structures (XYZ/PDB), pair and matrix modes";
}

void RmsdCommand::parse_args(int argc, char* argv[], int& i, CommandContext& context)
{
    std::string arg = argv[i];
    auto need_value = [&](std::string& out, const std::string& flag) {
        if (++i < argc)
        {
            out = argv[i];
        }
        else
        {
            context.warnings.push_back("Error: " + flag + " requires a value");
        }
    };
    if (arg == "-r" || arg == "--rotation")
    {
        std::string v;
        need_value(v, arg);
        if (!v.empty())
        {
            std::string l = v;
            for (auto& c : l)
            {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (l == "kabsch" || l == "quaternion" || l == "none")
            {
                rotation_str_ = l;
            }
            else
            {
                context.warnings.push_back("Error: --rotation must be kabsch|quaternion|none");
            }
        }
    }
    else if (arg == "--reorder")
    {
        options_.do_reorder = true;
    }
    else if (arg == "--reorder-method")
    {
        std::string v;
        need_value(v, arg);
        if (!v.empty())
        {
            std::string l = v;
            for (auto& c : l)
            {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (l == "none" || l == "hungarian" || l == "inertia-hungarian" || l == "distance" || l == "brute")
            {
                reorder_method_str_ = l;
            }
            else
            {
                context.warnings.push_back("Error: --reorder-method must be none|hungarian|inertia-hungarian|distance|brute");
            }
        }
    }
    else if (arg == "--use-reflections")
    {
        options_.reflections = Rmsd::Reflections::All;
    }
    else if (arg == "--use-reflections-keep-stereo")
    {
        options_.reflections = Rmsd::Reflections::KeepStereo;
    }
    else if (arg == "--only-alpha-carbons")
    {
        only_alpha_ = true;
    }
    else if (arg == "-n" || arg == "--ignore-hydrogen" || arg == "--no-hydrogen")
    {
        options_.filter.kind = Rmsd::FilterKind::NoHydrogen;
    }
    else if (arg == "--remove-idx" || arg == "--add-idx")
    {
        bool is_remove = (arg == "--remove-idx");
        options_.filter.kind = is_remove ? Rmsd::FilterKind::RemoveIdx : Rmsd::FilterKind::AddIdx;
        options_.filter.indices.clear();
        while (i + 1 < argc && argv[i + 1][0] != '-')
        {
            // Accept comma-separated groups; stop (without consuming) at the
            // first non-integer token so file positionals survive.
            std::string tok = argv[i + 1];
            std::vector<std::string> parts;
            {
                std::string cur;
                for (char c : tok)
                {
                    if (c == ',')
                    {
                        parts.push_back(cur);
                        cur.clear();
                    }
                    else
                    {
                        cur += c;
                    }
                }
                parts.push_back(cur);
            }
            bool all_int = !parts.empty();
            for (const auto& p : parts)
            {
                if (p.empty())
                {
                    all_int = false;
                    break;
                }
                for (char c : p)
                {
                    if (!std::isdigit(static_cast<unsigned char>(c)))
                    {
                        all_int = false;
                        break;
                    }
                }
            }
            if (!all_int)
            {
                break;
            }
            ++i;
            for (const auto& p : parts)
            {
                options_.filter.indices.push_back(static_cast<size_t>(std::stoul(p)));
            }
        }
        if (options_.filter.indices.empty())
        {
            context.warnings.push_back("Error: " + arg + " requires integer indices");
        }
    }
    else if (arg == "--format")
    {
        need_value(format_str_, arg);
        format_forced_ = !format_str_.empty();
    }
    else if (arg == "--from-list")
    {
        need_value(from_list_, arg);
    }
    else if (arg == "--rows")
    {
        need_value(rows_, arg);
    }
    else if (arg == "--out")
    {
        need_value(out_, arg);
    }
    else if (arg == "--max-mem")
    {
        need_value(max_mem_str_, arg);
    }
    else if (arg == "--memory-limit")
    {
        if (++i < argc)
        {
            try
            {
                memory_limit_mb_ = static_cast<size_t>(std::stoul(argv[i]));
            }
            catch (...)
            {
                context.warnings.push_back("Error: --memory-limit requires a numeric value (MB)");
            }
        }
    }
    else if (arg == "--overwrite")
    {
        overwrite_ = true;
    }
    else if (arg == "--merge-csv")
    {
        need_value(merge_csv_, arg);
    }
    else if (arg == "-p" || arg == "--print" || arg == "--output")
    {
        options_.print_rotated = true;
    }
    else if (arg == "--print-only-rmsd-atoms")
    {
        options_.print_only_rmsd_atoms = true;
    }
    else if (!arg.empty() && arg[0] == '-')
    {
        // Leave to common options / other parsers; record only if truly unknown later.
        // Do not consume i.
    }
    else
    {
        context.files.push_back(arg);
    }
}

static Rmsd::FileFormat resolve_format(const std::string& path, const std::string& forced, bool& gz)
{
    if (!forced.empty())
    {
        Rmsd::FileFormat f;
        if (!Rmsd::parse_format(forced, f))
        {
            throw Rmsd::RmsdException(Rmsd::RmsdErrorCode::Format, "unsupported format: '" + forced + "'");
        }
        std::string l = path;
        for (auto& c : l)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        gz = l.size() >= 3 && l.substr(l.size() - 3) == ".gz";
        return f;
    }
    Rmsd::FileFormat f;
    std::string err;
    if (!Rmsd::detect_format(path, f, gz, err))
    {
        throw Rmsd::RmsdException(Rmsd::RmsdErrorCode::Format, err);
    }
    return f;
}

int RmsdCommand::run_pair(const CommandContext& context) const
{
    if (context.files.size() < 2)
    {
        std::cerr << "rmsd: expected FILE_A FILE_B\n";
        return 1;
    }
    std::string fa = context.files[0];
    std::string fb = context.files[1];
    bool gza = false, gzb = false;
    Rmsd::FileFormat fma, fmb;
    try
    {
        fma = resolve_format(fa, format_str_, gza);
        fmb = resolve_format(fb, format_str_, gzb);
    }
    catch (const std::exception& e)
    {
        std::cerr << "rmsd: " << e.what() << "\n";
        return 1;
    }
    if (fma != fmb)
    {
        std::cerr << "rmsd: both inputs must use the same format\n";
        return 1;
    }
    if (only_alpha_ && fma != Rmsd::FileFormat::Pdb)
    {
        std::cerr << "rmsd: --only-alpha-carbons only applies to PDB format\n";
        return 1;
    }
    Rmsd::Options opts = options_;
    try
    {
        opts.rotation = Rmsd::parse_rotation(rotation_str_);
        opts.reorder = Rmsd::parse_reorder_method(reorder_method_str_);
    }
    catch (const std::exception& e)
    {
        std::cerr << "rmsd: " << e.what() << "\n";
        return 1;
    }
    if (opts.do_reorder && opts.reorder == Rmsd::ReorderMethod::None)
    {
        std::cerr << "rmsd: --reorder requires a --reorder-method other than 'none'\n";
        return 1;
    }
    if (options_.reflections == Rmsd::Reflections::All && opts.reflections == Rmsd::Reflections::KeepStereo)
    {
        std::cerr << "rmsd: --use-reflections conflicts with --use-reflections-keep-stereo\n";
        return 1;
    }
    try
    {
        Rmsd::Molecule p = Rmsd::load_molecule(fa, fma, gza, only_alpha_);
        Rmsd::Molecule q = Rmsd::load_molecule(fb, fmb, gzb, only_alpha_);
        Rmsd::Output out = Rmsd::compute_rmsd(p, q, opts, fa, fb);
        if (out.has_rotated)
        {
            std::ostringstream title;
            title << out.title << " with RMSD " << std::fixed << std::setprecision(8) << out.rmsd;
            std::cout << Rmsd::write_xyz(out.rotated, title.str(), 8) << "\n";
        }
        else
        {
            std::cout << std::setprecision(17) << out.rmsd << "\n";
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "rmsd: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

int RmsdCommand::run_matrix(const CommandContext& context) const
{
    std::string list_path = from_list_;
    if (list_path.empty())
    {
        if (context.files.size() == 1)
        {
            list_path = context.files[0];
        }
        else
        {
            std::cerr << "rmsd: expected FILE_A FILE_B, a single LIST file, or --from-list LIST\n";
            return 1;
        }
    }
    else if (!context.files.empty())
    {
        std::cerr << "rmsd: --from-list cannot be combined with FILE_A/FILE_B\n";
        return 1;
    }
    if (options_.print_rotated)
    {
        std::cerr << "rmsd: --print is only available in pair mode (two structures)\n";
        return 1;
    }
    Rmsd::Options opts = options_;
    try
    {
        opts.rotation = Rmsd::parse_rotation(rotation_str_);
        opts.reorder = Rmsd::parse_reorder_method(reorder_method_str_);
    }
    catch (const std::exception& e)
    {
        std::cerr << "rmsd: " << e.what() << "\n";
        return 1;
    }
    if (opts.do_reorder && opts.reorder == Rmsd::ReorderMethod::None)
    {
        std::cerr << "rmsd: --reorder requires a --reorder-method other than 'none'\n";
        return 1;
    }
    std::vector<Rmsd::ListEntry> entries;
    try
    {
        entries = Rmsd::load_list_file(list_path);
    }
    catch (const std::exception& e)
    {
        std::cerr << "rmsd: " << e.what() << "\n";
        return 1;
    }
    size_t rs = 0, re = entries.size();
    if (!rows_.empty())
    {
        try
        {
            auto pr = Rmsd::parse_row_range(rows_, entries.size());
            rs = pr.first;
            re = pr.second;
        }
        catch (const std::exception& e)
        {
            std::cerr << "rmsd: " << e.what() << "\n";
            return 1;
        }
    }
    // Extract-consistent threads + memory (agreed scheme).
    unsigned threads = calculateSafeThreadCount(context.requested_threads, static_cast<unsigned>(entries.size()),
        context.job_resources);
    size_t mem_mb = calculateSafeMemoryLimit(memory_limit_mb_, threads, context.job_resources);
    uint64_t max_mem = static_cast<uint64_t>(mem_mb) * 1024ULL * 1024ULL;
    if (!max_mem_str_.empty())
    {
        try
        {
            max_mem = Rmsd::parse_mem_size(max_mem_str_);
        }
        catch (const std::exception& e)
        {
            std::cerr << "rmsd: " << e.what() << "\n";
            return 1;
        }
    }
    std::vector<Rmsd::Molecule> mols;
    std::vector<std::string> labels;
    mols.reserve(re);
    labels.reserve(re);
    try
    {
        for (size_t k = 0; k < re; ++k)
        {
            bool gz = false;
            Rmsd::FileFormat f = resolve_format(entries[k].path, format_str_, gz);
            if (only_alpha_ && f != Rmsd::FileFormat::Pdb)
            {
                std::cerr << "rmsd: --only-alpha-carbons only applies to PDB format\n";
                return 1;
            }
            mols.push_back(Rmsd::load_molecule(entries[k].path, f, gz, only_alpha_));
            labels.push_back(entries[k].label);
        }
        Rmsd::check_budget(mols, max_mem);
    }
    catch (const std::exception& e)
    {
        std::cerr << "rmsd: " << e.what() << "\n";
        return 1;
    }
    std::vector<std::string> full_labels;
    for (const auto& e : entries)
    {
        full_labels.push_back(e.label);
    }
    std::string out_path = out_.empty() ? Rmsd::csv_path_for(list_path) : out_;
    try
    {
        fs::path op(out_path);
        if (op.has_parent_path() && !op.parent_path().empty())
        {
            fs::create_directories(op.parent_path());
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "rmsd: cannot create output directory: " << e.what() << "\n";
        return 1;
    }
    std::string expected_header = Rmsd::csv_header(full_labels);
    size_t start = rs;
    bool exists = fs::exists(out_path);
    if (exists && !overwrite_)
    {
        std::ifstream f(out_path, std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        try
        {
            auto st = Rmsd::resume_state(text, full_labels);
            if (st.first < rs || st.first > re)
            {
                std::cerr << "rmsd: existing output covers rows 0.." << st.first
                          << " which does not extend this range (use --overwrite)\n";
                return 1;
            }
            if (st.second != text.size())
            {
                std::FILE* fh = std::fopen(out_path.c_str(), "r+b");
                if (fh != nullptr)
                {
#if defined(_WIN32)
                    _chsize(_fileno(fh), static_cast<long>(st.second));
#else
                    int trunc_rc = ftruncate(fileno(fh), static_cast<off_t>(st.second));
                    (void)trunc_rc;
#endif
                    std::fclose(fh);
                }
            }
            start = st.first;
        }
        catch (const std::exception& e)
        {
            std::cerr << "rmsd: cannot resume '" << out_path << "': " << e.what() << "\n";
            return 1;
        }
    }
    size_t total = entries.size() * (entries.size() + 1) / 2;
    size_t base_done = start * (start + 1) / 2;
    std::atomic<size_t> shown(base_done);
    auto report = [&](size_t done, size_t) {
        size_t prev = shown.exchange(done > shown.load() ? done : shown.load());
        auto pct = [&](size_t d) { return d * 100 / (total == 0 ? 1 : total); };
        if (pct(done) > pct(prev) || done == total)
        {
            if (!context.quiet)
            {
                std::cerr << "[" << done << "/" << total << "] pairs computed\n";
            }
        }
    };
    if (start >= re)
    {
        if (exists)
        {
            // Archive the existing output with a timestamp before recalculating,
            // so previous data is preserved and the user is informed that a
            // recalculation is about to happen instead of silently stopping.
            auto now = std::chrono::system_clock::now();
            std::time_t now_t = std::chrono::system_clock::to_time_t(now);
            std::tm now_tm{};
#if defined(_WIN32)
            localtime_s(&now_tm, &now_t);
#else
            localtime_r(&now_t, &now_tm);
#endif
            char tsbuf[64];
            std::strftime(tsbuf, sizeof(tsbuf), "%Y%m%d_%H%M%S", &now_tm);
            fs::path p(out_path);
            std::string stem = p.stem().string();
            std::string ext = p.extension().string();
            // Pick a unique archived name (counter suffix if timestamp collides).
            fs::path archived;
            int counter = 0;
            do
            {
                archived = p.parent_path() /
                           (stem + "_" + tsbuf + (counter > 0 ? "_" + std::to_string(counter) : "") + ext);
                ++counter;
            } while (fs::exists(archived));
            std::error_code ec;
            fs::rename(out_path, archived, ec);
            if (ec)
            {
                std::cerr << "rmsd: cannot archive existing output '" << out_path
                          << "': " << ec.message() << "\n";
                return 1;
            }
            std::cout << "rmsd: output '" << out_path << "' already complete ("
                      << entries.size() << "x" << entries.size() << ", " << total
                      << " pairs); archived to '" << archived.string()
                      << "' and recalculating\n";
            start = rs;
            exists = false;
        }
        else
        {
            std::cout << "output '" << out_path << "' already complete (" << entries.size()
                      << "x" << entries.size() << ", " << total << " pairs)\n";
            return 0;
        }
    }
    base_done = start * (start + 1) / 2;
    shown.store(base_done);
    uint64_t row_bytes = (static_cast<uint64_t>(entries.size()) + 1) * 14;
    size_t block = Rmsd::block_rows(row_bytes, max_mem);
    bool need_header = !exists || overwrite_;
    try
    {
        for (size_t cur = start; cur < re;)
        {
            if (g_shutdown_requested.load())
            {
                std::cerr << "rmsd: interrupted\n";
                return 1;
            }
            size_t next = std::min(cur + block, re);
            Rmsd::MatrixOpt rows = Rmsd::pairwise_rows(mols, labels, opts, cur, next, report, threads);
            if (need_header)
            {
                std::ofstream of(out_path, std::ios::binary | std::ios::trunc);
                of << expected_header;
                need_header = false;
            }
            std::string chunk = Rmsd::rows_to_csv(full_labels, rows, cur);
            std::ofstream of(out_path, std::ios::binary | std::ios::app);
            of << chunk;
            cur = next;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "rmsd: " << e.what() << "\n";
        return 1;
    }
    std::cout << "wrote " << out_path << " (" << (re - rs) << "/" << entries.size() << " rows, " << total << " pairs)\n";
    return 0;
}

int RmsdCommand::run_merge() const
{
    return 1;
}

int RmsdCommand::execute(const CommandContext& context)
{
    for (const auto& w : context.warnings)
    {
        if (!context.quiet)
        {
            std::cout << w << "\n";
        }
    }
    // --merge-csv mode: positionals are partial CSVs.
    if (!merge_csv_.empty())
    {
        if (!from_list_.empty())
        {
            std::cerr << "rmsd: --merge-csv cannot be combined with --from-list\n";
            return 1;
        }
        if (context.files.size() < 2)
        {
            std::cerr << "rmsd: --merge-csv needs at least two partial CSV files\n";
            return 1;
        }
        try
        {
            std::vector<std::string> texts;
            for (const auto& p : context.files)
            {
                std::ifstream f(p, std::ios::binary);
                if (!f)
                {
                    std::cerr << "rmsd: cannot read '" << p << "'\n";
                    return 1;
                }
                texts.emplace_back((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            }
            auto merged = Rmsd::merge_partials(texts);
            std::string csv = Rmsd::csv_header(merged.first);
            for (const auto& r : merged.second)
            {
                csv += r + "\n";
            }
            std::ofstream of(merge_csv_, std::ios::binary | std::ios::trunc);
            of << csv;
            std::cout << "merged " << texts.size() << " partials into " << merge_csv_ << " (" << merged.first.size()
                      << "x" << merged.first.size() << ")\n";
            return 0;
        }
        catch (const std::exception& e)
        {
            std::cerr << "rmsd: cannot merge: " << e.what() << "\n";
            return 1;
        }
    }
    if (!from_list_.empty())
    {
        return run_matrix(context);
    }
    if (context.files.size() >= 2)
    {
        // Heuristic: two existing structure files = pair mode; else list mode handled in run_matrix.
        bool a_exists = fs::exists(context.files[0]);
        bool b_exists = fs::exists(context.files[1]);
        if (a_exists && b_exists)
        {
            return run_pair(context);
        }
        // Fall through: e.g. --rows chunk with list in files[0] is handled by run_matrix single-positional rule.
        if (context.files.size() == 2)
        {
            return run_pair(context);
        }
    }
    if (context.files.size() == 1 || !from_list_.empty())
    {
        return run_matrix(context);
    }
    if (context.files.size() >= 2)
    {
        return run_pair(context);
    }
    std::cerr << "rmsd: expected FILE_A FILE_B, a single LIST file, or --from-list LIST\n";
    return 1;
}
