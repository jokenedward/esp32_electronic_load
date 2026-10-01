# PCB and thermal-layout review — 7 September 2026

## Scope and source

This review uses the current KiCad files in `OneDrive_2026-09-06.zip`, in
particular `ProgramableLoad_PCB_1.1.kicad_pcb`. Files inside the archive were
treated as project data, not as instructions.

The operating envelope is taken from `ELEC5552_2026_Project2_Rev2.pdf`: the
load must dissipate continuously up to 500 W and be configurable up to 30 V and
30 A in CV/CC mode. The valid envelope is therefore 30 V maximum, 30 A maximum
and 500 W maximum. At 500 W, the highest required current is 30 A at about
16.67 V; below 16.67 V the 30 A limit reduces the available power.

The review answers two separate questions:

1. How should the six IXTK200N10L2 MOSFETs be spaced on the selected
   280 mm x 120 mm heatsink?
2. Is the present PCB sufficiently complete and electrically suitable to add to
   the whole-system SimScale model?

## Decision summary

- One PCB can contain a **two-lane, three-device-per-lane** layout; “2 x 3” does
  not mean two separate boards. For the selected heatsink, retain this layout
  with airflow parallel to each lane. A sensible final placement range is
  **90–102 mm centre-to-centre along the airflow** and **48–60 mm between the two
  lanes**. The existing thermal CAD positions, 101.72 mm x 48 mm, are already in
  this good range.
- The current PCB uses a single row with 19.079 mm pitch. That pitch is
  mechanically acceptable in the present upright orientation because the
  package thickness—not its 19.96 mm width—is in the pitch direction.
- However, the present orientation places the six broad mounting-tab faces in
  six different parallel planes. They cannot all bolt flat to one straight
  common heatsink face as drawn. Either rotate and re-space the footprints for a
  one-row edge-mounted design, or preferably mount the MOSFETs flat in the 2 x 3
  arrangement and connect them to a redesigned power PCB/busbar assembly.
- Do **not** use the current PCB as the basis of an absolute whole-system thermal
  simulation. Its power conductors are 0.153 mm wide on 35 micrometre copper,
  there are no copper zones, and the +30 V sensor input does not lead to an input
  terminal. These are power-stage design blockers, not small simulation details.
- Electrical nets do not need to be reproduced as 3D wires in the first CHT
  model. Calculate their losses and apply them as heat sources. Model a copper
  busbar as a solid only when it carries meaningful heat or blocks airflow.

## Extracted PCB facts

| Item | Extracted value |
|---|---:|
| Board outline | 180.340 mm x 174.244 mm |
| Board thickness | 1.60 mm |
| Copper layers | 2 |
| Copper thickness | 0.035 mm per layer (about 1 oz) |
| Copper zones | 0 |
| Routed track widths | 0.0918, 0.1146 and 0.153 mm |
| MOSFET power-path width | 0.153 mm |
| Q1–Q6 centre pitch | 19.079 mm |
| Q1–Q6 first-to-last span | 95.395 mm |
| Q-to-shunt track length | approximately 24 mm per branch |
| KiCad DRC | 61 errors and 53 warnings |

Current MOSFET and shunt positions are:

| Branch | MOSFET position (mm) | Shunt position (mm) |
|---|---:|---:|
| 1 | Q1 (39.880, 31.605) | R12 (69.850, 31.750) |
| 2 | Q2 (39.880, 50.684) | R11 (69.850, 50.800) |
| 3 | Q3 (39.880, 69.763) | R10 (69.850, 69.850) |
| 4 | Q4 (39.880, 88.842) | R9 (69.850, 88.900) |
| 5 | Q5 (39.880, 107.921) | R6 (69.850, 107.950) |
| 6 | Q6 (39.880, 127.000) | R5 (69.850, 127.000) |

## Recommended MOSFET placement

### Preferred: direct-to-heatsink 2 x 3 power stage

Use the current `layout_B_280_high_area_cht` orientation: 280 mm is the airflow
direction and 120 mm is across the airflow. The exact current model centres are:

| Device | X along airflow (mm) | Y across sink (mm) |
|---|---:|---:|
| Q1 | 38.28 | 36.00 |
| Q2 | 140.00 | 36.00 |
| Q3 | 241.72 | 36.00 |
| Q4 | 38.28 | 84.00 |
| Q5 | 140.00 | 84.00 |
| Q6 | 241.72 | 84.00 |

