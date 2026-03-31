#ifndef OMNISIM_PROJECTILE_PROJECTILE_SIMULATION_H
#define OMNISIM_PROJECTILE_PROJECTILE_SIMULATION_H

#include "projectile/projectile_state.h"
#include "simulation/i_simulation.h"

#include <string_view>
#include <memory>

#include "projectile/terrain_profile.h"
#include "projectile/i_projectile.h"

namespace omnisim::projectile {

enum class ProjectileType {
    Simple,
    Grenade,
    Missile
};

struct ProjectileConfig {
    math::Vector2 initial_position{0.0, 0.0};
    math::Vector2 initial_velocity{30.0, 30.0};
    math::Vector2 gravity{0.0, -9.81};
    double max_duration_seconds{20.0};
    bool enable_console_output{true};
    /// Linear air drag: acceleration += -coefficient * velocity (0 disables).
    double air_drag_coefficient{0.0};
    
    ProjectileType type{ProjectileType::Simple};
    const TerrainProfile* terrain{nullptr};
    
    /// Restitution coefficient for bouncing objects.
    double bounciness{0.5};
    
    /// Mass of the projectile (used for drag and collisions).
    double mass{1.0};
};

class ProjectileSimulation final : public simulation::ISimulation {
public:
    explicit ProjectileSimulation(ProjectileConfig config);

    void initialize() override;
    void step(double dt_seconds) override;
    [[nodiscard]] bool is_finished() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    void print_state() const override;
    [[nodiscard]] const ProjectileState& state() const noexcept;

private:
    ProjectileConfig config_{};
    ProjectileState state_{};
    bool finished_{false};
    std::unique_ptr<IProjectile> projectile_{nullptr};
};

}  // namespace omnisim::projectile

#endif  // OMNISIM_PROJECTILE_PROJECTILE_SIMULATION_H
