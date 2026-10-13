/**
 * @file rmsd_io.cpp
 * @brief IO implementation (port of Rust io/xyz.rs, io/pdb.rs, io/mod.rs).
 * @author Le Nhan Pham
 * @date 2026
 */

#include "rmsd/rmsd_io.h"
#include "rmsd/elements.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef __linux__
#if defined(RMSD_HAVE_ZLIB) && __has_include(<zlib.h>)
#include <zlib.h>
#define RMSD_USE_ZLIB 1
#endif
#endif

namespace Rmsd
{

namespace
{

std::string lower_str(std::string s)
{
    for (auto& c : s)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string trim_str(const std::string& s)
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
    return s.substr(b, e - b);
}

std::string capitalize_str(const std::string& s)
{
    std::string t = trim_str(s);
    if (t.empty())
    {
        return t;
    }
    std::string out;
    out += static_cast<char>(std::toupper(static_cast<unsigned char>(t[0])));
    for (size_t i = 1; i < t.size(); ++i)
    {
        out += static_cast<char>(std::tolower(static_cast<unsigned char>(t[i])));
    }
    return out;
}

bool parse_double_str(const std::string& s, double& out)
{
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end != s.c_str() && *end == '\0';
}

std::string col_slice(const std::string& line, size_t a, size_t b)
{
    if (line.size() < b)
    {
        return "";
    }
    return line.substr(a, b - a);
}

std::string clean2(const std::string& s)
{
    std::string no_digits;
    for (char c : s)
    {
        if (!std::isdigit(static_cast<unsigned char>(c)))
        {
            no_digits += c;
        }
    }
    return capitalize_str(no_digits);
}

bool known_sym(const std::string& s)
{
    uint8_t z;
    return element_atomic_number(s, z);
}

bool parse_atom_line(const std::string& line, std::string& out)
{
    std::string atom_col = col_slice(line, 12, 16);
    std::string first2 = atom_col.size() >= 2 ? atom_col.substr(0, 2) : atom_col;
    std::string atom = clean2(first2);
    if (atom.size() == 2 && atom[0] == 'H')
    {
        atom = "H";
    }
    if (!atom.empty() && known_sym(atom))
    {
        out = atom;
        return true;
    }
    std::istringstream iss(line);
    std::vector<std::string> tok;
    std::string t;
    while (iss >> t)
    {
        tok.push_back(t);
    }
    if (tok.size() < 3)
    {
        return false;
    }
    std::string name = tok[2];
    std::string c0 = name.substr(0, 1);
    if (known_sym(capitalize_str(c0)))
    {
        out = capitalize_str(c0);
        return true;
    }
    if (name.size() >= 2)
    {
        std::string c1 = name.substr(1, 1);
        std::string up = c1;
        for (auto& c : up)
        {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (up == "H")
        {
            out = c1;
            return true;
        }
    }
    return false;
}

bool parse_coord_line(const std::string& line, Coord& out)
{
    std::string xs = trim_str(col_slice(line, 30, 38));
    std::string ys = trim_str(col_slice(line, 38, 46));
    std::string zs = trim_str(col_slice(line, 46, 54));
    double a, b, c;
    if (parse_double_str(xs, a) && parse_double_str(ys, b) && parse_double_str(zs, c))
    {
        out = {a, b, c};
        return true;
    }
    std::istringstream iss(line);
    std::vector<std::string> tok;
    std::string t;
    while (iss >> t)
    {
        tok.push_back(t);
    }
    for (size_t i = 0; i + 2 < tok.size(); ++i)
    {
        if (tok[i].find('.') != std::string::npos && tok[i + 1].find('.') != std::string::npos
            && tok[i + 2].find('.') != std::string::npos)
        {
            if (parse_double_str(tok[i], a) && parse_double_str(tok[i + 1], b) && parse_double_str(tok[i + 2], c))
            {
                out = {a, b, c};
                return true;
            }
        }
    }
    return false;
}

} // namespace

bool parse_format(const std::string& s, FileFormat& out)
{
    std::string t = lower_str(trim_str(s));
    if (t == "xyz")
    {
        out = FileFormat::Xyz;
        return true;
    }
    if (t == "pdb")
    {
        out = FileFormat::Pdb;
        return true;
    }
    return false;
}

bool detect_format(const std::string& path, FileFormat& fmt, bool& is_gzip, std::string& err)
{
    std::string name = lower_str(path);
    is_gzip = name.size() >= 3 && name.substr(name.size() - 3) == ".gz";
    std::string stem = is_gzip ? name.substr(0, name.size() - 3) : name;
    size_t dot = stem.rfind('.');
    std::string ext = (dot == std::string::npos) ? "" : stem.substr(dot + 1);
    if (!parse_format(ext, fmt))
    {
        err = "unsupported format: '" + ext + "' (expected xyz or pdb)";
        return false;
    }
    return true;
}

std::string read_text_file(const std::string& path, bool is_gzip)
{
    if (!is_gzip)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f)
        {
            throw RmsdException(RmsdErrorCode::Io, "cannot read file: " + path);
        }
        std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        return s;
    }
#ifdef RMSD_USE_ZLIB
    gzFile g = gzopen(path.c_str(), "rb");
    if (g == nullptr)
    {
        throw RmsdException(RmsdErrorCode::Io, "cannot read gzip file: " + path);
    }
    std::string out;
    char buf[65536];
    int n;
    while ((n = gzread(g, buf, sizeof(buf))) > 0)
    {
        out.append(buf, static_cast<size_t>(n));
    }
    gzclose(g);
    return out;
#else
    throw RmsdException(RmsdErrorCode::Io, "gzip (.gz) input requires a zlib build: " + path);
#endif
}

