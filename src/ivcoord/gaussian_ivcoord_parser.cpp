#include "ivcoord/gaussian_ivcoord_parser.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Periodic table (index = atomic number, same table as coord_extractor.cpp)
// ---------------------------------------------------------------------------
static const std::vector<std::string> ELEMENT_SYMBOLS = {
    "",    "H",  "He", "Li", "Be", "B",  "C",  "N",  "O",  "F",  "Ne",
    "Na",  "Mg", "Al", "Si", "P",  "S",  "Cl", "Ar", "K",  "Ca", "Sc",
    "Ti",  "V",  "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn", "Ga", "Ge",
    "As",  "Se", "Br", "Kr", "Rb", "Sr", "Y",  "Zr", "Nb", "Mo", "Tc",
    "Ru",  "Rh", "Pd", "Ag", "Cd", "In", "Sn", "Sb", "Te", "I",  "Xe",
    "Cs",  "Ba", "La", "Ce", "Pr", "Nd", "Pm", "Sm", "Eu", "Gd", "Tb",
    "Dy",  "Ho", "Er", "Tm", "Yb", "Lu", "Hf", "Ta", "W",  "Re", "Os",
    "Ir",  "Pt", "Au", "Hg", "Tl", "Pb", "Bi", "Po", "At", "Rn", "Fr",
    "Ra",  "Ac", "Th", "Pa", "U",  "Np", "Pu", "Am", "Cm", "Bk", "Cf",
    "Es",  "Fm", "Md", "No", "Lr", "Rf", "Db", "Sg", "Bh", "Hs", "Mt",
    "Ds",  "Rg", "Cn", "Nh", "Fl", "Mc", "Lv", "Ts", "Og"
};

std::string GaussianIVCoordParser::atomic_symbol(int z)
{
    if (z >= 1 && z < static_cast<int>(ELEMENT_SYMBOLS.size()))
        return ELEMENT_SYMBOLS[z];
    return "X";
}

// ---------------------------------------------------------------------------
// can_parse: scan the first 60 lines for the Gaussian copyright marker
// ---------------------------------------------------------------------------
bool GaussianIVCoordParser::can_parse(const std::string& filepath)
{
    std::ifstream f(filepath);
    if (!f.is_open()) return false;

    std::string line;
    int count = 0;
    while (count < 60 && std::getline(f, line))
    {
        if (line.find("Gaussian, Inc.") != std::string::npos)
            return true;
        ++count;
    }
    return false;
}

