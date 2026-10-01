# Board bring-up

Wiring review: 1 October 2026.

The TFT connection uses individual jumper wires. The printed PCB revision still
needs to be checked against `Final PCB Design (Printed).zip`. The pin assignments
and component changes below are proposals pending that check and hardware-team
approval. Enter the agreed values in `main/project_config.hpp`.

## 1. Test setup

Start with Simulation and the host tests. Menus can be checked over serial once
the logic hardware is electrically safe. TFT, encoder and hardware-verification
flags are false by default.

Do not connect the load source or energize the MOSFET power stage during GUI,
ADC or DAC bench testing. **Physically isolate it** using an agreed hardware
method. The code not driving a DAC does not prove an analogue circuit is off.

The application blocks physical load control in all three test modes, even
with the hardware-verification flags enabled.

## 2. PCB checks

| Check | Why it matters |
|---|---|
| Printed board matches `ProgramableLoad_PCB_1.1` in the ZIP | Required before applying this pin map or rework |
| Actual R7/R8 values and orientation | Design files show an unsafe 1 MΩ top / 10 MΩ bottom divider |
| U7 COM has the correct ground connection | ADS7828 COM is unconnected in the design files |
| Are GPIO34/35 clock/data assignments corrected? | These are input-only pins |
| Encoder pull-ups are 3.3 V | R33/R34/R35 connect ESP32 inputs to 5 V in the design files |
| Current-sensor VOUT is conditioned before CH0 | The design connects the 5 V sensor directly to the ADC |
| DAC logic levels, CS at reset and output-off method | Level conversion and shutdown behaviour need checking |
| Actual screen pin labels and power/backlight requirements | J3 pin numbers cannot be assumed to match the TFT |
| Temperature-probe connector roles and fitted type | Code assumes Pt1000; proposed roles need confirmation |
| EN/GPIO0 access and common ground references | Required for reliable programming and measurements |

The [29 September PCB review](final_pcb_firmware_review_2026-09-29.md)
lists the net connections behind these findings. Its J2 ground-reference,
unvalued R18 and programming-reset issues also need resolving.

## 3. Proposed GPIO map

GPIO numbers below refer to **ESP32 GPIO names**, not module-pad numbers.
This map is for the original ESP32, not the TTGO/S3 prototype. GPIO44 is not
available on this module.

| Function | Proposed GPIO | Wiring check/change |
|---|---|---|
| ADC SDA / SCL | 33 / 32 | Existing I2C wiring; verify 3.3 V pull-ups |
| Shared TFT/DAC clock | 19 | Move clock from GPIO34 |
| DAC SDI | 26 | Reclaim unused SD data-in; move from GPIO35 |
| DAC CS | 25 | Reclaim unused SD CS; move from GPIO2 |
| 74HC595 SER / RCLK / OE | 18 / 16 / 17 | Existing shift-register pins |
| TFT RS / RD | 4 / 5 | RD is held high; screen is write-only |
| TFT CS / RESET | 12 / 14 | Verify GPIO12 stays LOW during reset/boot |
| TFT WR | 13 | Jumper to TFT WR, not TFT F_CS |
| Encoder A / B / push | 22 / 23 / 21 | Correct physical pull-up supply to 3.3 V |
| ESP32 programming RX / TX | 3 / 1 | MCP2200 TX / RX respectively |

Moving a net means disconnecting its old connection where required and verifying
continuity/isolation, not just adding wires that join the old and new GPIOs.
**Do not cut or solder until the correct PCB is confirmed and the group approves
the rework.** SD card use is excluded by this proposal; leave the TFT SD interface
disconnected so it cannot conflict with GPIO25/26 or the shared clock.

