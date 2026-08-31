#!/usr/bin/env python3
"""
Add U+00B7 MIDDLE DOT (0xB7) to the Roboto_Black ILI9341_t3 fonts.

Why: 0xB7 sat two codepoints past index2_last (181), so every Roboto_Black size
was missing it. A missing glyph measures zero and draws nothing, so a separator
written as "\\xB7" rendered as an unexplained gap rather than as an error.

How: the dot is not drawn by hand. It is the font's OWN period (0x2E) ink block,
lifted so its vertical centre sits at the midpoint of the cap band, which is
measured from the same font's capital H. So every size gets a middle dot with
that size's exact dot shape, weight and side bearings.

The index range is extended 181 -> 183; 0xB6 (pilcrow) falls inside the new range
and gets an explicit zero-size glyph, which renders as nothing and advances
nothing -- the same behaviour it had when it was out of range. bits_index is
recomputed, since appending to data[] can push the largest offset past the width
the file was generated with (Roboto_Black_12 needs this).

Every pre-existing glyph is decoded before and after and asserted identical.

Usage: add_middot.py FONTDIR
"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ilifont import Font

MIDDOT, PILCROW = 0xB7, 0xB6

class BitWriter:
    def __init__(self): self.bits = []
    def put(self, v, n):
        for i in range(n - 1, -1, -1): self.bits.append((v >> i) & 1)
    def pad(self):
        while len(self.bits) % 8: self.bits.append(0)
    def bytes(self):
        out = bytearray()
        for i in range(0, len(self.bits), 8):
            b = 0
            for k in range(8):
                b = (b << 1) | (self.bits[i + k] if i + k < len(self.bits) else 0)
            out.append(b)
        return out

def encode_glyph(f, w, h, xoff, yoff, delta, rows):
    bw = BitWriter()
    bw.put(0, 3)                                  # encoding
    bw.put(w, f["bits_width"]); bw.put(h, f["bits_height"])
    bw.put(xoff, f["bits_xoffset"]); bw.put(yoff, f["bits_yoffset"])
    bw.put(delta, f["bits_delta"])
    for r in rows:
        bw.put(0, 1)                              # per-row flag, as the generator emits
        for b in r: bw.put(b, 1)
    bw.pad()
    return bw.bytes()

def build_middot(F):
    """The period's ink, centred on the cap band."""
    per = F.glyph(ord('.'))
    cap = F.glyph(ord('H'))
    ink = [i for i, r in enumerate(per['rows']) if any(r)]
    capr = [i for i, r in enumerate(cap['rows']) if any(r)]
    shape = per['rows'][ink[0]:ink[-1] + 1]
    mid = (capr[0] + capr[-1]) / 2.0
    top = int(round(mid - (len(shape) - 1) / 2.0))
    rows = [[0] * per['w'] for _ in range(top)] + shape
    return per['w'], len(rows), per['delta'], rows

def emit_array(name, buf):
    lines = []
    for i in range(0, len(buf), 16):
        lines.append("  " + " ".join("0x%02X," % b for b in buf[i:i + 16]))
    return "static const unsigned char %s[] = {\n%s\n};" % (name, "\n".join(lines))

def patch(path):
    F = Font(path)
    before = {c: F.glyph(c) for c in F.codes()}
    f = dict(F.f)

    w, h, delta, rows = build_middot(F)
    data = bytearray(F.data)

    new_off = {}
    # 0xB6 first, so codes stay contiguous and in order.
    for code, blob in ((PILCROW, encode_glyph(f, 0, 0, 0, 0, 0, [])),
                       (MIDDOT,  encode_glyph(f, w, h, 0, 0, delta, rows))):
        new_off[code] = len(data)
        data += blob

    offs = [F.offset(c) for c in F.codes()] + [new_off[PILCROW], new_off[MIDDOT]]
    bits_index = max(f["bits_index"], max(offs).bit_length())
    bw = BitWriter()
    for o in offs: bw.put(o, bits_index)
    bw.pad()
    index = bw.bytes()

    def body(name, buf):
        """Just the `NAME[] = { ... };` part, so the `static const unsigned char`
        prefix already in the file is kept exactly as it was."""
        return emit_array(name, buf)[len("static const unsigned char "):]

    src = F.src
    src = re.sub(re.escape(F.name + "_data")  + r'\[\]\s*=\s*\{.*?\n\};',
                 lambda m: body(F.name + "_data",  data),  src, count=1, flags=re.S)
    src = re.sub(re.escape(F.name + "_index") + r'\[\]\s*=\s*\{.*?\n\};',
                 lambda m: body(F.name + "_index", index), src, count=1, flags=re.S)
    src = re.sub(r'\d+,(\s*//\s*index2_last)', lambda m: "%d,%s" % (MIDDOT, m.group(1)),
                 src, count=1)
    src = re.sub(r'\d+,(\s*//\s*bits_index)',  lambda m: "%d,%s" % (bits_index, m.group(1)),
                 src, count=1)
    src = src.replace("#include \"kcm_ili9341_font.h\"",
                      "// U+00B7 MIDDLE DOT added by add_middot.py (index2 extended 181 -> 183).\n"
                      "#include \"kcm_ili9341_font.h\"", 1)
    tmp = path + ".new"
    open(tmp, "w").write(src)

    # --- verify: every original glyph identical, and the new one as designed ---
    G = Font(tmp)
    assert G.f["index2_last"] == MIDDOT, "index2_last not updated"
    for c, g in before.items():
        n = G.glyph(c)
        assert n and (n['w'], n['h'], n['delta'], n['rows']) == (g['w'], g['h'], g['delta'], g['rows']), \
            "glyph 0x%02X changed in %s" % (c, path)
    m = G.glyph(MIDDOT)
    assert (m['w'], m['h'], m['delta'], m['rows']) == (w, h, delta, rows), "middot mismatch"
    p = G.glyph(PILCROW)
    assert (p['w'], p['h'], p['delta']) == (0, 0, 0), "pilcrow filler not blank"
    os.replace(tmp, path)
    return len(before), w, h, delta, len(F.data), len(data), F.f["bits_index"], bits_index

if __name__ == "__main__":
    d = sys.argv[1]
    for fn in sorted(os.listdir(d)):
        if not re.match(r'Roboto_Black_\d+\.c$', fn): continue
        n, w, h, dl, o, nn, bi0, bi = patch(os.path.join(d, fn))
        print("%-22s %3d glyphs verified | middot w=%-2d h=%-2d delta=%-2d | data %d->%d | bits_index %d%s"
              % (fn, n, w, h, dl, o, nn, bi, "" if bi == bi0 else " (was %d)" % bi0))
