#include "engine/engine_loop.h"
#include "projectile/projectile_simulation.h"
#include "simulation/simulation_factory.h"

#include <iostream>
#include <memory>
#include <string_view>

namespace {

void print_usage(const omnisim::simulation::SimulationFactory& factory) {
    std::cout << "Usage: omnisim_cli [simulation_id]\n";
    std::cout << "Available simulations:\n";
    for (const auto& id : factory.available_simulations()) {
        std::cout << "  - " << id << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    omnisim::simulation::SimulationFactory factory{};

    const bool registered = factory.register_simulation(
        "projectile",
        [] { return std::make_unique<omnisim::projectile::ProjectileSimulation>(omnisim::projectile::ProjectileConfig{}); });
    if (!registered) {
        std::cerr << "Failed to register projectile simulation.\n";
        return 1;
    }

    const std::string_view simulation_id = (argc > 1) ? argv[1] : "projectile";
    auto simulation = factory.create(simulation_id);
    if (!simulation.has_value()) {
        std::cerr << "Unknown simulation: " << simulation_id << '\n';
        print_usage(factory);
        return 1;
    }

    omnisim::engine::EngineLoop engine{omnisim::engine::TimeConfig{
        .fixed_dt_seconds = 1.0 / 60.0,
        .max_steps = 2'000,
    }};

    const std::size_t executed_steps = engine.run(std::move(simulation.value()));
    std::cout << "Simulation finished after " << executed_steps << " steps.\n";
    return 0;
}
