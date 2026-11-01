#!/usr/bin/env python3
"""gen_sysinfo_mockups.py -- design mockups of the Sys Info Display for
Sys_Info_Display_Design.md (ER-TFTM020-2: 240x320 portrait, 0.1275 mm pixels), drawn with
the panels' own ILI9341_t3 fonts so the text is pixel-exact to what the firmware will draw.

Every string goes through text(), which refuses a font whose capital H is shorter than
MIN_CAP_PX (22 px = 2.8 mm = 16 arcmin at 600 mm, design §1.1). Needs Pillow.

    python3 Documents/Developer/assets/gen_sysinfo_mockups.py      # -> assets/sysinfo/
"""
import os, sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
FONTS = os.path.join(HERE, "..", "..", "..", "Software", "Displays", "libraries",
                     "KerbalDisplayCommon", "src", "fonts_ili")
sys.path.insert(0, FONTS)
from ilifont import Font  # noqa: E402

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "sysinfo")
W, H = 240, 320
PITCH_MM = 0.1275            # ER-TFTM020-2 active area 30.60 x 40.80 mm / 240 x 320
MIN_CAP_PX = 22              # 2.8 mm
M = 2                        # side margin: room for the +/-2 px anti-sticking shift (design §2.4)

def c565(v):
    r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)

BLACK, OFFB, DGREY, GREY, LGREY, WHITE = map(c565, (0x0000, 0x2104, 0x39E7, 0x8410, 0xBDF7, 0xFFFF))
GREEN, RED, DRED = map(c565, (0x07E0, 0xF800, 0x6000))
YELLOW, ORANGE, SKY, NAVY, MAGENTA, BLUE = map(c565, (0xFDC2, 0xFBE0, 0x761F, 0x000F, 0xF81F, 0x001F))
DEMO_BG = BLUE              # the Annunciator's lit DEMO tile: white on TFT_BLUE (ScreenMain.ino modeGrid)

_fc, _cap = {}, {}
def font(name):
    if name not in _fc:
        f = Font(os.path.join(FONTS, name + ".c"))
        rows = [i for i, r in enumerate(f.glyph(ord("H"))["rows"]) if any(r)]
        _fc[name], _cap[name] = f, rows[-1] - rows[0] + 1
    return _fc[name]

R32, R48 = "Roboto_Black_32", "Roboto_Black_48"
PITCH = 34                    # Roboto Black 32 text rows, 34 px apart (caps 23 px)
TXT = R32

class Scr:
    def __init__(s):
        s.im = Image.new("RGB", (W, H), BLACK)
        s.px = s.im.load()
    def rect(s, x, y, w, h, c):
        for yy in range(max(0, y), min(H, y + h)):
            for xx in range(max(0, x), min(W, x + w)):
                s.px[xx, yy] = c
    def frame(s, x, y, w, h, c, t=2):
        s.rect(x, y, w, t, c); s.rect(x, y + h - t, w, t, c)
        s.rect(x, y, t, h, c); s.rect(x + w - t, y, t, h, c)
    def width(s, fn, text):
        f = font(fn)
        return sum(f.glyph(ord(ch))["delta"] for ch in text)
    def text(s, x, y, fn, text, c, align="l"):
        f = font(fn)
        assert _cap[fn] >= MIN_CAP_PX, "%s: caps %d px, below the %d px minimum" % (fn, _cap[fn], MIN_CAP_PX)
        if align == "r": x -= s.width(fn, text)
        if align == "c": x -= s.width(fn, text) // 2
        assert x >= M - 1 and x + s.width(fn, text) <= W - M + 1, "text outside the margins: %r" % text
        for ch in text:
            g = f.glyph(ord(ch))
            for r, row in enumerate(g["rows"]):
                for k, bit in enumerate(row):
                    if bit and 0 <= y + r < H:
                        s.px[x + k, y + r] = c
            x += g["delta"]
        return x
    def icon(s, x, y, rows, c, scale=2):
        for r, row in enumerate(rows):
            for k, ch in enumerate(row):
                if ch == "#":
                    s.rect(x + k * scale, y + r * scale, scale, scale, c)
    def save(s, name):
        s.im.save(os.path.join(OUT, name + ".png"))
        s.im.resize((W * 3, H * 3), Image.NEAREST).save(os.path.join(OUT, name + "_x3.png"))

