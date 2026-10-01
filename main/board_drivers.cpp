#include "board_drivers.hpp"
#include "project_config.hpp"
#include "sensor_math.hpp"
#include "esp_check.h"
#include "esp_cpu.h"
#include "esp_rom_sys.h"
#include "freertos/task.h"

namespace cfg = project_config;
namespace {
constexpr const char *kTag = "board_drivers";
SemaphoreHandle_t bus_mutex = nullptr;
uint32_t half_clock_cycles = 0;

void clock_delay()
{
    const uint32_t start = esp_cpu_get_cycle_count();
    while (static_cast<uint32_t>(esp_cpu_get_cycle_count() - start) < half_clock_cycles) {}
}
esp_err_t output(gpio_num_t pin, int initial)
{
    ESP_RETURN_ON_ERROR(gpio_set_level(pin, initial), kTag, "initial pin level");
    return gpio_set_direction(pin, GPIO_MODE_OUTPUT);
}
}

esp_err_t Ads7828::init()
{
    if (!cfg::kBoardReworkVerified || !cfg::kAdcInterfaceVerified)
        return ESP_ERR_INVALID_STATE;
    if (device_) return ESP_OK;
    i2c_master_bus_config_t bus_config{};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = cfg::kAdcSda;
    bus_config.scl_io_num = cfg::kAdcScl;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    // The PCB has external 2k pull-ups to 3.3 V.
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus_), kTag, "I2C bus");
    i2c_device_config_t device_config{};
    device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device_config.device_address = cfg::kAdcAddress;
    device_config.scl_speed_hz = 100000;
    device_config.scl_wait_us = 10000; // permit conversion clock stretching
    esp_err_t result = i2c_master_bus_add_device(bus_, &device_config, &device_);
    if (result == ESP_OK) {
        // PD1=1 turns on the internal reference after STOP; wait before sampling.
        const uint8_t command = ads7828_command(0);
        result = i2c_master_transmit(device_, &command, 1, 30);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (result != ESP_OK) {
        if (device_) i2c_master_bus_rm_device(device_);
        device_ = nullptr;
        i2c_del_master_bus(bus_);
        bus_ = nullptr;
    }
    return result;
}

esp_err_t Ads7828::convert(uint8_t channel, uint16_t *raw)
{
    if (!device_ || !raw || channel > 7) return ESP_ERR_INVALID_ARG;
    const uint8_t command = ads7828_command(channel);
    uint8_t bytes[2]{};
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(
        device_, &command, 1, bytes, 2, 30), kTag, "ADS7828 conversion");
    if (bytes[0] & 0xF0) return ESP_ERR_INVALID_RESPONSE;
    *raw = static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
    return ESP_OK;
}

esp_err_t Ads7828::read(uint8_t channel, uint16_t *raw, uint32_t *millivolts)
{
    if (!raw || !millivolts) return ESP_ERR_INVALID_ARG;
    *raw = 0;
    *millivolts = 0;
    uint16_t sample = 0;
    // Discard the first conversion after mux switching. This is NOT a substitute
    // for correctly designed source impedance and analogue filtering.
    ESP_RETURN_ON_ERROR(convert(channel, &sample), kTag, "settling conversion");
    uint32_t sum = 0;
    bool clipped = sample >= 4090;
    for (int i = 0; i < cfg::kAdcSamples; ++i) {
        ESP_RETURN_ON_ERROR(convert(channel, &sample), kTag, "sample");
        clipped |= sample >= 4090;
        sum += sample;
    }
    *raw = static_cast<uint16_t>((sum + cfg::kAdcSamples/2) / cfg::kAdcSamples);
    *millivolts = (static_cast<uint32_t>(*raw) * cfg::kAdcReferenceMv + 2048) / 4096;
    // Saturation is an error, not a believable voltage/current reading.
    return clipped ? ESP_ERR_INVALID_RESPONSE : ESP_OK;
}

