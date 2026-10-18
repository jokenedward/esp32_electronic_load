#pragma once

#include <cstdio>
#include <atomic>

#include "esp_err.h"
#include "system_types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

class DataLogger {
public:
    esp_err_t init();
    esp_err_t start_new_log();
    esp_err_t append(const SystemStatus &status);
    void stop();
    esp_err_t export_to_serial();
    bool recording() const { return state_.load() == State::Recording; }
    bool idle() const { return state_.load() == State::Idle; }
    bool failed() const { return failed_.load(); }

private:
    enum class State { Idle, Starting, Recording, Stopping, Exporting };
    enum class Operation { Start, Append, Stop, Export };
    struct Request { Operation operation; SystemStatus status{}; };
    static void worker(void *context);
    esp_err_t open_file();
    esp_err_t write_row(const SystemStatus &status);
    esp_err_t close_file();
    esp_err_t print_file();
    QueueHandle_t requests_{nullptr};
    std::atomic<State> state_{State::Idle};
    std::atomic_bool failed_{false}, force_close_{false};
    static constexpr const char *kLogPath = "/spiffs/discharge_log.csv";
    static constexpr long kMaximumLogBytes = 512 * 1024;

    FILE *file_{nullptr}; // Accessed only by the logging task.
};