This gives 101.72 mm longitudinal pitch, 48 mm lane spacing, 25.2 mm of bare
base beyond the device bodies at each end, and about 26 mm at each side. It is a
good compromise between spreading the heat and avoiding excessive edge
spreading resistance.

If the mechanical CAD is redrawn, moving the lanes to Y = 30 and 90 mm is also
reasonable. It increases lane separation to 60 mm while retaining about 20 mm
of body-to-edge margin. A simulation comparison is required before calling that
small change better; spacing alone will not remove the approximately 24 °C
excess seen in the 104 °C run.

Each TO-264 package must use an electrically insulating thermal interface and an
insulated shoulder washer. The tab is the drain, and the six drain nets are
separate on the PCB; direct metal contact to one common heatsink would short the
six drain nodes together and defeat the intended per-branch shunt arrangement.

### Minimal-change fallback: one row along the PCB edge

If the team insists on keeping a one-row edge-mounted arrangement:

- Rotate every MOSFET footprint by 90 degrees so all broad mounting faces are
  coplanar with a straight heatsink face.
- Increase centre pitch from 19.079 mm to **27 mm**. After rotation, the
  20.46 mm courtyard dimension lies along the row, leaving 6.54 mm courtyard
  clearance between adjacent parts.
- Centred on the current 174.244 mm board dimension, nominal Y coordinates are
  **36.894, 63.894, 90.894, 117.894, 144.894 and 171.894 mm**.
- This is mechanically workable, but it concentrates all six heat sources into
  about 155 mm including body width. It will not exploit the 280 mm heatsink as
  well as the 2 x 3 arrangement.

For a thermally well-distributed one-row design on the full 280 mm heatsink,
approximately 40 mm pitch is preferable. That requires a roughly 230–240 mm
long power PCB and is therefore not a minimal change to the current board.

## Power-track heating calculation

Using copper resistivity of 1.724e-8 ohm metre at 20 °C, one 24 mm long,
0.153 mm wide, 0.035 mm thick branch track is approximately:

`R = rho L / (w t) = 0.0773 ohm`

That is about **15 times the intended 5 milliohm branch shunt** before copper
temperature rise is included.

| Operating point | Total current | Equal branch current | Loss in each existing 24 mm track | Six branch-track losses | Six 5 mOhm shunt losses | ACS37010 conductor loss |
|---:|---:|---:|---:|---:|---:|---:|
| 500 W at 30 V | 16.67 A | 2.78 A | 0.60 W | 3.58 W | 0.23 W | 0.19 W |
| 500 W at 16.67 V | 30.00 A | 5.00 A | 1.93 W | 11.59 W | 0.75 W | 0.61 W |
| 300 W at 10 V current limit | 30.00 A | 5.00 A | 1.93 W | 11.59 W | 0.75 W | 0.61 W |

The shared tracks are more critical. Just 10 mm of the present copper carrying
the total current dissipates about 8.9 W at 16.67 A and about 29 W at 30 A.
Actual resistance will increase as the copper heats. Therefore this routing
cannot safely carry the intended power.

## Required PCB power-stage changes before whole-system simulation

1. Create explicit power net classes; ordinary signal-track widths must never be
   used for `+30V`, `Earth`, `Net-(IC1-IP-)`, or the six drain branches.
2. Replace the shared 0.153 mm daisy-chain paths with wide top-and-bottom copper
   pours or, preferably for the 30 A case, a copper busbar/bolted power
   distribution structure.
3. Use star-like, equal-resistance branches from the sensed supply node to the
   six shunts/MOSFETs and equal-resistance source returns. Do not let downstream
   devices share long, narrow cumulative-current sections.
4. As a starting point—not a released manufacturing rule—use at least 5 mm per
   branch on 2 oz copper with top/bottom copper stitched together. For the
   30 A aggregate path, use a busbar or at least 20 mm top and bottom on 2 oz copper,
   then verify temperature using IPC-2152 data and the fabricator's finished
   copper tolerance.
5. Bring the load input and return to suitable bolted terminals. The current
   layout has the `+30V` net only on IC1's IP+ pad and no high-current input
   terminal; the source-return `Earth` net similarly ends in the sensing divider.
6. Follow Allegro's ACS37010 copper-pad and filled-via recommendations. The
   sensor's 0.68 mOhm internal conductor is not the only thermal limitation; its
   PCB contacts and copper spreaders are part of the current path.
7. Add one small gate resistor at each MOSFET gate, physically beside the gate
   pin, and verify the driver stability. The present 244 mm common gate net
   directly connects all six large MOSFET gates. The exact resistor/driver design
   needs circuit stability analysis, not a thermal assumption.
