/**
 * @file pipeline.cpp
 * @brief Pipeline implementation (port of Rust pipeline.rs).
 * @author Le Nhan Pham
 * @date 2026
 */

#include "rmsd/pipeline.h"
#include "rmsd/geom.h"
#include "rmsd/linalg.h"
#include "rmsd/reflect.h"
#include "rmsd/reorder.h"
#include <set>

namespace Rmsd
{

namespace
{

Coord centroid_of_vec(const std::vector<Coord>& v)
{
    return centroid(v);
}

double rmsd_for_rotation(Rotation r, const std::vector<Coord>& p, const std::vector<Coord>& q)
{
    switch (r)
    {
        case Rotation::Kabsch:
            return kabsch_rmsd(p, q, nullptr, false);
        case Rotation::Quaternion:
            return quaternion_rmsd(p, q);
        case Rotation::None:
            return rmsd_plain(p, q);
    }
    return rmsd_plain(p, q);
}

ReorderFn reorder_fn_for(ReorderMethod m, bool& has)
{
    has = true;
    switch (m)
    {
        case ReorderMethod::None:
            has = false;
            return {};
        case ReorderMethod::Hungarian:
            return [](const std::vector<uint8_t>& pa, const std::vector<uint8_t>& qa,
                       const std::vector<Coord>& pc, const std::vector<Coord>& qc) {
                return reorder_hungarian(pa, qa, pc, qc);
            };
        case ReorderMethod::InertiaHungarian:
            return [](const std::vector<uint8_t>& pa, const std::vector<uint8_t>& qa,
                       const std::vector<Coord>& pc, const std::vector<Coord>& qc) {
                return reorder_inertia_hungarian(pa, qa, pc, qc);
            };
        case ReorderMethod::Distance:
            return [](const std::vector<uint8_t>& pa, const std::vector<uint8_t>& qa,
                       const std::vector<Coord>& pc, const std::vector<Coord>& qc) {
                return reorder_distance(pa, qa, pc, qc);
            };
        case ReorderMethod::Brute:
            return [](const std::vector<uint8_t>& pa, const std::vector<uint8_t>& qa,
                       const std::vector<Coord>& pc, const std::vector<Coord>& qc) {
                return reorder_brute(pa, qa, pc, qc);
            };
    }
    has = false;
    return {};
}

} // namespace

Output compute_rmsd(const Molecule& p, const Molecule& q, const Options& opts, const std::string& name_p,
    const std::string& name_q)
{
    if (p.size() != q.size())
    {
        throw RmsdException(RmsdErrorCode::SizeMismatch,
            "structures have different sizes (" + std::to_string(p.size()) + " vs " + std::to_string(q.size()) + ")");
    }
    bool reorder_active = opts.do_reorder && opts.reorder != ReorderMethod::None;
    bool has_refl_reorder = false;
    ReorderFn refl_reorder = reorder_fn_for(opts.reorder, has_refl_reorder);

    bool same_order = (p.atoms == q.atoms);
    if (!same_order && !reorder_active && opts.reflections == Reflections::None)
    {
        bool can_fix = opts.reflections != Reflections::None && has_refl_reorder;
        if (!can_fix)
        {
            throw RmsdException(
                RmsdErrorCode::AtomOrder, "atoms are not in the same order; use reordering to align them");
        }
    }

    std::vector<size_t> p_idx, q_idx;
    bool use_view = false;
    switch (opts.filter.kind)
    {
        case FilterKind::All:
            p_idx.resize(p.size());
            q_idx.resize(q.size());
            for (size_t i = 0; i < p_idx.size(); ++i)
            {
                p_idx[i] = i;
            }
            for (size_t i = 0; i < q_idx.size(); ++i)
            {
                q_idx[i] = i;
            }
            use_view = false;
            break;
        case FilterKind::NoHydrogen:
            for (size_t i = 0; i < p.size(); ++i)
            {
                if (p.atoms[i] != 1)
                {
                    p_idx.push_back(i);
                }
            }
            for (size_t i = 0; i < q.size(); ++i)
            {
                if (q.atoms[i] != 1)
                {
                    q_idx.push_back(i);
                }
            }
            use_view = true;
            break;
        case FilterKind::RemoveIdx:
        {
            std::set<size_t> drop(opts.filter.indices.begin(), opts.filter.indices.end());
            for (size_t i = 0; i < p.size(); ++i)
            {
                if (drop.find(i) == drop.end())
                {
                    p_idx.push_back(i);
                    q_idx.push_back(i);
                }
            }
            use_view = true;
            break;
        }
        case FilterKind::AddIdx:
            p_idx = opts.filter.indices;
            q_idx = opts.filter.indices;
            use_view = true;
            break;
    }

    std::vector<uint8_t> p_atoms, q_atoms;
    std::vector<Coord> p_sub, q_sub;
    for (size_t i : p_idx)
    {
        p_atoms.push_back(p.atoms[i]);
        p_sub.push_back(p.coords[i]);
    }
    for (size_t i : q_idx)
    {
        q_atoms.push_back(q.atoms[i]);
        q_sub.push_back(q.coords[i]);
    }

    Coord p_cent = centroid_of_vec(p_sub);
    Coord q_cent = centroid_of_vec(q_sub);
    for (auto& v : p_sub)
    {
        v[0] -= p_cent[0];
        v[1] -= p_cent[1];
        v[2] -= p_cent[2];
    }
    for (auto& v : q_sub)
    {
        v[0] -= q_cent[0];
        v[1] -= q_cent[1];
        v[2] -= q_cent[2];
    }

    bool has_result = false;
    double result_rmsd = 0.0;
    bool has_swap = false, has_refl = false;
    std::array<size_t, 3> q_swap{0, 1, 2};
    std::array<double, 3> q_refl{1, 1, 1};
    std::vector<size_t> q_review;
    bool has_review = false;

    if (opts.reflections != Reflections::None)
    {
        if (opts.print_rotated && use_view && reorder_active)
        {
            throw RmsdException(
                RmsdErrorCode::PrintFilterConflict, "cannot combine rotated output with atom filtering + reordering");
        }
        bool keep = (opts.reflections == Reflections::KeepStereo);
        Rotation rot = opts.rotation;
        RmsdFn fn = [rot](const std::vector<Coord>& a, const std::vector<Coord>& b) {
            return rmsd_for_rotation(rot, a, b);
        };
        ReflectResult rr =
            check_reflections(p_atoms, q_atoms, p_sub, q_sub, refl_reorder, has_refl_reorder, fn, keep);
        has_result = true;
        result_rmsd = rr.rmsd;
        has_swap = true;
        has_refl = true;
        q_swap = rr.swap;
        q_refl = rr.reflection;
        q_review = rr.review;
        has_review = true;
    }

    if (!has_result && reorder_active)
    {
        if (opts.print_rotated && use_view)
        {
            throw RmsdException(
                RmsdErrorCode::PrintFilterConflict, "cannot combine rotated output with atom filtering + reordering");
        }
        bool applied = false;
        q_review = reorder_dispatch(opts.reorder, p_atoms, q_atoms, p_sub, q_sub, applied);
        has_review = true;
    }

    if (has_review)
    {
        std::vector<uint8_t> qa2;
        std::vector<Coord> qs2;
        for (size_t i : q_review)
        {
            qa2.push_back(q_atoms[i]);
            qs2.push_back(q_sub[i]);
        }
        q_atoms = qa2;
        q_sub = qs2;
        if (p_atoms != q_atoms)
        {
            throw RmsdException(RmsdErrorCode::NotAligned, "structure not aligned after reordering (internal error)");
        }
    }

    double rmsd_value = has_result ? result_rmsd : rmsd_for_rotation(opts.rotation, p_sub, q_sub);

    Output out;
    out.rmsd = rmsd_value;
    out.title = "Rotated '" + name_q + "' to match '" + name_p + "'";
    if (opts.print_rotated)
    {
        std::vector<Coord> qs = q_sub;
        if (has_swap)
        {
            for (auto& v : qs)
            {
                Coord t = {v[q_swap[0]], v[q_swap[1]], v[q_swap[2]]};
                v = t;
            }
        }
        if (has_refl)
        {
            for (auto& v : qs)
            {
                v[0] *= q_refl[0];
                v[1] *= q_refl[1];
                v[2] *= q_refl[2];
            }
        }
        Mat3 u = kabsch_matrix(qs, p_sub);
        auto apply = [&](const Coord& v) { return apply_mat_row(v, u); };
        out.has_rotated = true;
        if (opts.print_only_rmsd_atoms || !use_view)
        {
            out.rotated.atoms = q_atoms;
            out.rotated.coords.clear();
            for (const auto& v : qs)
            {
                Coord r = apply(v);
                out.rotated.coords.push_back({r[0] + p_cent[0], r[1] + p_cent[1], r[2] + p_cent[2]});
            }
        }
        else
        {
            std::vector<Coord> qf;
            for (const auto& v : q.coords)
            {
                qf.push_back({v[0] - q_cent[0], v[1] - q_cent[1], v[2] - q_cent[2]});
            }
            if (has_swap)
            {
                for (auto& v : qf)
                {
                    Coord t = {v[q_swap[0]], v[q_swap[1]], v[q_swap[2]]};
                    v = t;
                }
            }
            if (has_refl)
            {
                for (auto& v : qf)
                {
                    v[0] *= q_refl[0];
                    v[1] *= q_refl[1];
                    v[2] *= q_refl[2];
                }
            }
            out.rotated.atoms = q.atoms;
            out.rotated.coords.clear();
            for (const auto& v : qf)
            {
                Coord r = apply(v);
                out.rotated.coords.push_back({r[0] + p_cent[0], r[1] + p_cent[1], r[2] + p_cent[2]});
            }
        }
    }
    return out;
}

} // namespace Rmsd