Molecule parse_xyz_text(const std::string& text)
{
    std::vector<std::string> lines;
    {
        std::istringstream iss(text);
        std::string l;
        while (std::getline(iss, l))
        {
            if (!l.empty() && l.back() == '\r')
            {
                l.pop_back();
            }
            lines.push_back(l);
        }
    }
    if (lines.empty())
    {
        throw RmsdException(RmsdErrorCode::Parse, "Could not obtain the number of atoms in the .xyz file.");
    }
    size_t n_atoms = 0;
    try
    {
        n_atoms = static_cast<size_t>(std::stoul(trim_str(lines[0])));
    }
    catch (...)
    {
        throw RmsdException(RmsdErrorCode::Parse, "Could not obtain the number of atoms in the .xyz file.");
    }
    std::vector<std::string> syms;
    std::vector<Coord> coords;
    // Proper record-oriented loop (empty lines skipped, stop after n_atoms records).
    syms.clear();
    coords.clear();
    size_t records = 0;
    for (size_t li = 2; li < lines.size() && records < n_atoms; ++li)
    {
        std::string line = trim_str(lines[li]);
        if (line.empty())
        {
            continue;
        }
        std::istringstream iss(line);
        std::vector<std::string> values;
        std::string t;
        while (iss >> t)
        {
            values.push_back(t);
        }
        std::string sym;
        std::vector<double> numbers;
        if (values.size() < 4)
        {
            std::string upper;
            for (char c : line)
            {
                if (std::isalpha(static_cast<unsigned char>(c)))
                {
                    upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                }
                else if (!upper.empty())
                {
                    break;
                }
            }
            sym = upper;
            std::string cur;
            for (size_t i = 0; i <= line.size(); ++i)
            {
                char c = i < line.size() ? line[i] : ' ';
                if (c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E' || std::isdigit(static_cast<unsigned char>(c)))
                {
                    cur += c;
                }
                else
                {
                    if (!cur.empty() && cur.find('.') != std::string::npos)
                    {
                        double v;
                        if (parse_double_str(cur, v))
                        {
                            numbers.push_back(v);
                        }
                    }
                    cur.clear();
                }
            }
        }
        else
        {
            sym = values[0];
            for (size_t i = 1; i < values.size(); ++i)
            {
                double v;
                if (parse_double_str(values[i], v))
                {
                    numbers.push_back(v);
                }
                else
                {
                    break;
                }
            }
        }
        if (numbers.size() >= 3)
        {
            coords.push_back({numbers[0], numbers[1], numbers[2]});
            syms.push_back(sym);
            ++records;
        }
        else
        {
            throw RmsdException(RmsdErrorCode::Parse,
                "Reading the .xyz file failed in line " + std::to_string(li + 1) + ". Please check the format.");
        }
    }
    Molecule mol;
    bool all_int = !syms.empty();
    for (const auto& s : syms)
    {
        char* end = nullptr;
        std::strtoul(s.c_str(), &end, 10);
        if (end == s.c_str() || *end != '\0')
        {
            all_int = false;
            break;
        }
    }
    if (all_int)
    {
        for (const auto& s : syms)
        {
            unsigned long n = std::strtoul(s.c_str(), nullptr, 10);
            if (n > 255)
            {
                throw RmsdException(RmsdErrorCode::Parse, "unknown atomic number: " + s);
            }
            std::string sym;
            if (!element_symbol(static_cast<uint8_t>(n), sym))
            {
                throw RmsdException(RmsdErrorCode::Parse, "unknown atomic number: " + s);
            }
            mol.atoms.push_back(static_cast<uint8_t>(n));
        }
    }
    else
    {
        for (const auto& s : syms)
        {
            uint8_t z;
            if (!element_atomic_number(capitalize_str(s), z))
            {
                throw RmsdException(RmsdErrorCode::Parse, "unknown element symbol: '" + s + "'");
            }
            mol.atoms.push_back(z);
        }
    }
    mol.coords = coords;
    return mol;
}

Molecule parse_pdb_text(const std::string& text, bool only_alpha)
{
    std::vector<std::string> symbols;
    std::vector<Coord> coords;
    std::vector<char> alpha;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.rfind("TER", 0) == 0 || line.rfind("END", 0) == 0)
        {
            break;
        }
        if (!(line.rfind("ATOM", 0) == 0 || line.rfind("HETATM", 0) == 0))
        {
            continue;
        }
        std::string atom;
        if (!parse_atom_line(line, atom))
        {
            throw RmsdException(RmsdErrorCode::Parse, "error: Parsing for atom line: " + line);
        }
        Coord c;
        if (!parse_coord_line(line, c))
        {
            throw RmsdException(RmsdErrorCode::Parse, "error: Parsing coordinates for line: " + line);
        }
        symbols.push_back(atom);
        coords.push_back(c);
        std::string atom_col = col_slice(line, 12, 16);
        std::string first2 = atom_col.size() >= 2 ? atom_col.substr(0, 2) : atom_col;
        char loc = atom_col.size() >= 3 ? atom_col[2] : ' ';
        alpha.push_back(clean2(first2) == "C" && loc == 'A' ? 1 : 0);
    }
    Molecule mol;
    for (const auto& s : symbols)
    {
        uint8_t z;
        if (!element_atomic_number(s, z))
        {
            throw RmsdException(RmsdErrorCode::Parse, "unknown element: '" + s + "'");
        }
        mol.atoms.push_back(z);
    }
    mol.coords = coords;
    if (only_alpha)
    {
        bool any = false;
        for (char a : alpha)
        {
            any = any || a;
        }
        if (!any)
        {
            throw RmsdException(
                RmsdErrorCode::Parse, "Trying to filter for alpha carbons, but couldn't find any");
        }
        Molecule f;
        for (size_t i = 0; i < mol.atoms.size(); ++i)
        {
            if (alpha[i])
            {
                f.atoms.push_back(mol.atoms[i]);
                f.coords.push_back(mol.coords[i]);
            }
        }
        return f;
    }
    return mol;
}

