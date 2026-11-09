#!/usr/bin/env python3
"""
gen_malfunction_diagrams.py -- malfunction decision diagrams for the TIM, in the style
of the Shuttle Flight Data File malfunction procedures.

Each figure is one portrait drawing, 650 units wide at 100 units per inch (6.5 in at
print, at most 6 in tall): the symptom in a title box, numbered decision boxes down
the left with YES to an action box on the right and NO down to the next step, and a
final action at the bottom. Controls are named as engraved, "A2 ENGINE – ARM".

  KCMk1_Malf_Throttle   THROTTLE DOES NOT RESPOND
  KCMk1_Malf_Sticks     STICKS DO NOTHING
  KCMk1_Malf_Audio      NO AUDIO / NO GPWS CALLOUTS
  KCMk1_Malf_KSPLink    FEWER THAN 5 KSP LINKS / SIMPIT LOST
  KCMk1_Malf_PanelDark  PANEL BUTTONS DARK

Every branch was checked against the firmware: Software/KCMk1_Master (ksp_frame.cpp,
interlocks.cpp, panel_throttle.cpp, panel_sticks.cpp, panels.cpp, module_bus.cpp,
panel_stability.cpp, panel_vehicle.cpp, panel_aux.cpp, panel_time.cpp, hold.cpp,
simpit_link.cpp, switches.cpp) and Software/Displays/KCMk1_Annunciator/GPWS.ino.

    python3 gen_malfunction_diagrams.py            # SVGs
    python3 gen_malfunction_diagrams.py --png      # SVGs + PNGs (Chromium, --scale 4)
"""
import argparse
import os

import gen_wiring_diagram as g
from gen_wiring_diagram import t, rect
from gen_block_diagrams import start, finish, line, render, INK, GREY, MUTED

W = 650
DX0, DX1 = 40, 330          # decision column
AX0, AX1 = 370, 640         # action column
PITCH = 72
DH = 48                     # decision box height
FS = 10.5                   # text size in boxes
GAP = 12.6


def hexbox(x0, y0, x1, y1):
    cy = (y0 + y1) / 2
    k = 14
    pts = [(x0, cy), (x0 + k, y0), (x1 - k, y0), (x1, cy), (x1 - k, y1), (x0 + k, y1)]
    g.out.append('<polygon points="%s" fill="#ffffff" stroke="%s" stroke-width="1.6"/>'
                 % (" ".join("%.1f,%.1f" % p for p in pts), INK))


def text_block(cx, cy, rows, size=FS, anchor="middle", x=None, **kw):
    n = len(rows)
    y = cy - (n - 1) * GAP / 2 + size * 0.36
    for i, r in enumerate(rows):
        t(cx if x is None else x, y + i * GAP, r, size=size, anchor=anchor, **kw)


def num(cx, cy, n):
    g.out.append('<circle cx="%.1f" cy="%.1f" r="10" fill="#ffffff" stroke="%s" stroke-width="1.4"/>' % (cx, cy, INK))
    t(cx, cy + 4, str(n), size=11, weight="bold", fill=INK)


def action(x0, y0, x1, y1, rows, end=True):
    rect(x0, y0, x1, y1, fill="#ffffff", stroke=INK, sw=1.6, r=0)
    text_block(0, (y0 + y1) / 2, rows, size=10.5, anchor="start", x=x0 + 9, fill=INK)
    if end:
        t(x1 - 6, y1 - 5, ">>", size=10.5, weight="bold", anchor="end", fill=INK)


