/**
 * @file rmsd_io.h
 * @brief XYZ/PDB readers, XYZ writer, format detect (port of Rust io/).
 * @author Le Nhan Pham
 * @date 2026
 */

#ifndef RMSD_IO_H
#define RMSD_IO_H

#include "rmsd/rmsd_types.h"
#include <string>

namespace Rmsd
{

enum class FileFormat
{
    Xyz,
    Pdb
};

bool parse_format(const std::string& s, FileFormat& out);
/** @brief Detect format from path; is_gzip set when suffix is .gz (case-insensitive). */
bool detect_format(const std::string& path, FileFormat& fmt, bool& is_gzip, std::string& err);
std::string read_text_file(const std::string& path, bool is_gzip);
Molecule parse_xyz_text(const std::string& text);
Molecule parse_pdb_text(const std::string& text, bool only_alpha);
std::string write_xyz(const Molecule& mol, const std::string& title, int decimals);
Molecule load_molecule(const std::string& path, FileFormat fmt, bool is_gzip, bool only_alpha);

} // namespace Rmsd

#endif // RMSD_IO_H
