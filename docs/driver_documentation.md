# Driver notes

These drivers use the interfaces in the final-board design. Check
[board bring-up](board_bringup.md) before enabling the hardware flags.

The current GPIO map follows the uploaded PCB, including its input-only
GPIO34 clock and GPIO35 DAC data assignments. TFT/DAC operation is blocked
at build time when requested, and the shared-bus initializer rejects this map
before touching any GPIO. Serial simulation is available. The previous
GPIO19/26/25 proposal is not selected in the code.

## Voltage: ADS7828

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
Raw conversion counts stay inside the ADC driver. Only converted measurements
and validity flags are passed to the UI, protection and logger. There is no
raw-ADC diagnostics menu.

## Current: ACS37010-050B5 on CH0

The proposal adds 10 kΩ / 10 kΩ attenuation to the **ADC branch only**.
Software reconstructs VOUT and calculates:

`I_mA = (VOUT_mV - zero_mV) * 1,000,000 / sensitivity_uV_per_A`

Configured nominal zero is 2500 mV and sensitivity is 40000 µV/A.
VREF is not routed to an ADC channel in the PCB design, so the calculation uses
a calibrated zero-current offset instead. Measure that offset on the assembled
board, then check polarity and gain. Negative current is kept in the reading.
[Allegro ACS37010 datasheet](https://www.allegromicro.com/-/media/files/datasheets/acs37010-2-datasheet.pdf).

## Temperature: Pt1000

NB-PTCO-006 is a platinum 1000 Ω sensor. The code uses the platinum RTD
resistance/temperature equation and the design's 1 kΩ pull-up arrangement.
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
A = Iref and B = Vref in the netlist. The configuration uses 5 V references
and unity DAC gain. SDI, SCK and CS need a verified logic-level interface
with the 5 V DAC. LDAC is grounded in the design.

The application does not call this class. Bench operation needs an approved,
output-capable pin map first; setting the bench flag with the current PCB map
fails compilation. Later isolated reference measurements also require the power
stage disconnected and the bench-verification flag set.
There is no analogue readback: the return value reports transmission status,
so output voltage must be checked with a meter. DAC shutdown leaves the output
high impedance and cannot be relied on to switch the MOSFETs off.
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

Refresh speed and available heap still need measuring on the board.
Panel initialization uses the prototype's
timings, which must be confirmed on the actual 7-inch module.
The optional bit reversal matches QH→DB0 through QA→DB7.
[SSD1963 datasheet](https://www.lcdwiki.com/res/MRB7963/SSD1963_DS_V1.1.pdf),
[TI 74HC595 datasheet](https://www.ti.com/lit/ds/symlink/sn74hc595.pdf).

## Encoder and task separation

Quadrature A/B is polled every 2 ms with illegal transitions rejected.
The push switch has 30 ms debounce. While stopped, a press/release selects the
highlighted row. Every submenu has a selectable Back row. While editing, turn
to change the value, press to choose Save or Back/cancel, then turn and press
to select the action. There is no long-press navigation. While preparing/running,
pressing requests Stop immediately
after debounce and suppresses the release event.
During preparation/running, the TFT footer and serial status show
`STOP BEFORE EDITING` and `STOP BEFORE DOWNLOAD` as reminders. They do not
replace protection messages or change the encoder's immediate Stop action.
Four transitions per detent is configurable. Pull-ups must be 3.3 V, not 5 V.

The input task collects events. The screen task owns menu/display work.
The application task owns settings and measurements; a separate logging task
owns the CSV file. Queues carry copies between them; the slow screen transfer
is not performed inside the control loop.
A software Stop request gets priority and clears old queued commands.
This is not a substitute for an emergency stop or hardware protection.

## CSV storage

SPIFFS stores `/spiffs/discharge_log.csv`, capped at 512 KiB. A new recording
overwrites the previous one. Download while stopped using `d` or the Logs menu;
save the text between the CSV begin/end markers.

Rows identify SIMULATED or MEASURED data. Unavailable measurements have empty
fields. The current mount configuration formats SPIFFS if
mounting fails, which can erase stored logs; save important results externally.
There is no SD card driver. GPIO25/26 are no longer reassigned to the DAC.
