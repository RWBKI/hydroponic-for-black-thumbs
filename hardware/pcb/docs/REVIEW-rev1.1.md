# Design review: controller board rev 1.1 (2-layer ESP32 hydroponic controller, never fabricated)

> **Note from the project, added after the review.** Finding 37 was fixed in the firmware afterwards and is not covered by this review: the PCB build now accepts a pH input only between -0.6 V and +0.6 V (`firmware/include/board.h`). Independently of that window, the dosing logic stops and raises a fault when the computed pH is outside 4 to 9. The other open findings stand as written.

> This review was produced by an AI reviewer (Claude) that did not take part in the rev 1.1 edits and was asked to find what they got wrong. It is not a review by a human electronics engineer. Nothing in it has been measured on hardware: every distance is computed from the saved board file, every electrical figure is a datasheet value or a calculation, and estimates are labelled as such.

Review date: 2026-11-15. Reviewed from the files only: the rev 1.1 KiCad project (schematic, board, project file, custom rules), the rev 1.0 project for comparison, the rev 1.1 Gerber zip, BOM, placement file, PDFs and images, the firmware pin map and hardware source file, and the rev 1.0 review. Nothing under review was modified; all work was done on copies. Finding numbers 1 to 23 are those of the rev 1.0 review; new findings start at 24. Findings 24, 26, 30 and 35 were raised on an earlier state of the rev 1.1 files and were fixed before this report was finalised; they are listed in their own section, and everything else in this report describes the files as they now are.

## Verdict

