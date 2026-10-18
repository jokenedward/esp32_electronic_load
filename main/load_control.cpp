#include "load_control.hpp"

#include <algorithm>
#include <cmath>

void LoadController::reset()
{
    integral_ = 0.0F;
    output_level_ = 0;
}

uint16_t LoadController::update(
    const Measurements &measurements,
    const LoadSettings &settings,
    float time_step_s)
{
    if (!measurements.valid || time_step_s <= 0.0F) {
        reset();
        return 0;
    }

    float error = 0.0F;
    float proportional_gain = 0.0F;
    float integral_gain = 0.0F;

    if (settings.mode == LoadMode::ConstantCurrent) {
        error = (static_cast<float>(settings.current_setpoint_mA) -
                 static_cast<float>(measurements.current_mA)) /
                static_cast<float>(kMaximumCurrentMa);
        proportional_gain = gains_.cc_kp;
        integral_gain = gains_.cc_ki;
    } else {
        // An electronic load draws more current when the input is above the CV target.
        error = (static_cast<float>(measurements.voltage_mV) -
                 static_cast<float>(settings.voltage_setpoint_mV)) /
                static_cast<float>(kMaximumVoltageMv);
        proportional_gain = gains_.cv_kp;
        integral_gain = gains_.cv_ki;
    }

    const float next_integral = integral_ + integral_gain * error * time_step_s;
    const float next_output = proportional_gain * error + next_integral;

    // Anti-windup: integrate normally inside the output range, or when the
    // present error would bring a saturated output back into range.
    if ((next_output > 0.0F && next_output < 1.0F) ||
        (next_output >= 1.0F && error < 0.0F) ||
        (next_output <= 0.0F && error > 0.0F)) {
        integral_ = next_integral;
    }

    const float output =
        std::clamp(proportional_gain * error + integral_, 0.0F, 1.0F);
    output_level_ = static_cast<uint16_t>(std::lround(output * 10000.0F));
    return output_level_;
}
