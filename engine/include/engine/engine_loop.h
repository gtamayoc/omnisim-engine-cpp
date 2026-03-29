#ifndef OMNISIM_ENGINE_ENGINE_LOOP_H
#define OMNISIM_ENGINE_ENGINE_LOOP_H

#include "engine/time_system.h"
#include "simulation/i_simulation.h"

#include <cstddef>
#include <memory>

namespace omnisim::engine {

class EngineLoop {
public:
    explicit EngineLoop(TimeConfig time_config);

    [[nodiscard]] std::size_t run(std::unique_ptr<simulation::ISimulation> simulation) const;

private:
    TimeSystem time_system_;
};

}  // namespace omnisim::engine

#endif  // OMNISIM_ENGINE_ENGINE_LOOP_H
