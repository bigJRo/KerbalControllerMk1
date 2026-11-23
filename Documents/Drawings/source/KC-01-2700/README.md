# KC-01-2700 7in TFT Expansion Unit: drawing sources

Source files for the released Rev - board drawings of the Expansion Main Board
(schematic KC-01-2701, PCB KC-01-2702). The released PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-2700.json` | Drawing package config: titles, revisions, release status, notes, schematic block layout, layer colours | all drawings |
| `SCH_KC-01-2701_..._2026-11-23.pdf` | EasyEDA Pro schematic export | 2701 SCH |
| `Gerber_KC-01-2702_..._2026-11-23.zip` | Fabrication zip as sent to JLCPCB: Gerbers, drill files, probe-test data | 2702 PWB, 2704 PLACE |
| `IPC-D-356A_KC-01-2702_..._2026-11-23.356a` | IPC-D-356A netlist (copper connectivity check, pad and net data) | reference |
| `BOM_KC-01-2702_..._2026-11-23.csv` | EasyEDA BOM export (UTF-16, tab separated), with H4 = C492404 | 2703 BOM, 2704 PLACE |
| `PickAndPlace_KC-01-2702_..._2026-11-23.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 2704 PLACE |
| `DRC_KC-01-2702_..._2026-11-23.txt` | EasyEDA design rule check report: no issues | reference |
| `ProPrj_KC-01-2700_..._2026-11-23.epro2` | EasyEDA Pro project | reference |

Part descriptions, packages and JLCPCB part classes for the BOM come from
`Documents/Drawings/tools/parts.json`, keyed by LCSC number.

## Export history

- The BOM, pick-and-place and `.epro2` files were re-exported after H4 was given its
  part (XFCN PZ254V-11-05P, C492404). Part positions did not change. The fabrication zip,
  netlist, DRC report and schematic are from the first export of the same day.

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-2700/KC-01-2700.json --out <output folder>
```

`--docs 1,2,3,4` limits the run to the schematic, PWB, BOM or PLACE drawing.
Paths in the config are relative to the config file. The Gerber zip is unpacked
for each run.
