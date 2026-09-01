#!/usr/bin/env python3
"""
make_mockups.py -- generate the screen renderings used by the KCMk1 display
                   user documentation in Documents/User/Displays/.

Every panel in the controller is a 1024x600 TFT, so every mockup is emitted at a
1024x600 viewBox with the same geometry the firmware uses (the layout constants
in Software/Displays/*/Screen_*.ino).  The drawings are schematic -- fonts and
bitmaps are not the real ones -- but the position, size and colour of every
region, tile, gauge and readout is taken from the source, so a pilot can match
what is on the glass to what is in the guide.

Colours are the firmware's own RGB565 constants (KerbalDisplayCommon.h),
converted to sRGB here.

Usage:  python3 make_mockups.py [output_dir]
        (default output_dir = ../images relative to this file)
"""

import os
import sys
import math

# ─────────────────────────────────────────────────────────────────────────────
# Palette -- RGB565 values copied from
# Software/Displays/libraries/KerbalDisplayCommon/src/KerbalDisplayCommon.h
# ─────────────────────────────────────────────────────────────────────────────
RGB565 = {
    "BLACK": 0x0000, "OFF_BLACK": 0x2104, "DARK_GREY": 0x39E7, "GREY": 0x8410,
    "LIGHT_GREY": 0xBDF7, "WHITE": 0xFFFF, "GREEN": 0x07E0, "DARK_GREEN": 0x03E0,
    "JUNGLE": 0x01E0, "RED": 0xF800, "MAROON": 0x7800, "CORNELL": 0xB0E3,
    "DARK_RED": 0x6000, "BLUE": 0x001F, "SKY": 0x761F, "ROYAL": 0x010C,
    "AQUA": 0x5D1C, "NAVY": 0x000F, "CYAN": 0x07FF, "FRENCH_BLUE": 0x347C,
    "MAGENTA": 0xF81F, "PURPLE": 0x8010, "VIOLET": 0x901A, "DIM_VIOLET": 0x480D,
    "YELLOW": 0xFDC2, "DULL_YELLOW": 0xEEEB, "DARK_YELLOW": 0xA500,
    "OLIVE": 0x8400, "BROWN": 0x8200, "SILVER": 0xC618, "GOLD": 0xD566,
    "ORANGE": 0xFBE0, "AIR_SUP_BLUE": 0x7517, "NEON_GREEN": 0x3FE2,
    "DIM_NEON_GRN": 0x1BE1, "SAP_GREEN": 0x53E5, "INT_ORANGE": 0xFA80,
    "UPS_BROWN": 0x6203, "MINT": 0xA6F6, "MED_GREEN": 0x0507, "TAN": 0xB46A,
    "ROSE": 0xF3CF, "CRIMSON": 0xD8A7, "OCEAN": 0x01F1,
}


def _to_hex(v):
    r = (v >> 11) & 0x1F
    g = (v >> 5) & 0x3F
    b = v & 0x1F
    return "#%02X%02X%02X" % (round(r * 255 / 31), round(g * 255 / 63),
                              round(b * 255 / 31))


C = {k: _to_hex(v) for k, v in RGB565.items()}

W, H = 1024, 600
FONT = "'DejaVu Sans','Verdana',sans-serif"
MONO = "'DejaVu Sans Mono','Consolas',monospace"


# ─────────────────────────────────────────────────────────────────────────────
# Tiny SVG builder
# ─────────────────────────────────────────────────────────────────────────────
class Svg(object):
    def __init__(self, title, w=W, h=H, bg="BLACK"):
        self.parts = []
        self.w, self.h = w, h
        self.title = title
        self.rect(0, 0, w, h, C[bg])

    # -- primitives ----------------------------------------------------------
    def rect(self, x, y, w, h, fill=None, stroke=None, sw=1, rx=0, op=None):
        a = 'x="%g" y="%g" width="%g" height="%g"' % (x, y, w, h)
        if rx:
            a += ' rx="%g"' % rx
        a += ' fill="%s"' % (fill if fill else "none")
        if stroke:
            a += ' stroke="%s" stroke-width="%g"' % (stroke, sw)
        if op is not None:
            a += ' opacity="%g"' % op
        self.parts.append("<rect %s/>" % a)

    def line(self, x1, y1, x2, y2, stroke, sw=1, dash=None, op=None):
        a = 'x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s" stroke-width="%g"' % (
            x1, y1, x2, y2, stroke, sw)
        if dash:
            a += ' stroke-dasharray="%s"' % dash
        if op is not None:
            a += ' opacity="%g"' % op
        self.parts.append("<line %s/>" % a)

    def circle(self, cx, cy, r, fill=None, stroke=None, sw=1, dash=None, op=None):
        a = 'cx="%g" cy="%g" r="%g" fill="%s"' % (cx, cy, r, fill if fill else "none")
        if stroke:
            a += ' stroke="%s" stroke-width="%g"' % (stroke, sw)
        if dash:
            a += ' stroke-dasharray="%s"' % dash
        if op is not None:
            a += ' opacity="%g"' % op
        self.parts.append("<circle %s/>" % a)

    def ellipse(self, cx, cy, rx, ry, fill=None, stroke=None, sw=1, dash=None, rot=0):
        a = 'cx="%g" cy="%g" rx="%g" ry="%g" fill="%s"' % (
            cx, cy, rx, ry, fill if fill else "none")
        if stroke:
            a += ' stroke="%s" stroke-width="%g"' % (stroke, sw)
        if dash:
            a += ' stroke-dasharray="%s"' % dash
        if rot:
            a += ' transform="rotate(%g %g %g)"' % (rot, cx, cy)
        self.parts.append("<ellipse %s/>" % a)

    def poly(self, pts, fill=None, stroke=None, sw=1, op=None):
        p = " ".join("%g,%g" % (x, y) for x, y in pts)
        a = 'points="%s" fill="%s"' % (p, fill if fill else "none")
        if stroke:
            a += ' stroke="%s" stroke-width="%g"' % (stroke, sw)
        if op is not None:
            a += ' opacity="%g"' % op
        self.parts.append("<polygon %s/>" % a)

    def path(self, d, fill=None, stroke=None, sw=1, dash=None, op=None):
        a = 'd="%s" fill="%s"' % (d, fill if fill else "none")
        if stroke:
            a += ' stroke="%s" stroke-width="%g"' % (stroke, sw)
        if dash:
            a += ' stroke-dasharray="%s"' % dash
        if op is not None:
            a += ' opacity="%g"' % op
        self.parts.append("<path %s/>" % a)

    def text(self, x, y, s, fill="#FFFFFF", size=16, anchor="start", weight="bold",
             font=None, rot=None, op=None, spacing=None):
        s = (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))
        a = ('x="%g" y="%g" fill="%s" font-size="%g" font-family="%s" '
             'font-weight="%s" text-anchor="%s"'
             % (x, y, fill, size, font or FONT, weight, anchor))
        if spacing:
            a += ' letter-spacing="%g"' % spacing
        if rot is not None:
            a += ' transform="rotate(%g %g %g)"' % (rot, x, y)
        if op is not None:
            a += ' opacity="%g"' % op
        self.parts.append("<text %s>%s</text>" % (a, s))

    # -- composites ----------------------------------------------------------
    def tile(self, x, y, w, h, label, fill, fg, border=None, size=13, pad=1,
             lines=None, weight="bold"):
        """A bordered annunciator/mode tile with centred (optionally wrapped) text."""
        self.rect(x + pad, y + pad, w - 2 * pad, h - 2 * pad, fill,
                  border or C["GREY"], 1)
        lines = lines or label.split("\n")
        n = len(lines)
        for i, ln in enumerate(lines):
            ty = y + h / 2 + size * 0.36 + (i - (n - 1) / 2.0) * (size + 2)
            self.text(x + w / 2, ty, ln, fg, size, "middle", weight)

    def row(self, x, y, w, h, label, value, lcol=None, vcol=None, size=26,
            bg=None, border=None):
        """A label-left / value-right readout row, the panel's standard cell."""
        if bg:
            self.rect(x, y, w, h, bg, border, 1)
        elif border:
            self.rect(x, y, w, h, None, border, 1)
        self.text(x + 8, y + h / 2 + size * 0.35, label, lcol or C["WHITE"], size)
        self.text(x + w - 8, y + h / 2 + size * 0.35, value,
                  vcol or C["DARK_GREEN"], size, "end")

    def save(self, path):
        head = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" '
                'width="%d" height="%d" role="img" aria-label="%s">'
                % (self.w, self.h, self.w, self.h, self.title))
        body = "\n".join(self.parts)
        with open(path, "w") as f:
            f.write(head + "\n<title>" + self.title + "</title>\n" + body + "\n</svg>\n")


# ─────────────────────────────────────────────────────────────────────────────
# Shared InfoDisp chrome
#   SCREEN 1024x600, SIDEBAR_W 84, TITLE_H 58 + 4 px rule => TITLE_TOP 62
#   Unit 1 (vehicle-type panel): sidebar left,  content x = 84
#   Unit 2 (mission-phase panel): sidebar right, content x = 0
# ─────────────────────────────────────────────────────────────────────────────
SIDEBAR_W = 84
TITLE_H = 58
TITLE_TOP = 62
CONTENT_W = W - SIDEBAR_W          # 940

SB_LABELS_U1 = ["PFD", "LNCH", "ORB", "TGT", "DESC", "VEH"]
SB_LABELS_U2 = ["PFD", "LNCH", "ORB", "TGT", "DESC", "ASC"]


def info_frame(title, sel, unit=2, chip="AUTO", sb=None, asc_armed=False):
    """Build an InfoDisp screen with title bar + sidebar; returns (svg, ox).

    ox is the x offset of the content area, so screen-space layout constants
    from the firmware (which are content-relative) can be used directly.
    """
    s = Svg(title)
    ox = SIDEBAR_W if unit == 1 else 0
    sbx = 0 if unit == 1 else W - SIDEBAR_W
    labels = sb or (SB_LABELS_U1 if unit == 1 else SB_LABELS_U2)

    # title bar spans the content area only
    s.rect(ox, 0, CONTENT_W, TITLE_H, C["BLACK"])
    s.rect(ox, TITLE_H, CONTENT_W, 4, C["GREY"])
    s.text(ox + CONTENT_W / 2, 44, title, C["WHITE"], 36, "middle")

    # AUTO / MAN chip -- 58x22 rounded outline, top right of the content area
    cw, ch = 58, 22
    ccol = C["GREY"] if chip == "AUTO" else C["DARK_GREEN"]
    s.rect(ox + CONTENT_W - cw - 6, 6, cw, ch, C["BLACK"], ccol, 2, rx=5)
    s.text(ox + CONTENT_W - cw / 2 - 6, 22, chip, ccol, 14, "middle")

    # sidebar keys -- 6 keys, 100 px tall each
    bh = H // 6
    for i, lab in enumerate(labels):
        y = i * bh
        on = (i == sel)
        fill = C["GREY"] if on else C["BLACK"]
        fg = C["BLACK"] if on else C["WHITE"]
        bd = C["WHITE"] if on else C["GREY"]
        if lab == "ASC" and asc_armed and not on:
            fg = C["DARK_GREEN"]
            bd = C["DARK_GREEN"]
        s.rect(sbx + (1 if unit == 2 else 0), y + 2, SIDEBAR_W - 2, bh - 4,
               fill, bd, 2)
        s.text(sbx + SIDEBAR_W / 2, y + bh / 2 + 9, lab, fg, 26, "middle")
    # divider rule on the sidebar's inboard edge
    dx = SIDEBAR_W - 1 if unit == 1 else W - SIDEBAR_W
    s.line(dx, 0, dx, H, C["GREY"], 1)
    return s, ox


