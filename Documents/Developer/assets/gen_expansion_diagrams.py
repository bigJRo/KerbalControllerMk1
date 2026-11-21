#!/usr/bin/env python3
"""
gen_expansion_diagrams.py -- figures for the Expansion Module Specification.

Draws two figures for Documents/Developer/Expansion_Module_Spec.md:

  Expansion_Block_Diagram   §4.1: the enclosure connectors, the routing board,
                            the Teensy carrier, the display, keyboard and encoder,
                            and every power, I2C, USB and signal path between them
  Expansion_Keypad_Layout   §7.1: the 25 keys and the encoder on the 21 mm grid,
                            with matrix rows / columns and the LED chain order

Writes the SVGs next to this script, and with --png also PNGs rendered by the
pre-installed Chromium (Playwright) at --scale (default 2).

    python3 gen_expansion_diagrams.py            # SVGs only
    python3 gen_expansion_diagrams.py --png      # SVGs + PNGs

The drawing style follows gen_wiring_diagram.py. Change the figures here when
the spec's §4.1 or §7.1 changes.
"""
import argparse
import os
import subprocess
from xml.sax.saxutils import escape

FONT = "Liberation Sans, Arial, Helvetica, sans-serif"

KIND = {
    "pwr":  ("#c62828", 2.8, None,  "Power (V_12, 5 V, 3.3 V)"),
    "i2c":  ("#6a1b9a", 2.6, None,  "I2C (EXT bus, local bus)"),
    "usb":  ("#1565c0", 2.6, None,  "USB 2.0"),
    "sig":  ("#2e7d32", 2.4, None,  "Discrete signals (INT, RST, matrix, encoder)"),
    "slot": ("#78909c", 4.0, None,  "Module slot (16-pin IDC, Hardware Reference §12)"),
    "spare": ("#90a4ae", 1.8, "5 4", "Spare hub port"),
}


class Svg:
    def __init__(self, w, h):
        self.w, self.h, self.o = w, h, []

    def t(self, x, y, s, size=12, weight="normal", anchor="middle", fill="#212121", italic=False):
        st = ' font-style="italic"' if italic else ""
        self.o.append('<text x="%.1f" y="%.1f" font-size="%s" font-weight="%s" text-anchor="%s" fill="%s"%s>%s</text>'
                      % (x, y, size, weight, anchor, fill, st, escape(s)))

    def rect(self, x0, y0, x1, y1, fill="#ffffff", stroke="#263238", sw=1.6, r=6, dash=None):
        d = ' stroke-dasharray="%s"' % dash if dash else ""
        self.o.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="%s" fill="%s" stroke="%s" stroke-width="%s"%s/>'
                      % (x0, y0, x1 - x0, y1 - y0, r, fill, stroke, sw, d))

    def circle(self, cx, cy, r, fill, stroke="#263238", sw=1.6):
        self.o.append('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="%s" stroke="%s" stroke-width="%s"/>' % (cx, cy, r, fill, stroke, sw))

    def wire(self, pts, kind):
        col, w, dash, _ = KIND[kind]
        d = ' stroke-dasharray="%s"' % dash if dash else ""
        p = " ".join("%.1f,%.1f" % q for q in pts)
        self.o.append('<polyline points="%s" fill="none" stroke="%s" stroke-width="%s" stroke-linejoin="round" '
                      'stroke-linecap="round"%s/>' % (p, col, w, d))

    def lbl(self, x, y, s, kind, anchor="middle", size=11):
        self.t(x, y, s, size=size, weight="bold", anchor=anchor, fill=KIND[kind][0])

    def board(self, x0, y0, x1, y1, title, sub, fill="#ffffff"):
        self.rect(x0, y0, x1, y1, fill=fill)
        self.t(x0 + 14, y0 + 22, title, size=14, weight="bold", anchor="start")
        self.t(x0 + 14, y0 + 39, sub, size=11.5, anchor="start", fill="#455a64")

    def item(self, x0, y0, x1, y1, rows, size=11.5, fill="#f1f8e9", stroke="#558b2f"):
        self.rect(x0, y0, x1, y1, fill=fill, stroke=stroke, sw=1.3, r=5)
        n = len(rows)
        gap = size * 1.3
        cy = (y0 + y1) / 2 - (n - 1) * gap / 2 + size * 0.36
        for i, r in enumerate(rows):
            bold = i == 0
            self.t((x0 + x1) / 2, cy + i * gap, r, size=size + (1 if bold else 0),
                   weight="bold" if bold else "normal", fill="#212121" if bold else "#37474f")

    def legend(self, x, y, kinds, step=None):
        cx = x
        for k in kinds:
            col, w, dash, text = KIND[k]
            self.wire([(cx, y - 4), (cx + 34, y - 4)], k)
            self.t(cx + 42, y, text, size=11.5, anchor="start", fill="#37474f")
            cx += 60 + 6.4 * len(text)

    def finish(self):
        return ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" '
                'font-family="%s">\n<rect width="100%%" height="100%%" fill="#ffffff"/>\n%s\n</svg>\n'
                % (self.w, self.h, self.w, self.h, FONT, "\n".join(self.o)))


