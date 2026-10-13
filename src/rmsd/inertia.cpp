/**
 * @file inertia.cpp
 * @brief Inertia implementation.
 * @author Le Nhan Pham
 * @date 2026
 */

#include "rmsd/inertia.h"
#include "rmsd/elements.h"
#include "rmsd/linalg.h"

namespace Rmsd
{

Coord center_of_mass(const std::vector<uint8_t>& atoms, const std::vector<Coord>& coords)
{
    double wsum = 0.0;
    Coord acc{0.0, 0.0, 0.0};
    for (size_t i = 0; i < atoms.size(); ++i)
    {
        double w = element_mass(atoms[i]);
        wsum += w;
        acc[0] += coords[i][0] * w;
        acc[1] += coords[i][1] * w;
        acc[2] += coords[i][2] * w;
    }
    if (wsum == 0.0)
    {
        double n = coords.empty() ? 1.0 : static_cast<double>(coords.size());
        acc[0] /= n;
        acc[1] /= n;
        acc[2] /= n;
        return acc;
    }
    acc[0] /= wsum;
    acc[1] /= wsum;
    acc[2] /= wsum;
    return acc;
}

Mat3 inertia_tensor(const std::vector<uint8_t>& atoms, const std::vector<Coord>& coords)
{
    Coord cm = center_of_mass(atoms, coords);
    Mat3 h{};
    for (size_t k = 0; k < atoms.size(); ++k)
    {
        double w = element_mass(atoms[k]);
        double x = coords[k][0] - cm[0];
        double y = coords[k][1] - cm[1];
        double z = coords[k][2] - cm[2];
        h[0][0] += x * w * x;
        h[0][1] += x * w * y;
        h[0][2] += x * w * z;
        h[1][0] += y * w * x;
        h[1][1] += y * w * y;
        h[1][2] += y * w * z;
        h[2][0] += z * w * x;
        h[2][1] += z * w * y;
        h[2][2] += z * w * z;
    }
    double tr = mat3_trace(h);
    Mat3 out{};
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            out[i][j] = (i == j ? tr : 0.0) - h[i][j];
        }
    }
    return out;
}

Coord principal_axis(const std::vector<uint8_t>& atoms, const std::vector<Coord>& coords)
{
    Mat3 inertia = inertia_tensor(atoms, coords);
    Mat3 v;
    double w[3];
    jacobi_eigen3(inertia, v, w);
    int best = 0;
    for (int i = 1; i < 3; ++i)
    {
        if (w[i] > w[best])
        {
            best = i;
        }
    }
    return {v[0][best], v[1][best], v[2][best]};
}

} // namespace Rmsd
