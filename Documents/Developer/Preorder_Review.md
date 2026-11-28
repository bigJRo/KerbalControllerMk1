# KCMk1 Pre-Order Design Review and Change Queue

**Organization:** Jeb's Controller Works  
**Author:** J. Rostoker  
**Version:** 1.0  
**Date:** 2026-11-28  
**Location:** `Documents/Developer/Preorder_Review.md`

A check of the main controller's electronics and hardware before components, boards and panels are
ordered, for errors that cannot be fixed in firmware afterwards. It covered power, board-to-board
connections, every board's schematic, BOM and assembly files, mechanical fit (with the full Fusion
assembly STEP, `Documents/Drawings/source/KC-01-1000/1000 STP.zip`), and the completeness of the
order lists. Vendor datasheets were read for the parts in question (2026-11-28).

This document is the change queue. Each item says what changes, where, and its status:
**Decided** (owner agreed, not yet applied), **Open** (owner to decide or check), **Done**.

---

## 1. Board changes (EasyEDA projects; re-export BOM, pick-and-place, Gerbers)

| # | Change | Boards | Kind | Status |
|---|---|---|---|---|
| 1.1 | **MPM3610 EN pull-up.** EN (pin 17) is tied straight to the 12 V input on every converter. The datasheet clamps EN with an internal 6.5 V Zener and requires a pull-up of at least 55 kΩ from VIN at 12 V (MPS uses 100 kΩ). Add 100 kΩ 0402 from IN to EN on each converter, close to pin 17, breaking the present EN–IN link: UNI-ROYAL 0402WGF1003TCE, LCSC C25741 (JLCPCB basic; 0603 alternative 0603WAF1003T5E, C25803). It sees about 5.5 V at 55 µA. | 1700 U1 (R2), 1730 U1, 1800 U2, 1810 U2, 1830 U2, 1840 U2, 1860 U2, 1870 U2 and U12, 1900 U1, 1910 U2 | Layout | Decided; 1700 done (R2, Rev D); 1910 done (R3, Rev C); 1900 done (R3, Rev C); 1730 done (R3, Rev B); 1800 done (R3$CBB15, Rev C draft); 1810 done (R3$CBB15, Rev C draft) |
| 1.2 | **MPM3610 input capacitor 4.7 µF → 10 µF.** Datasheet recommends 10 µF. Value swap on the existing 0805: Samsung CL21B106KAYQNNE, 10 µF 25 V X7R, LCSC C3039694 (the part the outputs use today; was CL21B475KAFNNNE C98195). Control Module C1, Panel Hub C1, Button / Wide Button / Joystick / 7-Segment / Dual Encoder C3, Throttle C3 and C15, 2in driver C1, 7in driver C4. | as 1.1 | BOM | Decided; 1700 done (C1, Rev D); 1910 done (C5, Rev C); 1900 done (C1, Rev C); 1730 done (C1, Rev B); 1800 done (C1$CBB15, Rev C draft); 1810 done (C1$CBB15, Rev C draft) |
| 1.2a | **MPM3610 output capacitor 10 µF → 22 µF.** Every 12 V reference circuit in the datasheet uses 22 µF; ceramics lose much of their value under DC bias. Value swap on the existing 0805: Samsung CL21A226MAQNNNE, 22 µF 25 V X5R, LCSC C45783 (JLCPCB basic; confirm in EasyEDA). 25 V also covers the Throttle's 9 V U12. Control Module C2, Panel Hub C2, Button / Wide Button / Joystick / 7-Segment / Dual Encoder C4, Throttle C4 and C16, 2in driver C2, 7in driver C5. | as 1.1 | BOM | Decided; 1700 done (C2, Rev D); 1910 done (C4, Rev C); 1900 done (C2, Rev C); 1730 done (C2, Rev B); 1800 done (C2$CBB15, Rev C draft); 1810 done (C2$CBB15, Rev C draft) |
| 1.3 | **Module I2C pull-ups 4.7 kΩ → 47 kΩ** (owner, 2026-11-28; was proposed as 33 kΩ). Each module's two pull-ups go from the PCA9306's 5 V side (SDA_MOD / SCL_MOD) to +5 V. The PCA9306 is a pass switch, so whatever pulls SDA_BUS low also sinks every module's pull-up: at 4.7 kΩ that is 13 × 1.06 mA + 0.7 mA ≈ 14.5 mA (I2C limit 3 mA at 0.4 V). Sized for every module position filled: 16 Panel Hub slots + 6 slots on the two 7in hub displays = 22 modules, 22 × 0.106 mA + 0.7 mA ≈ 3.0 mA (13 as planned: 2.1 mA). The pull-up only drives the last 3.3 → 3.5 V of a rising edge (the bus side and LTC4311 do the rest): about 90 ns with ~15 pF, inside 300 ns at 400 kHz. Part UNI-ROYAL 0402WGF4702TCE, LCSC C25792. Bench check: SDA_BUS low level under 0.4 V while the master polls. | 1800, 1810, 1830 R1$CBB15 / R2$CBB15; 1840, 1870 R1 / R2; 1860 R2 / R3 | BOM | Decided; 1800 done (R1/R2$CBB14, Rev C draft); 1810 done (R1/R2$CBB14, Rev C draft) |
| 1.4 | **Soft-latch FET Q1** IRF9540NS (117 mΩ, about 2 W at the typical 3.3 A) → **IRF4905STRLPBF** (20 mΩ, same D2PAK and pinout, VGS ±20 V), LCSC C2620 (JLCPCB extended, in stock). | 1700 Q1 | BOM | Done (1700 Rev D) |
| 1.5 | **Latch transistor Q2** AO3400A (gate held at 12 V = its ±12 V maximum) → **2N7002** (VGS ±20 V, VGS(th) 1.0–2.5 V, same SOT-23 pinout), LCSC C8545 (JLCPCB basic). With it, **R11 300 kΩ → 150 kΩ**, UNI-ROYAL 0603WAF1503T5E, LCSC C22807 (same 0603 footprint; JLCPCB basic) (C9 stays 10 µF) for a turn-off hold of about 3.6 s typical (2.7–5 s). Bench-check the hold time. | 1700 Q2, R11 (now R12) | BOM | Done (1700 Rev D: Q2, R12) |
| 1.6 | **Power input.** DC1 (DC-042Z-10A-2.5, 2.5 mm centre pin) does not take the 5.5 × 2.1 mm plug of the Adafruit 5607. Replace DC1 with a **fixed 2-way 5.08 mm screw terminal** (owner, 2026-11-28): DORABO DB127S-5.08-2P-GN-S, LCSC C2997519 (owner), the 2-way of the family already on the boards (6-way C2997523), 12 A, 26–14 AWG, placed by JLCPCB where DC1 is, on the rear edge with the wire entry facing the back panel, nets VIN and GND, silkscreen "+12V" / "GND". Cut the 5607's inner plug, crimp 0.5 mm² ferrules on its 20 AWG leads (red = centre = +12 V, black = shell = GND per Adafruit's drawing; the ALITOVE supply is centre-positive) and land them in the terminal. Besides the fit, this removes one 5 A-class barrel contact from the power path; the remaining one is the supply plug in the 5607 socket (hence the ≤ 5 A firmware warning). A pluggable terminal pair was considered and dropped: the input is wired once and the board already has about ten connectors to undo for removal. The straight run from the rear socket to DC1 is about 16 cm, so the 30 cm lead reaches with about 10 cm spare. **No reverse-polarity protection** (owner, 2026-11-28): the lead is internal once wired; check polarity with a meter before install. | 1700 DC1 | Layout | Done (1700 Rev D) |
| 1.7 | **Fader VR1 do-not-place.** VR1 (RSA0N11M9A0J, C470594) is in the JLCPCB BOM but is fitted after delivery and bought separately. | 1870 VR1 | BOM | Decided (owner to fix) |
| 1.8 | **Panel Hub V_3P3 bleeder.** V_3P3 (AP2112K U2 output, C4 / C5, slot pin 9 on P1–P8) has no DC load, and an LDO cannot sink: leakage back through the modules' PCA9306 references could lift it toward 4 V. Add a 10 kΩ 0402 bleeder from V_3P3 to GND near U2's output (owner, 2026-11-28): UNI-ROYAL 0402WGF1002TCE, LCSC C25744 (JLCPCB basic, already on the hub as R3). About 0.33 mA. | 1730 | Layout | Done (1730 Rev B, R5) |
| 1.9 | UPDI: 10 kΩ pull-up from UPDI to V_PROG. **Kept at 10 kΩ** (owner, 2026-11-28): the same value as Adafruit's ATtiny8x6 breakout (R1, UPDI to VCC), and the module test firmware is already loaded with SerialUPDI. Caveat: with the pull-up, the programmer's series resistor sets the low level (5 V × R / (R + 10 kΩ)); SerialUPDI with 470 Ω–1 kΩ gives 0.2–0.5 V, fine, but a jtag2updi with a 4.7 kΩ series resistor gives about 1.6 V, above the 0.2 × VDD input-low limit. Use SerialUPDI. | all ATtiny modules (R31, R11, R12, R17, R7) | BOM | Closed (owner) |
| 1.10 | Fader and joystick pots are fed from +5 V while the ADC reference is the ATtiny VDD (below 5 V by the BAT54C drop): the top 3–7 % of fader travel reads full scale. Feed the pot from the ATtiny VDD node. | 1870, 1830 | Layout | Open |
| 1.11 | **Power-button lamp feed fuse.** Switched 12 V (VCC) on H1-2 feeds the button lamp through the harness, unfused: a short there is fed by the full supply. Add a PTC F1 between VCC and H1-2 on the board, near H1 (owner, 2026-11-28: on the board, not in the harness): Yageo/Brightking SMD1206B010TF, LCSC C269108, 1206, 100 mA hold, 270 mA trip, 30 V, 100 A interrupt, about 1.5 Ω. The lamp draws tens of mA, under half the hold current even at the 85 °C derating (about 0.09 A). H1-2 becomes its own net; H1-1, H1-3 and H1-4 unchanged. | 1700 H1, new F1 | Layout | Done (1700 Rev D) |
| 1.12 | MAX7219 bulk capacitor C10 10 µF 0402 6.3 V keeps about 2–3 µF at 5 V; use 0805 25 V. | 1840 | BOM | Open (recommended) |
| 1.13 | Dual Encoder retention: the board has no mounting holes and hangs on the encoder bushings; no nuts are listed. Check a real PEC11R: with the body against the panel underside, at least about 2.5 mm of thread must stand above the 3 mm panel (the EasyEDA model shows 0.75 mm). If not, add mounting holes and standoffs. | 1860 | Check / layout | Open |
| 1.14 | **Control Module designators.** The owner renumbered the board in the 2026-11-28 export: the new EN pull-up is R2, so the released R2–R26 are now R3–R27 (shunt R4, hold resistor R12, LED resistors R8–R11, RST series R15, EXT_INT_1 pull-up R16 and series R17). Capacitors, ICs, Q1, Q2, DC1 and connectors keep their numbers. Connectivity is unchanged apart from the intended edits (terminal, EN pull-up, EN–IN link removed); the BOM matches 1.1–1.6. The same export also swapped USB2–USB5 (USB1, USB3, USB5, USB2, USB4, USB6 left to right), against the silkscreen labels "USB 1"–"USB 6": the owner renamed them back (export 2026-11-28, second). The ports also moved 1.0 mm in from the top edge and H1 6.6 mm right. Hardware Reference 2.16 updated. | 1700 | Layout / docs | Done (Rev D released) |

