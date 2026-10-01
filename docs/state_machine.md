# State machine

The same states support two different activities: a simulated load or
measurement-only recording. Neither drives the physical power stage.

| State | Meaning |
|---|---|
| STARTING | Storage, UI and the selected measurement source are initializing |
| IDLE | Ready to inspect readings, edit settings or start |
| RUNNING | Simulation and logging, or real measurement recording |
| COMPLETE | Normal stop: user, timer or simulated cutoff |
| FAULT | A simulated protection trip, invalid data or logging failure |

## Transitions

- STARTING → IDLE after initialization. If real ADC initialization failed,
  readings stay unavailable and Start is rejected; serial UI remains usable.
  Storage/UI initialization failures are reported through ESP-IDF.
- IDLE/COMPLETE → RUNNING only with valid settings, a new CSV file and valid
  required measurements. Simulation requires voltage/current/heatsink;
  recording requires voltage.
- RUNNING → COMPLETE on Stop or timer expiry. Configured voltage/current
  cutoffs also stop the simulation normally.
- RUNNING → FAULT on a simulation protection trip or recording/logging error.
- FAULT → IDLE after acknowledgement and valid required measurements.
  Acknowledgement does not start another run.

The full protection checker is used **only in Simulation**. Real monitor modes
stop recording on invalid voltage or log failure; they do not implement load
shutdown. Loss of optional current/probe readings appears as unavailable data.

Simulation fault reasons include over-voltage, over-current, over-power,
heatsink/probe over-temperature, missing required probe, invalid/stale data
and output/logging failure. All are named in `system_types.cpp`.
