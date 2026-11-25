# KC-01-1870 Throttle Module: drawing sources

Source files for the Rev B drawings of the Throttle Module, board V2.1 (assembly KC-01-1870,
schematic KC-01-1871, PCB KC-01-1872, BOM KC-01-1873, placement KC-01-1874). The released
PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1870.json` | Drawing package config: titles, revisions, release status, notes, schematic blocks, assembly views and parts list, layer colours | all drawings |
| `Fusion_KC-01-1870_..._2026-11-24.pdf` | Fusion 360 assembly drawing (views and parts list); views as Rev A | 1870 ASSY |
| `SCH_KC-01-1871_..._2026-11-24.pdf` | EasyEDA Pro schematic export | 1871 SCH |
| `Gerber_KC-01-1872_..._2026-11-24.zip` | Fabrication zip as sent to JLCPCB: Gerbers, drill files, probe-test data | 1872 PWB, 1874 PLACE |
| `IPC-D-356A_KC-01-1872_..._2026-11-24.356a` | IPC-D-356A netlist (copper connectivity check) | reference |
| `BOM_KC-01-1872_..._2026-11-24.csv` | EasyEDA BOM export (UTF-16, tab separated) | 1873 BOM, 1874 PLACE |
| `PickAndPlace_KC-01-1872_..._2026-11-24.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 1874 PLACE |
| `ProPrj_KC-01-1870_..._2026-11-24.epro2` | EasyEDA Pro project | reference |

The EasyEDA design rule check passes (reported by the owner; no report file in the export).
The assembly parts list is written in the config (`assy_parts`), from the Rev A Fusion list
with the board number corrected to KC-01-1872.

## Board V2.1

- The PCA9306 level converter has VREF2 and EN joined and pulled up to +5V through R23
  (200 kΩ), with C2 (100 pF) on that node (TI SCPS113O §10).
- Every ATtiny816 pin the firmware uses matches `Software/Modules/Throttle/Config.h`.
  THRTL_ENA (terminal U5) reaches PB5 with a pull-down; the firmware does not read it.
- The motor runs from U12 (MPM3610, R21 / R22 set 8.8 V) through U9 (L293DD, both halves
  paralleled). LED1 (red) and LED2 (green) are anti-parallel across the motor.
- The motorized fader VR1 is in the EasyEDA BOM but is fitted after delivery (owner, 2026-11-25).
- No mounting holes. The Fusion list carries the board as "10-1872"; the drawing uses KC-01-1872.
- The EasyEDA design rule check passes (reported by the owner; no report file in the export).

Part descriptions, packages and JLCPCB part classes for the BOM come from
`Documents/Drawings/tools/parts.json`, keyed by LCSC number.

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1870/KC-01-1870.json --out <output folder>
```

`--docs 0,1,2,3,4` limits the run to the assembly, schematic, PWB, BOM or PLACE drawing.
Paths in the config are relative to the config file. The Gerber zip is unpacked
for each run.
