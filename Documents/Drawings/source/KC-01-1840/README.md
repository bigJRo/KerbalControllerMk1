# KC-01-1840 7-Segment Display Module: drawing sources

Source files for the Rev B drawings of the 7-Segment Display Module, board V2.1 (assembly KC-01-1840,
schematic KC-01-1841, PCB KC-01-1842, BOM KC-01-1843, placement KC-01-1844). The released
PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1840.json` | Drawing package config: titles, revisions, release status, notes, schematic blocks, assembly views and parts list, layer colours | all drawings |
| `Fusion_KC-01-1840_..._2026-11-24.pdf` | Fusion 360 assembly drawing (views and parts list); views as Rev A | 1840 ASSY |
| `SCH_KC-01-1841_..._2026-11-24.pdf` | EasyEDA Pro schematic export | 1841 SCH |
| `Gerber_KC-01-1842_..._2026-11-24.zip` | Fabrication zip as sent to JLCPCB: Gerbers, drill files, probe-test data | 1842 PWB, 1844 PLACE |
| `IPC-D-356A_KC-01-1842_..._2026-11-24.356a` | IPC-D-356A netlist (copper connectivity check) | reference |
| `BOM_KC-01-1842_..._2026-11-24.csv` | EasyEDA BOM export (UTF-16, tab separated) | 1843 BOM, 1844 PLACE |
| `DRC_KC-01-1842_..._2026-11-24.txt` | EasyEDA design rule check report: 12 SK6812MINI-EA pad-to-cutout clearances (0.10 mm), accepted by design (PWB fab note 8) | reference |
| `PickAndPlace_KC-01-1842_..._2026-11-24.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 1844 PLACE |
| `ProPrj_KC-01-1840_..._2026-11-24.epro2` | EasyEDA Pro project | reference |

The assembly parts list is written in the config (`assy_parts`), from the Rev A Fusion list
with the board number corrected to KC-01-1842.

## Board V2.1

- The PCA9306 level converter has VREF2 and EN joined and pulled up to +5V through R19
  (200 kΩ), with C2 (100 pF) on that node (TI SCPS113O §10). Flat designators on this board.
- Every ATtiny816 pin matches `Kerbal7SegmentCore` (`K7SC_Config.h`).
- The three key switches SW2–SW4 (Kailh Choc V1 White, CPG135001D03) are not in the EasyEDA
  BOM; the BOM drawing adds them as a line fitted after delivery. Their keycaps are the MBK
  keycap 90000001, the same part as on the Expansion Keyboard (the Fusion list calls it a
  22 mm button cover).
- The Fusion list carries a file-name part number for the board; the drawing uses KC-01-1842.
- The silkscreen reads "7-Segment Module V2.1".

Part descriptions, packages and JLCPCB part classes for the BOM come from
`Documents/Drawings/tools/parts.json`, keyed by LCSC number.

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1840/KC-01-1840.json --out <output folder>
```

`--docs 0,1,2,3,4` limits the run to the assembly, schematic, PWB, BOM or PLACE drawing.
Paths in the config are relative to the config file. The Gerber zip is unpacked
for each run.
