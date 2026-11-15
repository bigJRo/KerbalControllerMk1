#!/usr/bin/env python3
"""
gen_block_diagrams.py -- functional block diagrams for the TIM (Apollo AOH style).

  KCMk1_Block_Power   12 V input, soft latch, INA228, the V_12 bus and every load group,
                      with the 12 V input current of each (Power Budget v1.2)
  KCMk1_Block_Data    USB tree to the five Kerbal Simpit links, the master's I2C bus with
                      every address and INT line, RST, the local touch and DFPlayer links,
                      and the expansion interface

Same family as the wiring figures (gen_wiring_diagram.py helpers, palette and fonts),
drawn at 100 units per inch: 650 x 730 units prints 6.5 x 7.3 in, so the smallest text
(10 units) prints at 7.2 pt.

Sources: Hardware_Reference.md §3, §5-§10; Hardware_Inventory.md §2-§7; Power_Budget.md
v1.2; I2C_Protocol_Specification.md §2, §3, §15; Software/KCMk1_Master/config.h,
module_bus.cpp, board.cpp; Software/Displays (KCMk1_SystemConfig.h, Annunciator README);
Software/Displays/KCMk1_SysInfo/AAA_Config.ino (supply limits).

    python3 gen_block_diagrams.py            # SVGs
    python3 gen_block_diagrams.py --png      # SVGs + PNGs (Chromium, --scale 4)
"""
import argparse
import os
import subprocess
from xml.sax.saxutils import escape

import gen_wiring_diagram as g
from gen_wiring_diagram import KIND, t, lines, rect

# Line classes used here that the wiring sheet does not have
KIND.setdefault("i2c", ("#37474f", 3.2, None, "34-pin panel cable: 12 V, I2C bus, RST, INT lines, safe line"))
KIND.setdefault("ser", ("#00838f", 2.0, None, "Serial link"))
KIND.setdefault("bus", ("#c62828", 4.0, None, "V_12 main bus"))

INK = "#263238"
GREY = "#455a64"
MUTED = "#607d8b"


# ------------------------------------------------------------------ shared helpers
def start(w, h, title, sub=None):
    g.out.clear()
    g.out.append('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" '
                 'font-family="%s">' % (w, h, w, h, g.FONT))
    g.out.append('<defs>')
    for name, col in (("k", INK), ("r", KIND["pwr"][0]), ("b", KIND["usb"][0]), ("p", KIND["exp"][0]),
                      ("t", KIND["ser"][0]), ("g", "#2e7d32")):
        g.out.append('<marker id="ah%s" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" '
                     'markerUnits="userSpaceOnUse" orient="auto"><path d="M0,0 L10,5 L0,10 z" fill="%s"/></marker>'
                     % (name, col))
    g.out.append('</defs>')
    g.out.append('<rect width="100%" height="100%" fill="#ffffff"/>')
    t(w / 2, 22, title, size=14, weight="bold", fill=INK)
    if sub:
        t(w / 2, 37, sub, size=10, fill=MUTED)


def finish():
    g.out.append("</svg>")
    return "\n".join(g.out)


def line(pts, col=INK, w=1.6, dash=None, head=None):
    """Polyline; head = marker letter (k, r, b, p, t, g) for an arrowhead at the end."""
    d = ' stroke-dasharray="%s"' % dash if dash else ""
    m = ' marker-end="url(#ah%s)"' % head if head else ""
    p = " ".join("%.1f,%.1f" % q for q in pts)
    g.out.append('<polyline points="%s" fill="none" stroke="%s" stroke-width="%s" stroke-linejoin="round"%s%s/>'
                 % (p, col, w, d, m))


def kline(pts, kind, head=False):
    col, w, dash, _ = KIND[kind]
    hk = {"pwr": "r", "bus": "r", "usb": "b", "exp": "p", "ser": "t", "i2c": "k"}.get(kind, "k")
    line(pts, col, w, dash, hk if head else None)


