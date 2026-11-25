# KC-01-1910 7in TFT Display Driver: drawing sources

Source files for the Rev B drawings of the 7in TFT Display Driver, board V2.1 (assembly KC-01-1910,
schematic KC-01-1911, PCB KC-01-1912, BOM KC-01-1913, placement KC-01-1914). The released
PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1910.json` | Drawing package config: titles, revisions, release status, notes, schematic blocks, assembly views and parts list, layer colours | all drawings |
| `Fusion_KC-01-1910_..._2026-11-23.pdf` | Fusion 360 assembly drawing (views and parts list); views unchanged from Rev A | 1910 ASSY |
| `SCH_KC-01-1911_..._2026-11-23.pdf` | EasyEDA Pro schematic export | 1911 SCH |
| `Gerber_KC-01-1912_..._2026-11-23.zip` | Fabrication zip as sent to JLCPCB: Gerbers, drill files, probe-test data | 1912 PWB, 1914 PLACE |
| `IPC-D-356A_KC-01-1912_..._2026-11-23.356a` | IPC-D-356A netlist (copper connectivity check) | reference |
| `BOM_KC-01-1912_..._2026-11-23.csv` | EasyEDA BOM export (UTF-16, tab separated) | 1913 BOM, 1914 PLACE |
| `PickAndPlace_KC-01-1912_..._2026-11-23.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 1914 PLACE |
| `ProPrj_KC-01-1910_..._2026-11-23.epro2` | EasyEDA Pro project | reference |

The EasyEDA design rule check passes (reported by the owner; no report file in the export).
The assembly parts list is written in the config (`assy_parts`): the Fusion list has the board
as "KC_01_1912" and no descriptions, so the descriptions come from the Rev A drawing.

## Board V2.1

- Schematic and PCB are both V2.1 (Rev A had a V2.1 schematic with V2.0 PCB drawings). V2.1
  wires the DFPlayer BUSY line back to the Teensy (AUDIO_BUSY, pin 11).
- Every connected Teensy pin matches `Software/Displays/libraries/KCMk1_SystemConfig` and
  `KerbalDisplayAudio`.
- Safe-switch terminal U3: pin 1 on 5V, V_EN on pins 2, 3, 5, pin 6 to INT_SAFE through the
  10 kΩ / 20 kΩ divider, as on the Panel Hub V2.1.
- Six layers, JLCPCB JLC06161H-3313 stackup: GND pour top, 5V / GND / V_IN / GND planes, 3V3 pour
  bottom.
- The schematic PDF and project were re-exported once to remove a stale "*Safe Switch
  Connection" note beside slot P2; nothing else changed.

Part descriptions, packages and JLCPCB part classes for the BOM come from
`Documents/Drawings/tools/parts.json`, keyed by LCSC number.

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1910/KC-01-1910.json --out <output folder>
```

`--docs 0,1,2,3,4` limits the run to the assembly, schematic, PWB, BOM or PLACE drawing.
Paths in the config are relative to the config file. The Gerber zip is unpacked
for each run.
