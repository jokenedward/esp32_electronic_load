# Final PCB / firmware review — 29 September 2026

This report records the design and firmware as they stood on 29 September.
For the current software and proposed wiring changes, see
[board bring-up](board_bringup.md).

## Result

The design files include the voltage-divider values, but several connections
need correction before the board can be used. Do not connect the 30 V load input
or enable the power stage until these issues are resolved.

The review covered design files and source code. It did not include physical
board tests, circuit simulation, full DRC/ERC or project compliance testing.
Firmware and PCB files were unchanged during the review. Software locks do not
protect against an incorrectly wired analogue circuit.

## Files checked

Source archive supplied by the team: `Final PCB Design (Printed).zip`
(not included in this firmware repository).

Active files inside it:

- `Group Project/ProgramableLoad_PCB_1.1.kicad_sch`
- `Group Project/ProgramableLoad_PCB_1.1.kicad_pcb`

Historical copies inside `.history` and `.git` were not used. A schematic XML
netlist was exported using KiCad CLI 10.0.6. Values and 159 assigned pads for
the relevant ESP32, ADC, DAC, shift register, TFT connector, current sensor,
divider and pull-up components were compared with the PCB: no differences
were found in that selected comparison. Trace routing and the manufactured
board still need checking.

SHA-256 of the checked schematic:
`08300701b3380e80e0c8099b6db9befb1a37d4386f4f14f4b1949ade5286778a`

SHA-256 of the checked PCB:
`8d08addb4d84fb9da8bc86047b5697cf095515071721d1a17de0de058d01b43c`

## 1. Voltage divider — confirmed values, unsafe ratio

| Component | Value in both files | Connection |
|---|---|---|
| R7 | 1 MΩ | Current-sensor IP− / load-positive node to `Vsense` |
| R8 | 10 MΩ | `Vsense` to GND |
| U7 pin 2 | ADS7828 CH1 | Connected directly to `Vsense` |

The ideal unloaded calculation is:

`Vsense = Vin × R8 / (R7 + R8) = Vin × 10/11`

At 30 V, that gives **27.27 V**, not a safe ADC input. Actual voltage could
instead be limited by device protection or damage; do not apply 30 V to test it.
`Vsense` also feeds U4's high-impedance non-inverting input through R15.

