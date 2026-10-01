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
    if (cfg::kSimulationMode) {
        m->simulated = true;
        m->current_mA = static_cast<uint32_t>(output_level_) * 30000U / 10000U;
        m->voltage_mV = std::max<int32_t>(12000 - m->current_mA / 10, 0);
        m->heatsink_temperature_mC = 25000 + m->current_mA / 20;
        m->probe_temperature_mC.fill(25000);
        m->probe_connected.fill(true);
        m->voltage_valid = m->current_valid = m->heatsink_valid = m->valid = true;
    } else {
        const int first = cfg::kTestMode == cfg::TestMode::VoltageMonitor ? cfg::kVoltageChannel : 0;
        const int last = cfg::kTestMode == cfg::TestMode::VoltageMonitor ? cfg::kVoltageChannel : 7;
        for (int ch = first; ch <= last; ++ch) {
            m->adc_valid[ch] = adc_.read(ch, &m->adc_raw[ch], &m->adc_mV[ch]) == ESP_OK;
        }
        m->voltage_valid = m->adc_valid[cfg::kVoltageChannel];
        if (m->voltage_valid) {
            m->voltage_mV = scale_voltage_divider_mV(
                m->adc_mV[cfg::kVoltageChannel],
                cfg::kVoltageDividerTopOhm, cfg::kVoltageDividerBottomOhm);
        }
        m->current_valid = cfg::kCurrentSensorInterfaceVerified &&
                           m->adc_valid[cfg::kCurrentChannel];
        if (m->current_valid) {
            const int32_t sensor_mV = scale_voltage_divider_mV(
                m->adc_mV[cfg::kCurrentChannel],
                cfg::kCurrentDividerTopOhm, cfg::kCurrentDividerBottomOhm);
            m->current_mA = current_from_sensor_mA(sensor_mV, cfg::kCurrentZeroMv,
                cfg::kCurrentSensitivityUvPerAmp, cfg::kCurrentPositiveSlope);
        }
        if (cfg::kTemperatureInterfaceVerified) {
            const auto temperature = [&](uint8_t ch, int32_t *value) {
                return ch < 8 && m->adc_valid[ch] && pt1000_temperature_mC(
                    m->adc_mV[ch], cfg::kTemperatureSupplyMv,
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
    m->timestamp_us = static_cast<uint64_t>(esp_timer_get_time());
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
