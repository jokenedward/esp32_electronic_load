#include "tft_ui.hpp"
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "project_config.hpp"

namespace cfg = project_config;
namespace {
constexpr const char *kTag = "ui";
constexpr uint8_t kBackground = rgb332(5,14,24), kHeader = rgb332(10,35,54);
constexpr uint8_t kCyan = rgb332(25,210,210), kWhite = rgb332(240,245,248);
constexpr uint8_t kGrey = rgb332(130,150,160), kRed = rgb332(230,45,50);
constexpr TickType_t kInputTick = pdMS_TO_TICKS(2) > 0 ? pdMS_TO_TICKS(2) : 1;
void format_value(char *text, unsigned size, bool valid, int32_t value, const char *unit)
{
    if (valid) std::snprintf(text, size, "%.2f %s", value/1000.0, unit);
    else std::snprintf(text, size, "-- %s", unit);
}
}

esp_err_t TftUi::init()
{
    commands_ = xQueueCreate(8, sizeof(UiCommand));
    events_ = xQueueCreate(32, sizeof(UiEvent));
    snapshots_ = xQueueCreate(1, sizeof(SystemStatus));
    if (!commands_ || !events_ || !snapshots_) return ESP_ERR_NO_MEM;
    const int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    if (cfg::kUseTft) {
        const esp_err_t result = display_.init();
        display_ready_ = result == ESP_OK;
        if (!display_ready_) ESP_LOGE(kTag, "TFT disabled: %s; serial UI remains available", esp_err_to_name(result));
    }
    if (cfg::kUseEncoder && cfg::kBoardReworkVerified && cfg::kEncoder3v3Verified) {
        gpio_config_t pins{};
        pins.pin_bit_mask = (1ULL<<cfg::kEncoderA) | (1ULL<<cfg::kEncoderB) |
                            (1ULL<<cfg::kEncoderPush);
        pins.mode = GPIO_MODE_INPUT;
        pins.pull_up_en = GPIO_PULLUP_ENABLE;
        encoder_ready_ = gpio_config(&pins) == ESP_OK;
    } else if (cfg::kUseEncoder) {
        ESP_LOGE(kTag, "Encoder locked: first correct/verify the 5 V pull-ups");
    }
    if (xTaskCreate(input_task, "ui_input", 3072, this, 4, nullptr) != pdPASS)
        return ESP_ERR_NO_MEM;
    if (xTaskCreate(screen_task, "ui_screen", 6144, this, 2, nullptr) != pdPASS)
        return ESP_ERR_NO_MEM;
    ESP_LOGI(kTag, "n/p=rotate, e=select, b=back; s=simulate/record, x=stop, a=ack, d=CSV");
    return ESP_OK;
}

void TftUi::render(const SystemStatus &status) { xQueueOverwrite(snapshots_, &status); }
bool TftUi::submit_command(const UiCommand &command)
{
    if (command.type == UiCommandType::Stop) { stop_requested_.store(true); return true; }
    return commands_ && xQueueSend(commands_, &command, 0) == pdTRUE;
}
bool TftUi::poll_command(UiCommand *command)
{
    if (!command) return false;
    if (stop_requested_.exchange(false)) {
        // An old queued Start must not immediately undo a later Stop.
        xQueueReset(commands_);
        command->type = UiCommandType::Stop;
        return true;
    }
    return commands_ && xQueueReceive(commands_, command, 0) == pdTRUE;
}

