# KC-01-1830 Joystick Module: drawing sources

Source files for the Rev C drawings of the Joystick Module, board V2.1 (assembly KC-01-1830,
schematic KC-01-1831, PCB KC-01-1832, BOM KC-01-1833, placement KC-01-1834). The released
PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1830.json` | Drawing package config: titles, revisions, release status, notes, schematic blocks, assembly views and parts list, layer colours | all drawings |
| `Fusion_KC-01-1830_..._2026-11-24.pdf` | Fusion 360 assembly drawing (views and parts list); views as Rev A | 1830 ASSY |
| `SCH_KC-01-1831_..._2026-11-24.pdf` | EasyEDA Pro schematic export | 1831 SCH |
| `Gerber_KC-01-1832_..._2026-11-24.zip` | Fabrication zip as sent to JLCPCB: Gerbers, drill files, probe-test data | 1832 PWB, 1834 PLACE |
| `IPC-D-356A_KC-01-1832_..._2026-11-24.356a` | IPC-D-356A netlist (copper connectivity check) | reference |
| `BOM_KC-01-1832_..._2026-11-24.csv` | EasyEDA BOM export (UTF-16, tab separated) | 1833 BOM, 1834 PLACE |
| `PickAndPlace_KC-01-1832_..._2026-11-24.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 1834 PLACE |
| `ProPrj_KC-01-1830_..._2026-11-24.epro2` | EasyEDA Pro project | reference |

The EasyEDA design rule check passes (reported by the owner; no report file in the export).
The assembly parts list is written in the config (`assy_parts`), from the Rev A Fusion list
with the board number corrected to KC-01-1832.

## Board V2.1

- Rev C (2026-11-25) only corrects the stick in the notes to the JH-DX400B-R4 (4-axis; three axes wired to U6), as fitted on panels A2 and B2. Sources unchanged from Rev B.

- The PCA9306 level converter is the reusable sub-sheet "I2C Level Converter v2": VREF2 and
  EN joined and pulled up to +5V through R3$CBB15 (200 kΩ), with C2$CBB15 (100 pF) on that
  node (TI SCPS113O §10). Its parts carry the `$CBB15` suffix; the netlist calls them UT1–UT6.
- R13 (10 kΩ) pulls BUTTON_JOY low, so the stick button, wired from U6 pin 6 (+5V) to pin 2,
  reads active-high as the firmware expects. The first V2.1 export had no pull-down.
- BUTTON01 (SW1) is on ATtiny816 PC2 (pin 17). KerbalJoystickCore 2.3.1 reads it there; 2.3.0
  and earlier read PC0, which is not connected.
- The Fusion drawing came with the first export; the update changed only the board (R13).

Part descriptions, packages and JLCPCB part classes for the BOM come from
`Documents/Drawings/tools/parts.json`, keyed by LCSC number.

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1830/KC-01-1830.json --out <output folder>
```

`--docs 0,1,2,3,4` limits the run to the assembly, schematic, PWB, BOM or PLACE drawing.
Paths in the config are relative to the config file. The Gerber zip is unpacked
for each run.
- Rev D (2026-11-26, draft) corrects the stick in the notes again, to the SaiDian JH-D400X-R4
  (owner, 2026-11-26). Rev C's JH-DX400B-R4 came from the Fusion part name. Sources unchanged.
