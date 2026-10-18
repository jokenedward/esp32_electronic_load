# State machine

The same states support two different activities: a simulated load or
measurement-only recording. Neither drives the physical power stage.

| State | Meaning |
|---|---|
| STARTING | Storage, UI and the selected measurement source are initializing |
| IDLE | Ready to inspect readings, edit settings or start |
| PREPARING | Opening the CSV asynchronously; no load output is enabled |
| RUNNING | Simulation and logging, or real measurement recording |
| COMPLETE | Normal stop: user, timer or configured cutoff |
| FAULT | A latched protection trip, invalid data, timing or logging failure |

## Transitions

- STARTING → IDLE after initialization. If real ADC initialization failed,
  readings stay unavailable and Start is rejected; serial UI remains usable.
  Storage/UI initialization failures are reported through ESP-IDF.
- IDLE/COMPLETE → PREPARING only with valid settings and safe required readings.
  Simulation/SensorMonitor require voltage/current/heatsink and enabled probes.
  VoltageMonitor requires voltage only.
- PREPARING → RUNNING after the CSV opens and fresh readings pass the checks again.
  Preparation can be cancelled with Stop; a storage timeout becomes a fault.
- RUNNING → COMPLETE on Stop or timer expiry. Configured voltage/current
  cutoffs also stop the simulation or recording normally.
- RUNNING → FAULT on a checked protection trip, stale data, delayed control loop
  or logging error. A late logging failure after COMPLETE also becomes a fault.
- FAULT → IDLE after two seconds of safe recovery readings, storage completion
  and manual acknowledgement. Electrical/temperature recovery margins apply.
  Acknowledgement does not start another run.

The full checker runs in Simulation and SensorMonitor. VoltageMonitor checks only
voltage and timer. Monitor modes do not implement physical load shutdown.
See [software protection](software_protection.md) for the limits and test scope.

Simulation fault reasons include over-voltage, over-current, over-power,
heatsink/probe over-temperature, missing required probe, invalid/stale data
and output/logging failure. All are named in `system_types.cpp`.