I would order rev 1.1 as it is, as a first prototype. I found no Blocker and no Major: the schematic and the board agree pin for pin (434 pads, 0 differences), the only pins of existing parts that changed net are the ten that were meant to change, the isolated island is separated from the rest of the board by at least 1.00 mm of bare laminate on both layers with U8 as the only bridge, the guard closes around the high-impedance node on both layers, the saved board passes DRC as saved and after a refill, and the Gerbers in the zip are byte-identical (dates aside) to a fresh export of the saved board. All three Major findings of the rev 1.0 review are resolved in the files, and the two layout and part points I raised on an earlier state of rev 1.1 (the isolated-side bypass capacitor and the regulator's input rating) have been fixed.

What remains is not visible in files: rev 1.1 puts a switching isolated supply (125 MHz oscillator, 600 kHz PWM) 10 mm from a high-impedance input that still has no RF filter. Whether that disturbs the pH reading can only be measured.

Test first on the bench, in this order:

1. **Isolated supply.** `+3V3_ISO` should be about 3.3 V (1.23 V x 26.9k / 10k = 3.31 V). Look at its ripple and at `PH_SIG` with an oscilloscope (finding 25).
2. **I2C through the isolator.** A bus scan must show 0x48 and 0x49. Measure the low level on `SDA_ISO` / `SCL_ISO`: 0.6 to 0.9 V is by design, not a fault (finding 28).
3. **Isolation.** Unpowered, `GND` to `GND_ISO` must read open circuit. Then, with the probe in a buffer and the EC electrodes in the same liquid, the pH reading must not move when the EC pins are driven low, high or left floating. This is the test for finding 2.
4. **pH offset and noise.** With the BNC shorted, then with a known voltage through 100 MΩ or more, compare readings with Wi-Fi idle and transmitting (findings 1, 10, 25). Also note what the firmware reports with the probe unplugged (finding 37).
5. **3.3 V rail.** Level, and step response when Wi-Fi transmits, on USB power only (findings 3 and 36).

Open findings by severity: **0 Blocker, 0 Major, 4 Minor, 12 Note.**

## Status of the three Major findings of the rev 1.0 review

| # | Rev 1.0 finding | Status in rev 1.1 | Basis |
|---|---|---|---|
| 1 | Guard does not enclose the high-impedance node | **Resolved.** Residual Note (see table). | On F.Cu and on B.Cu the `PH_SIG` copper has closed holes that contain all `PH_IN` / `PH_IN_F` copper and no other net. Each fill is one piece, tied to the net solidly at U6 pads 6 and 7 and by vias at (65.2, 56.5) and (67.6, 56.4). Node to guard 0.30 mm everywhere. Narrowest guard section towards other copper: 0.44 mm, between J3 pad 1 and the BNC shield pin at (62.2, 57.1 to 57.54), same on both layers. Nearest non-guard copper to the node: `BNC_SH` (J3 pad 2) at 0.94 mm, which is the connector's own pin spacing; `GND_ISO` at 1.33 mm (was ground at 0.30 mm). |
| 2 | pH and EC share liquid and ground | **Resolved on the board; confirm on the bench (test 3).** | Netlist: U8 is the only component with pins on both sides; J3 is the only connector on an island net. Copper: minimum island to non-island distance 1.0003 mm on F.Cu and on B.Cu (zone to zone), 3.66 mm under U8. No non-island track, via or netted pad lies inside the island outline on either layer; every stitching via inside it is on `GND_ISO`. New risks that come with the isolator are findings 25, 27 and 28. |
| 3 | AMS1117 below its minimum input voltage | **Resolved.** Residual Note 36. | U3 is LDL1117S33R (C435835) in schematic, board and BOM; copper around U3 is unchanged from rev 1.0. Pin-out 1 GND, 2 VOUT, 3 VIN with the tab on VOUT matches the SOT-223 "tab = pin 2" footprint and the nets (`GND`, `+3V3`, `V_LDO`). Dropout 0.35 V typ / 0.60 V max at 1.2 A over temperature. Headroom in the worst case I can construct (4.75 V at the USB connector, 0.6 V across D5): 0.85 V at a nominal output, 0.75 V with the output at its +3 % limit; more on 12 V power. Input 2.6 to 18 V, 20 V absolute maximum, so the unclamped `V_LDO` node is no longer a concern. The control loop is designed for a ceramic output capacitor (4.7 µF suggested, 1 µF minimum at the input); the board has 22 µF + 100 nF at the output and 10 µF at the input. This also closes finding 15. |

Other rev 1.0 findings touched by rev 1.1: finding 11 (saved board failing its own rules, stale Gerbers) is resolved. Findings 4, 10, 14, 18 and 20 are still open and are in the table below. Findings 5 to 9, 12, 13, 16, 17, 19, 22 and 23 concern parts of the board that rev 1.1 did not change (verified: no net, footprint position, pad or track differs there) and were not reviewed again.

## How the review was done

- Netlists of both revisions exported with `kicad-cli sch export netlist` and compared pin by pin with each other and with the board pads (`pcbnew` Python module, KiCad 7.0.11).
- All copper (pads, tracks, vias, filled zones as saved) converted to polygons per layer and net; distances, enclosure and connectivity computed with `shapely`, independently of the DRC engine. Plots of the area were inspected.
- DRC run with `pcbnew.WriteDRCReport` on a copy of the saved board, again after refilling all zones, and twice more as a control: with the custom rules tightened (to prove they are loaded) and with the rules file removed. A further control run checked which of the two custom rules wins where both apply.
- Gerbers and drill files re-exported with `kicad-cli` and compared with the zip. Schematic PDF re-exported and compared as text.
- Datasheets opened: [ADM3260](https://www.analog.com/media/en/technical-documentation/data-sheets/ADM3260.pdf), [RS-20 package drawing](https://wcm-cce.cldnet.analog.com/media/en/package-pcb-resources/package/pkg_pdf/ssoprs/rs_20.pdf), [AN-0971](https://www.analog.com/media/en/technical-documentation/application-notes/AN-0971.pdf), [LDL1117](https://www.st.com/resource/en/datasheet/ldl1117.pdf), [ADS1115](https://www.ti.com/lit/ds/symlink/ads1115.pdf), and the LCSC pages for [C435835](https://www.lcsc.com/product-detail/C435835.html), [C208558](https://www.lcsc.com/product-detail/C208558.html), [C25954](https://www.lcsc.com/product-detail/C25954.html) and [C1525](https://www.lcsc.com/product-detail/C1525.html).
- Copper weight is not stated in the files; 1 oz (35 µm) is assumed for resistance figures.

## Open findings

Coordinates are board coordinates in mm (the placement file negates Y).

| # | Severity | What | Evidence | Consequence | Fix |
|---|---|---|---|---|---|
| 4 | Minor (carried over, improved) | The ESP32 supply is shorter but still a 0.25 mm track that passes the ADC supply tap, and it now also feeds the isolator. | `+3V3` is still in the Default net class. Routed path U3 pin 2 to U1 pin 2, same method on both revisions: 71.2 mm in rev 1.0 (the earlier review counted 77 mm from a different start point), 51.4 mm in rev 1.1 (26.2 mm F.Cu + 25.1 mm B.Cu, 2 vias, 0.25 mm throughout). Calculated 0.10 Ω (was 0.14 Ω). It still passes the U5 pin 8 tap at 15.1 mm, and C21 (U5's bypass) now sits in the trunk at 20.6 mm instead of on a side branch. The first 25.0 mm is now shared with U8 (`VIN` and `VDDP`), which then has its own 22 mm spur (0.25 / 0.30 mm) from the via at (89.47, 49.06). Branches to J16, J17 and the sensor pull-ups leave earlier than before (16.3 mm instead of 26.3 mm). | About 38 mV drop at the 379 mA transmit peak and 51 mV at 0.5 A (was 53 and 70 mV). The step on the U5 supply is unchanged (about 11 to 15 mV). U8 sees the same steps; its line regulation makes that harmless. Better than rev 1.0, not the fix that finding 4 asked for. | As in rev 1.0: route U3 to U1 pin 2 directly at 0.8 to 1.0 mm, branch the ADC and U8 off at the regulator, add `+3V3` to a power net class. |
| 10 | Minor (carried over, now more relevant) | Still no capacitor at the buffer input and no series resistance between the BNC shell and the ADC. | `PH_IN_F` connects only R20 pad 2 and U6 pin 5. `BNC_SH` goes straight to U7 pin 4 (with C24 100 nF to `GND_ISO`). | Same as rev 1.0, with a new RF source on the island (finding 25). | 100 pF to 1 nF C0G from `PH_IN_F` to `PH_REF` inside the guard; 1 k in series with U7 AIN0. A leaded capacitor across U6 pins 5 and 4 is a possible bench rework. |
| 25 | Minor (new) | The switching isolated supply feeds the analog front end directly, on a 2-layer board, next to an unfiltered high-impedance input. | U8 `VISO` goes to U6, U7 and the R18 / R19 divider with nothing but the bypass capacitors in between (C34 and C35 at the converter, 100 nF at the loads). Datasheet: 125 MHz oscillator, 600 kHz PWM, 50 mV p-p ripple at 18 mA with the recommended capacitors. U8 centre is 9.9 mm from U6 pin 5 and 13.3 mm from J3 pad 1. No capacitance across the barrier. AN-0971 (written for a sister family at 180 MHz) treats 4 layers and barrier stitching capacitance as the baseline for emissions and notes that emissions fall sharply at light load; the load here is light (about 2.5 to 4 mA of 20 mA). | Possible offset or noise on the pH reading from RF pick-up on the probe input, and conducted ripple into U6. The reference divider is well filtered (C23) and the ADC averages, so this may turn out negligible. Unknown until measured (bench tests 1 and 4). | If the bench shows it: finding 10's capacitor, and 10 Ω + 10 µF (or a ferrite bead) between U8 `VISO` and the analog parts. Both need copper. |
| 37 | Minor (new, firmware) | The check that is meant to reject a disconnected or saturated pH input cannot trip on this board. | `firmware/src/hw.cpp`, `measure()`: a reading is accepted if it lies between -3.0 V and +3.2 V. On the PCB build the value is AIN0 - AIN1 = `BNC_SH` - `PH_SIG`, which is confined to about ±1.65 V even with the buffer at a rail, and to ±0.41 V for any real pH. The window was written for the module build, where the input is single-ended. | With the probe unplugged or the buffer saturated, the reading is still flagged valid. What happens next depends on code I was not given (`phFromVolts` and the dosing logic); if an out-of-range pH is clamped rather than rejected, the pH-down pump could be dosed on a meaningless value. A floating input can also sit inside any window, so this check can only ever catch part of the cases. | Use board-specific limits (for the PCB about ±0.6 V), and reject any computed pH outside 0 to 14 before it reaches the dosing logic. |
| 1 | Note (residual) | The guard is closed but lies under solder mask, and the node was not shortened. | No mask opening over the guard on either side (mask layers contain pads only). R20 is where it was: `PH_IN` 4.2 mm and `PH_IN_F` 2.4 mm of track. Guard strip between J3 pad 1 and J3 pad 2: 0.44 mm; under R20's body between its pads: 0.25 mm. | Leakage in and on the laminate is intercepted; a film on top of the mask is not. The nearest exposed conductor is now the shield pin, so such leakage loads the electrode (a slope error that calibration absorbs) instead of adding an offset from ground as in rev 1.0. | Optional: open the mask over the ring. Clean flux residue around J3 after hand soldering. |
| 14 | Note (carried over) | U5 `ALERT/RDY` is still tied to `GND`. | U5 pin 2 on `GND`. The new U7 has it left open, which is what the datasheet asks for. | None with the comparator disabled. | Leave open on the next revision. |
| 18 | Note (carried over, changed) | The BNC shell and its two posts are on the island; anything conductive that joins them to the main ground or to earth undoes finding 2's fix. H1 sits in the island corner. | J3 pad 2 (three pads) = `BNC_SH`. H1 is a non-plated 3.2 mm hole at (54, 44) with no copper; island pour starts 3.59 mm from its centre (1.99 mm from the hole edge), under mask. Nearest component pads: a BNC post at 5.42 mm, C35 at 5.96 mm, R51 at 6.04 mm from the centre. | A metal panel or enclosure touching the BNC body ties the reference electrode to whatever the panel is tied to. A screw head or washer up to 7 mm across at H1 stays off the pour by 0.09 mm; a wider washer or metal standoff rests on masked `GND_ISO` copper. | Readme: insulate the BNC from any conductive panel; use a nylon washer or screw at H1. |
| 20 | Note (carried over) | Footprints have no library link. | 152 "library ''" DRC warnings, one per footprint (was 140). | None for fabrication. | As rev 1.0. |
| 27 | Note (new) | The floating island has no defined discharge path to the main ground. | No component bridges the 1.0 mm moat except U8. | A static discharge to the BNC shell while the probe is out of the liquid charges the island until something breaks over, most likely the moat. Probably harmless; undefined. | Optional unpopulated footprints across the moat for a small high-voltage capacitor and a resistor of 100 MΩ or more. Note that capacitance across the barrier also couples EC excitation into the island, so keep it small. |
| 28 | Note (new) | The logic-low margin on the isolated I2C side is small by design. | ADM3260 side 1 (pins 18, 19) drives low to 0.6 to 0.9 V at 3 mA (0.6 to 0.85 V at 0.5 mA). ADS1115 accepts up to 0.3 x VDD as low: 0.99 V at 3.31 V. Worst-case margin about 90 mV, less if `VISO` is low; typical parts leave more. In the other direction the ADS1115 pulls to 0.4 V max, below the isolator's 0.5 V minimum input threshold. R53 / R54 2.2 k load the outputs with 1.1 to 1.4 mA, inside the specified 0.5 to 3 mA. | Within the datasheets. Ripple on `+3V3_ISO` eats into the margin (finding 25). | Bench test 2. No change needed if communication is clean at the bus speed used. |
| 29 | Note (new) | Tracks and a via lie inside the copper-free strip under U8; the barrier on the board is narrower than the package allows. | The rule area (67.0 to 71.45 x 40.4 to 49.0) excludes pour only. Inside it: the `GND` bus at x = 71.2 that joins pins 1, 5, 7, 8, 10; the `SDA_ISO` track at x = 67.1 and its via at (67.35, 48.55). Smallest copper gap across the strip: 3.66 mm on F.Cu, 3.70 mm on B.Cu, against 5.45 mm between the two pad rows. The datasheet quotes 5.1 mm creepage for the package. The rules file now carries the comment "functional isolation only, not a safety barrier". | None at the few volts present here. The board must not be described as providing safety isolation. | Optional: move the `GND` bus outside the pad row and the `SDA_ISO` via west of x = 67. Repeat the "functional isolation only" statement in the readme. |
| 31 | Note (new) | The island ground is cut into sub-areas by the B.Cu tracks; the ADC's corner still hangs on one narrow neck. | B.Cu `GND_ISO` is two separate pieces (west, and east of the `SCL_ISO` / `SDA_ISO` tracks at x = 74.5 / 75.3), joined only through F.Cu. Within the west piece, the 56 mm² area under U7 and C22 joins the rest through a single neck 0.26 to 0.30 mm wide near (72.7, 57.8). On F.Cu the island ground is four pieces. U7's ground pins now reach B.Cu through three vias, (65.7, 62.95), (69.5, 63.0) and (72.8, 64.3), and C22 through one at (71.1, 60.6), but all four land in that same B.Cu area, so the neck is still the only link between the ADC's ground and the ground of U6, the reference divider and U8. | No DC error: the ADC measures AIN0 - AIN1 and the currents are a milliamp or less. The added vias improve the local return of U7's bypass; they do not change how that corner is tied to the rest. | Widen the neck: shift the `+3V3_ISO` diagonal at (72.6 to 73.7, 57.6 to 58.75) by 0.5 mm, or bring `SCL_ISO` / `SDA_ISO` to U7 on F.Cu. |
| 32 | Note (new) | The new parts are tight against the top edge and each other. | Pad to board edge: C32 0.52 mm, C35 0.58 mm, C33 0.67 mm, C34 0.89 mm, U8 1.10 mm; U8 body 0.65 mm from the edge; new vias at (75.65, 40.8) and (63.0, 40.85) 0.50 and 0.55 mm (rule 0.40 mm). Rev 1.0 already had SW2 at 0.65 mm. Courtyard gaps of 0.02 to 0.07 mm around U8, C34, C35, R51, R52, C36 and U7 / C22; no overlap. The `GND_ISO` via at (63.0, 40.85) is 0.25 mm from C35 pad 1 (`+3V3_ISO`); the `+3V3_ISO` via at (59.6, 43.03) is 0.125 mm from R51 pad 1 (same net). U8 pins 19 and 20 are 1.2 mm from the BNC body outline. | Passes DRC. C32 stands perpendicular to the edge, the unfavourable orientation if boards are separated by V-score. Rework on U8 pins 18 to 20 is awkward once the BNC is fitted, and C34 (0402) is not a part to place by hand. Edge clearance requirements of the assembler were not checked. | Order as individually routed boards, or accept. |
| 33 | Note (new) | Cost and availability of U8. | LCSC page for C208558 on the review date: 46 in stock, about 8.4 USD each. For comparison C435835 (U3): about 50 000 in stock; C1525 (C34): several million. | U8 becomes the most expensive part on the board and may run out. | Check stock at the BOM step. |
| 34 | Note (new) | Cosmetic points. | The "pH PROBE" label is at (69.5, 65.7), below U7 and not next to the BNC. In `images/silkscreen-top.png` the value texts collide around U7 and U8 and one sits above the board edge near C35 (these are not on the Gerber silkscreen; DRC reports no silkscreen overlap). Five joints under 90° at vias or pads: (76.05, 45.30) 60°, (61.40, 47.00) 45°, (70.12, 57.91) 71°, (74.50, 62.10) 39°, (61.95, 41.30) 64°. In the schematic the isolated block is drawn 5 to 6 mm higher than in the earlier state of the file; the netlist is unaffected. | None electrically. | Tidy at the next edit. |
| 36 | Note (new) | The stability region of the new regulator was not read from its plots. | LDL1117 datasheet: loop designed for a ceramic output capacitor, 4.7 µF suggested, output capacitor to stay inside the region of the stability plots; no maximum capacitance and no minimum ESR is given in the text. I could not read the plots. On the board: C14 22 µF + C15 100 nF at U3, a further 22 µF + 100 nF at the module behind about 0.1 Ω of track, 10 µF at U8. | Probably fine: more ceramic capacitance than the suggested value is the usual direction of safety for this kind of regulator. Not confirmed. | Bench test 5: look for ringing on `+3V3` at a load step. If it rings, C14 can be changed to 10 µF or 4.7 µF on the same footprint. |

## Raised earlier in this review and since fixed

These four findings were made on an earlier state of the rev 1.1 files. The follow-up edits were compared with that state item by item: on the board they touch only C34, C35, R51, R52, the value of U3, the visibility of U8's reference, 22 removed and 34 added tracks and vias (all on `+3V3_ISO`, `GND_ISO` or `VSEL`), and nothing else; zones, drawings, outline and every other footprint are identical. All checks in this report were run again on the result.

| # | Was | What was changed | Verified |
|---|---|---|---|
| 24 | Minor: the isolated-side 100 nF of the converter had an 8 mm ground return; the 10 µF was 12 mm of track away | C34 is now an 0402 (C1525) at (63.8, 41.65), turned 90°, beside U8 pins 11 and 12. C35 moved to (61.0, 41.3). R51 and R52 re-placed, `VSEL` and the `VISO` / `VDDISO` link re-routed. | Copper from C34 pad 1 to pin 11: 0.45 mm; from pad 2 to pin 12: 0.45 mm (1.6 mm of 0.25 mm track each, centre to centre), against the datasheet's 2 mm total. C34 has its own `GND_ISO` via 0.25 mm from its ground pad. C35: 2.2 mm of copper from pin 12; its ground pad reaches pin 11 through its own via, 3.6 mm of B.Cu plane and C34's via. All island load current now leaves through C34 and C35 first. `VSEL` is 6.1 mm of track (was 5.7 mm), 0.20 mm or more from its neighbours. Footprint dimensions equal the standard KiCad 0402 pattern (pads 0.56 x 0.62 mm at ±0.48 mm). DRC clean. Side effects are in finding 32. |
| 26 | Minor: the AP2114H regulator used at that stage had a 6.5 V absolute maximum input on an unclamped node | U3 is now LDL1117S33R. | See finding 3 in the status table: 18 V operating, 20 V absolute maximum, lower dropout than the part it replaces, same pin-out and tab. Finding 3 stays closed. Residual: finding 36. |
| 30 | Note: the 0.8 mm rule overrode the 1.0 mm isolation rule for the two input nets | Rules reordered, isolation rule last. | Control run with a net placed in the Analog class 1.01 mm from main-side pour and the isolation rule set to 1.2 mm: flagged with the new order, not flagged with the old one. Both rules still exempt copper without a net (three unconnected IC pads); the nearest such pad of the main side is 4.72 mm from island copper. |
| 35 | Note: the first access to the isolated ADC after power-up could be refused | `firmware/src/hw.cpp` probes address 0x49 up to 25 times, 20 ms apart, before giving up. | Read in the source: at most about 0.5 s, then a log line either way. The result is latched at boot: if the probe fails, pH stays unavailable until the next reset, which is a reasonable choice. A separate point found in the same file is finding 37. |

## Checked and found correct

### A. Schematic against board, and rev 1.0 against rev 1.1

- Rev 1.1: 152 components, 434 pins, 109 nets (89 named + 20 intentionally unconnected pins). Board: 152 footprints, 434 numbered pads, 6 unnumbered mechanical pads. **0 net differences**, all 20 unconnected pins match, values and footprint names match (the only text difference, "SMF15A" against "SMF15A (TVS)" for D2, is also in rev 1.0).
- Rev 1.0 for reference: 140 components, 384 pins, 0 differences.
- New components: C31 to C36, R51 to R54, U7, U8. None removed. U3 (now LDL1117S33R, C435835) is the only existing component whose value or part number changed.
- Pins of existing parts whose net changed, complete list: C22.1 and R18.1 and U6.8 `+3V3` to `+3V3_ISO`; C22.2, C23.2, C24.2, R19.2, U6.4 `GND` to `GND_ISO`; U5.4 `BNC_SH` to `GND`; U5.5 `PH_SIG` to `GND`. All ten are intended. Nothing else changed.
- No existing footprint moved or rotated, no existing pad changed size or position, board outline identical.
- No single-pin named net.

### B. Isolation

- Components with pins on both sides: U8 only (island pins 11 to 14 and 16 to 20, main pins 1 to 5 and 7 to 10; pins 6 and 15 unconnected).
- Island nets reach one connector, J3. They reach no mounting hole, test point, switch or header.
- Minimum copper distance island to non-island (pads, tracks, vias, filled zones; unconnected pads excluded):

| Layer | Minimum | Where |
|---|---|---|
| F.Cu | 1.0003 mm | `GND_ISO` pour to `GND` pour at (82.04, 57.69) |
| B.Cu | 1.0003 mm | `GND_ISO` pour to `GND` pour at (81.93, 58.60) |
| F.Cu, tracks and pads | 1.0005 mm | `BNC_SH` track along y = 49.4 and R21 pad 1 to `GND` pour |
| B.Cu, tracks and vias | 1.0005 mm | `SDA_ISO` track at y = 49.3 and via at (75.3, 62.8) to `GND` pour |
| Nearest main-side via | 1.056 mm | `+3V3` via at (78.56, 60.24) to `GND_ISO` pour, both layers |
| Under U8 | 3.66 mm F.Cu, 3.70 mm B.Cu | see finding 29 |

- Nothing of the main side lies inside the island outline on either layer. Island copper outside the outline: two R21 pads by 0.025 mm and the `SDA_ISO` vias at (75.3, 62.8) by 0.09 mm and (67.35, 48.55) by 0.60 mm (into the strip under U8); all keep 1.0 mm or more to main-side copper.
- Rev 1.0 had 30 vias inside what is now the island outline (28 on `GND` or `+3V3`, 2 on `PH_SIG`). Rev 1.1 has 31, every one on an island net: 21 `GND_ISO` (15 of them at old `GND` positions, re-netted), 4 `+3V3_ISO`, 3 `PH_SIG`, 2 `SCL_ISO`, 1 `SDA_ISO`. No via kept the wrong net.
- The USB shell and all main-side connectors are outside the island. H1 has no copper. No drawing or text is on a copper layer.
- The island's own ground is one net: every piece of `GND_ISO` pour is joined to the others through vias, and DRC reports 0 unconnected items.

### C. Guard ring

| | F.Cu | B.Cu |
|---|---|---|
| Node copper enclosed by `PH_SIG` copper | yes, in two closed cells (J3 pad 1 + track + R20 pad 1; R20 pad 2 + track + U6 pin 5) | yes (J3 pad 1) |
| Other nets inside the cells | none | none |
| Guard fill | one piece, 49.2 mm² | one piece, 58.7 mm² |
| Joined to the net by | U6 pads 6 and 7 (solid), 2 vias | 2 vias, track to U7 pin 5 |
| Node to guard | 0.30 mm | 0.30 mm |
| Narrowest guard towards other copper | 0.44 mm at (62.2, 57.1) | 0.44 mm at (62.2, 57.1) |
| Ring width, median along the cell boundary | 1.45 mm and 1.34 mm | 1.00 mm |
| Node to nearest non-guard copper | 0.94 mm, `BNC_SH` (J3 pad 2) | 0.94 mm, `BNC_SH` (J3 pad 2) |
| Node to `GND_ISO` | 1.33 mm (pour) | 1.33 mm (pour) |

- No track of another net crosses the guard rectangle on either layer.
- U6's pin order helps: pin 5 (input) is next to pin 6 (`PH_SIG`), and the ring passes between the pad rows, 0.82 mm or more wide, before pin 4 (`GND_ISO`).

### D. DRC

| Run | Errors | Warnings | Unconnected |
|---|---|---|---|
| Saved board | 0 | 152 `lib_footprint_issues`, 7 `silk_edge_clearance` | 0 |
| After refilling all zones | 0 | same 159 | 0 |
| Control: rules tightened to 1.2 / 1.0 mm | 28 `clearance` | same 159 | 0 |
| Control: rules file removed | 0 | same 159 | 0 |

- The 152 library warnings are finding 20. The 7 silkscreen warnings are connector and module outlines that overhang the board edge (USB, BNC, antenna end of the module), as in rev 1.0. Both are acceptable.
- The control run proves the custom rules are loaded and act on pours, tracks and vias. A second control (see finding 30 in the previous section) shows that the isolation rule, now last in the file, takes priority where both rules apply.
- Refill: every zone fill is identical to the saved one (area difference 0, same number of pieces: guard 1 + 1, `GND_ISO` 5 + 2, `GND` 27 + 8).
- Severity settings are the same as in rev 1.0; courtyard overlap is an error and none is reported.

### E. Datasheets

ADM3260 (U8), all 20 pins, symbol against datasheet against net against position on the board (U8 is rotated 180°: pins 1 to 10 at x = 72.6 on the main side, pins 11 to 20 at x = 65.4 on the island; pin 1 at (72.6, 47.18)):

| Pin | Datasheet | Net | Pin | Datasheet | Net |
|---|---|---|---|---|---|
| 1 | GNDP | `GND` | 20 | GNDISO | `GND_ISO` |
| 2 | SCL2 | `SCL` | 19 | SCL1 | `SCL_ISO` |
| 3 | SDA2 | `SDA` | 18 | SDA1 | `SDA_ISO` |
| 4 | VDDP | `+3V3` | 17 | VDDISO | `+3V3_ISO` |
| 5 | GNDP | `GND` | 16 | GNDISO | `GND_ISO` |
| 6 | NC | open | 15 | NC | open |
| 7 | GNDP | `GND` | 14 | GNDISO | `GND_ISO` |
| 8 | PDIS | `GND` (converter on) | 13 | VSEL | `VSEL` |
| 9 | VIN | `+3V3` | 12 | VISO | `+3V3_ISO` |
| 10 | GNDP | `GND` | 11 | GNDISO | `GND_ISO` |

- `VSEL` divider: R51 16.9 k from `VISO`, R52 10 k to `GNDISO`. These are the datasheet's values for 3.3 V.
- Supply: `VIN`, `VDDP`, `VDDISO` each 3.0 to 5.5 V; 3.3 V in / 3.3 V out is a listed combination with 20 mA available. Island load, calculated: isolator logic 1.9 to 3.0 mA, ADS1115 0.15 to 0.3 mA, op-amp about 0.2 mA (figure not re-opened), `VSEL` divider 0.12 mA, reference divider 0.02 mA, plus 1.1 to 1.4 mA per pull-up while a line is low. About 2.5 to 4 mA steady, up to about 6.5 mA during transfers.
- Bypass capacitors: all four required positions are populated (C32 + C33 on `VIN`, C31 on `VDDP`, C34 + C35 on `VISO`, C36 on `VDDISO`). Copper path from capacitor pad to pin, measured inside the copper: C32 0.84 mm + 0.80 mm (`VIN`), C34 0.45 mm + 0.45 mm (`VISO`), both inside the 2 mm guidance for the 100 nF; C31 0.80 mm + 0.86 mm, C36 0.55 mm + 2.3 mm on the two logic supplies, for which the datasheet gives no distance.
- Primary side logic: thresholds 0.3 / 0.7 x `VDDP`, output low 0.4 V max, standard I2C. R16 / R17 4.7 k give 0.62 to 0.70 mA, above the 0.5 mA lower end of the specified static load. Compatible with the ESP32 at 3.3 V (ESP32 thresholds recalled, not re-opened).
- Isolated side logic: see finding 28.
- LDL1117S33R (U3): see the status table. 1.2 A, current limit 1.5 A minimum, fixed 3.3 V ±2 % at 25 °C and ±3 % over temperature (3.20 to 3.40 V; still above the 3.0 V minimum of the module and of U8 and the 3.1 V of the USB bridge after the track drop of finding 4). Quiescent current 0.25 mA typical. Thermal resistance 120 °C/W junction to ambient, the same order as the part it replaces.
- ADS1115 (U7): pin-out of the 10-pin package matches symbol and nets. `ADDR` to VDD gives address 1001001b = 0x49; U5 with `ADDR` to ground stays at 0x48. Unused inputs may float or be grounded: U7 AIN2 / AIN3 and U5 AIN0 / AIN1 are grounded. Unused `ALERT/RDY` may be left open (U7). Inputs stay inside the supply: 1.24 to 2.06 V on a 3.3 V island.

### F. SSOP-20 land pattern (U8)

| | Board | Package drawing RS-20 (MO-150-AE) |
|---|---|---|
| Pitch | 0.65 mm | 0.65 mm |
| Pad | 1.75 x 0.45 mm | lead width 0.22 to 0.38 mm, foot 0.55 to 0.95 mm |
| Row centres | ±3.6 mm | lead tips at ±3.7 to ±4.1 mm |
| Pad span, outer / inner | 8.95 / 5.45 mm | tip to tip 7.4 to 8.2 mm; body 5.0 to 5.6 mm |
| Body outline | 5.3 x 7.2 mm | 5.3 x 7.2 mm nominal |

Toe fillet 0.38 to 0.78 mm, heel inside the pad for all tolerances, body clear of the pads. Pin 1 marker and chamfer are at pin 1. This is the standard KiCad footprint for this package.

### G. Routing of what was added

- 207 tracks and vias are new against rev 1.0. Widths: `+3V3` to U8 0.25 / 0.30 mm, `+3V3_ISO` 0.20 to 0.30 mm, `GND` / `GND_ISO` stubs 0.20 to 0.30 mm, signals 0.20 mm. Adequate for the currents (20 mA or less on the island, under 100 mA into U8).
- No new via in a pad. One new via, (59.6, 43.03), is 0.125 mm from a pad of its own net (R51 pad 1). Vias are tented.
- No new copper closer than 0.225 mm to a pad of another net; no dangling track ends.
- I2C on the island: `SCL_ISO` 26.4 mm, `SDA_ISO` 29.9 mm, two vias each. Closest approach to the high-impedance node, in plan view on any layer: `SCL_ISO` 3.30 mm, `SDA_ISO` 3.93 mm, `+3V3_ISO` 2.45 mm, always with guard copper in between. Main-side `SDA` / `SCL` stay 7 mm or more away.
- `SDA` hop: 5.9 mm on F.Cu from (93.6, 50.2) to (95.4, 55.04), two extra vias (four in total between U8 and the existing bus). Harmless at I2C speeds.
- `SDA` / `SCL` / `+3V3` reach U8 on B.Cu as described; they leave one B.Cu ground sliver of 23 mm² with a single via between the `SDA` and `SCL` tracks, and the F.Cu ground pour is now two large pieces joined through B.Cu. No functional concern.
- Under the BNC body: `BNC_SH` tracks, island pour, guard, and the first 3.0 mm of the `PH_IN` track, all under mask. Rev 1.0 already had pour, vias and the same `PH_IN` track there.

### H. Mechanical

- No courtyard overlap (DRC, and computed separately). Closest: U7 to C22 0.02 mm; U8 to C34 0.04 mm and to R52, C36 and J3 0.05 mm; C35 to R51 0.04 mm; R51 to R52 and C36 0.07 mm; C33 to SW3 0.22 mm.
- SW3 was not moved; the nearest new part is C33, whose courtyard is 0.62 mm from the button body outline. "PAUSE" label moved 1.7 mm.
- H1: pour excluded within 3.6 mm of its centre on both layers; no track within 4.2 mm.
- BNC body outline on the board: x up to 64.95, y 48.60 to 63.40; no new part inside it.

### I. Fabrication files

- BOM: 48 lines, 145 references, each once. The 7 schematic parts not in it are H1 to H4, J16, J17, J18, as in rev 1.0. Every LCSC number and footprint equals the schematic field. New or changed lines against rev 1.0: U3 LDL1117S33R C435835, U8 C208558, U7 added to the C37593 line, R51 C25954, C34 on a line of its own (100 nF, 0402, C1525), R52 / R53 / R54, C31 to C33, C35 and C36 added to existing lines.
- LCSC pages opened: C435835 is LDL1117S33R in SOT-223; C208558 is ADM3260ARSZ in 5.3 mm SSOP-20; C25954 is 16.9 kΩ 1 % 0603; C1525 is 100 nF 16 V X7R 0402.
- Placement file: the same 145 references, all on the top side. Positions equal the footprint origin (centre of pads for J2, J3, U1, as in rev 1.0). Rotation offsets are consistent by footprint type with the existing rows: +270° for SOIC, VSSOP and now SSOP-20 (U8: board 180°, file 90°; U7 like U5), 0° for the passives (C34: board -90°, file 270°; C35 180°; R51 270°; R52 0°), 180° for SOT-23 and SOT-223, 0° for the rest.
- Gerbers: all 9 layers, both drill files, both drill maps and the job file in the zip are identical to a re-export of the saved board, creation dates aside (silkscreen with "subtract solder mask" on). 451 plated holes in the drill file = 401 vias + 50 through-hole pads. Mask openings: 446 top, 56 bottom, pads only.
- Schematic PDF: text identical to a re-export of the saved schematic; title block says rev 1.1. Board title block, top silkscreen and bottom legend say v1.1; the bottom legend reads "pH: isolated ADS1115 0x49, A0 = shield, A1 = probe" and "EC G25/G14 + ADS 0x48 A2".
- No personal name or place name found in the rev 1.1 files, PDFs or Gerbers.

## Could not verify

- **ERC.** Not available in `kicad-cli` 7.0.11. Replaced by netlist checks; pin-type conflicts were not checked by tool.
- **Anything electrical on real hardware**: `+3V3_ISO` level and ripple, converter emissions, their effect on the pH reading, I2C levels and error rate, regulator stability and temperature. All are bench tests listed in the verdict.
- **ADM3260 table columns.** The text I could extract lost the min / typ / max headings. The values used (0.5 / 0.7 V input threshold, 0.6 to 0.9 V output low, 1.9 / 3.0 mA logic supply, 3.3 / 10.5 mA no-load input current) are read as the ranges usual for this isolator family. The datasheet gives the feedback reference as 1.25 V in one place and 1.23 V in the equation; `VISO` is 3.36 V or 3.31 V accordingly. No tolerance for `VISO` was found.
- **LDL1117 stability plots and dropout curve.** Only the text of the datasheet could be read. The stable region of output capacitance and ESR (finding 36) and the dropout at currents below 1.2 A were not read from the figures. The tab connection is stated in the pin description.
- **Firmware beyond the two files supplied.** `phFromVolts`, the calibration code and the dosing logic were not available, so the consequence of finding 37 is conditional.
- **KiCad library copy of the 0402 footprint.** C34's pads and courtyard were compared with the standard dimensions of that footprint; no independent installation of the library was available to diff against.
- **Placement orientation and centroid at the assembler** for U8, U7, U3 and the 0402 C34: consistent with the existing rows, but whether those match the assembler's library can only be seen in its placement preview. Same for assembly eligibility, stock and edge-clearance rules at the assembler.
- **Assembly PDF**: contains v1.1 and the new references; not compared graphically with the board.
- **BNC connector body**: whether it is metal and whether it stands off the board was not looked up; it matters for the tracks under it (section G) and for finding 18.
- **Island bus capacitance** against the 40 pF limit of the isolator's side 1: estimated under 15 pF from 30 mm of track and one device, not measured.
- **Thresholds of the ESP32 and supply current of the MCP6002**: recalled from the earlier review, datasheets not re-opened.
- **Parts of the board that rev 1.1 did not change** were not reviewed again beyond confirming that they are unchanged.
- **Copper weight, via plating, electrode and junction resistances**: not in the files; values used are stated as assumptions.
