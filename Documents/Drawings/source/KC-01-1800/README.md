# KC-01-1800 Button Module: drawing sources

Source files for the Rev C drawings (draft, 2026-11-28; Rev B released 2026-11-24) of the Button Module, board V2.1 (assembly KC-01-1800,
schematic KC-01-1801, PCB KC-01-1802, BOM KC-01-1803, placement KC-01-1804). The released
PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1800.json` | Drawing package config: titles, revisions, release status, notes, schematic blocks, assembly views and parts list, layer colours | all drawings |
| `Fusion_KC-01-1800_..._2026-11-24.pdf` | Fusion 360 assembly drawing (views and parts list); views as Rev A | 1800 ASSY |
| `SCH_KC-01-1801_..._2026-11-28.pdf` | EasyEDA Pro schematic export | 1801 SCH |
| `Gerber_KC-01-1802_..._2026-11-28.zip` | Fabrication zip as sent to JLCPCB: Gerbers, drill files, probe-test data | 1802 PWB, 1804 PLACE |
| `IPC-D-356A_KC-01-1802_..._2026-11-28.356a` | IPC-D-356A netlist (copper connectivity check) | reference |
| `BOM_KC-01-1802_..._2026-11-28.csv` | EasyEDA BOM export (UTF-16, tab separated) | 1803 BOM, 1804 PLACE |
| `PickAndPlace_KC-01-1802_..._2026-11-28.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 1804 PLACE |
| `ProPrj_KC-01-1800_..._2026-11-28.epro2` | EasyEDA Pro project | reference |

The EasyEDA design rule check passes (reported by the owner; no report file in the export).
The assembly parts list is written in the config (`assy_parts`), from the Rev A Fusion list
with the board number corrected to KC-01-1802.

## Board V2.1

V2.0 tied the PCA9306 VREF2 and EN pins straight to +5V. TI's datasheet (SCPS113O §8.1.2,
§9.2.2.1, §10) requires them joined and pulled up through 200 kΩ, with 100 pF on VREF2;
without it the reference FET carries current from +5V into V_3P3. V2.1 adds R3$CBB15
(200 kΩ, C25764) and moves C2$CBB15 to the VREF2/EN node. The level converter is now the
reusable sub-sheet "I2C Level Converter v2", so its parts carry the `$CBB15` suffix in the
schematic, BOM and placement files (the netlist calls them UT1–UT6). The silkscreen reads V2.1.

Part descriptions, packages and JLCPCB part classes for the BOM come from
`Documents/Drawings/tools/parts.json`, keyed by LCSC number.

## Rev C (2026-11-28)

The pre-order review changes (`Documents/Developer/Preorder_Review.md` 1.1, 1.2, 1.2a, 1.3), from the owner's 2026-11-28 export (DRC clean, owner):

- The 5 V converter is now a reusable sub-sheet too, so its parts carry `$CBB15` and the I²C level converter's parts moved to `$CBB14`. Rev B → Rev C: U2 → U1$CBB15 (MPM3610), C3 → C1$CBB15, C4 → C2$CBB15, R3 → R1$CBB15, R4 → R2$CBB15; U1$CBB15 (PCA9306) → U1$CBB14, R1–R3$CBB15 → R1–R3$CBB14, C1/C2$CBB15 → C1/C2$CBB14.
- MPM3610: EN (pin 17) pulled up from IN through R3$CBB15, 100 kΩ (C25741), instead of tied to IN. Input C1$CBB15 10 µF (C3039694; was 4.7 µF), output C2$CBB15 22 µF (C45783; was 10 µF).
- I²C pull-ups R1$CBB14 / R2$CBB14 (SCL_MOD / SDA_MOD to +5 V) 47 kΩ (C25792; were 4.7 kΩ): through the PCA9306 every module's pull-up loads SDA_BUS, sized for 22 module positions (about 3.0 mA total sink).
- Checked against Rev B (IPC netlists by net and pin, sub-sheet aliases resolved): connectivity unchanged apart from EN; BOM matches; inner planes (+5 V, GND) clear every plated hole of another net by at least 0.41 mm; every outer-layer pour carries one net. The converter parts moved about 1.3 mm. A second export the same day re-routed the board (same netlist and BOM; the converter block 0.1 mm over); the sources are that export.
- The schematic export now has the level converter on page 3 and the MPM3610 on page 4; the config follows. Assembly views at 0.35 line weight.

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1800/KC-01-1800.json --out <output folder>
```

`--docs 0,1,2,3,4` limits the run to the assembly, schematic, PWB, BOM or PLACE drawing.
Paths in the config are relative to the config file. The Gerber zip is unpacked
for each run.
