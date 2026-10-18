# Board bring-up

The configuration follows `ProgramableLoad_PCB_1.1.kicad_sch` and
`ProgramableLoad_PCB_1.1.kicad_pcb` from the supplied
`Final PCB Design (Printed).zip`. Design-file connections are not confirmation
of the manufactured board or any later repairs.

## PCB status

A short between the 5 V rail and ground was reported during PCB bring-up.
A confirmed repair and powered retest have not been recorded in this repository.

Before connecting power, the hardware team must locate and repair the short,
complete unpowered checks, then verify the rails, current consumption and
component temperatures during current-limited power-up. Confirm the 5 V and
3.3 V supplies before attempting programming. Firmware cannot diagnose or
protect against a supply short before the controller is operating.

Keep the battery/load source disconnected and the MOSFET power stage physically
isolated during initial programming, display and sensor checks. None of the
current test modes provides physical load shutdown.

## GPIO assignments

Numbers below are ESP32 GPIO numbers, not module-pad or connector numbers.
The target is the original ESP32-WROOM-32E, not an ESP32-S3.

| Function | GPIO | Status |
|---|---|---|
| ADS7828 SDA / SCL | 33 / 32 | Verify 3.3 V I2C pull-ups |
| Shared TFT/DAC clock | 34 | Input-only; cannot generate a clock |
| MCP4922 SDI | 35 | Input-only; cannot generate DAC data |
| MCP4922 CS | 2 | R27 pulls CS LOW at reset; inactive state needs review |
| 74HC595 SER / RCLK / OE | 18 / 16 / 17 | Verify against the assembled board |
| TFT RS / RD | 4 / 5 | Driver holds RD HIGH for write-only operation |
| TFT CS / RESET | 12 / 14 | Check GPIO12 boot-strap level |
| TFT WR candidate | 13 | J3 pin 16, labelled Display Control 4; confirm actual WR |
| Encoder A / B / push | 22 / 23 / 21 | Verify 3.3 V input levels |
| UART RX / TX | 3 / 1 | MCP2200 TX / RX respectively |

GPIO34 and GPIO35 cannot drive outputs on this module. The code therefore
rejects TFT/DAC activation with the current assignments. A revised map must
match approved physical wiring; changing a constant alone does not reroute a PCB.

## Electrical checks affecting the firmware

These findings refer to the supplied design files. Check the fitted board
before recording them as repaired or verified.

| Area | Required check |
|---|---|
| Voltage divider | R7 is 1 MOhm from load-positive to Vsense; R8 is 10 MOhm from Vsense to ground. This ratio is unsuitable for the ADC. |
| ADC reference | Verify U7 COM grounding, supply and the internal 2.5 V reference before sampling. |
| Current input | The 5 V ACS37010 output connects directly to CH0 in the design. Its range needs conditioning for the ADC. |
| Encoder | R33/R34/R35 pull inputs to 5 V in the design; ESP32 inputs require safe 3.3 V levels. |
| DAC interface | Verify logic-level compatibility, CS during reset, reference voltages and both output channels. |
| TFT harness | Confirm signal names, power pins, 8-bit configuration and display timing. Use the actual module pinout. |
| Programming | Verify MCP2200 operation, 3.3 V UART levels and access to GPIO0/EN for BOOT/RESET. |
| Analogue ground | Confirm J2 /GND1 and its intended relationship to the main ground. |
| Analogue control | Confirm R18's fitted value, U4A feedback polarity and the D3-to-U5 pull-down path. |
| Shutdown | Establish the reference pair and electrical path that reliably turn all load MOSFETs OFF. |

Do not assume that zero DAC output or DAC shutdown turns the load off.
The OFF state, reference-update order, reset behaviour and loss of controller
power all need measurement with the power stage isolated.

## Calibration values

Values marked PROPOSAL in `main/project_config.hpp` are not confirmed fitted
components or calibration results.

| Input | Current software value | Confirmation required |
|---|---|---|
| Voltage | CH1, 12 kOhm top / 1 kOhm bottom, nominal 2500 mV ADC reference | Actual divider orientation/values and measured reference |
| Current | CH0, proposed ADC-only 10 kOhm / 10 kOhm attenuation, 2500 mV zero, 40000 uV/A sensitivity | Conditioning, zero-current offset, polarity and gain |
| Heatsinks | CH2 and CH3; use the hotter valid reading | Connector identities and sensor calibration |
| External probes | CH4 to CH7, Pt1000 | Connector identities and fitted sensor type |
| Temperature excitation | 3300 mV and 1 kOhm pull-ups | Measured supply, fitted resistance and self-heating |
| DAC references | 5000 mV | Actual A/B reference values and CC/CV scaling |

The design-file divider gives about 27.27 V at Vsense for a 30 V input.
The proposed 12 kOhm / 1 kOhm divider gives about 2.308 V. This proposal still
requires electrical approval, input protection and settling checks. Changing the
divider also changes analogue CV feedback, not just the displayed voltage.

Review measurement and output resolution during calibration. The driver rounds
ADC results to millivolts; with the proposed current attenuation, this adds
50 mA calculation steps. A 5 V, 12-bit DAC compared directly with a 40 mV/A
feedback signal gives about 30.5 mA per code. The menu's 10 mA setting step
does not establish physical resolution.

## Display and encoder

The TFT uses individual jumper wires. The design routes QH to DB0 through QA
to DB7, matching `kTftReverseDataBits = true`. Verify the full harness, especially
WR, CS, RS and RESET. If the data wires instead run QA to DB0 through QH to DB7,
the bit-reversal setting must match that wiring.

Use the module's verified supply and backlight connections. A GPIO is not a
backlight supply. Confirm GPIO12 is not pulled to an unsuitable boot level.
Screen timing, colours, orientation, available heap and refresh rate require
checks on the final board.

Only enable `kUseTft`, `kUseEncoder` and their verification flags after the
corresponding wiring checks. The current clock assignment must be corrected
before TFT activation. Keep DAC bench support disabled during GUI tests.

## Before physical load operation

Confirm both DAC output equations, inactive-channel settings, mode switching and
a reliable OFF path. Measure residual current and gate-to-source voltage during
Stop, a fault, reset and loss of communication. Verify analogue-loop stability,
MOSFET operating limits, cooling and the selected thermal trip temperature.
The 80 C software default is not a validated thermal limit.

The [test sequence](run_pcb_simulation.md) starts with serial simulation, then
display/input checks and calibrated measurement-only tests. Physical CC/CV
operation requires further implementation and verification.

## Device references

- [ESP32-WROOM-32E datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32e_esp32-wroom-32ue_datasheet_en.pdf)
- [ESP32 boot-mode selection](https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html)
- [TI ADS7828 datasheet](https://www.ti.com/lit/ds/symlink/ads7828.pdf)
- [Allegro ACS37010 datasheet](https://www.allegromicro.com/-/media/files/datasheets/acs37010-2-datasheet.pdf)
- [Microchip MCP4922 datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/22250A.pdf)
- [TE NB-PTCO-006 sensor](https://www.te.com/en/product-NB-PTCO-006.html)
- [SSD1963 datasheet](https://www.lcdwiki.com/res/MRB7963/SSD1963_DS_V1.1.pdf)