# --------------------------------------------------------------------------- block diagram

def block_diagram():
    s = Svg(1400, 880)
    s.t(30, 30, "KCMk1 Expansion Module: block diagram", size=19, weight="bold", anchor="start")
    s.t(30, 50, "Expansion Module Specification v0.5, §4.1. One landscape 7\" display; the master polls the expansion on the EXT bus.",
        size=12, anchor="start", fill="#607d8b")

    # Enclosure panel connectors
    s.rect(24, 96, 236, 790, fill="#fafafa", stroke="#90a4ae", sw=1.4, r=12, dash="8 5")
    s.t(36, 118, "Enclosure panel", size=14, weight="bold", anchor="start", fill="#37474f")
    s.t(36, 135, "cable from the main controller", size=11, anchor="start", fill="#607d8b")
    s.rect(44, 150, 216, 440, fill="#eceff1", stroke="#455a64", r=6)
    s.t(130, 172, "GX16-10", size=14, weight="bold")
    pins = [(196, "1–2", "V_12"), (216, "3–4", "GND"),
            (268, "5", "SDA_EXT"), (288, "6", "SCL_EXT"),
            (350, "7", "EXT_INT_1"), (370, "8", "EXT_INT_2"), (390, "9", "RST"),
            (424, "10", "spare")]
    for y, n, sig in pins:
        s.t(60, y, n, size=11.5, anchor="start", fill="#455a64")
        s.t(92, y, sig, size=11.5, anchor="start", weight="bold", fill="#263238")
    s.rect(44, 610, 216, 700, fill="#eceff1", stroke="#455a64", r=6)
    s.t(130, 636, "USB-C panel mount", size=13.5, weight="bold")
    s.t(130, 656, "from Hub 2 Port 4", size=11, fill="#455a64")
    s.t(130, 672, "(USBC2), ≤ 1 m cable", size=11, fill="#455a64")
    s.t(130, 688, "VBUS not connected", size=11, fill="#455a64")

    # Routing board EX10
    s.board(290, 96, 650, 790, "Expansion Routing Board", "KC-01-EX10", fill="#e3f2fd")
    s.item(310, 152, 630, 226, ["Power", "MPM3610: 12 V → 5 V", "AP2112K: 5 V → 3.3 V"])
    s.item(310, 250, 630, 306, ["SDA_EXT / SCL_EXT: passive", "LTC4311 and pull-ups are on the Ctrl Module"], size=11)
    s.item(310, 330, 630, 412, ["Signal conditioning", "EXT_INT_1: 47 Ω series", "EXT_INT_2: 4.7 kΩ pull-up",
                                "RST: 10 kΩ pull-up, 100 Ω + 100 nF"], size=11)
    s.item(310, 620, 450, 690, ["TUSB212", "USB 2.0 signal", "conditioner"], size=11)
    s.item(490, 520, 630, 770, ["CH334F hub", "", "", "", "", "", "", "", ""], size=11, fill="#ffffff", stroke="#1565c0")
    for y, p in ((560, "Port 1"), (620, "Port 2"), (680, "Port 3"), (740, "Port 4")):
        s.t(620, y + 4, p, size=11, anchor="end", fill="#1565c0", weight="bold")

    # GX16 / USB-C → EX10
    s.wire([(216, 206), (310, 206)], "pwr"); s.lbl(263, 199, "V_12, GND", "pwr", size=10)
    s.wire([(216, 278), (310, 278)], "i2c"); s.lbl(263, 271, "SDA, SCL", "i2c", size=10)
    s.wire([(216, 370), (310, 370)], "sig"); s.lbl(263, 363, "INT_1/2, RST", "sig", size=10)
    s.wire([(216, 655), (310, 655)], "usb"); s.lbl(263, 648, "USB 2.0 HS", "usb", size=10)
    s.wire([(450, 655), (470, 655), (470, 620), (490, 620)], "usb")

    # Teensy carrier EX23
    s.board(730, 96, 1090, 590, "Expansion Teensy Carrier", "KC-01-EX23")
    s.item(750, 152, 960, 450, ["Teensy 4.1", "EXT: I2C target, 0x15 (proposed)", "Local bus: I2C master",
                                "Keypad matrix scan", "Encoder A / B / push", "Key backlight (NeoPixel data)",
                                "Program logic", "Own Kerbal Simpit link"], size=11, fill="#fff8e1", stroke="#f9a825")
    s.item(985, 152, 1072, 262, ["Slot S1", "16-pin", "INT, RST"], size=10.5, fill="#eceff1", stroke="#455a64")
    s.item(750, 490, 880, 568, ["J1 / J2", "2 × 10-pin", "to keyboard"], size=10.5, fill="#eceff1", stroke="#455a64")
    s.item(910, 490, 1072, 568, ["Encoder header", "A, B, push"], size=10.5, fill="#eceff1", stroke="#455a64")
    s.wire([(960, 207), (985, 207)], "i2c")
    s.wire([(815, 450), (815, 490)], "sig")
    s.wire([(940, 450), (940, 490)], "sig")

    # EX10 → EX23
    s.wire([(630, 196), (750, 196)], "pwr"); s.lbl(690, 189, "5 V, 3.3 V", "pwr")
    s.wire([(630, 162), (676, 162), (676, 78), (1028, 78), (1028, 152)], "pwr")
    s.lbl(852, 71, "V_12 to the display slot (the display driver regulates its own)", "pwr")
    s.wire([(630, 278), (750, 278)], "i2c"); s.lbl(690, 271, "EXT I2C", "i2c")
    s.wire([(630, 370), (750, 370)], "sig"); s.lbl(690, 363, "INT_1/2, RST", "sig")
    s.wire([(630, 560), (750, 560)], "usb"); s.lbl(690, 553, "Port 1", "usb")

    # Display
    s.board(1140, 96, 1376, 340, "Display", "KC-01-1911 + 7\" panel")
    s.item(1156, 152, 1360, 326, ["7\" TFT Display Driver", "Teensy 4.1 + LT7683", "ER-TFT070A2-6-5633",
                                  "1024 × 600, landscape", "Capacitive touch", "Local I2C 0x16 (proposed)",
                                  "Own Kerbal Simpit link"], size=11, fill="#fff8e1", stroke="#f9a825")
    s.wire([(1072, 207), (1156, 207)], "slot")
    s.t(1114, 196, "12 V, I2C", size=10.5, weight="bold", fill="#546e7a")
    s.t(1114, 226, "INT, RST", size=10.5, weight="bold", fill="#546e7a")

    # USB port 2 → display, ports 3 / 4 spare
    s.wire([(630, 620), (668, 620), (668, 828), (1316, 828), (1316, 326)], "usb")
    s.lbl(992, 821, "Port 2: display's Simpit link", "usb")
    s.wire([(630, 680), (654, 680)], "spare"); s.wire([(630, 740), (654, 740)], "spare")
    s.t(662, 684, "spare", size=10, anchor="start", fill="#78909c"); s.t(662, 744, "spare", size=10, anchor="start", fill="#78909c")

    # Keyboard and encoder
    s.board(740, 640, 1010, 800, "Keyboard Matrix PCB", "KC-01-EX21")
    s.t(875, 712, "25 × Kailh Choc V1, 1N4148W per key", size=11, fill="#37474f")
    s.t(875, 730, "4 rows × 7 columns, 21 mm grid", size=11, fill="#37474f")
    s.t(875, 748, "25 × SK6812MINI-EA, chained", size=11, fill="#37474f")
    s.t(875, 766, "LED_0 (PROG) → LED_24", size=11, fill="#37474f")
    s.t(875, 784, "MBK keycaps (§7.1, Figure 7-1)", size=11, fill="#37474f")
    s.wire([(815, 568), (815, 640)], "sig")
    s.lbl(822, 600, "COL_0–6, ROW_0–3", "sig", anchor="start", size=10.5)
    s.lbl(822, 615, "LED_DIN, 5 V, GND", "sig", anchor="start", size=10.5)
    s.board(1040, 640, 1270, 800, "Encoder PCB", "KC-01-EX22")
    s.t(1155, 712, "Bourns PEC11R-4220F-S0024", size=11, fill="#37474f")
    s.t(1155, 730, "24 detents, push switch", size=11, fill="#37474f")
    s.t(1155, 748, "20 mm flatted shaft", size=11, fill="#37474f")
    s.wire([(990, 568), (990, 604), (1155, 604), (1155, 640)], "sig")
    s.lbl(1162, 624, "A, B, push", "sig", anchor="start", size=10.5)

    s.legend(30, 864, ["pwr", "i2c", "usb", "sig", "slot", "spare"])
    return s.finish()


