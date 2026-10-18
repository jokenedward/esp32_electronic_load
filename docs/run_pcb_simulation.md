# Programming and testing

Use VS Code with the ESP-IDF extension and ESP-IDF 6.0.2. The target is
`esp32`, not `esp32s3`. This project does not use the Arduino framework.

## 1. Computer checks

From an ESP-IDF terminal in the project folder:

```sh
idf.py build
```

On Mac or Linux with a C++ compiler:

```sh
sh tests/run_host_tests.sh
```

The host tests use generated values. They do not run the FreeRTOS tasks,
communicate with the PCB or prove that the power stage is safe.

## 2. Electrical checks before upload

Complete [board bring-up](board_bringup.md), including repair and verification
of the reported 5 V-to-ground short. Keep the load source disconnected and
physically isolate the MOSFET stage.

The MCP2200 UART connection uses 3.3 V logic, with TX connected to ESP32 GPIO3
and RX connected to GPIO1, plus a common ground. Confirm the actual programming
circuit and BOOT/RESET access before connecting the USB supply.

## 3. Upload serial simulation

Leave `main/project_config.hpp` unchanged for this test: Simulation selected,
TFT/encoder disabled and all verification flags false.

1. Connect the electrically checked board to the computer.
2. Select its USB serial port with ESP-IDF: Select Port to Use.
3. Run the following, replacing `YOUR_SERIAL_PORT` with the actual port.

```sh
idf.py -p YOUR_SERIAL_PORT flash monitor
```

For manual download mode, use the board's verified BOOT/RESET controls: hold
BOOT, press and release RESET, then release BOOT while the tool is connecting.
After flashing, reset the board to run the application. The serial baud rate
is 115200. A Bluetooth or debug-console port is not the PCB's USB-UART port.

Expected startup messages include:

```text
PCB simulation: ESP32 application running; CSV storage READY
ADC/current/temperature inputs SIMULATED; physical load control UNUSED
TFT: NOT TESTED (disabled) | encoder: NOT TESTED (disabled)
```

Check that the state reaches IDLE without repeated resets. A storage failure
blocks Start.

## 4. Menus and a normal run

| Key | Action |
|---|---|
| `n` / `p` | Next / previous menu row or value |
| `e` | Select; requests Stop during an active run |
| `b` | Back or cancel an edit |
| `s` / `x` | Start / Stop |
| `a` | Acknowledge a recovered fault; does not restart |
| `?` | Print one status snapshot |
| `d` | Export the last recording while stopped |

Press `?`: default idle readings are about 12 V, 0 A and 25 C. Press `s`,
wait 5-10 seconds, then press `?`. The default CC simulation should approach
1 A and 11.9 V. Press `x`; expect COMPLETE and USER_STOPPED.

Visit all six menus. Check Back, setting edits and Save/Cancel. An active run
must stop when Select is pressed from any screen. For a CV check, select CV
and an 11 V target; the simulated source is 12 V, so it cannot demonstrate
the complete physical voltage range.

Set a one-minute timer and check TIMER_EXPIRED after 60 seconds.
Settings and downloads must remain blocked while running.

## 5. Fault tests

Use the default limits unless the test states otherwise.

| Key | Simulated input | Expected reason |
|---|---|---|
| `0` | Normal readings | Restores inputs; does not acknowledge a fault |
| `1` | 31 V | OVER_VOLTAGE |
| `2` | 31 A | OVER_CURRENT |
| `3` | 20 V, 26 A, 520 W | OVER_POWER |
| `4` | 95 C heatsink | HEATSINK_OVER_TEMPERATURE |
| `5` | Missing current reading | INVALID_MEASUREMENT |
| `6` | Missing sample timestamp | STALE_MEASUREMENT |
| `7` | Probe 1 at 70 C | PROBE_OVER_TEMPERATURE when Probe 1 Stop is ON |
| `8` | Probe 1 disconnected | PROBE_DISCONNECTED when Probe 1 Stop is ON |

Start, inject one fault and check FAULT with simulated demand zero. Press `0`,
wait at least two seconds for the recovery indication, then press `a`.
The state must return to IDLE without restarting. Start separately for the next
test. Enable Probe 1 Stop for tests 7 and 8; its default limit is 60 C.

## 6. CSV export

After stopping, allow the file to close, then press `d` or select LOG DOWNLOAD
and DOWNLOAD CSV OVER USB. If the logger is still busy, wait and retry.
Copy only the CSV text between DISCHARGE_LOG_CSV_BEGIN and
DISCHARGE_LOG_CSV_END into a plain-text file with a `.csv` extension.

Check elapsed time, SIMULATED/MEASURED labels, readings and the final stop reason.
Voltage, current, power and temperatures use milliunits; divide those fields
by 1000 to obtain V, A, W and C. Empty readings indicate missing data.
The output-level field is a 0..10000 demand, not a measurement.

Do not send other commands during export, and inspect the file for status
messages mixed into the CSV. Save results before another Start, which replaces
the previous recording. Current storage/export limitations are listed in the
[verification record](verification.md).

## 7. Final-board checks still required

| Stage | Checks |
|---|---|
| TFT and encoder | After approved pin/wiring corrections, keep simulated readings and check all corners, colours, menus, direction, Save/Cancel and Stop during repaint. Releasing Stop must not restart. |
| VoltageMonitor | After all ADC inputs are electrically safe, enter fitted divider/reference values. Compare several low-energy known inputs with a meter and check recorded values. |
| SensorMonitor | Calibrate current zero/gain and all temperature channels. Check channel identity, invalid readings and enabled-probe disconnection using a supervised test fixture. |
| Runtime and storage | Check repeated Start/Stop, Stop during file opening, full/failed storage, CSV export, reset and power-loss recovery. |
| DAC and analogue control | Use a separately reviewed, isolated bench test to measure both outputs and verify the OFF path before implementing physical load control. |
| Power and cooling | After low-power control/shutdown tests pass, validate the operating envelope, thermal limits and sustained power capability. |

A responding device or successful transmission does not prove every component
works. The display is write-only and the DAC has no analogue readback; both need
external checks. Calibration requires reference instruments. Software cannot
clear a supply short or switch off a failed-short MOSFET.
