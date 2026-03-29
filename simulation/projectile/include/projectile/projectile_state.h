#ifndef OMNISIM_PROJECTILE_PROJECTILE_STATE_H
#define OMNISIM_PROJECTILE_PROJECTILE_STATE_H

#include "math/vector2.h"

namespace omnisim::projectile {

struct ProjectileState {
    math::Vector2 position{0.0, 0.0};
    math::Vector2 velocity{0.0, 0.0};
    math::Vector2 acceleration{0.0, -9.81};
    double elapsed_seconds{0.0};
};

}  // namespace omnisim::projectile

#endif  // OMNISIM_PROJECTILE_PROJECTILE_STATE_H
