#include "data_logger.hpp"

#include <array>

#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/task.h"

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
    requests_ = xQueueCreate(8, sizeof(Request));
    if (!requests_) return ESP_ERR_NO_MEM;
    if (xTaskCreate(worker, "csv_logger", 4096, this, 1, nullptr) != pdPASS) {
        vQueueDelete(requests_);
        requests_ = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t DataLogger::start_new_log()
{
    if (!requests_) return ESP_ERR_INVALID_STATE;
    State expected = State::Idle;
    if (!state_.compare_exchange_strong(expected, State::Starting)) return ESP_ERR_INVALID_STATE;
    failed_.store(false);
    const Request request{Operation::Start};
    if (xQueueSend(requests_, &request, 0) == pdTRUE) return ESP_OK;
    failed_.store(true);
    state_.store(State::Idle);
    return ESP_FAIL;
}

esp_err_t DataLogger::append(const SystemStatus &status)
{
    if (!requests_ || !recording() || failed()) return ESP_ERR_INVALID_STATE;
    // Reserve a queue slot for Stop. Never wait for flash from the control task.
    const Request request{Operation::Append, status};
    if (uxQueueSpacesAvailable(requests_) > 1 &&
        xQueueSend(requests_, &request, 0) == pdTRUE) return ESP_OK;
    failed_.store(true);
    return ESP_ERR_NO_MEM;
}

void DataLogger::stop()
{
    if (!requests_ || idle() || state_.load() == State::Stopping ||
        state_.load() == State::Exporting) return;
    state_.store(State::Stopping);
    const Request request{Operation::Stop};
    if (xQueueSend(requests_, &request, 0) != pdTRUE) {
        failed_.store(true);
        force_close_.store(true); // Worker will discard queued rows and close.
    }
}

esp_err_t DataLogger::export_to_serial()
{
    if (!requests_) return ESP_ERR_INVALID_STATE;
    State expected = State::Idle;
    if (!state_.compare_exchange_strong(expected, State::Exporting)) return ESP_ERR_INVALID_STATE;
    const Request request{Operation::Export};
    if (xQueueSend(requests_, &request, 0) == pdTRUE) return ESP_OK;
    state_.store(State::Idle);
    return ESP_FAIL;
}

void DataLogger::worker(void *context)
{
    auto &logger = *static_cast<DataLogger *>(context);
    Request request{};
    while (true) {
        if (logger.force_close_.exchange(false)) {
            xQueueReset(logger.requests_);
            logger.close_file();
            logger.state_.store(State::Idle);
        }
        if (xQueueReceive(logger.requests_, &request, pdMS_TO_TICKS(100)) != pdTRUE) continue;
        switch (request.operation) {
        case Operation::Start: {
            const bool opened = logger.open_file() == ESP_OK;
            if (!opened) logger.failed_.store(true);
            State expected = State::Starting;
            // A Stop received during fopen must not be undone by this worker.
            logger.state_.compare_exchange_strong(expected, opened ? State::Recording : State::Idle);
            break;
        }
        case Operation::Append:
            if (!logger.failed() && logger.write_row(request.status) != ESP_OK)
                logger.failed_.store(true);
            break;
        case Operation::Stop:
            if (logger.close_file() != ESP_OK) logger.failed_.store(true);
            logger.state_.store(State::Idle);
            break;
        case Operation::Export:
            logger.print_file();
            logger.state_.store(State::Idle);
            break;
        }
    }
}

esp_err_t DataLogger::open_file()
{
    file_ = std::fopen(kLogPath, "w");
    if (file_ == nullptr) {
        return ESP_FAIL;
    }

    const int written = std::fprintf(
        file_,
        "time_s,state,reason,data_source,voltage_mV,current_mA,power_mW,heatsink_mC,"
        "probe1_mC,probe2_mC,probe3_mC,probe4_mC,output_level_0_to_10000\n");
    if (written < 0 || std::fflush(file_) != 0) {
        close_file();
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t DataLogger::write_row(const SystemStatus &status)
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

esp_err_t DataLogger::close_file()
{
    esp_err_t result = ESP_OK;
    if (file_ != nullptr) {
        if (std::fflush(file_) != 0) result = ESP_FAIL;
        if (std::fclose(file_) != 0) result = ESP_FAIL;
        file_ = nullptr;
    }
    return result;
}

esp_err_t DataLogger::print_file()
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
