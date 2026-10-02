# ESP32 Programmable Electronic Load

Firmware for the ELEC5552 electronic-load project, using an ESP32-WROOM-32E-N4
and ESP-IDF 6.0.2.

The interface uses a 7-inch, 800 × 480 SSD1963 TFT and one rotary encoder.
The PCB design uses an ADS7828 ADC, ACS37010 current sensor, Pt1000 temperature
probes and MCP4922 DAC.

## Current status

The code supports menu testing, sensor readings and CSV logging. It starts in
simulation mode, with the TFT, encoder and real sensor interfaces disabled.

## Build

Open this folder in VS Code with the ESP-IDF extension. In an ESP-IDF terminal:

```sh
idf.py build
```

Use the `esp32` target, not `esp32s3`. The configuration uses 4 MB flash for the
N4 module, with a 1 MB application partition.
[ESP32-WROOM-32E datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32e_esp32-wroom-32ue_datasheet_en.pdf).

Build files are generated in `build/` and excluded from Git. Building also
generates the compiler information used by VS Code, so build once after cloning.
Each person needs their own ESP-IDF installation and serial-port settings.

For the USB-UART wiring and flashing steps, see
[programming the ESP32](docs/esp32_programming_connection.md).

## Test modes

Set `kTestMode` in `main/project_config.hpp`:

| Mode | Behaviour |
|---|---|
| `Simulation` | Generates simulated readings and runs the simulated CC/CV controller |
| `VoltageMonitor` | Reads voltage from ADS7828 CH1 |
| `SensorMonitor` | Reads all eight ADC channels and converts the verified sensor inputs |

Start runs the simulation or begins recording measured values, depending on the
mode. None of these modes drives the physical load. Protection settings apply
to the simulation; the monitor modes are for measurement and recording only.

Hardware flags stay false until the corresponding wiring checks are complete.
The display labels simulated data and shows `--` for unavailable readings.

## Files

| File in `main/` | Purpose |
|---|---|
| `app_main.cpp` | Startup, run state and application loop |
| `project_config.hpp` | Test mode, proposed GPIO assignments and calibration values |
| `hardware.cpp` | Simulated or measured sensor readings; blocks physical load control |
| `board_drivers.cpp` | ADS7828, MCP4922 and 74HC595 communication |
| `sensor_math.cpp` | Voltage, current and Pt1000 calculations |
| `menu_model.cpp` | Menu navigation and setting changes |
| `tft_ui.cpp` | Encoder/serial input and screen layout |
| `display.cpp` | SSD1963 initialization and full-screen drawing |
| `load_control.cpp` | Simulated CC/CV control and protection checks |
| `data_logger.cpp` | CSV recording and serial download |
| `system_types.hpp/.cpp` | Settings, measurements, states and error names |

The `.hpp` files contain declarations shared between source files.
`font_5x7.hpp` contains the display font. Tests are in `tests/`; setup and
hardware notes are in `docs/`. Presentations and thermal models are kept separately.

## Controls

- Turn: move through a menu or change the selected value.
- Short press: select, edit or save. On the live screen: Start, Stop or Acknowledge.
- Hold for at least 0.7 seconds, then release: go back or cancel an edit.

Serial menu keys are `n` (next), `p` (previous), `e` (select) and `b` (back).
Shortcuts are `s` (start simulation/recording), `x` (stop), `a` (acknowledge)
and `d` (download CSV).

Menus cover live readings, mode/setpoint, protection, automatic stops, external
probes, log download and ADC diagnostics. Settings can only change while stopped.
Each new recording replaces the previous CSV, so download it first if needed.

## Tests

On Mac or Linux with a C++ compiler:

```sh
sh tests/run_host_tests.sh
```

The tests cover sensor calculations, menu edits, encoder decoding, simulated
CC/CV behaviour and protection checks. Build results and hardware tests still
outstanding are listed in the [test record](docs/software_validation_2026-10-01.md).


For more detail, see [driver notes](docs/driver_documentation.md),
[code flow](docs/software_architecture.md) and [run states](docs/state_machine.md).
The [contribution guide](CONTRIBUTING.md) covers the team's GitHub workflow.

## License

[MIT](LICENSE).
