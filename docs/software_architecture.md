# Software structure

The firmware is one ESP-IDF component, `main`. Common settings and measurements
are shared through `system_types.hpp`; board configuration is kept in
`project_config.hpp`.

## Files and responsibilities

Names below refer to the matching source and header files in `main/`.

| Section | Files | Responsibility |
|---|---|---|
| Integration | `app_main.cpp`, `project_config.hpp`, `system_types` | Startup, application state, shared settings and GPIO assignments |
| Menus and input | `menu_model`, `tft_ui` | Six menus, setting edits, encoder decoding and serial commands |
| Display | `display`, `font_5x7.hpp` | SSD1963 setup, drawing and full-screen scaling |
| Measurements | `hardware`, `sensor_math` | Sensor acquisition, conversion and validity checks |
| Control | `load_control`, `simulation.hpp` | Simulated CC/CV response and generated test inputs |
| Protection | `protection` | Limit checks, stop reasons, fault latching and recovery |
| Recording | `data_logger` | Internal-flash CSV recording and serial export |
| Device communication | `board_drivers` | ADS7828, MCP4922 and shared display-bus transfers |

Sensor calculations, menu logic, simulated control and protection can be tested
on a computer without ESP-IDF. The hardware drivers and FreeRTOS tasks need
separate board tests.

## Startup and application loop

Startup initializes the selected measurement source, storage and user interface,
then enters IDLE. Failed sensor initialization leaves readings unavailable.
Failed storage initialization blocks Start.

The application loop:

1. Reads measurements and checks for delayed cycles.
2. Processes Start, Stop, Acknowledge, setting changes and download requests.
3. Waits for the recording to open before accepting a run.
4. Checks protection and, in Simulation only, updates the simulated output.
5. Publishes the latest status and queues a CSV row approximately once per second.

Four tasks divide the work:

| Task | Work |
|---|---|
| Application | Measurements, commands, run state and protection; target period 10 ms in Simulation or 50 ms in monitor modes |
| Input | Encoder and serial polling; nominal 2 ms interval, with 30 ms button debounce |
| Screen | Menu events and display updates; normal refresh target 500 ms |
| Logger | File opening, writing, closing and export |

Queues pass copies of commands and status between tasks. Stop requests get
priority over queued commands. Display transfers yield between rows, and file
operations run outside the application task. These arrangements reduce blocking;
they do not guarantee a hardware protection response time.

## Run states

| State | Meaning |
|---|---|
| STARTING | Initializing the selected interfaces |
| IDLE | Ready to inspect readings, edit settings or request Start |
| PREPARING | Opening the recording and rechecking measurements |
| RUNNING | Simulated operation or measurement-only recording |
| COMPLETE | Stopped by the user, timer or an enabled automatic cutoff |
| FAULT | A protection, measurement, timing, output or logging fault is latched |

Start from IDLE or COMPLETE requires valid settings and acceptable readings.
PREPARING ends when the file opens; failure to prepare within three seconds
causes LOG_FAILURE. Stop can cancel preparation.

A fault stays latched until readings recover, storage finishes and the user
acknowledges it. Acknowledgement returns to IDLE, not RUNNING. Reset does not
resume a previous run.

## Menus and settings

The six menus are Live Measurements, Mode and Setpoint, Protection, Auto Stop,
External Probes and Log Download. Turn to select a row and press to open it.
Every submenu has a Back row. During editing, turn to change the value, press
to choose Save or Back, then turn and press again to confirm. Back cancels
the draft. No hold action is required.

Settings can only change while stopped. A debounced press during PREPARING or
RUNNING requests Stop from any screen. STOP BEFORE EDITING and STOP BEFORE
DOWNLOAD are reminders, not fault reasons.

The current target defaults are 1.00 A for CC and 12.00 V for CV. Menu steps
are 0.01 A and 0.1 V. Settings return to their defaults after reboot.

## Protection

Simulation and SensorMonitor check required voltage, current and heatsink
readings, electrical limits and enabled external probes. VoltageMonitor checks
voltage and the timer only. A monitor fault stops recording, not a physical load.

| Setting | Default |
|---|---|
| Over-voltage / over-current / over-power | 30 V / 30 A / 500 W |
| Heatsink limit | 80 C |
| External probe limits | 60 C, with each probe's Stop option OFF |
| Automatic voltage/current cutoffs and timer | OFF |

These are software defaults, not validated operating ratings. Invalid readings
block a run. A disconnected external probe trips only when its Stop option is ON.
Low-current cutoff has a one-second startup grace period.

Samples older than 250 ms are rejected. An active application-cycle gap over
250 ms produces CONTROL_LOOP_TIMEOUT when the task resumes; this is not an
independent watchdog. Recovery requires two seconds of healthy readings.
Electrical trips must fall to 95% of their configured limit, and temperature
trips must cool 5 C below their limit before acknowledgement.

The application clears simulated demand before updating the log and interface.
Physical MOSFET shutdown has not been implemented.

## Measurement and display drivers

The ADS7828 uses 100 kHz I2C at address 0x48. It discards the first conversion
after channel selection and averages four readings. Communication errors and
near-full-scale samples are marked invalid. Voltage, current and Pt1000
conversions use the values in `project_config.hpp`; unverified values are
identified in [board bring-up](board_bringup.md).

The MCP4922 driver provides bench-only command support. It is not called by
Start/Stop, and transmitted commands do not confirm analogue output voltage.

The SSD1963 driver sends bytes through a 74HC595. A 400 x 240 drawing buffer
is scaled to the full 800 x 480 panel. Two buffers use 192,000 bytes of RAM,
and only changed spans are retransmitted after the first frame. If display
initialization fails, the serial interface remains available. The submitted
GPIO map currently prevents TFT/DAC activation.

## Recording

The logger writes `/spiffs/discharge_log.csv` in internal flash at approximately
one-second intervals. The file is capped at 512 KiB and replaced by the next run.
Rows contain elapsed time, state, stop reason, data source, voltage, current,
power, heatsink temperature, four probe temperatures and requested output level.
Unavailable readings have empty fields. No SD-card driver is included.

Only Simulation updates the PI controller's 0..10000 output demand. This number
is not a physical DAC reference. Real CC/CV operation still requires verified
reference equations, mode switching and an electrical OFF path.
