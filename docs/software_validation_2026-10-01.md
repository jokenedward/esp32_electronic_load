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

## GPIO baseline update - 7 October 2026

The GPIO map now follows the uploaded PCB: shared clock GPIO34, DAC data GPIO35
and DAC CS GPIO2. This replaces the earlier GPIO19/26/25 proposal in code.
The schematic/PCB SHA-256 hashes still match the 29 September review.

- Default serial Simulation rebuilt successfully with ESP-IDF 6.0.2.
- Existing host tests passed with AddressSanitizer and UndefinedBehaviorSanitizer.
- Compiler checks confirmed the 34/35/2 assignments and that the output map is invalid.
- Temporary configuration checks confirmed that enabling TFT or DAC bench testing
  is rejected by the output-pin assertion. Neither test changed the project flags.
- The bus initializer also rejects invalid output pins before any GPIO operation.

The earlier TFT-enabled build results above describe the previous proposed map;
they do not apply to this uploaded-PCB map. Hardware flags remain false, physical
load control remains locked, and no board was flashed or physically tested.
Sensor scaling and temperature-role proposals were not changed by this GPIO update.

## Software simplification review - 16 October 2026

- Removed the raw-ADC menu and raw channel arrays from shared status snapshots.
  ADC acquisition, clipping checks and engineering-unit conversions remain.
- Removed the once-per-second serial print from the application loop. The UI task
  prints a snapshot on `?`, on state/recovery changes without a ready TFT, or when
  entering the Live page over serial. Fault and initialization messages remain.
- Confirmed all six main-menu destinations, navigation wraparound and that a
  status refresh does not save/cancel an edit in the host tests.
- Added compile-time checks for unique/in-range sensor channels and nonzero
  sensor scaling/reference values.
- Host tests passed with AddressSanitizer/UndefinedBehaviorSanitizer and warnings
  treated as errors. The existing protection regression tests also passed.
- ESP-IDF 6.0.2 default Simulation build passed. Image size: 185,984 bytes
  (`0x2d680`); 82% of the 1 MiB application partition is free.
- esptool identified an ESP32 image with a 4 MB flash header, valid checksum and
  valid hash. `git diff --check` passed.

The image size is not a measure of available runtime RAM. The review simplifies
the interface/shared data; it does not claim a substantial firmware-size reduction.
No hardware was flashed or tested, and TFT/encoder/verification flags remain
false. The physical load path is still blocked. Host tests do not run the UI,
I2C, logging worker or other FreeRTOS tasks on a board.

[Final software and PCB test plan](final_code_and_testing.md) lists missing team
data, test stages and what software can/cannot establish about the components.

## PCB simulation preparation - 16 October 2026

- Added `simulation.hpp` so the firmware and host controller tests use the same
  generated measurements. Serial keys 0..8 select normal data or test faults.
- All eight injected fault cases passed the host tests, including recovery,
  acknowledgement without restart, and optional-probe enable behaviour.
- Default Simulation build passed with ESP-IDF 6.0.2. Final image size:
  187,264 bytes (`0x2db80`), with a valid ESP32 image checksum/hash and 4 MB header.
- First-boot messages label simulated inputs and report storage initialization.
  TFT/encoder messages distinguish disabled, initialization failure and driver
  readiness requiring a physical check.
- GPIO assignments and hardware-verification flags remain unchanged. No board
  was available, so no upload or physical wiring test was performed.
- Full QEMU execution was not available: the cached emulator requires missing
  Mac libraries. Host tests do not constitute a run of the full ESP-IDF firmware.

See [run the PCB simulation](run_pcb_simulation.md) for the first boot, normal
run, fault tests, recovery and CSV download sequence.

## Selectable Back - 16 October 2026

Every submenu now has a Back row, including Live. Back returns to the main menu
with the previous submenu highlighted. Value editing offers Save and Back/cancel;
there is no encoder long-press navigation. Pressing during PREPARING/RUNNING
still requests Stop before navigation.

Host tests passed for all submenu Back actions, fault-state navigation, edit
save/cancel, cancellation after invalid limits, and Stop priority. The ESP-IDF
6.0.2 build passed with an image size of 187,696 bytes (`0x2dd30`). The settings
remain Simulation with TFT/encoder and hardware-verification flags false.
The display layout still needs a physical screen check when hardware is available.
