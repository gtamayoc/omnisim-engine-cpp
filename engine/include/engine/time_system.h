#ifndef OMNISIM_ENGINE_TIME_SYSTEM_H
#define OMNISIM_ENGINE_TIME_SYSTEM_H

#include <cstddef>

namespace omnisim::engine {

struct TimeConfig {
    double fixed_dt_seconds{1.0 / 60.0};
    std::size_t max_steps{600};
};

class TimeSystem {
public:
    explicit TimeSystem(TimeConfig config) : config_(config) {}

    [[nodiscard]] double fixed_dt() const noexcept { return config_.fixed_dt_seconds; }
    [[nodiscard]] std::size_t max_steps() const noexcept { return config_.max_steps; }

private:
    TimeConfig config_{};
};

}  // namespace omnisim::engine

#endif  // OMNISIM_ENGINE_TIME_SYSTEM_H
