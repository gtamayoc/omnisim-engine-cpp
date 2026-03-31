#ifndef OMNISIM_PROJECTILE_I_PROJECTILE_H
#define OMNISIM_PROJECTILE_I_PROJECTILE_H

#include "projectile/projectile_state.h"
#include "projectile/terrain_profile.h"

namespace omnisim::projectile {

class IProjectile {
public:
    virtual ~IProjectile() = default;

    /// Update the projectile's physics state by dt_seconds.
    /// Returns true if the projectile's lifetime has ended (e.g. exploded, stopped).
    virtual bool step(double dt_seconds, const TerrainProfile* terrain, ProjectileState& state) = 0;
};

}  // namespace omnisim::projectile

#endif  // OMNISIM_PROJECTILE_I_PROJECTILE_H
