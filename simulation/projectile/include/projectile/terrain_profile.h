#ifndef OMNISIM_PROJECTILE_TERRAIN_PROFILE_H
#define OMNISIM_PROJECTILE_TERRAIN_PROFILE_H

namespace omnisim::projectile {

class TerrainProfile {
public:
    [[nodiscard]] double height_at(double x) const noexcept;
};

}  // namespace omnisim::projectile

#endif  // OMNISIM_PROJECTILE_TERRAIN_PROFILE_H
