/**
 * @file struct_list.cpp
 * @brief List implementation (port of Rust list.rs).
 * @author Le Nhan Pham
 * @date 2026
 */

#include "rmsd/struct_list.h"
#include "rmsd/rmsd_types.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Rmsd
{

namespace fs = std::filesystem;

std::vector<std::string> parse_list_text(const std::string& text)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : text)
    {
        if (c == ',' || std::isspace(static_cast<unsigned char>(c)))
        {
            if (!cur.empty())
            {
                out.push_back(cur);
                cur.clear();
            }
        }
        else
        {
            cur += c;
        }
    }
    if (!cur.empty())
    {
        out.push_back(cur);
    }
    return out;
}

std::vector<ListEntry> load_list_file(const std::string& list_path)
{
    std::ifstream f(list_path, std::ios::binary);
    if (!f)
    {
        throw RmsdException(RmsdErrorCode::Io, "I/O error reading list '" + list_path + "'");
    }
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::vector<std::string> tokens = parse_list_text(text);
    if (tokens.empty())
    {
        throw RmsdException(RmsdErrorCode::Args, "list '" + list_path + "' contains no structure names");
    }
    fs::path lp(list_path);
    fs::path parent = lp.parent_path();
    bool has_parent = !parent.empty();
    std::vector<ListEntry> out;
    for (const auto& label : tokens)
    {
        fs::path raw(label);
        fs::path resolved;
        if (has_parent && raw.is_relative())
        {
            resolved = parent / raw;
        }
        else
        {
            resolved = raw;
        }
        out.push_back({label, resolved.string()});
    }
    return out;
}

std::string csv_path_for(const std::string& list_path)
{
    fs::path p(list_path);
    p.replace_extension(".csv");
    return p.string();
}

} // namespace Rmsd