# --------------------------------------------------------------------------- keypad

KEYS = [  # (row, col 1..7, legend lines, group)
    (0, 1, ["PROG"], "fn"), (0, 2, ["HOLD"], "fn"), (0, 3, ["MENU"], "fn"), (0, 4, ["INFO"], "fn"),
    (0, 5, ["MARK"], "fn"), (0, 6, ["PAGE"], "fn"),
    (1, 1, ["VERB"], "vn"), (1, 2, ["+"], "num"), (1, 3, ["7"], "num"), (1, 4, ["8"], "num"),
    (1, 5, ["9"], "num"), (1, 6, ["CLR"], "act"), (1, 7, ["ENTR"], "act"),
    (2, 1, ["NOUN"], "vn"), (2, 2, ["−"], "num"), (2, 3, ["4"], "num"), (2, 4, ["5"], "num"),
    (2, 5, ["6"], "num"), (2, 6, ["PRO"], "act"), (2, 7, ["RSET"], "act"),
    (3, 2, ["0"], "num"), (3, 3, ["1"], "num"), (3, 4, ["2"], "num"), (3, 5, ["3"], "num"),
    (3, 6, ["KEY", "REL"], "act"),
]
GROUP = {
    "fn":  ("#37474f", "#ffffff", "Function row"),
    "vn":  ("#1565c0", "#ffffff", "VERB / NOUN"),
    "num": ("#eceff1", "#212121", "Digits and sign"),
    "act": ("#ffe0b2", "#4e342e", "Entry and action keys"),
}