// ---------------------------------------------------------------------------
// parse: extract geometry + displacement vectors
// ---------------------------------------------------------------------------
IVCoordData GaussianIVCoordParser::parse(const std::string& filepath)
{
    IVCoordData result;

    // -----------------------------------------------------------------------
    // Read entire file into lines
    // -----------------------------------------------------------------------
    std::ifstream f(filepath);
    if (!f.is_open())
    {
        result.error_message = "Cannot open file: " + filepath;
        return result;
    }

    std::vector<std::string> lines;
    {
        std::string line;
        while (std::getline(f, line))
        {
            // Normalize Windows line endings
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back(std::move(line));
        }
    }
    f.close();

    const int nlines = static_cast<int>(lines.size());

    // -----------------------------------------------------------------------
    // Step 1: Geometry — backward scan for last orientation block
    // -----------------------------------------------------------------------
    int geo_start = -1;
    for (int i = nlines - 1; i >= 0; --i)
    {
        if (lines[i].find("Standard orientation:") != std::string::npos ||
            lines[i].find("Input orientation:") != std::string::npos)
        {
            geo_start = i;
            break;
        }
    }

    if (geo_start == -1)
    {
        result.error_message = "No orientation section found";
        return result;
    }

    // Find the closing "----" delimiter (skip 5 header lines)
    int geo_end = -1;
    for (int i = geo_start + 5; i < nlines; ++i)
    {
        if (lines[i].find("----") != std::string::npos)
        {
            geo_end = i;
            break;
        }
    }

    if (geo_end == -1)
    {
        result.error_message = "No end delimiter found for orientation section";
        return result;
    }

    result.natom = geo_end - geo_start - 5;
    if (result.natom <= 0)
    {
        result.error_message = "Invalid atom count in orientation section";
        return result;
    }

    result.elements.resize(result.natom);
    result.coords.resize(result.natom);

    for (int i = geo_start + 5; i < geo_end; ++i)
    {
        const int idx = i - (geo_start + 5);
        std::istringstream ss(lines[i]);
        int    center, atomic_num, atom_type;
        double x, y, z;
        if (!(ss >> center >> atomic_num >> atom_type >> x >> y >> z))
        {
            result.error_message = "Failed to parse coordinate line: " + lines[i];
            return result;
        }
        result.elements[idx] = atomic_symbol(atomic_num);
        result.coords[idx]   = {x, y, z};
    }

    // -----------------------------------------------------------------------
    // Step 2: Displacements — forward scan for first imaginary mode
    //
    // Gaussian frequency blocks (3 modes per block):
    //
    //  Frequencies --   <f1>    <f2>    <f3>
    //  Red. masses --   ...
    //  Frc consts  --   ...
    //  IR Inten    --   ...
    //   Atom  AN      X      Y      Z        X      Y      Z        X      Y      Z
    //      1   6   d00    d01    d02      d10    d11    d12      d20    d21    d22
    //      ...
    //
    // Token layout per atom row:  [atom_idx, AN, X0,Y0,Z0, X1,Y1,Z1, X2,Y2,Z2]
    // For target column c: tokens at indices 2+c*3, 3+c*3, 4+c*3
    // -----------------------------------------------------------------------
    result.disps.assign(result.natom, {0.0, 0.0, 0.0});

    bool found = false;
    int  block_offset = 0;  // number of modes seen before current block

    for (int i = 0; i < nlines && !found; ++i)
    {
        // "Low frequencies ---" (lowercase f, 3 dashes) does NOT match
        // "Frequencies --" (capital F, 2 dashes) — safe to skip the find below.
        if (lines[i].find("Frequencies --") == std::string::npos)
            continue;

        // Parse frequency values from this block
        const std::string& fline = lines[i];
        const size_t       arrow = fline.find("Frequencies --");
        std::istringstream fss(fline.substr(arrow + 14));  // skip "Frequencies --"

        std::vector<double> block_freqs;
        double              v;
        while (fss >> v)
            block_freqs.push_back(v);

        // Find the first negative frequency in this block
        int target_col = -1;
        for (int c = 0; c < static_cast<int>(block_freqs.size()); ++c)
        {
            if (block_freqs[c] < 0.0)
            {
                target_col       = c;
                result.im_freq   = block_freqs[c];
                result.mode_index = block_offset + c;
                break;
            }
        }

        block_offset += static_cast<int>(block_freqs.size());

        if (target_col < 0)
            continue;  // No imaginary mode in this block, keep scanning

        // Find the "Atom  AN" displacement header line
        int atom_header = -1;
        for (int j = i + 1; j < nlines; ++j)
        {
            if (lines[j].find("Atom  AN") != std::string::npos ||
                lines[j].find("Atom AN")  != std::string::npos)
            {
                atom_header = j;
                break;
            }
            // Guard: stop if we reach the next frequency block
            if (lines[j].find("Frequencies --") != std::string::npos)
                break;
        }

        if (atom_header == -1)
        {
            result.error_message = "Could not find 'Atom  AN' displacement header";
            return result;
        }

        // Read natom displacement rows
        int atoms_read = 0;
        for (int j = atom_header + 1; atoms_read < result.natom && j < nlines; ++j)
        {
            const std::string& aline = lines[j];
            if (aline.empty())
                break;

            std::istringstream ass(aline);
            std::vector<std::string> tokens;
            std::string tok;
            while (ass >> tok)
                tokens.push_back(tok);

            // Minimum tokens needed: 2 (atom_idx, AN) + 3*(target_col+1) displacements
            const size_t needed = static_cast<size_t>(2 + target_col * 3 + 3);
            if (tokens.size() >= needed)
            {
                try
                {
                    result.disps[atoms_read][0] = std::stod(tokens[2 + target_col * 3]);
                    result.disps[atoms_read][1] = std::stod(tokens[3 + target_col * 3]);
                    result.disps[atoms_read][2] = std::stod(tokens[4 + target_col * 3]);
                }
                catch (const std::exception&)
                {
                    result.error_message = "Failed to parse displacement value on line: " + aline;
                    return result;
                }
            }

            ++atoms_read;
        }

        if (atoms_read != result.natom)
        {
            result.error_message = "Displacement block has fewer atom rows than expected";
            return result;
        }

        found = true;
    }

    if (!found)
    {
        result.error_message = "No imaginary frequency found in file";
        return result;
    }

    result.parsed_ok = true;
    return result;
}
