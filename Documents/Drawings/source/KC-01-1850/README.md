# KC-01-1850 Encoder: drawing sources

Source files for the Encoder board drawings (schematic KC-01-1851, PCB KC-01-1852). This
package has no KC-01-1850 assembly drawing. Rev A supersedes the earlier drawings that
were named by board version (KC-01-185x_V2.0_Encoder Module_*.pdf). The released PDFs are
in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1850.json` | Drawing package config: titles, revisions, release status, notes, schematic block layout, scales, layer colours | all drawings |
| `SCH_KC-01-1851_Encoder_2026-11-23.pdf` | EasyEDA Pro schematic export | 1851 SCH |
| `Gerber_KC-01-1852_Encoder_PCB_2026-11-23.zip` | Fabrication zip as sent to JLCPCB: Gerbers, drill files (with the two routed tab slots), probe-test data | 1852 PWB, 1854 PLACE |
| `IPC-D-356A_KC-01-1852_Encoder_PCB_2026-11-23.356a` | IPC-D-356A netlist | reference |
| `BOM_KC-01-1852_Encoder_PCB_2026-11-23.csv` | EasyEDA BOM export (UTF-16, tab separated) | 1853 BOM, 1854 PLACE |
| `PickAndPlace_KC-01-1852_Encoder_PCB_2026-11-23.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 1854 PLACE |
| `DRC_KC-01-1852_Encoder_PCB_2026-11-23.txt` | EasyEDA design rule check: no issues | reference |
| `ProPrj_KC-01-1850_Encoder_2026-11-23.epro2` | EasyEDA Pro project | reference |

## Review notes (2026-11-24)

- Copper connectivity: all 7 nets in the IPC netlist are single copper islands; no opens, no shorts.
- The netlist matches the schematic and the expansion's H4 pinout (1 VCC, 2 ENC_A, 3 ENC_B, 4 BTN_EN, 5 GND).

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1850/KC-01-1850.json --out <output folder> --docs 1,2,3,4
```
