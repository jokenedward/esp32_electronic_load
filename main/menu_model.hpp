#pragma once
#include "system_types.hpp"

enum class UiEvent : uint8_t { None, Next, Previous, Select, Back };
enum class UiCommandType : uint8_t { Start, Stop, Acknowledge, ApplySettings, DownloadLog };
struct UiCommand {
    UiCommandType type{UiCommandType::Stop};
    LoadSettings settings{};
};

// No ESP-IDF dependencies here: menu behaviour can be tested on the computer.
class MenuModel {
public:
    enum class Screen { Main, Live, Mode, Protection, AutoStop, Probes, Logs, Adc };
    bool handle(UiEvent event, const SystemStatus &status, UiCommand *command);
    int field_count() const;
    const char *title() const;
    void field_text(int index, char *label, unsigned label_size,
                    char *value, unsigned value_size) const;
    Screen screen{Screen::Main};
    int selected{0};
    bool editing{false};
    LoadSettings draft{};
    const char *message{""};
    void cancel_pending_save() { save_pending_ = false; }
private:
    void adjust(int direction);
    bool save_pending_{false};
    uint32_t saved_revision_{0};
};

// Rejects illegal quadrature jumps; contact bounce moves forward then backward.
class RotaryDecoder {
public:
    void reset(uint8_t state) { previous_ = state & 3; movement_ = 0; }
    int update(uint8_t state, int transitions_per_step);
private:
    uint8_t previous_{3};
    int movement_{0};
};
