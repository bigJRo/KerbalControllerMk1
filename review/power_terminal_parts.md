# 12 V pluggable power input – terminal block parts (KC-01-1702 Control Module)

Requirement: a 2-position, 5.08 mm pitch pluggable terminal block. The board header is a
right-angle THT part placed by JLCPCB at the rear board edge, with the plug opening facing the
back panel. The mating plug is bought separately: screw clamp, wire entry in line with the
mating direction. Load is about 3.3 A typical and 5.8 A peak, so a 10 A or higher rating is wanted.

Data was read on 2026-11-28 from LCSC product pages (`www.lcsc.com/product-detail/<C>.html`),
JLCPCB part pages (`jlcpcb.com/partdetail/<C>`) and the LCSC datasheet PDFs. The PDFs are
drawings without a text layer, so they were rendered to images and read by eye. Stock figures
are LCSC stock on that date. The JLCPCB pages did not show a stock count without logging in,
and that count is not confirmed here.

## Recommended pair – Cixi Kefa Elec KF2EDG series

| | Board header | Mating plug |
|---|---|---|
| MPN | **KF2EDGRC-5.08-2P** (closed ends) | **KF2EDGK-5.08-2P-AA** |
| LCSC C number | **C441236** | **C440880** |
| Type | Right-angle (horizontal-entry) THT header, "Board Side/Socket – Closed" | Free-hanging plug, M2.5 screw clamp |
| Current / voltage | Datasheet: UL 10 A / 300 V; IEC 15 A / 320 V. LCSC lists 10 A / 300 V. | Datasheet: UL 10 A / 300 V; IEC 15 A / 320 V. LCSC/JLCPCB list 15 A / 320 V. |
| Wire range | – | 24–12 AWG, 2.5 mm² max. 20 AWG / 0.5 mm² with ferrule is inside the range. Strip 7–8 mm, screw M2.5, 0.4 N·m max. |
| JLCPCB assembly | **Extended** part; JLCPCB assembly type is **Wave Soldering (THT)**, PCBA "Economic and Standard" | Bought separately (JLCPCB also lists it as Extended / Wave Soldering) |
| Stock (LCSC) | 19,530 | 33,655 |
| Datasheet | https://datasheet.lcsc.com/datasheet/pdf/141d518b1842a2786c5a906b6c6b08b5.pdf | https://datasheet.lcsc.com/datasheet/pdf/976c61d45369c406685d0a029c29bc60.pdf |
| Confidence | Read on page and in the datasheet | Ratings and wire range read on page and in the datasheet. Mating is inferred (see below). |

Header footprint, read from the KF2EDGRC-5.08 drawing:
- Pitch 5.08 ±0.03 mm. PCB hole Ø1.60 (+0.10/0) mm. Pin length 4.1 mm, pin section about 1.0 mm.
- The hole row is 1.30 mm from the rear (pin-side) edge of the housing, per the PCB-layout view.
- Housing is 8.40 mm deep (board plane, mating direction) × 12.20 mm tall. Width is P×5.08 + 1.60, which is about 11.8 mm for 2 positions.
- **Closed-ended:** the housing has end walls (the "C" suffix and the +1.60 mm width). There are **no locking ears or screw flange**. The plug is held by friction only.

Mating:
- The plug drawing is titled "KF2EDGK-5.08" and the header drawing "KF2EDGRC-5.08". Both are Kefa's KF2EDG 5.08 mm pluggable family, with the same pitch and the same 10 A / 300 V UL and 15 A / 320 V IEC rating blocks.
- **The header datasheet does not name its mating plug, and the plug datasheet does not name the header.** The mate follows from the shared KF2EDG series designation, not from a statement on a page. Also check the plug width (P×5.08) against the closed header's inner opening.
- LCSC has no listing for a plain "KF2EDGK-5.08-2P" without a suffix. C440880 carries "-AA", and its LCSC/JLCPCB descriptions and drawing match the plain KF2EDGK-5.08. The meaning of "-AA" could not be confirmed. A similar plug, KF2EDGKC-5.08-2P (C2922831, M2.5 screw, 15 A / 320 V, 12–24 AWG), uses the same LCSC datasheet, but only 84 were in stock.
- Wire entry: the drawing shows the screws on the front face and the wire entry on the side opposite the mating tongue. The wire is therefore in line with the mating direction, as required.

