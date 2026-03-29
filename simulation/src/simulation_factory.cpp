#include "simulation/simulation_factory.h"

#include <algorithm>

namespace omnisim::simulation {

bool SimulationFactory::register_simulation(std::string simulation_id, SimulationBuilder builder) {
    if (simulation_id.empty() || !builder) {
        return false;
    }

    return registry_.emplace(std::move(simulation_id), std::move(builder)).second;
}

std::optional<std::unique_ptr<ISimulation>> SimulationFactory::create(const std::string_view simulation_id) const {
    const auto it = registry_.find(std::string{simulation_id});
    if (it == registry_.end()) {
        return std::nullopt;
    }
    return it->second();
}

std::vector<std::string> SimulationFactory::available_simulations() const {
    std::vector<std::string> names{};
    names.reserve(registry_.size());

    for (const auto& [id, _] : registry_) {
        names.push_back(id);
    }

    std::ranges::sort(names);
    return names;
}

}  // namespace omnisim::simulation
