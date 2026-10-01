#pragma once

#include <cstdio>

#include "esp_err.h"
#include "system_types.hpp"

class DataLogger {
public:
    esp_err_t init();
    esp_err_t start_new_log();
    esp_err_t append(const SystemStatus &status);
    void stop();
    esp_err_t export_to_serial();

private:
    static constexpr const char *kLogPath = "/spiffs/discharge_log.csv";
    static constexpr long kMaximumLogBytes = 512 * 1024;

    FILE *file_{nullptr};
};