## 2. Panel production files (Ponoko) and panel drawings

| # | Change | Panels | Status |
|---|---|---|---|
| 2.1 | LA38 holes cut at Ø22.00; the model's LA38 selector has a Ø22.23 section in the hole. Cut Ø22.4 for the LA38 parts (A2 ABORT, B2 MODE). Test-fit the 22 mm metal buttons (A1 POWER, B2 STAGE) before changing theirs. The selector has no anti-rotation key. | A2, B2 (A1) | Open |
| 2.2 | 12 mm toggle holes cut 12.00 × 11.91 (D); the A2 toggles overlap them in the model. Consider Ø12.3. | A2, B2 | Open |
| 2.3 | 7-segment keycap cut-outs leave 0.10–0.28 mm to the moving caps; widen about 0.5 mm per side. Caps stand 0.7 mm proud. | A2, B2 | Open |
| 2.4 | Joystick screw holes on a 32.5 mm square; the stick's pattern is 33.0 mm (0.15 mm play per screw with M2 in Ø3.0). Consider 33.0 mm; compare the Ø38 cut-out with the joystick's recommended cut-out. | A2, B2 | Open |

The A1 and B1 lower brace-side mounting hole (27 mm up) is intentional: it matches the as-built internal brace (owner, 2026-11-27). Closed.

