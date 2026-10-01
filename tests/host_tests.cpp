#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "sensor_math.hpp"
#include "menu_model.hpp"
#include "load_control.hpp"

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
    assert(check(m, s) == StopReason::LowCurrentCutoff);
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
        m.current_mA = output*30000U/10000U;
        output = controller.update(m, s, 0.01F);
        assert(output <= 10000);
    }
    assert(std::abs(m.current_mA-5000) < 20);
    controller.reset(); s.mode = LoadMode::ConstantVoltage; s.voltage_setpoint_mV = 11000;
    output = 0;
    for (int i = 0; i < 10000; ++i) {
        m.current_mA = output*30000U/10000U; m.voltage_mV = 12000-m.current_mA/10;
        output = controller.update(m, s, 0.01F);
    }
    assert(std::abs(m.voltage_mV-11000) < 10);
    m.valid = false; assert(controller.update(m, s, 0.01F) == 0);
    std::puts("PASS CC/CV simulated-plant convergence and invalid-data reset");
}
void menu_tests()
{
    MenuModel menu; SystemStatus status; UiCommand command;
    status.state = RunState::Idle; status.measurements = good_measurements();
    assert(!menu.handle(UiEvent::Previous, status, &command));
    assert(menu.selected == 6);
    menu.handle(UiEvent::Next, status, &command); assert(menu.selected == 0);
    menu.handle(UiEvent::Next, status, &command); // mode menu
    menu.handle(UiEvent::Select, status, &command);
    assert(menu.screen == MenuModel::Screen::Mode);
    menu.handle(UiEvent::Next, status, &command); // current field
    menu.handle(UiEvent::Select, status, &command); assert(menu.editing);
    menu.handle(UiEvent::Next, status, &command); assert(menu.draft.current_setpoint_mA == 1010);
    assert(menu.handle(UiEvent::Select, status, &command));
    assert(command.type == UiCommandType::ApplySettings);
    // A rapid second edit must wait for the application's updated snapshot.
    assert(!menu.handle(UiEvent::Select, status, &command));
    assert(!menu.editing);
    status.settings = command.settings;
    ++status.settings_revision;
    menu.handle(UiEvent::Select, status, &command);
    menu.handle(UiEvent::Next, status, &command);
    menu.handle(UiEvent::Back, status, &command);
    assert(!menu.editing && menu.draft.current_setpoint_mA == 1010);
    status.state = RunState::Running;
    assert(!menu.handle(UiEvent::Select, status, &command)); assert(!menu.editing);
    menu.screen = MenuModel::Screen::Live;
    assert(menu.handle(UiEvent::Select, status, &command));
    assert(command.type == UiCommandType::Stop);
    status.state = RunState::Fault;
    assert(menu.handle(UiEvent::Select, status, &command));
    assert(command.type == UiCommandType::Acknowledge);
    for (int page = 0; page < 8; ++page) {
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
    std::puts("PASS menu navigation, edit/save/cancel, run lock and encoder decoding");
}
}
int main()
{
    sensor_tests(); protection_tests(); controller_tests(); menu_tests();
    std::puts("All host tests passed. This does not validate physical hardware.");
}
