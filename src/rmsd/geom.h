/**
 * @file geom.h
 * @brief Geometry: RMSD, Kabsch, quaternion.
 * @author Le Nhan Pham
 * @date 2026
 */

#ifndef RMSD_GEOM_H
#define RMSD_GEOM_H

#include "rmsd/rmsd_types.h"
#include <vector>

namespace Rmsd
{

Coord centroid(const std::vector<Coord>& coords);
double rmsd_plain(const std::vector<Coord>& p, const std::vector<Coord>& q);
Mat3 kabsch_matrix(const std::vector<Coord>& p, const std::vector<Coord>& q);
std::vector<Coord> kabsch_rotate(const std::vector<Coord>& p, const std::vector<Coord>& q);
std::vector<Coord> kabsch_fit(const std::vector<Coord>& p, const std::vector<Coord>& q);
double kabsch_rmsd(const std::vector<Coord>& p, const std::vector<Coord>& q, const std::vector<double>* w, bool translate);
double kabsch_weighted_rmsd(const std::vector<Coord>& p, const std::vector<Coord>& q, const std::vector<double>* w);

struct WeightedResult
{
    Mat3 rot;
    Coord trans;
    double rmsd;
};

WeightedResult kabsch_weighted(const std::vector<Coord>& p, const std::vector<Coord>& q, const std::vector<double>* w);

Mat3 quaternion_transform(const std::array<double, 4>& r);
Mat3 quaternion_rotate(const std::vector<Coord>& x, const std::vector<Coord>& y);
double quaternion_rmsd(const std::vector<Coord>& p, const std::vector<Coord>& q);

} // namespace Rmsd

#endif // RMSD_GEOM_H
