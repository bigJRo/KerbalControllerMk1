# KC-01-1730 Panel Hub: drawing sources

Source files for the Rev A board drawings of the Panel Hub (board V2.1)
(schematic KC-01-1731, PCB KC-01-1732, BOM KC-01-1733, placement KC-01-1734). There is no
KC-01-1730 assembly drawing. The released PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1730.json` | Drawing package config: titles, revisions, release status, notes, schematic block layout, layer colours, U4 note | all drawings |
| `SCH_KC-01-1731_..._2026-11-23.pdf` | EasyEDA Pro schematic export | 1731 SCH |
| `Gerber_KC-01-1732_..._2026-11-23.zip` | Fabrication zip as sent to JLCPCB: Gerbers, drill files, probe-test data | 1732 PWB, 1734 PLACE |
| `IPC-D-356A_KC-01-1732_..._2026-11-23.356a` | IPC-D-356A netlist (copper connectivity check). Exported in inch units (`UNITS CUST 0`, 0.0001 in) | reference |
| `BOM_KC-01-1732_..._2026-11-23.csv` | EasyEDA BOM export (UTF-16, tab separated) | 1733 BOM, 1734 PLACE |
| `PickAndPlace_KC-01-1732_..._2026-11-23.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 1734 PLACE |
| `ProPrj_KC-01-1730_..._2026-11-23.epro2` | EasyEDA Pro project | reference |

Part descriptions, packages and JLCPCB part classes for the BOM come from
`Documents/Drawings/tools/parts.json`, keyed by LCSC number.

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1730/KC-01-1730.json --out <output folder>
```

`--docs 1,2,3,4` limits the run to the schematic, PWB, BOM or PLACE drawing.
Paths in the config are relative to the config file. The Gerber zip is unpacked
for each run.

## Export history

- The first export (V2.0 layout) left U4 pin 1 unconnected: its "5V" symbol was not the
  +5V net. The board was corrected to V2.1 (U4 pin 1 on +5V) and re-exported; these are
  the V2.1 files. Netlist, BOM and placement are otherwise identical to the first export.
- The EasyEDA design rule check passes on V2.1 (reported by the owner; no report file in
  the export). The netlist is in inch units, like the Extension's.
- The silkscreen still reads "Panel Hub V2.0".
