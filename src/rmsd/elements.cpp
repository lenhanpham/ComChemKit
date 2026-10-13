/**
 * @file elements.cpp
 * @brief Element tables.
 * @author Le Nhan Pham
 * @date 2026
 */

#include "rmsd/elements.h"
#include <cctype>

namespace Rmsd
{

namespace
{

const double MASS_TABLE[117] = {
    0.0, 1.00797, 4.00260, 6.941, 9.01218, 10.81, 12.011, 14.0067, 15.9994, 18.998403, 20.179, 22.98977,
    24.305, 26.98154, 28.0855, 30.97376, 32.06, 35.453, 39.948, 39.0983, 40.08, 44.9559, 47.90, 50.9415,
    51.996, 54.9380, 55.847, 58.9332, 58.70, 63.546, 65.38, 69.72, 72.59, 74.9216, 78.96, 79.904, 83.80,
    85.4678, 87.62, 88.9059, 91.22, 92.9064, 95.94, 98.0, 101.07, 102.9055, 106.4, 107.868, 112.41, 114.82,
    118.69, 121.75, 127.60, 126.9045, 131.30, 132.9054, 137.33, 138.9055, 140.12, 140.9077, 144.24, 145.0,
    150.4, 151.96, 157.25, 158.9254, 162.50, 164.9304, 167.26, 168.9342, 173.04, 174.967, 178.49, 180.9479,
    183.85, 186.207, 190.2, 192.22, 195.09, 196.9665, 200.59, 204.37, 207.2, 208.9804, 209.0, 210.0, 222.0,
    223.0, 226.0254, 227.0278, 232.0381, 231.0359, 238.029, 237.0482, 242.0, 243.0, 247.0, 247.0, 251.0,
    252.0, 257.0, 258.0, 250.0, 260.0, 261.0, 262.0, 263.0, 262.0, 255.0, 256.0, 269.0, 272.0, 277.0
};

const char* SYMBOL_TABLE[117] = {
    "", "H", "He", "Li", "Be", "B", "C", "N", "O", "F", "Ne", "Na", "Mg", "Al", "Si", "P", "S", "Cl",
    "Ar", "K", "Ca", "Sc", "Ti", "V", "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn", "Ga", "Ge", "As", "Se",
    "Br", "Kr", "Rb", "Sr", "Y", "Zr", "Nb", "Mo", "Tc", "Ru", "Rh", "Pd", "Ag", "Cd", "In", "Sn", "Sb",
    "Te", "I", "Xe", "Cs", "Ba", "La", "Ce", "Pr", "Nd", "Pm", "Sm", "Eu", "Gd", "Tb", "Dy", "Ho", "Er",
    "Tm", "Yb", "Lu", "Hf", "Ta", "W", "Re", "Os", "Ir", "Pt", "Au", "Hg", "Tl", "Pb", "Bi", "Po", "At",
    "Rn", "Fr", "Ra", "Ac", "Th", "Pa", "U", "Np", "Pu", "Am", "Cm", "Bk", "Cf", "Es", "Fm", "Md", "No",
    "Lr", "Rf", "Db", "Sg", "Bh", "Hs", "Mt", "Ds", "Rg", "Cn", "", "Uuq", "", "Uuh"
};

} // namespace

double element_mass(uint8_t z)
{
    if (z < 117)
    {
        return MASS_TABLE[z];
    }
    return 0.0;
}

bool element_symbol(uint8_t z, std::string& out)
{
    if (z < 117 && SYMBOL_TABLE[z][0] != '\0')
    {
        out = SYMBOL_TABLE[z];
        return true;
    }
    return false;
}

bool element_atomic_number(const std::string& sym, uint8_t& out)
{
    size_t b = 0;
    while (b < sym.size() && std::isspace(static_cast<unsigned char>(sym[b])))
    {
        ++b;
    }
    size_t e = sym.size();
    while (e > b && std::isspace(static_cast<unsigned char>(sym[e - 1])))
    {
        --e;
    }
    if (b >= e)
    {
        return false;
    }
    std::string norm;
    norm.reserve(e - b);
    norm += static_cast<char>(std::toupper(static_cast<unsigned char>(sym[b])));
    for (size_t i = b + 1; i < e; ++i)
    {
        norm += static_cast<char>(std::tolower(static_cast<unsigned char>(sym[i])));
    }
    for (int i = 1; i < 117; ++i)
    {
        if (norm == SYMBOL_TABLE[i])
        {
            out = static_cast<uint8_t>(i);
            return true;
        }
    }
    return false;
}

} // namespace Rmsd
