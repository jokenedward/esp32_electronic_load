#pragma once
#include <cstdint>

// Pure calculations: tested on the computer without an ESP32.
constexpr uint8_t ads7828_command(uint8_t channel)
{
    // Table II: CH0,2,4,6,1,3,5,7, not sequential binary order.
    return static_cast<uint8_t>(0x8C | ((((channel & 1) << 2) | (channel >> 1)) << 4));
}
constexpr uint16_t mcp4922_command(bool channel_b, uint16_t code, bool enabled)
{
    // Unbuffered reference, gain=1; SHDN=0 disables the selected channel.
    return static_cast<uint16_t>((channel_b ? 0x8000 : 0) | 0x2000 |
                                (enabled ? 0x1000 : 0) | (code & 0x0FFF));
}
constexpr uint8_t reverse_bits(uint8_t value)
{
    uint8_t result = 0;
    for (int bit = 0; bit < 8; ++bit) {
        result = static_cast<uint8_t>((result << 1) | (value & 1));
        value >>= 1;
    }
    return result;
}
uint32_t scale_voltage_divider_mV(uint32_t adc_mV, uint32_t top, uint32_t bottom);
int32_t current_from_sensor_mA(int32_t output_mV, int32_t zero_mV,
                              uint32_t sensitivity_uV_per_amp, bool positive_slope);
bool pt1000_temperature_mC(uint32_t adc_mV, uint32_t supply_mV,
                          uint32_t pullup_ohm, int32_t *temperature_mC);
