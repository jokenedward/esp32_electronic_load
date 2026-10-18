# Verification record

Software checks: 18 October 2026. Firmware source revision: `9ef0f5d`.
Target: ESP32-WROOM-32E-N4, ESP-IDF 6.0.2, 4 MB flash.

## Completed checks

| Check | Result |
|---|---|
| Host tests | Passed with C++17, warnings treated as errors, AddressSanitizer and UndefinedBehaviorSanitizer |
| Default ESP32 Simulation build | Passed with the installed ESP-IDF build tools |
| Application size | 187,936 bytes; 82% of the 1 MiB application partition free |
| Git whitespace check | Passed |

Host-test coverage includes sensor conversions, ADC/DAC command construction,
encoder decoding, all six menus, Back and Save/Cancel, simulated CC/CV response,
Start interlocks, protection limits, automatic stops, all eight injected sensor
faults, first-fault retention and acknowledgement without restart.

The tested configuration has Simulation selected, TFT/encoder disabled and all
hardware-verification flags false. The physical DAC output is not controlled.
The image-size result describes flash usage, not free runtime RAM.

These checks do not execute the FreeRTOS input, display and logging tasks on
the board, or verify ADC timing, calibration and physical shutdown. Earlier
display demonstrations on a different board do not validate the final PCB.

## PCB status and remaining verification

A 5 V-to-ground short was reported during PCB bring-up. Repair and controlled
power-up results have not been recorded here. Resolve that fault before
attempting further powered tests.

| Area | Remaining verification |
|---|---|
| Supply and programming | Short removed; stable 5 V/3.3 V rails; acceptable current and temperatures; successful upload, boot and serial communication |
| TFT and encoder | Approved output-capable pin map, safe input levels, full-screen drawing, navigation and Stop during repaint |
| Sensors | Fitted divider values, reference voltage, ADC COM, current calibration, temperature-channel identity and probe accuracy |
| Recording | Long runs, storage errors, repeated Start/Stop, CSV integrity and behaviour after reset/power loss |
| Physical control | DAC A/B equations, inactive references, mode changes, analogue-loop stability and physical OFF action |
| Protection and cooling | Measured shutdown response, reset/power-loss behaviour, thermal trip limit and sustained power capability |

No final-board firmware run or successful physical load test is established by
this software record. Operation at 30 V, 30 A or sustained 500 W has not been
validated. At 30 V and 30 A together the input power would be 900 W, above the
500 W design target.

## Known recording limitations

- Each new run replaces the previous recording; the file is capped at 512 KiB.
- SPIFFS formats itself if mounting fails, which can erase an existing log.
- Status messages can mix with serial CSV output. Inspect exports before analysis.
- A download request made while storage is busy can be rejected without a clear
  busy message. File-read and serial-output errors need stronger reporting.
- Recording uses internal flash; SD-card logging is not implemented.

These are recorded limitations, not completed fixes. Save important results
externally. Follow [programming and testing](run_pcb_simulation.md) for the
current test sequence and [board bring-up](board_bringup.md) for the electrical
checks and unverified configuration values.
