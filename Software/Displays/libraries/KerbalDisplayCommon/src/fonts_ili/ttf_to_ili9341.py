#!/usr/bin/env python3
"""
Render a monospace TTF to ILI9341_t3 1-bit font .c files (KCMk1 displays).

Output format is byte-identical in structure to tfont_to_ili9341.py so the
wwatson4506 RA8876 GFX text engine and getFontStringWidth() treat these fonts
exactly like the existing ones:
  index[] : bit-packed, bits_index-wide, value = BYTE offset of glyph in data[]
  data[]  : per glyph, MSB-first: [3b enc=0][width][height][xoff][yoff][delta]
            then per row [1b flag=0][width bits], byte-padded per glyph.
  Glyphs full-height, top-aligned (height=cell_h, yoffset=0, cap_height=cell_h)
  so origin_y == cursor_y. Monospace: width == delta == cell_w for every glyph.

Every glyph is round-trip decoded with the ILI9341_t3 fetchbits logic and
asserted pixel-identical before emitting. ASCII 0x20..0x7e only.

Usage: ttf_to_ili9341.py FONT.ttf OUTDIR NAME SIZE [SIZE ...]
"""
import sys, os
from PIL import Image, ImageFont, ImageDraw

class BitWriter:
    def __init__(self): self.bits = []
    def put(self, value, nbits):
        for i in range(nbits - 1, -1, -1): self.bits.append((value >> i) & 1)
    def put_signed(self, value, nbits): self.put(value & ((1 << nbits) - 1), nbits)
    def pad_to_byte(self):
        while len(self.bits) % 8: self.bits.append(0)
    def bytes(self):
        out = bytearray()
        for i in range(0, len(self.bits), 8):
            b = 0
            for j in range(8): b = (b << 1) | (self.bits[i + j] if i + j < len(self.bits) else 0)
            out.append(b)
        return out

def fetchbits_unsigned(bits, index, req):
    v = 0
    for i in range(req): v = (v << 1) | bits[index + i]
    return v
def fetchbits_signed(bits, index, req):
    v = fetchbits_unsigned(bits, index, req)
    return v - (1 << req) if v & (1 << (req - 1)) else v

def render_glyphs(ttf_path, px):
    """Return (cell_w, cell_h, {code: [rows of 0/1 bits], ...}) for ASCII 0x20..0x7e."""
    font = ImageFont.truetype(ttf_path, px)
    ascent, descent = font.getmetrics()
    cell_h = ascent + descent
    # Monospace advance (all glyphs equal). Use a wide glyph to be safe.
    cell_w = round(font.getlength("M"))
    codes = list(range(0x20, 0x7f))
    glyphs = {}
    for c in codes:
        ch = chr(c)
        # Render into an oversized canvas, threshold at 128 (matches the approved
        # 1-bit preview), then verify the ink fits the monospace cell.
        pad = px
        canvas = Image.new("L", (cell_w + 2 * pad, cell_h + 2 * pad), 0)
        d = ImageDraw.Draw(canvas)
        d.fontmode = "L"                       # antialiased, we threshold below
        d.text((pad, pad), ch, font=font, fill=255, anchor="la")
        canvas = canvas.point(lambda p: 1 if p >= 128 else 0)
        px_data = canvas.load()
        # Ink bbox check — nothing may fall outside the (0..cell_w, 0..cell_h) cell
        # once we shift the pad off. anchor "la" puts the ascender row at y=pad.
        rows = []
        for r in range(cell_h):
            row = []
            for col in range(cell_w):
                row.append(px_data[pad + col, pad + r])
            rows.append(row)
        # clipping guard: assert no ink in the pad margins that belongs to this glyph
        ink_outside = 0
        W, H = canvas.size
        for yy in range(H):
            for xx in range(W):
                if px_data[xx, yy]:
                    inx = pad <= xx < pad + cell_w
                    iny = pad <= yy < pad + cell_h
                    if not (inx and iny): ink_outside += 1
        if ink_outside:
            raise RuntimeError(f"glyph 0x{c:02x} '{ch}' has {ink_outside}px ink outside "
                               f"the {cell_w}x{cell_h} cell at size {px}")
        glyphs[c] = rows
    return cell_w, cell_h, glyphs

