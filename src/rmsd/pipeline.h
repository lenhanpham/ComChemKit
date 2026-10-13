/**
 * @file pipeline.h
 * @brief End-to-end RMSD pipeline (port of Rust pipeline.rs).
 * @author Le Nhan Pham
 * @date 2026
 */

#ifndef RMSD_PIPELINE_H
#define RMSD_PIPELINE_H

#include "rmsd/rmsd_types.h"
#include <string>

namespace Rmsd
{

struct Options
{
    Rotation rotation = Rotation::Kabsch;
    ReorderMethod reorder = ReorderMethod::InertiaHungarian;
    bool do_reorder = false;
    Reflections reflections = Reflections::None;
    Filter filter;
    bool print_rotated = false;
    bool print_only_rmsd_atoms = false;
};

struct Output
{
    double rmsd = 0.0;
    bool has_rotated = false;
    Molecule rotated;
    std::string title;
};

Output compute_rmsd(const Molecule& p, const Molecule& q, const Options& opts, const std::string& name_p,
    const std::string& name_q);

} // namespace Rmsd

#endif // RMSD_PIPELINE_H
