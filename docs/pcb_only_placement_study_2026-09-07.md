# PCB-only MOSFET placement study — 7 September 2026

## Purpose

This stage considers only the PCB outline, component placement cells and power
routing space. It does not create a combined PCB, MOSFET, heatsink, fan or
housing model.

The project brief requires continuous operation up to 500 W, with a maximum of
30 V and 30 A. The PCB must therefore be laid out for 30 A total and an ideal
5 A per MOSFET branch at the 500 W / 16.67 V corner.

“Two rows of three” means six devices on the same PCB:

```text
Row 1:  Q1 -------- Q2 -------- Q3
Row 2:  Q4 -------- Q5 -------- Q6
```

The rows are intended to run along the long direction of the eventual cooling
assembly. The MOSFETs still need electrically insulating interfaces when they
are eventually attached to one common heatsink.

## Important limitation

A PCB-only thermal simulation cannot determine MOSFET temperature if the
MOSFETs and their heat paths are omitted. At this stage the useful work is a
placement and routing-space study. The footprint cells below reserve enough
space to compare layouts without yet creating the full assembly geometry.

## Option A — retain the present 180.340 x 174.244 mm outline

The current board extends from X = 27.432 to 207.772 mm and Y = 17.272 to
191.516 mm. An evenly distributed 2 x 3 grid over this outline is:

| Device | Absolute KiCad X (mm) | Absolute KiCad Y (mm) |
|---|---:|---:|
| Q1 | 57.489 | 60.833 |
| Q2 | 117.602 | 60.833 |
| Q3 | 177.715 | 60.833 |
| Q4 | 57.489 | 147.955 |
| Q5 | 117.602 | 147.955 |
| Q6 | 177.715 | 147.955 |

This provides approximately 60.11 mm pitch along each row and 87.12 mm between
rows. With a 26.16 x 19.96 mm package envelope, the nominal body-to-body gaps
are approximately 33.95 mm along a row and 67.16 mm between rows.

This arrangement is geometrically spacious, but it conflicts with much of the
current control-circuit placement and does not align with the 280 x 120 mm
heatsink footprint. Retaining this outline would require a major component
placement and routing revision anyway.

## Option B — recommended one-PCB outline aligned to the cooling envelope

Use one approximately **260 x 110 mm PCB**, centred beneath the 280 x 120 mm
heatsink envelope. This does not mean using a second PCB. It reshapes the single
board so that the six placement cells match the already tested thermal layout.

Recommended coordinates relative to the new PCB origin are:

| Device | PCB X (mm) | PCB Y (mm) |
|---|---:|---:|
| Q1 | 28.28 | 31.00 |
| Q2 | 130.00 | 31.00 |
| Q3 | 231.72 | 31.00 |
| Q4 | 28.28 | 79.00 |
| Q5 | 130.00 | 79.00 |
| Q6 | 231.72 | 79.00 |

This gives:

- 101.72 mm centre pitch along each row;
- 48 mm centre spacing between rows;
- approximately 75.56 mm clear body-to-body distance along each row;
- approximately 28.04 mm clear body-to-body distance between rows;
- approximately 15.2 mm body clearance from each short PCB edge; and
- approximately 21.0 mm body clearance from each long PCB edge.

This is the preferred starting layout because it uses the full heatsink length,
keeps the device positions symmetric and preserves a compact 120 mm system
width. Whether it is mechanically possible depends on using a lay-flat or
bent-lead TO-264 mounting arrangement; the current upright footprint cannot
place two rows against one flat heatsink face.

## Placement-cell and routing rules for either option

1. Reserve at least a **35 x 30 mm placement cell** around each TO-264 body for
   the package, insulating pad, mounting screw/washer, lead bends and assembly
   tolerance. Confirm the final keepout from the selected shoulder washer and
   pad drawings.
2. Place each 5 mOhm shunt beside its corresponding MOSFET branch. Keep all six
   high-current branch paths equal in copper thickness, width and length.
3. Reserve at least 5 mm-wide copper paths for each nominal 5 A branch when
   using 2 oz copper and top/bottom stitched pours. This is a placement-space
   reservation, not final IPC-2152 validation.
4. Reserve a continuous aggregate-current corridor at least 20 mm wide on both
   copper layers, or space for a bolted copper busbar, for the 30 A path.
5. Put the input terminal and ACS37010 at the start of that aggregate-current
   corridor. Follow the sensor manufacturer's copper-pad and filled-via layout.
6. Use star-like distribution from the sensed input bus to the six branches and
   a symmetric star-like source return. Do not retain the present thin daisy
   chain.
7. Place one individual gate resistor beside every gate pin. Keep the six gate
   paths similar in length and away from the high-current copper.
8. Keep the ESP32, ADC, op-amps and voltage/current-sense signals outside the
   reserved hot power cells and away from fan-motor and high-current magnetic
   fields.
9. Reserve mounting holes and cable bend radius before routing. The current
   board has no defined 30 A load input/return terminals.

## Decision needed before editing KiCad

Choose one of these PCB-only constraints:

- **B1 — recommended:** one reshaped 260 x 110 mm PCB using the coordinates in
  Option B; or
- **A1 — retain outline:** keep 180.340 x 174.244 mm and use the coordinates in
  Option A.

Also confirm whether the TO-264 leads may be bent for lay-flat mounting. Without
that, a two-row arrangement on one flat common heatsink is not mechanically
possible, regardless of the 2D PCB spacing.
