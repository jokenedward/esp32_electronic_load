#pragma once
#include <cstdint>
#include <string_view>
#include "esp_err.h"

constexpr uint8_t rgb332(uint8_t r, uint8_t g, uint8_t b)
{
    return static_cast<uint8_t>((r & 0xE0) | ((g >> 3) & 0x1C) | (b >> 6));
}
class Display {
public:
    static constexpr int kWidth = 400, kHeight = 240;
    esp_err_t init();
    esp_err_t present();
    void clear(uint8_t color);
    void fill_rect(int x, int y, int width, int height, uint8_t color);
    void draw_text(int x, int y, std::string_view text, uint8_t color, int scale = 1);
private:
    void command(uint8_t cmd, const uint8_t *data = nullptr, unsigned count = 0);
    void window(int x0, int y0, int x1, int y1);
    uint8_t *pixels_{nullptr};
    uint8_t *previous_{nullptr};
    bool first_frame_{true};
};
