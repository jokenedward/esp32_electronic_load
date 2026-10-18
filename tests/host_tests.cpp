#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "sensor_math.hpp"
#include "menu_model.hpp"
#include "load_control.hpp"
#include "protection.hpp"
#include "simulation.hpp"

void protection_regression_tests();

namespace {
Measurements good_measurements()
{
    Measurements m{};
    m.valid = m.voltage_valid = m.current_valid = m.heatsink_valid = true;
    m.voltage_mV = 12000; m.current_mA = 1000; m.heatsink_temperature_mC = 25000;
    m.probe_connected.fill(true); m.probe_temperature_mC.fill(25000);
    m.timestamp_us = 1000000;
    return m;
}
void sensor_tests()
{
    constexpr uint8_t expected[]{0x8C,0xCC,0x9C,0xDC,0xAC,0xEC,0xBC,0xFC};
    for (int i = 0; i < 8; ++i) assert(ads7828_command(i) == expected[i]);
    assert(mcp4922_command(false, 0, true) == 0x3000);
    assert(mcp4922_command(true, 4095, true) == 0xBFFF);
    assert(mcp4922_command(false, 0, false) == 0x2000);
    assert(mcp4922_command(true, 0, false) == 0xA000);
    for (int i = 0; i < 256; ++i) assert(reverse_bits(reverse_bits(i)) == i);
    assert(reverse_bits(1) == 128);
    assert(scale_voltage_divider_mV(2308, 120000, 10000) == 30004);
    assert(scale_voltage_divider_mV(1250, 10000, 10000) == 2500);
    assert(scale_voltage_divider_mV(1000, 1000, 0) == 0);
    assert(scale_voltage_divider_mV(UINT32_MAX, UINT32_MAX, UINT32_MAX) == UINT32_MAX);
    assert(current_from_sensor_mA(3700, 2500, 40000, true) == 30000);
    assert(current_from_sensor_mA(1300, 2500, 40000, true) == -30000);
    assert(current_from_sensor_mA(1300, 2500, 40000, false) == 30000);
    int32_t temperature = 0;
    assert(pt1000_temperature_mC(1650, 3300, 1000, &temperature));
    assert(std::abs(temperature) < 2);
    for (int t : {-30, 0, 25, 80, 120}) {
        const double r = 1000*(1+3.9083e-3*t-5.775e-7*t*t+
            (t<0 ? -4.183e-12*(t-100)*t*t*t : 0));
        const auto mv = static_cast<uint32_t>(std::lround(3300*r/(1000+r)));
        assert(pt1000_temperature_mC(mv, 3300, 1000, &temperature));
        assert(std::abs(temperature-t*1000) < 700); // mV quantization
    }
    assert(!pt1000_temperature_mC(0, 3300, 1000, &temperature));
    assert(!pt1000_temperature_mC(3300, 3300, 1000, &temperature));
    assert(!pt1000_temperature_mC(10, 3300, 1000, &temperature));
    assert(!pt1000_temperature_mC(1650, 3300, 1000, nullptr));
    std::puts("PASS sensor conversions, ADC channel commands, DAC frames and bit reversal");
}
void protection_tests()
{
    const auto check = [](Measurements m, LoadSettings s = {}, uint32_t elapsed = 0) {
        return check_protection(m, s, elapsed, 1000000);
    };
    auto m = good_measurements(); LoadSettings s;
    assert(check(m) == StopReason::None);
    m.valid = false; assert(check(m) == StopReason::InvalidMeasurement);
    m = good_measurements(); m.heatsink_valid = false;
    assert(check(m) == StopReason::InvalidMeasurement);
    m = good_measurements(); m.timestamp_us = 1;
    assert(check(m) == StopReason::StaleMeasurement);
    m = good_measurements(); m.timestamp_us = 1000001;
    assert(check(m) == StopReason::StaleMeasurement);
    m = good_measurements(); m.voltage_mV = 30001;
    assert(check(m) == StopReason::OverVoltage);
    m = good_measurements(); m.current_mA = 30001;
    assert(check(m) == StopReason::OverCurrent);
    m = good_measurements(); m.voltage_mV = 30000; m.current_mA = 20000;
    assert(check(m) == StopReason::OverPower);
    m = good_measurements(); m.heatsink_temperature_mC = 80001;
    assert(check(m) == StopReason::HeatsinkOverTemperature);
    m = good_measurements(); m.current_mA = -1;
    assert(check(m) == StopReason::InvalidMeasurement);
    for (unsigned probe = 0; probe < 4; ++probe) {
        s = {}; s.probe_stop_enabled[probe] = true;
        m = good_measurements(); m.probe_connected[probe] = false;
        assert(check(m, s) == StopReason::ProbeDisconnected);
        m = good_measurements(); m.probe_temperature_mC[probe] = 60001;
        assert(check(m, s) == StopReason::ProbeOverTemperature);
    }
    m = good_measurements(); s = {}; s.low_voltage_cutoff_enabled = true;
    s.low_voltage_cutoff_mV = 13000; assert(check(m, s) == StopReason::LowVoltageCutoff);
    s = {}; s.high_voltage_cutoff_enabled = true; s.high_voltage_cutoff_mV = 11000;
    assert(check(m, s) == StopReason::HighVoltageCutoff);
    s = {}; s.low_current_cutoff_enabled = true; s.low_current_cutoff_mA = 2000;
    assert(check(m, s, 0) == StopReason::None);
    assert(check(m, s, kLowCurrentStartupGraceS) == StopReason::LowCurrentCutoff);
    s = {}; s.high_current_cutoff_enabled = true; s.high_current_cutoff_mA = 500;
    assert(check(m, s) == StopReason::HighCurrentCutoff);
    s = {}; s.maximum_run_time_s = 60;
    assert(check(m, s, 60) == StopReason::TimerExpired);
    assert(check(m, s, 59) == StopReason::None);
    s = {}; s.current_setpoint_mA = 1001; assert(!settings_are_valid(s));
    s = {}; s.voltage_setpoint_mV = 12001; assert(!settings_are_valid(s));
    s = {}; s.over_current_mA = 0; assert(!settings_are_valid(s));
    s = {}; s.mode = static_cast<LoadMode>(99); assert(!settings_are_valid(s));
    s = {}; s.low_voltage_cutoff_enabled = s.high_voltage_cutoff_enabled = true;
    s.low_voltage_cutoff_mV = 20000; s.high_voltage_cutoff_mV = 10000;
    assert(!settings_are_valid(s));
    std::puts("PASS settings validation and every protection/automatic-stop condition");
}
void controller_tests()
{
    LoadController controller({0.8F,1.2F,0.8F,1.2F});
    auto m = good_measurements(); LoadSettings s;
    s.current_setpoint_mA = 5000;
    uint16_t output = 0;
    for (int i = 0; i < 10000; ++i) {
        m = simulation_measurements(output, 1000000);
        output = controller.update(m, s, 0.01F);
        assert(output <= 10000);
    }
    assert(std::abs(m.current_mA-5000) < 20);
    controller.reset(); s.mode = LoadMode::ConstantVoltage; s.voltage_setpoint_mV = 11000;
    output = 0;
    for (int i = 0; i < 10000; ++i) {
        m = simulation_measurements(output, 1000000);
        output = controller.update(m, s, 0.01F);
    }
    assert(std::abs(m.voltage_mV-11000) < 10);
    m.valid = false; assert(controller.update(m, s, 0.01F) == 0);
    std::puts("PASS CC/CV simulated-plant convergence and invalid-data reset");
}

void simulation_tests()
{
    const auto idle = simulation_measurements(0, 1000000);
    assert(idle.simulated && idle.valid);
    assert(idle.voltage_mV == 12000 && idle.current_mA == 0);
    assert(idle.heatsink_temperature_mC == 25000);
    assert(check_start_protection(idle, {}, 1000000) == StopReason::None);

    const SimulationFault faults[]{SimulationFault::OverVoltage, SimulationFault::OverCurrent,
        SimulationFault::OverPower, SimulationFault::HotHeatsink,
        SimulationFault::SensorFailure, SimulationFault::StaleSensor,
        SimulationFault::HotProbe, SimulationFault::MissingProbe};
    const StopReason reasons[]{StopReason::OverVoltage, StopReason::OverCurrent,
        StopReason::OverPower, StopReason::HeatsinkOverTemperature,
        StopReason::InvalidMeasurement, StopReason::StaleMeasurement,
        StopReason::ProbeOverTemperature, StopReason::ProbeDisconnected};
    for (unsigned i = 0; i < 8; ++i) {
        SystemStatus status;
        status.state = RunState::Running;
        status.output_level = 1000;
        status.settings.probe_stop_enabled[0] = true;
        status.measurements = simulation_measurements(1000, 1000000, faults[i]);
        const auto reason = check_protection(status.measurements, status.settings, 2, 1000000);
        assert(reason == reasons[i]);
        assert(record_stop(status, reason));
        assert(status.state == RunState::Fault && status.output_level == 0);
        assert(!acknowledge_fault(status));

        FaultRecovery recovery;
        for (uint64_t now = 2000000; now <= 4000000; now += 50000) {
            status.measurements = simulation_measurements(0, now);
            status.fault_can_acknowledge = recovery.update(status.measurements,
                status.settings, reason, now);
        }
        assert(status.fault_can_acknowledge);
        assert(acknowledge_fault(status));
        assert(status.state == RunState::Idle && status.output_level == 0);
    }
    // Probe faults only stop a run when the corresponding option is enabled.
    assert(check_protection(simulation_measurements(0, 1000000, SimulationFault::MissingProbe),
        {}, 2, 1000000) == StopReason::None);
    assert(simulation_measurements(UINT16_MAX, 1000000).current_mA == 30000);
    std::puts("PASS PCB simulation inputs, all eight injected faults and recovery without restart");
}
void menu_tests()
{
    MenuModel menu; SystemStatus status; UiCommand command;
    status.state = RunState::Idle; status.measurements = good_measurements();
    const char *main_items[]{"LIVE MEASUREMENTS", "MODE AND SETPOINT", "PROTECTION",
        "AUTO STOP", "EXTERNAL PROBES", "LOG DOWNLOAD"};
    assert(menu.field_count() == 6);
    for (int i = 0; i < menu.field_count(); ++i) {
        char label[40], value[32];
        menu.field_text(i, label, sizeof(label), value, sizeof(value));
        assert(std::strcmp(label, main_items[i]) == 0);
        menu.selected = i;
        assert(!menu.handle(UiEvent::Select, status, &command));
        assert(menu.screen == static_cast<MenuModel::Screen>(i + 1));
        menu.selected = menu.field_count() - 1;
        menu.field_text(menu.selected, label, sizeof(label), value, sizeof(value));
        assert(std::strcmp(label, "BACK") == 0);
        assert(!menu.handle(UiEvent::Select, status, &command));
        assert(menu.screen == MenuModel::Screen::Main);
        assert(menu.selected == i && !menu.editing);
    }
    menu.selected = 0;
    assert(!menu.handle(UiEvent::Previous, status, &command));
    assert(menu.selected == 5);
    menu.handle(UiEvent::Next, status, &command); assert(menu.selected == 0);
    menu.handle(UiEvent::Next, status, &command); // mode menu
    menu.handle(UiEvent::Select, status, &command);
    assert(menu.screen == MenuModel::Screen::Mode);
    menu.handle(UiEvent::Next, status, &command); // current field
    menu.handle(UiEvent::Select, status, &command); assert(menu.editing);
    menu.handle(UiEvent::Next, status, &command); assert(menu.draft.current_setpoint_mA == 1010);
    assert(!menu.handle(UiEvent::Refresh, status, &command));
    assert(menu.editing && menu.draft.current_setpoint_mA == 1010);
    assert(!menu.handle(UiEvent::Select, status, &command));
    assert(menu.choosing_edit_action && menu.edit_action == 0);
    assert(menu.handle(UiEvent::Select, status, &command));
    assert(command.type == UiCommandType::ApplySettings);
    // A rapid second edit must wait for the application's updated snapshot.
    assert(!menu.handle(UiEvent::Select, status, &command));
    assert(!menu.editing);
    status.settings = command.settings;
    ++status.settings_revision;
    menu.handle(UiEvent::Select, status, &command);
    menu.handle(UiEvent::Next, status, &command);
    menu.handle(UiEvent::Select, status, &command); // Choose Save or Back.
    menu.handle(UiEvent::Next, status, &command); // Back cancels the edit.
    assert(!menu.handle(UiEvent::Select, status, &command));
    assert(!menu.editing && menu.draft.current_setpoint_mA == 1010);
    status.state = RunState::Running;
    assert(menu.handle(UiEvent::Select, status, &command));
    assert(command.type == UiCommandType::Stop && !menu.editing);
    menu.screen = MenuModel::Screen::Live;
    menu.selected = 0;
    assert(menu.handle(UiEvent::Select, status, &command));
    assert(command.type == UiCommandType::Stop);
    status.state = RunState::Fault;
    assert(menu.handle(UiEvent::Select, status, &command));
    assert(command.type == UiCommandType::Acknowledge);
    status.state = RunState::Preparing;
    assert(menu.handle(UiEvent::Select, status, &command));
    assert(command.type == UiCommandType::Stop);
    menu.screen = MenuModel::Screen::Mode;
    assert(menu.handle(UiEvent::Select, status, &command));
    assert(command.type == UiCommandType::Stop);
    assert(!menu.editing);

    for (const auto state : {RunState::Idle, RunState::Complete, RunState::Fault,
                             RunState::Preparing, RunState::Running}) {
        for (int page = 1; page <= static_cast<int>(MenuModel::Screen::Logs); ++page) {
            MenuModel back_menu;
            back_menu.screen = static_cast<MenuModel::Screen>(page);
            back_menu.selected = back_menu.field_count() - 1;
            status.state = state;
            const bool command_sent = back_menu.handle(UiEvent::Select, status, &command);
            assert(command_sent == run_is_active(state));
            if (run_is_active(state)) {
                assert(command.type == UiCommandType::Stop);
                assert(back_menu.screen == static_cast<MenuModel::Screen>(page));
            } else {
                assert(back_menu.screen == MenuModel::Screen::Main);
                assert(back_menu.selected == page - 1);
            }
            assert(status.state == state); // Back never acknowledges/restarts a fault.
        }
    }

    // Invalid limits must still allow cancellation using only rotate/press.
    MenuModel invalid_edit;
    invalid_edit.screen = MenuModel::Screen::AutoStop;
    status.state = RunState::Idle;
    invalid_edit.handle(UiEvent::Select, status, &command);
    invalid_edit.draft.low_voltage_cutoff_enabled = true;
    invalid_edit.draft.high_voltage_cutoff_enabled = true;
    invalid_edit.draft.low_voltage_cutoff_mV = 20000;
    invalid_edit.draft.high_voltage_cutoff_mV = 10000;
    invalid_edit.handle(UiEvent::Select, status, &command);
    assert(!invalid_edit.handle(UiEvent::Select, status, &command));
    assert(invalid_edit.editing && invalid_edit.choosing_edit_action);
    invalid_edit.handle(UiEvent::Next, status, &command);
    assert(!invalid_edit.handle(UiEvent::Select, status, &command));
    assert(!invalid_edit.editing && !invalid_edit.choosing_edit_action);
    assert(invalid_edit.draft.current_setpoint_mA == status.settings.current_setpoint_mA);
    assert(invalid_edit.draft.low_voltage_cutoff_enabled == status.settings.low_voltage_cutoff_enabled);
    for (int page = 0; page <= static_cast<int>(MenuModel::Screen::Logs); ++page) {
        menu.screen = static_cast<MenuModel::Screen>(page);
        for (int field = 0; field < menu.field_count(); ++field) {
            char label[40], value[32];
            menu.field_text(field, label, sizeof(label), value, sizeof(value));
            assert(std::strlen(label) < sizeof(label));
            assert(std::strlen(value) < sizeof(value));
        }
    }
    RotaryDecoder decoder; decoder.reset(3);
    int steps = 0;
    for (uint8_t state : {2,0,1,3}) steps += decoder.update(state, 4);
    assert(std::abs(steps) == 1);
    decoder.reset(3); steps = 0;
    for (uint8_t state : {2,3,2,3}) steps += decoder.update(state, 4);
    assert(steps == 0); // bounce, no full detent
    decoder.reset(3); assert(decoder.update(0, 4) == 0); // illegal transition
    std::puts("PASS selectable Back on all pages, Save/Cancel, invalid-edit exit, Stop priority and encoder decoding");
}
}
int main()
{
    sensor_tests(); protection_tests(); protection_regression_tests(); controller_tests();
    simulation_tests(); menu_tests();
    std::puts("All host tests passed. This does not validate physical hardware.");
}