## 3. Purchased parts, drawings and manual (queued for the next revisions)

### 3.1 Cables

| Cable | Length | Qty | Runs |
|---|---|---|---|
| 50-pin IDC ribbon | 20 cm | 1 | Control Module CN2 → Extension CN1 (lies flat through the internal brace's bottom slot) |
| 34-pin IDC ribbon | 30 cm | 4 | Control Module CN3 → A2 Panel Hub; CN4 → A1 Information Display 1; Extension CN4 → B2 Panel Hub; CN5 → B1 Resource Display |
| 16-pin IDC ribbon | 10 cm | 2 | A2 hub P8 → Function Control (Wide); B2 hub P4 → Vehicle Control (Wide) |
| 16-pin IDC ribbon | 20 cm | 7 | A2 hub P1 Action, P2 2in Sys Info driver, P4 UI, P6 GPWS, P7 Translation; B2 hub P3 Time Control, P8 Rotation |
| 16-pin IDC ribbon | 30 cm | 7 | A2 hub P3 Throttle; B2 hub P2 Pre-Warp, P6 Stability, P7 AUX; B1 Resource P2 → Information Display 2, P4 → Dual Encoder; A1 Information Display 1 P2 → Annunciator |
| USB-A to micro-B | (B07G934SJ9) | 5 | Control Module USB1–USB5 → master Teensy and the four 7in drivers |
| Power-button lead | 22 AWG, 2×2 Dupont housing + 4 contacts | 1 | Control Module H1: 1 GND → lamp −, 2 switched 12 V → lamp +, 3 GND → switch common, 4 → switch NO |
| Terminal harnesses | hookup wire, ferrules | — | Panel Hub U4 (ABORT / STAGE + arming), Wide Button U16/U17 (Switch Groups 1 and 2), Throttle U5/U7/U8/U10/U11, Time Control CN2 (mode selector), Joystick U6 |

Lengths are the shortest of 10, 20 or 30 cm (owner: sizes easy to buy) that gives at least 3 cm of
slack on the routed estimate from the STEP and covers its high estimate. Every IDC cable must be pin 1 to pin 1, with
both sockets keyed the same way: a flipped 34- or 16-pin cable puts 12 V on GND.

Apply to: manual Table 21-1 (purchased parts, one row per length), Table 23-1 and the cable figures
(lengths); the panel assembly drawings' parts lists (KC-01-1020, 1030, 1040, 1050: the 16-pin
cables of each panel) and KC-01-1000 (50- and 34-pin cables, USB leads, power-button lead).

