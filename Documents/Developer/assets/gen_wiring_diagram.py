#!/usr/bin/env python3
"""
gen_wiring_diagram.py -- the Kerbal Controller Mk1 internal wiring diagram.

Draws every internal cable of the controller (TIM chapter "Wiring Diagram",
Table "Internal Cable Connections") as one figure: the four panels, the Control
Module and its Extension, the module slots, the USB tree, power, fans, the
expansion connectors and the discrete panel wiring. Sources: Hardware Inventory
§2-§7 and Hardware Reference §3.6, §5, §7, §10 (both from the schematics).

Writes KCMk1_Wiring_Diagram.svg next to this script, and with --png also a
PNG rendered by the pre-installed Chromium (Playwright) at --scale (default 3,
5400 x 3750 px).

    python3 gen_wiring_diagram.py            # SVG only
    python3 gen_wiring_diagram.py --png      # SVG + PNG

This single sheet is for viewing on screen. The print figures in the TIM (overview,
Side A, Side B) are drawn by gen_wiring_figures.py from the same helpers; change
both when the wiring changes.
"""
import argparse
import os
from xml.sax.saxutils import escape

W, H = 1800, 1250
FONT = "Liberation Sans, Arial, Helvetica, sans-serif"

# Cable / wire classes: colour, width, dash, legend text
KIND = {
    "idc":   ("#37474f", 5.0, None,  "Ribbon cable, IDC (50-pin interboard, 34-pin panel)"),
    "slot":  ("#78909c", 3.0, None,  "Module in a hub slot (16-pin IDC)"),
    "usb":   ("#1565c0", 2.4, None,  "USB (Micro-USB to each Teensy 4.1; USB-C at the rear)"),
    "pwr":   ("#c62828", 2.6, None,  "12 V power / power-button wiring"),
    "wire":  ("#2e7d32", 2.2, None,  "Panel switch wiring to screw terminals"),
    "fan":   ("#e65100", 2.2, "7 4", "Fan cable (4-pin)"),
    "exp":   ("#6a1b9a", 2.4, "7 4", "Expansion (reserved, no unit fitted)"),
    "spk":   ("#00838f", 2.2, None,  "Speaker lead (2-pin)"),
}

out = []


def t(x, y, s, size=12, weight="normal", anchor="middle", fill="#212121", italic=False):
    st = ' font-style="italic"' if italic else ""
    out.append('<text x="%.1f" y="%.1f" font-size="%s" font-weight="%s" text-anchor="%s" fill="%s"%s>%s</text>'
               % (x, y, size, weight, anchor, fill, st, escape(s)))


def lines(x, y, rows, size=12, gap=None, **kw):
    gap = gap or size * 1.22
    for i, r in enumerate(rows):
        if isinstance(r, tuple):
            t(x, y + i * gap, r[0], size=size, **{**kw, **r[1]})
        else:
            t(x, y + i * gap, r, size=size, **kw)


def rect(x0, y0, x1, y1, fill="#ffffff", stroke="#263238", sw=1.6, r=6, dash=None):
    d = ' stroke-dasharray="%s"' % dash if dash else ""
    out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="%s" fill="%s" stroke="%s" stroke-width="%s"%s/>'
               % (x0, y0, x1 - x0, y1 - y0, r, fill, stroke, sw, d))


def wire(pts, kind, label=None, at=0, off=(0, -6), anchor="middle", lsize=11):
    col, w, dash, _ = KIND[kind]
    d = ' stroke-dasharray="%s"' % dash if dash else ""
    p = " ".join("%.1f,%.1f" % q for q in pts)
    out.append('<polyline points="%s" fill="none" stroke="%s" stroke-width="%s" stroke-linejoin="round" '
               'stroke-linecap="round"%s/>' % (p, col, w, d))
    if label:
        (x0, y0), (x1, y1) = pts[at], pts[at + 1]
        mx, my = (x0 + x1) / 2 + off[0], (y0 + y1) / 2 + off[1]
        if x0 == x1:            # vertical segment: label to the side
            anchor = "start" if off[0] >= 0 else "end"
            mx = x0 + (off[0] if off[0] else 7)
            my = (y0 + y1) / 2 + 4 + off[1] + 6
        t(mx, my, label, size=lsize, anchor=anchor, fill=col, weight="bold")