def keypad():
    P = 104          # 21 mm grid pitch, ≈ 4.95 px per mm
    CAP = 90         # keycap face (MBK caps are 17.5 × 16.5 mm; drawn square for clarity)
    X0, Y0 = 150, 196
    s = Svg(1200, 720)
    s.t(30, 30, "KCMk1 Expansion Module: keypad layout", size=19, weight="bold", anchor="start")
    s.t(30, 50, "Expansion Module Specification v0.5, §7.1. Seen from the front. 25 Kailh Choc V1 keys and one encoder on a 21 mm grid.",
        size=12, anchor="start", fill="#607d8b")

    # Board outline (KC-01-EX21, 147 × 100 mm, drawn to the same scale as the grid)
    bw, bh = 147 * P / 21, 100 * P / 21
    by0 = Y0 + 2 * P - bh / 2
    s.rect(X0, by0, X0 + bw, by0 + bh, fill="#f5f7f8", stroke="#90a4ae", sw=1.4, r=10, dash="8 5")

    for c in range(7):
        cx = X0 + c * P + P / 2
        s.t(cx, by0 - 34, "Col %d" % (c + 1), size=12, weight="bold", fill="#37474f")
        s.t(cx, by0 - 16, "COL_%d" % c, size=11, fill="#2e7d32", weight="bold")
    for r in range(4):
        cy = Y0 + r * P + P / 2
        s.t(X0 - 20, cy - 2, "Row %d" % r, size=12, weight="bold", anchor="end", fill="#37474f")
        s.t(X0 - 20, cy + 14, "ROW_%d" % r, size=11, anchor="end", fill="#2e7d32", weight="bold")

    led = 0
    for row, col, leg, g in KEYS:
        x = X0 + (col - 1) * P + (P - CAP) / 2
        y = Y0 + row * P + (P - CAP) / 2
        bg, fg, _ = GROUP[g]
        s.rect(x, y, x + CAP, y + CAP, fill=bg, stroke="#263238", sw=1.4, r=9)
        size = 30 if len(leg[0]) == 1 else (18 if len(leg) == 1 else 16)
        if len(leg) == 1:
            s.t(x + CAP / 2, y + CAP / 2 + size * 0.36 - 4, leg[0], size=size, weight="bold", fill=fg)
        else:
            s.t(x + CAP / 2, y + CAP / 2 - 8, leg[0], size=size, weight="bold", fill=fg)
            s.t(x + CAP / 2, y + CAP / 2 + 12, leg[1], size=size, weight="bold", fill=fg)
        s.t(x + CAP - 6, y + CAP - 7, "LED %d" % led, size=9.5, anchor="end",
            fill="#b0bec5" if g in ("fn", "vn") else "#78909c")
        led += 1

    # Encoder at row 0, col 7
    ex, ey = X0 + 6 * P + P / 2, Y0 + P / 2
    s.circle(ex, ey, 40, "#cfd8dc", sw=1.6)
    s.circle(ex, ey, 26, "#78909c", stroke="#455a64", sw=1.2)
    s.t(ex, ey + 5, "ENC", size=14, weight="bold", fill="#ffffff")
    s.t(ex, ey + 56, "turn + push", size=10, fill="#455a64")

    # Empty positions on row 3
    for col in (1, 7):
        x = X0 + (col - 1) * P + (P - CAP) / 2
        y = Y0 + 3 * P + (P - CAP) / 2
        s.rect(x, y, x + CAP, y + CAP, fill="none", stroke="#b0bec5", sw=1.2, r=9, dash="5 4")
        s.t(x + CAP / 2, y + CAP / 2 + 4, "no key", size=11, fill="#90a4ae")

    # Dimension: 21 mm pitch
    dy = by0 + bh + 26
    s.o.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#607d8b" stroke-width="1.2"/>' % (X0, dy, X0 + P, dy))
    for xx in (X0, X0 + P):
        s.o.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#607d8b" stroke-width="1.2"/>' % (xx, dy - 6, xx, dy + 6))
    s.t(X0 + P / 2, dy + 18, "21 mm", size=11, fill="#607d8b", weight="bold")
    s.t(X0 + bw / 2, dy + 18, "Keyboard Matrix PCB KC-01-EX21, 147 × 100 mm (dashed)", size=11, fill="#607d8b")

    # Legend and notes
    lx = X0 + bw + 50
    s.t(lx, by0 + 4, "Key groups", size=14, weight="bold", anchor="start")
    for i, g in enumerate(("fn", "vn", "num", "act")):
        bg, fg, name = GROUP[g]
        yy = by0 + 24 + i * 34
        s.rect(lx, yy, lx + 24, yy + 24, fill=bg, stroke="#263238", sw=1.2, r=4)
        s.t(lx + 34, yy + 17, name, size=12.5, anchor="start", fill="#37474f")
    notes = [
        ("Matrix", True),
        ("One 1N4148W diode per key.", False),
        ("Columns driven low by the Teensy;", False),
        ("rows read with pull-ups (§7.2).", False),
        ("", False),
        ("Backlight", True),
        ("SK6812MINI-EA per key, reverse", False),
        ("mounted. Chained LED_0 (PROG) to", False),
        ("LED_24 (KEY REL), left to right,", False),
        ("top to bottom (§7.3).", False),
        ("", False),
        ("Keycaps", True),
        ("MBK caps with custom legends", False),
        ("(FKcaps). Spacing check: E-17.", False),
        ("Encoder role: E-20.", False),
    ]
    yy = by0 + 196
    for text, head in notes:
        if text:
            s.t(lx, yy, text, size=13 if head else 12, weight="bold" if head else "normal",
                anchor="start", fill="#212121" if head else "#455a64")
        yy += 18
    return s.finish()


def render_png(svg_path, png_path, w, h, scale):
    js = """
const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: %d, height: %d }, deviceScaleFactor: %s });
  await p.goto('file://%s');
  await p.screenshot({ path: '%s' });
  await b.close();
})();""" % (w, h, scale, svg_path, png_path)
    subprocess.run(["node", "-e", js], check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png", action="store_true")
    ap.add_argument("--scale", type=float, default=2.0)
    ap.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    a = ap.parse_args()
    for name, fn in (("Expansion_Block_Diagram", block_diagram), ("Expansion_Keypad_Layout", keypad)):
        svg = fn()
        sp = os.path.join(a.out, name + ".svg")
        open(sp, "w").write(svg)
        if a.png:
            w = int(svg.split('width="')[1].split('"')[0])
            h = int(svg.split('height="')[1].split('"')[0])
            render_png(sp, os.path.join(a.out, name + ".png"), w, h, a.scale)
        print("wrote", sp)


if __name__ == "__main__":
    main()
