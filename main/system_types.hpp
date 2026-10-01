#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

constexpr uint32_t kMaximumVoltageMv = 30000;
constexpr uint32_t kMaximumCurrentMa = 30000;
constexpr uint32_t kMaximumPowerMw = 500000;
constexpr uint32_t kVoltageStepMv = 100;
constexpr uint32_t kCurrentStepMa = 10;
constexpr std::size_t kTemperatureProbeCount = 4;

enum class LoadMode : uint8_t {
    ConstantCurrent,
    ConstantVoltage,
};

enum class RunState : uint8_t {
    Starting,
    Idle,
    Running,
    Complete,
    Fault,
};

enum class StopReason : uint8_t {
    None,
    UserStopped,
    InvalidMeasurement,
    StaleMeasurement,
    OverVoltage,
    OverCurrent,
    OverPower,
    HeatsinkOverTemperature,
    ProbeOverTemperature,
    ProbeDisconnected,
    LowVoltageCutoff,
    HighVoltageCutoff,
    LowCurrentCutoff,
    HighCurrentCutoff,
    TimerExpired,
    OutputDriverFailure,
    LogFailure,
};

struct LoadSettings {
    LoadMode mode{LoadMode::ConstantCurrent};
    uint32_t current_setpoint_mA{1000};
    uint32_t voltage_setpoint_mV{12000};

    uint32_t over_voltage_mV{kMaximumVoltageMv};
    uint32_t over_current_mA{kMaximumCurrentMa};
    uint32_t over_power_mW{kMaximumPowerMw};
    int32_t heatsink_limit_mC{80000}; // demo value, not a validated thermal limit

    bool low_voltage_cutoff_enabled{false};
    uint32_t low_voltage_cutoff_mV{0};
    bool high_voltage_cutoff_enabled{false};
    uint32_t high_voltage_cutoff_mV{kMaximumVoltageMv};
    bool low_current_cutoff_enabled{false};
    uint32_t low_current_cutoff_mA{0};
    bool high_current_cutoff_enabled{false};
    uint32_t high_current_cutoff_mA{kMaximumCurrentMa};

    std::array<bool, kTemperatureProbeCount> probe_stop_enabled{false, false, false, false};
    std::array<int32_t, kTemperatureProbeCount> probe_limit_mC{
        60000, 60000, 60000, 60000};

    // Zero disables the timer.
    uint32_t maximum_run_time_s{0};
};

struct Measurements {
    int32_t voltage_mV{0};
    int32_t current_mA{0};
    int32_t heatsink_temperature_mC{0};
    std::array<int32_t, kTemperatureProbeCount> probe_temperature_mC{0, 0, 0, 0};
    std::array<bool, kTemperatureProbeCount> probe_connected{false, false, false, false};
    uint64_t timestamp_us{0};
    bool simulated{false};
    bool voltage_valid{false};
    bool current_valid{false};
    bool heatsink_valid{false};
    std::array<uint16_t, 8> adc_raw{};
    std::array<uint32_t, 8> adc_mV{};
    std::array<bool, 8> adc_valid{};
    bool valid{false};
};

struct SystemStatus {
    RunState state{RunState::Starting};
    StopReason stop_reason{StopReason::None};
    LoadSettings settings{};
    // Acknowledges handled setting edits (including rejected/cancelled requests).
    uint32_t settings_revision{0};
    Measurements measurements{};
    uint32_t elapsed_s{0};
    // Requested load output: 0 = off, 10000 = maximum.
    uint16_t output_level{0};
};

inline uint32_t calculate_power_mW(const Measurements &measurements)
{
    if (measurements.voltage_mV <= 0 || measurements.current_mA <= 0) {
        return 0;
    }

    return static_cast<uint32_t>(
        (static_cast<uint64_t>(measurements.voltage_mV) *
         static_cast<uint64_t>(measurements.current_mA)) /
        1000ULL);
}

inline bool settings_are_valid(const LoadSettings &settings)
{
    const bool setpoints_valid =
        (settings.mode == LoadMode::ConstantCurrent || settings.mode == LoadMode::ConstantVoltage) &&
        settings.current_setpoint_mA <= kMaximumCurrentMa &&
        settings.current_setpoint_mA % kCurrentStepMa == 0 &&
        settings.voltage_setpoint_mV <= kMaximumVoltageMv &&
        settings.voltage_setpoint_mV % kVoltageStepMv == 0;

    const bool safety_limits_valid =
        settings.over_voltage_mV > 0 && settings.over_current_mA > 0 &&
        settings.over_power_mW > 0 &&
        settings.over_voltage_mV <= kMaximumVoltageMv &&
        settings.over_current_mA <= kMaximumCurrentMa &&
        settings.over_power_mW <= kMaximumPowerMw &&
        settings.heatsink_limit_mC >= 0 && settings.heatsink_limit_mC <= 150000;

    const bool cutoffs_valid =
        settings.low_voltage_cutoff_mV <= kMaximumVoltageMv &&
        settings.high_voltage_cutoff_mV <= kMaximumVoltageMv &&
        settings.low_current_cutoff_mA <= kMaximumCurrentMa &&
        settings.high_current_cutoff_mA <= kMaximumCurrentMa &&
        (!settings.low_voltage_cutoff_enabled || !settings.high_voltage_cutoff_enabled ||
         settings.low_voltage_cutoff_mV <= settings.high_voltage_cutoff_mV) &&
        (!settings.low_current_cutoff_enabled || !settings.high_current_cutoff_enabled ||
         settings.low_current_cutoff_mA <= settings.high_current_cutoff_mA);

    bool probe_limits_valid = true;
    for (const int32_t limit_mC : settings.probe_limit_mC) {
        probe_limits_valid =
            probe_limits_valid && limit_mC >= -40000 && limit_mC <= 150000;
    }

    return setpoints_valid && safety_limits_valid && cutoffs_valid && probe_limits_valid;
}

const char *run_state_name(RunState state);
const char *stop_reason_name(StopReason reason);
