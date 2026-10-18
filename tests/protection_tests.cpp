#include <cassert>
#include <cstdio>
#include <climits>
#include "protection.hpp"

namespace {
Measurements healthy(uint64_t now = 1000000)
{
    Measurements m{};
    m.valid = m.voltage_valid = m.current_valid = m.heatsink_valid = true;
    m.voltage_mV = 12000; m.current_mA = 1000; m.heatsink_temperature_mC = 25000;
    m.probe_connected.fill(true); m.probe_temperature_mC.fill(25000);
    m.timestamp_us = now;
    return m;
}

void measurement_tests()
{
    auto m = healthy(); LoadSettings s;
    assert(check_start_protection(m, s, m.timestamp_us) == StopReason::None);
    assert(check_protection(m, s, 0, m.timestamp_us + kMaximumMeasurementAgeUs) == StopReason::None);
    assert(check_protection(m, s, 0, m.timestamp_us + kMaximumMeasurementAgeUs + 1) == StopReason::StaleMeasurement);
    m.timestamp_us = 0;
    assert(check_start_protection(m, s, 1000000) == StopReason::StaleMeasurement);
    m = healthy();
    assert(check_start_protection(m, s, m.timestamp_us - 1) == StopReason::StaleMeasurement);
    m.voltage_mV = 30000;
    assert(check_start_protection(m, s, m.timestamp_us) == StopReason::None);
    ++m.voltage_mV;
    assert(check_start_protection(m, s, m.timestamp_us) == StopReason::OverVoltage);
    m = healthy(); m.current_mA = 30000;
    assert(check_start_protection(m, s, m.timestamp_us) == StopReason::None);
    ++m.current_mA;
    assert(check_start_protection(m, s, m.timestamp_us) == StopReason::OverCurrent);
    m = healthy(); m.voltage_mV = 20000; m.current_mA = 25000;
    assert(check_start_protection(m, s, m.timestamp_us) == StopReason::None);
    ++m.current_mA;
    assert(check_start_protection(m, s, m.timestamp_us) == StopReason::OverPower);
    m = healthy(); m.heatsink_temperature_mC = 80000;
    assert(check_start_protection(m, s, m.timestamp_us) == StopReason::None);
    ++m.heatsink_temperature_mC;
    assert(check_start_protection(m, s, m.timestamp_us) == StopReason::HeatsinkOverTemperature);
    m = healthy(); m.voltage_mV = m.current_mA = INT_MAX;
    assert(calculate_power_mW(m) == UINT32_MAX);
    assert(check_start_protection(m, s, m.timestamp_us) == StopReason::OverVoltage);
    m = healthy(); m.heatsink_temperature_mC = -40001;
    assert(check_start_protection(m, s, m.timestamp_us) == StopReason::InvalidMeasurement);
    m = healthy(); m.probe_temperature_mC[0] = INT_MAX; s.probe_stop_enabled[0] = true;
    assert(check_start_protection(m, s, m.timestamp_us) == StopReason::InvalidMeasurement);
    s = {}; s.over_voltage_mV = UINT32_MAX;
    assert(check_start_protection(m, s, m.timestamp_us) == StopReason::InvalidSettings);
    s = {}; m = healthy(); s.low_current_cutoff_enabled = true; s.low_current_cutoff_mA = 2000;
    assert(check_start_protection(m, s, m.timestamp_us) == StopReason::None);
    assert(check_protection(m, s, 0, m.timestamp_us) == StopReason::None);
    assert(check_protection(m, s, 1, m.timestamp_us) == StopReason::LowCurrentCutoff);
    m.current_mA = 30001; // Upper limits are never delayed by startup grace.
    assert(check_protection(m, s, 0, m.timestamp_us) == StopReason::OverCurrent);
    m = healthy(); s = {}; s.low_voltage_cutoff_enabled = true; s.low_voltage_cutoff_mV = 13000;
    assert(check_start_protection(m, s, m.timestamp_us) == StopReason::LowVoltageCutoff);
    assert(!is_normal_completion(StopReason::InvalidSettings));
    assert(!is_normal_completion(StopReason::ControlLoopTimeout));
}

void monitor_tests()
{
    auto m = healthy(); LoadSettings s;
    m.valid = m.current_valid = m.heatsink_valid = false;
    m.probe_connected.fill(false); s.probe_stop_enabled.fill(true);
    assert(check_protection(m, s, 0, m.timestamp_us, ProtectionScope::VoltageOnly) == StopReason::None);
    assert(check_protection(m, s, 0, m.timestamp_us, ProtectionScope::AllSensors) == StopReason::InvalidMeasurement);
    m.voltage_mV = 30001;
    assert(check_protection(m, s, 0, m.timestamp_us, ProtectionScope::VoltageOnly) == StopReason::OverVoltage);
    m.voltage_mV = 12000; s.maximum_run_time_s = 5;
    assert(check_protection(m, s, 5, m.timestamp_us, ProtectionScope::VoltageOnly) == StopReason::TimerExpired);
    m.voltage_valid = false;
    assert(check_protection(m, s, 0, m.timestamp_us, ProtectionScope::VoltageOnly) == StopReason::InvalidMeasurement);
    m = healthy(); m.simulated = false; m.current_mA = 30001;
    assert(check_protection(m, {}, 0, m.timestamp_us) == StopReason::OverCurrent);
}

bool recovered(FaultRecovery &recovery, Measurements m, const LoadSettings &s,
               StopReason fault, uint64_t start)
{
    bool ready = false;
    for (uint64_t offset = 0; offset <= kFaultRecoveryTimeUs; offset += 50000) {
        m.timestamp_us = start + offset;
        ready = recovery.update(m, s, fault, m.timestamp_us);
        if (offset < kFaultRecoveryTimeUs) assert(!ready);
    }
    return ready;
}

void recovery_tests()
{
    FaultRecovery recovery; auto m = healthy(); LoadSettings s;
    m.heatsink_temperature_mC = 76000;
    assert(!recovery.update(m, s, StopReason::HeatsinkOverTemperature, m.timestamp_us));
    m.heatsink_temperature_mC = 75000;
    assert(recovered(recovery, m, s, StopReason::HeatsinkOverTemperature, 2000000));
    m.timestamp_us = 4050000; m.heatsink_temperature_mC = 80001;
    assert(!recovery.update(m, s, StopReason::HeatsinkOverTemperature, m.timestamp_us));
    m.heatsink_temperature_mC = 25000;
    assert(recovered(recovery, m, s, StopReason::HeatsinkOverTemperature, 4100000));
    m.timestamp_us = 7000000; // Missing scans restart the recovery period.
    assert(!recovery.update(m, s, StopReason::HeatsinkOverTemperature, m.timestamp_us));
    assert(!recovery.update(m, s, StopReason::HeatsinkOverTemperature, 8000000));
    recovery.reset(); m = healthy(); m.voltage_mV = 29000;
    assert(!recovery.update(m, s, StopReason::OverVoltage, m.timestamp_us));
    m.voltage_mV = 28500;
    assert(recovered(recovery, m, s, StopReason::OverVoltage, 1000000));
    recovery.reset(); m = healthy(); m.current_mA = 29000;
    assert(!recovery.update(m, s, StopReason::OverCurrent, m.timestamp_us));
    m.current_mA = 28500;
    assert(recovered(recovery, m, s, StopReason::OverCurrent, 1000000));
    recovery.reset(); m = healthy(); m.voltage_mV = 20000; m.current_mA = 24000;
    assert(!recovery.update(m, s, StopReason::OverPower, m.timestamp_us));
    m.current_mA = 23750;
    assert(recovered(recovery, m, s, StopReason::OverPower, 1000000));
    for (unsigned i = 0; i < 4; ++i) {
        recovery.reset(); m = healthy(); s = {}; s.probe_stop_enabled[i] = true;
        m.probe_connected[i] = false;
        assert(!recovery.update(m, s, StopReason::ProbeDisconnected, m.timestamp_us));
        m.probe_connected[i] = true; m.probe_temperature_mC[i] = 56000;
        assert(!recovery.update(m, s, StopReason::ProbeOverTemperature, m.timestamp_us));
        m.probe_temperature_mC[i] = 55000;
        assert(recovered(recovery, m, s, StopReason::ProbeOverTemperature, 1000000));
    }
    recovery.reset(); m = healthy(); s = {}; s.heatsink_limit_mC = INT_MIN;
    assert(!recovery.update(m, s, StopReason::HeatsinkOverTemperature, m.timestamp_us));
    s = {}; m.voltage_valid = false;
    assert(!recovery.update(m, s, StopReason::InvalidMeasurement, m.timestamp_us));
    m = healthy();
    assert(recovered(recovery, m, s, StopReason::InvalidMeasurement, 1000000));
    recovery.reset();
    assert(!recovery.update(m, s, StopReason::InvalidMeasurement, m.timestamp_us));
    assert(!recovery.update(m, s, StopReason::InvalidMeasurement, m.timestamp_us - 1));
}

void fault_latch_tests()
{
    SystemStatus status;
    status.state = RunState::Running; status.output_level = 4000;
    assert(record_stop(status, StopReason::OverCurrent));
    assert(status.state == RunState::Fault && status.output_level == 0);
    assert(status.stop_reason == StopReason::OverCurrent);
    assert(!acknowledge_fault(status));
    assert(!record_stop(status, StopReason::UserStopped));
    assert(!record_stop(status, StopReason::LogFailure));
    assert(status.state == RunState::Fault && status.stop_reason == StopReason::OverCurrent);
    status.fault_can_acknowledge = true;
    assert(acknowledge_fault(status));
    assert(status.state == RunState::Idle && status.output_level == 0);
    assert(!status.fault_can_acknowledge && status.stop_reason == StopReason::None);
    assert(!acknowledge_fault(status));
    status.state = RunState::Preparing;
    assert(record_stop(status, StopReason::UserStopped));
    assert(status.state == RunState::Complete && !run_is_active(status.state));
    assert(record_stop(status, StopReason::LogFailure)); // Late write/close failure.
    assert(status.state == RunState::Fault);
    assert(!record_stop(status, StopReason::None));
}
}

void protection_regression_tests()
{
    measurement_tests(); monitor_tests(); recovery_tests(); fault_latch_tests();
    std::puts("PASS start interlocks, trip boundaries, monitor scopes, startup grace, fault latch and recovery");
}
