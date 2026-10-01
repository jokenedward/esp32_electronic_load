#pragma once
#include <atomic>
#include "display.hpp"
#include "menu_model.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

class TftUi {
public:
    esp_err_t init();
    void render(const SystemStatus &status); // publish snapshot; never draws in control task
    bool poll_command(UiCommand *command);
    bool submit_command(const UiCommand &command);
private:
    static void input_task(void *context);
    static void screen_task(void *context);
    void draw(const SystemStatus &status);
    void field(int y, const char *label, const char *value, bool selected = false);
    QueueHandle_t commands_{nullptr}, events_{nullptr}, snapshots_{nullptr};
    std::atomic_bool stop_requested_{false};
    Display display_;
    MenuModel menu_;
    bool display_ready_{false}, encoder_ready_{false};
};
