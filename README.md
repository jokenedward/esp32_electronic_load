# ESP32 Programmable Electronic Load

ESP-IDF firmware for the ELEC5552 electronic load, using the **ESP32-WROOM-32E-N4**.
This is the final-board project, not the separate TTGO/LILYGO demo.

## Start here

Working with teammates? Read [the short collaboration guide](CONTRIBUTING.md).

The software now includes the 7-inch GUI, encoder menus, ADS7828 sensor acquisition,
Pt1000 conversion and a separate MCP4922 bench driver. **The physical power stage
is not enabled by this firmware.** The uploaded PCB has unresolved electrical
issues and may not be the board your group actually printed.

Read [Board setup and checks](docs/board_bringup.md) before wiring or changing flags.
The proposed pin map is not approval to rework a board that has not been identified.
The KiCad files and uploaded ZIP have not been changed.

## Three test modes

Choose `kTestMode` in `main/project_config.hpp`:

| Mode | What it does | What Start does |
|---|---|---|
| `Simulation` (default) | Generates fake voltage/current/temperatures | Runs and logs the simulated CC/CV load |
| `VoltageMonitor` | Reads only ADS7828 CH1 | Records measured voltage; no physical output |
| `SensorMonitor` | Reads the eight ADC channels; converts verified sensors | Records available measurements; no physical output |

Default TFT, encoder and hardware-verification flags are all **false**. This lets
you build and explore menus over serial without assuming the PCB is safe.
The GUI labels simulated data. Missing real measurements appear as `--`, not zero.
Protection settings act on the simulated load only; monitor modes are not a
physical protection system.

## What belongs in this repository

Keep `main/` (firmware), `tests/`, the current setup/safety notes in `docs/`,
the CMake files, `sdkconfig`, `sdkconfig.defaults`, `partitions.csv`, and the
README, collaboration guide and license. The hidden Git and VS Code folders
support version control and editing; personal editor settings are not uploaded.

Presentations, thermal models, old build folders and superseded hardware reports
have been moved to a separate local archive outside this repository.
No firmware source files were removed.

`idf.py build` recreates `build/` when needed. It remains ignored by Git.
After a cleanup or fresh clone, build once to regenerate the compiler include
paths used by VS Code.

## Files in plain language

| File in `main/` | Purpose |
|---|---|
| `project_config.hpp` | Test mode, proposed pins, resistor values and verification flags |
| `app_main.cpp` | Startup, Start/Stop, system state and periodic work |
| `hardware.cpp` | Chooses fake or real measurements; keeps real load control locked |
| `board_drivers.cpp` | Low-level ADS7828, MCP4922 and 74HC595 communication |
| `sensor_math.cpp` | Turns ADC values into voltage, current and Pt1000 temperature |
| `menu_model.cpp` | Menu navigation and setting edits |
| `tft_ui.cpp` | Encoder/serial inputs and arranging the screen |
| `display.cpp` | Sends the full 800 × 480 image to the SSD1963 |
| `load_control.cpp` | Simulated CC/CV control and protection decisions |
| `data_logger.cpp` | Records CSV and downloads it through USB/UART |
| `system_types.hpp/.cpp` | Shared settings, measurements and error names |

Corresponding `.hpp` files declare what the `.cpp` files provide.
The small built-in font is in `font_5x7.hpp`. No Arduino or GUI framework is required.

## Build and test

Open **this folder** in VS Code with the ESP-IDF extension. Use an ESP-IDF terminal:

```sh
idf.py build
```

The current target is `esp32`, with 4 MB flash for the N4 module.
The existing 1 MB application partition and storage layout are retained.
The N4 flash size is documented in the
[Espressif module datasheet, series comparison](https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32e_esp32-wroom-32ue_datasheet_en.pdf).

Computer-side tests (Mac/Linux with a C++ compiler):

```sh
sh tests/run_host_tests.sh
```

These test calculations, menus, encoder decoding, simulated CC/CV behaviour and
protection logic. They do **not** test real wiring, screen timing or safe load control.
See [the verification record](docs/software_validation_2026-10-01.md) for checks performed.
For flashing, use [Programming connection](docs/esp32_programming_connection.md)
only after the electrical checks. No board was flashed during this update.

## Controls

- Turn encoder: move selection; while editing, change the value.
- Short press: open/select/edit/save. On Live Measurements: Start/Stop/Acknowledge.
- Hold for at least 0.7 seconds and release: Back, or cancel an edit.
- Serial equivalents: `n` next, `p` previous, `e` select, `b` back.
- Serial shortcuts: `s` simulate/record, `x` stop, `a` acknowledge, `d` download CSV.

Menus: Live Measurements, Mode and Setpoint, Protection, Auto Stop,
External Probes, Log Download, ADC Diagnostics. Settings cannot change during a run.
Download before starting another recording: a new recording replaces the old CSV.

## Still required for the final project

Approved wiring and calibration; a verified default-OFF hardware shutdown path;
validated analogue CC/CV reference mapping and stability; setpoint-resolution
checks; thermal/electrical testing; and physical GUI/encoder testing.
A successful build is not proof of 30 V / 30 A / 500 W operation.

More detail: [drivers](docs/driver_documentation.md),
[code flow](docs/software_architecture.md), [states](docs/state_machine.md).
Dated audit documents describe the files at the time of each audit, not the
current firmware.

## License

MIT. See `LICENSE`.
# esp32_electronic_load
# esp32_electronic_load
