# KC-01-2710 Expansion Keyboard: drawing sources

Source files for the Expansion Keyboard drawings (schematic KC-01-2711, PCB KC-01-2712).
Released PDFs go in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-2710.json` | Drawing package config: titles, revisions, release status, notes, schematic block layout and per-sheet scales, layer colours, extra BOM lines | all drawings |
| `SCH_KC-01-2711_..._2026-11-23.pdf` | EasyEDA Pro schematic export | 2711 SCH |
| `Gerber_KC-01-2712_..._2026-11-23.zip` | Fabrication zip as sent to JLCPCB: Gerbers, drill files (PTH, NPTH, vias), probe-test data | 2712 PWB, 2714 PLACE |
| `IPC-D-356A_KC-01-2712_..._2026-11-23.356a` | IPC-D-356A netlist (copper connectivity check, pad and net data) | reference |
| `BOM_KC-01-2712_..._2026-11-23.csv` | EasyEDA BOM export (UTF-16, tab separated). The key switches are not in it; the config adds them | 2713 BOM, 2714 PLACE |
| `PickAndPlace_KC-01-2712_..._2026-11-23.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 2714 PLACE |
| `DRC_KC-01-2712_..._2026-11-23.txt` | EasyEDA design rule check: 50 pad-to-cutout clearance errors on the SK6812MINI-EA footprints, accepted (0.10 mm by design, PWB note 8) | reference |
| `ProPrj_KC-01-2710_..._2026-11-23.epro2` | EasyEDA Pro project | reference |

Part descriptions, packages and JLCPCB part classes for the BOM come from
`Documents/Drawings/tools/parts.json`, keyed by LCSC number.

## Review notes (2026-11-24)

- Copper connectivity: all 64 nets in the IPC netlist are single copper islands; no opens, no shorts.
- The PTH drill file repeats all 104 small holes that are also in the via file. The drawings
  count each hole once (268 holes); JLCPCB's CAM merges duplicate hits.

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-2710/KC-01-2710.json --out <output folder> --docs 1,2,3,4
```

Add `0` to `--docs` once the KC-01-2710 assembly drawing's Fusion export is in this folder
and in the config (`assy_pdf`, `assy_sheets`, `assy_parts`, `assy_notes`).
