# KC-01-1730 Panel Hub: drawing sources

Source files for the Rev B board drawings of the Panel Hub (board V2.1; Rev B released 2026-11-28, owner; supersedes Rev A)
(schematic KC-01-1731, PCB KC-01-1732, BOM KC-01-1733, placement KC-01-1734). There is no
KC-01-1730 assembly drawing. The released PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1730.json` | Drawing package config: titles, revisions, release status, notes, schematic block layout, layer colours, U4 note | all drawings |
| `SCH_KC-01-1731_..._2026-11-28.pdf` | EasyEDA Pro schematic export | 1731 SCH |
| `Gerber_KC-01-1732_..._2026-11-28.zip` | Fabrication zip as sent to JLCPCB: Gerbers, drill files, probe-test data | 1732 PWB, 1734 PLACE |
| `IPC-D-356A_KC-01-1732_..._2026-11-28.356a` | IPC-D-356A netlist (copper connectivity check). Rev A was exported in inch units (`UNITS CUST 0`, 0.0001 in), Rev B in metric (`UNITS CUST 1`, 0.001 mm) | reference |
| `BOM_KC-01-1732_..._2026-11-28.csv` | EasyEDA BOM export (UTF-16, tab separated) | 1733 BOM, 1734 PLACE |
| `PickAndPlace_KC-01-1732_..._2026-11-28.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 1734 PLACE |
| `ProPrj_KC-01-1730_..._2026-11-28.epro2` | EasyEDA Pro project | reference |

Part descriptions, packages and JLCPCB part classes for the BOM come from
`Documents/Drawings/tools/parts.json`, keyed by LCSC number.

## Rev B (2026-11-28)

The pre-order review changes (`Documents/Developer/Preorder_Review.md` 1.1, 1.2, 1.2a, 1.8), from the owner's 2026-11-28 export:

- MPM3610 U1: EN (pin 17) pulled up from V_IN through R3, 100 kΩ 0402 (C25741), instead of tied to V_IN.
- Input capacitor C1 10 µF (C3039694; was 4.7 µF), output capacitor C2 22 µF (C45783; was 10 µF).
- V_3P3 bleeder R5, 10 kΩ 0402 (C25744) to GND, beside U2's output capacitors: V_3P3 had no DC load, and the AP2112K cannot sink leakage coming back from the modules. About 0.33 mA.
- The new R3 moves the Rev A R3 (INT_SAFE divider, 10 kΩ, with R4) to R6, in the same place. U1, R1, R2 and the capacitors moved under 1.5 mm.
- Checked against Rev A (IPC netlists, part by part on pads and nets): connectivity unchanged apart from EN and R5; inner planes (V_IN, GND) clear every plated hole of another net by at least 0.41 mm; every outer-layer pour carries one net.
- The schematic export now has the 3.3 V regulator on page 2 and the MPM3610 on page 3; the config follows. The bleeder is on page 1, in the power block, whose crop is widened to take it.

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
- A second V2.1 export changed only the bottom silkscreen (now "Panel Hub V2.1") and removed a
  stale "*Safe Switch Connection" note from the schematic. The schematic, fabrication zip and
  project file are from that export; the netlist, BOM and placement files were unchanged by it.
