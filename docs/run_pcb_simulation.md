# Run the simulation on the PCB

The checked-in configuration is ready for a first serial simulation on the final
ESP32-WROOM-32E PCB. GPIO, ADC and DAC details do not prevent this simulation
because it uses generated measurements and the USB serial controls.

The PCB was not available during this update. The ESP-IDF build and computer tests
passed; the firmware has not been uploaded or observed running on the board.

## 1. Upload when the board arrives

Have the hardware team approve the initial logic/USB power connection and keep
the battery/load source disconnected. Open the project in VS Code with the
ESP-IDF extension and its ESP-IDF 6.0.2 terminal.

Leave `main/project_config.hpp` as supplied: Simulation mode, TFT and encoder
disabled. There is no need to change GPIOs or hardware-verification flags for
this first serial test. The target is `esp32`.

Select the PCB's USB serial port with **ESP-IDF: Select Port to Use**, then run:

```sh
idf.py build
idf.py -p YOUR_SERIAL_PORT flash monitor
```

Replace `YOUR_SERIAL_PORT` with the selected `/dev/cu...` port. If manual download
mode is required, hold BOOT, press/release RESET, then release BOOT during
Connecting. After flashing, press RESET to start the program. See the existing
[USB/programming instructions](esp32_programming_connection.md) for the circuit.

A missing USB serial port is a USB/UART connection problem to resolve before
uploading. The Bluetooth/debug-console ports are not the PCB.

## 2. Confirm the first boot

Look for these messages:

```text
PCB simulation: ESP32 application running; CSV storage READY
ADC/current/temperature inputs SIMULATED; physical load control UNUSED
TFT: NOT TESTED (disabled) | encoder: NOT TESTED (disabled)
```

They may appear in a different order. A storage failure is reported as FAILED
and prevents Start. The state should reach IDLE with no reboot loop.

This establishes that the application boots, the UART works and the internal
log storage mounts. It does not establish that the sensor, TFT or DAC wiring
works. Simulated inputs are labelled throughout the run and in the CSV.

## 3. First run: about ten seconds

Press each key separately in the serial monitor:

| Step | Key | Expected result |
|---|---|---|
| 1 | `?` | IDLE, about 12 V, 0 A and 25 C |
| 2 | `s` | PREPARING, then RUNNING |
| 3 | Wait 5-10 seconds, then `?` | Default CC operation approaches 1 A and 11.9 V; time increases |
| 4 | `x` | COMPLETE / USER_STOPPED; simulated demand returns to zero |
| 5 | `d` | CSV appears between the begin/end markers |

Wait until the log has closed before downloading. Save the CSV before the next
Start, which replaces it. A download rejected while storage is busy can be
retried once stopped/idle. Avoid other commands during the serial download.

Use `n`/`p` to move and `e` to select. Each submenu has a Back row: highlight it
and press `e`. While editing, change the value, press `e` to choose Save or Back,
turn with `n`/`p`, then press `e` again. Back cancels the edit. The `b` shortcut
also returns/cancels. No long press is needed on the encoder. During a run, a
press stops first; select Back after stopping. All six menus are available through
serial. Settings can be changed while stopped. In the Mode
menu, choose CV and 11 V to exercise the simulated voltage controller.
While preparing/running, the TFT footer and serial status show `STOP BEFORE EDITING`
and `STOP BEFORE DOWNLOAD`. These are reminders to stop first, not fault messages.

## 4. Try a protection fault

Start with the default settings. Press `s`, wait for RUNNING, then press `4`.
The generated heatsink value becomes 95 C. Expect FAULT with
HEATSINK_OVER_TEMPERATURE and simulated demand zero.

Press `0` to restore normal readings. Wait for the recovery message (at least
two seconds), press `a`, then `?`. Expect IDLE. It must not restart until `s`.

The following test keys work only in Simulation mode:

| Key | Generated test input | Expected fault with default limits |
|---|---|---|
| `0` | Normal model | Clears the injected input; fault still needs recovery/acknowledgement |
| `1` | 31 V | OVER_VOLTAGE |
| `2` | 31 A | OVER_CURRENT |
| `3` | 20 V and 26 A = 520 W | OVER_POWER |
| `4` | 95 C heatsink | HEATSINK_OVER_TEMPERATURE |
| `5` | Missing current reading | INVALID_MEASUREMENT |
| `6` | Missing reading timestamp | STALE_MEASUREMENT |
| `7` | External probe 1 at 70 C | PROBE_OVER_TEMPERATURE if Probe 1 Stop is ON and its limit is below 70 C |
| `8` | External probe 1 disconnected | PROBE_DISCONNECTED if Probe 1 Stop is ON |

Enable Probe 1 Stop in the External Probes menu before testing `7`/`8`. Its
default limit is 60 C. Optional probes do not trip when their Stop option is OFF.
Changed temperature limits can change the expected result for `4`/`7`.

Only the first fault is retained. Use `0`, wait, acknowledge, then Start between
tests. These keys only change generated values; they send no ADC/DAC commands.

Set a one-minute timer in Auto Stop, Start, and check TIMER_EXPIRED after 60 seconds.
Also confirm edits/downloads are blocked while running and Stop responds promptly.

## 5. What this run checks

| Can be checked on the PCB in this configuration | Still untested |
|---|---|
| ESP32 boot, tasks, USB serial controls | External TFT and encoder wiring |
| CC/CV software response to the shared model | Actual current/voltage/temperature sensors and ADC |
| Limit checks, fault latching, recovery and no automatic restart | DAC outputs, op-amp loops, physical MOSFET shutdown |
| Internal-flash CSV writing and download | Analogue accuracy, timing, cooling and power ratings |

TFT/encoder and real sensor tests come after this serial run. They require their
actual connections. The [full PCB test plan](final_code_and_testing.md) describes
that sequence. A simulated reading does not diagnose which physical component
is faulty.

Before the hardware arrives, `sh tests/run_host_tests.sh` exercises the same
simulation model, menu logic, controller, protection checks and all eight injected
faults on the computer. It does not run FreeRTOS or emulate the PCB peripherals.
