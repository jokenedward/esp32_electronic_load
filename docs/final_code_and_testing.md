# Final software and PCB test plan

Reviewed 16 October 2026. Target: ESP32-WROOM-32E-N4, ESP-IDF 6.0.2.

The code builds and supports simulation and measurement-only testing. It is
**not final operating firmware yet**: it cannot switch the real load on or off.
Keep the power stage physically isolated during the tests below. A simulation
setting, successful upload or software Stop does not make the analogue circuit safe.

## 1. Keep the software small

There are six menus: Live Measurements, Mode and Setpoint, Protection, Auto Stop,
External Probes and Log Download. Raw ADC diagnostics are removed. ADC conversions
still run internally because voltage/current/temperature control needs them.
Errors, invalid-reading checks and protection must stay.

| Team section | Files in `main/` | Work remaining |
|---|---|---|
| Display and encoder | `menu_model`, `tft_ui`, `display` | Verify TFT wiring/timing, full-screen refresh and encoder direction on the PCB |
| Sensors | `hardware`, `sensor_math` | Enter approved scaling, calibrate and confirm all six temperature channels |
| Protection | `protection` | Approve limits and verify the physical shutdown action and response time |
| CC/CV control | `load_control`, DAC in `board_drivers` | Replace simulation-only output with verified A/B references and safe mode changes |
| Recording | `data_logger` | Check CSV download and required recording duration on the board |
| Integration | `app_main`, `project_config`, `system_types` | Combine verified sections, own shared settings and final GPIO map |

Each named source has its matching `.cpp`/`.hpp` where applicable. Keep common
settings in `project_config.hpp`/`system_types.hpp`; avoid separate pin maps in
each person's files. Tests are computer-only and are not included in the firmware.

### Intended final operation - not enabled yet

1. Boot with a verified electrical OFF state; never restart a previous run.
2. Read sensors and check settings, validity and limits before accepting Start.
3. Open the log, then apply the approved CC/CV reference pair in the approved order.
4. Repeatedly read sensors, check protection, update references as needed, refresh
   the display and record data.
5. On Stop or a fault, command the verified OFF state before slower display/log work.
6. Keep a fault latched until readings recover and the user acknowledges it.
   A separate Start is required.

`load_control.cpp` currently models a software PI controller for a simulated load.
The real PCB has analogue loops. Do not send the simulation's 0..10000 demand
straight to the DAC or add a second fast control loop without validating stability.
No pretend production mode or guessed OFF voltage has been added.

## 2. Information still needed

| Needed from the team | Why the code needs it |
|---|---|
| Confirmed PCB revision and corrected output-capable clock/data GPIOs | The submitted GPIO34/35 assignments cannot drive the TFT/DAC. A code-only pin change does not change PCB wiring. |
| Final TFT signal-to-GPIO map, especially WR; confirmed panel timing and 8-bit setup | The control labels on the PCB are not enough to prove the actual jumper wiring. |
| Fitted voltage-divider values, orientation and measured ADC reference | Present 12k/1k values are proposals, not the submitted 1M/10M divider. |
| Current-sensor conditioning, measured zero-current offset, direction and gain | The proposed ADC-only 10k/10k divider and nominal 2.5 V zero are unverified. |
| Which two temperature channels are heatsinks and which four are external probes; sensor type and excitation voltage | CH2/3 = heatsinks and CH4..7 = external probes is only a proposal. |
| Validated heatsink trip temperature, allowed operating envelope and fault response time | 80 C is a demonstration value, not a verified safe limit. Software polling cannot guarantee protection against every fault. |
| DAC A/B reference equations for CC and CV, inactive-channel setting and update order | Both analogue branches affect the MOSFET gates. The code needs more than just a DAC maximum voltage. |
| Verified OFF command and behaviour during reset, power loss and loss of communication | Zero DAC output and DAC shutdown are not established safe OFF actions. |
| Required recording duration and whether one saved run is sufficient | Current internal-flash CSV is capped at 512 KiB and replaced by the next run. Settings reset on reboot. |

