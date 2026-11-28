# KC-01-1810 Wide Button Module: drawing sources

Source files for the Rev C drawings (draft, 2026-11-28; Rev B released 2026-11-24) of the Wide Button Module, board V2.1 (assembly KC-01-1810,
schematic KC-01-1811, PCB KC-01-1812, BOM KC-01-1813, placement KC-01-1814). The released
PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1810.json` | Drawing package config: titles, revisions, release status, notes, schematic blocks, assembly views and parts list, layer colours | all drawings |
| `Fusion_KC-01-1810_..._2026-11-24.pdf` | Fusion 360 assembly drawing (views and parts list); views as Rev A | 1810 ASSY |
| `SCH_KC-01-1811_..._2026-11-28.pdf` | EasyEDA Pro schematic export | 1811 SCH |
| `Gerber_KC-01-1812_..._2026-11-28.zip` | Fabrication zip as sent to JLCPCB: Gerbers, drill files, probe-test data | 1812 PWB, 1814 PLACE |
| `IPC-D-356A_KC-01-1812_..._2026-11-28.356a` | IPC-D-356A netlist (copper connectivity check) | reference |
| `BOM_KC-01-1812_..._2026-11-28.csv` | EasyEDA BOM export (UTF-16, tab separated) | 1813 BOM, 1814 PLACE |
| `PickAndPlace_KC-01-1812_..._2026-11-28.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 1814 PLACE |
| `ProPrj_KC-01-1810_..._2026-11-28.epro2` | EasyEDA Pro project | reference |

The EasyEDA design rule check passes (reported by the owner; no report file in the export).
The assembly parts list is written in the config (`assy_parts`), from the Rev A Fusion list
with the board number corrected to KC-01-1812.

## Board V2.1

- The PCA9306 level converter is the reusable sub-sheet "I2C Level Converter v2": VREF2 and
  EN joined and pulled up to +5V through R3$CBB15 (200 kΩ), with C2$CBB15 (100 pF) on that
  node (TI SCPS113O §10). Its parts carry the `$CBB15` suffix; the netlist calls them UT1–UT6.
- Shift register U19 uses only inputs A–D; E–H (pins 3–6) are grounded. The first V2.1 export
  also grounded pin 7 (/QH, an output), shorting it whenever QH was low. These files are the
  corrected export: pin 7 is unconnected, and only the bottom copper there changed.
- The silkscreen reads "Wide Button Module V2.1" (fabrication zip and project re-exported for
  that change only; netlist, BOM and placement files unchanged).
- The assembly includes a Panel Hub (item 5), listed as KC-01-1732.

Part descriptions, packages and JLCPCB part classes for the BOM come from
`Documents/Drawings/tools/parts.json`, keyed by LCSC number.

## Rev C (2026-11-28)

The pre-order review changes (`Documents/Developer/Preorder_Review.md` 1.1, 1.2, 1.2a, 1.3), from the owner's 2026-11-28 export, made as on the Button Module (KC-01-1800 Rev C):

- The 5 V converter is a reusable sub-sheet (`$CBB15`); the I²C level converter moves to `$CBB14`. Rev B → Rev C: U2 → U1$CBB15 (MPM3610), C3 → C1$CBB15, C4 → C2$CBB15, R3 → R1$CBB15, R4 → R2$CBB15; U1$CBB15 (PCA9306) → U1$CBB14, R1–R3$CBB15 → R1–R3$CBB14, C1/C2$CBB15 → C1/C2$CBB14.
- MPM3610 EN pulled up from IN through R3$CBB15, 100 kΩ (C25741). Input C1$CBB15 10 µF (C3039694), output C2$CBB15 22 µF (C45783).
- I²C pull-ups R1$CBB14 / R2$CBB14 47 kΩ (C25792; were 4.7 kΩ), sized for 22 module positions on SDA_BUS.
- Checked against Rev B (IPC netlists by net and pin, with the renames above): connectivity unchanged apart from EN; BOM matches; inner planes (+5 V, GND) clear every plated hole of another net by at least 0.41 mm; every outer-layer pour carries one net. The converter parts moved about 1.3 mm.
- The schematic export now has the level converter on page 3 and the MPM3610 on page 4; the config follows. Assembly views at 0.35 line weight.

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1810/KC-01-1810.json --out <output folder>
```

`--docs 0,1,2,3,4` limits the run to the assembly, schematic, PWB, BOM or PLACE drawing.
Paths in the config are relative to the config file. The Gerber zip is unpacked
for each run.
