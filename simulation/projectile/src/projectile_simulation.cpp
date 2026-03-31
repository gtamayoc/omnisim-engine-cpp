#include "projectile/projectile_simulation.h"
#include "math/vector2.h"

#include <cmath>
#include <iostream>

namespace omnisim::projectile {

namespace {

math::Vector2 get_terrain_normal(const TerrainProfile* terrain, double x) {
    if (!terrain) return {0.0, 1.0};
    const double h = 0.1;
    const double dy = terrain->height_at(x + h) - terrain->height_at(x - h);
    const double dx = 2.0 * h;
    return math::normalized({-dy, dx});
}

class SimpleProjectile : public IProjectile {
public:
    explicit SimpleProjectile(const ProjectileConfig& config) : config_(config) {}

    bool step(double dt_seconds, const TerrainProfile* terrain, ProjectileState& state) override {
        state.acceleration = config_.gravity;
        if (config_.air_drag_coefficient > 0.0) {
            state.acceleration -= state.velocity * (config_.air_drag_coefficient / config_.mass);
        }

        state.velocity += state.acceleration * dt_seconds;
        state.position += state.velocity * dt_seconds;
        state.elapsed_seconds += dt_seconds;

        if (terrain) {
            const double ground_y = terrain->height_at(state.position.x);
            if (state.position.y <= ground_y) {
                // Collision!
                state.position.y = ground_y;
                
                auto n = get_terrain_normal(terrain, state.position.x);
                double v_dot_n = math::dot(state.velocity, n);
                
                if (v_dot_n < 0) {
                    // Bounce
                    math::Vector2 impulse = n * ((1.0 + config_.bounciness) * v_dot_n);
                    state.velocity -= impulse;
                    
                    // Friction/damping on tangential velocity
                    math::Vector2 tangent = {-n.y, n.x};
                    double v_dot_t = math::dot(state.velocity, tangent);
                    state.velocity -= tangent * (v_dot_t * 0.1); 
                }
                
                // If velocity is very low, stop simulating
                if (math::magnitude(state.velocity) < 0.5) {
                    state.velocity = {0.0, 0.0};
                    return true; 
                }
            }
        }
        
        return state.elapsed_seconds >= config_.max_duration_seconds;
    }

private:
    ProjectileConfig config_;
};

class GrenadeProjectile : public IProjectile {
public:
    explicit GrenadeProjectile(const ProjectileConfig& config) : config_(config), fuse_timer_(3.0) {}

    bool step(double dt_seconds, const TerrainProfile* terrain, ProjectileState& state) override {
        state.acceleration = config_.gravity;
        // Grenades usually have higher drag
        state.acceleration -= state.velocity * ( (config_.air_drag_coefficient + 0.05) / config_.mass);

        state.velocity += state.acceleration * dt_seconds;
        state.position += state.velocity * dt_seconds;
        state.elapsed_seconds += dt_seconds;
        fuse_timer_ -= dt_seconds;

        if (terrain) {
            const double ground_y = terrain->height_at(state.position.x);
            if (state.position.y <= ground_y) {
                state.position.y = ground_y;
                auto n = get_terrain_normal(terrain, state.position.x);
                double v_dot_n = math::dot(state.velocity, n);
                
                if (v_dot_n < 0) {
                    // Bouncy
                    math::Vector2 impulse = n * ((1.0 + config_.bounciness * 0.7) * v_dot_n);
                    state.velocity -= impulse;
                    
                    // Higher friction for a grenade rolling
                    math::Vector2 tangent = {-n.y, n.x};
                    double v_dot_t = math::dot(state.velocity, tangent);
                    state.velocity -= tangent * (v_dot_t * 0.3);
                }
            }
        }

        if (fuse_timer_ <= 0.0) {
            // EXPLOSION 
            if (config_.enable_console_output) {
                std::cout << "[Grenade] BOOM! Exploded at t=" << state.elapsed_seconds << "s\n";
            }
            return true;
        }

        return state.elapsed_seconds >= config_.max_duration_seconds;
    }

private:
    ProjectileConfig config_;
    double fuse_timer_{3.0};
};

class MissileProjectile : public IProjectile {
public:
    explicit MissileProjectile(const ProjectileConfig& config) : config_(config), thrust_timer_(2.0) {
        // Assume missile direction is initially aligned with velocity
        double mag = math::magnitude(config.initial_velocity);
        if (mag > 1e-6) {
            thrust_dir_ = math::normalized(config.initial_velocity);
        }
    }