U7 is supplied from 3.3 V through R19. Its reference pin only has a capacitor
to ground in the schematic, so normal firmware operation would need to enable
the internal 2.5 V reference. The conversion range is then 0–2.5 V relative to
COM. The ADC supply voltage is not the same thing as the measurement range.
[TI ADS7828 datasheet, electrical characteristics and reference section](https://www.ti.com/lit/ds/symlink/ads7828.pdf).

Simply exchanging R7 and R8 would give 2.727 V at 30 V, still above that
conversion range. The hardware team must choose a ratio with margin for maximum
input and tolerances, and check input protection and source impedance. The very
high resistor values also need checking against ADC sampling and leakage.
Changing this divider changes the analogue CV feedback scaling as well as the
software voltage calculation. Do not treat it as a firmware-only adjustment.

## 2. Confirmed electrical issues affecting software

| Area | What the files show | Required resolution |
|---|---|---|
| Clock output | U1 pad 6 = GPIO34 drives U8 SCK, U9 SRCLK and J3 pin 36 | Move the clock net to an output-capable GPIO. GPIO34 cannot generate it. |
| DAC data | U1 pad 7 = GPIO35 drives U8 SDI | Move DAC data to an output-capable GPIO. GPIO35 cannot generate it. |
| Encoder inputs | R33/R34/R35 pull GPIO23/GPIO21/GPIO22 to 5 V | Use safe 3.3 V input levels; correct the physical pull-up supply. |
| ADC common | U7 pin 11 COM is marked unconnected in schematic and PCB | Provide the correct common reference, normally analogue ground for these ground-referenced single-ended readings. |
| DAC logic levels | U8 MCP4922 VDD is 5 V; ESP32 drives it directly | Resolve the logic-level mismatch before relying on writes. |

GPIO34–39 are input-only on the original ESP32; GPIOs are not 5 V tolerant.
[Espressif hardware FAQ](https://docs.espressif.com/projects/esp-faq/en/latest/hardware-related/hardware-design.html).

ADS7828 single-ended readings use COM as the negative input.
[TI ADS7828 datasheet, channel-selection table](https://www.ti.com/lit/ds/symlink/ads7828.pdf).

MCP4922 specifies a minimum HIGH input of 0.7 × VDD: 3.5 V at a 5 V supply.
Thus a 3.3 V signal is not guaranteed to register HIGH. A suitable level
interface or a reviewed supply/reference redesign is needed; lowering the DAC
supply while leaving its references at 5 V is not a drop-in fix.
[Microchip MCP4922 datasheet, DC characteristics](https://ww1.microchip.com/downloads/en/DeviceDoc/22250A.pdf).

### Current sensing also needs conditioning

IC1 ACS37010LLZATR-050B5 is powered from 5 V and VOUT goes directly to U7 CH0.
Its nominal output is 2.5 V at zero current and increases by 40 mV/A, giving
3.7 V at +30 A. This exceeds the ADC's 2.5 V conversion range and, at +30 A,
its specified absolute input range with a 3.3 V supply. The sensor interface
must be redesigned before current measurements are enabled.
[Allegro ACS37010 datasheet](https://www.allegromicro.com/-/media/files/datasheets/acs37010-2-datasheet.pdf),
[TI ADS7828 datasheet](https://www.ti.com/lit/ds/symlink/ads7828.pdf).

IC1 VREF is labelled `Overcurrent Input`, but has no other node in the exported
netlist. It is a reference output, not a fault flag. The firmware's
VOUT-minus-VREF calculation at the time of this review would need changing
for this wiring.
[Allegro ACS37010 product description](https://www.allegromicro.com/en/products/sense/current-sensor-ics/integrated-current-sensors/acs37010).

## 3. TFT connections

The specified display is a 7-inch SSD1963, 800 × 480, with 8-bit mode
confirmed by the team. The PCB uses a **74HC595 shift register**, not the TTGO demo's eight
direct GPIO data outputs. The driver must shift a byte into U9, latch it, then
issue the TFT write strobe with the required control levels.

J3 uses a `Conn_02x20_Counter_Clockwise` symbol and a
`PinHeader_2x20_P2.54mm_Vertical` footprint. Do not assume matching schematic
positions mean matching physical connector positions.

If the actual TFT is the MRB7963/MRB7966-style module mentioned in the demo
notes, a same-number straight-through connection has these mismatches:

| Pin number | Final PCB J3 | LCDWiki module definition |
|---|---|---|
| 1 | Unconnected | GND |
| 2 | +5 V | EX_3.3V, normally unused |
| 3 | +5 V | NC |
| 5 | Unconnected | WR |
| 18 | Unconnected | +5 V input |
| 20 | GND | NC / touch interrupt |
| 21–28 | U9 QH through QA | DB0 through DB7 |

This comparison only applies if the fitted module matches those models.
Check its markings and the harness before connecting power. Do not put 5 V
on EX_3.3V. Record the full pin mapping if a custom harness is used.
[LCDWiki module pin definitions](https://www.lcdwiki.com/7.0inch_16BIT_Module_SSD1963).

J3 control signals in the PCB are GPIO4 → pin 4, GPIO5 → pin 6,
GPIO12 → pin 15, GPIO13 → pin 16, and GPIO14 → pin 17. They are only labelled
`Display Controls 1–5`; the TFT signal names must be confirmed. U9 QA–QH reach
J3 pins 28–21 respectively. Software can reverse data-bit order, but cannot
repair missing power or WR connections.

## 4. Final-board software map extracted from the files

This table records the design-file connections before any proposed corrections.

| Function | Design-file connection |
|---|---|
| MCU | U1 ESP32-WROOM-32E; N4 variant specified by the team |
| ADC I²C SDA / SCL | GPIO33 / GPIO32 |
| ADC address | U7 A0=A1=GND → 7-bit address 0x48 |
| ADC CH0 / CH1 | Current VOUT / voltage divider |
| ADC CH2–CH5 | TEMP_1–TEMP_4, connectors J7–J10 |
| ADC CH6–CH7 | TEMP_EXT_1–TEMP_EXT_2, connectors J19–J20 |
| Temperature divider pull-ups | R22–R25 and R43–R44: 1 kΩ to 3.3 V |
| DAC A / B | U8 VoutA → Iref; VoutB → Vref |
| DAC chip select | GPIO2; R27 pulls it LOW at reset |
| DAC clock / data | GPIO34 / GPIO35 — invalid output assignments |
| TFT shift-register data / latch / enable | GPIO18 / GPIO16 / GPIO17 |
| TFT shift-register clock | Shared GPIO34 — invalid output assignment |
| Encoder push / A / B | GPIO21 / GPIO22 / GPIO23, with unsafe 5 V pull-ups |
| UART programming RX / TX | ESP32 GPIO3 / GPIO1 to MCP2200 TX / RX |

The earlier BOM names NB-PTCO-006. TE identifies it as a **Pt1000 RTD**, not
a 10 kΩ NTC. If these are the fitted probes, replace the NTC conversion with
the correct RTD calculation and check excitation/self-heating. Confirm which
of the six connectors measure MOSFET/heatsink temperature and which are external.
[TE NB-PTCO-006 product details](https://www.te.com/en/product-NB-PTCO-006.html).

## 5. Firmware changes identified on 29 September

| File / area | State at time of review | Required final-board work |
|---|---|---|
| `main/project_config.hpp` | Simulation enabled; internal-ADC and PWM placeholders | Replace with approved external-ADC/DAC/TFT mapping and measured calibration values. Keep hardware locks until verified. |
| `main/hardware.cpp` | ESP32 internal ADC, NTC conversion, PWM output | Add ADS7828 acquisition and MCP4922 reference writes; use the actual probe model. |
| `main/tft_ui.cpp` | Serial text only; enabling TFT returns `ESP_ERR_NOT_SUPPORTED` | Implement final-board SSD1963/74HC595 transport and encoder navigation. |
| GUI layout | Full-screen scaling exists in the separate TTGO demo | Reuse menu/layout logic, not TTGO pin definitions or its S3-specific display transport. |
| CC/CV control | Simulation PI loop adjusts a generic output level | Review against the analogue CC/CV loops and two DAC references; do not blindly replace PWM duty with a DAC code. |
| Protection | Code assumes an independent load-enable GPIO | Agree on a physical default-OFF shutdown path and fault behaviour before enabling real output. |

A 400 × 240 drawing buffer can be scaled to the full 800 × 480 panel.
The shift-register transport's speed and RAM use need testing; refresh rates
from the TTGO demo cannot be assumed for this board.

### Programming and shutdown checks

- GPIO0 and EN have pull-ups but no connected boot/reset controls in the
  exported netlist. Confirm access for manual boot/reset or a proper automatic
  programming circuit. USB-UART TX/RX alone does not provide automatic reset.
  [Espressif boot-mode guidance](https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html).
- No independent MCU-controlled load-disable signal was identified; U8 SHDN
  is tied high. A software simulation flag cannot hold the physical MOSFET
  circuit off during boot or failure. The hardware team must verify this path.
- J2's `/GND1` is distinct from `GND` in the netlist. Confirm the external
  split-supply reference and ground return. R18 is still valued only `R`.
  These need hardware-team review; the analogue-loop stability was not audited.

## Open questions

1. Does the assembled/manufactured board match this ZIP, or have the divider,
   GPIO34/35, encoder pull-ups and ADC COM already been corrected?
2. What is the approved corrected pin map, including a verified J3-to-TFT
   cable map and the hardware output-disable method?
3. What values are actually fitted at R7/R8, and which temperature probes are
   on each connector?

After hardware corrections are agreed, bring up serial programming and GUI
with the load-power stage disconnected, then validate safe ADC test inputs,
then DAC references with the power stage disconnected, and only then conduct
supervised low-power load testing. Keep full-power testing out of this stage.
