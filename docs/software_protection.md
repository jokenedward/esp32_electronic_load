# Software protection

Updated 14 October 2026. Physical shutdown is still NOT implemented or verified.
Do not connect an energised power stage to test these changes.

## What the code does

- `protection.cpp`: checks limits, keeps the first fault latched and checks recovery.
- `app_main.cpp`: stops the simulated output before logging or displaying the fault.
- `hardware.cpp`: dates readings from the beginning of the ADC scan and aborts
  on a communication error rather than repeating timeouts across every channel.
- `data_logger.cpp`: a separate task owns the file. The application sends bounded,
  non-blocking requests. Full queues, failed writes and failed closes are faults.
- `tft_ui.cpp`: a debounced encoder press while preparing/running requests Stop
  directly, on any screen. Releasing that press cannot become another Start.

Simulation and SensorMonitor require valid voltage/current/heatsink measurements.
They check over-voltage, over-current, over-power, heatsink temperature and the
enabled external probes. Probe disconnection trips only when that probe's Stop
option is enabled. Required sensor failure or an implausible temperature trips.
VoltageMonitor intentionally checks only voltage and the timer. It is not a
substitute for full protection. Monitor faults stop recording, NOT the real load.

Start is rejected if required measurements or settings are invalid, or a checked
limit is already exceeded. PREPARING waits for the logger (maximum three seconds)
and rechecks measurements before entering RUNNING. Stop can cancel preparation.

Readings older than 250 ms are rejected. A gap longer than 250 ms between active
application cycles latches CONTROL_LOOP_TIMEOUT when the task resumes. This is
not an independent watchdog and cannot stop a permanently frozen processor.

Low-current cutoff has a one-second startup grace period; all upper limits stay
active throughout startup. This grace time and the temperature limits need bench
validation. The 80 C heatsink default is still a demo value, not a thermal rating.

Fault acknowledgement needs two seconds of consecutive healthy readings and the
logger to finish closing. Over-voltage/current/power trips must drop to 95% of
their limit. Temperature trips must cool 5 C below the limit. New invalid samples,
missing scans or settings changes restart the recovery wait. Acknowledge returns
to IDLE, never RUNNING; another Start is required. Old queued commands are cleared
on Stop, a fault and successful acknowledgement. A reboot never resumes a run.

Moving logging to another task removes synchronous file operations from the
application loop. Flash operations can still stall the processor; this is NOT a
guaranteed real-time protection system. Measure response time on the final board.

## Still needed before real load operation

The MCP4922 remains bench-only. `Hardware::disable_load()` only clears simulated
demand, and real enable/output calls remain blocked. No GPIO assignment, divider
value or hardware-verification flag was changed in this update. GPIO34/35 remain
input-only blockers for the submitted clock/data wiring.

Do not choose zero DAC output or DAC shutdown as an assumed OFF command. A DAC
shutdown does not disconnect the power stage. Do not remove the hardware locks
just because the software tests pass. A lost SPI connection, processor crash,
incorrect sensor reading or shorted MOSFET cannot be made safe by these checks.

Ask the analogue-circuit teammate:

1. With our existing circuit, what exact voltages on MCP4922 **A (Iref)** and
   **B (Vref)** guarantee that ALL MOSFETs are OFF, in both CC and CV operation?
   Does their update order matter? Please verify with the actual circuit rather
   than assuming 0 V or DAC shutdown is safe.
2. What happens to those references and the MOSFET gates if the ESP32 resets,
   loses power or stops sending commands while the DAC/op-amps stay powered?
3. How will we verify the OFF command at low power: gate-to-source voltage,
   residual load current, settling time and restart behaviour?

The divider and sensor calibration values are also awaiting team confirmation.
They must be correct before readings can be used for protection.

## Tests

Run `sh tests/run_host_tests.sh` and `idf.py build` from the repository root.
Host tests cover trip boundaries, stale/future timestamps, missing sensors,
invalid settings, extreme readings, startup grace, measured-mode checks, recovery
delays/hysteresis, first-fault retention and acknowledgement without restart.

Still requires a board test with the power stage isolated: slow/full storage,
Stop during log creation or a TFT repaint, encoder release after Stop, repeated
Start/Stop, ADC disconnection, serial export and simultaneous input commands.
Host tests do not exercise FreeRTOS scheduling or the asynchronous logger worker.
Only after the OFF condition and electrical interfaces are verified should the
team plan current-limited, low-power physical shutdown tests with lab supervision.