esp_err_t board_bus::init()
{
    if (!cfg::kBoardReworkVerified || !cfg::kTftWiringVerified)
        return ESP_ERR_INVALID_STATE;
    if (bus_mutex) return ESP_OK;
    // Requires DAC CS to be physically moved off GPIO2 and pulled up safely.
    ESP_RETURN_ON_ERROR(output(cfg::kDacCs, 1), kTag, "DAC deselect");
    ESP_RETURN_ON_ERROR(output(cfg::kTftCs, 1), kTag, "LCD deselect");
    ESP_RETURN_ON_ERROR(output(cfg::kTftRd, 1), kTag, "LCD read disable");
    ESP_RETURN_ON_ERROR(output(cfg::kTftWr, 1), kTag, "LCD write idle");
    ESP_RETURN_ON_ERROR(output(cfg::kTftOe, 1), kTag, "595 disable");
    ESP_RETURN_ON_ERROR(output(cfg::kTftLatch, 0), kTag, "595 latch idle");
    ESP_RETURN_ON_ERROR(output(cfg::kTftRs, 0), kTag, "LCD command");
    ESP_RETURN_ON_ERROR(output(cfg::kTftReset, 1), kTag, "LCD reset idle");
    ESP_RETURN_ON_ERROR(output(cfg::kSharedClock, 0), kTag, "clock idle");
    ESP_RETURN_ON_ERROR(output(cfg::kTftData, 0), kTag, "595 data idle");
    ESP_RETURN_ON_ERROR(output(cfg::kDacData, 0), kTag, "DAC data idle");
    half_clock_cycles = esp_rom_get_cpu_ticks_per_us() / 4; // >=250 ns each half
    bus_mutex = xSemaphoreCreateMutex();
    return bus_mutex ? ESP_OK : ESP_ERR_NO_MEM;
}

bool board_bus::lock()
{
    return bus_mutex && xSemaphoreTake(bus_mutex, pdMS_TO_TICKS(1000)) == pdTRUE;
}
void board_bus::unlock() { xSemaphoreGive(bus_mutex); }

void board_bus::shift_byte(gpio_num_t data_pin, uint8_t value)
{
    // Deliberately small, readable software-SPI driver. No ESP32-S3 peripherals.
    for (int bit = 7; bit >= 0; --bit) {
        gpio_set_level(data_pin, (value >> bit) & 1);
        clock_delay();
        gpio_set_level(cfg::kSharedClock, 1);
        clock_delay();
        gpio_set_level(cfg::kSharedClock, 0);
    }
}

void board_bus::lcd_byte(uint8_t value)
{
    if (cfg::kTftReverseDataBits) value = reverse_bits(value);
    shift_byte(cfg::kTftData, value);
    gpio_set_level(cfg::kTftLatch, 1);
    clock_delay(); // output register propagation before WR
    gpio_set_level(cfg::kTftLatch, 0);
    gpio_set_level(cfg::kTftWr, 0);
    clock_delay();
    gpio_set_level(cfg::kTftWr, 1);
    clock_delay();
}

esp_err_t Mcp4922::init()
{
    if (!cfg::kDacBenchTestVerified) return ESP_ERR_INVALID_STATE;
    ESP_RETURN_ON_ERROR(board_bus::init(), kTag, "DAC bus");
    initialized_ = true;
    const esp_err_t result = shutdown();
    if (result != ESP_OK) initialized_ = false;
    return result;
}

esp_err_t Mcp4922::write_word(uint16_t word)
{
    if (!initialized_) return ESP_ERR_INVALID_STATE;
    if (!board_bus::lock()) return ESP_ERR_TIMEOUT;
    gpio_set_level(cfg::kTftCs, 1);
    gpio_set_level(cfg::kDacCs, 0);
    esp_rom_delay_us(1);
    board_bus::shift_byte(cfg::kDacData, static_cast<uint8_t>(word >> 8));
    board_bus::shift_byte(cfg::kDacData, static_cast<uint8_t>(word));
    esp_rom_delay_us(1);
    gpio_set_level(cfg::kDacCs, 1);
    board_bus::unlock();
    // MCP4922 has no readback: ESP_OK confirms transmission, not analogue output.
    return ESP_OK;
}

esp_err_t Mcp4922::write_mV(bool channel_b, uint32_t millivolts)
{
    if (millivolts >= cfg::kDacReferenceMv) return ESP_ERR_INVALID_ARG;
    const uint32_t code = static_cast<uint64_t>(millivolts) * 4096 / cfg::kDacReferenceMv;
    return write_word(mcp4922_command(channel_b, static_cast<uint16_t>(code), true));
}

esp_err_t Mcp4922::shutdown()
{
    const esp_err_t first = write_word(mcp4922_command(false, 0, false));
    const esp_err_t second = write_word(mcp4922_command(true, 0, false));
    return first != ESP_OK ? first : second;
}