# 16x16 icons ('#' = lit), drawn at 2x (32 px). These become 1-bit arrays in the firmware.
ICON = {
 "bolt": [
  ".........#####..", "........#####...", ".......#####....", "......#####.....",
  ".....#####......", "....#####.......", "...###########..", "..###########...",
  ".......#####....", "......#####.....", ".....#####......", "....####........",
  "...####.........", "..###...........", "..##............", ".#.............."],
 "therm": [
  "......###.......", ".....##.##......", ".....#...#..##..", ".....#.#.#......",
  ".....#.#.#..##..", ".....#.#.#......", ".....#.#.#..##..", ".....#.#.#......",
  ".....#.#.#......", "....##.#.##.....", "...##.###.##....", "...#.#####.#....",
  "...#.#####.#....", "...##.###.##....", "....##...##.....", ".....#####......"],
 "fan": [
  "......####......", ".....######.....", ".....######.....", "......####......",
  "##....####....##", "###...####...###", "####..####..####", "######.##.######",
  "######.##.######", "####..####..####", "###...####...###", "##....####....##",
  "......####......", ".....######.....", ".....######.....", "......####......"],
 "link": [
  "................", "....#####.......", "...##...##......", "..##.....##.....",
  "..#.......#.....", "..#...#####.....", "..##.##...##....", "...###.....##...",
  "....##.....###..", "....##...##.##..", ".....#####...#..", ".....#.......#..",
  ".....##.....##..", "......##...##...", ".......#####....", "................"],
 "chip": [
  "...#..#..#..#...", "...#..#..#..#...", "..############..", "..#..........#..",
  "###..........###", "..#..######..#..", "..#..#....#..#..", "###..#....#..###",
  "..#..#....#..#..", "..#..######..#..", "###..........###", "..#..........#..",
  "..############..", "...#..#..#..#...", "...#..#..#..#...", "................"],
 "ok": [
  "................", "..............##", ".............###", "............###.",
  "...........###..", "..........###...", ".........###....", "##......###.....",
  "###....###......", ".###..###.......", "..######........", "...####.........",
  "....##..........", "................", "................", "................"],
 "warn": [
  ".......##.......", "......####......", "......####......", ".....##..##.....",
  ".....##..##.....", "....##.##.##....", "....##.##.##....", "...##..##..##...",
  "...##..##..##...", "..##...##...##..", "..##........##..", ".##.....##....##",
  ".##.....##....##", "##............##", "################", "################"],
 "err": [
  "................", ".##..........##.", ".###........###.", "..###......###..",
  "...###....###...", "....###..###....", ".....######.....", "......####......",
  "......####......", ".....######.....", "....###..###....", "...###....###...",
  "..###......###..", ".###........###.", ".##..........##.", "................"],
}

SEV = [LGREY, GREEN, YELLOW, RED]

def wrap(s, text, rows=3, width=W):
    """Word wrap by pixel width; runs of spaces collapse; a word wider than a row is cut."""
    out, cur = [], ""
    for word in text.split():
        cand = (cur + " " + word) if cur else word
        if s.width(TXT, cand) <= width:
            cur = cand
        else:
            if cur: out.append(cur)
            while s.width(TXT, word) > width: word = word[:-1]
            cur = word
    if cur: out.append(cur)
    return out[:rows]

def title_bar(s, left, right=None, color=NAVY):
    s.rect(0, 0, W, 44, color)
    s.text(6, 0, R32, left, WHITE)
    if right: s.text(W - 6, 0, R32, right, WHITE, "r")

# ---------------------------------------------------------------- boot

