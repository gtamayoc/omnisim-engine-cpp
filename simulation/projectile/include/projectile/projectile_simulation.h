#ifndef OMNISIM_PROJECTILE_PROJECTILE_SIMULATION_H
#define OMNISIM_PROJECTILE_PROJECTILE_SIMULATION_H

#include "projectile/projectile_state.h"
#include "simulation/i_simulation.h"

#include <string_view>

namespace omnisim::projectile {

struct ProjectileConfig {
    math::Vector2 initial_position{0.0, 0.0};
    math::Vector2 initial_velocity{30.0, 30.0};
    math::Vector2 gravity{0.0, -9.81};
    double max_duration_seconds{20.0};
    bool enable_console_output{true};
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
};

}  // namespace omnisim::projectile

#endif  // OMNISIM_PROJECTILE_PROJECTILE_SIMULATION_H