### 3.2 Other parts and corrections

| # | Change | Where | Status |
|---|---|---|---|
| 3.2.1 | Rear USB-C: Adafruit **4218** (USB-C round panel-mount extension, plug inside) ×2 replaces the 4259. Fits the Ø22.7 holes and panels up to 16 mm. Its wire list does not show the jack's second USB 2 pair: test both plug orientations on arrival. | KC-01-1000 item 13, Table 21-1, Table 23-1 | Decided; KC-01-1000 done (Rev B), manual pending |
| 3.2.2 | Joystick screws M2 × 12, 8 (4 per stick), missing from the lists. | KC-01-1030, 1050, Table 21-1 | Decided |
| 3.2.3 | Power input: the 5607's leads go to the new Control Module terminal (1.6); add the power lead to Table 23-1. | KC-01-1000, Table 23-1 | Done (manual r20, KC-01-1000 Rev B) |
| 3.2.4 | Board part changes 1.3–1.6 in the BOM drawings (KC-01-xxx3) and Table 21-1 where listed. | drawings, manual | After 1.x |
| 3.2.5 | Dual Encoder: the manual still shows KC-01-1850 encoder carriers on 5-pin H1/H2 (Tables 22-6, 23-1, Figure 23-3). In V2.1 both encoders are on the module and H1 is the UPDI header. Do not order 1850 boards for the controller. | manual | Decided |
| 3.2.6 | Button Module CN1 pin numbering is swapped in Hardware_Inventory §5.2 and Table 22-6 (netlist: CN1-1 BUTTON14, CN1-2 BUTTON15). | docs | Decided |
| 3.2.7 | PEC11R count: JLCPCB places 4 (2 on the Dual Encoder, 1 on each 7-Segment), not 2. | Table 21-1 | Decided |
| 3.2.8 | KC-01-1000 item 20 (Control Module Extension) should read assembled, not bare PWB. | KC-01-1000 | Done (KC-01-1000 Rev B) |
| 3.2.9 | Fan screws 6-32 × 2" are about 14 mm longer than needed; 1-3/4" suffices. 7-Segment panel screws M3 × 5 engage 2 mm; M3 × 6 gives 3 mm. | KC-01-1000, 1030, 1050 | Open |

