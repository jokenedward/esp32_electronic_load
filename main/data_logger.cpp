#include "data_logger.hpp"

#include <array>

#include "esp_log.h"
#include "esp_spiffs.h"

namespace {
constexpr char kTag[] = "data_logger";
}

esp_err_t DataLogger::init()
{
    esp_vfs_spiffs_conf_t config{};
    config.base_path = "/spiffs";
    config.partition_label = "storage";
    config.max_files = 3;
    config.format_if_mount_failed = true;

    const esp_err_t result = esp_vfs_spiffs_register(&config);
    if (result != ESP_OK) {
        ESP_LOGE(kTag, "Could not mount log storage: %s", esp_err_to_name(result));
        return result;
    }

    size_t total_bytes = 0;
    size_t used_bytes = 0;
    if (esp_spiffs_info("storage", &total_bytes, &used_bytes) == ESP_OK) {
        ESP_LOGI(
            kTag,
            "Log storage ready: %lu/%lu bytes used",
            static_cast<unsigned long>(used_bytes),
            static_cast<unsigned long>(total_bytes));
    }
    return ESP_OK;
}

esp_err_t DataLogger::start_new_log()
{
    stop();
    file_ = std::fopen(kLogPath, "w");
    if (file_ == nullptr) {
        return ESP_FAIL;
    }

    const int written = std::fprintf(
        file_,
        "time_s,state,reason,data_source,voltage_mV,current_mA,power_mW,heatsink_mC,"
        "probe1_mC,probe2_mC,probe3_mC,probe4_mC,output_level_0_to_10000\n");
    if (written < 0 || std::fflush(file_) != 0) {
        stop();
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t DataLogger::append(const SystemStatus &status)
{
    if (file_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    const long current_size = std::ftell(file_);
    if (current_size < 0 || current_size >= kMaximumLogBytes) {
        ESP_LOGE(kTag, "Log file reached its 512 KiB safety limit");
        return ESP_ERR_NO_MEM;
    }

    // Empty CSV fields mean unavailable, not a measured zero.
    const auto value_text = [](char *text, bool valid, int32_t value) {
        if (valid) std::snprintf(text, 24, "%ld", static_cast<long>(value));
        else text[0] = '\0';
    };
    char voltage[24], current[24], power[24], heatsink[24], probes[4][24];
    const auto &m = status.measurements;
    value_text(voltage, m.voltage_valid, m.voltage_mV);
    value_text(current, m.current_valid, m.current_mA);
    value_text(power, m.voltage_valid && m.current_valid, calculate_power_mW(m));
    value_text(heatsink, m.heatsink_valid, m.heatsink_temperature_mC);
    for (unsigned i = 0; i < 4; ++i)
        value_text(probes[i], m.probe_connected[i], m.probe_temperature_mC[i]);
    const int written = std::fprintf(
        file_,
        "%lu,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%u\n",
        static_cast<unsigned long>(status.elapsed_s),
        run_state_name(status.state),
        stop_reason_name(status.stop_reason),
        m.simulated ? "SIMULATED" : "MEASURED",
        voltage, current, power, heatsink, probes[0], probes[1], probes[2], probes[3],
        status.output_level);

    return written >= 0 && std::fflush(file_) == 0 ? ESP_OK : ESP_FAIL;
}

void DataLogger::stop()
{
    if (file_ != nullptr) {
        std::fflush(file_);
        std::fclose(file_);
        file_ = nullptr;
    }
}

esp_err_t DataLogger::export_to_serial()
{
    if (file_ != nullptr) {
        std::fflush(file_);
    }

    FILE *input = std::fopen(kLogPath, "r");
    if (input == nullptr) {
        ESP_LOGW(kTag, "No discharge log is available yet");
        return ESP_ERR_NOT_FOUND;
    }

    std::printf("\n----- DISCHARGE_LOG_CSV_BEGIN -----\n");
    std::array<char, 192> line{};
    while (std::fgets(line.data(), line.size(), input) != nullptr) {
        std::fputs(line.data(), stdout);
    }
    std::printf("----- DISCHARGE_LOG_CSV_END -----\n");
    std::fflush(stdout);
    std::fclose(input);
    return ESP_OK;
}
