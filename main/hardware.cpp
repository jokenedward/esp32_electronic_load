#include "hardware.hpp"
#include <algorithm>
#include "project_config.hpp"
#include "sensor_math.hpp"
#include "esp_log.h"
#include "esp_timer.h"

namespace cfg = project_config;
namespace { constexpr const char *kTag = "hardware"; }

esp_err_t Hardware::init()
{
    if (cfg::kSimulationMode) {
        ESP_LOGW(kTag, "SIMULATED measurements/output; no ADC acquisition or DAC reference commands");
        initialized_ = true;
        return ESP_OK;
    }
    if (!cfg::kBoardReworkVerified || !cfg::kAdcInterfaceVerified) {
        ESP_LOGE(kTag, "ADC locked: PCB, divider, current input and COM need verification");
        return ESP_ERR_INVALID_STATE;
    }
    const esp_err_t result = adc_.init();
    initialized_ = result == ESP_OK;
    ESP_LOGW(kTag, "Measurement-only mode. Start cannot enable any physical load.");
    return result;
}

esp_err_t Hardware::read_measurements(Measurements *m)
{
    if (!m) return ESP_ERR_INVALID_ARG;
    *m = Measurements{};
    if (!initialized_) return ESP_ERR_INVALID_STATE;
    // Timestamp the oldest sample, not the end of a possibly slow ADC scan.
    m->timestamp_us = static_cast<uint64_t>(esp_timer_get_time());
    if (cfg::kSimulationMode) {
        *m = simulation_measurements(output_level_, m->timestamp_us, simulation_fault_);
    } else {
        // ADC channel values are temporary. The rest of the program only needs
        // voltage, current, temperatures and their validity flags.
        std::array<uint32_t, 8> adc_mV{};
        std::array<bool, 8> adc_valid{};
        const int first = cfg::kTestMode == cfg::TestMode::VoltageMonitor ? cfg::kVoltageChannel : 0;
        const int last = cfg::kTestMode == cfg::TestMode::VoltageMonitor ? cfg::kVoltageChannel : 7;
        for (int ch = first; ch <= last; ++ch) {
            const esp_err_t result = adc_.read(ch, &adc_mV[ch]);
            adc_valid[ch] = result == ESP_OK;
            // Abort a failed bus scan instead of timing out on every channel.
            // Saturated channels remain individually invalid.
            if (result != ESP_OK && result != ESP_ERR_INVALID_RESPONSE) return result;
        }
        m->voltage_valid = adc_valid[cfg::kVoltageChannel];
        if (m->voltage_valid) {
            m->voltage_mV = scale_voltage_divider_mV(
                adc_mV[cfg::kVoltageChannel],
                cfg::kVoltageDividerTopOhm, cfg::kVoltageDividerBottomOhm);
        }
        m->current_valid = cfg::kCurrentSensorInterfaceVerified &&
                           adc_valid[cfg::kCurrentChannel];
        if (m->current_valid) {
            const int32_t sensor_mV = scale_voltage_divider_mV(
                adc_mV[cfg::kCurrentChannel],
                cfg::kCurrentDividerTopOhm, cfg::kCurrentDividerBottomOhm);
            m->current_mA = current_from_sensor_mA(sensor_mV, cfg::kCurrentZeroMv,
                cfg::kCurrentSensitivityUvPerAmp, cfg::kCurrentPositiveSlope);
        }
        if (cfg::kTemperatureInterfaceVerified) {
            const auto temperature = [&](uint8_t ch, int32_t *value) {
                return ch < 8 && adc_valid[ch] && pt1000_temperature_mC(
                    adc_mV[ch], cfg::kTemperatureSupplyMv,
                    cfg::kTemperaturePullupOhm, value);
            };
            int32_t first_heatsink = 0, second_heatsink = 0;
            const bool first_ok = temperature(cfg::kHeatsinkChannels[0], &first_heatsink);
            const bool second_ok = temperature(cfg::kHeatsinkChannels[1], &second_heatsink);
            m->heatsink_valid = first_ok && second_ok;
            if (m->heatsink_valid)
                m->heatsink_temperature_mC = std::max(first_heatsink, second_heatsink);
            for (unsigned i = 0; i < kTemperatureProbeCount; ++i)
                m->probe_connected[i] = temperature(cfg::kProbeChannels[i], &m->probe_temperature_mC[i]);
        }
        m->valid = m->voltage_valid && m->current_valid && m->heatsink_valid;
    }
    // No extra low-pass filter on the protection path: don't conceal a real trip.
    return m->voltage_valid ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

esp_err_t Hardware::enable_load()
{
    if (!initialized_) return ESP_ERR_INVALID_STATE;
    if (!cfg::kSimulationMode) {
        ESP_LOGE(kTag, "Real load control locked: independent shutdown/analogue loops unverified");
        return ESP_ERR_NOT_SUPPORTED;
    }
    output_level_ = 0;
    load_enabled_ = true;
    return ESP_OK;
}
esp_err_t Hardware::set_load_output(uint16_t level)
{
    if (!cfg::kSimulationMode) return ESP_ERR_NOT_SUPPORTED;
    if (!initialized_ || !load_enabled_) return ESP_ERR_INVALID_STATE;
    output_level_ = std::min<uint16_t>(level, 10000);
    return ESP_OK;
}
void Hardware::disable_load()
{
    // No physical output is initialized in this commissioning firmware.
    // This does NOT replace an electrical kill circuit on the PCB.
    output_level_ = 0;
    load_enabled_ = false;
}

esp_err_t Hardware::set_simulation_fault(SimulationFault fault)
{
    if (!cfg::kSimulationMode) return ESP_ERR_NOT_SUPPORTED;
    if (fault > SimulationFault::MissingProbe) return ESP_ERR_INVALID_ARG;
    simulation_fault_ = fault;
    return ESP_OK;
}