void TftUi::input_task(void *context)
{
    auto &ui = *static_cast<TftUi *>(context);
    RotaryDecoder decoder;
    const auto encoder_state = [] {
        return static_cast<uint8_t>((gpio_get_level(cfg::kEncoderA)<<1) |
                                     gpio_get_level(cfg::kEncoderB));
    };
    bool raw = false, stable = false;
    int64_t changed = esp_timer_get_time(), pressed = changed;
    if (ui.encoder_ready_) {
        decoder.reset(encoder_state());
        raw = stable = gpio_get_level(cfg::kEncoderPush) == 0;
    }
    const auto send_event = [&](UiEvent event) {
        if (event != UiEvent::None && xQueueSend(ui.events_, &event, 0) != pdTRUE)
            ESP_LOGW(kTag, "Menu busy: input queue full");
    };
    while (true) {
        if (ui.encoder_ready_) {
            int movement = decoder.update(encoder_state(), cfg::kEncoderTransitionsPerStep);
            if (cfg::kEncoderReverse) movement = -movement;
            if (movement) send_event(movement > 0 ? UiEvent::Next : UiEvent::Previous);
            const int64_t now = esp_timer_get_time();
            const bool reading = gpio_get_level(cfg::kEncoderPush) == 0;
            if (reading != raw) { raw = reading; changed = now; }
            if (raw != stable && now-changed >= 30000) {
                stable = raw;
                if (stable) pressed = now;
                else send_event(now-pressed >= 700000 ? UiEvent::Back : UiEvent::Select);
            }
        }
        char input;
        // Bound work per poll so a serial flood does not starve encoder polling.
        for (int count = 0; count < 16 && read(STDIN_FILENO, &input, 1) == 1; ++count) {
            UiCommand command{};
            bool action = true;
            switch (input) {
            case 's': command.type = UiCommandType::Start; break;
            case 'x': command.type = UiCommandType::Stop; break;
            case 'a': command.type = UiCommandType::Acknowledge; break;
            case 'd': command.type = UiCommandType::DownloadLog; break;
            default: action = false; break;
            }
            if (action) {
                if (!ui.submit_command(command)) ESP_LOGW(kTag, "Command queue full");
            } else {
                switch (input) {
                case 'n': send_event(UiEvent::Next); break;
                case 'p': send_event(UiEvent::Previous); break;
                case 'e': send_event(UiEvent::Select); break;
                case 'b': send_event(UiEvent::Back); break;
                default: break;
                }
            }
        }
        vTaskDelay(kInputTick);
    }
}