def block(x0, y0, x1, y1, title, rows=(), fill="#ffffff", stroke=INK, size=10, tsize=11, gap=12.5,
          anchor="middle", pad=8, sw=1.4, r=5, dash=None, tfill=INK):
    """A box with a bold title and rows below it. rows: str or (str, {t kwargs})."""
    rect(x0, y0, x1, y1, fill=fill, stroke=stroke, sw=sw, r=r, dash=dash)
    x = (x0 + x1) / 2 if anchor == "middle" else x0 + pad
    y = y0 + tsize + 5
    if title:
        t(x, y, title, size=tsize, weight="bold", anchor=anchor, fill=tfill)
        y += gap + 1
    for r_ in rows:
        if isinstance(r_, tuple):
            t(x, y, r_[0], size=size, anchor=anchor, **{"fill": GREY, **r_[1]})
        else:
            t(x, y, r_, size=size, anchor=anchor, fill=GREY)
        y += gap
    return y


def group(x0, y0, x1, y1, name, desc, tint="#f7f9fb"):
    rect(x0, y0, x1, y1, fill=tint, stroke="#90a4ae", sw=1.3, r=9, dash="7 4")
    t(x0 + 8, y0 + 15, name, size=11.5, weight="bold", anchor="start", fill="#37474f")
    if desc:
        t(x0 + 8, y0 + 28, desc, size=10, anchor="start", fill=MUTED)


def table_rows(x, y, cols, rows, size=10, gap=12.5, bold_last=False):
    """cols: list of (x offset, anchor). rows: tuples of cell strings."""
    for i, row in enumerate(rows):
        bold = bold_last and i == len(rows) - 1
        for (dx, anc), cell in zip(cols, row):
            if cell:
                t(x + dx, y + i * gap, cell, size=size, anchor=anc, weight="bold" if bold else "normal",
                  fill=INK if bold else GREY)
    return y + len(rows) * gap


def render(jobs, scale):
    """jobs: list of (svg_path, png_path, w, h). One Chromium for all of them."""
    items = ",".join("['%s','%s',%d,%d]" % j for j in jobs)
    js = """
const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch();
  for (const [s, p, w, h] of [%s]) {
    const pg = await b.newPage({ viewport: { width: w, height: h }, deviceScaleFactor: %s });
    await pg.goto('file://' + s);
    await pg.screenshot({ path: p });
    await pg.close();
  }
  await b.close();
})();""" % (items, scale)
    subprocess.run(["node", "-e", js], check=True)


# ------------------------------------------------------------------ power
PW, PH = 650, 730


