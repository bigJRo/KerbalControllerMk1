#!/usr/bin/env python3
"""
gen_wiring_figures.py -- the wiring diagram split for print: three portrait figures.

The single-sheet KCMk1_Wiring_Diagram (gen_wiring_diagram.py) shows every cable on
one 1800-unit-wide drawing: right for a screen, where it can be zoomed, but on a
Letter page its smallest text prints at under 3 pt. These three figures carry the
same content at the same font sizes on drawings about 860 units wide, so on a 6.5 in
wide page nothing prints much under 5.5 pt:

  KCMk1_Wiring_Overview  Figure 23-1: blocks and the cables between them, the rear
                         panel, and the legend for all three figures
  KCMk1_Wiring_SideA     Figure 23-2: Panels A1 and A2 and the Control Module
  KCMk1_Wiring_SideB     Figure 23-3: Panels B1 and B2 and the Control Module Extension

A cable that runs from one side to the other ends at a grey "continued" flag naming
the other figure; its cable tag (pin count, P/N) is drawn once, on Side B.

    python3 gen_wiring_figures.py            # SVGs
    python3 gen_wiring_figures.py --png      # SVGs + PNGs (Chromium, --scale 4)
"""
import argparse
import os
import subprocess

import gen_wiring_diagram as g
from gen_wiring_diagram import (KIND, TBD, t, lines, rect, wire, tab, ctag, board, panel,
                                module, item)

FW, FH = 860, 1000


def start(title):
    g.out.clear()
    g.out.append('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" '
                 'font-family="%s">' % (FW, FH, FW, FH, g.FONT))
    g.out.append('<rect width="100%" height="100%" fill="#ffffff"/>')
    t(FW / 2, 24, title, size=15, weight="bold", fill="#263238")


def finish():
    g.out.append("</svg>")
    return "\n".join(g.out)


def flag(x0, y0, x1, y1, rows):
    """Off-sheet continuation: the cable carries on in another figure."""
    rect(x0, y0, x1, y1, fill="#eceff1", stroke="#78909c", sw=1.2, r=4, dash="4 3")
    n = len(rows)
    lines((x0 + x1) / 2, (y0 + y1) / 2 - (n - 1) * 6.5 + 4, rows, size=10.5, gap=13, fill="#455a64")


def rear_item(x0, x1, y, rows):
    rect(x0, y - 20, x1, y + 20, fill="#ffffff", stroke="#8d6e63", sw=1.2, r=4)
    lines((x0 + x1) / 2, y - 3, rows, size=10.5, gap=13)


def input_panel(x0, x1, y0, y1, cn_x, cy_top, src_cn, mods, tag, safe_rows, items, cable_tag=True):
    """A Panel Hub with its eight slots, the modules in them and the panel wiring."""
    HY0, HY1 = y0 + 46, y0 + 104
    hx0, hx1 = x0 + 58, x1 - 10
    board(hx0, HY0, hx1, HY1, "PANEL HUB  ·  KC-01-1730",
          ["CN1 34-pin · 8 module slots · safe-switch terminal U4"], hub=True)
    wire([(cn_x, cy_top), (cn_x, HY0)], "idc")
    tab(cn_x, HY0, "CN1", "top")
    tab(cn_x, cy_top, src_cn, "bottom")
    if cable_tag:
        ctag(cn_x, (cy_top + HY0) / 2 + 2, ["34-pin IDC", TBD], "idc")
    pitch = (hx1 - hx0 - 12) / 8
    cxs = [hx0 + 6 + pitch * (i + 0.5) for i in range(8)]
    MY = HY1 + 64
    for i, (cx, m) in enumerate(zip(cxs, mods)):
        tab(cx, HY1, "P%d" % (i + 1), "bottom")
        intn = "%sINT_%d" % (tag, i + 1)
        if m is None:
            module(cx, MY, None, intn, None, empty=True, w=84)
        else:
            wire([(cx, HY1 + 8), (cx, MY)], "slot")
            ctag(cx, HY1 + 36, ["16-pin IDC", TBD], "slot", size=9)
            module(cx, MY, m[0], "%s · %s" % (m[1], intn), m[2], w=84)
    IY0, IY1 = y1 - 62, y1 - 10
    sy = (HY0 + HY1) / 2
    sx = x0 + 24
    tab(hx0, sy, "U4", "left")
    wire([(hx0 - 14, sy), (sx, sy), (sx, (IY0 + IY1) / 2), (x0 + 34, (IY0 + IY1) / 2)], "wire")
    ctag(sx, (MY + 88 + IY0) / 2, ["6-pin", TBD], "wire", size=9)
    item(x0 + 34, IY0, x0 + 34 + 104, IY1, safe_rows, size=11)
    t(x0 + 144, IY1 - 4, "→ %sINT_SAFE" % tag, size=10, anchor="start", fill="#2e7d32")
    for (slot, rows_tag, rows, w) in items:
        cx = cxs[slot - 1]
        wire([(cx, IY0), (cx, MY + 88)], "wire")
        ctag(cx, (MY + 88 + IY0) / 2, rows_tag + [TBD], "wire", size=9)
        item(cx - w / 2, IY0, cx + w / 2, IY1, rows, size=10)


