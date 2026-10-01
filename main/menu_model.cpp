#include "menu_model.hpp"
#include <algorithm>
#include <cstdio>

namespace {
void step(uint32_t &value, int direction, uint32_t increment, uint32_t low, uint32_t high)
{
    value = static_cast<uint32_t>(std::clamp<int64_t>(
        static_cast<int64_t>(value) + direction * static_cast<int64_t>(increment), low, high));
}
void temperature_step(int32_t &value, int direction, int32_t low = -40000)
{
    value = std::clamp<int32_t>(value + direction * 1000, low, 150000);
}
const char *enabled(bool value) { return value ? "ON" : "OFF"; }
}

int RotaryDecoder::update(uint8_t state, int transitions)
{
    static constexpr int8_t movement[16] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};
    state &= 3;
    if ((state ^ previous_) == 3) movement_ = 0;
    else movement_ += movement[(previous_ << 2) | state];
    previous_ = state;
    if (transitions <= 0) return 0;
    if (movement_ >= transitions) { movement_ = 0; return 1; }
    if (movement_ <= -transitions) { movement_ = 0; return -1; }
    return 0;
}

int MenuModel::field_count() const
{
    switch (screen) {
    case Screen::Main: return 7;
    case Screen::Mode: return 3;
    case Screen::Protection: return 4;
    case Screen::AutoStop: return 9;
    case Screen::Probes: return 8;
    case Screen::Logs: return 2;
    default: return 1;
    }
}
const char *MenuModel::title() const
{
    switch (screen) {
    case Screen::Main: return "ELECTRONIC LOAD";
    case Screen::Live: return "LIVE MEASUREMENTS";
    case Screen::Mode: return "MODE AND SETPOINT";
    case Screen::Protection: return "PROTECTION";
    case Screen::AutoStop: return "AUTO STOP";
    case Screen::Probes: return "EXTERNAL PROBES";
    case Screen::Logs: return "LOG DOWNLOAD";
    case Screen::Adc: return "ADC DIAGNOSTICS";
    }
    return "";
}

bool MenuModel::handle(UiEvent event, const SystemStatus &status, UiCommand *command)
{
    if (!command || event == UiEvent::None) return false;
    if (save_pending_ && status.settings_revision != saved_revision_)
        save_pending_ = false;
    message = "";
    if (event == UiEvent::Back) {
        if (editing) { editing = false; draft = status.settings; message = "EDIT CANCELLED"; }
        else { screen = Screen::Main; selected = 0; }
        return false;
    }
    if (event == UiEvent::Next || event == UiEvent::Previous) {
        const int direction = event == UiEvent::Next ? 1 : -1;
        if (editing) adjust(direction);
        else selected = (selected + direction + field_count()) % field_count();
        return false;
    }
    if (event != UiEvent::Select) return false;
    if (screen == Screen::Main) {
        screen = static_cast<Screen>(selected + 1);
        selected = 0;
        draft = status.settings;
        return false;
    }
    if (screen == Screen::Live) {
        command->type = status.state == RunState::Running ? UiCommandType::Stop :
                        status.state == RunState::Fault ? UiCommandType::Acknowledge :
                                                         UiCommandType::Start;
        return true;
    }
    if (screen == Screen::Adc) return false;
    if (screen == Screen::Logs) {
        if (selected == 1) { screen = Screen::Main; selected = 0; return false; }
        if (status.state == RunState::Running) { message = "STOP BEFORE DOWNLOAD"; return false; }
        command->type = UiCommandType::DownloadLog;
        return true;
    }
    if (status.state == RunState::Running) {
        message = "STOP BEFORE EDITING";
        editing = false;
        return false;
    }
    if (!editing) {
        // Wait for the application to receive the previous edit. A fast second
        // edit must not overwrite it with settings from an older snapshot.
        if (save_pending_) { message = "WAIT FOR SETTING TO APPLY"; return false; }
        draft = status.settings;
        editing = true;
        return false;
    }
    if (!settings_are_valid(draft)) { message = "INVALID LIMITS - EDIT OR BACK"; return false; }
    editing = false;
    command->type = UiCommandType::ApplySettings;
    command->settings = draft;
    saved_revision_ = status.settings_revision;
    save_pending_ = true;
    message = "SETTING SENT";
    return true;
}