def panel_rows(s, ox, rows, x=580, w=360, nrows=8, size=26):
    """The standard right-hand 8-row readout panel used by most InfoDisp screens.

    rows: list of (label, value, colour) or None for a blank row, or
          ((l1,v1,c1),(l2,v2,c2)) for a split row.
    """
    rh = (H - TITLE_TOP) // nrows          # 67 px at nrows = 8
    for i, r in enumerate(rows[:nrows]):
        y = TITLE_TOP + i * rh + 2
        if r is None:
            continue
        if isinstance(r[0], tuple):
            hw = w // 2
            for j, sub in enumerate(r):
                s.row(ox + x + j * hw, y, hw, rh - 4, sub[0], sub[1],
                      C["WHITE"], sub[2], 18)
        else:
            s.row(ox + x, y, w, rh - 4, r[0], r[1], C["WHITE"], r[2], size)
    return rh


def caption(s, x, y, text, col=None, size=13, anchor="start"):
    s.text(x, y, text, col or C["LIGHT_GREY"], size, anchor, "normal")


# ─────────────────────────────────────────────────────────────────────────────
# ANNUNCIATOR PANEL  (I2C 0x10)
# ─────────────────────────────────────────────────────────────────────────────
CW_LABELS = [
    # row 0 -- warnings
    ("LOW ΔV", "RED"), ("HIGH G", "RED"), ("HIGH TEMP", "RED"),
    ("BUS VOLTAGE", "RED"), ("ABORT", "RED"),
    # row 1 -- mixed
    ("GROUND PROX", "RED"), ("Pe LOW", "RED"), ("PROP LOW", "RED"),
    ("LIFE SUPPORT", "RED"), ("O2 PRESENT", "NAVY"),
    # row 2 -- cautions
    ("IMPACT IMM", "YELLOW"), ("ALT", "YELLOW"), ("DESCENT", "YELLOW"),
    ("GEAR UP", "YELLOW"), ("ATMO", "YELLOW"),
    # row 3 -- cautions
    ("RCS LOW", "YELLOW"), ("PROP RATIO", "YELLOW"), ("COMM LOST", "YELLOW"),
    ("Ap LOW", "YELLOW"), ("HIGH Q", "YELLOW"),
    # row 4 -- cell order: SRB, ORBIT STABLE, ELEC GEN, CHUTE ENV, EVA ACTIVE
    ("SRB ACTIVE", "INT_ORANGE"), ("ORBIT STABLE", "DARK_GREEN"),
    ("ELEC GEN", "DARK_GREEN"), ("CHUTE ENV", "DARK_GREEN"),
    ("EVA ACTIVE", "INT_ORANGE"),
]
# yellow-lamp legends are drawn dark on yellow; everything else white on colour
CW_DARK_FG = {"YELLOW"}


