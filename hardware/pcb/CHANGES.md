# Controller board: changes

## Rev 1.1 (2026-11-15)

Rev 1.1 answers the three major findings of the [rev 1.0 review](docs/REVIEW-rev1.0.md). Like rev 1.0 it has **never been fabricated**. The edits were made by script on the rev 1.0 files, not in an interactive layout tool, then checked by the tools in [`tools/`](tools/) and reviewed again by a separate AI session: [docs/REVIEW-rev1.1.md](docs/REVIEW-rev1.1.md). That review found no blocker and no major problem; it leaves 4 minor findings and 12 notes open.

### What changed

| Rev 1.0 finding | Change | Kind |
|---|---|---|
| 1. The pH guard ring was not a ring | The guard (net `PH_SIG`) now encloses the high-impedance input on both layers. Ground copper, which was 0.30 mm from the input, is now 1.33 mm away, behind the guard | Copper |
| 2. pH and EC shared the liquid and the board ground | The pH amplifier, its reference and a second ADS1115 (U7, address 0x49) sit on an isolated island with its own ground (`GND_ISO`). An ADM3260 (U8) carries I2C and power across. At least 1.0 mm of bare laminate separates the island from the rest of the board | Copper, 12 new parts |
| 3. The AMS1117 was fed below its minimum input voltage | U3 is now an LDL1117S33R: same footprint and pin-out, 0.35 V typical dropout, 18 V maximum input | Part |

Also:

- U5 (address 0x48) stays on the main side for EC. Its two former pH inputs are grounded.
- The +3.3 V feed to the ESP32 no longer runs through the pH area.
- Ten pins of existing parts changed net. No existing part moved.
- Silkscreen and title blocks read v1.1. The bottom legend gives the new ADC addresses.

### New parts

| Ref | Part | Role |
|---|---|---|
| U8 | ADM3260ARSZ | Isolated I2C and isolated 3.3 V supply |
| U7 | ADS1115 | pH converter on the island, address 0x49 |
| C31, C32, C33 | 100 nF, 100 nF, 10 µF | U8 supply bypass, main side |
| C34, C35, C36 | 100 nF (0402), 10 µF, 100 nF | U8 supply bypass, isolated side |
| R51, R52 | 16.9 k, 10 k | Sets the isolated supply to 3.3 V |
| R53, R54 | 2.2 k | I2C pull-ups on the island |

### What was checked, and how to repeat it

| Check | Result | Tool |
|---|---|---|
| Schematic against board | 434 pins, 0 differences | `tools/compare_netlist.py` |
| Nets that changed since rev 1.0 | The ten intended pins, nothing else | `tools/compare_netlist.py` with the rev 1.0 netlist |
| Island to main copper, both layers | 1.00 mm minimum | `tools/verify_geometry.py` |
| Guard ring closed, both layers | Yes. Narrowest section 0.44 mm | `tools/verify_geometry.py` |
| Design rules, as saved and after a zone refill | 0 errors, 0 unconnected | KiCad 7.0.11 DRC with the project's custom rules |
| Firmware for this board | Compiles; reads pH from 0x49 | See [firmware](../../firmware/) |

The remaining DRC warnings are the same two kinds as in rev 1.0: footprints without a library link, and connector outlines that overhang the board edge on the silkscreen.

### What nobody has checked

Anything electrical. In particular: the noise of the isolated supply on the pH reading, the I2C levels through the isolator, and the isolation itself. The review lists the bench tests to run first, in order.

### Limits to keep in mind

- The isolation is functional, for measurement only. It is not a safety barrier.
- The BNC body and mounting hole H1 are on the island. A metal panel touching the BNC, or a metal standoff at H1 tied to the other holes, bridges the isolation. Use a nylon screw at H1.

## Rev 1.0 (2026-10-12)

First version. Not published as files; reviewed in [docs/REVIEW-rev1.0.md](docs/REVIEW-rev1.0.md).
