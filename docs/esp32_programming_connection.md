# Connecting and programming the ESP32

## Recommended software

Use VS Code with Espressif's official **ESP-IDF** extension. The extension builds,
flashes and monitors this existing CMake project without converting it to Arduino.

PlatformIO is possible because it supports `framework = espidf`, but it should be
treated as an alternative toolchain, not used at the same time as the ESP-IDF
extension. Different toolchains can select different ESP-IDF versions and produce
different build folders. The official ESP-IDF extension is the simplest choice for
this project.

## Parts needed for the final PCB connection

The ESP32-WROOM-32E is a module rather than a USB development board. The final PCB
therefore needs all of the following:

- A regulated 3.3 V supply capable of at least 500 mA.
- Local 10 uF or 22 uF bulk capacitance and 0.1 uF bypass capacitance.
- A USB connector and a USB data cable.
- The MCP2200 USB-UART IC, its 12 MHz crystal and the crystal load components.
- A 0.22 uF to 0.47 uF ceramic capacitor at the MCP2200 VUSB pin.
- USB ESD protection. If USB-C is used, also add the required CC1 and CC2 device resistors.
- A RESET button that momentarily connects ESP32 EN to ground.
- A BOOT button that momentarily connects ESP32 GPIO0 to ground.
- Pull-ups for EN and GPIO0, following the Espressif reference circuit.
- Accessible UART, EN, GPIO0, 3.3 V and GND test pads or a programming header.

The ESP32-WROOM-32E already contains its main 40 MHz crystal and flash memory. The
12 MHz crystal in the component list is only for the MCP2200.

## UART wiring

Use 3.3 V UART logic. Do not power the MCP2200 UART side at 5 V when it is directly
connected to the ESP32.

| MCP2200 signal | ESP32-WROOM-32E signal | Purpose |
| --- | --- | --- |
| TX | RXD0 / GPIO3 | Computer sends data to ESP32 |
| RX | TXD0 / GPIO1 | ESP32 sends logs to computer |
| GND | GND | Common reference |
| 3.3 V logic supply | 3V3 | Correct UART logic level |

TX and RX are crossed. The USB-UART connection is used both for flashing and for
the serial controls and CSV download already implemented in the firmware.

## Manual flashing with the MCP2200

The MCP2200 exposes RTS/CTS flow-control signals but no separate DTR output in its
standard pinout. The simplest reliable first PCB therefore uses manual BOOT and
RESET buttons instead of an automatic programming circuit.

1. Connect the PCB to the computer by USB.
2. In VS Code, run `ESP-IDF: Select Port to Use` and select the MCP2200 serial port.
3. Run `ESP-IDF: Flash (UART) your Project`.
4. When the terminal shows `Connecting`, hold BOOT, press and release RESET, then release BOOT.
5. When flashing finishes, press and release RESET once to run the program.
6. Start `ESP-IDF: Monitor your Device` to view the output.

For a first test, keep `kTestMode = TestMode::Simulation` in
`main/project_config.hpp`, connect only electrically checked logic/USB power,
and physically isolate the power MOSFET stage. A software flag is not a hardware
shutdown. See [the board checks](board_bringup.md) before powering the uploaded PCB.

## Current final-board configuration

The project is built for the original `esp32` target, not `esp32s3`, using
ESP-IDF 6.0.2 and 4 MB flash for the N4 module. No Arduino conversion is required.
The uploaded design identifies ADS7828 acquisition, MCP4922 references,
Pt1000 probes and a 74HC595-to-SSD1963 display bus.

Before enabling any hardware flags, confirm:

- That the uploaded PCB is the actual printed revision.
- The corrected pin map and individual TFT jumper connections.
- Safe ADC divider/current input levels and COM/reference wiring.
- Encoder 3.3 V pull-ups and Pt1000 connector roles/calibration.
- DAC logic levels and safe inactive CS during reset.
- Access to BOOT/EN and the MCP2200's required reset circuitry.
- A physical default-OFF shutdown method before any future load test.

The application now supports simulation or measurement-only recording. Its PI
controller remains simulation-only; no physical PWM or DAC load demand is sent.
The proposed rework and staged test instructions are in [board_bringup.md](board_bringup.md).
