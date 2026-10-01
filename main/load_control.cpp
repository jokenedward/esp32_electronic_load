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

StopReason check_protection(
    const Measurements &measurements,
    const LoadSettings &settings,
    uint32_t elapsed_s,
    uint64_t now_us)
{
    if (!measurements.valid || !measurements.voltage_valid || !measurements.current_valid ||
        !measurements.heatsink_valid || measurements.voltage_mV < 0 || measurements.current_mA < 0) {
        return StopReason::InvalidMeasurement;
    }
    if (now_us < measurements.timestamp_us || now_us - measurements.timestamp_us > 250000ULL) {
        return StopReason::StaleMeasurement;
    }

    if (static_cast<uint32_t>(measurements.voltage_mV) > settings.over_voltage_mV) {
        return StopReason::OverVoltage;
    }
    if (static_cast<uint32_t>(measurements.current_mA) > settings.over_current_mA) {
        return StopReason::OverCurrent;
    }
    if (calculate_power_mW(measurements) > settings.over_power_mW) {
        return StopReason::OverPower;
    }
    if (measurements.heatsink_temperature_mC > settings.heatsink_limit_mC) {
        return StopReason::HeatsinkOverTemperature;
    }

    for (std::size_t index = 0; index < kTemperatureProbeCount; ++index) {
        if (!settings.probe_stop_enabled[index]) {
            continue;
        }
        if (!measurements.probe_connected[index]) {
            return StopReason::ProbeDisconnected;
        }
        if (measurements.probe_temperature_mC[index] > settings.probe_limit_mC[index]) {
            return StopReason::ProbeOverTemperature;
        }
    }

    const uint32_t voltage_mV = static_cast<uint32_t>(measurements.voltage_mV);
    const uint32_t current_mA = static_cast<uint32_t>(measurements.current_mA);
    if (settings.low_voltage_cutoff_enabled && voltage_mV < settings.low_voltage_cutoff_mV) {
        return StopReason::LowVoltageCutoff;
    }
    if (settings.high_voltage_cutoff_enabled && voltage_mV > settings.high_voltage_cutoff_mV) {
        return StopReason::HighVoltageCutoff;
    }
    if (settings.low_current_cutoff_enabled && current_mA < settings.low_current_cutoff_mA) {
        return StopReason::LowCurrentCutoff;
    }
    if (settings.high_current_cutoff_enabled && current_mA > settings.high_current_cutoff_mA) {
        return StopReason::HighCurrentCutoff;
    }
    if (settings.maximum_run_time_s > 0 && elapsed_s >= settings.maximum_run_time_s) {
        return StopReason::TimerExpired;
    }

    return StopReason::None;
}

bool is_normal_completion(StopReason reason)
{
    return reason == StopReason::UserStopped ||
           reason == StopReason::LowVoltageCutoff ||
           reason == StopReason::HighVoltageCutoff ||
           reason == StopReason::LowCurrentCutoff ||
           reason == StopReason::HighCurrentCutoff || reason == StopReason::TimerExpired;
}
