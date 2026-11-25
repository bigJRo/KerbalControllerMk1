# KC-01-1860 Dual Encoder Module: drawing sources

Source files for the Rev B drawings of the Dual Encoder Module, board V2.1 (assembly KC-01-1860,
schematic KC-01-1861, PCB KC-01-1862, BOM KC-01-1863, placement KC-01-1864). The released
PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1860.json` | Drawing package config: titles, revisions, release status, notes, schematic blocks, assembly views and parts list, layer colours | all drawings |
| `Fusion_KC-01-1860_..._2026-11-24.pdf` | Fusion 360 assembly drawing (views and parts list); views as Rev A | 1860 ASSY |
| `SCH_KC-01-1861_..._2026-11-24.pdf` | EasyEDA Pro schematic export | 1861 SCH |
| `Gerber_KC-01-1862_..._2026-11-24.zip` | Fabrication zip as sent to JLCPCB: Gerbers, drill files, probe-test data | 1862 PWB, 1864 PLACE |
| `IPC-D-356A_KC-01-1862_..._2026-11-24.356a` | IPC-D-356A netlist (copper connectivity check) | reference |
| `BOM_KC-01-1862_..._2026-11-24.csv` | EasyEDA BOM export (UTF-16, tab separated) | 1863 BOM, 1864 PLACE |
| `PickAndPlace_KC-01-1862_..._2026-11-24.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 1864 PLACE |
| `ProPrj_KC-01-1860_..._2026-11-24.epro2` | EasyEDA Pro project | reference |

The EasyEDA design rule check passes (reported by the owner; no report file in the export).
The assembly parts list is written in the config (`assy_parts`), from the Rev A Fusion list
with the board number corrected to KC-01-1862.

## Board V2.1

- Both PEC11R encoders (SW1, SW2) now mount on the module, with the A / B RC filters and
  push-button pull-downs on board. V2.0 used two Encoder boards (KC-01-1850) on 5-pin headers.
- The safe-switch terminal is gone (it drove slot pin 16, which no hub connects).
- The PCA9306 level converter has VREF2 and EN joined and pulled up to +5V through R1
  (200 kΩ), with C1 (100 pF) on that node (TI SCPS113O §10).
- Every ATtiny816 pin matches the Dual Encoder firmware (`Software/Modules/Dual Encoder/Config.h`).
- No mounting holes. The Fusion parts list has items 1 and 3 only and the board as "KC_01_1862";
  the drawing lists the board (KC-01-1862) and the two knobs as items 1 and 2.
- The EasyEDA design rule check passes (reported by the owner; no report file in the export).

Part descriptions, packages and JLCPCB part classes for the BOM come from
`Documents/Drawings/tools/parts.json`, keyed by LCSC number.

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1860/KC-01-1860.json --out <output folder>
```

`--docs 0,1,2,3,4` limits the run to the assembly, schematic, PWB, BOM or PLACE drawing.
Paths in the config are relative to the config file. The Gerber zip is unpacked
for each run.
