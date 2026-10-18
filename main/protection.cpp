#include "protection.hpp"

namespace {
bool temperature_plausible(int32_t value)
{
    return value >= -40000 && value <= 150000;
}

StopReason measurement_limits(const Measurements &m, const LoadSettings &s,
                             uint64_t now, ProtectionScope scope)
{
    if (!settings_are_valid(s)) return StopReason::InvalidSettings;
    if (!m.voltage_valid || m.voltage_mV < 0) return StopReason::InvalidMeasurement;
    if (m.timestamp_us == 0 || now < m.timestamp_us ||
        now - m.timestamp_us > kMaximumMeasurementAgeUs)
        return StopReason::StaleMeasurement;
    if (static_cast<uint32_t>(m.voltage_mV) > s.over_voltage_mV)
        return StopReason::OverVoltage;
    if (scope == ProtectionScope::VoltageOnly) return StopReason::None;

    if (!m.valid || !m.current_valid || !m.heatsink_valid || m.current_mA < 0 ||
        !temperature_plausible(m.heatsink_temperature_mC))
        return StopReason::InvalidMeasurement;
    if (static_cast<uint32_t>(m.current_mA) > s.over_current_mA)
        return StopReason::OverCurrent;
    // Compare before narrowing to 32 bits; a corrupt large reading cannot wrap.
    const uint64_t power = static_cast<uint64_t>(m.voltage_mV) * m.current_mA / 1000;
    if (power > s.over_power_mW) return StopReason::OverPower;
    if (m.heatsink_temperature_mC > s.heatsink_limit_mC)
        return StopReason::HeatsinkOverTemperature;
    for (unsigned i = 0; i < kTemperatureProbeCount; ++i) {
        if (!s.probe_stop_enabled[i]) continue;
        if (!m.probe_connected[i]) return StopReason::ProbeDisconnected;
        if (!temperature_plausible(m.probe_temperature_mC[i]))
            return StopReason::InvalidMeasurement;
        if (m.probe_temperature_mC[i] > s.probe_limit_mC[i])
            return StopReason::ProbeOverTemperature;
    }
    return StopReason::None;
}
}

StopReason check_start_protection(const Measurements &m, const LoadSettings &s,
                                 uint64_t now, ProtectionScope scope)
{
    const StopReason fault = measurement_limits(m, s, now, scope);
    if (fault != StopReason::None) return fault;
    if (s.low_voltage_cutoff_enabled && m.voltage_mV < static_cast<int32_t>(s.low_voltage_cutoff_mV))
        return StopReason::LowVoltageCutoff;
    if (s.high_voltage_cutoff_enabled && m.voltage_mV > static_cast<int32_t>(s.high_voltage_cutoff_mV))
        return StopReason::HighVoltageCutoff;
    if (scope == ProtectionScope::AllSensors && s.high_current_cutoff_enabled &&
        m.current_mA > static_cast<int32_t>(s.high_current_cutoff_mA))
        return StopReason::HighCurrentCutoff;
    // Low current is expected while the output is still off.
    return StopReason::None;
}

StopReason check_protection(const Measurements &m, const LoadSettings &s,
                           uint32_t elapsed, uint64_t now, ProtectionScope scope)
{
    const StopReason fault = check_start_protection(m, s, now, scope);
    if (fault != StopReason::None) return fault;
    if (scope == ProtectionScope::AllSensors && s.low_current_cutoff_enabled &&
        elapsed >= kLowCurrentStartupGraceS &&
        m.current_mA < static_cast<int32_t>(s.low_current_cutoff_mA))
        return StopReason::LowCurrentCutoff;
    if (s.maximum_run_time_s > 0 && elapsed >= s.maximum_run_time_s)
        return StopReason::TimerExpired;
    return StopReason::None;
}

bool is_normal_completion(StopReason reason)
{
    return reason == StopReason::UserStopped || reason == StopReason::TimerExpired ||
        reason == StopReason::LowVoltageCutoff || reason == StopReason::HighVoltageCutoff ||
        reason == StopReason::LowCurrentCutoff || reason == StopReason::HighCurrentCutoff;
}

bool record_stop(SystemStatus &status, StopReason reason)
{
    if (reason == StopReason::None) return false;
    status.output_level = 0;
    if (status.state == RunState::Fault) return false; // Preserve the first fault.
    status.stop_reason = reason;
    status.state = is_normal_completion(reason) ? RunState::Complete : RunState::Fault;
    status.fault_can_acknowledge = false;
    ++status.settings_revision;
    return true;
}

bool acknowledge_fault(SystemStatus &status)
{
    if (status.state != RunState::Fault || !status.fault_can_acknowledge) return false;
    status.state = RunState::Idle;
    status.stop_reason = StopReason::None;
    status.output_level = 0;
    status.fault_can_acknowledge = false;
    ++status.settings_revision;
    return true;
}

void FaultRecovery::reset()
{
    healthy_since_us_ = last_check_us_ = 0;
    tracking_ = false;
}

bool FaultRecovery::update(const Measurements &m, const LoadSettings &s,
                          StopReason fault, uint64_t now, ProtectionScope scope)
{
    if (measurement_limits(m, s, now, scope) != StopReason::None) {
        reset();
        return false;
    }
    bool safe = true;
    // Electrical trips must fall to 95% of the limit before acknowledgement.
    if (fault == StopReason::OverVoltage)
        safe &= m.voltage_mV >= 0 && static_cast<uint64_t>(m.voltage_mV) * 100 <= s.over_voltage_mV * 95ULL;
    if (scope == ProtectionScope::AllSensors) {
        if (fault == StopReason::OverCurrent)
            safe &= m.current_mA >= 0 && static_cast<uint64_t>(m.current_mA) * 100 <= s.over_current_mA * 95ULL;
        if (fault == StopReason::OverPower)
            safe &= m.voltage_mV >= 0 && m.current_mA >= 0 &&
                static_cast<uint64_t>(m.voltage_mV) * m.current_mA <= s.over_power_mW * 950ULL;
        if (fault == StopReason::HeatsinkOverTemperature)
            safe &= m.heatsink_temperature_mC <= s.heatsink_limit_mC - kTemperatureRecoveryMarginMc;
        if (fault == StopReason::ProbeOverTemperature)
            for (unsigned i = 0; i < kTemperatureProbeCount; ++i)
                if (s.probe_stop_enabled[i])
                    safe &= m.probe_temperature_mC[i] <= s.probe_limit_mC[i] - kTemperatureRecoveryMarginMc;
    }
    if (!safe) { reset(); return false; }
    if (!tracking_ || now < last_check_us_ || now - last_check_us_ > kMaximumMeasurementAgeUs) {
        healthy_since_us_ = now;
        tracking_ = true;
    }
    last_check_us_ = now;
    return now - healthy_since_us_ >= kFaultRecoveryTimeUs;
}
