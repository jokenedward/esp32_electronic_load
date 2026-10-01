# Software test record

1 October 2026. Target: ESP32-WROOM-32E-N4. Toolchain: ESP-IDF 6.0.2.

## Build results

| Configuration | Result |
|---|---|
| Default Simulation | Passed |
| VoltageMonitor with TFT/encoder enabled | Passed |
| SensorMonitor with TFT/encoder enabled | Passed |
| esptool image check | Valid ESP32 image, 4 MB flash header, checksum and hash |

The two monitor builds used temporary copies with hardware flags enabled for
compilation only. Those builds were not flashed and do not confirm the wiring.

The default application image was 182,240 bytes, leaving 83% of the 1 MiB
application partition free. This is flash storage, not available RAM.
TFT buffer allocation still needs checking on the board.

## Host tests

All assertions passed with C++17, warnings treated as errors, AddressSanitizer
and UndefinedBehaviorSanitizer.

Coverage included:

- ADS7828 channel commands, MCP4922 command frames and bit reversal.
- Voltage-divider, current-sensor and Pt1000 calculations.
- CC/CV response using the simulated load model.
- Protection limits and automatic-stop conditions.
- Menu navigation, save/cancel and settings acknowledgement.
- Blocking edits while running.
- Encoder decoding and contact bounce.

## Configuration at the time of testing

Simulation was the default. TFT, encoder and hardware-verification flags were
false. The application did not send physical DAC reference commands, and the
hardware abstraction rejected real load output.

The flash configuration was 4 MB, with the existing partition offsets retained.
No board was flashed or physically modified during these checks.

## Hardware tests outstanding

Boot and programming; display timing, colours, refresh rate and available heap;
encoder operation; I2C reliability; ADC calibration; Pt1000 self-heating; DAC
output levels; analogue CC/CV performance; shutdown response; and electrical
and thermal limits.

These need to be tested on the confirmed board with the power stage isolated
during initial checks.

## Build environment

ESP-IDF reported non-fatal configuration and debug-environment notices.
The build completed after the installed ESP-IDF paths and version were selected.
Use the ESP-IDF terminal for `idf.py build`.