## 4. Owner checks on parts

| Part | Check |
|---|---|
| PB26-13M RGB buttons | Common-anode LED needed (anode pin 1/5, cathodes 4/7/6). Diode-test one sample: meter + on pin 1, − on 4/6/7 lights each colour. Datasheet not found. |
| Adafruit 5607 | 20 AWG pigtail, no current rating published; the 5.5 × 2.1 barrel is about 5 A class. Set the firmware current warning at or below 5 A. Its thread is M22 (Adafruit also quotes 23 mm): the model's rear holes are Ø22.7 with 21.2 mm flats, which an M22 thread won't pass. If the built back panel doesn't already take it, file the flats off that hole; the nut holds the jack. |
| Adafruit 4218 | Test USB data with the plug both ways round. |
| Joystick JH-D400X-R4 | Mounting screws (M2?) and recommended cut-out vs Ø38. |
| PEC11R-4220F-S0024 | Bushing length (1.13). |

## 5. Firmware follow-ups (no ordering impact)

- Current caution and warning thresholds at or below 5 A (barrel connector).
- Configure the EMC2101 PWM output as open drain; keep the fan curve.
- Pull-ups or pull-downs on the master's INT and safe-line inputs so unplugged panels don't float; INPUT_PULLUP on the 7in drivers' RST input (pin 1) and AUDIO_BUSY (pin 11).
- Cap the DFPlayer volume and RGB brightness; limit fader stall time.
- Drop the I2C bus to 100 kHz if the bench rise times are poor.

## 6. Checked and closed

- **7in display connector:** H3 meets the display's JP1 face to face, so H3 pin N carries JP1 pin N±1; all 40 pins land on the right signal (datasheet, 2026-11-27).
- **2in display:** pin order matches; BLK is a logic-level backlight control.
- **CH334F hub:** load capacitors are built in (about 16 pF); RESET#, PSELF, OVCUR# and the LED pins may float; no upstream VBUS needed.
- **KEC 2N3904S:** pin order is E-B-C as the board has it (do not substitute another maker's SOT-23 2N3904).
- **EMC2101 FAN pin:** open drain, 5 V tolerant, 8 mA sink: fine for two fans.
- **Terminal blocks** (DB127S, DB125, KF127S): fixed screw terminals, no mating plugs.
- **Cables and signals:** 50-, 34- and 16-pin pinouts match on every board; INT map and I2C addresses free of conflicts; no 5 V signal reaches a Teensy or XIAO pin; no Teensy back-feeds USB.
- **Mechanical:** every panel hole pattern and cut-out lines up with its board; no board or enclosure clearance under 1 mm in the STEP; the panel braces clear everything, with Dual Lock fitted too (closest 1.4 mm); overall 862 × 412 × 218.7 mm.
- **Assembly files:** BOM and pick-and-place match one to one; Gerbers complete.