def ann_main(path):
    s = Svg("Annunciator -- MAIN screen")
    lit = {  # cell index -> lit (mid-ascent example: SRB burning, in atmosphere)
        12: True, 14: True, 19: True, 20: True,
    }
    # MASTER ALARM 274x176
    s.tile(0, 0, 274, 176, "MASTER\nALARM", C["OFF_BLACK"], C["DARK_GREY"],
           C["GREY"], 30, lines=["MASTER", "ALARM"])
    # SOI label 274x48 + globe 274x176
    s.rect(1, 177, 272, 46, C["BLACK"], C["GREY"], 1)
    s.text(9, 208, "SOI:", C["WHITE"], 24)
    s.text(265, 208, "Kerbin", C["DARK_GREEN"], 24, "end")
    s.rect(1, 225, 272, 174, C["BLACK"], C["GREY"], 1)
    s.circle(137, 312, 66, C["OCEAN"], C["GREY"], 1)
    s.path("M 90 288 q 26 -16 46 2 q 22 18 44 -4 q 8 26 -12 44 q -34 26 -66 6 "
           "q -18 -18 -12 -48 Z", C["JUNGLE"])
    caption(s, 137, 392, "body thumbnail — touch to open SOI", C["GREY"], 12, "middle")

    # C&W panel 5 x 5, 120x80, origin (274,0)
    for i, (lab, col) in enumerate(CW_LABELS):
        cx = 274 + (i % 5) * 120
        cy = (i // 5) * 80
        on = lit.get(i, False)
        fill = C[col] if on else C["OFF_BLACK"]
        fg = (C["DARK_GREY"] if col in CW_DARK_FG else C["WHITE"]) if on \
            else C["DARK_GREY"]
        lines = lab.split(" ") if len(lab) > 9 else [lab]
        s.tile(cx, cy, 120, 80, lab, fill, fg, C["GREY"], 15, lines=lines)

    # DOCK vertical indicator 75x100 at (874,0)
    s.rect(875, 1, 73, 98, C["OFF_BLACK"], C["GREY"], 1)
    for i, ch in enumerate("DOCK"):
        s.text(911, 26 + i * 21, ch, C["DARK_GREY"], 18, "middle")
    # regime column 4 x 75x75 at (874,100)
    regimes = ["FLYING LOW", "FLYING HIGH", "LOW SPACE", "HIGH SPACE"]
    for i, r in enumerate(regimes):
        on = (i == 1)
        s.tile(874, 100 + i * 75, 75, 75, r,
               C["DARK_GREEN"] if on else C["OFF_BLACK"],
               C["WHITE"] if on else C["DARK_GREY"], C["GREY"], 12,
               lines=r.split(" "))
    # situation column 8 x 75x50 at (949,0)
    sits = [("CONTACT", "SKY"), ("PRE-\nLAUNCH", "JUNGLE"), ("FLIGHT", "JUNGLE"),
            ("SUB-\nORBIT", "JUNGLE"), ("ORBIT", "JUNGLE"), ("ESCAPE", "JUNGLE"),
            ("LANDED", "JUNGLE"), ("SPLASH", "NAVY")]
    for i, (lab, col) in enumerate(sits):
        on = (i == 2)
        s.tile(949, i * 50, 75, 50, lab,
               C[col] if on else C["OFF_BLACK"],
               C["WHITE"] if on else C["DARK_GREY"], C["GREY"], 12,
               lines=lab.split("\n"))

    # bottom zone -------------------------------------------------------------
    s.line(0, 400, W, 400, C["SILVER"], 2)
    # vessel name (424 wide) + TimeWarp
    s.rect(1, 401, 422, 58, C["BLACK"], C["GREY"], 1)
    s.text(9, 440, "Kerbal X", C["WHITE"], 30)
    s.rect(1, 461, 422, 58, C["BLACK"], C["GREY"], 1)
    s.text(9, 500, "TimeWarp:", C["WHITE"], 26)
    s.text(415, 500, "1x", C["DARK_GREEN"], 26, "end")
    # telemetry triples 200 wide each
    tel = [("STG:", "2"), ("Tmax:", "41%"), ("CREW:", "3")]
    tel2 = [("COMM:", "88%"), ("Tskin:", "36%"), ("CAP:", "0")]
    for i, (l, v) in enumerate(tel):
        s.row(424 + i * 200, 401, 198, 58, l, v, C["WHITE"], C["DARK_GREEN"], 24,
              C["BLACK"], C["GREY"])
    for i, (l, v) in enumerate(tel2):
        s.row(424 + i * 200, 461, 198, 58, l, v, C["WHITE"], C["DARK_GREEN"], 24,
              C["BLACK"], C["GREY"])
    # row 3 -- CtrlGrp (212) + SPCFT tile (212) + mode grid 6x2 of 100x40
    s.rect(1, 521, 210, 78, C["BLACK"], C["GREY"], 1)
    s.text(9, 552, "CtrlGrp:", C["WHITE"], 22)
    s.text(203, 588, "3", C["DARK_GREEN"], 34, "end")
    s.rect(213, 521, 210, 78, C["BLACK"], C["GREY"], 1)
    s.text(288, 573, "SPCFT", C["DARK_GREEN"], 34, "middle")
    s.rect(360, 536, 46, 48, None, C["DARK_GREEN"], 2)
    s.poly([(383, 540), (398, 578), (368, 578)], C["DARK_GREEN"])
    modes = [("DEMO", "BLUE"), ("WARP", "YELLOW"), ("AUDIO", "DARK_GREEN"),
             ("THRTL ENA", "DARK_GREEN"), ("TRIM", "AQUA"), ("AUTOPILOT", "DARK_GREEN"),
             ("DEBUG", "PURPLE"), ("SWITCH ERR", "RED"), ("SIMPIT LOST", "RED"),
             ("THRTL PREC", "DARK_GREEN"), ("INPUT PREC", "DARK_GREEN"),
             ("ENG ARM", "DARK_GREEN")]
    on_idx = {2, 3, 11}
    for i, (lab, col) in enumerate(modes):
        x = 424 + (i % 6) * 100
        y = 520 + (i // 6) * 40
        on = i in on_idx
        fg = (C["DARK_GREY"] if col == "YELLOW" else C["WHITE"]) if on \
            else C["DARK_GREY"]
        s.tile(x, y, 100, 40, lab, C[col] if on else C["OFF_BLACK"], fg,
               C["GREY"], 13)
    s.save(path)


def ann_soi(path):
    s = Svg("Annunciator -- SOI screen")
    # left KASA meatball slot 240x168, right body slot 240x168, name centred
    s.rect(2, 2, 236, 164, C["BLACK"], C["GREY"], 1)
    s.circle(120, 84, 58, C["NAVY"])
    s.ellipse(120, 84, 78, 22, None, C["WHITE"], 2, rot=-20)
    s.text(120, 96, "KASA", C["WHITE"], 30, "middle")
    s.text(512, 110, "KERBIN", C["WHITE"], 72, "middle")
    s.rect(786, 2, 236, 164, C["BLACK"], C["GREY"], 1)
    s.circle(904, 84, 62, C["OCEAN"])
    s.path("M 858 60 q 26 -16 46 2 q 22 18 44 -4 q 8 26 -12 44 q -34 26 -66 6 "
           "q -18 -18 -12 -48 Z", C["JUNGLE"])
    rows = [("Min Safe Alt:", "70.0 km"), ("SOI Radius:", "84.16 Mm"),
            ("Reentry Alt:", "45.0 km"), ("High Atmo Alt:", "18.0 km"),
            ("Low Space Alt:", "70.0 km"), ("High Space Alt:", "250.0 km"),
            ("Condition:", "Home"), ("Surf. Gravity:", "9.81 m/s²")]
    for i, (l, v) in enumerate(rows):
        y = 168 + i * 52
        s.text(8, y + 38, l, C["WHITE"], 34)
        s.text(1016, y + 38, v, C["DARK_GREEN"], 34, "end")
    s.save(path)


def ann_standby(path):
    s = Svg("Annunciator / Resource Display -- STANDBY splash")
    s.rect(0, 0, W, H, C["BLACK"])
    for i in range(60):
        s.circle((i * 137) % W, (i * 271) % H, 1.2, C["GREY"], op=0.5)
    s.circle(512, 300, 150, C["ROYAL"], C["GREY"], 2)
    s.text(512, 288, "JEB'S", C["SILVER"], 44, "middle")
    s.text(512, 336, "CONTROLLER WORKS", C["SILVER"], 30, "middle")
    caption(s, 512, 560, "StandbySplash_1024x600.bmp — no live data on this screen",
            C["GREY"], 16, "middle")
    s.save(path)


# ─────────────────────────────────────────────────────────────────────────────
# RESOURCE DISPLAY  (I2C 0x11)
#   SIDEBAR_W 84 (left), AXIS_W 50, PERC_H 36, LABEL_H 44, BAR_PAD 14
# ─────────────────────────────────────────────────────────────────────────────
RES_SIDEBAR = ["TOTL", "DFLT", "SEL", "DATA"]


def res_sidebar(s, mode_stage=False):
    bh = H // 4
    for i, lab in enumerate(RES_SIDEBAR):
        y = i * bh
        rev = (i == 0 and mode_stage)
        fill = C["GREY"] if rev else C["BLACK"]
        fg = C["BLACK"] if rev else C["WHITE"]
        if i == 0 and mode_stage:
            lab = "STG"
        s.rect(2, y + 3, 80, bh - 6, fill, C["GREY"], 2)
        s.text(42, y + bh / 2 + 9, lab, fg, 24, "middle")
    s.line(83, 0, 83, H, C["GREY"], 1)


def res_bars(s, slots, stage=False):
    axis_x = 84
    bar0 = axis_x + 50
    n = len(slots)
    avail = W - bar0
    bw = (avail - 14 * (n + 1)) / float(n)
    top, bot = 44, H - 44
    # y axis
    for p in range(0, 101, 20):
        y = bot - (bot - top) * p / 100.0
        s.line(bar0 - 8, y, bar0, y, C["GREY"], 1)
        s.text(bar0 - 12, y + 5, "%d" % p, C["GREY"], 14, "end")
    s.line(bar0, top, bar0, bot, C["GREY"], 1)
    for i, (lab, col, frac) in enumerate(slots):
        x = bar0 + 14 + i * (bw + 14)
        bh = (bot - top) * frac
        s.rect(x, bot - bh, bw, bh, C[col])
        s.rect(x, top, bw, bot - top, None, C["DARK_GREY"], 1)
        pct = int(frac * 100)
        pcol = C["WHITE"] if pct >= 30 else (C["YELLOW"] if pct >= 10 else C["RED"])
        s.text(x + bw / 2, 30, "%d%%" % pct, pcol, 22, "middle")
        s.text(x + bw / 2, H - 12, lab, C["WHITE"], 26, "middle")


def res_main(path):
    s = Svg("Resource Display -- MAIN screen (STD preset)")
    res_sidebar(s, False)
    res_bars(s, [("EC", "YELLOW", 0.62), ("LF", "ORANGE", 0.44),
                 ("LOx", "BLUE", 0.44), ("MP", "DARK_GREEN", 0.81),
                 ("SF", "RED", 0.07), ("O2", "SILVER", 0.93),
                 ("FD", "OLIVE", 0.88), ("H2O", "CYAN", 0.90),
                 ("ABL", "VIOLET", 1.00)])
    s.save(path)


def res_main_eva(path):
    s = Svg("Resource Display -- MAIN screen, EVA mode")
    res_sidebar(s, False)
    res_bars(s, [("EC", "YELLOW", 0.24), ("EVA", "MINT", 0.55),
                 ("O2", "SILVER", 0.71), ("FD", "OLIVE", 0.96),
                 ("H2O", "CYAN", 0.94)])
    s.save(path)


RES_GRID = [
    ("EC", "YELLOW"), ("StC", "AIR_SUP_BLUE"), ("LF", "ORANGE"), ("LOx", "BLUE"),
    ("SF", "RED"), ("MP", "DARK_GREEN"), ("XE", "MAGENTA"), ("LH2", "FRENCH_BLUE"),
    ("LMe", "ROYAL"), ("Li", "INT_ORANGE"), ("AIR", "AQUA"), ("EUr", "NEON_GREEN"),
    ("DFu", "SAP_GREEN"), ("ORE", "MAROON"), ("ABL", "VIOLET"), ("O2", "SILVER"),
    ("CO2", "CORNELL"), ("FD", "OLIVE"), ("WST", "BROWN"), ("H2O", "CYAN"),
    ("LWS", "DULL_YELLOW"), ("FER", "UPS_BROWN"), ("EVA", "MINT"),
]


def res_select(path):
    s = Svg("Resource Display -- SELECT screen")
    pad, title_h, preset_h = 6, 48, 44
    top_h = title_h + preset_h
    grid_w = (W * 3) // 4                      # 768
    cols, rows = 5, 5
    bw = (grid_w - pad * (cols + 1)) // cols
    bh = (H - top_h - pad * (rows + 1)) // rows
    s.text(pad + 6, 38, "Select Resources", C["WHITE"], 34)
    back_w, back_x = 110, W - 110 - pad
    s.text(back_x - 14, 34, "9 / 16", C["GREY"], 18, "end")
    s.rect(back_x, pad, back_w, top_h - pad * 2, C["BLACK"], C["GREY"], 2)
    s.text(back_x + back_w / 2, top_h / 2 + 10, "BACK", C["WHITE"], 24, "middle")
    pw = (back_x - pad * 7) // 6
    for i, p in enumerate(["STD", "XPD", "VEH", "LSP", "AIR", "ADV"]):
        x = pad + i * (pw + pad)
        s.rect(x, title_h + pad, pw, preset_h - pad * 2, C["BLACK"], C["GREY"], 2)
        s.text(x + pw / 2, title_h + preset_h / 2 + 7, p, C["WHITE"], 20, "middle")
    sel = {"EC", "LF", "LOx", "MP", "SF", "O2", "FD", "H2O", "ABL"}
    for i, (lab, col) in enumerate(RES_GRID):
        x = pad + (i % cols) * (bw + pad)
        y = top_h + pad + (i // cols) * (bh + pad)
        on = lab in sel
        if lab == "EVA":
            s.rect(x, y, bw, bh, C["BLACK"], C["DARK_GREY"], 1)
            s.text(x + bw / 2, y + bh / 2 + 6, "—", C["DARK_GREY"], 18, "middle")
            continue
        s.rect(x, y, bw, bh, C[col] if on else C["OFF_BLACK"],
               C["WHITE"] if on else C["GREY"], 2)
        s.text(x + bw / 2, y + bh / 2 + 9, lab,
               C["BLACK"] if on and col in ("YELLOW", "SILVER", "MINT", "CYAN",
                                            "NEON_GREEN", "AQUA", "DULL_YELLOW",
                                            "LIGHT_GREY") else C["WHITE"],
               26, "middle")
    px = grid_w + pad * 2
    pwid = W - px - pad
    s.rect(px, top_h + pad, pwid, H - top_h - pad * 2 - 48, C["BLACK"], C["GREY"], 1)
    s.text(px + pwid / 2, top_h + 34, "ORDER", C["WHITE"], 20, "middle")
    order = ["1 EC", "2 LF", "3 LOx", "4 MP", "5 SF", "6 O2", "7 FD", "8 H2O",
             "9 ABL"]
    for i, o in enumerate(order):
        s.text(px + 12, top_h + 68 + i * 34, o, C["LIGHT_GREY"], 22)
    s.rect(px, H - 48 - pad, pwid, 48, C["OFF_BLACK"], C["ORANGE"], 2)
    s.text(px + pwid / 2, H - 20 - pad, "CLEAR", C["ORANGE"], 26, "middle")
    s.save(path)


def res_detail(path):
    s = Svg("Resource Display -- DETAIL screen")
    sel_w, hdr_h = 180, 66
    rh = (H - hdr_h) // 6
    slots = ["EC", "LF", "LOx", "MP", "SF", "O2", "FD", "H2O", "ABL"]
    bh = H // len(slots)
    for i, lab in enumerate(slots):
        on = (lab == "LF")
        s.rect(3, i * bh + 2, sel_w - 6, bh - 4,
               C["ORANGE"] if on else C["OFF_BLACK"],
               C["WHITE"] if on else C["GREY"], 2)
        s.text(sel_w / 2, i * bh + bh / 2 + 9,
               lab, C["BLACK"] if on else C["WHITE"], 26, "middle")
    px = sel_w + 1
    s.line(px, 0, px, H, C["GREY"], 1)
    s.rect(px + 6, 8, 4, hdr_h - 16, C["ORANGE"])
    s.text(px + 22, 50, "Liquid Fuel", C["WHITE"], 48)
    s.rect(W - 116, 6, 110, hdr_h - 12, C["BLACK"], C["GREY"], 2)
    s.text(W - 61, hdr_h / 2 + 9, "BACK", C["WHITE"], 24, "middle")
    secs = [("CRAFT", 0, 3), ("STAGE", 3, 6)]
    labels = ["Available:", "Total:", "Remaining:"]
    vals = [["1440.0", "3240.0", "44%"], ["360.0", "720.0", "50%"]]
    for name, a, b in secs:
        s.rect(px, hdr_h + a * rh, 32, (b - a) * rh, C["OFF_BLACK"], C["GREY"], 1)
        s.text(px + 16, hdr_h + (a + (b - a) / 2.0) * rh, name, C["WHITE"], 20,
               "middle", rot=-90)
        for i in range(3):
            y = hdr_h + (a + i) * rh
            s.text(px + 48, y + rh / 2 + 12, labels[i], C["WHITE"], 32)
            s.text(W - 12, y + rh / 2 + 12, vals[0 if a == 0 else 1][i],
                   C["DARK_GREEN"], 32, "end")
            s.line(px + 32, y + rh, W, y + rh, C["DARK_GREY"], 1)
    s.save(path)


# ─────────────────────────────────────────────────────────────────────────────
# INFO DISPLAY -- shared instrument primitives
# ─────────────────────────────────────────────────────────────────────────────
def eadi_ball(s, cx, cy, r, pitch=8, roll=-18, aircraft=False):
    """The EADI attitude ball used by the SPACECRAFT and AIRCRAFT PFDs.
    EADI_SCALE = r/30 px per degree of pitch."""
    cid = "eadi%d" % int(cx)
    s.parts.append('<clipPath id="%s"><circle cx="%g" cy="%g" r="%g"/></clipPath>'
                   % (cid, cx, cy, r))
    s.parts.append('<g clip-path="url(#%s)" transform="rotate(%g %g %g)">'
                   % (cid, -roll, cx, cy))
    scale = r / 30.0
    hy = cy + pitch * scale
    s.rect(cx - 2 * r, hy - 2 * r, 4 * r, 2 * r, C["ROYAL"])
    s.rect(cx - 2 * r, hy, 4 * r, 2 * r, C["UPS_BROWN"])
    s.line(cx - 2 * r, hy, cx + 2 * r, hy, C["WHITE"], 2)
    for d in range(-30, 31, 5):
        if d == 0:
            continue
        y = hy - d * scale
        half = 47 if d % 10 == 0 else 29
        s.line(cx - half, y, cx + half, y, C["WHITE"], 1)
        if d % 10 == 0:
            s.text(cx - half - 8, y + 5, str(abs(d)), C["WHITE"], 15, "end")
            s.text(cx + half + 8, y + 5, str(abs(d)), C["WHITE"], 15)
    s.parts.append("</g>")
    s.circle(cx, cy, r, None, C["SILVER"], 2)
    # roll scale + pointer
    for t in (-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60):
        a = math.radians(t - 90)
        x1, y1 = cx + (r + 3) * math.cos(a), cy + (r + 3) * math.sin(a)
        x2, y2 = cx + (r + 16) * math.cos(a), cy + (r + 16) * math.sin(a)
        s.line(x1, y1, x2, y2, C["WHITE"], 2 if t % 30 == 0 else 1)
    for t in (-60, -30, 30, 60):
        a = math.radians(t - 90)
        s.text(cx + (r + 30) * math.cos(a), cy + (r + 30) * math.sin(a) + 5,
               str(abs(t)), C["WHITE"], 15, "middle")
    a = math.radians(roll - 90)
    tip = (cx + (r + 3) * math.cos(a), cy + (r + 3) * math.sin(a))
    bl = (cx + (r + 22) * math.cos(a + 0.05), cy + (r + 22) * math.sin(a + 0.05))
    br = (cx + (r + 22) * math.cos(a - 0.05), cy + (r + 22) * math.sin(a - 0.05))
    s.poly([tip, bl, br], C["YELLOW"])
    # fixed boresight symbol
    if aircraft:
        s.line(cx - 60, cy, cx - 20, cy, C["YELLOW"], 4)
        s.line(cx + 20, cy, cx + 60, cy, C["YELLOW"], 4)
        s.line(cx - 20, cy, cx - 10, cy + 12, C["YELLOW"], 4)
        s.line(cx - 10, cy + 12, cx, cy, C["YELLOW"], 4)
        s.line(cx, cy, cx + 10, cy + 12, C["YELLOW"], 4)
        s.line(cx + 10, cy + 12, cx + 20, cy, C["YELLOW"], 4)
    else:
        s.circle(cx, cy, 7, None, C["YELLOW"], 3)
        s.line(cx - 46, cy, cx - 12, cy, C["YELLOW"], 4)
        s.line(cx + 12, cy, cx + 46, cy, C["YELLOW"], 4)
        s.line(cx, cy - 46, cx, cy - 12, C["YELLOW"], 4)


def vtape(s, x, y, w, h, ticks, marker=None, label=None, fill=None,
          tickfmt="%d", mcol=None, tcol=None):
    """A generic vertical tape/bar gauge: border, ticks, optional fill + marker."""
    s.rect(x, y, w, h, C["OFF_BLACK"], C["GREY"], 1)
    if fill is not None:
        fh = h * fill
        s.rect(x + 1, y + h - fh, w - 2, fh, tcol or C["DARK_GREEN"])
    for frac, lab in ticks:
        ty = y + h * (1 - frac)
        s.line(x - 6, ty, x, ty, C["GREY"], 1)
        s.text(x - 9, ty + 5, lab, C["GREY"], 13, "end")
    if marker is not None:
        my = y + h * (1 - marker)
        s.poly([(x + w + 2, my), (x + w + 14, my - 7), (x + w + 14, my + 7)],
               mcol or C["WHITE"])
    if label:
        s.text(x + w / 2, y - 6, label, C["WHITE"], 16, "middle")


def compass_card(s, cx, cy, r, hdg=45, trk=None, brg=None, rover=True):
    """The shared rotating compass card (Compass.ino) used by ROVER and NAV."""
    s.circle(cx, cy, r, C["BLACK"], C["SILVER"], 2)
    for d in range(0, 360, 5):
        a = math.radians(d - hdg - 90)
        outer = r - 4
        inner = r - 22 if d % 30 == 0 else r - 15
        s.line(cx + outer * math.cos(a), cy + outer * math.sin(a),
               cx + inner * math.cos(a), cy + inner * math.sin(a),
               C["WHITE"], 2 if d % 30 == 0 else 1)
    for d, lab in ((0, "N"), (90, "E"), (180, "S"), (270, "W")):
        a = math.radians(d - hdg - 90)
        s.text(cx + (r - 45) * math.cos(a), cy + (r - 45) * math.sin(a) + 10,
               lab, C["WHITE"], 28, "middle")
    for d in (30, 60, 120, 150, 210, 240, 300, 330):
        a = math.radians(d - hdg - 90)
        s.text(cx + (r - 45) * math.cos(a), cy + (r - 45) * math.sin(a) + 7,
               "%02d" % (d // 10), C["LIGHT_GREY"], 18, "middle")
    # fixed nose index at 12 o'clock
    s.poly([(cx, cy - r - 4), (cx - 12, cy - r - 19), (cx + 12, cy - r - 19)],
           C["YELLOW"])
    if trk is not None:
        a = math.radians(trk - hdg - 90)
        tip = (cx + (r - 96) * math.cos(a), cy + (r - 96) * math.sin(a))
        s.poly([tip,
                (cx + (r - 78) * math.cos(a + 0.10), cy + (r - 78) * math.sin(a + 0.10)),
                (cx + (r - 78) * math.cos(a - 0.10), cy + (r - 78) * math.sin(a - 0.10))],
               C["NEON_GREEN"])
    if brg is not None:
        a = math.radians(brg - hdg - 90)
        tip = (cx + (r - 96) * math.cos(a), cy + (r - 96) * math.sin(a))
        s.poly([tip,
                (cx + (r - 78) * math.cos(a + 0.10), cy + (r - 78) * math.sin(a + 0.10)),
                (cx + (r - 78) * math.cos(a - 0.10), cy + (r - 78) * math.sin(a - 0.10))],
               C["VIOLET"])
    if rover:
        s.rect(cx - 27, cy - 45, 54, 90, C["OFF_BLACK"], C["SILVER"], 2)
        for wy in (-29, 21):
            s.rect(cx - 39, cy + wy, 12, 24, C["SILVER"])
            s.rect(cx + 27, cy + wy, 12, 24, C["SILVER"])
        s.poly([(cx, cy - 60), (cx - 10, cy - 45), (cx + 10, cy - 45)], C["SILVER"])
    else:
        s.poly([(cx, cy - 52), (cx - 6, cy - 26), (cx - 66, cy + 6),
                (cx - 66, cy + 20), (cx - 6, cy + 4), (cx - 6, cy + 34),
                (cx - 24, cy + 46), (cx - 24, cy + 54), (cx, cy + 46),
                (cx + 24, cy + 54), (cx + 24, cy + 46), (cx + 6, cy + 34),
                (cx + 6, cy + 4), (cx + 66, cy + 20), (cx + 66, cy + 6),
                (cx + 6, cy - 26)], C["SILVER"])


def reticle(s, cx, cy, r, rings, ring_labels, vel=None, tgt=None, mnvr=None,
            note=None, bar=None, barlab=None, barval=None, ox=0):
    """The shared MNVR / TGT / DOCK reticle disc + bottom ΔV / closure bar."""
    s.circle(cx, cy, r, C["BLACK"], C["SILVER"], 2)
    for frac, col, lab in zip(rings, (C["YELLOW"], C["RED"], C["GREY"]),
                              ring_labels):
        s.circle(cx, cy, r * frac, None, col, 1, dash="4 6")
        d = r * frac * 0.7071
        s.text(cx + d + 6, cy - d - 4, lab, col, 15)
    s.line(cx - r, cy, cx + r, cy, C["DARK_GREY"], 1)
    s.line(cx, cy - r, cx, cy + r, C["DARK_GREY"], 1)
    # fixed nose crosshair
    s.line(cx - 26, cy, cx - 8, cy, C["WHITE"], 3)
    s.line(cx + 8, cy, cx + 26, cy, C["WHITE"], 3)
    s.line(cx, cy - 26, cx, cy - 8, C["WHITE"], 3)
    s.line(cx, cy + 8, cx, cy + 26, C["WHITE"], 3)
    s.text(cx + 30, cy + 42, "NOSE", C["WHITE"], 14)
    if note:
        s.text(cx - r, cy - r - 14, note, C["GREY"], 15)
    if tgt:
        tx, ty = cx + tgt[0], cy + tgt[1]
        s.circle(tx, ty, 13, None, C["VIOLET"], 3)
        s.line(tx - 20, ty, tx + 20, ty, C["VIOLET"], 2)
        s.line(tx, ty - 20, tx, ty + 20, C["VIOLET"], 2)
        s.text(tx + 22, ty - 16, "TGT", C["VIOLET"], 14)
    if mnvr:
        mx, my = cx + mnvr[0], cy + mnvr[1]
        s.circle(mx, my, 13, None, C["SKY"], 3)
        for a in (0, 120, 240):
            rad = math.radians(a)
            s.line(mx + 13 * math.cos(rad), my + 13 * math.sin(rad),
                   mx + 26 * math.cos(rad), my + 26 * math.sin(rad), C["SKY"], 3)
        s.text(mx + 26, my - 18, "NODE", C["SKY"], 14)
    if vel:
        vx, vy = cx + vel[0], cy + vel[1]
        s.circle(vx, vy, 12, None, C["NEON_GREEN"], 3)
        for a in (90, 210, 330):
            rad = math.radians(a)
            s.line(vx + 12 * math.cos(rad), vy + 12 * math.sin(rad),
                   vx + 24 * math.cos(rad), vy + 24 * math.sin(rad),
                   C["NEON_GREEN"], 3)
        s.text(vx + 24, vy + 26, "VEL", C["NEON_GREEN"], 14)
    if bar is not None:
        bw, bx = 450, cx - 225
        by = cy + r + 22
        s.text(bx, by - 8, barlab, C["WHITE"], 18)
        s.text(bx + bw, by - 8, barval, C["DARK_GREEN"], 18, "end")
        s.rect(bx, by, bw, 26, C["OFF_BLACK"], C["GREY"], 1)
        s.rect(bx + 1, by + 1, (bw - 2) * bar, 24, C["DARK_GREEN"])


# ─────────────────────────────────────────────────────────────────────────────
# INFO DISPLAY screens
# ─────────────────────────────────────────────────────────────────────────────
def ref_chip(s, ox, text, held=False, trim=False):
    """The velocity/altitude reference chip (and the TRIM annunciation beside it)
    on the SPACECRAFT and AIRCRAFT PFDs -- 52x24 at content (120, 475)."""
    col = C["DARK_GREEN"] if held else C["GREY"]
    s.rect(ox + 120, 475, 52, 24, C["BLACK"], col, 2, rx=5)
    s.text(ox + 146, 492, text, col, 15, "middle")
    s.rect(ox + 526, 475, 52, 24, C["BLACK"],
           C["AQUA"] if trim else C["DARK_GREY"], 2, rx=5)
    s.text(ox + 552, 492, "TRIM", C["AQUA"] if trim else C["DARK_GREY"], 14, "middle")


def info_chrome(path):
    s, ox = info_frame("SPACECRAFT", 0, unit=1, chip="AUTO")
    s.rect(ox + 6, TITLE_TOP + 6, CONTENT_W - 12, H - TITLE_TOP - 12,
           C["OFF_BLACK"], C["DARK_GREY"], 1)
    s.text(ox + CONTENT_W / 2, 150, "CONTENT AREA", C["LIGHT_GREY"], 34, "middle")
    s.text(ox + CONTENT_W / 2, 184, "940 × 538 px", C["GREY"], 22, "middle")
    s.text(ox + CONTENT_W / 2, 224,
           "graphics on the outboard side, a 360 px eight-row readout panel inboard",
           C["GREY"], 17, "middle", "normal")

    def badge(n, cx, cy):
        s.circle(cx, cy, 14, C["YELLOW"], C["BLACK"], 2)
        s.text(cx, cy + 6, str(n), C["BLACK"], 17, "middle")

    badge(1, ox + 40, 29)
    badge(2, ox + CONTENT_W - 98, 18)
    badge(4, ox + 26, 84)
    badge(3, ox + 26, 300)
    s.line(SIDEBAR_W, 84, ox + 12, 84, C["YELLOW"], 1, dash="3 3")
    s.line(SIDEBAR_W, 300, ox + 12, 300, C["YELLOW"], 1, dash="3 3")

    legend = [
        (1, "Title bar — the screen's own name (58 px, plus a 4 px rule)"),
        (2, "AUTO / MAN chip — did the panel choose this screen, or did you?"),
        (3, "Sidebar — six keys on the panel's outboard edge"),
        (4, "The selected key reverse-videos: black on grey"),
    ]
    for i, (n, t) in enumerate(legend):
        y = 358 + i * 40
        badge(n, ox + 76, y - 6)
        s.text(ox + 102, y, t, C["LIGHT_GREY"], 19, "start", "normal")
    s.text(ox + 62, 556,
           "Unit 1 is drawn here — on unit 2 the sidebar sits on the right instead.",
           C["GREY"], 17, "start", "normal")
    s.save(path)


def info_prelaunch(path):
    s, ox = info_frame("LAUNCH", 1, unit=2, chip="AUTO")
    s.text(ox + CONTENT_W / 2, 118, "PRE-LAUNCH", C["YELLOW"], 44, "middle")
    left = [("Vessel:", "Kerbal X", C["WHITE"]), ("Type:", "Ship", C["DARK_GREEN"])]
    y = 150
    for l, v, c in left:
        s.row(ox + 20, y, CONTENT_W - 40, 52, l, v, C["WHITE"], c, 30)
        y += 58
    pairs = [
        (("SAS:", "ON", C["DARK_GREEN"]), ("RCS:", "ON", C["DARK_GREEN"])),
        (("Thrtl:", "100%", C["DARK_GREEN"]), ("EC%:", "97%", C["DARK_GREEN"])),
        (("Crew:", "3 / 3", C["DARK_GREEN"]), ("Comm:", "100%", C["DARK_GREEN"])),
        (("Drogue:", "STOWED", C["DARK_GREEN"]), ("Main:", "STOWED", C["DARK_GREEN"])),
        (("D.Cut:", "CAG 2", C["LIGHT_GREY"]), ("M.Cut:", "CAG 4", C["LIGHT_GREY"])),
    ]
    hw = (CONTENT_W - 40) / 2
    for (a, b) in pairs:
        s.row(ox + 20, y, hw, 52, a[0], a[1], C["WHITE"], a[2], 28)
        s.row(ox + 20 + hw, y, hw, 52, b[0], b[1], C["WHITE"], b[2], 28)
        y += 58
    s.text(ox + 20, 578, "ΔV.Tot:", C["WHITE"], 22)
    s.text(ox + 240, 578, "4 250 m/s", C["DARK_GREEN"], 22, "end")
    s.text(ox + CONTENT_W - 20, 578, "touch anywhere to dismiss",
           C["GREY"], 18, "end", "normal")
    s.save(path)


def info_ascent(path):
    s, ox = info_frame("LAUNCH", 1, unit=2, chip="AUTO")
    top, bot = 80, 582
    # altitude ladder, tick label column 0..66, line at 72, markers at 82
    s.line(ox + 72, top, ox + 72, bot, C["SILVER"], 2)
    for i, lab in enumerate(["100 km", "80 km", "60 km", "40 km", "20 km", "GND"]):
        y = top + i * (bot - top) / 5.0
        s.line(ox + 62, y, ox + 72, y, C["SILVER"], 2)
        s.text(ox + 58, y + 5, lab, C["LIGHT_GREY"], 15, "end")
    s.line(ox + 72, top + 90, ox + 150, top + 90, C["SKY"], 1, dash="5 5")
    s.poly([(ox + 82, 300), (ox + 96, 293), (ox + 96, 307)], C["WHITE"])
    s.text(ox + 100, 305, "ALT", C["WHITE"], 15)
    s.poly([(ox + 82, 170), (ox + 96, 163), (ox + 96, 177)], None, C["SKY"], 2)
    s.text(ox + 100, 175, "Ap", C["SKY"], 15)
    # ATMO gauge -- name + bar only, no value window
    ax = ox + 166
    s.text(ax + 22, 100, "ATMO", C["WHITE"], 17, "middle")
    s.parts.append('<linearGradient id="atm" x1="0" y1="0" x2="0" y2="1">'
                   '<stop offset="0" stop-color="%s"/>'
                   '<stop offset="1" stop-color="%s"/></linearGradient>'
                   % (C["NAVY"], C["SKY"]))
    s.rect(ax, 130, 44, 432, "url(#atm)", C["GREY"], 1)
    s.poly([(ax - 2, 380), (ax - 14, 373), (ax - 14, 387)], C["WHITE"])
    s.poly([(ax + 46, 380), (ax + 58, 373), (ax + 58, 387)], C["WHITE"])
    s.text(ax + 22, 578, "GND", C["GREY"], 13, "middle")
    # V.Vrt and V.Orb bar gauges -- name, value window, bar
    for gx, name, val, frac, top_lab, bot_lab in (
            (ox + 240, "V.Vrt", "+412", 0.71, "+500 m/s", "-500 m/s"),
            (ox + 330, "V.Orb", "1 843", 0.62, "2 500 m/s", "0")):
        s.text(gx + 22, 100, name, C["WHITE"], 17, "middle")
        s.rect(gx - 12, 108, 68, 26, C["BLACK"], C["GREY"], 1)
        s.text(gx + 22, 128, val, C["DARK_GREEN"], 19, "middle")
        s.text(gx + 22, 152, top_lab, C["GREY"], 12, "middle")
        s.rect(gx, 158, 44, 400, C["OFF_BLACK"], C["GREY"], 1)
        s.rect(gx + 1, 158 + 400 * (1 - frac), 42, 400 * frac, C["DARK_GREEN"])
        s.text(gx + 22, 574, bot_lab, C["GREY"], 12, "middle")
    # FPA dial + heading tape
    s.circle(ox + 470, 250, 74, C["BLACK"], C["SILVER"], 2)
    for d in range(-90, 91, 30):
        a = math.radians(d)
        s.line(ox + 470 + 62 * math.cos(a), 250 - 62 * math.sin(a),
               ox + 470 + 74 * math.cos(a), 250 - 74 * math.sin(a), C["WHITE"], 1)
    a = math.radians(38)
    s.line(ox + 470, 250, ox + 470 + 68 * math.cos(a), 250 - 68 * math.sin(a),
           C["NEON_GREEN"], 3)
    s.text(ox + 470, 344, "FPA  38°", C["WHITE"], 20, "middle")
    s.rect(ox + 400, 380, 140, 40, C["BLACK"], C["GREY"], 2)
    s.text(ox + 470, 408, "090°", C["DARK_GREEN"], 26, "middle")
    s.text(ox + 470, 440, "HDG", C["WHITE"], 16, "middle")
    # right column -- seven numeric rows
    rows = [("T+Ap:", "1m 12s", C["DARK_GREEN"]), ("Thrtl:", "100%", C["DARK_GREEN"]),
            ("Q:", "24.1 kPa", C["YELLOW"]), ("Mach:", "2.41", C["DARK_GREEN"]),
            ("G:", "2.1 g", C["DARK_GREEN"]), ("Stg.Brn:", "84 s", C["DARK_GREEN"]),
            ("ΔV.Stg:", "1 620 m/s", C["DARK_GREEN"])]
    rh = (H - TITLE_TOP) // 7
    for i, (l, v, c) in enumerate(rows):
        s.row(ox + 580, TITLE_TOP + i * rh + 2, 360, rh - 4, l, v,
              C["WHITE"], c, 28)
    s.save(path)


def info_circ(path):
    s, ox = info_frame("LAUNCH", 1, unit=2, chip="MAN")
    # attitude alignment disc (left rail)
    s.circle(ox + 74, 172, 58, C["BLACK"], C["SILVER"], 2)
    s.circle(ox + 74, 172, 29, None, C["DARK_GREEN"], 1, dash="3 4")
    s.line(ox + 54, 172, ox + 94, 172, C["WHITE"], 2)
    s.line(ox + 74, 152, ox + 74, 192, C["WHITE"], 2)
    s.circle(ox + 86, 160, 8, None, C["NEON_GREEN"], 3)
    s.text(ox + 74, 244, "ATT", C["WHITE"], 20, "middle")
    s.text(ox + 74, 274, "Ecc:", C["WHITE"], 18, "middle")
    s.text(ox + 74, 300, "0.0021", C["DARK_GREEN"], 24, "middle")
    s.text(ox + 74, 384, "Ap-Pe:", C["WHITE"], 18, "middle")
    s.text(ox + 74, 412, "6.4 km", C["DARK_GREEN"], 24, "middle")
    # orbit diagram
    s.ellipse(ox + 348, 272, 175, 128, None, C["SILVER"], 2)
    s.circle(ox + 348, 272, 44, C["OCEAN"], C["GREY"], 1)
    s.circle(ox + 523, 272, 8, C["SKY"])
    s.text(ox + 536, 268, "Ap", C["SKY"], 18)
    s.circle(ox + 173, 272, 8, C["MAGENTA"])
    s.text(ox + 130, 268, "Pe", C["MAGENTA"], 18)
    # ΔV burn bar + T+Ign
    s.text(ox + 40, 500, "ΔV Burn", C["WHITE"], 22)
    s.text(ox + 536, 500, "245 m/s", C["DARK_GREEN"], 22, "end")
    s.rect(ox + 40, 518, 496, 26, C["OFF_BLACK"], C["GREY"], 1)
    s.rect(ox + 41, 519, 300, 24, C["DARK_GREEN"])
    s.row(ox + 40, 551, 496, 40, "T+Ign:", "0m 42s", C["WHITE"], C["YELLOW"], 28)
    # apsis convergence tape (right third)
    tx, rail = ox + 580, ox + 676
    s.text(tx + 6, 84, "APSIS CONVERGENCE", C["WHITE"], 18)
    s.line(rail, 104, rail, 460, C["SILVER"], 2)
    for i, lab in enumerate(["120k", "105k", "90k", "75k", "60k"]):
        y = 104 + i * 89
        s.line(rail - 9, y, rail, y, C["SILVER"], 1)
        s.text(rail - 13, y + 5, lab, C["GREY"], 14, "end")
    s.line(rail, 282, rail + 240, 282, C["DARK_GREEN"], 1, dash="6 5")
    s.text(rail + 244, 286, "safe", C["DARK_GREEN"], 13)
    s.poly([(rail, 148), (rail + 18, 139), (rail + 18, 157)], C["SKY"])
    s.text(rail + 27, 154, "Ap  102.4 km", C["SKY"], 18)
    s.poly([(rail, 246), (rail + 18, 237), (rail + 18, 255)], C["MAGENTA"])
    s.text(rail + 27, 252, "Pe   96.0 km", C["MAGENTA"], 18)
    s.line(rail + 156, 148, rail + 156, 246, C["WHITE"], 1)
    s.line(rail + 150, 148, rail + 162, 148, C["WHITE"], 1)
    s.line(rail + 150, 246, rail + 162, 246, C["WHITE"], 1)
    rows = [("ΔV.Circ:", "245 m/s"), ("Burn Dur:", "0m 38s"), ("Stg.Brn:", "84 s")]
    for i, (l, v) in enumerate(rows):
        s.row(tx, 476 + i * 42, 360, 38, l, v, C["WHITE"], C["DARK_GREEN"], 24)
    s.save(path)


def info_ascentap(path):
    s, ox = info_frame("ASCENT AUTOPILOT", 5, unit=2, chip="MAN", asc_armed=True)
    # phase banner
    s.rect(ox + 6, TITLE_TOP, CONTENT_W - 12, 64, C["OFF_BLACK"], C["DARK_GREEN"], 2)
    s.text(ox + CONTENT_W / 2, TITLE_TOP + 44, "ARMED — GRAVITY TURN",
           C["DARK_GREEN"], 34, "middle")
    cols = [(6, "MISSION", [("Tgt Ap:", "100 km", False), ("Inc:", "0.0°", False),
                            ("Launch:", "NORTH", False)]),
            (322, "VEH PROFILE", [("Loft:", "0.55", False), ("Roll:", "OFF", False),
                                  ("Max-G:", "3.0", True)]),
            (638, "GUIDANCE", [("Pitch:", "42.6°", None), ("Hdg:", "090°", None),
                               ("Thrtl:", "100%", None), ("G:", "1.9", None),
                               ("Q:", "12.4 kPa", None), ("ApA:", "62.1 km", None),
                               ("PeA:", "-410 km", None)])]
    for cx, hdr, rows in cols:
        s.text(ox + cx + 149, 154, hdr, C["WHITE"], 24, "middle")
        for i, (l, v, pend) in enumerate(rows):
            y = 172 + i * 58
            s.text(ox + cx + 6, y + 36, l, C["LIGHT_GREY"], 24)
            if pend is None:                          # output — no box
                s.text(ox + cx + 292, y + 36, v, C["DARK_GREEN"], 26, "end")
            else:
                bc = C["SKY"] if pend else C["GREY"]
                vc = C["SKY"] if pend else C["DARK_GREEN"]
                s.rect(ox + cx + 116, y + 4, 176, 50, C["BLACK"], bc, 2)
                s.text(ox + cx + 284, y + 38, v, vc, 26, "end")
    s.line(ox + 314, 132, ox + 314, 590, C["DARK_GREY"], 1)
    s.line(ox + 630, 132, ox + 630, 590, C["DARK_GREY"], 1)
    # ARM button
    s.rect(ox + 322, 460, 298, 136, C["OFF_BLACK"], C["DARK_GREEN"], 3)
    s.text(ox + 471, 530, "DISARM", C["DARK_GREEN"], 44, "middle")
    s.text(ox + 471, 566, "autopilot reports ARMED", C["LIGHT_GREY"], 15, "middle")
    s.save(path)


def info_orbit(path):
    s, ox = info_frame("ORBIT", 2, unit=2, chip="AUTO")
    s.line(ox + 470, TITLE_TOP, ox + 470, H, C["DARK_GREY"], 1)
    s.line(ox + 6, 92, ox + 934, 92, C["DARK_GREY"], 1)
    s.text(ox + 235, 86, "APSIDES", C["WHITE"], 22, "middle")
    s.text(ox + 705, 86, "INCLINATION", C["WHITE"], 22, "middle")
    # plan view
    s.ellipse(ox + 235, 265, 170, 124, None, C["SILVER"], 2)
    s.circle(ox + 235, 265, 48, C["OCEAN"], C["GREY"], 1)
    s.circle(ox + 405, 265, 8, C["SKY"]); s.text(ox + 416, 261, "Ap", C["SKY"], 17)
    s.circle(ox + 65, 265, 8, C["MAGENTA"]); s.text(ox + 24, 261, "Pe", C["MAGENTA"], 17)
    s.poly([(ox + 300, 190), (ox + 316, 200), (ox + 300, 210)], C["NEON_GREEN"])
    # inclination view
    s.circle(ox + 705, 265, 24, C["OCEAN"], C["GREY"], 1)
    s.line(ox + 545, 265, ox + 865, 265, C["DARK_GREY"], 1, dash="5 5")
    s.line(ox + 552, 300, ox + 858, 230, C["SILVER"], 2)
    s.text(ox + 862, 226, "28.5°", C["SILVER"], 18)
    rows_l = [("SMA:", "700.0 km"), ("Ecc:", "0.0021"), ("PeA:", "96.0 km"),
              ("ApA:", "102.4 km")]
    rows_r = [("Inc:", "28.5°"), ("Period:", "32m 10s"), ("Arg.Pe:", "104.6°"),
              ("T+Ap:", "8m 42s")]
    for i, (l, v) in enumerate(rows_l):
        s.row(ox + 6, 448 + i * 37, 456, 34, l, v, C["WHITE"], C["DARK_GREEN"], 24)
    for i, (l, v) in enumerate(rows_r):
        s.row(ox + 476, 448 + i * 37, 458, 34, l, v, C["WHITE"], C["DARK_GREEN"], 24)
    s.save(path)


def info_orbadv(path):
    s, ox = info_frame("ORBIT ADVANCED", 2, unit=2, chip="MAN")
    left = [("SMA:", "700.0 km"), ("Ecc:", "0.0021"), ("PeA:", "96.0 km"),
            ("ApA:", "102.4 km"), ("Alt.SL:", "99.8 km"), ("V.Orb:", "2 246 m/s"),
            ("Period:", "32m 10s")]
    right = [("Inc:", "28.51°"), ("LAN:", "142.08°"), ("Arg.Pe:", "104.62°"),
             ("True Anom:", "212.44°"), ("Mean Anom:", "213.01°"),
             ("T+Pe:", "24m 03s"), ("T+Ap:", "8m 42s")]
    for i, (l, v) in enumerate(left):
        y = 90 + i * 73
        s.text(ox + 14, y + 36, l, C["WHITE"], 30)
        s.text(ox + 462, y + 36, v, C["DARK_GREEN"], 30, "end")
    for i, (l, v) in enumerate(right):
        y = 90 + i * 73
        s.text(ox + 480, y + 36, l, C["WHITE"], 30)
        s.text(ox + 930, y + 36, v, C["DARK_GREEN"], 30, "end")
    s.line(ox + 470, TITLE_TOP + 4, ox + 470, H - 4, C["DARK_GREY"], 1)
    s.save(path)


def info_spacecraft(path):
    s, ox = info_frame("SPACECRAFT", 0, unit=1, chip="AUTO")
    eadi_ball(s, ox + 345, 300, 206, pitch=12, roll=-22, aircraft=False)
    # navball markers
    s.circle(ox + 300, 244, 11, None, C["NEON_GREEN"], 3)
    s.text(ox + 314, 234, "PRO", C["NEON_GREEN"], 13)
    s.circle(ox + 402, 348, 11, None, C["VIOLET"], 3)
    s.text(ox + 416, 340, "TGT", C["VIOLET"], 13)
    # throttle strip
    s.rect(ox + 2, 100, 18, 424, C["OFF_BLACK"], C["GREY"], 1)
    s.rect(ox + 3, 100 + 424 * 0.25, 16, 424 * 0.75, C["DARK_GREEN"])
    s.text(ox + 11, 588, "THR", C["WHITE"], 15, "middle", rot=-90)
    s.text(ox + 30, 540, "75%", C["DARK_GREEN"], 14, "middle")
    # pitch tape
    s.rect(ox + 76, 94, 36, 420, C["OFF_BLACK"], C["GREY"], 1)
    for d in range(-30, 31, 10):
        y = 300 + d * (206 / 30.0)
        s.line(ox + 76, y, ox + 92, y, C["WHITE"], 1)
        s.text(ox + 96, y + 5, str(abs(d)), C["GREY"], 13)
    s.rect(ox + 44, 281, 68, 38, C["BLACK"], C["WHITE"], 2)
    s.text(ox + 106, 308, "12", C["DARK_GREEN"], 26, "end")
    # heading tape
    s.rect(ox + 112, 514, 466, 32, C["OFF_BLACK"], C["GREY"], 1)
    for i in range(-3, 4):
        x = ox + 345 + i * 66
        s.line(x, 514, x, 526, C["WHITE"], 1)
        s.text(x, 542, "%03d" % ((90 + i * 10) % 360), C["LIGHT_GREY"], 14, "middle")
    s.rect(ox + 309, 510, 72, 40, C["BLACK"], C["WHITE"], 2)
    s.text(ox + 345, 540, "090", C["DARK_GREEN"], 24, "middle")
    ref_chip(s, ox, "ORB", held=False, trim=True)
    # vitals strip EC / CORE / SKIN
    for i, (lab, frac, col) in enumerate((("EC", 0.82, "DARK_GREEN"),
                                          ("CORE", 0.36, "DARK_GREEN"),
                                          ("SKIN", 0.78, "YELLOW"))):
        y = 556 + i * 14
        s.text(ox + 112, y + 9, lab, C["WHITE"], 12, "end")
        s.rect(ox + 116, y, 416, 10, C["OFF_BLACK"], C["GREY"], 1)
        s.rect(ox + 117, y + 1, 414 * frac, 8, C[col])
        s.text(ox + 578, y + 9, "%d%%" % int(frac * 100), C["LIGHT_GREY"], 11, "end")
    # rate bars
    for i, (lab, v) in enumerate((("P", 0.3), ("R", -0.5), ("Y", 0.1))):
        y = 136 + i * 114
        s.text(ox + 567, y - 4, lab, C["WHITE"], 13, "middle")
        s.rect(ox + 557, y, 20, 92, C["OFF_BLACK"], C["GREY"], 1)
        mid = y + 46
        s.line(ox + 557, mid, ox + 577, mid, C["GREY"], 1)
        s.rect(ox + 558, min(mid, mid - 46 * v), 18, abs(46 * v), C["DARK_GREEN"])
    panel_rows(s, ox, [
        ("Alt.SL:", "99.8 km", C["DARK_GREEN"]),
        ("V.Orb:", "2 246 m/s", C["DARK_GREEN"]),
        ("ApA:", "102.4 km", C["DARK_GREEN"]),
        ("PeA:", "96.0 km", C["DARK_GREEN"]),
        ("T+Ap:", "8m 42s", C["DARK_GREEN"]),
        ("T+Ign:", "---", C["GREY"]),
        ("ΔV.Stg:", "1 620 m/s", C["DARK_GREEN"]),
        None])
    s.rect(ox + 580, TITLE_TOP + 7 * 67 + 2, 176, 63, C["BLACK"], C["GREY"], 2)
    s.text(ox + 668, TITLE_TOP + 7 * 67 + 44, "RCS  ON", C["DARK_GREEN"], 24, "middle")
    s.rect(ox + 760, TITLE_TOP + 7 * 67 + 2, 180, 63, C["BLACK"], C["GREY"], 2)
    s.text(ox + 850, TITLE_TOP + 7 * 67 + 44, "SAS PRO", C["DARK_GREEN"], 24, "middle")
    s.save(path)


def info_aircraft(path):
    s, ox = info_frame("AIRCRAFT", 0, unit=1, chip="AUTO", sb=SB_LABELS_U1)
    eadi_ball(s, ox + 345, 300, 206, pitch=6, roll=-24, aircraft=True)
    # slip ball under the roll pointer
    s.rect(ox + 315, 128, 60, 18, C["BLACK"], C["WHITE"], 1, rx=6)
    s.circle(ox + 352, 137, 7, C["WHITE"])
    s.text(ox + 345, 160, "Slip", C["WHITE"], 13, "middle")
    # AoA arc
    s.path("M %g %g A 232 232 0 0 1 %g %g" % (ox + 345 - 232 * 0.5, 300 - 232 * 0.87,
                                              ox + 345 - 232 * 0.87, 300 - 232 * 0.5),
           None, C["DARK_GREEN"], 6)
    s.text(ox + 120, 150, "AoA", C["DARK_GREEN"], 16)
    # VSI strip
    s.rect(ox + 556, 120, 20, 360, C["OFF_BLACK"], C["GREY"], 1)
    s.line(ox + 556, 300, ox + 576, 300, C["GREY"], 1)
    s.rect(ox + 557, 240, 18, 60, C["DARK_GREEN"])
    s.text(ox + 566, 110, "VSI", C["WHITE"], 13, "middle")
    # pitch + heading tapes (same geometry as SCFT)
    s.rect(ox + 76, 94, 36, 420, C["OFF_BLACK"], C["GREY"], 1)
    for d in range(-30, 31, 10):
        y = 300 + d * (206 / 30.0)
        s.line(ox + 76, y, ox + 92, y, C["WHITE"], 1)
        s.text(ox + 96, y + 5, str(abs(d)), C["GREY"], 13)
    s.rect(ox + 44, 281, 68, 38, C["BLACK"], C["WHITE"], 2)
    s.text(ox + 106, 308, "6", C["DARK_GREEN"], 26, "end")
    s.rect(ox + 112, 514, 466, 32, C["OFF_BLACK"], C["GREY"], 1)
    for i in range(-3, 4):
        x = ox + 345 + i * 66
        s.line(x, 514, x, 526, C["WHITE"], 1)
        s.text(x, 542, "%03d" % ((271 + i * 10) % 360), C["LIGHT_GREY"], 14, "middle")
    s.rect(ox + 309, 510, 72, 40, C["BLACK"], C["WHITE"], 2)
    s.text(ox + 345, 540, "271", C["DARK_GREEN"], 24, "middle")
    ref_chip(s, ox, "RDR", held=True, trim=False)
    panel_rows(s, ox, [
        ("Alt.Rdr:", "412 m", C["YELLOW"]),
        ("V.Srf:", "118 m/s", C["DARK_GREEN"]),
        ("IAS:", "104 m/s", C["DARK_GREEN"]),
        ("V.Vrt:", "-3.4 m/s", C["DARK_GREEN"]),
        (("Ma:", "0.34", C["DARK_GREEN"]), ("G:", "1.2", C["DARK_GREEN"])),
        (("AoA:", "7.6°", C["DARK_GREEN"]), ("Slip:", "1.2°", C["DARK_GREEN"])),
        None, None])
    yb = TITLE_TOP + 6 * 67 + 2
    for i, (lab, val, col) in enumerate((("GEAR", "DOWN", "DARK_GREEN"),
                                         ("AIRBRK", "IN", "GREY"))):
        s.rect(ox + 580 + i * 180, yb, 176, 63, C["BLACK"], C["GREY"], 2)
        s.text(ox + 668 + i * 180, yb + 28, lab, C["WHITE"], 18, "middle")
        s.text(ox + 668 + i * 180, yb + 52, val, C[col], 22, "middle")
    s.rect(ox + 580, yb + 67, 356, 63, C["BLACK"], C["GREY"], 2)
    s.text(ox + 758, yb + 108, "BRAKES  OFF", C["WHITE"], 24, "middle")
    s.save(path)


def info_rover(path):
    s, ox = info_frame("ROVER", 0, unit=1, chip="AUTO")
    compass_card(s, ox + 470, 344, 200, hdg=45, brg=118, rover=True)
    s.rect(ox + 415, 62, 110, 52, C["BLACK"], C["WHITE"], 2)
    s.text(ox + 470, 102, "045°", C["DARK_GREEN"], 34, "middle")
    # FWD / REV drive blocks
    s.rect(ox + 6, TITLE_TOP + 4, 150, 60, C["DARK_GREEN"], C["GREY"], 2)
    s.text(ox + 81, TITLE_TOP + 46, "FWD", C["WHITE"], 30, "middle")
    s.rect(ox + 784, TITLE_TOP + 4, 150, 60, C["OFF_BLACK"], C["GREY"], 2)
    s.text(ox + 859, TITLE_TOP + 46, "REV", C["DARK_GREY"], 30, "middle")
    # left column
    s.row(ox + 6, 150, 190, 48, "V.Srf:", "7.4 m/s", C["WHITE"], C["DARK_GREEN"], 20)
    s.row(ox + 6, 206, 190, 48, "Endur:", "2h 14m", C["WHITE"], C["DARK_GREEN"], 20)
    for i, (lab, val, col) in enumerate((("BRAKES", "OFF", "WHITE"),
                                         ("GEAR", "DOWN", "DARK_GREEN"),
                                         ("SAS", "STAB", "AQUA"))):
        y = 320 + i * 74
        s.rect(ox + 6, y, 190, 66, C["BLACK"], C["GREY"], 2)
        s.text(ox + 101, y + 28, lab, C["WHITE"], 18, "middle")
        s.text(ox + 101, y + 54, val, C[col], 24, "middle")
    # right column
    s.row(ox + 750, 150, 190, 48, "Alt.Trn:", "128 m", C["WHITE"], C["DARK_GREEN"], 22)
    s.row(ox + 750, 206, 190, 48, "Pitch:", "+6.2°", C["WHITE"], C["DARK_GREEN"], 22)
    s.row(ox + 750, 262, 190, 48, "Roll:", "-17.4°", C["WHITE"], C["YELLOW"], 22)
    # tilt indicators
    for i, (lab, ang, col) in enumerate((("PITCH", 6.2, "DARK_GREEN"),
                                         ("ROLL", -17.4, "YELLOW"))):
        cx, cy = ox + 845, 372 + i * 100
        s.circle(cx, cy, 38, C["BLACK"], C["SILVER"], 2)
        a = math.radians(ang)
        s.line(cx - 34 * math.cos(a), cy + 34 * math.sin(a),
               cx + 34 * math.cos(a), cy - 34 * math.sin(a), C[col], 3)
        s.text(cx - 52, cy + 5, lab, C["WHITE"], 14, "end")
    # Dist strip
    s.rect(ox + 6, 552, 928, 44, C["BLACK"], C["GREY"], 1)
    s.text(ox + 196, 584, "Dist:", C["WHITE"], 28, "end")
    s.text(ox + 424, 584, "4.62 km", C["DARK_GREEN"], 28, "end")
    s.text(ox + 553, 584, "T+Tgt:", C["WHITE"], 28, "end")
    s.text(ox + 750, 584, "10m 24s", C["DARK_GREEN"], 28, "end")
    s.save(path)


def info_vehicle(path):
    s, ox = info_frame("VEHICLE INFO", 5, unit=1, chip="AUTO")
    secs = [("INFO", 0, 5), ("CREW", 5, 6), ("PROP", 6, 8)]
    rows = [("Vessel:", "Kerbal X", C["WHITE"]),
            ("Type:", "Ship", C["DARK_GREEN"]),
            ("Status:", "IN ORBIT", C["DARK_GREEN"]),
            ("Control:", "Full Control", C["DARK_GREEN"]),
            ("Comm:", "88%", C["DARK_GREEN"]),
            ("Crew:", "3 / 3", C["DARK_GREEN"]),
            ("ΔV.Stg:", "1 620 m/s", C["DARK_GREEN"]),
            ("ΔV.Tot:", "2 940 m/s", C["DARK_GREEN"])]
    rh = (H - TITLE_TOP) // 8
    for name, a, b in secs:
        s.rect(ox + 6, TITLE_TOP + a * rh, 34, (b - a) * rh, C["OFF_BLACK"],
               C["GREY"], 1)
        s.text(ox + 23, TITLE_TOP + (a + (b - a) / 2.0) * rh, name, C["WHITE"], 18,
               "middle", rot=-90)
    for i, (l, v, c) in enumerate(rows):
        y = TITLE_TOP + i * rh
        s.row(ox + 42, y + 2, CONTENT_W - 52, rh - 4, l, v, C["WHITE"], c, 30)
    s.save(path)


def info_maneuver(path):
    s, ox = info_frame("MANEUVER", 2, unit=2, chip="AUTO")
    reticle(s, ox + 289, 300, 210, (0.25, 0.5, 1.0), ("5", "10", "20"),
            mnvr=(-88, -66), note="±20° SCALE",
            bar=0.42, barlab="ΔV Burn", barval="612 m/s")
    panel_rows(s, ox, [
        ("ΔV.Mnvr:", "612 m/s", C["DARK_GREEN"]),
        ("ΔV.Plan:", "612 m/s", C["DARK_GREEN"]),
        ("ΔV.Stg:", "1 620 m/s", C["DARK_GREEN"]),
        ("T+Ign:", "0m 46s", C["YELLOW"]),
        ("T+Mnvr:", "1m 34s", C["DARK_GREEN"]),
        ("Burn Dur:", "0m 48s", C["DARK_GREEN"]),
        (("Brg:", "-8.4°", C["DARK_GREEN"]), ("Elv:", "+6.1°", C["DARK_GREEN"])),
        None])
    yb = TITLE_TOP + 7 * 67 + 2
    s.rect(ox + 580, yb, 176, 63, C["BLACK"], C["GREY"], 2)
    s.text(ox + 668, yb + 42, "RCS  ON", C["DARK_GREEN"], 24, "middle")
    s.rect(ox + 760, yb, 180, 63, C["BLACK"], C["GREY"], 2)
    s.text(ox + 850, yb + 42, "SAS MNV", C["DARK_GREEN"], 24, "middle")
    s.save(path)


def info_target(path):
    s, ox = info_frame("TARGET", 3, unit=2, chip="AUTO")
    reticle(s, ox + 289, 300, 210, (0.25, 0.5, 1.0), ("15", "30", "60"),
            vel=(-96, 62), tgt=(112, -84), note="±60° SCALE",
            bar=0.55, barlab="CLOSURE", barval="-12 m/s")
    panel_rows(s, ox, [
        ("Alt.SL:", "99.8 km", C["DARK_GREEN"]),
        ("V.Orb:", "2 246 m/s", C["DARK_GREEN"]),
        ("Dist:", "1.42 km", C["DARK_GREEN"]),
        ("V.Close:", "-12.4 m/s", C["DARK_GREEN"]),
        (("Brg:", "+18.2°", C["YELLOW"]), ("Elv:", "-9.6°", C["DARK_GREEN"])),
        (("V.Brg:", "-4.1°", C["DARK_GREEN"]), ("V.Elv:", "+2.8°", C["DARK_GREEN"])),
        ("T+Int:", "1m 54s", C["DARK_GREEN"]),
        None])
    s.save(path)


def info_docking(path):
    s, ox = info_frame("DOCKING", 3, unit=2, chip="AUTO")
    reticle(s, ox + 289, 300, 210, (0.25, 0.5, 1.0), ("5", "10", "20"),
            vel=(86, -70), tgt=(26, -20), note="±20° SCALE",
            bar=0.30, barlab="APPROACH", barval="82 m")
    panel_rows(s, ox, [
        (("Dist:", "82 m", C["YELLOW"]), ("T+Dock:", "1m 22s", C["DARK_GREEN"])),
        (("V.Close:", "-1.0 m/s", C["DARK_GREEN"]), ("V.Lat:", "0.14 m/s", C["YELLOW"])),
        (("Brg:", "+2.6°", C["DARK_GREEN"]), ("Elv:", "-1.4°", C["DARK_GREEN"])),
        (("V.Brg:", "+3.9°", C["DARK_GREEN"]), ("V.Elv:", "-0.8°", C["DARK_GREEN"])),
        ("Nos.Off:", "4.2°", C["DARK_GREEN"]),
        None, None, None])
    yb = TITLE_TOP + 5 * 67 + 2
    s.rect(ox + 580, yb, 176, 63, C["BLACK"], C["GREY"], 2)
    s.text(ox + 668, yb + 42, "RCS  ON", C["DARK_GREEN"], 24, "middle")
    s.rect(ox + 760, yb, 180, 63, C["BLACK"], C["GREY"], 2)
    s.text(ox + 850, yb + 42, "SAS TGT", C["DARK_GREEN"], 24, "middle")
    s.save(path)


def info_nav(path):
    s, ox = info_frame("NAVIGATION", 3, unit=2, chip="AUTO")
    compass_card(s, ox + 470, 370, 224, hdg=271, trk=279, brg=302, rover=False)
    s.rect(ox + 415, 69, 110, 52, C["BLACK"], C["WHITE"], 2)
    s.text(ox + 470, 109, "271°", C["DARK_GREEN"], 34, "middle")
    # ground-track line from own-ship out to the green marker
    a = math.radians(279 - 271 - 90)
    s.line(ox + 470 + 60 * math.cos(a), 370 + 60 * math.sin(a),
           ox + 470 + 128 * math.cos(a), 370 + 128 * math.sin(a),
           C["NEON_GREEN"], 2)
    for i, (lab, val, col) in enumerate((("TRK", "279°", "DARK_GREEN"),
                                         ("DRIFT", "+8°", "DARK_GREEN"),
                                         ("BRG", "302°", "VIOLET"))):
        y = TITLE_TOP + i * 120
        s.text(ox + 10, y + 26, lab, C["WHITE"], 22)
        s.text(ox + 180, y + 74, val, C[col], 40, "end")
    for i, (lab, val, col) in enumerate((("DIST", "12.4 km", "DARK_GREEN"),
                                         ("V.CLOSE", "-88 m/s", "DARK_GREEN"),
                                         ("T+INT", "2m 21s", "DARK_GREEN"))):
        y = TITLE_TOP + i * 120
        s.text(ox + 750, y + 26, lab, C["WHITE"], 22)
        s.text(ox + 930, y + 74, val, C[col], 36, "end")
    s.text(ox + 10, 550, "TRK  GND TRACK", C["NEON_GREEN"], 15)
    s.text(ox + 10, 580, "BRG  TO TARGET", C["VIOLET"], 15)
    s.save(path)


def info_descent(path):
    s, ox = info_frame("POWERED DESCENT", 4, unit=2, chip="AUTO")
    # altitude tape x=40 w=44
    vtape(s, ox + 40, 70, 44, 526,
          [(0.0, "0"), (0.25, "250"), (0.5, "500"), (0.75, "750"), (1.0, "1k")],
          marker=0.18, fill=0.18, tcol=C["DARK_GREEN"])
    s.text(ox + 14, 340, "ALTITUDE", C["WHITE"], 14, "middle", rot=-90)
    # X-pointer 332 square at (136, 88)
    s.rect(ox + 136, 88, 332, 332, C["DARK_GREY"], C["GREY"], 1)
    for i in range(1, 4):
        s.line(ox + 136 + i * 83, 88, ox + 136 + i * 83, 420, C["LIGHT_GREY"], 1)
        s.line(ox + 136, 88 + i * 83, ox + 468, 88 + i * 83, C["LIGHT_GREY"], 1)
    s.rect(ox + 260, 212, 84, 84, None, C["GREY"], 1)
    s.line(ox + 302, 88, ox + 302, 420, C["WHITE"], 1)
    s.line(ox + 136, 254, ox + 468, 254, C["WHITE"], 1)
    s.circle(ox + 334, 226, 10, None, C["NEON_GREEN"], 3)
    s.line(ox + 302, 226, ox + 324, 226, C["NEON_GREEN"], 2)
    s.line(ox + 334, 254, ox + 334, 236, C["NEON_GREEN"], 2)
    s.text(ox + 302, 82, "SURF DRIFT", C["WHITE"], 16, "middle")
    s.text(ox + 302, 440, "FWD ↑   AFT ↓   LATERAL →", C["LIGHT_GREY"], 14, "middle")
    # ATT bullseye and ground-track rose
    s.circle(ox + 216, 534, 52, C["BLACK"], C["SILVER"], 2)
    s.circle(ox + 216, 534, 13, None, C["DARK_GREEN"], 1, dash="3 4")
    s.circle(ox + 224, 526, 8, None, C["NEON_GREEN"], 3)
    s.text(ox + 216, 596, "ATT", C["WHITE"], 16, "middle")
    s.circle(ox + 388, 534, 52, C["BLACK"], C["SILVER"], 2)
    for d, lab in ((0, "N"), (90, "E"), (180, "S"), (270, "W")):
        a = math.radians(d - 90)
        s.text(ox + 388 + 40 * math.cos(a), 534 + 40 * math.sin(a) + 5, lab,
               C["LIGHT_GREY"], 13, "middle")
    s.line(ox + 388, 534, ox + 418, 512, C["NEON_GREEN"], 3)
    s.text(ox + 388, 596, "GND TRK", C["WHITE"], 16, "middle")
    # V.Vrt bar x=530 w=44
    vtape(s, ox + 530, 70, 44, 527,
          [(0.0, "0"), (0.33, "-5"), (0.53, "-8"), (1.0, "-15")],
          marker=0.24, fill=0.24, tcol=C["DARK_GREEN"])
    s.text(ox + 498, 340, "V.Vrt", C["WHITE"], 14, "middle", rot=-90)
    s.line(ox + 578, TITLE_TOP, ox + 578, H, C["DARK_GREY"], 2)
    panel_rows(s, ox, [
        ("V.Vrt:", "-3.6 m/s", C["DARK_GREEN"]),
        ("T+Grnd:", "24 s", C["DARK_GREEN"]),
        ("Alt.Rdr:", "86 m", C["YELLOW"]),
        ("Stg.Brn:", "42 s", C["DARK_GREEN"]),
        (("Fwd:", "+1.4", C["DARK_GREEN"]), ("Lat:", "-0.6", C["DARK_GREEN"])),
        ("ΔV.Stg:", "820 m/s", C["DARK_GREEN"]),
        None, None])
    # rows 6 and 7 are split: Thrtl | RCS, then GEAR | SAS
    y6 = TITLE_TOP + 6 * 67 + 2
    s.row(ox + 580, y6, 180, 63, "Thrtl:", "62%", C["WHITE"], C["DARK_GREEN"], 22)
    s.rect(ox + 760, y6, 180, 63, C["DARK_GREEN"], C["GREY"], 2)
    s.text(ox + 850, y6 + 42, "RCS", C["WHITE"], 26, "middle")
    y7 = TITLE_TOP + 7 * 67 + 2
    s.rect(ox + 580, y7, 180, 63, C["DARK_GREEN"], C["GREY"], 2)
    s.text(ox + 670, y7 + 42, "GEAR", C["WHITE"], 26, "middle")
    s.rect(ox + 760, y7, 180, 63, C["DARK_GREEN"], C["GREY"], 2)
    s.text(ox + 850, y7 + 42, "RETR", C["WHITE"], 26, "middle")
    s.save(path)


def info_reentry(path):
    s, ox = info_frame("RE-ENTRY", 4, unit=2, chip="AUTO")
    # altitude tape 44/35 and ATMO bar 130/35
    vtape(s, ox + 44, 70, 35, 527,
          [(0.0, "0"), (0.35, "25k"), (0.7, "50k"), (1.0, "70k")],
          marker=0.46, fill=None)
    s.text(ox + 26, 340, "ALTITUDE", C["WHITE"], 14, "middle", rot=-90)
    s.parts.append('<linearGradient id="atm2" x1="0" y1="0" x2="0" y2="1">'
                   '<stop offset="0" stop-color="%s"/>'
                   '<stop offset="1" stop-color="%s"/></linearGradient>'
                   % (C["NAVY"], C["SKY"]))
    s.rect(ox + 130, 78, 35, 511, "url(#atm2)", C["GREY"], 1)
    s.poly([(ox + 128, 300), (ox + 116, 293), (ox + 116, 307)], C["WHITE"])
    s.text(ox + 114, 340, "ATMOSPHERE", C["WHITE"], 14, "middle", rot=-90)
    # G meter
    s.rect(ox + 218, 78, 34, 511, C["OFF_BLACK"], C["GREY"], 1)
    s.rect(ox + 219, 320, 32, 268, C["DARK_GREEN"])
    for frac, lab in ((0.0, "0"), (0.25, "3"), (0.5, "5"), (0.75, "7"), (1.0, "9")):
        y = 589 - 511 * frac
        s.line(ox + 212, y, ox + 218, y, C["GREY"], 1)
        s.text(ox + 209, y + 5, lab, C["GREY"], 12, "end")
    s.text(ox + 182, 340, "G METER", C["WHITE"], 14, "middle", rot=-90)
    # retrograde alignment ball
    s.circle(ox + 428, 212, 122, C["BLACK"], C["SILVER"], 2)
    s.circle(ox + 428, 212, 40, None, C["DARK_GREEN"], 1, dash="4 5")
    s.line(ox + 396, 212, ox + 460, 212, C["WHITE"], 2)
    s.line(ox + 428, 180, ox + 428, 244, C["WHITE"], 2)
    s.circle(ox + 452, 192, 11, None, C["NEON_GREEN"], 3)
    s.text(ox + 428, 356, "RETRO ALIGNMENT", C["WHITE"], 16, "middle")
    # chute deploy envelope tape
    s.text(ox + 297, 412, "CHUTE DEPLOY", C["WHITE"], 15)
    s.rect(ox + 297, 436, 262, 54, C["OFF_BLACK"], C["GREY"], 1)
    s.rect(ox + 298, 437, 96, 52, C["DARK_GREEN"])
    s.rect(ox + 394, 437, 92, 52, C["YELLOW"])
    s.rect(ox + 486, 437, 72, 52, C["RED"])
    s.poly([(ox + 430, 432), (ox + 422, 420), (ox + 438, 420)], C["WHITE"])
    s.text(ox + 297, 508, "AIRSPEED (m/s)", C["GREY"], 13)
    s.text(ox + 282, 544, "SKIN", C["WHITE"], 15, "end")
    s.rect(ox + 288, 528, 270, 18, C["OFF_BLACK"], C["GREY"], 1)
    s.rect(ox + 289, 529, 216, 16, C["YELLOW"])
    s.text(ox + 282, 574, "CORE", C["WHITE"], 15, "end")
    s.rect(ox + 288, 558, 270, 18, C["OFF_BLACK"], C["GREY"], 1)
    s.rect(ox + 289, 559, 108, 16, C["DARK_GREEN"])
    s.line(ox + 578, TITLE_TOP, ox + 578, H, C["DARK_GREY"], 2)
    panel_rows(s, ox, [
        ("T+Grnd:", "3m 12s", C["DARK_GREEN"]),
        ("Alt.SL:", "38.4 km", C["DARK_GREEN"]),
        ("V.Srf:", "1 640 m/s", C["DARK_GREEN"]),
        ("V.Vrt:", "-284 m/s", C["DARK_GREEN"]),
        ("PeA:", "-142 km", C["DARK_GREEN"]),
        ("Mach:", "5.42", C["DARK_GREEN"]),
        (("Drogue:", "STOWED", C["DARK_GREEN"]), ("Main:", "STOWED", C["DARK_GREEN"])),
        None])
    yb = TITLE_TOP + 7 * 67 + 2
    s.rect(ox + 580, yb, 180, 63, C["OFF_BLACK"], C["GREY"], 2)
    s.text(ox + 670, yb + 42, "GEAR", C["DARK_GREY"], 26, "middle")
    s.rect(ox + 760, yb, 180, 63, C["DARK_GREEN"], C["GREY"], 2)
    s.text(ox + 850, yb + 42, "RETR", C["WHITE"], 26, "middle")
    s.save(path)


# ─────────────────────────────────────────────────────────────────────────────
# Colour-vocabulary swatch card
# ─────────────────────────────────────────────────────────────────────────────
def colour_key(path):
    s = Svg("KCMk1 display colour vocabulary", 1024, 400)
    entries = [
        ("Nominal", "DARK_GREEN", "WHITE",
         "value is inside its normal band — nothing to do"),
        ("Caution", "YELLOW", "DARK_GREY",
         "approaching a limit — plan an action"),
        ("Alarm", "RED", "WHITE",
         "limit exceeded — act now (white on red)"),
        ("Inactive", "DARK_GREY", "LIGHT_GREY",
         "not applicable, or no data (shown as ---)"),
        ("Pilot entry", "SKY", "BLACK",
         "cyan: entered by you, not yet confirmed by the system"),
        ("Chrome", "GREY", "BLACK",
         "keys, borders, scales — never carries alerting colour"),
    ]
    s.text(24, 46, "COLOUR VOCABULARY — the same on every panel", C["WHITE"], 30)
    for i, (name, col, fg, desc) in enumerate(entries):
        y = 74 + i * 52
        s.rect(24, y, 200, 42, C[col], C["GREY"], 1)
        s.text(124, y + 29, name, C[fg], 24, "middle")
        s.text(244, y + 29, desc, C["LIGHT_GREY"], 21, weight="normal")
    s.save(path)


# ─────────────────────────────────────────────────────────────────────────────
def main():
    out = sys.argv[1] if len(sys.argv) > 1 else \
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "images")
    out = os.path.abspath(out)
    if not os.path.isdir(out):
        os.makedirs(out)
    jobs = [
        ("colour-vocabulary.svg", colour_key),
        ("ann-main.svg", ann_main),
        ("ann-soi.svg", ann_soi),
        ("standby-splash.svg", ann_standby),
        ("res-main.svg", res_main),
        ("res-main-eva.svg", res_main_eva),
        ("res-select.svg", res_select),
        ("res-detail.svg", res_detail),
        ("info-chrome.svg", info_chrome),
        ("info-prelaunch.svg", info_prelaunch),
        ("info-ascent.svg", info_ascent),
        ("info-circ.svg", info_circ),
        ("info-ascentap.svg", info_ascentap),
        ("info-orbit.svg", info_orbit),
        ("info-orbadv.svg", info_orbadv),
        ("info-spacecraft.svg", info_spacecraft),
        ("info-aircraft.svg", info_aircraft),
        ("info-rover.svg", info_rover),
        ("info-vehicle.svg", info_vehicle),
        ("info-maneuver.svg", info_maneuver),
        ("info-target.svg", info_target),
        ("info-docking.svg", info_docking),
        ("info-nav.svg", info_nav),
        ("info-descent.svg", info_descent),
        ("info-reentry.svg", info_reentry),
    ]
    for name, fn in jobs:
        fn(os.path.join(out, name))
        print("wrote", name)


if __name__ == "__main__":
    main()