def tab(x, y, name, side, fill="#eceff1"):
    """Connector designator tab on a board edge. side: top/bottom/left/right."""
    w = max(26, 7.2 * len(name) + 8)
    h = 16
    if side in ("top", "bottom"):
        x0, x1 = x - w / 2, x + w / 2
        y0, y1 = (y - h / 2, y + h / 2)
    else:
        x0, x1 = (x - w / 2, x + w / 2)
        y0, y1 = y - h / 2, y + h / 2
    rect(x0, y0, x1, y1, fill=fill, stroke="#455a64", sw=1, r=3)
    t(x, y + 4, name, size=10.5, weight="bold", fill="#263238")


def board(x0, y0, x1, y1, title, sub=None, hub=False, extra=None, tsize=13.5, ty=20):
    """ty: title baseline below the top edge (larger when tabs sit on the top edge)."""
    rect(x0, y0, x1, y1, fill="#e3f2fd" if hub else "#ffffff")
    cx = (x0 + x1) / 2
    t(cx, y0 + ty, title, size=tsize, weight="bold")
    if sub:
        lines(cx, y0 + ty + 16, sub if isinstance(sub, list) else [sub], size=11, fill="#455a64")
    if extra:
        extra(cx)


def panel(x0, y0, x1, y1, name, desc, tint):
    rect(x0, y0, x1, y1, fill=tint, stroke="#90a4ae", sw=1.4, r=12, dash="8 5")
    t(x0 + 14, y0 + 22, name, size=15, weight="bold", anchor="start", fill="#37474f")
    t(x0 + 14 + 10.6 * len(name) + 8, y0 + 22, desc, size=12, anchor="start", fill="#607d8b")


def item(x0, y0, x1, y1, rows, size=11):
    rect(x0, y0, x1, y1, fill="#f1f8e9", stroke="#558b2f", sw=1.3, r=5)
    n = len(rows)
    cy = (y0 + y1) / 2 - (n - 1) * size * 0.62 + 4
    lines((x0 + x1) / 2, cy, rows, size=size, gap=size * 1.24)


def ctag(x, y, rows, kind, size=9.5):
    """Cable tag centred on (x, y): pin count / part number, in the cable's colour."""
    col = KIND[kind][0]
    wpx = max(len(r) for r in rows) * size * 0.56 + 10
    hpx = len(rows) * size * 1.18 + 5
    rect(x - wpx / 2, y - hpx / 2, x + wpx / 2, y + hpx / 2, fill="#ffffff", stroke=col, sw=1.1, r=3)
    y0 = y - hpx / 2 + size + 1.5
    for i, r in enumerate(rows):
        t(x, y0 + i * size * 1.18, r, size=size, weight="bold" if i == 0 else "normal",
          fill=col if i == 0 else "#546e7a")


TBD = "P/N TBD"


def module(cx, y0, name, addr, pn, empty=False, w=86, h=88):
    x0, x1 = cx - w / 2, cx + w / 2
    if empty:
        rect(x0, y0, x1, y0 + h, fill="#fafafa", stroke="#b0bec5", sw=1.2, dash="4 3")
        t(cx, y0 + h / 2 - 2, "empty", size=11, fill="#90a4ae", italic=True)
        t(cx, y0 + h / 2 + 12, addr, size=9.5, fill="#90a4ae")
        return
    rect(x0, y0, x1, y0 + h, fill="#ffffff")
    for i, s in enumerate(name):
        t(cx, y0 + 16 + i * 13.5, s, size=11.5, weight="bold")
    t(cx, y0 + h - 22, addr, size=10, fill="#455a64")
    t(cx, y0 + h - 8, pn, size=10, weight="bold", fill="#37474f")


