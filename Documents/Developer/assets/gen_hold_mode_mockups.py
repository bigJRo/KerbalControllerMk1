#!/usr/bin/env python3
"""Generate the Hold-Mode Autopilot console mockups (1024x600) in the InfoDisp panel style.

Geometry mirrors Screen_LNCH_AscentAP.ino so the mockups are a faithful preview of what the
existing chrome/draw helpers would produce: TITLE_TOP 62, banner 64 px, column x = 6/322/638,
column width 298, row pitch 58 from y=172, value boxes 176x50, sidebar 84 px with six keys.
"""
import os, sys

# RGB565 palette from KerbalDisplayCommon.h, converted to CSS hex
BLACK      = "#000000"
WHITE      = "#FFFFFF"
GREY       = "#848284"   # TFT_GREY / KDC_LABEL_COLOR
DARK_GREY  = "#3A3D3A"   # TFT_DARK_GREY
DARK_GREEN = "#007D00"   # TFT_DARK_GREEN  (confirmed values, engaged buttons)
NEON_GREEN = "#3AFF10"   # TFT_NEON_GREEN  (ARMED / ENGAGED banner)
SKY        = "#73C2FF"   # TFT_SKY         (pending / unconfirmed)
CYAN       = "#00FFFF"   # TFT_CYAN
ORANGE     = "#FF7D00"   # TFT_ORANGE      (guard / disconnect reason)
YELLOW     = "#FFBA10"   # TFT_YELLOW
RED        = "#FF0000"   # TFT_RED

FONT = "font-family=\"Roboto, 'Arial Black', Arial, sans-serif\" font-weight=\"900\""

W, H = 1024, 600
TITLE_TOP = 62
SIDEBAR_X = 940
CONTENT_W = 940
BANNER_Y, BANNER_H = TITLE_TOP, 64
COL_Y = BANNER_Y + BANNER_H + 6          # 132
ROW_Y0 = COL_Y + 40                       # 172
ROW_H = 58
COL_BOT = 590
C1X, C2X, C3X = 6, 322, 638
COLW = 298
DIV1_X, DIV2_X = 314, 630
VALW, VALH = 176, ROW_H - 8
BTN_W = 112                               # mode-button width inside a row
ARM_H = 136
ARM_Y = H - 4 - ARM_H                     # 460

def row_y(i): return ROW_Y0 + i * ROW_H
def val_x(cx): return cx + COLW - VALW

out = []
def add(s): out.append(s)

def text(x, y, s, size, fill, anchor="start", extra=""):
    add(f'<text x="{x}" y="{y}" font-size="{size}" fill="{fill}" text-anchor="{anchor}" '
        f'dominant-baseline="middle" {FONT} {extra}>{s}</text>')

def rect(x, y, w, h, fill="none", stroke="none", sw=1):
    add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>')

