# Software checks — 1 October 2026

## Passed

- ESP-IDF 6.0.2 build for the original ESP32, default Simulation configuration.
- Separate temporary-copy builds with TFT/encoder enabled in VoltageMonitor
  and SensorMonitor. Their verification flags were enabled for compilation only;
  this is not evidence of a verified board. Do not flash those temporary binaries.
- Host tests compiled with C++17, warnings treated as errors, AddressSanitizer
  and UndefinedBehaviorSanitizer. All assertions passed.
- esptool image validation: ESP32 target, 4 MB flash header, valid checksum/hash.

Host tests cover all eight ADS7828 channel commands, DAC frame fields,
byte reversal, divider/current/Pt1000 calculations, CC/CV convergence on the
simple simulated plant, every protection/cutoff decision, menu edits/cancellation,
waiting for settings acknowledgement, running-state edit lock and encoder bounce.

Final default application image: **182,240 bytes**, with **83% of its 1 MiB
application partition free**. This percentage is free program storage, not RAM
or proof that the TFT buffers will allocate on the physical device.

## Actual project left in this state

- `TestMode::Simulation`.
- TFT and encoder disabled.
- All hardware-verification flags false.
- Real load control rejected by the hardware abstraction.
- No physical DAC reference commands issued by the application.
- Original partition offsets retained; flash configuration corrected to 4 MB.
- No flashing, PCB/CAD edits or physical component changes.

## Not tested

Physical boot/programming, LCD wiring/timing/color/refresh/heap, encoder electrical
behaviour, I2C bus reliability, ADC calibration, Pt1000 self-heating, DAC analogue
outputs, actual CC/CV loop performance, hardware shutdown and power/thermal limits.
These require the confirmed board and supervised bench measurements.

The build produced non-fatal ESP-IDF configuration/debug-environment notices.
An ordinary shell initially lacked the ESP-IDF environment paths/version; the
build succeeded with the installed environment selected. Use the ESP-IDF
extension's terminal for normal `idf.py build` commands.

The pre-update source backup is at
`/private/tmp/esp32-firmware-before-update.SOMNus/`.
This is temporary storage, not a permanent version-control backup.
