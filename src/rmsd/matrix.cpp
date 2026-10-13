/**
 * @file matrix.cpp
 * @brief Matrix implementation (port of Rust matrix.rs, std::thread pool).
 * @author Le Nhan Pham
 * @date 2026
 */

#include "rmsd/matrix.h"
#include <atomic>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <thread>

extern std::atomic<bool> g_shutdown_requested;

namespace Rmsd
{

uint64_t estimate_molecule_bytes(const Molecule& m)
{
    return static_cast<uint64_t>(m.size()) * 32 + 64;
}

uint64_t estimate_set_bytes(const std::vector<Molecule>& mols)
{
    uint64_t s = 0;
    for (const auto& m : mols)
    {
        s += estimate_molecule_bytes(m);
    }
    return s;
}

void check_budget(const std::vector<Molecule>& mols, uint64_t cap)
{
    uint64_t needed = estimate_set_bytes(mols);
    if (needed > cap)
    {
        throw RmsdException(RmsdErrorCode::Budget,
            "molecule set needs ~" + std::to_string(needed) + " bytes but the memory budget is "
                + std::to_string(cap) + " bytes (raise --max-mem or split the run with --rows)");
    }
}

uint64_t parse_mem_size(const std::string& s)
{
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
    {
        ++b;
    }
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
    {
        --e;
    }
    std::string t = s.substr(b, e - b);
    size_t cut = t.size();
    for (size_t i = 0; i < t.size(); ++i)
    {
        if (std::isalpha(static_cast<unsigned char>(t[i])))
        {
            cut = i;
            break;
        }
    }
    std::string num = t.substr(0, cut);
    std::string suf = t.substr(cut);
    for (auto& c : suf)
    {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    uint64_t n = 0;
    try
    {
        size_t pos = 0;
        unsigned long long v = std::stoull(num, &pos);
        if (pos != num.size())
        {
            throw RmsdException(RmsdErrorCode::Args, "invalid memory size: '" + s + "'");
        }
        n = v;
    }
    catch (const RmsdException&)
    {
        throw;
    }
    catch (...)
    {
        throw RmsdException(RmsdErrorCode::Args, "invalid memory size: '" + s + "'");
    }
    uint64_t mult = 0;
    bool known = true;
    if (suf.empty() || suf == "B")
    {
        mult = 1;
    }
    else if (suf == "K" || suf == "KB")
    {
        mult = 1024;
    }
    else if (suf == "M" || suf == "MB")
    {
        mult = 1024ULL * 1024ULL;
    }
    else if (suf == "G" || suf == "GB")
    {
        mult = 1024ULL * 1024ULL * 1024ULL;
    }
    else
    {
        known = false;
    }
    if (!known)
    {
        throw RmsdException(RmsdErrorCode::Args, "invalid memory size: '" + s + "'");
    }
    if (mult != 0 && n > std::numeric_limits<uint64_t>::max() / mult)
    {
        throw RmsdException(RmsdErrorCode::Args, "memory size overflows: '" + s + "'");
    }
    return n * mult;
}

std::pair<size_t, size_t> parse_row_range(const std::string& s, size_t n)
{
    size_t colon = s.find(':');
    if (colon == std::string::npos)
    {
        throw RmsdException(RmsdErrorCode::Args, "rows must look like START:END, got '" + s + "'");
    }
    try
    {
        size_t a = static_cast<size_t>(std::stoul(s.substr(0, colon)));
        size_t b = static_cast<size_t>(std::stoul(s.substr(colon + 1)));
        if (a >= b || b > n)
        {
            throw RmsdException(RmsdErrorCode::Args, "row range '" + s + "' out of bounds");
        }
        return {a, b};
    }
    catch (const RmsdException&)
    {
        throw;
    }
    catch (...)
    {
        throw RmsdException(RmsdErrorCode::Args, "invalid row range: '" + s + "'");
    }
}

size_t block_rows(uint64_t row_bytes, uint64_t max_mem)
{
    uint64_t target = max_mem / 64;
    uint64_t rows = target / (row_bytes == 0 ? 1 : row_bytes);
    if (rows < 1)
    {
        rows = 1;
    }
    if (rows > 4096)
    {
        rows = 4096;
    }
    return static_cast<size_t>(rows);
}

MatrixOpt pairwise_rows(const std::vector<Molecule>& mols, const std::vector<std::string>& labels,
    const Options& opts, size_t range_start, size_t range_end, const ProgressFn& progress, unsigned thread_count)
{
    size_t n = labels.size();
    if (mols.size() > labels.size() || range_end > mols.size())
    {
        throw RmsdException(RmsdErrorCode::Args, "row range exceeds loaded molecules");
    }
    for (size_t k = 0; k < range_end; ++k)
    {
        if (mols[k].size() != mols[0].size())
        {
            throw RmsdException(RmsdErrorCode::Ragged, "structures have different sizes; a matrix needs uniform sizes");
        }
    }
    size_t total = n * (n + 1) / 2;
    size_t s = range_start;
    auto row_pairs = [](size_t i, size_t sb) { return (i + 1) * (i + 2) / 2 - sb * (sb + 1) / 2; };
    size_t base_done = s * (s + 1) / 2;
    size_t count = (range_end > range_start) ? (range_end - range_start) : 0;
    MatrixOpt out(count, std::vector<std::optional<double>>(n, std::nullopt));
    if (count == 0)
    {
        return out;
    }
    unsigned workers = thread_count == 0 ? 1 : thread_count;
    if (workers > count)
    {
        workers = static_cast<unsigned>(count);
    }
    std::atomic<size_t> next(0);
    std::atomic<bool> failed(false);
    std::string first_err;
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < workers; ++t)
    {
        (void)t;
        pool.emplace_back([&]() {
            while (true)
            {
                if (failed.load() || g_shutdown_requested.load())
                {
                    return;
                }
                size_t r = next.fetch_add(1);
                if (r >= count)
                {
                    return;
                }
                size_t i = range_start + r;
                try
                {
                    for (size_t j = 0; j <= i; ++j)
                    {
                        if (i == j)
                        {
                            out[r][j] = 0.0;
                        }
                        else
                        {
                            Output o = compute_rmsd(mols[i], mols[j], opts, labels[i], labels[j]);
                            out[r][j] = o.rmsd;
                        }
                    }
                    if (progress)
                    {
                        progress(base_done + row_pairs(i, s), total);
                    }
                }
                catch (const std::exception& e)
                {
                    if (!failed.exchange(true))
                    {
                        first_err = e.what();
                    }
                    return;
                }
            }
        });
    }
    for (auto& th : pool)
    {
        th.join();
    }
    if (failed.load())
    {
        throw RmsdException(RmsdErrorCode::Args, first_err.empty() ? "pairwise computation failed" : first_err);
    }
    if (g_shutdown_requested.load())
    {
        throw RmsdException(RmsdErrorCode::Args, "Processing interrupted by shutdown signal");
    }
    return out;
}

MatrixOpt pairwise_matrix(const std::vector<Molecule>& mols, const std::vector<std::string>& labels,
    const Options& opts, const ProgressFn& progress, unsigned thread_count)
{
    return pairwise_rows(mols, labels, opts, 0, mols.size(), progress, thread_count);
}

std::string csv_header(const std::vector<std::string>& labels)
{
    std::string out = ",";
    for (size_t i = 0; i < labels.size(); ++i)
    {
        if (i)
        {
            out += ",";
        }
        out += labels[i];
    }
    out += "\n";
    return out;
}

std::string rows_to_csv(const std::vector<std::string>& labels, const MatrixOpt& rows, size_t offset)
{
    std::ostringstream out;
    out.setf(std::ios::fixed, std::ios::floatfield);
    out.precision(6);
    std::string s;
    for (size_t r = 0; r < rows.size(); ++r)
    {
        s += labels[offset + r];
        for (const auto& cell : rows[r])
        {
            s += ",";
            if (cell.has_value())
            {
                std::ostringstream v;
                v.setf(std::ios::fixed, std::ios::floatfield);
                v.precision(6);
                v << cell.value();
                s += v.str();
            }
        }
        for (size_t k = rows[r].size(); k < labels.size(); ++k)
        {
            s += ",";
        }
        s += "\n";
    }
    return s;
}

std::string to_csv(const std::vector<std::string>& labels, const MatrixOpt& matrix)
{
    return csv_header(labels) + rows_to_csv(labels, matrix, 0);
}

std::pair<size_t, size_t> resume_state(const std::string& text, const std::vector<std::string>& labels)
{
    size_t cut = text.rfind('\n');
    if (cut == std::string::npos)
    {
        throw RmsdException(RmsdErrorCode::Args, "existing output has no complete lines");
    }
    ++cut;
    std::string body = text.substr(0, cut);
    std::istringstream iss(body);
    std::string header;
    if (!std::getline(iss, header))
    {
        throw RmsdException(RmsdErrorCode::Args, "existing output is empty");
    }
    std::string expected = csv_header(labels);
    if (!expected.empty() && expected.back() == '\n')
    {
        expected.pop_back();
    }
    if (header != expected)
    {
        throw RmsdException(RmsdErrorCode::Args,
            "existing output header does not match this structure list (use --overwrite)");
    }
    size_t done = 0;
    std::string line;
    while (std::getline(iss, line))
    {
        size_t comma = line.find(',');
        if (comma == std::string::npos)
        {
            throw RmsdException(RmsdErrorCode::Args, "malformed row in existing output: '" + line + "'");
        }
        std::string label = line.substr(0, comma);
        if (done >= labels.size() || label != labels[done])
        {
            throw RmsdException(RmsdErrorCode::Args,
                "existing output rows do not match this structure list (use --overwrite)");
        }
        ++done;
    }
    return {done, cut};
}

std::pair<std::vector<std::string>, std::vector<std::string>> merge_partials(
    const std::vector<std::string>& texts)
{
    if (texts.empty())
    {
        throw RmsdException(RmsdErrorCode::Args, "nothing to merge");
    }
    auto first_line = [](const std::string& t) {
        size_t p = t.find('\n');
        return p == std::string::npos ? t : t.substr(0, p);
    };
    std::string header = first_line(texts[0]);
    if (header.empty() || header[0] != ',')
    {
        throw RmsdException(RmsdErrorCode::Args, "partial output missing label header");
    }
    std::vector<std::string> labels;
    {
        std::string rest = header.substr(1);
        std::istringstream iss(rest);
        std::string tok;
        while (std::getline(iss, tok, ','))
        {
            labels.push_back(tok);
        }
    }
    auto pos = [&](const std::string& label) -> size_t {
        for (size_t i = 0; i < labels.size(); ++i)
        {
            if (labels[i] == label)
            {
                return i;
            }
        }
        throw RmsdException(RmsdErrorCode::Args, "unknown row label '" + label + "'");
    };
    std::vector<std::optional<std::string>> rows(labels.size(), std::nullopt);
    for (const auto& text : texts)
    {
        std::istringstream iss(text);
        std::string h;
        std::getline(iss, h);
        if (h != header)
        {
            throw RmsdException(RmsdErrorCode::Args, "partial outputs have mismatching headers");
        }
        std::string line;
        while (std::getline(iss, line))
        {
            if (line.empty())
            {
                continue;
            }
            size_t comma = line.find(',');
            if (comma == std::string::npos)
            {
                throw RmsdException(RmsdErrorCode::Args, "malformed row: '" + line + "'");
            }
            size_t idx = pos(line.substr(0, comma));
            if (rows[idx].has_value())
            {
                throw RmsdException(RmsdErrorCode::Args, "duplicate row '" + line.substr(0, comma) + "'");
            }
            rows[idx] = line;
        }
    }
    std::vector<std::string> out;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (!rows[i].has_value())
        {
            throw RmsdException(RmsdErrorCode::Args, "missing row '" + labels[i] + "' in partial outputs");
        }
        out.push_back(rows[i].value());
    }
    return {labels, out};
}

} // namespace Rmsd