def frame(title, sidebar_labels, active_key, active_key_green=False):
    add(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">')
    rect(0, 0, W, H, BLACK)
    # title bar + rule
    text(CONTENT_W // 2, 30, title, 36, WHITE, "middle")
    rect(0, 58, CONTENT_W, 4, GREY)
    # sidebar (unit 2: outboard right edge)
    add(f'<line x1="{SIDEBAR_X}" y1="0" x2="{SIDEBAR_X}" y2="{H}" stroke="{GREY}" stroke-width="1"/>')
    bh = H // 6
    for i, lbl in enumerate(sidebar_labels):
        y = i * bh
        if i == active_key:
            fill = DARK_GREEN if active_key_green else GREY
            rect(SIDEBAR_X + 1, y + 1, 82, bh - 2, fill)
            text(SIDEBAR_X + 42, y + bh // 2, lbl, 20, WHITE if active_key_green else BLACK, "middle")
        else:
            rect(SIDEBAR_X + 1, y + 1, 82, bh - 2, BLACK, GREY)
            text(SIDEBAR_X + 42, y + bh // 2, lbl, 20, WHITE, "middle")

def banner(left, left_color, right_top, right_bottom, right_color, reason=None):
    text(8, BANNER_Y + BANNER_H // 2, left, 36, left_color)
    if reason:
        text(330, BANNER_Y + BANNER_H // 2, reason, 24, ORANGE)
    text(CONTENT_W - 6, BANNER_Y + 16, right_top, 20, GREY, "end")
    text(CONTENT_W - 6, BANNER_Y + 45, right_bottom, 28, right_color, "end")
    rect(0, BANNER_Y + BANNER_H, CONTENT_W, 2, GREY)

def columns(h1, h2, h3):
    add(f'<line x1="{DIV1_X}" y1="{COL_Y}" x2="{DIV1_X}" y2="{COL_BOT}" stroke="{GREY}"/>')
    add(f'<line x1="{DIV2_X}" y1="{COL_Y}" x2="{DIV2_X}" y2="{COL_BOT}" stroke="{GREY}"/>')
    text(C1X, COL_Y + 16, h1, 28, WHITE)
    text(C2X, COL_Y + 16, h2, 28, WHITE)
    text(C3X, COL_Y + 16, h3, 28, WHITE)

# state: "off" | "pending" | "on"
def mode_row(cx, row, name, state, value, value_state="confirmed", no_box=False):
    y = row_y(row)
    bx, by, bw, bh = cx + 2, y + 4, BTN_W, VALH
    if state == "on":
        rect(bx, by, bw, bh, DARK_GREEN); tc = WHITE
    elif state == "pending":
        rect(bx, by, bw, bh, BLACK, CYAN, 3); tc = CYAN
    else:
        rect(bx, by, bw, bh, BLACK, GREY); tc = WHITE
    text(bx + bw // 2, by + bh // 2, name, 24, tc, "middle")
    if not no_box:
        vx = val_x(cx)
        rect(vx, y + 4, VALW - 2, VALH, BLACK, GREY)
        vc = {"confirmed": DARK_GREEN, "pending": SKY, "muted": DARK_GREY}[value_state]
        text(vx + 8, y + 4 + VALH // 2, value, 32, vc)

def edit_row(cx, row, label, value, pending=False):
    y = row_y(row)
    text(cx + 4, y + ROW_H // 2, label, 24, GREY)
    vx = val_x(cx)
    rect(vx, y + 4, VALW - 2, VALH, BLACK, GREY)
    text(vx + 8, y + 4 + VALH // 2, value, 32, SKY if pending else DARK_GREEN)

def out_row(cx, row, label, value, color=DARK_GREEN):
    y = row_y(row)
    text(cx + 4, y + ROW_H // 2, label, 24, GREY)
    vx = val_x(cx)
    text(vx + 8, y + 4 + VALH // 2, value, 32, color)

def big_button(x, y, w, h, label, hint, fill=None, stroke=GREY, tc=WHITE):
    rect(x, y, w, h, fill or BLACK, stroke, 3)
    text(x + w // 2, y + h // 2 - 12, label, 28, tc, "middle")
    if hint:
        text(x + w // 2, y + h // 2 + 26, hint, 16, GREY if fill is None else WHITE, "middle")

def hint(cx, row, s):
    text(cx + 4, row_y(row) + 18, s, 16, DARK_GREY)

def legend(y):
    # small state legend along the bottom of column 1
    items = [("OFF", BLACK, GREY, WHITE), ("PENDING", BLACK, CYAN, CYAN), ("ENGAGED", DARK_GREEN, DARK_GREEN, WHITE)]
    x = C1X + 2
    for lbl, f, s, t in items:
        rect(x, y, 92, 28, f, s, 2 if s == CYAN else 1)
        text(x + 46, y + 14, lbl, 14, t, "middle")
        x += 100

def finish(path):
    add('</svg>')
    with open(path, "w") as fh:
        fh.write("\n".join(out))
    out.clear()

# ─────────────────────────────────────────────────────────────────────────────
# AIRCRAFT AUTOPILOT console
# Depicts: ALT hold engaged (setpoint confirmed), HDG engaged, IAS engaged with the
# pilot having just dialled a new speed (pending, cyan), ROLL/ATT/AOA/V/S/MACH off.
# ─────────────────────────────────────────────────────────────────────────────
frame("AIRCRAFT AUTOPILOT", ["PFD", "LNCH", "ORB", "TGT", "LNDG", "ACAP"], 5, active_key_green=True)
banner("ALT  HDG  IAS", DARK_GREEN, "KERBIN  ·  SPACEPLANE MK2", "ENGAGED", NEON_GREEN)
columns("PITCH", "LATERAL / THRUST", "FLIGHT DATA")
mode_row(C1X, 0, "ATT",  "off", "5.0°",   "muted")
mode_row(C1X, 1, "AOA",  "off", "3.0°",   "muted")
mode_row(C1X, 2, "V/S",  "off", "+0 m/s",      "muted")
mode_row(C1X, 3, "ALT",  "on",  "6,000 m")
hint(C1X, 4, "Tap a mode to engage and capture")
hint(C1X, 4.35, "the current value. Tap a box to edit.")
legend(row_y(5) + 12)
mode_row(C2X, 0, "ROLL", "off", "0.0°", "muted")
mode_row(C2X, 1, "HDG",  "on",  "090°")
mode_row(C2X, 2, "IAS",  "on",  "180 m/s", "pending")
mode_row(C2X, 3, "MACH", "off", "0.85", "muted")
big_button(C2X, ARM_Y, 145, ARM_H, "LVL", "wings level, V/S 0")
big_button(C2X + 153, ARM_Y, 145, ARM_H, "A/P OFF", "disconnect all")
out_row(C3X, 0, "PITCH", "+2.4°")
out_row(C3X, 1, "ROLL",  "+11.7°")
out_row(C3X, 2, "HDG",   "087°")
out_row(C3X, 3, "V/S",   "+0.8 m/s")
out_row(C3X, 4, "ALT",   "5,988 m")
out_row(C3X, 5, "IAS",   "171 m/s")
out_row(C3X, 6, "MACH",  "0.52")
finish("/home/user/KerbalControllerMk1/Documents/Developer/assets/Hold_Mode_ACFT_Console.svg")

# ─────────────────────────────────────────────────────────────────────────────
# AIRCRAFT console — disconnect state (autothrottle dropped because the pilot
# grabbed the throttle lever; pitch/lateral modes still engaged)
# ─────────────────────────────────────────────────────────────────────────────
frame("AIRCRAFT AUTOPILOT", ["PFD", "LNCH", "ORB", "TGT", "LNDG", "ACAP"], 5, active_key_green=True)
banner("ALT  HDG", DARK_GREEN, "KERBIN  ·  SPACEPLANE MK2", "ENGAGED", NEON_GREEN, reason="A/T OFF: LEVER")
columns("PITCH", "LATERAL / THRUST", "FLIGHT DATA")
mode_row(C1X, 0, "ATT",  "off", "5.0°",   "muted")
mode_row(C1X, 1, "AOA",  "off", "3.0°",   "muted")
mode_row(C1X, 2, "V/S",  "off", "+0 m/s",      "muted")
mode_row(C1X, 3, "ALT",  "on",  "6,000 m")
hint(C1X, 4, "Tap a mode to engage and capture")
hint(C1X, 4.35, "the current value. Tap a box to edit.")
legend(row_y(5) + 12)
mode_row(C2X, 0, "ROLL", "off", "0.0°", "muted")
mode_row(C2X, 1, "HDG",  "on",  "090°")
mode_row(C2X, 2, "IAS",  "off", "180 m/s", "muted")
mode_row(C2X, 3, "MACH", "off", "0.85", "muted")
big_button(C2X, ARM_Y, 145, ARM_H, "LVL", "wings level, V/S 0")
big_button(C2X + 153, ARM_Y, 145, ARM_H, "A/P OFF", "disconnect all")
out_row(C3X, 0, "PITCH", "+2.1°")
out_row(C3X, 1, "ROLL",  "+0.3°")
out_row(C3X, 2, "HDG",   "090°")
out_row(C3X, 3, "V/S",   "+0.1 m/s")
out_row(C3X, 4, "ALT",   "6,002 m")
out_row(C3X, 5, "IAS",   "176 m/s")
out_row(C3X, 6, "MACH",  "0.53")
finish("/home/user/KerbalControllerMk1/Documents/Developer/assets/Hold_Mode_ACFT_Console_Disconnect.svg")

# ─────────────────────────────────────────────────────────────────────────────
# ROVER AUTOPILOT console
# Depicts: CRUISE engaged at 12 m/s, HDG pending (tap sent, not yet echoed), TGT off,
# guard limits editable, slope guard currently reducing speed (orange readout).
# ─────────────────────────────────────────────────────────────────────────────
frame("ROVER AUTOPILOT", ["PFD", "LNCH", "ORB", "TGT", "LNDG", "RVAP"], 5, active_key_green=True)
banner("CRUISE", DARK_GREEN, "MUN  ·  ROVER 3", "ENGAGED", NEON_GREEN, reason="SLOPE LIMIT")
columns("DRIVE", "GUARD LIMITS", "DRIVE DATA")
mode_row(C1X, 0, "CRUISE", "on",      "12.0 m/s")
mode_row(C1X, 1, "HDG",    "pending", "045°", "pending")
mode_row(C1X, 2, "TGT",    "off",     "BRG 112°", "muted")
hint(C1X, 3, "TGT steers to the target bearing;")
hint(C1X, 3.35, "HDG and TGT are exclusive.")
legend(row_y(5) + 12)
edit_row(C2X, 0, "SPEED", "20.0 m/s")
edit_row(C2X, 1, "SLOPE", "20°")
edit_row(C2X, 2, "ROLL",  "25°")
hint(C2X, 3, "Slope guard scales the cruise")
hint(C2X, 3.35, "setpoint; roll guard brakes.")
big_button(C2X, ARM_Y, COLW, ARM_H, "A/P OFF", "disconnect all, brakes stay as set")
out_row(C3X, 0, "SPEED",   "+9.6 m/s", ORANGE)
out_row(C3X, 1, "HDG",     "038°")
out_row(C3X, 2, "TGT BRG", "112°")
out_row(C3X, 3, "PITCH",   "-14.2°", ORANGE)
out_row(C3X, 4, "ROLL",    "+3.1°")
out_row(C3X, 5, "WHL THR", "-18 %")
out_row(C3X, 6, "BRAKES",  "OFF", GREY)
finish("/home/user/KerbalControllerMk1/Documents/Developer/assets/Hold_Mode_ROVR_Console.svg")
print("ok")
