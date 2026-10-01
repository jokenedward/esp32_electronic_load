#pragma once

#include <cstdint>

#include "system_types.hpp"

struct ControlGains {
    float cc_kp{0.0F};
    float cc_ki{0.0F};
    float cv_kp{0.0F};
    float cv_ki{0.0F};
};

class LoadController {
public:
    explicit LoadController(const ControlGains &gains) : gains_(gains) {}

    void reset();
    uint16_t update(
        const Measurements &measurements,
        const LoadSettings &settings,
        float time_step_s);

private:
    ControlGains gains_{};
    float integral_{0.0F};
    uint16_t output_level_{0};
};

StopReason check_protection(
    const Measurements &measurements,
    const LoadSettings &settings,
    uint32_t elapsed_s,
    uint64_t now_us);

bool is_normal_completion(StopReason reason);
