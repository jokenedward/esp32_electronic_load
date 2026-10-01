#pragma once
#include <cstdint>
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class Ads7828 {
public:
    esp_err_t init();
    esp_err_t read(uint8_t channel, uint16_t *raw, uint32_t *millivolts);
private:
    esp_err_t convert(uint8_t channel, uint16_t *raw);
    i2c_master_bus_handle_t bus_{nullptr};
    i2c_master_dev_handle_t device_{nullptr};
};

// The 595 and DAC share a clock, so a whole transfer must hold this lock.
// LCD writes keep DAC CS high; DAC writes keep LCD CS high and do not latch U9.
namespace board_bus {
esp_err_t init();
bool lock();
void unlock();
void shift_byte(gpio_num_t data_pin, uint8_t value);
void lcd_byte(uint8_t value);
}

// Bench-only driver. NOT connected to Start/Stop or the simulated PI controller.
class Mcp4922 {
public:
    esp_err_t init();
    esp_err_t write_mV(bool channel_b, uint32_t millivolts);
    esp_err_t shutdown();
private:
    esp_err_t write_word(uint16_t word);
    bool initialized_{false};
};