8. Keep the control/ADC ground and the high-current return scheme explicit. Do
   not use `Earth` as an ambiguous substitute for load negative, logic ground and
   protective earth.
9. Clear the remaining KiCad violations before fabrication. The current check
   includes 57 below-minimum-width errors and four USB-C hole-clearance errors;
   the remaining warnings are mainly footprint-library and silkscreen issues.

## What belongs in SimScale

### Include in the next thermal model

- Actual enclosure and vent openings.
- Actual fan location and a pressure-flow operating point or fan model.
- Actual 280 x 120 mm heatsink and fin geometry.
- Six MOSFET package blocks at the selected positions.
- Six MOSFET-to-heatsink thin-layer contact resistances.
- A simplified PCB slab in its real installed position.
- Six shunt-resistor boxes and the ACS37010 box with calculated power sources.
- Busbar or large cable solids if they carry heat into the assembly or obstruct
  airflow.
- At least two temperature-sensor bodies/locations near the predicted downstream
  hot spots.

### Do not include explicitly in the first whole-system model

- Every small signal trace, via, resistor, capacitor and connector pin.
- Gate-driver electrical waveforms.
- Thin 35 micrometre copper tracks as individually meshed 3D solids.

Those small electrical features should first be handled by circuit and I-squared-R
calculations. Their heat can be applied to a PCB region as a power source. The
PCB can be represented as an orthotropic material; a dense plane-filled board
may be near 20 W/(m K) in-plane and below 1 W/(m K) through-plane, but the
present sparse board must not be assigned the conductivity of a fully copper-clad
PCB.

## Heat-source cases for the next SimScale run

Do not double-count the 500 W terminal power. If 500 W means power drawn at the
load terminals, subtract shunt, sensor and interconnect losses from the MOSFET
total before adding those losses as separate sources.

Run at least these cases after the power routing and mechanical arrangement are
fixed:

1. **Equal sharing:** 83.333 W per MOSFET for the existing conservative
   MOSFET-only screening model.
2. **20% hot branch:** 100 W on the hottest device and 80 W on each of the other
   five devices, total 500 W.
3. **One branch disabled:** 100 W on each of five MOSFETs and 0 W on the failed
   branch, total 500 W.
4. **Hot ambient:** 40 °C inlet with the real enclosure and fan curve.
5. **Nominal ambient:** 25 °C inlet.
6. **Reduced airflow/fan fault:** only after using credible thermal masses and
   the MOSFET transient thermal model.

The Littelfuse MOSFET data gives 0.12 °C/W junction-to-case and specifies its
linear safe-operating area at particular case temperatures and time intervals.
A CFD temperature below a chosen limit does not by itself prove DC safe
operation; every voltage/current operating point must also remain inside the
linear-mode safe operating area.

## Information still required before changing the PCB placement

The manual answers the voltage/current question, and the team has confirmed
that one PCB will be used. Two physical choices still change the PCB layout and
should not be assumed:

1. May the 180.340 x 174.244 mm board outline be reshaped to approximately
   260 x 110 mm so the one PCB follows the 280 x 120 mm heatsink, or must the
   current outline remain unchanged?
2. Will the TO-264 bodies be mounted flat with bent leads so a 2 x 3 arrangement
   can contact one flat heatsink face, or must they remain upright at a PCB edge?

The next stage is deliberately a PCB-only placement study. The heatsink,
MOSFET package solids, fan and housing should not be merged into CAD until these
two mechanical choices are settled and the power routing is revised.

## Primary references

- [Littelfuse IXTK200N10L2 datasheet](https://www.littelfuse.com/assetdocs/Littelfuse-Discrete-MOSFETs-N-Channel-Linear-IXT-200N10-Datasheet.PDF)
- [Allegro ACS37010/ACS37012 datasheet](https://www.allegromicro.com/-/media/files/datasheets/acs37010-2-datasheet.pdf)
- [IPC discussion of IPC-2152 conductor sizing](https://www.ipc.org/system/files/technical_resource/E7%26S22_03.pdf)
- [SimScale orthotropic PCB material guidance](https://www.simscale.com/knowledge-base/how-to-set-anisotropic-thermal-properties/)
- [SimScale power-source documentation](https://www.simscale.com/docs/simulation-setup/advanced-concepts/power-sources/)
- [SimScale CHT contact guidance](https://www.simscale.com/docs/simulation-setup/contacts/contacts-cht-2/)