std::string write_xyz(const Molecule& mol, const std::string& title, int decimals)
{
    // Match Rust write_xyz: lines joined by \n (no trailing newline);
    // per-atom format "{:<2} {:>15.prec$} {:>15.prec$} {:>15.prec$}".
    std::vector<std::string> lines;
    lines.push_back(std::to_string(mol.size()));
    lines.push_back(title);
    for (size_t i = 0; i < mol.size(); ++i)
    {
        std::string sym;
        if (!element_symbol(mol.atoms[i], sym))
        {
            sym = "X";
        }
        std::ostringstream line;
        line << std::left;
        line.width(2);
        line << sym << std::right;
        line.setf(std::ios::fixed, std::ios::floatfield);
        line.precision(decimals);
        for (int k = 0; k < 3; ++k)
        {
            line << " ";
            line.width(15);
            line << mol.coords[i][k];
        }
        lines.push_back(line.str());
    }
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (i)
        {
            out += "\n";
        }
        out += lines[i];
    }
    return out;
}

Molecule load_molecule(const std::string& path, FileFormat fmt, bool is_gzip, bool only_alpha)
{
    std::string text = read_text_file(path, is_gzip);
    if (fmt == FileFormat::Xyz)
    {
        return parse_xyz_text(text);
    }
    return parse_pdb_text(text, only_alpha);
}

} // namespace Rmsd
