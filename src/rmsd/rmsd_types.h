/**
 * @file rmsd_types.h
 * @brief Shared types and error class for the rmsd module.
 * @author Le Nhan Pham
 * @date 2026
 */

#ifndef RMSD_TYPES_H
#define RMSD_TYPES_H

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace Rmsd
{

/** @brief Cartesian coordinate triplet. */
using Coord = std::array<double, 3>;

/** @brief 3x3 row-major matrix: m[row][col]. */
using Mat3 = std::array<std::array<double, 3>, 3>;

/** @brief 4x4 row-major matrix for quaternion method. */
using Mat4 = std::array<std::array<double, 4>, 4>;

struct Molecule
{
    std::vector<uint8_t> atoms;
    std::vector<Coord> coords;

    size_t size() const
    {
        return atoms.size();
    }
    bool empty() const
    {
        return atoms.empty();
    }
};

enum class Rotation
{
    Kabsch,
    Quaternion,
    None
};

enum class Reflections
{
    None,
    All,
    KeepStereo
};

enum class ReorderMethod
{
    None,
    Hungarian,
    InertiaHungarian,
    Distance,
    Brute
};

enum class FilterKind
{
    All,
    NoHydrogen,
    RemoveIdx,
    AddIdx
};

struct Filter
{
    FilterKind kind = FilterKind::All;
    std::vector<size_t> indices;
};

enum class RmsdErrorCode
{
    SizeMismatch,
    AtomOrder,
    NotAligned,
    PrintFilterConflict,
    Io,
    Parse,
    Format,
    Budget,
    Ragged,
    Args
};

/**
 * @brief Exception carrying an RmsdErrorCode plus message.
 */
class RmsdException : public std::runtime_error
{
public:
    RmsdException(RmsdErrorCode code, const std::string& msg)
        : std::runtime_error(msg)
        , code_(code)
    {
    }
    RmsdErrorCode code() const
    {
        return code_;
    }

private:
    RmsdErrorCode code_;
};

inline Rotation parse_rotation(const std::string& s)
{
    std::string t = s;
    for (auto& c : t)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (t == "kabsch")
    {
        return Rotation::Kabsch;
    }
    if (t == "quaternion")
    {
        return Rotation::Quaternion;
    }
    if (t == "none")
    {
        return Rotation::None;
    }
    throw RmsdException(RmsdErrorCode::Args, "invalid rotation method: '" + s + "'");
}

inline ReorderMethod parse_reorder_method(const std::string& s)
{
    std::string t = s;
    for (auto& c : t)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (t == "none")
    {
        return ReorderMethod::None;
    }
    if (t == "hungarian")
    {
        return ReorderMethod::Hungarian;
    }
    if (t == "inertia-hungarian")
    {
        return ReorderMethod::InertiaHungarian;
    }
    if (t == "distance")
    {
        return ReorderMethod::Distance;
    }
    if (t == "brute")
    {
        return ReorderMethod::Brute;
    }
    throw RmsdException(RmsdErrorCode::Args, "invalid reorder method: '" + s + "'");
}

inline const char* reorder_name(ReorderMethod m)
{
    switch (m)
    {
        case ReorderMethod::None:
            return "none";
        case ReorderMethod::Hungarian:
            return "hungarian";
        case ReorderMethod::InertiaHungarian:
            return "inertia-hungarian";
        case ReorderMethod::Distance:
            return "distance";
        case ReorderMethod::Brute:
            return "brute";
    }
    return "none";
}

} // namespace Rmsd

#endif // RMSD_TYPES_H
