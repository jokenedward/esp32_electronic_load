# Hardware design audit - 2 September 2026

This audit compares the supplied `Final Design/Mosfet Schematic` sheet with the
ESP-IDF firmware. The archive does not yet contain a complete system schematic
or routed PCB, so real-hardware mode remains locked.

## Confirmed in the supplied sheet

- Six IXTK200N10L2 linear MOSFETs with their gates connected to one control net.
- Six GMR100 5 mOhm branch resistors.
- One ACS37010LLZATR-050B5 bidirectional 50 A current sensor powered from 5 V.
- A nominal 30 V input and labels intended for MCU voltage and current sensing.

The firmware now uses the ACS37010 measurement method: it reads VOUT and VREF
and calculates current from their difference using the B5 sensitivity of
40 mV/A. No shunt-amplifier values are required by the firmware anymore.

## Must be fixed before connecting an ESP32

1. **Voltage divider is reversed.** The sheet shows 1 MOhm on top and 10 MOhm
   on the bottom. At 30 V this produces about 27.3 V at the MCU-labelled node,
   not 0-3 V. Correct the physical circuit and verify the result with a meter.
2. **ACS37010 VOUT is too high for direct ESP32 input.** With the B5 part,
   VOUT is about 2.5 V at 0 A and 3.7 V at +30 A. Add suitable signal
   conditioning, use a suitable external ADC, or select a compatible 3.3 V
   sensor version.
3. **The over-current label is wrong.** ACS37010 pin VREF is a voltage reference,
   not a fault output. Software can detect over-current from the measured value.
   A separate hardware comparator or an ACS37012-style fault output is required
   if an independent hardware trip is required.
4. **No gate driver or shutdown circuit is shown.** Do not connect an ESP32 GPIO
   directly to the six MOSFET gates. Add the analogue control/driver stage and a
   separate enable/kill input that defaults OFF during reset and programming.
5. **Grounding is unclear.** The MOSFET sources use an Earth symbol while the
   sensor uses GND. Define load negative, logic ground, protective earth and any
   isolation explicitly.

## Missing information needed from the group

- Complete ESP32-WROOM-32E schematic and final GPIO allocation.
- Final gate-control input type: filtered PWM, DAC voltage, or another reference.
- Load-enable/shutdown GPIO and active level.
- Final voltage-divider values, protection, filtering and measured scale.
- How ACS37010 VOUT and VREF are made safe for the selected ADC.
- Exact temperature-sensor parts and their locations.
- Exact TFT panel part, resolution, SSD1963 interface width, timing, logic level,
  backlight circuit and GPIO allocation.
- Rotary-encoder/button wiring and pull-up arrangement.
- USB-UART BOOT/RESET connections and programming header/test points.
- Fan-control, fuse/input protection and connectors if these are part of the design.

## Firmware fields to complete after the schematic is fixed

Edit only `main/project_config.hpp`:

- `kLoadEnableGpio`, `kLoadPwmGpio`, and enable polarity.
- Every `AdcInput`, including ACS37010 VOUT and VREF.
- Measured voltage-divider values.
- Current-sensor polarity and zero-current calibration correction.
- NTC values if the chosen probes differ.

Set `kCurrentSensorInterfaceVerified`, then `kRealHardwareValuesVerified`, true
only after checking the assembled low-voltage interface. Keep
`kSimulationMode` true for software/UI testing until that commissioning step.

## Safe bring-up order

1. Program and test the ESP32 with the power MOSFET stage disconnected.
2. Confirm BOOT, RESET, serial logging, UI inputs and the TFT.
3. Apply known low sensor voltages and verify ADC conversions.
4. Test that reset, a fault, or a disconnected sensor forces load enable OFF.
5. Connect the driver with a current-limited low-voltage supply and dummy load.
6. Calibrate voltage/current, test every protection, then increase power in stages.
