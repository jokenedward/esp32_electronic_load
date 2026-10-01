#include <cstdint>
#include <cstdio>

#include "data_logger.hpp"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hardware.hpp"
#include "load_control.hpp"
#include "project_config.hpp"
#include "system_types.hpp"
#include "tft_ui.hpp"

namespace {
constexpr char kTag[] = "electronic_load";
constexpr TickType_t kControlPeriod = pdMS_TO_TICKS(project_config::kControlPeriodMs);

Hardware g_hardware;
DataLogger g_logger;
TftUi g_ui;
LoadController g_controller({
    project_config::kCcKp,
    project_config::kCcKi,
    project_config::kCvKp,
    project_config::kCvKi,
});
SystemStatus g_status;
uint64_t g_run_started_us = 0;

void stop_run(StopReason reason)
{
    // Stop the simulated demand first. Real modes never enable physical output.
    g_hardware.disable_load();
    g_controller.reset();
    g_status.output_level = 0;
    g_status.stop_reason = reason;
    g_status.state = is_normal_completion(reason) ? RunState::Complete : RunState::Fault;

    if (reason != StopReason::LogFailure) {
        g_logger.append(g_status); // Best effort final row.
    }
    g_logger.stop();

    ESP_LOGW(kTag, "Load stopped: %s", stop_reason_name(reason));
}

void start_run(uint64_t now_us)
{
    if ((g_status.state != RunState::Idle && g_status.state != RunState::Complete) ||
        !(project_config::kSimulationMode ? g_status.measurements.valid :
                                          g_status.measurements.voltage_valid) ||
        !settings_are_valid(g_status.settings)) {
        ESP_LOGW(kTag, "Start rejected: state, measurements or settings are not ready");
        return;
    }

    if (g_logger.start_new_log() != ESP_OK) {
        g_status.state = RunState::Fault;
        g_status.stop_reason = StopReason::LogFailure;
        return;
    }

    g_controller.reset();
    if (project_config::kSimulationMode && g_hardware.enable_load() != ESP_OK) {
        g_logger.stop();
        g_status.state = RunState::Fault;
        g_status.stop_reason = StopReason::OutputDriverFailure;
        return;
    }

    g_status.state = RunState::Running;
    g_status.stop_reason = StopReason::None;
    g_status.elapsed_s = 0;
    g_status.output_level = 0;
    g_run_started_us = now_us;
    ESP_LOGI(kTag, "%s started; firmware load control is locked",
             project_config::kSimulationMode ? "Simulated discharge" : "Measurement recording");
}

void process_ui_commands(uint64_t now_us)
{
    UiCommand command{};
    for (int count = 0; count < 8 && g_ui.poll_command(&command); ++count) {
        switch (command.type) {
        case UiCommandType::Start:
            start_run(now_us);
            break;
        case UiCommandType::Stop:
            ++g_status.settings_revision; // Stop cancels old queued edits as well.
            if (g_status.state == RunState::Running) {
                stop_run(StopReason::UserStopped);
            }
            break;
        case UiCommandType::Acknowledge:
            if (g_status.state == RunState::Fault &&
                (project_config::kSimulationMode ? g_status.measurements.valid :
                                                  g_status.measurements.voltage_valid)) {
                g_status.state = RunState::Idle;
                g_status.stop_reason = StopReason::None;
            }
            break;
        case UiCommandType::ApplySettings:
            if (g_status.state != RunState::Running && settings_are_valid(command.settings)) {
                g_status.settings = command.settings;
            } else {
                ESP_LOGW(kTag, "Settings rejected while running or because a value is invalid");
            }
            ++g_status.settings_revision;
            break;
        case UiCommandType::DownloadLog:
            if (g_status.state != RunState::Running) {
                g_logger.export_to_serial();
            }
            break;
        }
    }
}

void control_task(void *)
{
    TickType_t last_wake = xTaskGetTickCount();
    uint64_t last_serial_us = 0;
    uint64_t last_log_us = 0;

    while (true) {
        Measurements measurements{};
        if (g_hardware.read_measurements(&measurements) != ESP_OK) {
            measurements.valid = false;
        }
        g_status.measurements = measurements;

        const uint64_t now_us = static_cast<uint64_t>(esp_timer_get_time());
        process_ui_commands(now_us);

        if (g_status.state == RunState::Running) {
            g_status.elapsed_s =
                static_cast<uint32_t>((now_us - g_run_started_us) / 1000000ULL);

            // Real modes record only: NEVER feed the simulated PI output to a DAC.
            StopReason protection_result = StopReason::None;
            if (project_config::kSimulationMode) {
                protection_result = check_protection(
                    g_status.measurements, g_status.settings, g_status.elapsed_s, now_us);
            } else if (!g_status.measurements.voltage_valid) {
                protection_result = StopReason::InvalidMeasurement;
            } else if (g_status.settings.maximum_run_time_s > 0 &&
                       g_status.elapsed_s >= g_status.settings.maximum_run_time_s) {
                protection_result = StopReason::TimerExpired;
            }
            if (protection_result != StopReason::None) {
                stop_run(protection_result);
            } else if (project_config::kSimulationMode) {
                const float time_step_s =
                    static_cast<float>(project_config::kControlPeriodMs) / 1000.0F;
                g_status.output_level =
                    g_controller.update(g_status.measurements, g_status.settings, time_step_s);
                if (g_hardware.set_load_output(g_status.output_level) != ESP_OK) {
                    stop_run(StopReason::OutputDriverFailure);
                }
            }
        }

        g_ui.render(g_status); // snapshot only; TFT runs in its own task
        if (now_us - last_serial_us >= 1000000ULL) {
            char voltage[24] = "--", current[24] = "--";
            if (measurements.voltage_valid)
                std::snprintf(voltage, sizeof(voltage), "%.3f V", measurements.voltage_mV/1000.0);
            if (measurements.current_valid)
                std::snprintf(current, sizeof(current), "%.3f A", measurements.current_mA/1000.0);
            ESP_LOGI(kTag, "%s | %s | voltage=%s current=%s | LOAD CONTROL LOCKED",
                project_config::kSimulationMode ? "SIMULATED" : "MEASURED",
                run_state_name(g_status.state), voltage, current);
            last_serial_us = now_us;
        }

        if (g_status.state == RunState::Running &&
            now_us - last_log_us >= project_config::kLogPeriodMs * 1000ULL) {
            if (g_logger.append(g_status) != ESP_OK) {
                stop_run(StopReason::LogFailure);
            }
            last_log_us = now_us;
        }

        xTaskDelayUntil(&last_wake, kControlPeriod);
    }
}
} // namespace

extern "C" void app_main(void)
{
    g_status = SystemStatus{};

    // Firmware does not drive a physical load. A software flag cannot make an
    // unsafe PCB safe: verify a physical default-OFF circuit before load testing.
    const esp_err_t hardware_result = g_hardware.init();
    if (hardware_result != ESP_OK)
        ESP_LOGE(kTag, "Sensors unavailable/locked: %s", esp_err_to_name(hardware_result));
    ESP_ERROR_CHECK(g_logger.init());
    ESP_ERROR_CHECK(g_ui.init());

    if (!settings_are_valid(g_status.settings)) {
        ESP_LOGE(kTag, "Default settings are invalid");
        return;
    }

    g_status.state = RunState::Idle;
    const BaseType_t task_result =
        xTaskCreate(control_task, "electronic_load", 8192, nullptr, 6, nullptr);
    ESP_ERROR_CHECK(task_result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_LOGI(kTag, "Ready. s=start simulation/recording, x=stop, a=acknowledge, d=CSV");
}
