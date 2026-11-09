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
about 300 dpi across a 6.5 in page width at 1700 px).

    python3 gen_wiring_diagram.py            # SVG only
    python3 gen_wiring_diagram.py --png      # SVG + PNG
"""
import argparse
import os
from xml.sax.saxutils import escape

W, H = 1700, 1150
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


def module(cx, y0, lines_, empty=False, w=86, h=66):
    x0, x1 = cx - w / 2, cx + w / 2
    if empty:
        rect(x0, y0, x1, y0 + h, fill="#fafafa", stroke="#b0bec5", sw=1.2, dash="4 3")
        t(cx, y0 + h / 2 + 4, "empty", size=11, fill="#90a4ae", italic=True)
        return
    rect(x0, y0, x1, y0 + h, fill="#ffffff")
    name = lines_[0]
    for i, s in enumerate(name):
        t(cx, y0 + 17 + i * 13.5, s, size=11.5, weight="bold")
    t(cx, y0 + h - 8, lines_[1], size=10.5, fill="#455a64")


def item(x0, y0, x1, y1, rows, size=11):
    rect(x0, y0, x1, y1, fill="#f1f8e9", stroke="#558b2f", sw=1.3, r=5)
    n = len(rows)
    cy = (y0 + y1) / 2 - (n - 1) * size * 0.62 + 4
    lines((x0 + x1) / 2, cy, rows, size=size, gap=size * 1.24)


def build():
    out.append('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" '
               'font-family="%s">' % (W, H, W, H, FONT))
    out.append('<rect width="100%" height="100%" fill="#ffffff"/>')

    # ---------------------------------------------------------------- panels
    AX0, AX1, BX0, BX1 = 18, 832, 868, 1682
    panel(AX0, 52, AX1, 330, "PANEL A1", "left display panel", "#f7f9fb")
    panel(BX0, 52, BX1, 330, "PANEL B1", "right display panel", "#f7f9fb")
    panel(AX0, 634, AX1, 1040, "PANEL A2", "left input panel", "#f7f9fb")
    panel(BX0, 634, BX1, 1040, "PANEL B2", "right input panel", "#f7f9fb")
    t(W / 2, 30, "KERBAL CONTROLLER Mk1 — INTERNAL WIRING", size=17, weight="bold", fill="#263238")

    # ---------------------------------------------------------------- Ctrl Module
    CX0, CY0, CX1, CY1 = 262, 390, 722, 604
    board(CX0, CY0, CX1, CY1, "CONTROL MODULE", ["KC-01-1701 · power latch · INA228 · EMC2101 · USB hub (2 × CH334F)"],
          ty=34)
    # Master Teensy inside, on USB1
    rect(436, 470, 658, 540, fill="#fff8e1", stroke="#8d6e63", sw=1.3, r=5)
    t(547, 494, "Master Teensy 4.1", size=12.5, weight="bold")
    t(547, 512, "I2C master · Simpit host link", size=10.5, fill="#5d4037")
    t(547, 528, "reads the INT and safe lines, drives RST", size=10.5, fill="#5d4037")
    tab(360, 505, "USB1", "left")
    wire([(377, 505), (436, 505)], "usb")
    t(360, 560, "USB6: spare", size=10.5, fill="#78909c", italic=True)
    t(547, 590, "LEDs: 3.3 V (red), 12 V (green)", size=10.5, fill="#78909c")

    # ---------------------------------------------------------------- Extension
    EX0, EY0, EX1, EY1 = 1000, 412, 1440, 578
    board(EX0, EY0, EX1, EY1, "CONTROL MODULE EXTENSION", ["KC-01-1711 · passive, every signal straight through"],
          ty=32)
    lines((EX0 + EX1) / 2, 506, ["Carries the B-side lines: B_INT_1–12, B_INT_SAFE(2), I2C bus,",
                                 "RST, the expansion bus (SDA/SCL_EXT, EXT_INT_1/2), FAN_PWM, 12 V"],
          size=10.5, fill="#78909c")
    wire([(CX1, 486), (EX0, 486)], "idc", "50-pin interboard", off=(0, -12))
    tab(CX1, 486, "CN2", "right")
    tab(EX0, 486, "CN1", "left")

    # ---------------------------------------------------------------- Rear panel (flanks)
    rect(30, 372, 214, 622, fill="#fbe9e7", stroke="#8d6e63", sw=1.4, r=8)
    t(122, 392, "REAR PANEL", size=13, weight="bold", fill="#5d4037")
    rect(1500, 400, 1670, 590, fill="#fbe9e7", stroke="#8d6e63", sw=1.4, r=8)
    t(1585, 420, "REAR PANEL", size=13, weight="bold", fill="#5d4037")

    def rp(x0, x1, y, rows):
        rect(x0, y - 20, x1, y + 20, fill="#ffffff", stroke="#8d6e63", sw=1.2, r=4)
        lines((x0 + x1) / 2, y - 3 if len(rows) > 1 else y + 4, rows, size=11, gap=13)

    rp(42, 202, 428, ["12 V DC IN", ("2.5 mm barrel, ≥10 A", {"fill": "#6d4c41"})])
    rp(42, 202, 480, ["USB-C · HOST PC", ("panel mount", {"fill": "#6d4c41"})])
    rp(42, 202, 532, ["USB-C · EXPANSION", ("panel mount, reserved", {"fill": "#6d4c41"})])
    rp(42, 202, 584, ["FAN 1", ("120 mm PWM, tach", {"fill": "#6d4c41"})])
    wire([(202, 428), (CX0, 428)], "pwr")
    tab(CX0, 428, "DC1", "left")
    wire([(202, 480), (CX0, 480)], "usb")
    tab(CX0, 480, "USBC1", "left")
    wire([(202, 532), (CX0, 532)], "exp")
    tab(CX0, 532, "USBC2", "left")
    wire([(202, 584), (CX0, 584)], "fan")
    tab(CX0, 584, "CN1", "left")

    rp(1514, 1656, 458, ["FAN 2", ("120 mm PWM, no tach", {"fill": "#6d4c41"})])
    rp(1514, 1656, 540, ["GX16-10", ("expansion, reserved", {"fill": "#6d4c41"})])
    wire([(EX1, 458), (1514, 458)], "fan", "PWM", off=(0, -6))
    tab(EX1, 458, "CN3", "right")
    wire([(EX1, 540), (1514, 540)], "exp", "10-wire", off=(0, -6))
    tab(EX1, 540, "CN2", "right")
    t(1477, 568, "cross-mapped", size=10, fill=KIND["exp"][0])

    # ---------------------------------------------------------------- Panel A1
    # Annunciator (left), Information Display 1 (hub, centre), power button (right)
    board(46, 92, 262, 186, "ANNUNCIATOR", ["0x10 · 7\" TFT Display Driver", "KC-01-1911 (Teensy 4.1)"])
    board(300, 200, 580, 308, "INFORMATION DISPLAY 1", ["0x12 · 7\" TFT Display Driver", "KC-01-1911 · panel hub (A1)"],
          hub=True, ty=32)
    t(440, 268 + 26, "safe terminal: not used", size=10, fill="#78909c", italic=True)
    # slots on the top edge
    for x, n in ((360, "P2"), (440, "P3"), (520, "P4")):
        tab(x, 200, n, "top")
    t(440, 182, "free", size=10.5, fill="#90a4ae", italic=True)
    t(520, 182, "free", size=10.5, fill="#90a4ae", italic=True)
    wire([(262, 140), (360, 140), (360, 192)], "slot", "16-pin", at=0, off=(0, -6))
    tab(262, 140, "P1", "right")
    # power button
    item(632, 214, 812, 264, ["POWER button", "momentary (on / hold 3 s off)"], size=11)

    # Ctrl top-edge connectors
    # USB2 -> Annunciator, CN4 -> Info 1, USB3 -> Info 1, H1 -> power button, USB4/5 -> B1
    wire([(120, 186), (120, 350), (292, 350), (292, CY0)], "usb", "USB2", at=1, off=(0, -6))
    tab(120, 186, "USB", "bottom")
    tab(292, CY0, "USB2", "top")
    wire([(352, 308), (352, CY0)], "idc", "34-pin IDC", off=(9, -14))
    tab(352, 308, "CN1", "bottom")
    tab(352, CY0, "CN4", "top")
    wire([(450, 308), (450, CY0)], "usb", "USB3", off=(8, -6))
    tab(450, 308, "USB", "bottom")
    tab(450, CY0, "USB3", "top")
    wire([(540, CY0), (540, 344), (722, 344), (722, 264)], "pwr", "soft latch", at=2, off=(7, 0))
    tab(540, CY0, "H1", "top")

    # ---------------------------------------------------------------- Panel B1
    board(896, 196, 1112, 300, "INFORMATION DISPLAY 2", ["0x13 · 7\" TFT Display Driver", "KC-01-1911 (Teensy 4.1)"],
          tsize=12.5, ty=32)
    board(1150, 200, 1430, 308, "RESOURCE DISPLAY", ["0x11 · 7\" TFT Display Driver", "KC-01-1911 · panel hub (B1)"],
          hub=True, ty=32)
    t(1290, 294, "safe terminal: not used", size=10, fill="#78909c", italic=True)
    for x, n in ((1200, "P2"), (1280, "P3"), (1360, "P4")):
        tab(x, 200, n, "top")
    t(1280, 182, "free", size=10.5, fill="#90a4ae", italic=True)
    wire([(1064, 196), (1064, 160), (1200, 160), (1200, 192)], "slot", "16-pin", at=1, off=(0, -6))
    tab(1064, 196, "P1", "top")
    board(1458, 214, 1664, 300, "DUAL ENCODER", ["0x2D · KC-01-1861", "safe terminal: no path"], ty=32)
    wire([(1360, 192), (1360, 176), (1444, 176), (1444, 268), (1458, 268)], "slot", "16-pin", at=1, off=(0, -6))
    tab(1458, 268, "P1", "left")
    item(1462, 92, 1556, 150, ["ENC1 · CTRL GRP", "Encoder carrier", "KC-01-1851"], size=10)
    item(1566, 92, 1660, 150, ["ENC2 · EXT CAM", "Encoder carrier", "KC-01-1851"], size=10)
    wire([(1509, 150), (1509, 214)], "wire", "5-pin", off=(6, -4))
    wire([(1613, 150), (1613, 214)], "wire", "5-pin", off=(6, -4))
    tab(1509, 214, "H1", "top")
    tab(1613, 214, "H2", "top")

    wire([(640, CY0), (640, 356), (980, 356), (980, 300)], "usb", "USB4", at=1, off=(80, -6))
    tab(980, 300, "USB", "bottom")
    tab(640, CY0, "USB4", "top")
    wire([(690, CY0), (690, 370), (1196, 370), (1196, 308)], "usb", "USB5", at=1, off=(150, -6))
    tab(1196, 308, "USB", "bottom")
    tab(690, CY0, "USB5", "top")
    wire([(1300, 308), (1300, EY0)], "idc", "34-pin IDC", off=(9, -14))
    tab(1300, 308, "CN1", "bottom")
    tab(1300, EY0, "CN5", "top")

    # ---------------------------------------------------------------- input panels
    def input_panel(x0, x1, cn_x, cy_top, mods, tag, safe_rows, items):
        HY0, HY1 = 680, 740
        hx0, hx1 = x0 + 66, x1 - 18
        board(hx0, HY0, hx1, HY1, "PANEL HUB  ·  KC-01-1731", ["CN1 34-pin · 8 module slots · safe-switch terminal U4"],
              hub=True)
        wire([(cn_x, cy_top), (cn_x, HY0)], "idc", "34-pin IDC", off=(9, -34))
        tab(cn_x, HY0, "CN1", "top")
        # slots and modules
        n = 8
        pitch = (hx1 - hx0 - 20) / n
        cxs = [hx0 + 10 + pitch * (i + 0.5) for i in range(n)]
        for i, (cx, m) in enumerate(zip(cxs, mods)):
            tab(cx, HY1, "P%d" % (i + 1), "bottom")
            if m is None:
                module(cx, 784, None, empty=True)
            else:
                wire([(cx, HY1 + 8), (cx, 784)], "slot")
                module(cx, 784, m)
            t(cx + 6, 770, "%sINT_%d" % (tag, i + 1), size=9.5, anchor="start", fill="#78909c")
        # safe switch on the hub's left edge
        sy = 712
        tab(hx0, sy, "U4", "left")
        wire([(hx0 - 14, sy), (x0 + 30, sy), (x0 + 30, 960), (x0 + 42, 960)], "wire", None)
        item(x0 + 42, 932, x0 + 42 + 112, 990, safe_rows, size=11)
        t(x0 + 36, 1010, "→ %sINT_SAFE (active-high)" % tag, size=10, anchor="start", fill="#2e7d32")
        # panel items under their modules
        for (slot, term, rows, w) in items:
            cx = cxs[slot - 1]
            wire([(cx, 932), (cx, 850)], "wire", term, off=(6, -2))
            item(cx - w / 2, 932, cx + w / 2, 990, rows, size=10)
        return cxs

    input_panel(AX0, AX1, 352, CY1,
                [(["Action", "Control"], "0x22 · 1801"),
                 (["Sys Info", "Display"], "0x14 · 1901"),
                 (["Throttle"], "0x2C · 1871"),
                 (["UI", "Control"], "0x20 · 1801"),
                 None,
                 (["GPWS"], "0x2A · 1841"),
                 (["Translation", "joystick"], "0x29 · 1831"),
                 (["Function", "Control"], "0x21 · 1811")],
                "", ["ABORT button", "+ arming switch"],
                [(3, "U7 U8 U10 U11", ["FULL · THRTL +", "THRTL − · IDLE", "(lamps: LED_ENA)"], 112),
                 (7, "U6", ["THC stick", "3 axes + button", "(6-wire)"], 84),
                 (8, "U16 U17", ["SWITCH GRP 1", "8 switches", "B16–B23"], 84)])
    input_panel(BX0, BX1, 1300, EY1,
                [None,
                 (["Pre-Warp", "Time"], "0x2B · 1841"),
                 (["Time", "Control"], "0x25 · 1801"),
                 (["Vehicle", "Control"], "0x24 · 1811"),
                 None,
                 (["Stability", "Control"], "0x23 · 1801"),
                 (["AUX", "Control"], "0x26 · 1801"),
                 (["Rotation", "joystick"], "0x28 · 1831")],
                "B_", ["STAGE button", "+ arming switch"],
                [(3, "CN2", ["CTRL MODE", "SPC · PLN · RVR", "B12 / B13"], 84),
                 (4, "U16 U17", ["SWITCH GRP 2", "8 switches", "B16–B23"], 84),
                 (8, "U6", ["RHC stick", "3 axes + button", "(6-wire)"], 84)])

    # ---------------------------------------------------------------- legend
    lx, ly = 30, 1068
    t(lx, ly, "LEGEND", size=12.5, weight="bold", anchor="start", fill="#37474f")
    keys = ["idc", "slot", "usb", "pwr", "wire", "fan", "exp"]
    colw = [560, 560, 560]
    for i, k in enumerate(keys):
        col, row = i % 3, i // 3
        x = lx + 80 + sum(colw[:col])
        y = ly - 4 + row * 22
        c, w, dash, txt = KIND[k]
        d = ' stroke-dasharray="%s"' % dash if dash else ""
        out.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="%s"%s stroke-linecap="round"/>'
                   % (x, y, x + 40, y, c, w, d))
        t(x + 50, y + 4, txt, size=11.5, anchor="start")
    t(lx, ly + 66, "Shaded boards are panel hubs. Each hub slot carries 12 V, the I2C bus, RST and one INT line "
                   "(the INT name is under each slot). Addresses and board numbers: 0xNN · KC-01-NNNN. "
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
