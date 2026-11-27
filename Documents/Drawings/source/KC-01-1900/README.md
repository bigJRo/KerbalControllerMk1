# KC-01-1900 2in TFT Display Driver: drawing sources

Source files for the Rev B drawings of the 2in TFT Display Driver (assembly KC-01-1900,
schematic KC-01-1901, PCB KC-01-1902, BOM KC-01-1903, placement KC-01-1904). The released
PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1900.json` | Drawing package config: titles, revisions, release status, notes, schematic blocks, assembly views and parts list, layer colours | all drawings |
| `Fusion_KC-01-1900_..._2026-11-23.pdf` | Fusion 360 assembly drawing (views and parts list); views unchanged from Rev A | 1900 ASSY |
| `SCH_KC-01-1901_..._2026-11-23.pdf` | EasyEDA Pro schematic export | 1901 SCH |
| `Gerber_KC-01-1902_..._2026-11-23.zip` | Fabrication zip as sent to JLCPCB: Gerbers, drill files, probe-test data | 1902 PWB, 1904 PLACE |
| `IPC-D-356A_KC-01-1902_..._2026-11-23.356a` | IPC-D-356A netlist (copper connectivity check) | reference |
| `BOM_KC-01-1902_..._2026-11-23.csv` | EasyEDA BOM export (UTF-16, tab separated) | 1903 BOM, 1904 PLACE |
| `PickAndPlace_KC-01-1902_..._2026-11-23.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 1904 PLACE |
| `ProPrj_KC-01-1900_..._2026-11-23.epro2` | EasyEDA Pro project | reference |
| `Datasheet_ER-TFTM020-2.pdf` | EastRising datasheet of the 2in display (8-pin header: GND, VCC, SCL, SDA, RES, DC, CS, BLK; BLK is a backlight on/off control input, VCC 3.0-3.5 V); matches H3, checked 2026-11-27 | reference |

The EasyEDA design rule check passes (reported by the owner; no report file in the export).
The assembly parts list is written in the config (`assy_parts`): the Fusion list carries a
file-name part number for the display and the old "01-1902" number for the board.

Part descriptions, packages and JLCPCB part classes for the BOM come from
`Documents/Drawings/tools/parts.json`, keyed by LCSC number.

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1900/KC-01-1900.json --out <output folder>
```

`--docs 0,1,2,3,4` limits the run to the assembly, schematic, PWB, BOM or PLACE drawing.
Paths in the config are relative to the config file. The Gerber zip is unpacked
for each run.