def diagram(title, steps, final):
    """steps: list of (question rows, action rows). final: action rows."""
    n = len(steps)
    fh = 16 + len(final) * GAP
    h = int(66 + n * PITCH + fh + 46)
    start(W, h, "")
    g.out.pop()                                      # drop the empty title text
    # symptom title box
    rect(10, 8, W - 10, 46, fill="#ffffff", stroke=INK, sw=2.2, r=0)
    t(W / 2, 33, title, size=15, weight="bold", fill=INK)
    cx = (DX0 + DX1) / 2
    y = 66
    line([(cx, 46), (cx, y - 1)], INK, 1.6, head="k")
    for i, (q, a) in enumerate(steps):
        y0, y1 = y, y + DH
        cy = (y0 + y1) / 2
        hexbox(DX0, y0, DX1, y1)
        num(20, cy, i + 1)
        text_block(cx, cy, q, fill=INK)
        # YES -> action
        ah = 14 + len(a) * GAP
        line([(DX1, cy), (AX0 - 1, cy)], INK, 1.6, head="k")
        t((DX1 + AX0) / 2, cy - 5, "YES", size=10, weight="bold", fill=INK)
        action(AX0, cy - ah / 2, AX1, cy + ah / 2, a)
        # NO -> next
        ny = y + PITCH
        line([(cx, y1), (cx, ny - 1)], INK, 1.6, head="k")
        t(cx + 6, y1 + (PITCH - DH) / 2 + 4, "NO", size=10, weight="bold", anchor="start", fill=INK)
        y = ny
    action(DX0 - 20, y, AX1, y + fh, final)
    ly = y + fh + 20
    t(10, ly, "Number: decision · YES: do the action on the right · NO: go down to the next step.",
      size=10, anchor="start", fill=MUTED)
    t(10, ly + 14, ">>  End of procedure. If the symptom remains, return to that step and take its NO branch.",
      size=10, anchor="start", fill=MUTED)
    return finish(), h


FLIGHT_Q = ["Out of a flight scene, or Annunciator", "SIMPIT LOST or DEMO lit?"]
FLIGHT_A = ["Enter a flight scene.",
            "SIMPIT LOST lit: reconnect (Section 8.8).",
            "DEMO lit: A2 SCE – NORM (in demo mode",
            "nothing is sent to KSP)."]