def power():
    start(PW, PH, "POWER DISTRIBUTION — FUNCTIONAL BLOCK DIAGRAM",
          "Figures beside each load: 12 V input current in mA, typical / peak (Power Budget v1.2)")
    R = KIND["pwr"][0]

    # Input chain, left to right
    Y0, Y1 = 50, 124
    block(10, Y0, 104, Y1, "12 V DC SUPPLY", ["external", "10 A rated", "(120 W)"], dash="5 3", fill="#fafafa",
          tsize=10.5)
    block(120, Y0, 226, Y1, "12 V DC IN", ["rear panel · DC1", "2.5 mm barrel", "jack"], fill="#fbe9e7",
          stroke="#8d6e63")
    block(242, Y0, 420, Y1, "SOFT-LATCH SWITCH", ["Control Module", "IRF9540N P-MOSFET load switch",
                                                  "analog RC hold-to-off timing",
                                                  "no Teensy pin (master cannot see it)"])
    block(436, Y0, 640, Y1, "INA228 POWER MONITOR", ["Control Module · I2C 0x40",
                                                     "15 mΩ shunt, latch output → 12 V bus",
                                                     "read by the master 4 times a second"])
    for xa, xb in ((104, 120), (226, 242), (420, 436)):
        kline([(xa, 87), (xb - 1, 87)], "pwr", head=True)

    # Power button into the latch
    block(256, 144, 406, 194, "POWER BUTTON (A1)", ["momentary, to Ctrl Module H1",
                                                    "short press ON · hold 3 s OFF"])
    kline([(331, 144), (331, Y1 + 1)], "pwr", head=True)

    # Supply limits, under the INA228
    block(458, 144, 640, 206, "SUPPLY LIMITS (INA228)", ["on the Systems Information Display",
                                                         "CAUTION: below 11.5 V or above 7 A",
                                                         "WARNING: below 11.0 V or above 8 A",
                                                         ], size=10, fill="#fffde7", stroke="#f9a825")
    t(549, 220, "The master also warns below 11.0 V", size=10, fill=MUTED)

    # USB isolation note
    block(10, 144, 226, 206, "USB: DATA ONLY, NO POWER", ["VBUS not connected on USBC1 (host)",
                                                          "and USBC2; Micro-USB VCC not connected.",
                                                          "Every board runs from the 12 V bus."],
          stroke=KIND["usb"][0], fill="#e3f2fd", size=10)

    # V_12 main bus
    BY = 238
    kline([(446, Y1), (446, BY)], "pwr")
    kline([(18, BY), (634, BY)], "bus")
    t(18, BY - 6, "V_12 MAIN BUS · 12 V", size=11, weight="bold", anchor="start", fill=R)
    t(260, BY - 6, "12 V LED (green) on the Control Module", size=10, anchor="start", fill=MUTED)

    # ---- left column: A1, A2, Control Module, fan 1 (feeder at x = 18)
    LX0, LX1, FX = 30, 318, 18
    ys = []

    def feed(y, fx=FX, x0=LX0):
        kline([(fx, y), (x0 - 1, y)], "pwr", head=True)

    cols = [(8, "start"), (280, "end")]
    # Panel A1
    a0 = 252
    rect(LX0, a0, LX1, a0 + 112, fill="#ffffff", stroke=INK, sw=1.4, r=5)
    t(LX0 + 8, a0 + 15, "PANEL A1 · Ctrl Module CN4 (34-pin)", size=11, weight="bold", anchor="start", fill=INK)
    t(LX0 + 8, a0 + 28, "7\" TFT Display Driver, each: MPM3610 5 V (1.2 A) →", size=10, anchor="start", fill=GREY)
    t(LX0 + 8, a0 + 40, "TFT panel VDD, Teensy 4.1; AP2112K 3.3 V → logic", size=10, anchor="start", fill=GREY)
    table_rows(LX0, a0 + 56, cols, [("Information Display 1 (panel hub)", "227 / 246"),
                                    ("Annunciator (slot P2)", "227 / 246"),
                                    ("  PAM8302A amp → SPEAKER 1 · DFPlayer → SPEAKER 2", ""),
                                    ("Power button", "negligible"),
                                    ("A1 total", "454 / 492")], bold_last=True)
    feed(a0 + 20)
    ys.append(a0 + 20)

    # Panel A2
    b0 = a0 + 118
    rect(LX0, b0, LX1, b0 + 152, fill="#ffffff", stroke=INK, sw=1.4, r=5)
    t(LX0 + 8, b0 + 15, "PANEL A2 · Ctrl Module CN3 (34-pin)", size=11, weight="bold", anchor="start", fill=INK)
    t(LX0 + 8, b0 + 28, "Panel Hub: MPM3610 5 V + AP2112K 3.3 V (slot V_3P3,", size=10, anchor="start", fill=GREY)
    t(LX0 + 8, b0 + 40, "safe switch). Each ATtiny816 module makes its own 5 V.", size=10, anchor="start", fill=GREY)
    c2 = [(8, "start"), (136, "end"), (150, "start"), (280, "end")]
    table_rows(LX0, b0 + 56, c2, [("Action Control", "192 / 419", "UI Control", "192 / 419"),
                                  ("Function Control", "197 / 424", "GPWS", "112 / 228"),
                                  ("Translation (THC)", "27 / 57", "ABORT lamp", "7 / 12"),
                                  ("Throttle", "608 / 1,147", "Sys Info Display", "TBD")])
    t(LX0 + 8, b0 + 109, "Throttle: own MPM3610 9 V → L293D → motorized lever", size=10, anchor="start", fill=GREY)
    t(LX0 + 8, b0 + 121, "Sys Info Display (XIAO RP2350): own 12 → 5 → 3.3 V,", size=10, anchor="start", fill=GREY)
    t(LX0 + 8, b0 + 133, "not yet measured", size=10, anchor="start", fill=GREY)
    t(LX0 + 8, b0 + 146, "A2 total", size=10, weight="bold", anchor="start", fill=INK)
    t(LX0 + 280, b0 + 146, "TBD", size=10, weight="bold", anchor="end", fill=INK)
    feed(b0 + 20)
    ys.append(b0 + 20)

    # Control Module local loads
    c0 = b0 + 158
    rect(LX0, c0, LX1, c0 + 64, fill="#fff8e1", stroke="#8d6e63", sw=1.3, r=5)
    t(LX0 + 8, c0 + 15, "CONTROL MODULE · local 5 V and 3.3 V", size=11, weight="bold", anchor="start", fill=INK)
    t(LX0 + 8, c0 + 28, "Master Teensy 4.1 · USB hub (2 × CH334F, V5 pin)", size=10, anchor="start", fill=GREY)
    t(LX0 + 8, c0 + 40, "LTC4311 I2C accelerators · 3.3 V LED (red)", size=10, anchor="start", fill=GREY)
    t(LX0 + 8, c0 + 54, "Overhead: master, soft latch, misc.", size=10, anchor="start", fill=GREY)
    t(LX0 + 280, c0 + 54, "50 / 50", size=10, anchor="end", fill=GREY)
    feed(c0 + 20)
    ys.append(c0 + 20)

    # Fan 1 + EMC2101
    f0 = c0 + 70
    rect(LX0, f0, LX1, f0 + 40, fill="#ffffff", stroke=KIND["fan"][0], sw=1.3, r=5)
    t(LX0 + 8, f0 + 15, "FAN 1 · Ctrl Module CN1 · 12 V direct", size=11, weight="bold", anchor="start", fill=INK)
    t(LX0 + 8, f0 + 29, "PWM + tach. EMC2101 (0x4C) drives FAN_PWM to both fans", size=10, anchor="start",
      fill=GREY)
    feed(f0 + 20)
    ys.append(f0 + 20)
    kline([(FX, BY), (FX, ys[-1])], "pwr")

    # ---- right column: the Extension and what it feeds (feeder at x = 342)
    RX0, RX1, RFX = 352, 640, 342
    kline([(RFX, BY), (RFX, 256)], "pwr")
    e0 = 256
    rect(RX0 - 14, e0, RX1, e0 + 42, fill="#eceff1", stroke=INK, sw=1.4, r=5)
    t(RX0 - 6, e0 + 15, "50-PIN INTERBOARD → CTRL MODULE EXTENSION", size=11, weight="bold", anchor="start",
      fill=INK)
    t(RX0 - 6, e0 + 30, "CN2 → CN1 · V_12 on pins 39–50 · passive, straight through", size=10,
      anchor="start", fill=GREY)
    rys = []

    def rfeed(y):
        kline([(RFX, y), (RX0 - 1, y)], "pwr", head=True)

    # Panel B1
    p0 = e0 + 54
    rect(RX0, p0, RX1, p0 + 88, fill="#ffffff", stroke=INK, sw=1.4, r=5)
    t(RX0 + 8, p0 + 15, "PANEL B1 · Extension CN5 (34-pin)", size=11, weight="bold", anchor="start", fill=INK)
    t(RX0 + 8, p0 + 28, "7\" drivers regulate as on Panel A1", size=10, anchor="start", fill=GREY)
    table_rows(RX0, p0 + 44, cols, [("Resource Display (panel hub)", "227 / 246"),
                                    ("Information Display 2 (slot P2)", "227 / 246"),
                                    ("Dual Encoder (slot P4)", "10 / 14"),
                                    ("B1 total", "464 / 506")], bold_last=True)
    rfeed(p0 + 20)
    rys.append(p0 + 20)

    # Panel B2
    q0 = p0 + 98
    rect(RX0, q0, RX1, q0 + 116, fill="#ffffff", stroke=INK, sw=1.4, r=5)
    t(RX0 + 8, q0 + 15, "PANEL B2 · Extension CN4 (34-pin)", size=11, weight="bold", anchor="start", fill=INK)
    t(RX0 + 8, q0 + 28, "Panel Hub and modules regulate as on Panel A2", size=10, anchor="start", fill=GREY)
    table_rows(RX0, q0 + 44, c2, [("Stability Control", "192 / 419", "Time Control", "192 / 419"),
                                  ("Vehicle Control", "197 / 424", "AUX Control", "192 / 419"),
                                  ("Pre-Warp Time", "100 / 201", "Rotation (RHC)", "27 / 57"),
                                  ("STAGE lamp", "7 / 12", "", "")])
    t(RX0 + 8, q0 + 106, "B2 total", size=10, weight="bold", anchor="start", fill=INK)
    t(RX0 + 280, q0 + 106, "907 / 1,951", size=10, weight="bold", anchor="end", fill=INK)
    rfeed(q0 + 20)
    rys.append(q0 + 20)

    # Fan 2
    r0 = q0 + 126
    rect(RX0, r0, RX1, r0 + 40, fill="#ffffff", stroke=KIND["fan"][0], sw=1.3, r=5)
    t(RX0 + 8, r0 + 15, "FAN 2 · Extension CN3 · 12 V direct", size=11, weight="bold", anchor="start", fill=INK)
    t(RX0 + 8, r0 + 29, "PWM only (FAN_PWM on 50-pin pin 1), no tach", size=10, anchor="start", fill=GREY)
    rfeed(r0 + 20)
    rys.append(r0 + 20)

    # Expansion
    s0 = r0 + 50
    rect(RX0, s0, RX1, s0 + 40, fill="#fafafa", stroke=KIND["exp"][0], sw=1.3, r=5, dash="5 3")
    t(RX0 + 8, s0 + 15, "EXPANSION · Extension CN2 → GX16-10", size=11, weight="bold", anchor="start", fill=INK)
    t(RX0 + 8, s0 + 29, "V_12 on GX16-10 pins 1–2 · reserved, no unit fitted", size=10, anchor="start",
      fill=GREY)
    rfeed(s0 + 20)
    rys.append(s0 + 20)
    kline([(RFX, e0 + 42), (RFX, rys[-1])], "pwr")
    kline([(RFX, BY), (RFX, e0)], "pwr")

    # ---- system summary
    z0 = 650
    rect(10, z0, 640, 724, fill="#f7f9fb", stroke=INK, sw=1.4, r=5)
    t(18, z0 + 16, "SYSTEM LOAD AT 12 V · Panels A1 + A2 + B1 + B2 (interim)", size=11, weight="bold",
      anchor="start", fill=INK)
    sc = [(8, "start"), (230, "end"), (350, "end")]
    t(10 + 230, z0 + 30, "Typical", size=10, weight="bold", anchor="end", fill=INK)
    t(10 + 350, z0 + 30, "Peak", size=10, weight="bold", anchor="end", fill=INK)
    table_rows(10, z0 + 43, sc, [("Load", "3.30 A · 39.5 W", "5.82 A · 69.8 W"),
                                 ("Supply rating", "10.0 A · 120 W", "10.0 A · 120 W"),
                                 ("Headroom", "6.70 A · 80.5 W", "4.18 A · 50.2 W")])
    lines(380, z0 + 30, ["Interim: the Sys Info Display is not yet",
                         "measured; its former RA4M1 figure is",
                         "used. Fans are not itemized."], size=10, anchor="start", fill=MUTED, gap=12.5)
    return finish()


