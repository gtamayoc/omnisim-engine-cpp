#include "projectile/terrain_profile.h"

#include <cmath>

namespace omnisim::projectile {

double TerrainProfile::height_at(const double x) const noexcept {
    const double h1 = 8.0 * std::sin((x - 30.0) * 0.06);
    const double h2 = 4.0 * std::sin((x + 20.0) * 0.15);
    const double mountain = 14.0 * std::exp(-std::pow((x - 120.0) / 55.0, 2.0));
    const double base = 6.0;
    return base + h1 + h2 + mountain;
}

}  // namespace omnisim::projectile
