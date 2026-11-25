# KC-01-1710 Control Module Extension: drawing sources

Source files for the Rev A board drawings of the Control Module Extension
(schematic KC-01-1711, PCB KC-01-1712, BOM KC-01-1713, placement KC-01-1714). There is no
KC-01-1710 assembly drawing. The released PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1710.json` | Drawing package config: titles, revisions, release status, notes, schematic block layout, layer colours, placement split | all drawings |
| `SCH_KC-01-1711_..._2026-11-23.pdf` | EasyEDA Pro schematic export | 1711 SCH |
| `Gerber_KC-01-1712_..._2026-11-23.zip` | Fabrication zip as sent to JLCPCB: Gerbers, drill files, probe-test data | 1712 PWB, 1714 PLACE |
| `IPC-D-356A_KC-01-1712_..._2026-11-23.356a` | IPC-D-356A netlist (copper connectivity check). Exported in inch units (`UNITS CUST 0`, 0.0001 in), unlike the other boards | reference |
| `BOM_KC-01-1712_..._2026-11-23.csv` | EasyEDA BOM export (UTF-16, tab separated) | 1713 BOM, 1714 PLACE |
| `PickAndPlace_KC-01-1712_..._2026-11-23.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 1714 PLACE |
| `DRC_KC-01-1712_..._2026-11-23.txt` | EasyEDA design rule check report: no issues | reference |
| `ProPrj_KC-01-1710_..._2026-11-23.epro2` | EasyEDA Pro project | reference |

Part descriptions, packages and JLCPCB part classes for the BOM come from
`Documents/Drawings/tools/parts.json`, keyed by LCSC number.

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1710/KC-01-1710.json --out <output folder>
```

`--docs 1,2,3,4` limits the run to the schematic, PWB, BOM or PLACE drawing.
Paths in the config are relative to the config file. The Gerber zip is unpacked
for each run.
