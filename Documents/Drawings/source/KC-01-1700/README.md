# KC-01-1700 Control Module: drawing sources

Source files for the Control Module drawings (assembly KC-01-1700, schematic KC-01-1701,
PCB KC-01-1702). Rev B supersedes the Rev A assembly drawing and the drawings named by board
version (KC-01-170x_V2.0_Control Module_*.pdf). The released PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1700.json` | Drawing package config: titles, revisions, release status, notes, schematic blocks and per-sheet scales, layer colours, placement split | all drawings |
| `Fusion_KC-01-1700_Control_Module_2026-06-20.pdf` | Fusion 360 assembly drawing (the Rev A drawing: views and parts list) | 1700 ASSY |
| `SCH_KC-01-1701_Control_Module_2026-11-23.pdf` | EasyEDA Pro schematic export (4 pages) | 1701 SCH |
| `Gerber_KC-01-1702_Control_Module_PCB_2026-11-23.zip` | Fabrication zip as sent to JLCPCB: 6-layer Gerbers, drill files (with routed slots), probe-test data | 1702 PWB, 1704 PLACE |
| `IPC-D-356A_KC-01-1702_Control_Module_PCB_2026-11-23.356a` | IPC-D-356A netlist | reference |
| `BOM_KC-01-1702_Control_Module_PCB_2026-11-23.csv` | EasyEDA BOM export (UTF-16, tab separated) | 1703 BOM, 1704 PLACE |
| `PickAndPlace_KC-01-1702_Control_Module_PCB_2026-11-23.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 1704 PLACE |
| `DRC_KC-01-1702_Control_Module_PCB_2026-11-23.txt` | EasyEDA design rule check: no issues | reference |
| `ProPrj_KC-01-1700_Control_Module_2026-11-23.epro2` | EasyEDA Pro project | reference |

## Review notes (2026-11-24)

- Copper connectivity on all six layers: the 85 nets in the IPC netlist are single copper islands; no opens, no shorts.
- Layers: Top (components, signals, GND pour), GND plane, 3.3 V plane, GND plane, 12 V plane, Bottom (signals, GND pour); stackup JLC06161H-3313.
- Changes from the V2.0 board: H4 (EMC2101 remote sensor header) and TP1 (test port) are now fitted (C492401, C2937625). Nothing plugs into H4 yet.
- TP1 pin order is V_12, V_3P3, GND; kept as is by decision, with a caution on the PLACE drawing.
- Fan header CN1 is numbered 1 PWM, 2 TACH, 3 12 V, 4 GND, but its silkscreen key puts PWM outside the ramp, so a standard fan plug mates correctly (Hardware Reference open item 1).

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1700/KC-01-1700.json --out <output folder>
```
