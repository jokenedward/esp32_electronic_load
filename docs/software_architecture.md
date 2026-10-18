# Code flow

The firmware is one ESP-IDF component, `main`. `app_main.cpp` owns the system
state and coordinates measurements, commands and logging.

## Startup

1. Read the configured test mode.
2. Initialize the simulation or the verified ADC interface.
3. Initialize CSV storage and UI queues.
4. Start the logging, input, screen and application tasks.
5. Enter IDLE.

The application does not send DAC reference commands. When the display bus is
used, it keeps DAC CS inactive. The power stage still needs to be physically
isolated for initial testing.

## Application loop

1. Read measurements and check their validity flags.
2. Handle Start, Stop, Acknowledge, Apply Settings and Download commands.
3. Check protection limits in Simulation and SensorMonitor; VoltageMonitor checks
   voltage and timer only. Only Simulation updates the simulated CC/CV output.
4. Keep faults latched until recovery checks and manual acknowledgement succeed.
5. Send a status snapshot to the UI.
6. Queue CSV rows approximately once per second. A separate logging task
   opens/writes/closes files and exports the CSV. Serial status is printed by the
   UI task only on request (`?`) or on state/recovery changes when no TFT is ready.

The target loop period is 10 ms in Simulation and 50 ms in the monitor modes.
An I2C timeout can extend a cycle, so these periods are not guaranteed hardware
protection response times.

## Input and display

The input task polls the encoder and serial input every 2 ms. The screen task
handles menu events and drawing. The application task processes setting changes,
which are only accepted while stopped.

Queues pass commands and status copies between tasks. This keeps the TFT
transfer out of the application loop. The display task yields between rows
during a repaint. An encoder press while preparing/running sends Stop directly
from the input task, without waiting for the screen task.

`board_drivers.cpp` handles communication with the devices. Sensor calculations
and menu logic are separate from ESP-IDF so they can be tested on a computer.
Raw ADC values stay in the driver/measurement code, not in shared status copies.
The UI has six menus and displays only normal engineering units, not ADC counts.

## Physical load control

The monitor modes only measure and record. Their setpoints do not drive the load,
and their protection settings do not trigger a physical shutdown.

Real control still needs the approved analogue-loop reference mapping and a
default-OFF shutdown circuit. The simulation's output value cannot be used
directly as a DAC code.

See [software protection](software_protection.md) for recovery rules and remaining
hardware checks. `protection.cpp` is separate from the simulated controller so
different teammates can maintain them.
See [final software and testing](final_code_and_testing.md) for assignments,
the missing integration data and the planned final operation sequence.