Open-ended option: **KF2EDGR-5.08-2P, C441204**. It has the same footprint (Ø1.60 holes, 1.30 mm row offset, 8.40 × 12.20 mm housing) and P×5.08 width. LCSC lists it at 15 A / 300 V, and its datasheet gives the same UL 10 A / IEC 15 A as the RC. It is Extended on JLCPCB with Wave Soldering, and 34,830 were in stock. Use it if the closed ends cause a clearance problem. Otherwise the closed RC is preferred because its end walls keep the plug aligned.
Datasheet: https://datasheet.lcsc.com/datasheet/pdf/50dc2d683fb501b638dea6cde12ab25d.pdf

## Alternative pair – DEGSON 2EDG series

| | Board header | Mating plug |
|---|---|---|
| MPN | **2EDGRC-5.08-02P-14-100A(H)** (closed ends) | **2EDGKD-5.08-02P-14-00A(H)** |
| LCSC C number | **C669314** | **C691854** |
| Type | Right-angle THT header, "Board Side/Socket – Closed" | Free-hanging screw-clamp plug, one latch hook on the 2P version |
| Current / voltage | LCSC/JLCPCB: 15 A / 300 V. Datasheet: UL 300 V / 15 A; VDE column 320 V (the current figure was hard to read in the rendered drawing). | Datasheet: UL 300 V / 10 A; IEC 320 V / 20 A. LCSC: 10 A / 300 V. |
| Wire range | – | 26–12 AWG, 0.2–2.5 mm², strip 9–10 mm. This covers 20 AWG / 0.5 mm² with a ferrule. |
| JLCPCB assembly | **Extended**; assembly type **Wave Soldering (THT)** | Bought separately (JLCPCB lists it as Extended, "SMT Assembly" – irrelevant here) |
| Stock (LCSC) | 4,615 | 357 |
| Datasheet | https://datasheet.lcsc.com/datasheet/pdf/83dfd937f69e87a59ec3580aaeb4838d.pdf | https://datasheet.lcsc.com/datasheet/pdf/40219ca853a3e486784fd1a441c6f225.pdf |
| Confidence | Read on page and in the datasheet | Ratings read in the datasheet. Mating is **inferred, lower confidence.** |

Header footprint, read from the 2EDGRC-5.08 drawing:
- Pitch 5.08 mm. PCB hole Ø1.70 mm. Hole row 2.10 mm from the rear housing edge. End hole 3.54 mm from the housing end.
- Pin section 1.00 mm square. Housing 8.60 mm deep × 12.00 mm tall. Width 5.08×N + 2.00, which is 12.16 mm for 2P.
- Closed ends, **no locking ears or flange**.

Mating caveats:
- An exact DEGSON "2EDGK-5.08-02P-14-00A(H)" could not be found on LCSC. The 2EDGKD is the closest plain screw plug in stock.
- Neither datasheet names the other part. The pairing rests on DEGSON's 2EDG 5.08 family naming.
- The 2EDGKD has a latch hook. **Whether the 2EDGRC header has a matching latch catch is not confirmed.** It may sit loosely or interfere with the closed end wall.
- 2EDGKM (C699500, 15 A) is a screw-flange plug meant for a flange header and does not suit the 2EDGRC.
- Because of these caveats, the Kefa pair is the recommendation.

## Mounting orientation for the footprint

- On both right-angle headers the pins exit the **rear** of the housing and bend down into the board. The plug opening is on the **opposite face**.
- Place the footprint with the **hole row inboard** and the **open mating face flush with, or slightly overhanging, the rear board edge**. The plug then inserts parallel to the board, moving from the back panel toward the board.
- The plug's wires leave straight back through the panel.
- Edge to hole-row distance is about 8.40 − 1.30 = **7.1 mm** for the Kefa part and about 8.60 − 2.10 = **6.5 mm** for the DEGSON part, when the housing front sits flush with the edge.
- Confirm these against the EasyEDA/JLCPCB footprint before layout. The offsets were read from the drawings by eye.
- A 10 A UL rating against a 5.8 A peak leaves margin. Use 0.5 mm² (20 AWG) or heavier wire with ferrules, sized to the 7–8 mm (Kefa) or 9–10 mm (DEGSON) strip length.
