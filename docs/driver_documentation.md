# Driver notes — final-board test firmware

This replaces the earlier internal-ADC, NTC and PWM placeholders.
Actual wiring must be approved using [the board checklist](board_bringup.md).

## Voltage: ADS7828, not an ESP32 ADC pin

`Ads7828` in `board_drivers.cpp` uses ESP-IDF's I2C master driver at 100 kHz:
GPIO33 SDA, GPIO32 SCL, address 0x48. Commands select single-ended channels with
the internal reference enabled. COM must have a valid ground reference.

The driver waits for reference startup, discards the first conversion after
channel selection and averages four subsequent readings. A failed transaction
or a sample near full scale is marked invalid. Filtering does not fix an unsafe
electrical input or excessive source impedance.

The configured reference is nominally 2500 mV. Conversion uses
`ADC_mV = raw * reference_mV / 4096`.
Voltage scaling uses `Vin = ADC_mV * (Rtop + Rbottom) / Rbottom`.
The **proposed**, not fitted/verified, divider is 12 kΩ top and 1 kΩ bottom.
[TI ADS7828 datasheet](https://www.ti.com/lit/ds/symlink/ads7828.pdf).

In VoltageMonitor, only the configured voltage channel is read. In SensorMonitor,
all eight channels are read. Every physical ADC input must still be electrically
safe even if firmware does not read it.

## Current: ACS37010-050B5 on CH0

The proposal adds 10 kΩ / 10 kΩ attenuation to the **ADC branch only**.
Software reconstructs VOUT and calculates:

`I_mA = (VOUT_mV - zero_mV) * 1,000,000 / sensitivity_uV_per_A`

Configured nominal zero is 2500 mV and sensitivity is 40000 µV/A.
Unlike the previous placeholder, this code does not pretend to measure VREF:
the uploaded PCB does not route it to a spare ADC channel. Measure and enter
the assembled board's zero-current offset, verify polarity and calibrate gain.
Negative current is retained, not silently reported as zero.
[Allegro ACS37010 datasheet](https://www.allegromicro.com/-/media/files/datasheets/acs37010-2-datasheet.pdf).

## Temperature: Pt1000, not NTC

NB-PTCO-006 is a platinum 1000 Ω sensor. The code uses the platinum RTD
resistance/temperature equation and the uploaded 1 kΩ pull-up arrangement.
It accepts -40 to +150 °C; open, short, clipped and out-of-range inputs are invalid.
A short bounded numerical search avoids a large temperature lookup table.
[TE part details](https://www.te.com/en/product-NB-PTCO-006.html).

The proposed assignment is CH2/3 = two heatsink sensors (display the hotter one);
CH4–7 = four external probes. Connector roles are **not yet confirmed**.
Check excitation/self-heating, lead resistance and actual supply voltage before
enabling conversions. The default 80 °C heatsink limit is for simulation and
is not a validated MOSFET or enclosure limit.

## DAC: MCP4922 bench support only

The class generates the 16-bit SPI frame for either 12-bit DAC channel.
A = Iref and B = Vref in the uploaded netlist. The configuration assumes the
uploaded 5 V references and unity DAC gain. SDI, SCK and CS require a verified
level interface with the 5 V DAC. LDAC is grounded in the uploaded design.

The class is deliberately **not called by the application**. It may be used
later for isolated reference measurements, with the power stage physically
disconnected and the bench-verification flag set. There is no analogue readback;
a successful function return proves transmission, not the output voltage.
DAC shutdown makes its output high impedance; it does not prove the MOSFETs are off.
[Microchip MCP4922 datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/22250A.pdf).

Do not substitute the simulation PI output for a DAC code. Real CC/CV selection,
reference offsets/gains, stability and an independent shutdown path are separate
hardware/firmware integration work.

## Display: 74HC595 → SSD1963

The final-board design uses a shift register. Software shifts a byte, latches
the outputs, then pulses TFT WR. It uses ordinary GPIO calls so it works on the
original ESP32, without the TTGO's ESP32-S3 LCD peripheral. The shared clock is
protected by a mutex and the inactive device is deselected.

A 400 × 240 RGB332 drawing buffer is scaled 2× to all **800 × 480** pixels.
A second buffer identifies changed spans, reducing repeated transfer work.
Both buffers together require 192,000 bytes of RAM; no PSRAM is assumed.
Allocation failure falls back to serial. The first frame updates the entire
panel. The screen task yields between rows so input sampling can continue.

This is a simple, relatively slow software bus; real refresh speed and available
heap still need measuring. Panel initialization reuses the earlier prototype's
timings, which must be confirmed on the actual 7-inch module.
The optional bit reversal matches QH→DB0 through QA→DB7.
[SSD1963 datasheet](https://www.lcdwiki.com/res/MRB7963/SSD1963_DS_V1.1.pdf),
[TI 74HC595 datasheet](https://www.ti.com/lit/ds/symlink/sn74hc595.pdf).

## Encoder and task separation

Quadrature A/B is polled every 2 ms with illegal transitions rejected.
The push switch has 30 ms debounce; a hold of 700 ms followed by release means Back.
Four transitions per detent is configurable. Pull-ups must be 3.3 V, not 5 V.

The input task collects events. The screen task owns menu/display work.
The application task owns settings, measurements and logging. Queues carry copies
between them; the slow screen transfer is not performed inside the control loop.
A software Stop request gets priority and clears old queued commands.
This is not a substitute for an emergency stop or hardware protection.

## CSV storage

SPIFFS stores `/spiffs/discharge_log.csv`, capped at 512 KiB. A new recording
overwrites the previous one. Download while stopped using `d` or the Logs menu;
save the text between the CSV begin/end markers.

Rows identify SIMULATED or MEASURED data. Unavailable measurements have empty
fields, not false zero readings. The legacy mount behaviour formats SPIFFS if
mounting fails, which can erase stored logs; save important results externally.
There is no SD card driver in this proposal because two SD GPIOs are reassigned.
