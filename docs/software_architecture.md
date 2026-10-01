# Code flow

The firmware is one ESP-IDF component, `main`. `app_main.cpp` owns the system
state and coordinates measurements, commands and logging.

## Startup

1. Read the configured test mode.
2. Initialize the simulation or the verified ADC interface.
3. Initialize CSV storage and UI queues.
4. Start the input, screen and application tasks.
5. Enter IDLE.

The application does not send DAC reference commands. When the display bus is
used, it keeps DAC CS inactive. The power stage still needs to be physically
isolated for initial testing.

## Application loop

1. Read measurements and check their validity flags.
2. Handle Start, Stop, Acknowledge, Apply Settings and Download commands.
3. In Simulation, check protection limits before updating the simulated CC/CV output.
4. In a monitor mode, check voltage validity and the recording timer.
5. Send a status snapshot to the UI.
6. Print status and write CSV approximately once per second.

The target loop period is 10 ms in Simulation and 50 ms in the monitor modes.
An I2C timeout can extend a cycle, so these periods are not guaranteed hardware
protection response times.

## Input and display

The input task polls the encoder and serial input every 2 ms. The screen task
handles menu events and drawing. The application task processes setting changes,
which are only accepted while stopped.

Queues pass commands and status copies between tasks. This keeps the TFT
transfer out of the application loop. The display task yields between rows
during a repaint.

`board_drivers.cpp` handles communication with the devices. Sensor calculations
and menu logic are separate from ESP-IDF so they can be tested on a computer.

## Physical load control

The monitor modes only measure and record. Their setpoints do not drive the load,
and their protection settings do not trigger a physical shutdown.

Real control still needs the approved analogue-loop reference mapping and a
default-OFF shutdown circuit. The simulation's output value cannot be used
directly as a DAC code.
