#include "engine/engine_loop.h"

#include <stdexcept>

namespace omnisim::engine {

EngineLoop::EngineLoop(TimeConfig time_config) : time_system_(time_config) {}

std::size_t EngineLoop::run(std::unique_ptr<simulation::ISimulation> simulation) const {
    if (!simulation) {
        throw std::invalid_argument{"EngineLoop requires a simulation instance"};
    }

    simulation->initialize();

    std::size_t step_count = 0;
    while (step_count < time_system_.max_steps() && !simulation->is_finished()) {
        simulation->step(time_system_.fixed_dt());
        simulation->print_state();
        ++step_count;
    }
    return step_count;
}

}  // namespace omnisim::engine
