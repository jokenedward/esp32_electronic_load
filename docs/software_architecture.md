# How the code runs

One ESP-IDF component, `main`, contains a few small parts. Start reading
`app_main.cpp`, then `hardware.cpp`, then `menu_model.cpp`.

## Startup

1. Select the configured test mode.
2. Initialize simulated measurements, or the verified ADC interface.
3. Initialize CSV storage and UI queues.
4. Start input, screen and application tasks.
5. Enter IDLE.

No application path sends DAC reference commands; the display bus holds DAC CS inactive.
A firmware lock does not electrically switch off an unsafe power stage.

## Application loop

1. Read measurements and their validity flags.
2. Handle Start, Stop, Acknowledge, Apply Settings or Download.
3. If running a simulation, check protections and calculate simulated CC/CV demand.
4. If recording real measurements, check voltage validity and the recording timer.
5. Publish a status snapshot for the screen.
6. Print status and append CSV approximately once per second.

Simulation has a 10 ms target period. Real measurement modes use 50 ms; a bus
timeout can make a cycle longer. These are scheduling targets, not verified
real-time protection guarantees.

## UI work

The input task polls encoder/serial input every 2 ms. The screen task interprets
menu events and repaints when needed. It receives snapshots rather than sharing
mutable application settings. Menu edits become commands; settings are accepted
only while stopped. The slow TFT transfer yields between rows.

Low-level bus code lives in `board_drivers.cpp`. Sensor calculations and menu
behaviour are separate from ESP-IDF, allowing computer-side tests.

## Deliberate limits

Real modes only observe and record. Their menu setpoints do not control the load,
and their protection settings are not a live hardware shutdown system.
The future real-control implementation must use the approved analogue-loop and
default-OFF shutdown design, not merely rename the simulated output as a DAC value.