def boot(name, last, counts, problem, silent=None, elapsed="4 S"):
    s = Scr()
    title_bar(s, "BOOT", elapsed)
    sev, line = last
    for i, r in enumerate(wrap(s, line)):
        s.text(M, 42 + i * PITCH, TXT, r, SEV[sev])
    for k, (ic, c, n) in enumerate((("ok", GREEN, counts[0]), ("warn", YELLOW, counts[1]), ("err", RED, counts[2]))):
        x = M + k * 80
        s.icon(x, 168, ICON[ic], c if n else DGREY)
        s.text(x + 36, 160, R32, str(n), c if n else GREY)
    if silent:
        s.rect(0, 212, W, 108, RED)
        s.text(W // 2, 220, R32, "MASTER", WHITE, "c")
        s.text(W // 2, 264, R32, "SILENT %d S" % silent, WHITE, "c")
    elif problem:
        sev, line = problem
        for i, r in enumerate(wrap(s, line)):
            s.text(M, 206 + i * PITCH, TXT, r, SEV[sev])
    else:
        s.text(M, 206, TXT, "NO FAULTS", GREEN)
    s.save(name)

# ---------------------------------------------------------------- status

def status(name, v=12.08, a=1.84, w=22.3, t=38, rpm=2140, ksp=(5, 5), mods=(13, 13),
           msg=("ALL SYSTEMS", "NOMINAL", GREEN), demo=False, stale=None):
    s = Scr()
    g = stale is not None
    vc = GREY if g else GREEN if v >= 11.5 else YELLOW if v >= 11.0 else RED
    s.icon(M, 8, ICON["bolt"], vc)
    s.text(W - M, -4, R48, "%.2fV" % v, vc, "r")
    ac = GREY if g else RED if a > 8.0 else YELLOW if a > 7.0 else WHITE
    s.text(M, 46, R32, "%.2fA" % a, ac)
    s.text(W - M, 46, R32, "%.1fW" % w, ac, "r")
    tc = GREY if g else GREEN if t < 50 else YELLOW if t < 60 else RED
    s.icon(M, 98, ICON["therm"], tc)
    s.text(36, 92, R32, "%dC" % t, tc)
    fc = GREY if g else RED if rpm == 0 else WHITE
    s.icon(112, 98, ICON["fan"], fc)
    s.text(W - M, 92, R32, "%d" % rpm if rpm else "STOP", fc, "r")
    kc = GREY if g else GREEN if ksp[0] == ksp[1] else ORANGE
    if demo and not g:      # demo: the KSP row is drawn as the Annunciator's DEMO tile
        s.rect(0, 136, W, 40, DEMO_BG)
        kc = WHITE
    s.icon(M, 144, ICON["link"], kc)
    s.text(40, 138, R32, "KSP", kc)
    s.text(W - M, 138, R32, "%d/%d" % ksp, kc, "r")
    mc = GREY if g else GREEN if mods[0] == mods[1] else RED
    s.icon(M, 186, ICON["chip"], mc)
    s.text(40, 180, R32, "MOD", mc)
    s.text(W - M, 180, R32, "%d/%d" % mods, mc, "r")
    if g:
        s.rect(0, 230, W, 90, RED)
        s.text(W // 2, 236, R32, "MASTER", WHITE, "c")
        s.text(W // 2, 276, R32, "SILENT %d S" % stale, WHITE, "c")
    else:
        l1, l2, c = msg[:3]
        if len(msg) > 3:    # an entry on a block (DEMO MODE: white on the DEMO tile blue)
            s.rect(0, 226, W, 72, msg[3])
        s.text(M, 228, TXT, l1, c)
        s.text(M, 228 + PITCH, TXT, l2, c)
    s.save(name)

# ---------------------------------------------------------------- SWITCH ERR

def swerr(name, errs, page=None):
    s = Scr()
    title_bar(s, "SWITCH ERR", None, RED)
    s.text(M, 42, TXT, "VESSEL IS:", GREY)
    rows = errs[:5] if len(errs) > 5 else errs
    for i, (nm, vsl) in enumerate(rows):
        y = 82 + i * PITCH
        assert s.width(TXT, nm) + 4 + s.width(TXT, vsl) <= W - 2 * M, "row too wide: %s %s" % (nm, vsl)
        s.text(M, y, TXT, nm, WHITE)
        s.text(W - M, y, TXT, vsl, RED, "r")
    if page:
        s.text(W // 2, 282, TXT, "PAGE %d/%d" % page, GREY, "c")
    s.save(name)

os.makedirs(OUT, exist_ok=True)
for f in os.listdir(OUT):
    if f.endswith(".png"): os.remove(os.path.join(OUT, f))
boot("SysInfo_BOOT", (1, "TRANSLATION OK"), (9, 0, 0), None, elapsed="2 S")
boot("SysInfo_BOOT_WAIT", (0, "KSP WAIT 74 S"), (24, 3, 2),
     (3, "RES DISP NO ANSWER"), elapsed="81 S")
boot("SysInfo_BOOT_HUNG", (1, "ACTION OK"), (8, 0, 0), None, silent=12, elapsed="14 S")
status("SysInfo_STATUS")
status("SysInfo_STATUS_SERVICE", msg=("UPTIME", "01:23:45", LGREY))
status("SysInfo_STATUS_FAULTS", v=10.84, a=8.31, w=90.1, t=52, rpm=0, ksp=(3, 5), mods=(12, 13),
       msg=("CURRENT", "HIGH 8.31 A", RED))
status("SysInfo_STATUS_FAULTS_2", v=11.32, a=7.24, w=82.0, t=52, rpm=0, ksp=(3, 5), mods=(12, 13),
       msg=("RESOURCE", "NO ANSWER", RED))
status("SysInfo_STATUS_DEMO", ksp=(4, 5), msg=("DEMO MODE", "ON", WHITE, DEMO_BG), demo=True)
status("SysInfo_STATUS_DEMO_2", ksp=(4, 5), msg=("ALL SYSTEMS", "NOMINAL", GREEN), demo=True)
status("SysInfo_STATUS_STALE", stale=7)
swerr("SysInfo_SWITCH_ERR", [("GEAR", "UP"), ("SAS", "OFF"), ("BRAKE", "REL")])
swerr("SysInfo_SWITCH_ERR_PAGED", [("CHUTE", "SAFE"), ("GEAR", "UP"), ("BRAKE", "REL"), ("EXT LT", "OFF"),
      ("THC/RHC", "NORM"), ("SAS", "OFF"), ("RCS", "OFF")], page=(1, 2))
print("ok")