# ------------------------------------------------------------------ data / I2C
DW, DH = 650, 730


def data():
    start(DW, DH, "DATA AND I2C — FUNCTIONAL BLOCK DIAGRAM",
          "Addresses are 7-bit I2C; INT lines are the default map the master learns at boot")
    U, S, E = KIND["usb"][0], KIND["ser"][0], KIND["exp"][0]
    LX0, LX1 = 8, 200          # A1, A2
    CX0, CX1 = 236, 414        # centre column
    RX0, RX1 = 450, 642        # B1, B2

    # Host PC and USB hub
    block(CX0, 46, CX1, 104, "HOST PC", ["KSP + Kerbal Simpit v2.4.0", "5 Simpit links · 115200 Bd",
                                         "10 COM ports (2 per Teensy)"], fill="#fafafa", dash="5 3")
    kline([(325, 104), (325, 125)], "usb", head=True)
    t(331, 118, "USB-C · USBC1 (rear)", size=10, anchor="start", fill=U)
    block(CX0, 126, CX1, 190, "USB HUB · CONTROL MODULE", ["2 × CH334F cascaded · USB 2.0 HS",
                                                          "VBUS not connected (data only)",
                                                          "USB6 spare · USBC2 → expansion"],
          fill="#e3f2fd", stroke=U)

    # ---- A1 group
    group(LX0, 46, LX1, 296, "PANEL A1", "left display panel")
    an = (16, 80, 192, 168)
    block(*an, "ANNUNCIATOR", [("0x10 · INT_9 · in Info Disp 1 slot P2", {}),
                                ("FT5316 touch 0x38 (local bus)", {}),
                                ("DFPlayer Mini on Serial2, 9600 Bd", {"fill": S}),
                                ("  → SPEAKER 2 (GPWS voice)", {"fill": S}),
                                ("tone → PAM8302A → SPEAKER 1", {"fill": S})],
          size=10, gap=12.3, anchor="start", pad=7)
    i1 = (16, 178, 192, 284)
    block(*i1, "INFORMATION DISPLAY 1", ["0x12 · INT_12 · panel hub", "CN1 ← Ctrl Module CN4",
                                         "FT5316 touch 0x38 (local bus)", "slots P3, P4 free (INT_10, 11)",
                                         "safe terminal empty (INT_SAFE2)"],
          size=10, gap=12.3, anchor="start", pad=7, fill="#e3f2fd")
    # slot link Info 1 P2 -> Annunciator P1
    line([(24, 178), (24, 168)], KIND["slot"][0], KIND["slot"][1])
    t(30, 175.5, "16-pin slot", size=8.5, anchor="start", fill=KIND["slot"][0])
    # USB2 / USB3
    kline([(CX0, 136), (an[2] + 1, 136)], "usb", head=True)
    t(214, 132, "USB2", size=10, fill=U, weight="bold")
    kline([(CX0, 182), (i1[2] + 1, 182)], "usb", head=True)
    t(214, 178, "USB3", size=10, fill=U, weight="bold")

    # ---- B1 group
    group(RX0, 46, RX1, 296, "PANEL B1", "right display panel")
    i2 = (458, 80, 634, 150)
    block(*i2, "INFORMATION DISPLAY 2", ["0x13 · B_INT_9", "in Resource Display slot P2",
                                         "FT5316 touch 0x38 (local bus)"],
          size=10, gap=12.3, anchor="start", pad=7)
    rd = (458, 158, 634, 244)
    block(*rd, "RESOURCE DISPLAY", ["0x11 · B_INT_12 · panel hub", "CN1 ← Extension CN5",
                                    "FT5316 touch 0x38 (local bus)", "slot P3 free (B_INT_10)",
                                    "safe terminal empty (B_INT_SAFE2)"],
          size=10, gap=12.3, anchor="start", pad=7, fill="#e3f2fd")
    de = (458, 252, 634, 290)
    block(*de, None, [("DUAL ENCODER  0x2D · B_INT_11", {"fill": INK, "weight": "bold"}),
                      "in Resource Display slot P4"], size=10, gap=12, anchor="start", pad=7)
    for ya, yb in ((150, 158), (244, 252)):
        line([(626, ya), (626, yb)], KIND["slot"][0], KIND["slot"][1])
    kline([(CX1, 136), (i2[0] - 1, 136)], "usb", head=True)
    t(436, 132, "USB4", size=10, fill=U, weight="bold")
    kline([(CX1, 182), (rd[0] - 1, 182)], "usb", head=True)
    t(436, 178, "USB5", size=10, fill=U, weight="bold")

    # ---- master
    M0, M1 = 214, 330
    kline([(325, 190), (325, M0 - 1)], "usb", head=True)
    t(331, 206, "USB1", size=10, anchor="start", fill=U, weight="bold")
    block(CX0, M0, CX1, M1, "MASTER · TEENSY 4.1", ["Control Module · I2C bus master",
                                                   ("Serial: debug / console", {"fill": U}),
                                                   ("SerialUSB1: Kerbal Simpit", {"fill": U}),
                                                   "Wire 18/19 → LTC4311 → I2C BUS",
                                                   "Wire1 17/16 → LTC4311 → EXT",
                                                   "24 INT inputs · 4 safe-line inputs",
                                                   "RST out, pin 2 (10 ms low pulse)"],
          fill="#fff8e1", stroke="#8d6e63", size=10, gap=12.3)

    # panel cables
    kline([(CX0, 262), (LX1, 262), (LX1, 262)], "i2c")
    kline([(LX1 + 8, 262), (i1[2] + 1, 262)], "i2c", head=True)
    t(214, 258, "CN4", size=10, weight="bold", fill=INK)
    kline([(CX0, 318), (LX1 + 1, 318)], "i2c", head=True)
    t(218, 314, "CN3", size=10, weight="bold", fill=INK)

    # INA228 / EMC2101 on the bus
    block(CX0, 346, 316, 384, "INA228", ["0x40 · 12 V"], size=10, tsize=10.5, gap=12)
    block(334, 346, CX1, 384, "EMC2101", ["0x4C · fans"], size=10, tsize=10.5, gap=12)
    kline([(276, M1), (276, 345)], "i2c", head=True)
    kline([(374, M1), (374, 345)], "i2c", head=True)

    # Extension
    X0, X1 = 404, 452
    kline([(325, M1), (325, X0 - 1)], "i2c", head=True)
    block(CX0, X0, CX1, X1, "CTRL MODULE EXTENSION", ["50-pin CN2 → CN1 · passive"], size=10, gap=12,
          fill="#eceff1")
    kline([(CX1, 414), (430, 414), (430, 232), (rd[0] - 1, 232)], "i2c", head=True)
    t(434, 300, "CN5", size=10, weight="bold", anchor="start", fill=INK)
    kline([(CX1, 440), (RX0 - 1, 440)], "i2c", head=True)
    t(432, 436, "CN4", size=10, weight="bold", fill=INK)

    # Expansion
    kline([(325, X1), (325, 471)], "exp", head=True)
    block(CX0, 472, CX1, 568, "EXPANSION · reserved", ["Ext CN2 → GX16-10 (rear panel):",
                                                      "EXT I2C bus (master is target)",
                                                      "EXT_INT_1 → master pin 33",
                                                      "EXT_INT_2 ← master pin 32",
                                                      "RST, 12 V · USB: USBC2 (rear)",
                                                      ("idle in firmware, no unit fitted", {"italic": True})],
          size=10, gap=12.3, stroke=E, dash="5 3", fill="#fafafa")

    # ---- A2 / B2
    def slots(x0, x1, y0, name, desc, rows, safe, ac=128, ic=158):
        group(x0, y0, x1, 568, name, desc)
        cols = [(8, "start"), (26, "start"), (ac, "start"), (ic, "start")]
        y = y0 + 46
        t(x0 + 8, y, "slot", size=9, fill=MUTED, anchor="start")
        t(x0 + 26, y, "module", size=9, fill=MUTED, anchor="start")
        t(x0 + ac, y, "addr", size=9, fill=MUTED, anchor="start")
        t(x0 + ic, y, "INT", size=9, fill=MUTED, anchor="start")
        y += 15
        for i, r_ in enumerate(rows):
            yy = y + i * 19
            rect(x0 + 4, yy - 12, x1 - 4, yy + 5, fill="#ffffff" if r_[1] else "#fafafa",
                 stroke="#b0bec5", sw=0.8, r=2, dash=None if r_[1] else "3 2")
            for (dx, anc), cell in zip(cols, r_):
                if cell:
                    t(x0 + dx, yy, cell, size=10, anchor=anc,
                      fill=(INK if r_[1] else "#90a4ae"), italic=not r_[1] and cell.startswith("—"))
        yy = y + len(rows) * 19 + 4
        for k, s in enumerate(safe):
            t(x0 + 8, yy + k * 12.5, s, size=10, anchor="start", fill="#2e7d32" if k < 2 else GREY)

    slots(LX0, LX1, 302, "PANEL A2", "Panel Hub on Ctrl Module CN3",
          [("P1", "Action Control", "0x22", "INT_1"), ("P2", "Sys Info Display", "0x14", "INT_2"),
           ("P3", "Throttle", "0x2C", "INT_3"), ("P4", "UI Control", "0x20", "INT_4"),
           ("P5", "", "", "INT_5"), ("P6", "GPWS", "0x2A", "INT_6"),
           ("P7", "Translation (THC)", "0x29", "INT_7"), ("P8", "Function Control", "0x21", "INT_8")],
          ["ABORT + arming switch → INT_SAFE", "(pin 31, active-high)",
           ("Sys Info: XIAO RP2350, no USB,"), "RST not connected"])
    slots(RX0, RX1, 302, "PANEL B2", "Panel Hub on Extension CN4",
          [("P1", "", "", "B_INT_1"), ("P2", "Pre-Warp Time", "0x2B", "B_INT_2"),
           ("P3", "Time Control", "0x25", "B_INT_3"), ("P4", "Vehicle Control", "0x24", "B_INT_4"),
           ("P5", "", "", "B_INT_5"), ("P6", "Stability Control", "0x23", "B_INT_6"),
           ("P7", "AUX Control", "0x26", "B_INT_7"), ("P8", "Rotation (RHC)", "0x28", "B_INT_8")],
          ["STAGE + arming switch →", "B_INT_SAFE (pin 35, active-high)"], ac=114, ic=144)

    # ---- legend / notes
    L0 = 580
    t(10, L0 + 12, "LEGEND", size=11, weight="bold", anchor="start", fill="#37474f")
    keys = [("usb", "USB 2.0 (Kerbal Simpit and debug serial)"), ("i2c", "34-pin panel cable (12 V, I2C, RST, INT, safe)"),
            ("slot", "16-pin module slot (12 V, I2C, RST, one INT)"), ("ser", "Serial / audio (Annunciator)"),
            ("exp", "Expansion (reserved)")]
    for i, (k, txt) in enumerate(keys):
        col, row = i % 2, i // 2
        x = 76 + col * 300
        y = L0 + 8 + row * 16
        c, w, dash, _ = KIND[k]
        d = ' stroke-dasharray="%s"' % dash if dash else ""
        g.out.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="%s"%s/>'
                     % (x, y, x + 30, y, c, w, d))
        t(x + 38, y + 3.5, txt, size=10, anchor="start")
    lines(10, L0 + 66, [
        "I2C BUS: 400 kHz, pulled up to 3.3 V at the master's LTC4311; every module and display is a target. "
        "INT: one active-low",
        "line per module, no wired-OR. Safe lines are active-high. RST reboots the four 7\" display drivers "
        "(and the expansion);",
        "the ATtiny816 modules and the Sys Info Display do not use it. Each 7\" driver: I2C target on Wire2 "
        "(24/25), INT out pin 0,",
        "RST in pin 1; its FT5316 touch panel is on its own software I2C bus (pins 4/5), not the module bus. "
        "On each Teensy the",
        "first USB serial port is debug, the second its Kerbal Simpit port (Section 7.3)."],
        size=10, anchor="start", fill=GREY, gap=12.5)
    return finish()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png", action="store_true")
    ap.add_argument("--scale", type=float, default=4.0)
    ap.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    a = ap.parse_args()
    jobs = []
    for name, fn, w, h in (("KCMk1_Block_Power", power, PW, PH), ("KCMk1_Block_Data", data, DW, DH)):
        sp = os.path.join(a.out, name + ".svg")
        open(sp, "w").write(fn())
        print("wrote", sp)
        jobs.append((sp, os.path.join(a.out, name + ".png"), w, h))
    if a.png:
        render(jobs, a.scale)
        for j in jobs:
            print("wrote", j[1])


if __name__ == "__main__":
    main()
