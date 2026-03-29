#include "projectile/projectile_simulation.h"

#include <cmath>
#include <iostream>

namespace omnisim::projectile {

ProjectileSimulation::ProjectileSimulation(ProjectileConfig config) : config_(config) {}

void ProjectileSimulation::initialize() {
    state_.position = config_.initial_position;
    state_.velocity = config_.initial_velocity;
    state_.acceleration = config_.gravity;
    state_.elapsed_seconds = 0.0;
    finished_ = false;
}

void ProjectileSimulation::step(const double dt_seconds) {
    if (finished_ || dt_seconds <= 0.0) {
        return;
    }

    // Semi-implicit Euler keeps energy behavior more stable than explicit Euler.
    state_.velocity += state_.acceleration * dt_seconds;
    state_.position += state_.velocity * dt_seconds;
    state_.elapsed_seconds += dt_seconds;

    const bool hit_ground = state_.position.y <= 0.0 && state_.elapsed_seconds > 0.0;
    const bool exceeded_time = state_.elapsed_seconds >= config_.max_duration_seconds;
    const bool invalid_state = std::isnan(state_.position.x) || std::isnan(state_.position.y) ||
                               std::isnan(state_.velocity.x) || std::isnan(state_.velocity.y);

    finished_ = hit_ground || exceeded_time || invalid_state;
}

bool ProjectileSimulation::is_finished() const noexcept {
    return finished_;
}

std::string_view ProjectileSimulation::name() const noexcept {
    return "projectile";
}

void ProjectileSimulation::print_state() const {
    std::cout << "[projectile] t=" << state_.elapsed_seconds << "s"
              << " pos=(" << state_.position.x << ", " << state_.position.y << ")"
              << " vel=(" << state_.velocity.x << ", " << state_.velocity.y << ")\n";
}

}  // namespace omnisim::projectile