void MenuModel::adjust(int direction)
{
    switch (screen) {
    case Screen::Mode:
        if (selected == 0) draft.mode = draft.mode == LoadMode::ConstantCurrent ?
            LoadMode::ConstantVoltage : LoadMode::ConstantCurrent;
        if (selected == 1) step(draft.current_setpoint_mA, direction, 10, 0, kMaximumCurrentMa);
        if (selected == 2) step(draft.voltage_setpoint_mV, direction, 100, 0, kMaximumVoltageMv);
        break;
    case Screen::Protection:
        if (selected == 0) step(draft.over_voltage_mV, direction, 100, 100, kMaximumVoltageMv);
        if (selected == 1) step(draft.over_current_mA, direction, 10, 10, kMaximumCurrentMa);
        if (selected == 2) step(draft.over_power_mW, direction, 1000, 1000, kMaximumPowerMw);
        if (selected == 3) temperature_step(draft.heatsink_limit_mC, direction, 0);
        break;
    case Screen::AutoStop:
        switch (selected) {
        case 0: draft.low_voltage_cutoff_enabled = !draft.low_voltage_cutoff_enabled; break;
        case 1: step(draft.low_voltage_cutoff_mV, direction, 100, 0, kMaximumVoltageMv); break;
        case 2: draft.high_voltage_cutoff_enabled = !draft.high_voltage_cutoff_enabled; break;
        case 3: step(draft.high_voltage_cutoff_mV, direction, 100, 0, kMaximumVoltageMv); break;
        case 4: draft.low_current_cutoff_enabled = !draft.low_current_cutoff_enabled; break;
        case 5: step(draft.low_current_cutoff_mA, direction, 10, 0, kMaximumCurrentMa); break;
        case 6: draft.high_current_cutoff_enabled = !draft.high_current_cutoff_enabled; break;
        case 7: step(draft.high_current_cutoff_mA, direction, 10, 0, kMaximumCurrentMa); break;
        case 8: step(draft.maximum_run_time_s, direction, 60, 0, 86400); break;
        }
        break;
    case Screen::Probes: {
        const unsigned index = static_cast<unsigned>(selected / 2);
        if (selected % 2 == 0) draft.probe_stop_enabled[index] = !draft.probe_stop_enabled[index];
        else temperature_step(draft.probe_limit_mC[index], direction);
        break;
    }
    default: break;
    }
}

void MenuModel::field_text(int index, char *label, unsigned ln, char *value, unsigned vn) const
{
    const auto text = [&](const char *l, const char *v) {
        std::snprintf(label, ln, "%s", l); std::snprintf(value, vn, "%s", v);
    };
    const auto quantity = [&](const char *l, int32_t amount, const char *unit) {
        std::snprintf(label, ln, "%s", l);
        std::snprintf(value, vn, "%.2f %s", static_cast<double>(amount)/1000, unit);
    };
    text("", "");
    switch (screen) {
    case Screen::Main: {
        static constexpr const char *items[] = {"LIVE MEASUREMENTS", "MODE AND SETPOINT",
            "PROTECTION", "AUTO STOP", "EXTERNAL PROBES", "LOG DOWNLOAD", "ADC DIAGNOSTICS"};
        if (index >= 0 && index < 7) text(items[index], "");
        break;
    }
    case Screen::Mode:
        if (index == 0) text("MODE", draft.mode == LoadMode::ConstantCurrent ? "CC" : "CV");
        if (index == 1) quantity("CURRENT TARGET", draft.current_setpoint_mA, "A");
        if (index == 2) quantity("VOLTAGE TARGET", draft.voltage_setpoint_mV, "V");
        break;
    case Screen::Protection:
        if (index == 0) quantity("OVER VOLTAGE", draft.over_voltage_mV, "V");
        if (index == 1) quantity("OVER CURRENT", draft.over_current_mA, "A");
        if (index == 2) quantity("OVER POWER", draft.over_power_mW, "W");
        if (index == 3) quantity("HEATSINK LIMIT", draft.heatsink_limit_mC, "C");
        break;
    case Screen::AutoStop:
        switch (index) {
        case 0: text("LOW VOLTAGE STOP", enabled(draft.low_voltage_cutoff_enabled)); break;
        case 1: quantity("LOW VOLTAGE", draft.low_voltage_cutoff_mV, "V"); break;
        case 2: text("HIGH VOLTAGE STOP", enabled(draft.high_voltage_cutoff_enabled)); break;
        case 3: quantity("HIGH VOLTAGE", draft.high_voltage_cutoff_mV, "V"); break;
        case 4: text("LOW CURRENT STOP", enabled(draft.low_current_cutoff_enabled)); break;
        case 5: quantity("LOW CURRENT", draft.low_current_cutoff_mA, "A"); break;
        case 6: text("HIGH CURRENT STOP", enabled(draft.high_current_cutoff_enabled)); break;
        case 7: quantity("HIGH CURRENT", draft.high_current_cutoff_mA, "A"); break;
        case 8:
            text("TIMER", "OFF");
            if (draft.maximum_run_time_s)
                std::snprintf(value, vn, "%lu MIN", static_cast<unsigned long>(draft.maximum_run_time_s/60));
            break;
        }
        break;
    case Screen::Probes:
        std::snprintf(label, ln, "PROBE %d %s", index/2 + 1, index%2 ? "LIMIT" : "STOP");
        if (index%2) std::snprintf(value, vn, "%.1f C", draft.probe_limit_mC[index/2]/1000.0);
        else std::snprintf(value, vn, "%s", enabled(draft.probe_stop_enabled[index/2]));
        break;
    case Screen::Logs: text(index == 0 ? "DOWNLOAD CSV OVER USB" : "BACK", ""); break;
    default: break;
    }
}
