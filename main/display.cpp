#include "display.hpp"
#include <algorithm>
#include <cstring>
#include "board_drivers.hpp"
#include "font_5x7.hpp"
#include "project_config.hpp"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "freertos/task.h"

namespace cfg = project_config;
namespace { constexpr const char *kTag = "display"; }

void Display::command(uint8_t cmd, const uint8_t *data, unsigned count)
{
    gpio_set_level(cfg::kTftRs, 0); // SSD1963 D/C low = command
    board_bus::lcd_byte(cmd);
    gpio_set_level(cfg::kTftRs, 1);
    for (unsigned i = 0; i < count; ++i) board_bus::lcd_byte(data[i]);
}

esp_err_t Display::init()
{
    if (!cfg::kUseTft || !cfg::kBoardReworkVerified || !cfg::kTftWiringVerified)
        return ESP_ERR_INVALID_STATE;
    ESP_RETURN_ON_ERROR(board_bus::init(), kTag, "display bus");
    // Two 8-bit canvases use the same 192 KB as one old RGB565 demo canvas.
    pixels_ = static_cast<uint8_t *>(heap_caps_malloc(kWidth*kHeight, MALLOC_CAP_8BIT));
    previous_ = static_cast<uint8_t *>(heap_caps_malloc(kWidth*kHeight, MALLOC_CAP_8BIT));
    if (!pixels_ || !previous_) {
        heap_caps_free(pixels_); heap_caps_free(previous_);
        pixels_ = previous_ = nullptr;
        return ESP_ERR_NO_MEM;
    }
    gpio_set_level(cfg::kTftReset, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(cfg::kTftReset, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    if (!board_bus::lock()) {
        heap_caps_free(pixels_); heap_caps_free(previous_);
        pixels_ = previous_ = nullptr;
        return ESP_ERR_TIMEOUT;
    }
    gpio_set_level(cfg::kDacCs, 1);
    gpio_set_level(cfg::kTftCs, 0);
    gpio_set_level(cfg::kTftOe, 0);

    // Same 800x480 panel timing as the working TTGO prototype. Confirm module.
    const uint8_t pll[]{0x1D, 0x02, 0x04};
    command(0xE2, pll, sizeof(pll));
    uint8_t value = 1; command(0xE0, &value, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    value = 3; command(0xE0, &value, 1);
    vTaskDelay(pdMS_TO_TICKS(12));
    command(0x01);
    vTaskDelay(pdMS_TO_TICKS(10));
    const uint8_t pixel_clock[]{0x03,0xFF,0xFF};
    const uint8_t mode[]{0,0,0x03,0x1F,0x01,0xDF,0};
    const uint8_t horizontal[]{0x04,0x1F,0,0x2E,0,0,0,0};
    const uint8_t vertical[]{0x02,0x0C,0,0x17,0x15,0,0};
    command(0xE6, pixel_clock, sizeof(pixel_clock));
    command(0xB0, mode, sizeof(mode));
    command(0xB4, horizontal, sizeof(horizontal));
    command(0xB6, vertical, sizeof(vertical));
    value = 0; command(0xF0, &value, 1); // 8-bit host, three bytes per pixel
    command(0x36, &value, 1); // landscape address order
    command(0xD0, &value, 1); // DBC off
    const uint8_t pwm[]{0x05,0xFE,0x01,0,0,0};
    command(0xBE, pwm, sizeof(pwm));
    const uint8_t panel_gpio[]{0x07,0x01};
    command(0xB8, panel_gpio, sizeof(panel_gpio));
    value = 1; command(0xBA, &value, 1);
    command(0x29);
    gpio_set_level(cfg::kTftCs, 1);
    board_bus::unlock();
    clear(0);
    return ESP_OK;
}

void Display::window(int x0, int y0, int x1, int y1)
{
    const uint8_t x[]{static_cast<uint8_t>(x0>>8), static_cast<uint8_t>(x0),
                      static_cast<uint8_t>(x1>>8), static_cast<uint8_t>(x1)};
    const uint8_t y[]{static_cast<uint8_t>(y0>>8), static_cast<uint8_t>(y0),
                      static_cast<uint8_t>(y1>>8), static_cast<uint8_t>(y1)};
    command(0x2A, x, sizeof(x)); command(0x2B, y, sizeof(y));
}

esp_err_t Display::present()
{
    if (!pixels_ || !previous_) return ESP_ERR_INVALID_STATE;
    for (int y = 0; y < kHeight; ++y) {
        const int offset = y*kWidth;
        int first = 0, last = kWidth - 1;
        if (!first_frame_) {
            while (first < kWidth && pixels_[offset+first] == previous_[offset+first]) ++first;
            while (last >= first && pixels_[offset+last] == previous_[offset+last]) --last;
            if (first > last) continue;
        }
        if (!board_bus::lock()) return ESP_ERR_TIMEOUT;
        gpio_set_level(cfg::kDacCs, 1);
        gpio_set_level(cfg::kTftCs, 0);
        // 400x240 canvas -> FULL 800x480 panel, inclusive address bounds.
        window(first*2, y*2, last*2+1, y*2+1);
        command(0x2C);
        for (int repeat_y = 0; repeat_y < 2; ++repeat_y) {
            for (int x = first; x <= last; ++x) {
                const uint8_t color = pixels_[offset+x];
                const uint8_t red = ((color>>5) & 7)*255/7;
                const uint8_t green = ((color>>2) & 7)*255/7;
                const uint8_t blue = (color & 3)*255/3;
                for (int repeat_x = 0; repeat_x < 2; ++repeat_x) {
                    board_bus::lcd_byte(red);
                    board_bus::lcd_byte(green);
                    board_bus::lcd_byte(blue);
                }
            }
        }
        gpio_set_level(cfg::kTftCs, 1);
        board_bus::unlock();
        std::memcpy(previous_+offset+first, pixels_+offset+first, last-first+1);
        // Permit encoder/serial tasks and watchdog/idle to run during a full repaint.
        vTaskDelay(1);
    }
    first_frame_ = false;
    return ESP_OK;
}
void Display::clear(uint8_t color) { std::memset(pixels_, color, kWidth*kHeight); }
void Display::fill_rect(int x, int y, int width, int height, uint8_t color)
{
    if (!pixels_ || width <= 0 || height <= 0) return;
    const int x0 = std::clamp(x, 0, kWidth), x1 = std::clamp(x+width, 0, kWidth);
    const int y0 = std::clamp(y, 0, kHeight), y1 = std::clamp(y+height, 0, kHeight);
    for (int row = y0; row < y1; ++row)
        std::fill(pixels_+row*kWidth+x0, pixels_+row*kWidth+x1, color);
}
void Display::draw_text(int x, int y, std::string_view text, uint8_t color, int scale)
{
    if (scale < 1) return;
    for (char character : text) {
        if (x+5*scale > kWidth) break;
        const uint8_t *shape = font_glyph(character);
        for (int col = 0; col < 5; ++col)
            for (int row = 0; row < 7; ++row)
                if (shape[col] & (1U<<row))
                    fill_rect(x+col*scale, y+row*scale, scale, scale, color);
        x += 6*scale;
    }
}
