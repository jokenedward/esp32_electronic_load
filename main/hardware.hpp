#pragma once
#include "board_drivers.hpp"
#include "system_types.hpp"

class Hardware {
public:
    esp_err_t init();
    esp_err_t read_measurements(Measurements *measurements);
    esp_err_t enable_load();
    esp_err_t set_load_output(uint16_t level);
    void disable_load();
private:
    Ads7828 adc_;
    bool initialized_{false};
    bool load_enabled_{false};
    uint16_t output_level_{0};
};
