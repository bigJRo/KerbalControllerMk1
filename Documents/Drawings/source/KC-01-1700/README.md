# KC-01-1700 Control Module: drawing sources

Source files for the Control Module drawings (assembly KC-01-1700, schematic KC-01-1701,
PCB KC-01-1702), board V2.1. Rev D supersedes Rev C (pre-order review changes, same board
version: no V2.1 board had been fabricated); earlier revisions and their sources are in git history. The released PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1700.json` | Drawing package config: titles, revisions, release status, notes, schematic blocks and per-sheet scales, layer colours, placement split | all drawings |
| `Fusion_KC-01-1700_Control_Module_2026-11-28.pdf` | Fusion 360 drawing of the board (owner, 2026-11-28): top, right, front and isometric views at 1:2 on page 1 (page 2, the bottom views, is not used). The Teensy is not in the model, so the views show the empty H2 / H3 sockets; the parts list comes from the config | 1700 ASSY |
| `SCH_KC-01-1701_Control_Module_2026-11-28.pdf` | EasyEDA Pro schematic export (4 pages) | 1701 SCH |
| `Gerber_KC-01-1702_Control_Module_PCB_2026-11-28.zip` | Fabrication zip as sent to JLCPCB: 6-layer Gerbers, drill files (with routed slots), probe-test data | 1702 PWB, 1704 PLACE |
| `IPC-D-356A_KC-01-1702_Control_Module_PCB_2026-11-28.356a` | IPC-D-356A netlist (units: 0.0001 in) | reference |
| `BOM_KC-01-1702_Control_Module_PCB_2026-11-28.csv` | EasyEDA BOM export (UTF-16, tab separated) | 1703 BOM, 1704 PLACE |
| `PickAndPlace_KC-01-1702_Control_Module_PCB_2026-11-28.csv` | EasyEDA pick-and-place export (UTF-16, tab separated) | 1704 PLACE |
| `ProPrj_KC-01-1700_Control_Module_2026-11-28.epro2` | EasyEDA Pro project | reference |

## Rev D (review 2026-11-28)

The pre-order review changes (`Documents/Developer/Preorder_Review.md` 1.1–1.6, 1.14):

- DC1 is a 2-way 5.08 mm screw terminal, DORABO DB127S-5.08-2P-GN-S (C2997519): pin 1 +12 V (VIN), pin 2 GND, wire entry towards the top edge, silkscreen "+12V" / "GND". The Rev C jack (DC-042Z, 2.5 mm pin) did not take the 5.5 × 2.1 mm plug of the rear-panel Adafruit 5607, whose leads now go to DC1 in ferrules. No reverse-polarity protection, by decision.
- Soft latch: Q1 IRF4905STRLPBF (C2620, 20 mΩ; was IRF9540NS), Q2 2N7002 (C8545; was AO3400A, whose ±12 V gate rating the 12 V gate drive reached), hold resistor R12 150 kΩ (C22807; was 300 kΩ) with C9 10 µF: about 3.6 s to turn off (2.7–5 s).
- MPM3610 (U1): EN pulled up from 12 V through R2, 100 kΩ (C25741), instead of being tied to 12 V (the EN pin's internal clamp needs ≥ 55 kΩ); input capacitor C1 10 µF (C3039694; was 4.7 µF), output C2 22 µF (C45783; was 10 µF).
- Renumbering: the new R2 moves the Rev C R2–R26 to R3–R27. Capacitors, ICs, transistors, connectors and USB1–USB6 keep their numbers.
- Placement: the power block (Q1, Q2, U3, H1, D1, LED4, C6–C11, R4–R14) moved about 6.6–8 mm right to make room for the terminal; USB1–USB6 moved 1.0 mm in from the top edge, so their bodies now reach about to the edge rather than 1 mm past it.
- Checked against the Rev C board (IPC netlists, part by part on pads and nets): connectivity unchanged apart from the changes above; inner planes clear every plated hole of another net by at least 0.41 mm; every outer-layer pour carries one net. BOM matches the decisions.

## Board V2.1 (review 2026-11-25)

- Copper connectivity on all six layers: the 86 nets in the IPC netlist are single copper islands; no opens, no shorts.
- The EasyEDA design rule check passes (reported by the owner; no report file in the export).
- Layers: Top (components, signals, GND pour), GND plane, 3.3 V plane, GND plane, 12 V plane, Bottom (signals, GND pour); stackup JLC06161H-3313.
- Status LEDs, all 0805 at about 0.5 mA: LED1 green on V_12 (R8, 20 kΩ), LED2 blue on +5V (R9, 4.7 kΩ, new), LED3 red on V_3P3 (R10, 2.7 kΩ), LED4 yellow on VIN (R11, 20 kΩ) (Rev D numbers). V2.0 ran them at about 5 mA (2 kΩ / 270 Ω).
- Test port TP1 is a 1×5 header (C492404): 1 V_12, 2 GND, 3 +5V, 4 GND, 5 V_3P3, so no two rails are adjacent. Labelled on the silkscreen; LED1–LED3 sit below their pins.
- USB1–USB6 are vertical USB-A receptacles (C456015), numbered left to right under the silkscreen labels "USB 1" to "USB 6": hub 1 (U8) ports 1–3, then hub 2 (U9) ports 1–3 (nets D1–D6). VBUS is not connected, as on V2.0; each downstream Teensy runs from its own board.
- The shells of USB1–USB6, USBC1 and USBC2 are on GND (V2.0: not connected).
- USB-A pin order (1 VBUS, 2 D−, 3 D+, 4 GND) checked by the owner against the part's pinout (2026-11-25). The SHOU HAN drawing gives the 2.50 / 2.00 / 2.50 mm pin pitch, Ø0.95 mm pins and the pin row 2.30 mm from the shell legs, all matched by the footprint, but does not number the pins. Its shell legs are 12.90 ± 0.15 mm apart in 0.80 × 2.30 mm slots; the footprint has 13.10 mm and 0.80 × 2.20 mm.
- The connector bodies of USB1–USB6 reached about 1 mm past the top board edge in Rev C; Rev D moves the ports 1.0 mm in. Enclosure clearance for the ports, plugs and cables confirmed by the owner (2026-11-25).
- H4 (EMC2101 remote sensor header) is fitted; nothing plugs into it yet.
- Fan header CN1 is numbered 1 PWM, 2 TACH, 3 12 V, 4 GND, but its key ramp covers pins 2–4 with PWM outside, so a standard fan plug mates correctly (Hardware Reference §6.6).
- No fuse on the 12 V input, by decision (Hardware Reference §6.1).

## Regenerating the drawings

Requirements: Python 3 with `gerbonara`, `numpy` and `pillow`; Node.js with `playwright`
and its Chromium; `pdftocairo` (Poppler).

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1700/KC-01-1700.json --out <output folder>
```
