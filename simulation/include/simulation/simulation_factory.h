#ifndef OMNISIM_SIMULATION_SIMULATION_FACTORY_H
#define OMNISIM_SIMULATION_SIMULATION_FACTORY_H

#include "simulation/i_simulation.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace omnisim::simulation {

class SimulationFactory {
public:
    using SimulationBuilder = std::function<std::unique_ptr<ISimulation>()>;

    bool register_simulation(std::string simulation_id, SimulationBuilder builder);
    [[nodiscard]] std::optional<std::unique_ptr<ISimulation>> create(std::string_view simulation_id) const;
    [[nodiscard]] std::vector<std::string> available_simulations() const;

private:
    std::unordered_map<std::string, SimulationBuilder> registry_{};
};

}  // namespace omnisim::simulation

#endif  // OMNISIM_SIMULATION_SIMULATION_FACTORY_H
