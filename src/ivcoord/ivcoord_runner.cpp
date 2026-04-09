#include "ivcoord/ivcoord_runner.h"
#include "ivcoord/ivcoord_parser_base.h"
#include "ivcoord/gaussian_ivcoord_parser.h"
// Add future parser includes here, e.g.:
// #include "ivcoord/orca_ivcoord_parser.h"

#include <array>
#include <fstream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Parser registry
// ---------------------------------------------------------------------------
// To register a new QC program parser, add it to this vector.
static std::vector<std::unique_ptr<IIVCoordParser>> make_parsers()
{
    std::vector<std::unique_ptr<IIVCoordParser>> parsers;
    parsers.push_back(std::make_unique<GaussianIVCoordParser>());
    // parsers.push_back(std::make_unique<OrcaIVCoordParser>());
    return parsers;
}

// ---------------------------------------------------------------------------
// extract: try each parser in order
// ---------------------------------------------------------------------------
IVCoordData IVCoordRunner::extract(const std::string& filepath)
{
    auto parsers = make_parsers();

    for (auto& parser : parsers)
    {
        if (parser->can_parse(filepath))
            return parser->parse(filepath);
    }

    IVCoordData failed;
    failed.error_message = "Unrecognized file format: " + filepath;
    return failed;
}

// ---------------------------------------------------------------------------
// write_xyz_file: internal helper
// Writes one displaced XYZ file with the given direction multiplier.
// ---------------------------------------------------------------------------
static void write_xyz_file(const IVCoordData&           data,
                           double                       amp,
                           int                          dir_sign,
                           const std::filesystem::path& out_dir,
                           const std::string&           stem,
                           const std::string&           suffix)
{
    std::filesystem::path out_path = out_dir / (stem + suffix + ".xyz");
    std::ofstream out(out_path);
    if (!out.is_open())
        throw std::runtime_error("Cannot open output file: " + out_path.string());

    // Line 1: atom count
    out << data.natom << "\n";
    // Line 2: comment (stem + direction tag)
    out << stem << suffix << "\n";

    for (int a = 0; a < data.natom; ++a)
    {
        const double x = data.coords[a][0] + dir_sign * amp * data.disps[a][0];
        const double y = data.coords[a][1] + dir_sign * amp * data.disps[a][1];
        const double z = data.coords[a][2] + dir_sign * amp * data.disps[a][2];

        out << std::left  << std::setw(10) << data.elements[a]
            << std::right << std::fixed    << std::setprecision(10)
            << std::setw(20) << x
            << std::setw(20) << y
            << std::setw(20) << z
            << "\n";
    }

    out.close();
}

// ---------------------------------------------------------------------------
// apply_displacement: public API
// ---------------------------------------------------------------------------
void IVCoordRunner::apply_displacement(const IVCoordData&           data,
                                       double                       amp,
                                       int                          direction,
                                       const std::filesystem::path& out_dir,
                                       const std::string&           stem)
{
    if (direction == 0)
    {
        // Both directions → use _p / _m suffixes to distinguish
        write_xyz_file(data, amp, +1, out_dir, stem, "_p");
        write_xyz_file(data, amp, -1, out_dir, stem, "_m");
    }
    else if (direction > 0)
    {
        // Single direction (positive) → no suffix, name matches the log file
        write_xyz_file(data, amp, +1, out_dir, stem, "");
    }
    else
    {
        // Single direction (negative) → no suffix, name matches the log file
        write_xyz_file(data, amp, -1, out_dir, stem, "");
    }
}
