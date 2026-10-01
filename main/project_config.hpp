#pragma once
#include <array>
#include <cstdint>
#include "driver/gpio.h"

// PROPOSED pins/values require PCB rework. See docs/board_bringup.md.
namespace project_config {
enum class TestMode { Simulation, VoltageMonitor, SensorMonitor };
constexpr TestMode kTestMode = TestMode::Simulation;
constexpr bool kSimulationMode = kTestMode == TestMode::Simulation;

// Default: serial simulation. No sensor, display or DAC GPIOs touched.
constexpr bool kUseTft = false;
constexpr bool kUseEncoder = false;
constexpr bool kBoardReworkVerified = false;
constexpr bool kTftWiringVerified = false;
constexpr bool kEncoder3v3Verified = false;
constexpr bool kAdcInterfaceVerified = false; // COM, ALL input levels, reference
constexpr bool kCurrentSensorInterfaceVerified = false;
constexpr bool kTemperatureInterfaceVerified = false; // Pt1000s AND channel roles
constexpr bool kDacBenchTestVerified = false; // power stage physically disconnected

// No real-load mode yet: analogue loops and default-OFF kill circuit are pending.
constexpr uint32_t kControlPeriodMs = kSimulationMode ? 10 : 50;
constexpr uint32_t kDisplayPeriodMs = 500;
constexpr uint32_t kLogPeriodMs = 1000;
constexpr float kCcKp = 0.80F, kCcKi = 1.20F;
constexpr float kCvKp = 0.80F, kCvKi = 1.20F; // simulated plant only

// ADS7828 I2C pins/address match the uploaded design.
constexpr gpio_num_t kAdcSda = GPIO_NUM_33;
constexpr gpio_num_t kAdcScl = GPIO_NUM_32;
constexpr uint8_t kAdcAddress = 0x48; // A0=A1=GND; 7-bit address
constexpr uint32_t kAdcReferenceMv = 2500; // internal reference; calibrate later
constexpr uint8_t kVoltageChannel = 1;
constexpr uint8_t kCurrentChannel = 0;
constexpr int kAdcSamples = 4;

// PROPOSAL: 12k top / 1k bottom gives 2.308 V at 30 V.
// NOT the uploaded 1M / 10M. Hardware team must check tolerances, protection,
// ADC settling/filtering, and the changed analogue CV feedback scale.
constexpr uint32_t kVoltageDividerTopOhm = 12000;
constexpr uint32_t kVoltageDividerBottomOhm = 1000;

// ACS37010-050B5: proposed 10k/10k attenuation on ADC branch ONLY.
// Do not change analogue CC feedback without reviewing its reference scaling.
// No spare ADC channel reads VREF: use a measured zero-current calibration.
constexpr uint32_t kCurrentDividerTopOhm = 10000;
constexpr uint32_t kCurrentDividerBottomOhm = 10000;
constexpr int32_t kCurrentZeroMv = 2500; // nominal, not calibrated
constexpr uint32_t kCurrentSensitivityUvPerAmp = 40000;
constexpr bool kCurrentPositiveSlope = true;

// Six Pt1000 connectors, each with a 1k pull-up in the uploaded PCB.
// PROPOSED roles: CH2/3 heatsinks; CH4..7 four external probes. Confirm physically.
constexpr uint32_t kTemperatureSupplyMv = 3300;
constexpr uint32_t kTemperaturePullupOhm = 1000;
constexpr std::array<uint8_t, 2> kHeatsinkChannels{2, 3};
constexpr std::array<uint8_t, 4> kProbeChannels{4, 5, 6, 7};

// PROPOSED REWORK: GPIO19 replaces input-only GPIO34 as the shared clock.
// GPIO25/26 reclaimed from UNUSED SD signals. SD card support is disabled.
constexpr gpio_num_t kSharedClock = GPIO_NUM_19;
constexpr gpio_num_t kDacData = GPIO_NUM_26;
constexpr gpio_num_t kDacCs = GPIO_NUM_25;
constexpr uint32_t kDacReferenceMv = 5000;

// Existing shift-register data/latch/enable pins.
constexpr gpio_num_t kTftData = GPIO_NUM_18;
constexpr gpio_num_t kTftLatch = GPIO_NUM_16;
constexpr gpio_num_t kTftOe = GPIO_NUM_17;
constexpr gpio_num_t kTftRs = GPIO_NUM_4;
constexpr gpio_num_t kTftRd = GPIO_NUM_5; // held HIGH: no TFT reads
constexpr gpio_num_t kTftCs = GPIO_NUM_12; // preserve LOW boot strap
constexpr gpio_num_t kTftReset = GPIO_NUM_14;
constexpr gpio_num_t kTftWr = GPIO_NUM_13; // REWIRE from TFT F_CS to TFT WR
constexpr bool kTftReverseDataBits = true; // QH..QA -> TFT DB0..DB7

constexpr gpio_num_t kEncoderA = GPIO_NUM_22;
constexpr gpio_num_t kEncoderB = GPIO_NUM_23;
constexpr gpio_num_t kEncoderPush = GPIO_NUM_21;
constexpr int kEncoderTransitionsPerStep = 4; // check physical detents
constexpr bool kEncoderReverse = false;

// Prevent the original invalid-output mistake at compile time.
constexpr std::array<gpio_num_t, 11> kOutputPins{
    kSharedClock, kDacData, kDacCs, kTftData, kTftLatch, kTftOe,
    kTftRs, kTftRd, kTftCs, kTftReset, kTftWr};
constexpr bool output_pins_valid()
{
    for (unsigned i = 0; i < kOutputPins.size(); ++i) {
        const int pin = kOutputPins[i];
        if (pin < 0 || pin >= 34 || (pin >= 6 && pin <= 11) ||
            pin == 20 || pin == 24 || (pin >= 28 && pin <= 31)) return false;
        for (unsigned j = 0; j < i; ++j) if (pin == kOutputPins[j]) return false;
    }
    return true;
}
static_assert(output_pins_valid(), "Invalid, flash-reserved or duplicate output GPIO");
static_assert(kVoltageChannel < 8 && kCurrentChannel < 8 &&
              kVoltageChannel != kCurrentChannel, "Invalid ADC channel allocation");
static_assert(kAdcSamples > 0 && kAdcSamples <= 64, "Invalid ADC averaging count");
} // namespace project_config