def emit(name, ttf_path, px, outdir):
    cw, ch, glyphs = render_glyphs(ttf_path, px)
    codes = sorted(glyphs.keys())
    bits_width  = max(1, cw.bit_length())
    bits_height = max(1, ch.bit_length())
    bits_xoffset = 1
    bits_yoffset = 1
    bits_delta  = max(1, cw.bit_length())

    def encode_glyph(rows):
        bw = BitWriter()
        bw.put(0, 3)
        bw.put(cw, bits_width)
        bw.put(ch, bits_height)
        bw.put_signed(0, bits_xoffset)
        bw.put_signed(0, bits_yoffset)
        bw.put(cw, bits_delta)
        for r in range(ch):
            bw.put(0, 1)
            for col in range(cw): bw.put(rows[r][col], 1)
        bw.pad_to_byte()
        return bw.bytes()

    data = bytearray()
    data += encode_glyph([[0]*cw for _ in range(ch)])   # blank glyph at offset 0
    offset_of = {}
    for c in codes:
        offset_of[c] = len(data)
        data += encode_glyph(glyphs[c])

    bits_index = max(1, len(data).bit_length())
    i1f, i1l = 0x20, 0x7e
    iw = BitWriter()
    for c in range(i1f, i1l + 1):
        iw.put(offset_of.get(c, 0), bits_index)
    iw.pad_to_byte()
    index = iw.bytes()

    # ---- round-trip verify (ILI9341_t3 decode) ----
    data_bits = []
    for b in data:
        for j in range(7, -1, -1): data_bits.append((b >> j) & 1)
    for c in codes:
        bo = offset_of[c] * 8
        assert fetchbits_unsigned(data_bits, bo, 3) == 0; bo += 3
        gw = fetchbits_unsigned(data_bits, bo, bits_width); bo += bits_width
        gh = fetchbits_unsigned(data_bits, bo, bits_height); bo += bits_height
        xo = fetchbits_signed(data_bits, bo, bits_xoffset); bo += bits_xoffset
        yo = fetchbits_signed(data_bits, bo, bits_yoffset); bo += bits_yoffset
        dl = fetchbits_unsigned(data_bits, bo, bits_delta); bo += bits_delta
        assert (gw, gh, xo, yo, dl) == (cw, ch, 0, 0, cw), f"0x{c:02x} hdr {(gw,gh,xo,yo,dl)}"
        for r in range(gh):
            assert data_bits[bo] == 0; bo += 1
            for col in range(gw):
                assert data_bits[bo] == glyphs[c][r][col], f"0x{c:02x} px({r},{col})"; bo += 1
    ib = []
    for b in index:
        for j in range(7, -1, -1): ib.append((b >> j) & 1)
    for n, c in enumerate(range(i1f, i1l + 1)):
        assert fetchbits_unsigned(ib, n * bits_index, bits_index) == offset_of.get(c, 0)

    line_space = ch + max(2, ch // 8)
    cap_height = ch
    def carr(nm, b):
        s = [f"static const unsigned char {nm}[] = {{"]
        for i in range(0, len(b), 16):
            s.append("  " + ", ".join(f"0x{x:02X}" for x in b[i:i+16]) + ",")
        s.append("};")
        return "\n".join(s)

    out = []
    out.append(f"// Auto-generated from Terminalia-Regular.ttf by ttf_to_ili9341.py — DO NOT EDIT.")
    out.append(f"// Terminalia (OFL 1.1), a Terminus-faithful rebuild; see OFL.txt in this dir.")
    out.append(f"// Font: {name}  (cell {cw}x{ch}, monospace, {len(codes)} glyphs, ASCII 0x20-0x7e)")
    out.append(f"// Round-trip verified: every glyph decodes identically via ILI9341_t3 bit logic.")
    out.append('#include "kcm_ili9341_font.h"')
    out.append("")
    out.append(carr(f"{name}_data", bytes(data)))
    out.append("")
    out.append(carr(f"{name}_index", bytes(index)))
    out.append("")
    out.append(f"const ILI9341_t3_font_t {name} = {{")
    out.append(f"  {name}_index,")
    out.append("  0, // unicode")
    out.append(f"  {name}_data,")
    out.append("  1, // version")
    out.append("  0, // reversed")
    out.append(f"  {i1f}, // index1_first")
    out.append(f"  {i1l}, // index1_last")
    out.append("  0, // index2_first")
    out.append("  0, // index2_last")
    out.append(f"  {bits_index}, // bits_index")
    out.append(f"  {bits_width}, // bits_width")
    out.append(f"  {bits_height}, // bits_height")
    out.append(f"  {bits_xoffset}, // bits_xoffset")
    out.append(f"  {bits_yoffset}, // bits_yoffset")
    out.append(f"  {bits_delta}, // bits_delta")
    out.append(f"  {line_space}, // line_space")
    out.append(f"  {cap_height}, // cap_height")
    out.append("};")
    out.append("")
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, f"{name}.c"), "w") as f:
        f.write("\n".join(out))
    return cw, ch, len(data), len(index), bits_index

if __name__ == "__main__":
    ttf, outdir, base = sys.argv[1], sys.argv[2], sys.argv[3]
    for s in sys.argv[4:]:
        s = int(s)
        cw, ch, dl, il, bi = emit(f"{base}_{s}", ttf, s, outdir)
        print(f"  OK {base}_{s:<3} cell={cw}x{ch} data={dl}B index={il}B bits_index={bi}")
    print("All fonts generated and round-trip verified.")
