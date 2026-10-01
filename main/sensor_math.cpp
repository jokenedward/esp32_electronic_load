#include "sensor_math.hpp"
#include <cmath>

uint32_t scale_voltage_divider_mV(uint32_t adc_mV, uint32_t top, uint32_t bottom)
{
    if (bottom == 0) return 0;
    const uint64_t total = static_cast<uint64_t>(top) + bottom;
    if (adc_mV > static_cast<uint64_t>(UINT32_MAX) * bottom / total) return UINT32_MAX;
    return static_cast<uint32_t>(static_cast<uint64_t>(adc_mV) * total / bottom);
}

int32_t current_from_sensor_mA(int32_t output_mV, int32_t zero_mV,
                              uint32_t sensitivity, bool positive_slope)
{
    if (sensitivity == 0) return 0;
    int64_t difference = static_cast<int64_t>(output_mV) - zero_mV;
    if (!positive_slope) difference = -difference;
    // Preserve sign: reverse current must not silently become zero.
    return static_cast<int32_t>(difference * 1000000LL / sensitivity);
}

bool pt1000_temperature_mC(uint32_t adc_mV, uint32_t supply_mV,
                          uint32_t pullup_ohm, int32_t *temperature_mC)
{
    if (!temperature_mC || !pullup_ohm || adc_mV == 0 || adc_mV >= supply_mV)
        return false;
    const double resistance = static_cast<double>(pullup_ohm) * adc_mV /
                              (supply_mV - adc_mV);
    // IEC 60751 / TE platinum RTD equation, including temperatures below zero.
    const auto resistance_at = [](double t) {
        constexpr double a = 3.9083e-3, b = -5.775e-7, c = -4.183e-12;
        return 1000.0 * (1 + a*t + b*t*t + (t < 0 ? c*(t-100)*t*t*t : 0));
    };
    double low = -40, high = 150;
    if (resistance < resistance_at(low) || resistance > resistance_at(high))
        return false;
    for (int i = 0; i < 24; ++i) {
        const double middle = (low + high) / 2;
        if (resistance_at(middle) < resistance) low = middle;
        else high = middle;
    }
    *temperature_mC = static_cast<int32_t>(std::lround((low + high) * 500));
    return true;
}
