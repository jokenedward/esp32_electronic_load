#include <cstdint>

#include "data_logger.hpp"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hardware.hpp"
#include "load_control.hpp"
#include "protection.hpp"
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
uint64_t g_prepare_started_us = 0;
FaultRecovery g_recovery;
constexpr ProtectionScope kProtectionScope =
    project_config::kTestMode == project_config::TestMode::VoltageMonitor ?
    ProtectionScope::VoltageOnly : ProtectionScope::AllSensors;

void stop_run(StopReason reason)
{
    // Output action precedes logging/UI. Monitor modes have NO physical OFF path.
    g_hardware.disable_load();
    g_controller.reset();
    if (!record_stop(g_status, reason)) return;
    g_recovery.reset();
    g_ui.discard_commands();

    if (reason != StopReason::LogFailure && g_logger.recording()) {
        g_logger.append(g_status); // Best effort final row.
    }
    g_logger.stop();

    ESP_LOGW(kTag, "%s: %s", project_config::kSimulationMode ?
        "SIMULATED output stopped" : "Recording stopped; physical load NOT controlled",
        stop_reason_name(reason));
}

void start_run()
{
    const uint64_t now_us = static_cast<uint64_t>(esp_timer_get_time());
    if ((g_status.state != RunState::Idle && g_status.state != RunState::Complete) ||
        !g_logger.idle()) {
        ESP_LOGW(kTag, "Start rejected: acknowledge fault or wait for storage");
        return;
    }
    const StopReason ready = check_start_protection(
        g_status.measurements, g_status.settings, now_us, kProtectionScope);
    if (ready != StopReason::None) { stop_run(ready); return; }
    if (g_logger.start_new_log() != ESP_OK) {
        stop_run(StopReason::LogFailure);
        return;
    }
    g_status.state = RunState::Preparing;
    g_status.stop_reason = StopReason::None;
    g_status.output_level = 0;
    g_status.elapsed_s = 0;
    g_prepare_started_us = now_us;
}

void finish_preparing()
{
    const uint64_t now_us = static_cast<uint64_t>(esp_timer_get_time());
    const StopReason ready = check_start_protection(
        g_status.measurements, g_status.settings, now_us, kProtectionScope);
    if (ready != StopReason::None) { stop_run(ready); return; }
    if (g_logger.failed() || now_us - g_prepare_started_us > 3000000ULL) {
        stop_run(StopReason::LogFailure);
        return;
    }
    if (!g_logger.recording()) return;
    g_controller.reset();
    if (project_config::kSimulationMode && g_hardware.enable_load() != ESP_OK) {
        stop_run(StopReason::OutputDriverFailure);
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

void process_ui_commands()
{
    UiCommand command{};
    for (int count = 0; count < 8 && g_ui.poll_command(&command); ++count) {
        switch (command.type) {
        case UiCommandType::Start:
            start_run();
            break;
        case UiCommandType::Stop:
            ++g_status.settings_revision; // Stop cancels old queued edits as well.
            if (run_is_active(g_status.state)) {
                stop_run(StopReason::UserStopped);
            }
            return; // Do not process an old Start in this cycle.
        case UiCommandType::Acknowledge:
            if (g_status.state == RunState::Fault)
                g_status.fault_can_acknowledge = g_recovery.update(g_status.measurements,
                    g_status.settings, g_status.stop_reason,
                    static_cast<uint64_t>(esp_timer_get_time()), kProtectionScope) && g_logger.idle();
            if (acknowledge_fault(g_status)) {
                g_recovery.reset();
                g_ui.discard_commands();
            } else if (g_status.state == RunState::Fault) {
                ESP_LOGW(kTag, "Fault still latched: wait for safe readings and storage to finish");
            }
            return;
        case UiCommandType::ApplySettings:
            if (!run_is_active(g_status.state) && settings_are_valid(command.settings)) {
                g_status.settings = command.settings;
                g_recovery.reset();
                g_status.fault_can_acknowledge = false;
            } else {
                ESP_LOGW(kTag, "Settings rejected while running or because a value is invalid");
            }
            ++g_status.settings_revision;
            break;
        case UiCommandType::DownloadLog:
            if (!run_is_active(g_status.state)) {
                g_logger.export_to_serial();
            }
            break;
        case UiCommandType::SetSimulationFault:
            if (g_hardware.set_simulation_fault(command.simulation_fault) == ESP_OK)
                ESP_LOGI(kTag, "SIMULATED test input: %s; 0=restore normal readings",
                    simulation_fault_name(command.simulation_fault));
            break;
        }
    }
}

void control_task(void *)
{
    TickType_t last_wake = xTaskGetTickCount();
    uint64_t last_log_us = 0;
    uint64_t last_cycle_us = 0;

    while (true) {
        const uint64_t cycle_us = static_cast<uint64_t>(esp_timer_get_time());
        if (run_is_active(g_status.state) && last_cycle_us != 0 &&
            cycle_us - last_cycle_us > kMaximumMeasurementAgeUs)
            stop_run(StopReason::ControlLoopTimeout);
        last_cycle_us = cycle_us;
        Measurements measurements{};
        if (g_hardware.read_measurements(&measurements) != ESP_OK) {
            measurements.valid = false;
        }
        g_status.measurements = measurements;

        uint64_t now_us = static_cast<uint64_t>(esp_timer_get_time());
        if ((run_is_active(g_status.state) || g_status.state == RunState::Complete) && g_logger.failed())
            stop_run(StopReason::LogFailure);
        if (g_status.state == RunState::Fault) {
            g_status.fault_can_acknowledge = g_recovery.update(g_status.measurements,
                g_status.settings, g_status.stop_reason, now_us, kProtectionScope) && g_logger.idle();
        }
        process_ui_commands();
        if (g_status.state == RunState::Preparing) finish_preparing();
        now_us = static_cast<uint64_t>(esp_timer_get_time());

        if (g_status.state == RunState::Running) {
            g_status.elapsed_s =
                static_cast<uint32_t>((now_us - g_run_started_us) / 1000000ULL);

            // Real modes record only: NEVER feed the simulated PI output to a DAC.
            const StopReason protection_result = check_protection(g_status.measurements,
                g_status.settings, g_status.elapsed_s, now_us, kProtectionScope);
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
    const esp_err_t log_result = g_logger.init();
    if (log_result != ESP_OK)
        ESP_LOGE(kTag, "Storage unavailable: %s; Start will be blocked", esp_err_to_name(log_result));
    ESP_ERROR_CHECK(g_ui.init());
    if (project_config::kSimulationMode) {
        ESP_LOGI(kTag, "PCB simulation: ESP32 application running; CSV storage %s",
                 log_result == ESP_OK ? "READY" : "FAILED");
        ESP_LOGI(kTag, "ADC/current/temperature inputs SIMULATED; physical load control UNUSED");
        ESP_LOGI(kTag, "Test keys: 0=normal, 1=31V, 2=31A, 3=520W, 4=95C heatsink");
        ESP_LOGI(kTag, "5=sensor missing, 6=stale sample, 7=hot probe 1, 8=probe 1 missing");
    }

    if (!settings_are_valid(g_status.settings)) {
        ESP_LOGE(kTag, "Default settings are invalid");
        return;
    }

    g_status.state = RunState::Idle;
    const BaseType_t task_result =
        xTaskCreate(control_task, "electronic_load", 8192, nullptr, 6, nullptr);
    ESP_ERROR_CHECK(task_result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_LOGI(kTag, "Ready. s=start simulation/recording, x=stop, a=acknowledge, d=CSV, ?=status");
}
