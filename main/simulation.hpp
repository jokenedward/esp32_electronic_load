#pragma once

#include <algorithm>
#include "system_types.hpp"

// Test inputs only. The same small model is used by firmware and computer tests.
enum class SimulationFault : uint8_t {
    None, OverVoltage, OverCurrent, OverPower, HotHeatsink,
    SensorFailure, StaleSensor, HotProbe, MissingProbe,
};

inline const char *simulation_fault_name(SimulationFault fault)
{
    switch (fault) {
    case SimulationFault::None: return "NORMAL";
    case SimulationFault::OverVoltage: return "31 V INPUT";
    case SimulationFault::OverCurrent: return "31 A INPUT";
    case SimulationFault::OverPower: return "20 V / 26 A INPUT (520 W)";
    case SimulationFault::HotHeatsink: return "95 C HEATSINK";
    case SimulationFault::SensorFailure: return "MISSING CURRENT READING";
    case SimulationFault::StaleSensor: return "MISSING SAMPLE TIMESTAMP";
    case SimulationFault::HotProbe: return "70 C EXTERNAL PROBE 1";
    case SimulationFault::MissingProbe: return "EXTERNAL PROBE 1 DISCONNECTED";
    }
    return "UNKNOWN";
}

inline Measurements simulation_measurements(uint16_t output_level, uint64_t now_us,
                                            SimulationFault fault = SimulationFault::None)
{
    Measurements m{};
    m.simulated = true;
    m.timestamp_us = now_us;
    m.current_mA = std::min<uint32_t>(output_level, 10000) * 30000U / 10000U;
    m.voltage_mV = std::max<int32_t>(12000 - m.current_mA / 10, 0);
    m.heatsink_temperature_mC = 25000 + m.current_mA / 20;
    m.probe_temperature_mC.fill(25000);
    m.probe_connected.fill(true);
    m.voltage_valid = m.current_valid = m.heatsink_valid = m.valid = true;

    switch (fault) {
    case SimulationFault::None: break;
    case SimulationFault::OverVoltage: m.voltage_mV = 31000; break;
    case SimulationFault::OverCurrent: m.current_mA = 31000; break;
    case SimulationFault::OverPower: m.voltage_mV = 20000; m.current_mA = 26000; break;
    case SimulationFault::HotHeatsink: m.heatsink_temperature_mC = 95000; break;
    case SimulationFault::SensorFailure: m.current_valid = m.valid = false; break;
    case SimulationFault::StaleSensor: m.timestamp_us = 0; break;
    case SimulationFault::HotProbe: m.probe_temperature_mC[0] = 70000; break;
    case SimulationFault::MissingProbe: m.probe_connected[0] = false; break;
    }
    return m;
}