A_MODS = [(["Action", "Control"], "0x22", "KC-01-1800"), (["Sys Info", "Display"], "0x14", "KC-01-1900"),
          (["Throttle"], "0x2C", "KC-01-1870"), (["UI", "Control"], "0x20", "KC-01-1800"), None,
          (["GPWS"], "0x2A", "KC-01-1840"), (["Translation", "joystick"], "0x29", "KC-01-1830"),
          (["Function", "Control"], "0x21", "KC-01-1810")]
A_ITEMS = [(3, ["U7 U8 U10 U11", "4 × 4-pin"], ["FULL · THRTL +", "THRTL − · IDLE", "(lamps: LED_ENA)"], 90),
           (7, ["U6", "6-pin"], ["THC stick", "3 axes + button"], 84),
           (8, ["U16 U17", "2 × 6-pin"], ["SWITCH GRP 1", "8 switches", "B16–B23"], 84)]
B_MODS = [None, (["Pre-Warp", "Time"], "0x2B", "KC-01-1840"), (["Time", "Control"], "0x25", "KC-01-1800"),
          (["Vehicle", "Control"], "0x24", "KC-01-1810"), None, (["Stability", "Control"], "0x23", "KC-01-1800"),
          (["AUX", "Control"], "0x26", "KC-01-1800"), (["Rotation", "joystick"], "0x28", "KC-01-1830")]
B_ITEMS = [(3, ["CN2", "4-pin"], ["CONTROLLER", "MODE SELECTOR", "SPC · PLN · RVR"], 92),
           (4, ["U16 U17", "2 × 6-pin"], ["SWITCH GRP 2", "8 switches", "B16–B23"], 84),
           (8, ["U6", "6-pin"], ["RHC stick", "3 axes + button"], 84)]


