#ifndef OMNISIM_SIMULATION_I_SIMULATION_H
#define OMNISIM_SIMULATION_I_SIMULATION_H

#include <string_view>

namespace omnisim::simulation {

class ISimulation {
public:
    virtual ~ISimulation() = default;

    virtual void initialize() = 0;
    virtual void step(double dt_seconds) = 0;
    [[nodiscard]] virtual bool is_finished() const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    virtual void print_state() const = 0;
};

}  // namespace omnisim::simulation

#endif  // OMNISIM_SIMULATION_I_SIMULATION_H