def build():
    out.append('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" '
               'font-family="%s">' % (W, H, W, H, FONT))
    out.append('<rect width="100%" height="100%" fill="#ffffff"/>')
    t(W / 2, 30, "KERBAL CONTROLLER Mk1 — INTERNAL WIRING", size=17, weight="bold", fill="#263238")

    # ---------------------------------------------------------------- panels
    AX0, AX1, BX0, BX1 = 18, 882, 918, 1782
    panel(AX0, 52, AX1, 350, "PANEL A1", "KC-01-1020 · left display panel", "#f7f9fb")
    panel(BX0, 52, BX1, 350, "PANEL B1", "KC-01-1040 · right display panel", "#f7f9fb")
    panel(AX0, 680, AX1, 1112, "PANEL A2", "KC-01-1030 · left input panel", "#f7f9fb")
    panel(BX0, 680, BX1, 1112, "PANEL B2", "KC-01-1050 · right input panel", "#f7f9fb")

    # ---------------------------------------------------------------- Ctrl Module
    CX0, CY0, CX1, CY1 = 300, 420, 800, 640
    board(CX0, CY0, CX1, CY1, "CONTROL MODULE", ["KC-01-1700 · power latch · INA228 · EMC2101",
                                                 "USB hub (2 × CH334F)"], ty=36)
    rect(560, 520, 772, 590, fill="#fff8e1", stroke="#8d6e63", sw=1.3, r=5)
    t(666, 544, "Master Teensy 4.1", size=12.5, weight="bold")
    t(666, 562, "I2C master · Simpit host link", size=10.5, fill="#5d4037")
    t(666, 578, "reads INT / safe lines, drives RST", size=10.5, fill="#5d4037")
    tab(400, 555, "USB1", "left")
    wire([(417, 555), (560, 555)], "usb")
    ctag(488, 555, ["5-pin Micro-USB", TBD], "usb")
    t(400, 610, "USB6: spare", size=10.5, fill="#78909c", italic=True)
    t(666, 626, "LEDs: 3.3 V (red), 12 V (green)", size=10.5, fill="#78909c")

    # ---------------------------------------------------------------- Extension
    EX0, EY0, EX1, EY1 = 1080, 440, 1520, 640
    board(EX0, EY0, EX1, EY1, "CONTROL MODULE EXTENSION", ["KC-01-1710 · passive, every signal straight through"],
          ty=34)
    lines((EX0 + EX1) / 2, 600, ["Carries the B-side lines: B_INT_1–12, B_INT_SAFE(2), I2C bus,",
                                 "RST, the expansion bus (SDA/SCL_EXT, EXT_INT_1/2), FAN_PWM, 12 V"],
          size=10.5, fill="#78909c")
    wire([(CX1, 560), (EX0, 560)], "idc")
    tab(CX1, 560, "CN2", "right")
    tab(EX0, 560, "CN1", "left")
    ctag(940, 560, ["50-pin IDC", "interboard", TBD], "idc")

    # ---------------------------------------------------------------- Rear panel (flanks)
    rect(22, 408, 182, 652, fill="#fbe9e7", stroke="#8d6e63", sw=1.4, r=8)
    t(102, 428, "REAR PANEL", size=13, weight="bold", fill="#5d4037")
    rect(1640, 470, 1780, 640, fill="#fbe9e7", stroke="#8d6e63", sw=1.4, r=8)
    t(1710, 490, "REAR PANEL", size=13, weight="bold", fill="#5d4037")

    def rp(x0, x1, y, rows):
        rect(x0, y - 20, x1, y + 20, fill="#ffffff", stroke="#8d6e63", sw=1.2, r=4)
        lines((x0 + x1) / 2, y - 3, rows, size=10.5, gap=13)

    rp(30, 174, 460, ["12 V DC IN", ("2.5 mm barrel, ≥10 A", {"fill": "#6d4c41"})])
    rp(30, 174, 512, ["USB-C · HOST PC", ("panel mount", {"fill": "#6d4c41"})])
    rp(30, 174, 564, ["USB-C · EXPANSION", ("panel mount, reserved", {"fill": "#6d4c41"})])
    rp(30, 174, 616, ["FAN 1", ("120 mm PWM, tach", {"fill": "#6d4c41"})])
    for y, kind, name, tag in ((460, "pwr", "DC1", ["2-pin 12 V", TBD]),
                               (512, "usb", "USBC1", ["24-pin USB-C", TBD]),
                               (564, "exp", "USBC2", ["24-pin USB-C", TBD]),
                               (616, "fan", "CN1", ["4-pin fan", TBD])):
        wire([(174, y), (CX0, y)], kind)
        tab(CX0, y, name, "left")
        ctag(232, y, tag, kind)

    rp(1650, 1770, 520, ["FAN 2", ("120 mm PWM", {"fill": "#6d4c41"})])
    rp(1650, 1770, 596, ["GX16-10", ("expansion, reserved", {"fill": "#6d4c41"})])
    wire([(EX1, 520), (1650, 520)], "fan")
    tab(EX1, 520, "CN3", "right")
    ctag(1585, 520, ["4-pin fan", TBD], "fan")
    wire([(EX1, 596), (1650, 596)], "exp")
    tab(EX1, 596, "CN2", "right")
    ctag(1585, 596, ["10-pin", "cross-mapped", TBD], "exp")

    # ---------------------------------------------------------------- Panel A1 (left to right as on the panel)
    item(40, 112, 190, 172, ["POWER button", "momentary", "(on / hold 3 s off)"], size=11)
    board(230, 100, 450, 200, "ANNUNCIATOR", ["0x10 · 7\" TFT Display Driver", "KC-01-1910 (Teensy 4.1)"], ty=24)
    # Annunciator audio: SPKR1 on the PAM8302A master-alarm amplifier, SPKR2 on the DFPlayer
    for x, n, rows in ((274, "SPKR1", ["SPEAKER 1", "alarm tones", "8 \u03a9"]),
                       (354, "SPKR2", ["SPEAKER 2", "GPWS voice", "8 \u03a9"])):
        wire([(x, 208), (x, 286)], "spk")
        tab(x, 200, n, "bottom")
        ctag(x, 246, ["2-pin", TBD], "spk")
        item(x - 37, 286, x + 37, 336, rows, size=10)
    board(520, 212, 800, 320, "INFORMATION DISPLAY 1", ["0x12 · 7\" TFT Display Driver", "KC-01-1910 · panel hub (A1)"],
          hub=True, ty=32)
    t(660, 306, "safe terminal: not used", size=10, fill="#78909c", italic=True)
    for x, n in ((570, "P2"), (650, "P3"), (730, "P4")):
        tab(x, 212, n, "top")
    t(650, 194, "free", size=10.5, fill="#90a4ae", italic=True)
    t(730, 194, "free", size=10.5, fill="#90a4ae", italic=True)
    # Annunciator P1 -> Info 1 P2
    wire([(450, 150), (570, 150), (570, 204)], "slot")
    tab(450, 150, "P1", "right")
    ctag(512, 150, ["16-pin IDC", TBD], "slot")
    # power button -> H1
    wire([(115, 172), (115, 380), (330, 380), (330, CY0)], "pwr")
    tab(330, CY0, "H1", "top")
    ctag(115, 270, ["4-pin", TBD], "pwr")
    # Annunciator USB -> USB2
    wire([(400, 200), (400, CY0)], "usb")
    tab(400, 200, "USB", "bottom")
    tab(400, CY0, "USB2", "top")
    ctag(400, 392, ["5-pin Micro-USB", TBD], "usb")
    # CN4 -> Info 1 CN1, USB3 -> Info 1
    wire([(580, 320), (580, CY0)], "idc")
    tab(580, 320, "CN1", "bottom")
    tab(580, CY0, "CN4", "top")
    ctag(580, 352, ["34-pin IDC", TBD], "idc")
    wire([(700, 320), (700, CY0)], "usb")
    tab(700, 320, "USB", "bottom")
    tab(700, CY0, "USB3", "top")
    ctag(700, 386, ["5-pin Micro-USB", TBD], "usb")

    # ---------------------------------------------------------------- Panel B1
    board(945, 200, 1165, 310, "INFORMATION DISPLAY 2", ["0x13 · 7\" TFT Display Driver", "KC-01-1910 (Teensy 4.1)"],
          tsize=12.5, ty=32)
    board(1210, 212, 1490, 320, "RESOURCE DISPLAY", ["0x11 · 7\" TFT Display Driver", "KC-01-1910 · panel hub (B1)"],
          hub=True, ty=32)
    t(1350, 306, "safe terminal: not used", size=10, fill="#78909c", italic=True)
    for x, n in ((1260, "P2"), (1340, "P3"), (1420, "P4")):
        tab(x, 212, n, "top")
    t(1340, 194, "free", size=10.5, fill="#90a4ae", italic=True)
    wire([(1000, 200), (1000, 166), (1260, 166), (1260, 204)], "slot")
    tab(1000, 200, "P1", "top")
    ctag(1130, 166, ["16-pin IDC", TBD], "slot")
    board(1560, 232, 1762, 320, "DUAL ENCODER", ["0x2D · KC-01-1860", "safe terminal: no path"], ty=32)
    wire([(1420, 204), (1420, 182), (1540, 182), (1540, 290), (1560, 290)], "slot")
    tab(1560, 290, "P1", "left")
    ctag(1480, 182, ["16-pin IDC", TBD], "slot")
    item(1556, 84, 1656, 148, ["CONTROL GROUP", "SELECTOR · ENC1", "KC-01-1850"], size=10)
    item(1664, 84, 1764, 148, ["EXT CAM", "ENCODER · ENC2", "KC-01-1850"], size=10)
    for x, n in ((1610, "H1"), (1714, "H2")):
        wire([(x, 148), (x, 232)], "wire")
        tab(x, 232, n, "top")
        ctag(x, 190, ["5-pin", TBD], "wire")
    # USB4 / USB5 leave the Ctrl Module's right edge and climb through the gap between the panels
    wire([(CX1, 455), (892, 455), (892, 370), (1110, 370), (1110, 310)], "usb")
    tab(CX1, 455, "USB4", "right")
    tab(1110, 310, "USB", "bottom")
    ctag(1000, 370, ["5-pin Micro-USB", TBD], "usb")
    wire([(CX1, 485), (908, 485), (908, 404), (1260, 404), (1260, 320)], "usb")
    tab(CX1, 485, "USB5", "right")
    tab(1260, 320, "USB", "bottom")
    ctag(1185, 404, ["5-pin Micro-USB", TBD], "usb")
    # CN5 -> Resource CN1
    wire([(1390, 320), (1390, EY0)], "idc")
    tab(1390, 320, "CN1", "bottom")
    tab(1390, EY0, "CN5", "top")
    ctag(1390, 380, ["34-pin IDC", TBD], "idc")

    # ---------------------------------------------------------------- input panels
    def input_panel(x0, x1, cn_x, cy_top, src_cn, mods, tag, safe_rows, items):
        HY0, HY1 = 730, 790
        hx0, hx1 = x0 + 74, x1 - 18
        board(hx0, HY0, hx1, HY1, "PANEL HUB  ·  KC-01-1730",
              ["CN1 34-pin · 8 module slots · safe-switch terminal U4"], hub=True)
        wire([(cn_x, cy_top), (cn_x, HY0)], "idc")
        tab(cn_x, HY0, "CN1", "top")
        tab(cn_x, cy_top, src_cn, "bottom")
        ctag(cn_x, 662, ["34-pin IDC", TBD], "idc")
        n = 8
        pitch = (hx1 - hx0 - 16) / n
        cxs = [hx0 + 8 + pitch * (i + 0.5) for i in range(n)]
        MY = 862
        for i, (cx, m) in enumerate(zip(cxs, mods)):
            tab(cx, HY1, "P%d" % (i + 1), "bottom")
            intn = "%sINT_%d" % (tag, i + 1)
            if m is None:
                module(cx, MY, None, intn, None, empty=True)
            else:
                wire([(cx, HY1 + 8), (cx, MY)], "slot")
                ctag(cx, 826, ["16-pin IDC", TBD], "slot", size=9)
                module(cx, MY, m[0], "%s · %s" % (m[1], intn), m[2])
        # safe switch from the hub's left edge
        sy = 760
        sx = x0 + 34
        tab(hx0, sy, "U4", "left")
        wire([(hx0 - 14, sy), (sx, sy), (sx, 1062), (x0 + 50, 1062)], "wire")
        ctag(sx, 900, ["6-pin", TBD], "wire", size=9)
        item(x0 + 50, 1034, x0 + 50 + 112, 1090, safe_rows, size=11)
        t(x0 + 168, 1080, "→ %sINT_SAFE" % tag, size=10, anchor="start", fill="#2e7d32")
        for (slot, rows_tag, rows, w) in items:
            cx = cxs[slot - 1]
            wire([(cx, 1034), (cx, MY + 88)], "wire")
            ctag(cx, 992, rows_tag + [TBD], "wire", size=9)
            item(cx - w / 2, 1034, cx + w / 2, 1090, rows, size=10)

    input_panel(AX0, AX1, 580, CY1, "CN3",
                [(["Action", "Control"], "0x22", "KC-01-1800"),
                 (["Sys Info", "Display"], "0x14", "KC-01-1900"),
                 (["Throttle"], "0x2C", "KC-01-1870"),
                 (["UI", "Control"], "0x20", "KC-01-1800"),
                 None,
                 (["GPWS"], "0x2A", "KC-01-1840"),
                 (["Translation", "joystick"], "0x29", "KC-01-1830"),
                 (["Function", "Control"], "0x21", "KC-01-1810")],
                "", ["ABORT button", "+ arming switch"],
                [(3, ["U7 U8 U10 U11", "4 × 4-pin"], ["FULL · THRTL +", "THRTL − · IDLE", "(lamps: LED_ENA)"], 90),
                 (7, ["U6", "6-pin"], ["THC stick", "3 axes + button"], 86),
                 (8, ["U16 U17", "2 × 6-pin"], ["SWITCH GRP 1", "8 switches", "B16–B23"], 86)])
    input_panel(BX0, BX1, 1390, EY1, "CN4",
                [None,
                 (["Pre-Warp", "Time"], "0x2B", "KC-01-1840"),
                 (["Time", "Control"], "0x25", "KC-01-1800"),
                 (["Vehicle", "Control"], "0x24", "KC-01-1810"),
                 None,
                 (["Stability", "Control"], "0x23", "KC-01-1800"),
                 (["AUX", "Control"], "0x26", "KC-01-1800"),
                 (["Rotation", "joystick"], "0x28", "KC-01-1830")],
                "B_", ["STAGE button", "+ arming switch"],
                [(3, ["CN2", "4-pin"], ["CONTROLLER", "MODE SELECTOR", "SPC · PLN · RVR"], 92),
                 (4, ["U16 U17", "2 × 6-pin"], ["SWITCH GRP 2", "8 switches", "B16–B23"], 86),
                 (8, ["U6", "6-pin"], ["RHC stick", "3 axes + button"], 86)])

    # ---------------------------------------------------------------- legend
    lx, ly = 30, 1142
    t(lx, ly, "LEGEND", size=12.5, weight="bold", anchor="start", fill="#37474f")
    keys = ["idc", "slot", "usb", "pwr", "wire", "fan", "exp", "spk"]
    colw = [600, 600]
    for i, k in enumerate(keys):
        col, row = i % 3, i // 3
        x = lx + 80 + col * 560
        y = ly - 4 + row * 22
        c, w, dash, txt = KIND[k]
        d = ' stroke-dasharray="%s"' % dash if dash else ""
        out.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="%s"%s stroke-linecap="round"/>'
                   % (x, y, x + 40, y, c, w, d))
        t(x + 50, y + 4, txt, size=11.5, anchor="start")
    ctag(lx + 80 + 1120 + 52, ly + 40, ["N-pin", TBD], "idc")
    t(lx + 80 + 1120 + 90, ly + 44, "Cable tag: connector pin count, cable part number (to be assigned)",
      size=11.5, anchor="start")
    t(lx, ly + 84, "Shaded boards are panel hubs. Each hub slot carries 12 V, the I2C bus, RST and one INT line. "
                   "Module boxes: I2C address · INT line, then the assembly part number (KC-01-XXX0). "
                   "Sources: Hardware Inventory §2–§7, Hardware Reference §3.6, §5, §7, §10.",
      size=10.5, anchor="start", fill="#607d8b")

    out.append("</svg>")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png", action="store_true", help="also render a PNG with Chromium")
    ap.add_argument("--scale", type=float, default=3.0)
    ap.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    a = ap.parse_args()
    svg = build()
    sp = os.path.join(a.out, "KCMk1_Wiring_Diagram.svg")
    open(sp, "w").write(svg)
    print("wrote", sp)
    if a.png:
        import subprocess
        pp = os.path.join(a.out, "KCMk1_Wiring_Diagram.png")
        js = """
const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: %d, height: %d }, deviceScaleFactor: %s });
  await p.goto('file://%s');
  await p.screenshot({ path: '%s', fullPage: false });
  await b.close();
})();""" % (W, H, a.scale, sp, pp)
        subprocess.run(["node", "-e", js], check=True)
        print("wrote", pp)


if __name__ == "__main__":
    main()