Known electrical issues in the submitted design are recorded in
[board_bringup.md](board_bringup.md). They include ADC COM/input levels, encoder
5 V pull-ups, DAC logic levels and the input-only GPIO assignments. Flags must
describe completed checks, not bypass them.

The 14 October schematic review also found that U4A's current feedback polarity,
R7/R8, and the D3-to-U5 pull-down path need analogue review. Teammates being unsure
does not establish a safe DAC setting. Ask the lab demonstrator to review these
points before powering a connected load.

Physical 10 mA setpoint resolution is still unresolved: with a 5 V DAC reference
and direct comparison to a 40 mV/A current signal, one DAC code is about 30.5 mA.
The menu's 10 mA steps do not prove that the analogue output achieves them.
The ADC conversion also currently rounds to millivolts; with the proposed 2:1
current attenuation that adds 50 mA calculation steps. Review resolution and
precision alongside the final sensor/DAC scaling, not by adding display digits.

## 3. Test now, before the PCB arrives

For the first PCB simulation, including the new single-key fault inputs, use
[run_pcb_simulation.md](run_pcb_simulation.md).

From the project folder, in an ESP-IDF terminal:

```sh
sh tests/run_host_tests.sh
idf.py build
```

Expected: all host tests pass and the ESP32 build completes. Tests exercise
conversions, CC/CV simulation, all protection reasons, recovery and menu behaviour.
They do not emulate the PCB, TFT timing, FreeRTOS scheduling or actual shutdown.
No hardware or Arduino framework is needed for the host tests.

## 4. When the PCB arrives

### A. Electrical checks first

Have the hardware team verify the assembled revision, rails, grounds, shorts,
safe ESP32/ADC input levels, BOOT/EN access and the corrections above. Use the
agreed current-limited logic supply and physical power-stage isolation. Do not
start with a battery or a high-power input connected. Code cannot do these checks
before the MCU is powered, nor can it protect an over-voltage GPIO.

### B. Upload the simulated version

Keep the checked-in configuration: `TestMode::Simulation`; TFT, encoder and all
verification flags false. Use VS Code's ESP-IDF extension, not Arduino.

Select the actual serial port and run:

```sh
idf.py -p YOUR_SERIAL_PORT flash monitor
```

Replace `YOUR_SERIAL_PORT` with the port shown by the extension (on a Mac usually
`/dev/cu...`). For the MCP2200 manual BOOT/RESET sequence, follow
[programming instructions](esp32_programming_connection.md). Do not change the
target to `esp32s3`; the final board uses `esp32`.

This runs the firmware on the real ESP32 but supplies simulated sensor values.
It tests application behaviour, not physical ADC/DAC operation.

| Serial key | Action |
|---|---|
| `?` | Show one status snapshot: state, V/A/W, temperatures and time; no ADC counts |
| `n` / `p` | Next / previous menu item or value |
| `e` / `b` | Select / back shortcut; `e` stops an active run |
| `s` / `x` | Start / stop the simulation or measurement recording |
| `a` | Acknowledge a recovered fault; does not restart |
| `d` | Download the last CSV while stopped |

First check: press `?`, `s`, wait a few seconds, then `?`, `x`, `d`.
With default settings, idle is approximately 12 V, 0 A and 25 C; simulated CC
converges toward 1 A and approximately 11.9 V. These are model values, not ADC readings.
Status appears on request/state changes instead of printing every second.

Check all six menus, save/cancel, CC/CV selection, timer and Start/Stop. In the
simulated model, an 11 V CV target should settle near 11 V; a 12 V source model
cannot reach every voltage in the project's full range.

Every submenu has a selectable Back row. While editing, press to choose Save
or Back/cancel, turn to select, then press again. No encoder hold is required.

To test a fault without danger, in Simulation set the over-voltage limit below
the model's 12 V idle value (for example 11 V), then press Start. Expect
`OVER_VOLTAGE`, a latched fault and no run. Restore the limit to 30 V, wait for
the recovery indication, press `a`, and confirm IDLE. It must not restart itself.
For the timer, set one minute, Start and check `TIMER_EXPIRED` after 60 seconds.

