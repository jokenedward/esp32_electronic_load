#include "system_types.hpp"

const char *run_state_name(RunState state)
{
    switch (state) {
    case RunState::Starting: return "STARTING";
    case RunState::Idle: return "IDLE";
    case RunState::Preparing: return "PREPARING";
    case RunState::Running: return "RUNNING";
    case RunState::Complete: return "COMPLETE";
    case RunState::Fault: return "FAULT";
    }
    return "UNKNOWN";
}

const char *stop_reason_name(StopReason reason)
{
    switch (reason) {
    case StopReason::None: return "NONE";
    case StopReason::UserStopped: return "USER_STOPPED";
    case StopReason::InvalidMeasurement: return "INVALID_MEASUREMENT";
    case StopReason::StaleMeasurement: return "STALE_MEASUREMENT";
    case StopReason::OverVoltage: return "OVER_VOLTAGE";
    case StopReason::OverCurrent: return "OVER_CURRENT";
    case StopReason::OverPower: return "OVER_POWER";
    case StopReason::HeatsinkOverTemperature: return "HEATSINK_OVER_TEMPERATURE";
    case StopReason::ProbeOverTemperature: return "PROBE_OVER_TEMPERATURE";
    case StopReason::ProbeDisconnected: return "PROBE_DISCONNECTED";
    case StopReason::LowVoltageCutoff: return "LOW_VOLTAGE_CUTOFF";
    case StopReason::HighVoltageCutoff: return "HIGH_VOLTAGE_CUTOFF";
    case StopReason::LowCurrentCutoff: return "LOW_CURRENT_CUTOFF";
    case StopReason::HighCurrentCutoff: return "HIGH_CURRENT_CUTOFF";
    case StopReason::TimerExpired: return "TIMER_EXPIRED";
    case StopReason::OutputDriverFailure: return "OUTPUT_DRIVER_FAILURE";
    case StopReason::LogFailure: return "LOG_FAILURE";
    case StopReason::InvalidSettings: return "INVALID_SETTINGS";
    case StopReason::ControlLoopTimeout: return "CONTROL_LOOP_TIMEOUT";
    }
    return "UNKNOWN";
}