At 5 V DAC supply, 3.3 V logic HIGH is not guaranteed. Use an approved 3.3-to-5 V
interface for DAC SCK, SDI and CS, with CS safely inactive during reset.
Keep the 74HC595 side of the shared clock at 3.3 V; do not level-shift the whole
clock net to 5 V. R27's original CS pull-down also needs review.
[Microchip DAC electrical specifications](https://ww1.microchip.com/downloads/en/DeviceDoc/22250A.pdf).

GPIO12 is a boot strap controlling flash supply selection. If the TFT pulls it
high during reset, revise the assignment with the group rather than trying to
fix the resulting boot failure in software.
[Espressif boot-mode guidance](https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html).

### Individual TFT jumpers

Use the actual TFT's **signal names**, not an assumed straight-through cable:

| Shift-register output | TFT data input |
|---|---|
| QH | DB0 |
| QG | DB1 |
| QF | DB2 |
| QE | DB3 |
| QD | DB4 |
| QC | DB5 |
| QB | DB6 |
| QA | DB7 |

This matches `kTftReverseDataBits = true`. If the approved harness instead uses
QA→DB0 through QH→DB7, change that one flag to false.

Connect the control signals from the previous table. Hold unused TFT F_CS
inactive at the appropriate logic level (normally 3.3 V HIGH on the referenced
module); it is **not** the LCD write input. Verify display 8-bit configuration.
Do not assume unused DB8–DB15, touch or SD pins need wiring; follow the module's
actual interface configuration.

Supply the TFT only through its verified power pins, and connect logic grounds
correctly. Keep the externally powered backlight on the module's approved
backlight circuit. Do not infer LED_A voltage/current from its name or use a
GPIO as its supply. The J3 power-pin assignments in the design do not match the
conditional MRB7963-style comparison in the audit.
[LCDWiki module pin table](https://www.lcdwiki.com/7.0inch_16BIT_Module_SSD1963).

## 4. Voltage input: required hardware correction

The divider in the design files gives approximately 27.27 V at the ADC for a
30 V input. Do not apply that input. Swapping R7 and R8 is not enough.

The proposed values in the configuration are **12 kΩ top / 1 kΩ bottom**:

`ADC voltage = input voltage / 13`

At 30 V this is about 2.308 V, below the configured internal 2.5 V ADC reference.
These are proposed values only. The group must review maximum input/transients,
tolerances, resistor ratings, filtering, settling and input protection, then
measure and enter the fitted values. Changing R7/R8 also changes the analogue
CV feedback; its DAC-reference scaling must be updated accordingly.

U7 COM must have the correct ground reference. All ADC pins must be safe even
in voltage-only mode. Software rejecting a clipped reading cannot protect an
over-voltage input.
[TI ADS7828 reference/input specifications](https://www.ti.com/lit/ds/symlink/ads7828.pdf).

## 5. Current and temperature inputs

- Current: the proposed ADC-only 10 kΩ / 10 kΩ divider halves VOUT.
  Do not alter the analogue CC feedback branch without redesigning its scaling.
  Confirm ADC drive/settling and protection, calibrate zero and sensitivity.
- Temperature: proposed CH2/3 = heatsinks and CH4–7 = four external Pt1000 probes.
  Confirm the fitted probes and connector roles before using this assignment.
- With a 3.3 V supply and 1 kΩ pull-up, a Pt1000 at 0 °C dissipates approximately
  2.7 mW. Check the resulting self-heating against the sensor datasheet and
  mounting before accepting temperature accuracy.
  [TE sensor details](https://www.te.com/en/product-NB-PTCO-006.html).

Current and temperature readings stay unavailable until their interface flags
are true. SensorMonitor provides raw ADC diagnostics once the ADC interface
has been checked.

## 6. Test sequence

Set each verification flag only after its physical checks are complete.
Rebuild after changing the configuration.

### A. Menus with simulated readings

After the power stage is physically isolated and the corrected clock/DAC-CS/TFT
wiring is approved, keep `kTestMode = TestMode::Simulation` and set:

- `kBoardReworkVerified = true`
- `kTftWiringVerified = true`
- `kUseTft = true`

For the rotary encoder, also verify 3.3 V levels and set
`kEncoder3v3Verified = true`, `kUseEncoder = true`.
Otherwise use serial menu keys. Keep ADC and DAC verification flags false.

Test all menus, save/cancel and Start/Stop. The first transfer writes all
800 × 480 pixels; allow time for the software bus to repaint. Check the panel
timings, colours, orientation and refresh speed on the actual display.

### B. Voltage display and recording

After **all ADC input levels**, reference/COM and divider have been checked:

- Select `TestMode::VoltageMonitor`.
- Set `kBoardReworkVerified = true`, `kAdcInterfaceVerified = true`.
- Enter the actual approved divider values.
- Keep current/temperature/DAC flags false. TFT/encoder may remain as in Stage A.

Start with a supervised, low, current-limited test input to the verified sensing
circuit, keeping the load-power stage isolated. Compare the displayed voltage
with a trusted meter at several known points. Do not start at 30 V.
Confirm CH1 diagnostics, missing-sensor display and CSV values.

Start now means **record voltage**, not start drawing current.
Stop ends recording. A timer can end recording too. Download before the next
Start, because a new run replaces the previous CSV.

### C. All sensors

Select `TestMode::SensorMonitor` only after safe inputs are confirmed.
Enable current and temperature conversion flags only after calibration and
connector-role checks. Verify probe disconnects and implausible/clipped values.
There is still no physical load operation.

### D. DAC bench test and load control

The application does not call the bench DAC class. A separate, reviewed test
must verify channel outputs with the power stage disconnected before integrating
it into real control. Do not enable it merely by changing the normal test mode.

A 5 V / 4096 DAC step is approximately 1.22 mV. If compared directly against the
nominal 40 mV/A current-sensor output, that corresponds to about **30.5 mA per
code**, not the required 10 mA setting step. This is a calculation for direct
feedback comparison, not a final prediction of the full analogue circuit.
Review gains/offsets/reference range or DAC resolution. GUI step size alone
does not establish physical resolution.

DAC zero or software shutdown is **not** a proven output-OFF command. Approve a
default-OFF hardware kill path, mode/reference behaviour, stability, maximum
ratings and thermal limits before adding physical load control. Only then
plan supervised low-power tests and subsequent full-system qualification.

## 7. Remaining hardware tests

The manufactured PCB, ADC accuracy, MOSFET safe operating area, analogue-loop
stability, over-temperature shutdown response, display refresh rate and
sustained 500 W operation have not been validated.