Download and inspect the CSV: SIMULATED label, units, times and stop reason.
Do not send menu/status commands during export, since serial messages can mix
with the CSV. Save the file before the next Start. SPIFFS currently formats if
mounting fails; do not treat its single log as the only copy of important results.

### C. Test TFT and encoder with simulated data

Only after the GPIO corrections, actual TFT harness and 3.3 V encoder levels
are approved, enable their verified configuration flags as described in
[board_bringup.md](board_bringup.md). Rebuild and upload; keep Simulation and the
ADC/DAC flags disabled. The present GPIO34/35 map deliberately prevents this build.

Check all four screen corners, colours, text, all menus, encoder direction and
one-step movement. Stop must work from any menu while running, including during
a slow repaint. Releasing the Stop press must not start again. Reset must return
to Idle. If the TFT allocation/init fails, the serial interface remains available.

### D. Test real sensors without running the load

After ALL ADC pins, COM/reference and sensor interfaces are electrically safe,
enter measured calibration values and use `VoltageMonitor`, then `SensorMonitor`.
Only enable verification flags after those checks. These modes display engineering
units and `--` for unavailable values; Start records data but does not drive a load.

Use supervised, low-energy known inputs and compare against a trusted meter or
temperature reference. Check several points, not just zero. Stimulate one probe
at a time to confirm connector roles. Test probe disconnection with a supervised
test fixture, not by pulling arbitrary powered PCB wires. An enabled external
probe's loss must stop recording; optional disabled probes do not cause a trip.

Also check ADC communication loss, Stop during a repaint, repeated Start/Stop,
CSV contents, storage errors and power/reset recovery. These are pending board
tests, not claims that the existing host tests cover them.

### E. DAC, analogue loops and physical protection

This stage is blocked until the OFF path, reference mapping and safe interfaces
are verified. The application never calls the bench DAC driver. A separately
reviewed, isolated test must measure both DAC outputs and the gate-drive response.
Do not use a sweep of all GPIOs or DAC codes as an automatic component check.

After that verification, integrate real CC/CV control and test at low power under
lab supervision. Confirm Stop/fault shutdown time, reset/power-loss behaviour,
residual current and no automatic restart before increasing the load. Do not
qualify 500 W operation using this simulation; electrical and thermal validation
are separate requirements. 30 V at 30 A is 900 W and exceeds the 500 W limit.

## 5. Can software prove every component works?

No. Use the existing status/error checks plus controlled stimuli and instruments.

| Part | What the code can show | What still needs checking |
|---|---|---|
| ESP32 / USB-UART / flash | Boot, upload, serial commands and CSV write/download | Rail quality, reset wiring and all unused pins are not covered |
| ADS7828 / I2C | Conversion communication succeeds or fails; clipped/invalid samples rejected | Response is not proof of correct calibration, wiring or every ADC channel |
| Current/voltage sensors | Converted values change; required invalid readings block Start | Known inputs and meter comparison; stuck-but-plausible data can go undetected |
| Pt1000 probes | Valid temperatures or unavailable readings; enabled-probe faults | Connector identity, accuracy, lead resistance and thermal attachment |
| Encoder | Menu moves and button actions occur | Turn/press each control; disconnected inputs can resemble an idle encoder |
| TFT / 74HC595 | Firmware completes write operations | This driver is write-only: visually inspect the actual panel and all data lines |
| MCP4922 | Firmware constructs/transmits commands in an isolated bench test | No readback/acknowledgement: measure both analogue outputs with a meter/scope |
| Op-amps / MOSFETs / resistors / regulators / cooling | Some resulting V/I/temperature faults may be visible | Electrical/thermal tests; software cannot identify every failed part or turn off a shorted MOSFET |

An `ESP_OK` return is not an "all components passed" certificate. In Simulation,
successful readings provide no evidence that sensors are connected at all.

References: [ADS7828 input/reference/interface specifications](https://www.ti.com/lit/ds/symlink/ads7828.pdf),
[MCP4922 unidirectional interface, section 5.1](https://ww1.microchip.com/downloads/en/DeviceDoc/22250A.pdf),
[Espressif build/flash/monitor guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/start-project.html).
