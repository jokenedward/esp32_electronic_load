# ESP32 Programmable Electronic Load

ELEC5552 firmware for the ESP32-WROOM-32E-N4, using ESP-IDF 6.0.2.

The board design uses an ADS7828 ADC, ACS37010 current sensor, Pt1000 probes,
MCP4922 DAC, and a 7-inch 800 x 480 SSD1963 TFT with one rotary encoder.

## Submission status

The firmware supports simulated CC/CV operation, menus, protection checks and
CSV recording. Measurement-only modes are provided for sensor testing after
the interfaces have been verified.

The default configuration uses serial simulation. The TFT, encoder and hardware
interfaces are disabled. Physical load control is not implemented: Start and
Stop do not switch the actual MOSFET load. The PCB issues and calibration values
still requiring confirmation are listed in [board bring-up](docs/board_bringup.md).

## Build and test

Open this folder in VS Code with the ESP-IDF extension and an ESP-IDF 6.0.2
terminal. The target is `esp32`, with 4 MB flash; it is not `esp32s3`.

```sh
idf.py build
```

Run the computer-based tests on Mac or Linux with a C++ compiler:

```sh
sh tests/run_host_tests.sh
```

Build output is generated locally in `build/` and is not included in Git.
See [programming and testing](docs/run_pcb_simulation.md) for upload instructions.

## Test modes

Set the mode in `main/project_config.hpp`.

| Mode | Purpose |
|---|---|
| `Simulation` | Generated readings and a simulated CC/CV controller |
| `VoltageMonitor` | Voltage measurement and recording from ADS7828 CH1 |
| `SensorMonitor` | Measurement and recording from all eight ADC channels |

None of these modes drives the physical load. Hardware-verification flags remain
false until the corresponding electrical checks are complete.

## Interface and recording

There are six menus: Live Measurements, Mode and Setpoint, Protection, Auto Stop,
External Probes and Log Download. Each submenu has a Back selection.
Pressing the encoder during an active run requests Stop.

Serial controls include `n/p` to navigate, `e` to select, `b` to go back,
`s` to start, `x` to stop, `a` to acknowledge a recovered fault,
`?` for status and `d` to export the CSV.

Recordings use internal flash, not the SD card. A new run replaces the previous
recording. Save the exported CSV before starting another run.

## Documentation

| Document | Contents |
|---|---|
| [Software structure](docs/software_architecture.md) | File responsibilities, code flow, states and protection |
| [Board bring-up](docs/board_bringup.md) | GPIO map, calibration assumptions and outstanding hardware checks |
| [Programming and testing](docs/run_pcb_simulation.md) | Upload procedure, menu checks, fault tests and CSV export |
| [Verification record](docs/verification.md) | Completed software checks, known limitations and remaining tests |

The `main/` folder contains the firmware and `tests/` contains the host tests.
See [team workflow](CONTRIBUTING.md) for collaboration.