# ------------------------------------------------------------------ Side A
def side_a():
    start("SIDE A — PANELS A1 AND A2, CONTROL MODULE")
    panel(10, 38, 850, 300, "PANEL A1", "KC-01-1020 · left display panel", "#f7f9fb")
    panel(10, 592, 850, 992, "PANEL A2", "KC-01-1030 · left input panel", "#f7f9fb")

    # Panel A1, in panel order: power button, Annunciator, Information Display 1
    item(24, 84, 170, 144, ["POWER button", "momentary", "(on / hold 3 s off)"], size=11)
    board(196, 72, 436, 172, "ANNUNCIATOR", ["0x10 · 7\" TFT Display Driver", "KC-01-1910 (Teensy 4.1)"], ty=22)
    for x, n, rows in ((242, "SPKR1", ["SPEAKER 1", "alarm tones", "8 Ω"]),
                       (322, "SPKR2", ["SPEAKER 2", "GPWS voice", "8 Ω"])):
        wire([(x, 180), (x, 238)], "spk")
        tab(x, 172, n, "bottom")
        ctag(x, 208, ["2-pin", TBD], "spk")
        item(x - 37, 238, x + 37, 290, rows, size=10)
    board(520, 186, 836, 290, "INFORMATION DISPLAY 1", ["0x12 · 7\" TFT Display Driver", "KC-01-1910 · panel hub (A1)"],
          hub=True, ty=30)
    for x, n in ((570, "P2"), (650, "P3"), (730, "P4")):
        tab(x, 186, n, "top")
    t(650, 168, "free", size=10.5, fill="#90a4ae", italic=True)
    t(730, 168, "free", size=10.5, fill="#90a4ae", italic=True)
    wire([(436, 122), (570, 122), (570, 178)], "slot")
    tab(436, 122, "P1", "right")
    ctag(503, 122, ["16-pin IDC", TBD], "slot")

    # Control Module
    CX0, CY0, CX1, CY1 = 236, 360, 720, 560
    board(CX0, CY0, CX1, CY1, "CONTROL MODULE", ["KC-01-1700 · power latch · INA228 · EMC2101",
                                                 "USB hub (2 × CH334F)"], ty=36)
    rect(470, 448, 700, 516, fill="#fff8e1", stroke="#8d6e63", sw=1.3, r=5)
    t(585, 470, "Master Teensy 4.1", size=12.5, weight="bold")
    t(585, 487, "I2C master · Simpit host link", size=10.5, fill="#5d4037")
    t(585, 502, "reads INT / safe lines, drives RST", size=10.5, fill="#5d4037")
    tab(330, 482, "USB1", "left")
    wire([(347, 482), (470, 482)], "usb")
    ctag(408, 482, ["5-pin Micro-USB", TBD], "usb")
    t(330, 536, "USB6: spare", size=10.5, fill="#78909c", italic=True)
    t(585, 548, "LEDs: 3.3 V (red), 12 V (green)", size=10.5, fill="#78909c")

    # power button -> H1, Annunciator USB -> USB2, Info 1 -> CN4 / USB3
    wire([(97, 144), (97, 332), (290, 332), (290, CY0)], "pwr")
    tab(290, CY0, "H1", "top")
    ctag(97, 230, ["4-pin", TBD], "pwr")
    wire([(400, 180), (400, CY0)], "usb")
    tab(400, 172, "USB", "bottom")
    tab(400, CY0, "USB2", "top")
    ctag(400, 326, ["5-pin Micro-USB", TBD], "usb")
    wire([(575, 290), (575, CY0)], "idc")
    tab(575, 290, "CN1", "bottom")
    tab(575, CY0, "CN4", "top")
    ctag(575, 326, ["34-pin IDC", TBD], "idc")
    wire([(700, 290), (700, CY0)], "usb")
    tab(700, 290, "USB", "bottom")
    tab(700, CY0, "USB3", "top")
    ctag(700, 326, ["5-pin Micro-USB", TBD], "usb")

    # Rear panel, left
    rect(10, 362, 146, 572, fill="#fbe9e7", stroke="#8d6e63", sw=1.4, r=8)
    t(78, 380, "REAR PANEL", size=12.5, weight="bold", fill="#5d4037")
    for y, kind, name, tg, rows in ((408, "pwr", "DC1", ["2-pin 12 V", TBD], ["12 V DC IN", ("2.5 mm barrel jack", {"fill": "#6d4c41"})]),
                                    (456, "usb", "USBC1", ["24-pin USB-C", TBD], ["USB-C · HOST", ("panel mount", {"fill": "#6d4c41"})]),
                                    (504, "exp", "USBC2", ["24-pin USB-C", TBD], ["USB-C · EXP", ("reserved", {"fill": "#6d4c41"})]),
                                    (552, "fan", "CN1", ["4-pin fan", TBD], ["FAN 1", ("PWM + tach", {"fill": "#6d4c41"})])):
        rear_item(18, 138, y, rows)
        wire([(138, y), (CX0, y)], kind)
        tab(CX0, y, name, "left")
        ctag(184, y, tg, kind, size=9)

    # Continued on Side B
    for y, kind, name, rows in ((396, "usb", "USB4", ["to Information", "Display 2 · Fig. 23‑3"]),
                                (436, "usb", "USB5", ["to Resource", "Display · Fig. 23‑3"]),
                                (500, "idc", "CN2", ["50-pin to Extension", "CN1 · Fig. 23‑3"])):
        wire([(CX1, y), (742, y)], kind)
        tab(CX1, y, name, "right")
        flag(742, y - 17, 850, y + 17, rows)

    input_panel(10, 850, 592, 992, 575, CY1, "CN3", A_MODS, "", ["ABORT button", "+ arming switch"], A_ITEMS)
    return finish()