DIAGRAMS = {
    "Throttle": ("THROTTLE DOES NOT RESPOND", [
        (["Annunciator ENG ARM dark", "(A2 ENGINE on SAFE)?"],
         ["A2 ENGINE – ARM.", "The lever is driven to 0 %; throttle input", "is taken once it is there."]),
        (["Annunciator THRTL PREC lit", "(A2 THRTL on FINE)?"],
         ["In FINE the throttle buttons are ignored and", "the lever covers ±10 % around the setting",
          "at selection. For the buttons or full travel:", "A2 THRTL – STD."]),
        (["Annunciator AUTOPILOT lit, with an", "autopilot using the throttle?"],
         ["Lever and buttons are ignored. Disconnect:", "RHC stick button (SPC, PLN), A/P OFF on",
          "the Information Display 2 console, or", "A2 A/P – INHIB."]),
        (FLIGHT_Q, FLIGHT_A),
    ], ["Open the input test screen on Information Display 1 (Section 10.3). THROTTLE row ACTIVE, and",
        "the lever bar and button boxes follow the controls? Row MISSING: turn the controller off and",
        "reseat the Throttle module's 16-pin cable."]),

    "Sticks": ("STICKS DO NOTHING", [
        (["Annunciator AUTOPILOT lit", "(autopilot or HOLD engaged)?"],
         ["Sticks are ignored while engaged.", "SPC, PLN: press the RHC stick button",
          "(A/P disconnect). RVR: press B2 HOLD", "again, or A/P OFF on the console."]),
        (["Kerbal on EVA?"],
         ["Normal: on EVA the RHC and THC hold the", "movement keys (W A S D, Q E, Shift, Ctrl)",
          "to move the Kerbal (Figure 3‑12)."]),
        (["Only translation (THC) dead?"],
         ["B2 RCS – ENAB (KSP translates only with", "RCS on). Release the THC stick button:",
          "while it is held the THC moves the camera."]),
        (["Rotation axes wrong, or the RHC", "steers the wheels?"],
         ["Set the controller mode selector to the", "vessel type (SPC, PLN, RVR). The Annunciator",
          "mode tile is green when they match."]),
        (["Response weak", "(Annunciator INPUT PREC lit)?"],
         ["B2 THC/RHC – NORM.", "PREC scales both sticks to 30 %."]),
        (FLIGHT_Q, FLIGHT_A),
    ], ["Open the input test screen on Information Display 1 (Section 10.3). Do the TRANSLATION and",
        "ROTATION dots and X: Y: Z: values follow the sticks? Row MISSING: turn the controller off and",
        "reseat that joystick module's 16-pin cable."]),

    "Audio": ("NO AUDIO / NO GPWS CALLOUTS", [
        (["Annunciator AUDIO dark", "(B2 AUDIO on MUTE)?"],
         ["B2 AUDIO – LIVE.", "MUTE silences every output, the master", "alarm included."]),
        (["Master alarm and the other", "audio cues silent too?"],
         ["Turn the controller off and check the", "SPEAKER 1 lead (alarm tones, PAM8302A",
          "amplifier) on the Annunciator driver."]),
        (["GPWS display blank, A2 ENA", "backlit only (GPWS OFF)?"],
         ["Press A2 ENA: lit green = full GPWS,", "lit amber = callouts only. PROX and RNDZ",
          "also select callouts only."]),
        (["Out of a flight scene?"],
         ["GPWS sounds only in a flight scene.", "Enter a flight."]),
        (["DFPlayer card missing, or without", "folder /01 (clips 001–034)?"],
         ["Load the Annunciator DFPlayer Mini card", "from Software/Displays/SD_Cards",
          "(Section 11.4)."]),
    ], ["Turn the controller off and check the SPEAKER 2 lead (GPWS voice, DFPlayer) on the Annunciator",
        "driver. Callouts sound only when their condition occurs; callouts only (amber) gives no warnings",
        "(Section 6)."]),

    "KSPLink": ("FEWER THAN 5 KSP LINKS / SIMPIT LOST", [
        (["KSP not running, or no game", "loaded?"],
         ["Start KSP and load the game.", "The controller retries the link every", "few seconds."]),
        (["Annunciator DEMO lit", "(A2 SCE on AUX)?"],
         ["A2 SCE – NORM.", "In demo mode the master does not", "contact KSP."]),
        (["Kerbal Simpit missing, or Settings.cfg", "without all five Simpit ports",
          "at 115200 Bd?"],
         ["Install Kerbal Simpit. List each Simpit port", "(the second COM port of each Teensy) in",
          "its own SerialPort block at 115200 Bd", "(Section 7.4)."]),
        (["Controller re-plugged, or COM port", "numbers changed?"],
         ["Identify the ports again (Section 7.3)", "and correct Settings.cfg."]),
        (["A display NO ANSWER on the", "Systems Information Display?"],
         ["That display does not answer the master.", "Check its cable; power cycle."]),
    ], ["Reconnect (Section 8.8): wait up to 10 s for the automatic retry; restart the Kerbal Simpit",
        "connection in KSP; if the link still does not recover, turn the controller off and on."]),

    "PanelDark": ("PANEL BUTTONS DARK", [
        (["All panel buttons dark?"],
         ["Modules are enabled only in a flight scene", "with the KSP link up, or in demo mode.",
          "Enter a flight. SIMPIT LOST lit: Section 8.8."]),
        (["Only the SAS mode buttons dark?"],
         ["B2 SAS – ENAB. With SAS on, a dark mode", "is not available: check the target, the",
          "node or the pilot level."]),
        (["MAIN and DROGUE dark?"], ["B2 CHUTE – ARM."]),
        (["EVA buttons (AUX) dark?"], ["Normal: they work only while a Kerbal", "is on EVA."]),
        (["A WARP TO button dark?"], ["Normal: its event does not exist", "(Table 3‑16)."]),
        (["A module MISSING on the Systems", "Information Display?"],
         ["Turn the controller off, reseat that", "module's 16-pin cable, turn it on."]),
    ], ["Check the module on the input test screen (Section 10.3). FAULT: A2 MSTR – RESET. Hold",
        "A2 LTG – TEST: every button LED and 7-segment display lights; one still dark has a fault."]),
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png", action="store_true")
    ap.add_argument("--scale", type=float, default=4.0)
    ap.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    a = ap.parse_args()
    jobs = []
    for key, (title, steps, final) in DIAGRAMS.items():
        svg, h = diagram(title, steps, final)
        sp = os.path.join(a.out, "KCMk1_Malf_%s.svg" % key)
        open(sp, "w").write(svg)
        print("wrote %s  (%.2f x %.2f in)" % (sp, W / 100, h / 100))
        jobs.append((sp, sp[:-4] + ".png", W, h))
    if a.png:
        render(jobs, a.scale)


if __name__ == "__main__":
    main()