    bool step(double dt_seconds, const TerrainProfile* terrain, ProjectileState& state) override {
        state.acceleration = config_.gravity;
        
        // Thrust phase
        if (thrust_timer_ > 0.0) {
            state.acceleration += thrust_dir_ * 60.0; // Fixed thrust acceleration
            thrust_timer_ -= dt_seconds;
        }
        
        // Drag
        if (config_.air_drag_coefficient > 0.0) {
            state.acceleration -= state.velocity * (config_.air_drag_coefficient / config_.mass);
        }

        state.velocity += state.acceleration * dt_seconds;
        state.position += state.velocity * dt_seconds;
        state.elapsed_seconds += dt_seconds;

        // Thrust vector slowly aligns with velocity (gravity turn)
        double mag = math::magnitude(state.velocity);
        if (mag > 1.0) {
            thrust_dir_ = math::normalized(state.velocity);
        }

        if (terrain) {
            const double ground_y = terrain->height_at(state.position.x);
            if (state.position.y <= ground_y) {
                // Missile explodes on impact immediately!
                state.position.y = ground_y;
                if (config_.enable_console_output) {
                    std::cout << "[Missile] Impact explosion at t=" << state.elapsed_seconds << "s\n";
                }
                return true;
            }
        }

        return state.elapsed_seconds >= config_.max_duration_seconds;
    }

private:
    ProjectileConfig config_;
    double thrust_timer_{2.0};
    math::Vector2 thrust_dir_{1.0, 0.0};
};

} // namespace

ProjectileSimulation::ProjectileSimulation(ProjectileConfig config) : config_(config) {
    initialize();
}

void ProjectileSimulation::initialize() {
    state_.position = config_.initial_position;
    state_.velocity = config_.initial_velocity;
    state_.acceleration = config_.gravity;
    state_.elapsed_seconds = 0.0;
    finished_ = false;
    
    // Re-instantiate to restart internal state
    switch (config_.type) {
        case ProjectileType::Simple:
            projectile_ = std::make_unique<SimpleProjectile>(config_);
            break;
        case ProjectileType::Grenade:
            projectile_ = std::make_unique<GrenadeProjectile>(config_);
            break;
        case ProjectileType::Missile:
            projectile_ = std::make_unique<MissileProjectile>(config_);
            break;
    }
}

void ProjectileSimulation::step(const double dt_seconds) {
    if (finished_ || dt_seconds <= 0.0 || !projectile_) {
        return;
    }

    const bool invalid_state = std::isnan(state_.position.x) || std::isnan(state_.position.y) ||
                               std::isnan(state_.velocity.x) || std::isnan(state_.velocity.y);

    if (invalid_state) {
        finished_ = true;
        return;
    }

    finished_ = projectile_->step(dt_seconds, config_.terrain, state_);
}

bool ProjectileSimulation::is_finished() const noexcept {
    return finished_;
}

std::string_view ProjectileSimulation::name() const noexcept {
    switch (config_.type) {
        case ProjectileType::Simple: return "projectile_simple";
        case ProjectileType::Grenade: return "projectile_grenade";
        case ProjectileType::Missile: return "projectile_missile";
        default: return "projectile";
    }
}

void ProjectileSimulation::print_state() const {
    if (!config_.enable_console_output) {
        return;
    }
    std::cout << "[" << name() << "] t=" << state_.elapsed_seconds << "s"
              << " pos=(" << state_.position.x << ", " << state_.position.y << ")"
              << " vel=(" << state_.velocity.x << ", " << state_.velocity.y << ")\n";
}

const ProjectileState& ProjectileSimulation::state() const noexcept {
    return state_;
}

}  // namespace omnisim::projectile
