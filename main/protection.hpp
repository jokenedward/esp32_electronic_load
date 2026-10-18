#pragma once
#include "system_types.hpp"

// VoltageMonitor deliberately cannot provide current/temperature protection.
enum class ProtectionScope { VoltageOnly, AllSensors };
constexpr uint64_t kMaximumMeasurementAgeUs = 250000;
constexpr uint64_t kFaultRecoveryTimeUs = 2000000;
constexpr uint32_t kLowCurrentStartupGraceS = 1;
constexpr int32_t kTemperatureRecoveryMarginMc = 5000;

StopReason check_start_protection(const Measurements &, const LoadSettings &,
    uint64_t now_us, ProtectionScope scope = ProtectionScope::AllSensors);
StopReason check_protection(const Measurements &, const LoadSettings &,
    uint32_t elapsed_s, uint64_t now_us,
    ProtectionScope scope = ProtectionScope::AllSensors);
bool is_normal_completion(StopReason reason);
bool record_stop(SystemStatus &status, StopReason reason);
bool acknowledge_fault(SystemStatus &status);

// A fault stays latched until the readings have recovered AND the user presses
// acknowledge. This class checks recovery only; it never starts the load.
class FaultRecovery {
public:
    void reset();
    bool update(const Measurements &, const LoadSettings &, StopReason fault,
        uint64_t now_us, ProtectionScope scope = ProtectionScope::AllSensors);
private:
    uint64_t healthy_since_us_{0}, last_check_us_{0};
    bool tracking_{false};
};
