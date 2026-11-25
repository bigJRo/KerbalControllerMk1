# KC-01-1700 Control Module: drawing sources

Source files for the Control Module drawings (assembly KC-01-1700, schematic KC-01-1701,
PCB KC-01-1702), board V2.1. Rev C supersedes Rev B (board V2.0); earlier revisions and
their sources are in git history. The released PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1700.json` | Drawing package config: titles, revisions, release status, notes, schematic blocks and per-sheet scales, layer colours, placement split | all drawings |
| `Fusion_KC-01-1700_Control_Module_2026-11-25.pdf` | Fusion 360 assembly drawing (views and parts list; the title block still shows Rev A) | 1700 ASSY |
| `SCH_KC-01-1701_Control_Module_2026-11-25.pdf` | EasyEDA Pro schematic export (4 pages) | 1701 SCH |
| `Gerber_KC-01-1702_Control_Module_PCB_2026-11-25.zip` | Fabrication zip as sent to JLCPCB: 6-layer Gerbers, drill files (with routed slots), probe-test data | 1702 PWB, 1704 PLACE |
| `IPC-D-356A_KC-01-1702_Control_Module_PCB_2026-11-25.356a` | IPC-D-356A netlist (units: 0.0001 in) | reference |
| `BOM_KC-01-1702_Control_Module_PCB_2026-11-25.csv` | EasyEDA BOM export (UTF-16, tab separated) | 1703 BOM, 1704 PLACE |
| `PickAndPlace_KC-01-1702_Control_Module_PCB_2026-11-25.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 1704 PLACE |
| `ProPrj_KC-01-1700_Control_Module_2026-11-25.epro2` | EasyEDA Pro project | reference |

## Board V2.1 (review 2026-11-25)

- Copper connectivity on all six layers: the 86 nets in the IPC netlist are single copper islands; no opens, no shorts.
- The EasyEDA design rule check passes (reported by the owner; no report file in the export).
- Layers: Top (components, signals, GND pour), GND plane, 3.3 V plane, GND plane, 12 V plane, Bottom (signals, GND pour); stackup JLC06161H-3313.
- Status LEDs, all 0805 at about 0.5 mA: LED1 green on V_12 (R7, 20 kΩ), LED2 blue on +5V (R8, 4.7 kΩ, new), LED3 red on V_3P3 (R9, 2.7 kΩ), LED4 yellow on VIN (R10, 20 kΩ). V2.0 ran them at about 5 mA (2 kΩ / 270 Ω).
- Test port TP1 is a 1×5 header (C492404): 1 V_12, 2 GND, 3 +5V, 4 GND, 5 V_3P3, so no two rails are adjacent. Labelled on the silkscreen; LED1–LED3 sit below their pins.
- USB1–USB6 are vertical USB-A receptacles (C456015), numbered left to right under the silkscreen labels "USB 1" to "USB 6": hub 1 (U8) ports 1–3, then hub 2 (U9) ports 1–3 (nets D1–D6). VBUS is not connected, as on V2.0; each downstream Teensy runs from its own board.
- The shells of USB1–USB6, USBC1 and USBC2 are on GND (V2.0: not connected).
- USB-A pin order (1 VBUS, 2 D−, 3 D+, 4 GND) checked by the owner against the part's pinout (2026-11-25). The SHOU HAN drawing gives the 2.50 / 2.00 / 2.50 mm pin pitch, Ø0.95 mm pins and the pin row 2.30 mm from the shell legs, all matched by the footprint, but does not number the pins. Its shell legs are 12.90 ± 0.15 mm apart in 0.80 × 2.30 mm slots; the footprint has 13.10 mm and 0.80 × 2.20 mm.
- The connector bodies of USB1–USB6 reach about 1 mm past the top board edge (footprint outline); the pads stay on the board.
- H4 (EMC2101 remote sensor header) is fitted; nothing plugs into it yet.
- Fan header CN1 is numbered 1 PWM, 2 TACH, 3 12 V, 4 GND, but its key ramp covers pins 2–4 with PWM outside, so a standard fan plug mates correctly (Hardware Reference §6.6).
- No fuse on the 12 V input, by decision (Hardware Reference §6.1).

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1700/KC-01-1700.json --out <output folder>
```