void TftUi::screen_task(void *context)
{
    auto &ui = *static_cast<TftUi *>(context);
    SystemStatus status{};
    int64_t next_draw = 0;
    RunState previous_state = RunState::Starting;
    while (true) {
        xQueueReceive(ui.snapshots_, &status, 0);
        bool changed = false;
        if (status.state == RunState::Fault && previous_state != RunState::Fault) {
            ui.menu_.screen = MenuModel::Screen::Live;
            ui.menu_.editing = false;
            changed = true;
        }
        previous_state = status.state;
        UiEvent event;
        while (xQueueReceive(ui.events_, &event, 0) == pdTRUE) {
            // Latest snapshot avoids basing an edit on settings from a slow repaint.
            xQueueReceive(ui.snapshots_, &status, 0);
            UiCommand command;
            if (ui.menu_.handle(event, status, &command) && !ui.submit_command(command)) {
                ui.menu_.cancel_pending_save();
                ui.menu_.message = "COMMAND QUEUE FULL";
            }
            changed = true;
        }
        const int64_t now = esp_timer_get_time();
        if (changed || now >= next_draw) {
            if (ui.display_ready_) {
                ui.draw(status);
                const esp_err_t result = ui.display_.present();
                if (result != ESP_OK) {
                    ESP_LOGE(kTag, "TFT update failed: %s", esp_err_to_name(result));
                    ui.display_ready_ = false;
                }
            } else if (changed) {
                char label[40], value[32];
                ui.menu_.field_text(ui.menu_.selected, label, sizeof(label), value, sizeof(value));
                ESP_LOGI(kTag, "%s > %s %s %s | %s", ui.menu_.title(), label, value,
                    ui.menu_.editing ? "[EDIT]" : "", ui.menu_.message);
            }
            next_draw = esp_timer_get_time() + cfg::kDisplayPeriodMs*1000ULL;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void TftUi::field(int y, const char *label, const char *value, bool selected)
{
    if (selected) display_.fill_rect(10, y-5, 380, 23, kCyan);
    const uint8_t color = selected ? kBackground : kWhite;
    display_.draw_text(18, y, label, color);
    display_.draw_text(245, y, value, color);
}

void TftUi::draw(const SystemStatus &status)
{
    display_.clear(kBackground);
    display_.fill_rect(0, 0, 400, 32, status.state == RunState::Fault ? kRed : kHeader);
    display_.draw_text(10, 9, menu_.title(), kWhite, 2);
    display_.draw_text(12, 37, cfg::kSimulationMode ?
        "SIMULATED DATA - LOAD CONTROL LOCKED" :
        "MEASUREMENT ONLY - LOAD CONTROL LOCKED", kCyan);
    const Measurements &m = status.measurements;
    char value[40], label[40];
    if (menu_.screen == MenuModel::Screen::Live) {
        format_value(value, sizeof(value), m.voltage_valid, m.voltage_mV, "V");
        field(57, "VOLTAGE", value);
        format_value(value, sizeof(value), m.current_valid, m.current_mA, "A");
        field(79, "CURRENT", value);
        format_value(value, sizeof(value), m.voltage_valid && m.current_valid,
                     calculate_power_mW(m), "W");
        field(101, "POWER", value);
        format_value(value, sizeof(value), m.heatsink_valid, m.heatsink_temperature_mC, "C");
        field(123, "HOTTEST HEATSINK", value);
        std::snprintf(value, sizeof(value), "%lu S", static_cast<unsigned long>(status.elapsed_s));
        field(145, run_state_name(status.state), value);
        display_.draw_text(14, 167, stop_reason_name(status.stop_reason), kWhite);
        display_.draw_text(14, 187, status.state == RunState::Fault ? "PRESS TO ACKNOWLEDGE" :
            status.state == RunState::Running ? "PRESS TO STOP" :
            cfg::kSimulationMode ? "PRESS TO START SIMULATION" : "PRESS TO RECORD MEASUREMENTS", kCyan);
    } else if (menu_.screen == MenuModel::Screen::Adc) {
        for (int ch = 0; ch < 8; ++ch) {
            std::snprintf(label, sizeof(label), "CH%d", ch);
            if (m.adc_valid[ch])
                std::snprintf(value, sizeof(value), "%u / %lu MV", m.adc_raw[ch],
                              static_cast<unsigned long>(m.adc_mV[ch]));
            else std::snprintf(value, sizeof(value), "-- / NOT VALID");
            field(55+ch*18, label, value);
        }
    } else {
        const int first = menu_.selected >= 6 ? menu_.selected-5 : 0;
        for (int line = 0; line < 6 && first+line < menu_.field_count(); ++line) {
            const int index = first+line;
            menu_.field_text(index, label, sizeof(label), value, sizeof(value));
            field(57+line*24, label, value, index == menu_.selected);
        }
        // Probe readings shown with their selected stop/limit, never fake zeros.
        if (menu_.screen == MenuModel::Screen::Probes && !menu_.message[0]) {
            const unsigned probe = menu_.selected/2;
            format_value(value, sizeof(value), m.probe_connected[probe],
                         m.probe_temperature_mC[probe], "C");
            std::snprintf(label, sizeof(label), "PROBE %u NOW: %.16s", probe+1, value);
            display_.draw_text(14, 203, label, kCyan);
        }
    }
    if (menu_.message[0]) display_.draw_text(14, 203, menu_.message, kWhite);
    display_.fill_rect(0, 218, 400, 22, kHeader);
    display_.draw_text(10, 226, menu_.editing ?
        "TURN CHANGE / PRESS SAVE / HOLD CANCEL" :
        "TURN MOVE / PRESS SELECT / HOLD BACK", kGrey);
}