# ------------------------------------------------------------------ Side B
def side_b():
    start("SIDE B — PANELS B1 AND B2, CONTROL MODULE EXTENSION")
    panel(10, 38, 850, 300, "PANEL B1", "KC-01-1040 · right display panel", "#f7f9fb")
    panel(10, 592, 850, 992, "PANEL B2", "KC-01-1050 · right input panel", "#f7f9fb")

    board(22, 150, 236, 256, "INFORMATION DISPLAY 2", ["0x13 · 7\" TFT Display Driver", "KC-01-1910 (Teensy 4.1)"],
          tsize=12.5, ty=30)
    board(290, 160, 590, 266, "RESOURCE DISPLAY", ["0x11 · 7\" TFT Display Driver", "KC-01-1910 · panel hub (B1)"],
          hub=True, ty=30)
    for x, n in ((345, "P2"), (430, "P3"), (515, "P4")):
        tab(x, 160, n, "top")
    t(430, 142, "free", size=10.5, fill="#90a4ae", italic=True)
    wire([(150, 150), (150, 112), (345, 112), (345, 152)], "slot")
    tab(150, 150, "P1", "top")
    ctag(248, 112, ["16-pin IDC", TBD], "slot")
    board(640, 170, 840, 258, "DUAL ENCODER", ["0x2D · KC-01-1860", "safe terminal: no path"], ty=30)
    wire([(515, 152), (515, 134), (620, 134), (620, 228), (640, 228)], "slot")
    tab(640, 228, "P1", "left")
    ctag(567, 134, ["16-pin IDC", TBD], "slot")
    item(640, 56, 738, 116, ["CONTROL GROUP", "SELECTOR · ENC1", "KC-01-1850"], size=10)
    item(744, 56, 842, 116, ["EXT CAM", "ENCODER · ENC2", "KC-01-1850"], size=10)
    for x, n in ((689, "H1"), (793, "H2")):
        wire([(x, 116), (x, 170)], "wire")
        tab(x, 170, n, "top")
        ctag(x, 143, ["5-pin", TBD], "wire", size=9)

    # From Side A: USB4, USB5, the 50-pin interboard
    flag(10, 312, 120, 346, ["from Control Module", "USB4 · Fig. 23‑2"])
    wire([(120, 329), (200, 329), (200, 264)], "usb")
    tab(200, 256, "USB", "bottom")
    ctag(200, 296, ["5-pin Micro-USB", TBD], "usb")
    flag(10, 362, 120, 396, ["from Control Module", "USB5 · Fig. 23‑2"])
    wire([(120, 379), (340, 379), (340, 274)], "usb")
    tab(340, 266, "USB", "bottom")
    ctag(340, 312, ["5-pin Micro-USB", TBD], "usb")

    EX0, EY0, EX1, EY1 = 256, 420, 620, 560
    board(EX0, EY0, EX1, EY1, "CONTROL MODULE EXTENSION", ["KC-01-1710 · passive, every signal", "straight through"], ty=34)
    lines((EX0 + EX1) / 2, 520, ["B_INT_1–12, B_INT_SAFE(2), I2C bus, RST, the",
                                 "expansion bus, FAN_PWM and 12 V pass through"], size=10, fill="#78909c")
    flag(10, 473, 120, 507, ["from Control Module", "CN2 · Fig. 23‑2"])
    wire([(120, 490), (EX0, 490)], "idc")
    tab(EX0, 490, "CN1", "left")
    ctag(186, 490, ["50-pin IDC", "interboard", TBD], "idc", size=9)
    wire([(440, 266), (440, EY0)], "idc")
    tab(440, 266, "CN1", "bottom")
    tab(440, EY0, "CN5", "top")
    ctag(440, 400, ["34-pin IDC", TBD], "idc")

    # Rear panel, right
    rect(714, 420, 850, 572, fill="#fbe9e7", stroke="#8d6e63", sw=1.4, r=8)
    t(782, 438, "REAR PANEL", size=12.5, weight="bold", fill="#5d4037")
    rear_item(722, 842, 470, ["FAN 2", ("PWM, no tach", {"fill": "#6d4c41"})])
    rear_item(722, 842, 534, ["GX16-10 (M)", ("expansion, reserved", {"fill": "#6d4c41"})])
    wire([(EX1, 470), (722, 470)], "fan")
    tab(EX1, 470, "CN3", "right")
    ctag(668, 470, ["4-pin fan", TBD], "fan", size=9)
    wire([(EX1, 534), (722, 534)], "exp")
    tab(EX1, 534, "CN2", "right")
    ctag(668, 534, ["10-pin", "cross-mapped", TBD], "exp", size=9)

    input_panel(10, 850, 592, 992, 440, EY1, "CN4", B_MODS, "B_", ["STAGE button", "+ arming switch"], B_ITEMS)
    return finish()


# ------------------------------------------------------------------ Overview
def blk(x0, y0, x1, y1, title, rows, hub=False):
    rect(x0, y0, x1, y1, fill="#e3f2fd" if hub else "#ffffff")
    t((x0 + x1) / 2, y0 + 19, title, size=12.5, weight="bold")
    if rows:
        lines((x0 + x1) / 2, y0 + 35, rows, size=10.5, fill="#455a64", gap=13)


def label(x, y, s, kind, anchor="middle"):
    t(x, y, s, size=11, weight="bold", anchor=anchor, fill=KIND[kind][0])


def overview():
    start("INTERNAL WIRING — OVERVIEW")
    panel(10, 38, 422, 262, "PANEL A1", "left display", "#f7f9fb")
    panel(438, 38, 850, 262, "PANEL B1", "right display", "#f7f9fb")
    blk(24, 78, 116, 128, "POWER", ["button"])
    blk(130, 78, 270, 154, "ANNUNCIATOR", ["0x10 · KC-01-1910", "2 speakers"])
    blk(284, 150, 410, 226, "INFO DISPLAY 1", ["0x12 · panel hub", "slots P2–P4"], hub=True)
    blk(452, 150, 578, 226, "INFO DISPLAY 2", ["0x13 · KC-01-1910"])
    blk(592, 150, 718, 226, "RESOURCE DISP.", ["0x11 · panel hub", "slots P2–P4"], hub=True)
    blk(732, 150, 840, 226, "DUAL ENC.", ["0x2D", "+ 2 encoders"])
    wire([(270, 116), (330, 116), (330, 150)], "slot")
    wire([(578, 188), (592, 188)], "slot")
    wire([(718, 210), (732, 210)], "slot")
    label(300, 108, "16-pin", "slot")

    # Control Module and Extension
    blk(60, 330, 400, 450, "CONTROL MODULE", ["KC-01-1700 \u00b7 master Teensy 4.1", "power latch \u00b7 USB hub \u00b7 fan control"])
    blk(460, 330, 800, 450, "CONTROL MODULE EXTENSION", ["KC-01-1710 \u00b7 passive", "B-side panels, fan 2, expansion"])
    wire([(400, 400), (460, 400)], "idc")
    label(430, 392, "50-pin", "idc")
    # panel cables to the display hubs
    wire([(300, 226), (300, 330)], "idc"); label(294, 318, "34-pin IDC", "idc", anchor="end")
    wire([(655, 226), (655, 330)], "idc"); label(661, 318, "34-pin IDC", "idc", anchor="start")
    # USB tree: USB2 Annunciator, USB3 Info 1, USB4 Info 2, USB5 Resource
    wire([(200, 330), (200, 154)], "usb"); label(194, 300, "USB2", "usb", anchor="end")
    wire([(360, 330), (360, 226)], "usb"); label(354, 250, "USB3", "usb", anchor="end")
    wire([(374, 330), (374, 292), (515, 292), (515, 226)], "usb"); label(521, 252, "USB4", "usb", anchor="start")
    wire([(388, 330), (388, 306), (630, 306), (630, 226)], "usb"); label(624, 252, "USB5", "usb", anchor="end")
    # power button
    wire([(80, 128), (80, 330)], "pwr"); label(86, 230, "H1", "pwr", anchor="start")

    # Rear panel (the A2 / B2 ribbons pass through the gaps at x 385 and 475)
    rect(10, 480, 850, 560, fill="#fbe9e7", stroke="#8d6e63", sw=1.4, r=8)
    t(24, 498, "REAR PANEL", size=12.5, weight="bold", anchor="start", fill="#5d4037")
    rp = [(72, "12 V DC IN", "barrel jack", "pwr", "DC1"),
          (160, "USB-C HOST", "to PC", "usb", "USBC1"),
          (248, "USB-C EXP", "reserved", "exp", "USBC2"),
          (336, "FAN 1", "PWM + tach", "fan", "CN1"),
          (545, "BLANK PLUG", "unused", None, None),
          (640, "GX16-10", "expansion", "exp", "CN2"),
          (735, "FAN 2", "PWM", "fan", "CN3")]
    for x, a_, b_, kind, conn in rp:
        rect(x - 42, 508, x + 42, 552, fill="#ffffff", stroke="#8d6e63", sw=1.2, r=4)
        lines(x, 526, [a_, (b_, {"fill": "#6d4c41"})], size=10.5, gap=13)
        if kind:
            wire([(x, 508), (x, 450)], kind)
            t(x + 4, 470, conn, size=9.5, anchor="start", fill=KIND[kind][0])

    # Input panels
    panel(10, 590, 422, 780, "PANEL A2", "left input", "#f7f9fb")
    panel(438, 590, 850, 780, "PANEL B2", "right input", "#f7f9fb")
    blk(30, 630, 402, 680, "PANEL HUB \u00b7 KC-01-1730", ["8 slots: 7 modules + Sys Info Display"], hub=True)
    blk(458, 630, 830, 680, "PANEL HUB \u00b7 KC-01-1730", ["8 slots: 6 modules (P1, P5 free)"], hub=True)
    lines(216, 704, ["Action, Sys Info, Throttle, UI, GPWS,", "Translation stick, Function (16-pin IDC)",
                     "ABORT, throttle buttons, THC, Switch Group 1"], size=10.5, fill="#455a64", gap=14)
    lines(644, 704, ["Pre-Warp, Time, Vehicle, Stability, AUX,", "Rotation stick (16-pin IDC)",
                     "STAGE, controller mode selector, SG 2, RHC"], size=10.5, fill="#455a64", gap=14)
    wire([(385, 450), (385, 630)], "idc")
    wire([(475, 450), (475, 630)], "idc")
    label(379, 577, "34-pin IDC (Ctrl CN3)", "idc", anchor="end")
    label(481, 577, "34-pin IDC (Ext CN4)", "idc", anchor="start")

    # Legend
    ly = 812
    t(16, ly, "LEGEND", size=12.5, weight="bold", anchor="start", fill="#37474f")
    keys = ["idc", "slot", "usb", "pwr", "wire", "fan", "exp", "spk"]
    for i, k in enumerate(keys):
        col, row = i % 2, i // 2
        x = 16 + col * 420
        y = ly + 22 + row * 24
        c, w, dash, txt = KIND[k]
        d = ' stroke-dasharray="%s"' % dash if dash else ""
        g.out.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="%s"%s stroke-linecap="round"/>'
                     % (x, y, x + 40, y, c, w, d))
        t(x + 50, y + 4, txt, size=11, anchor="start")
    ctag(36, ly + 126, ["N-pin", TBD], "idc")
    t(66, ly + 130, "Cable tag (Figures 23\u20112, 23\u20113): connector pin count, cable part number (to be assigned)",
      size=11, anchor="start")
    rect(16, ly + 144, 56, ly + 164, fill="#eceff1", stroke="#78909c", sw=1.2, r=4, dash="4 3")
    t(66, ly + 158, "Cable continued in the other side figure", size=11, anchor="start")
    t(16, ly + 182, "Shaded boards are panel hubs; each hub slot carries 12 V, the I2C bus, RST and one INT line. "
                    "Module boxes: I2C address · INT line, then the assembly part number (KC-01-XXX0).",
      size=10, anchor="start", fill="#607d8b")
    return finish()


def render_png(svg_path, png_path, scale):
    js = """
const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: %d, height: %d }, deviceScaleFactor: %s });
  await p.goto('file://%s');
  await p.screenshot({ path: '%s' });
  await b.close();
})();""" % (FW, FH, scale, svg_path, png_path)
    subprocess.run(["node", "-e", js], check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png", action="store_true")
    ap.add_argument("--scale", type=float, default=4.0)
    ap.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    a = ap.parse_args()
    for name, fn in (("KCMk1_Wiring_Overview", overview), ("KCMk1_Wiring_SideA", side_a), ("KCMk1_Wiring_SideB", side_b)):
        svg = fn()
        sp = os.path.join(a.out, name + ".svg")
        open(sp, "w").write(svg)
        if a.png:
            render_png(sp, os.path.join(a.out, name + ".png"), a.scale)
        print("wrote", sp)


if __name__ == "__main__":
    main()
