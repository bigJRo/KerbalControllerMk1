#!/usr/bin/env python3
"""
Convert sumotoy tFont .c files (Kerbal Controller Mk1 displays) to ILI9341_t3
font format (.c) for the wwatson4506 TeensyRA8876-GFX-Common text engine.

Source glyph format (sumotoy/LCD-Image-Converter "RA8875" template):
  1bpp, row-major, MSB-first, packed CONTIGUOUSLY (no per-row byte padding),
  width*height bits per glyph, 1 = foreground. height = tFont.font_height.

ILI9341_t3 target format (confirmed from PaulStoffregen/ILI9341_t3 drawFontChar):
  index[] : bit-packed, bits_index-wide entries, value = BYTE offset of glyph in
            data[] (glyphs start byte-aligned).
  data[]  : per glyph, bit-packed, MSB-first:
              [3 bits] encoding = 0
              [bits_width]   width
              [bits_height]  height
              [bits_xoffset] xoffset (signed)
              [bits_yoffset] yoffset (signed)
              [bits_delta]   delta (advance)
            then for each row: [1 bit flag=0 literal][width bits row pixels].
  Vertical: origin_y = cursor_y + cap_height - height - yoffset.
  We make glyphs full-height & top-aligned: height=font_height, yoffset=0,
  cap_height=font_height  =>  origin_y = cursor_y (matches old top-left cursor).

Verification: every glyph is decoded back from the emitted stream and compared
to the source bitmap. Any mismatch aborts with a report.
"""
import re, sys, os, math, glob

# ---- bit helpers (MSB-first, matching ILI9341_t3 fetchbits) -----------------
class BitWriter:
    def __init__(self):
        self.bits = []          # list of 0/1
    def put(self, value, nbits):
        for i in range(nbits - 1, -1, -1):
            self.bits.append((value >> i) & 1)
    def put_signed(self, value, nbits):
        self.put(value & ((1 << nbits) - 1), nbits)
    def bitlen(self):
        return len(self.bits)
    def pad_to_byte(self):
        while len(self.bits) % 8:
            self.bits.append(0)
    def bytes(self):
        out = bytearray()
        for i in range(0, len(self.bits), 8):
            b = 0
            for j in range(8):
                b = (b << 1) | (self.bits[i + j] if i + j < len(self.bits) else 0)
            out.append(b)
        return out

def fetchbits_unsigned(data_bits, index, required):
    v = 0
    for i in range(required):
        v = (v << 1) | data_bits[index + i]
    return v

def fetchbits_signed(data_bits, index, required):
    v = fetchbits_unsigned(data_bits, index, required)
    if v & (1 << (required - 1)):
        v -= (1 << required)
    return v

# ---- parse a sumotoy tFont .c ----------------------------------------------
def parse_tfont(path):
    txt = open(path, encoding='utf-8', errors='replace').read()
    # font_height from:  const tFont NAME = { LEN, NAME_array, fw, HEIGHT, comp };
    m = re.search(r'const\s+tFont\s+(\w+)\b[^=]*=\s*\{\s*(\d+)\s*,\s*(\w+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}', txt)
    if not m:
        raise RuntimeError(f"{path}: tFont decl not found")
    fontname = m.group(1)
    font_height = int(m.group(5))

    # image_data arrays:  uint8_t NAME[len] ... = { bytes };
    datamap = {}
    for dm in re.finditer(r'uint8_t\s+(\w+)\s*\[\d+\]\s*[^=]*=\s*\{([^}]*)\}', txt):
        name = dm.group(1)
        nums = re.findall(r'0x[0-9a-fA-F]+|\d+', dm.group(2))
        datamap[name] = bytes(int(n, 16) if n.lower().startswith('0x') else int(n) for n in nums)

    # tImage:  tImage IMG ... = { DATAVAR, WIDTH, DATALEN };
    widthmap, imgdata = {}, {}
    for im in re.finditer(r'tImage\s+(\w+)\s*[^=]*=\s*\{\s*(\w+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}', txt):
        img, datavar, width = im.group(1), im.group(2), int(im.group(3))
        widthmap[img] = width
        imgdata[img] = datamap.get(datavar, b'')

    # array entries:  { 0xNN, &IMG }
    glyphs = {}   # code -> (width, databytes)
    for am in re.finditer(r'\{\s*(0x[0-9a-fA-F]+)\s*,\s*&(\w+)\s*\}', txt):
        code = int(am.group(1), 16)
        img = am.group(2)
        if img in widthmap:
            glyphs[code] = (widthmap[img], imgdata[img])
    return fontname, font_height, glyphs

def src_pixel(data, width, height, row, col):
    idx = row * width + col
    if idx >> 3 >= len(data):
        return 0
    return (data[idx >> 3] >> (7 - (idx & 7))) & 1

# ---- encode one font --------------------------------------------------------
def convert(path, outdir):
    fontname, fh, glyphs = parse_tfont(path)
    codes = sorted(glyphs.keys())
    maxw = max((glyphs[c][0] for c in codes), default=1)

    bits_width  = max(1, maxw.bit_length())
    bits_height = max(1, fh.bit_length())
    bits_xoffset = 1   # always 0
    bits_yoffset = 1   # always 0
    bits_delta  = max(1, maxw.bit_length())

    # Ranges: range1 = printable ASCII present; range2 = special block if any.
    ascii_codes  = [c for c in codes if 0x20 <= c <= 0x7e]
    hi_codes     = [c for c in codes if c >= 0x80]
    if ascii_codes:
        i1f, i1l = min(ascii_codes), max(ascii_codes)
    else:
        i1f, i1l = 0x20, 0x20
    if hi_codes:
        i2f, i2l = min(hi_codes), max(hi_codes)
    else:
        i2f = i2l = 0   # no second range

    # Build a per-glyph encoded blob (byte-aligned), record byte offsets.
    def encode_glyph(width, data):
        bw = BitWriter()
        bw.put(0, 3)                 # encoding = 0
        bw.put(width, bits_width)
        bw.put(fh, bits_height)
        bw.put_signed(0, bits_xoffset)
        bw.put_signed(0, bits_yoffset)
        bw.put(width, bits_delta)    # delta = advance = width
        for r in range(fh):
            bw.put(0, 1)             # literal row flag
            for col in range(width):
                bw.put(src_pixel(data, width, fh, r, col), 1)
        bw.pad_to_byte()
        return bw.bytes()

    data = bytearray()
    blank_off = 0
    data += encode_glyph(0, b'')     # blank glyph at offset 0 for missing codes
    offset_of = {}
    for c in codes:
        w, d = glyphs[c]
        offset_of[c] = len(data)
        data += encode_glyph(w, d)

    # bits_index sized to address any byte in data[].
    bits_index = max(1, (len(data)).bit_length())

    # Build index[] over range1 then range2, in code order.
    iw = BitWriter()
    full_range = list(range(i1f, i1l + 1))
    if i2l >= i2f and (i2f != 0 or i2l != 0):
        full_range += list(range(i2f, i2l + 1))
    for c in full_range:
        iw.put(offset_of.get(c, blank_off), bits_index)
    iw.pad_to_byte()
    index = iw.bytes()

    # ----- verify by decoding every present glyph back to a bitmap -----------
    data_bits = []
    for b in data:
        for j in range(7, -1, -1):
            data_bits.append((b >> j) & 1)
    for c in codes:
        w, d = glyphs[c]
        off = offset_of[c] * 8
        bo = off
        enc = fetchbits_unsigned(data_bits, bo, 3); bo += 3
        assert enc == 0, f"{fontname} 0x{c:02x}: encoding!=0"
        gw = fetchbits_unsigned(data_bits, bo, bits_width); bo += bits_width
        gh = fetchbits_unsigned(data_bits, bo, bits_height); bo += bits_height
        xo = fetchbits_signed(data_bits, bo, bits_xoffset); bo += bits_xoffset
        yo = fetchbits_signed(data_bits, bo, bits_yoffset); bo += bits_yoffset
        dl = fetchbits_unsigned(data_bits, bo, bits_delta); bo += bits_delta
        assert (gw, gh, xo, yo, dl) == (w, fh, 0, 0, w), \
            f"{fontname} 0x{c:02x}: header {(gw,gh,xo,yo,dl)} != {(w,fh,0,0,w)}"
        for r in range(gh):
            flag = data_bits[bo]; bo += 1
            assert flag == 0, f"{fontname} 0x{c:02x} row {r}: flag!=0"
            for col in range(gw):
                bit = data_bits[bo]; bo += 1
                exp = src_pixel(d, w, fh, r, col)
                assert bit == exp, f"{fontname} 0x{c:02x} px({r},{col}) {bit}!={exp}"

    # verify index round-trips to the right offsets
    index_bits = []
    for b in index:
        for j in range(7, -1, -1):
            index_bits.append((b >> j) & 1)
    for n, c in enumerate(full_range):
        got = fetchbits_unsigned(index_bits, n * bits_index, bits_index)
        assert got == offset_of.get(c, blank_off), f"{fontname} index 0x{c:02x} {got}"

    # ----- emit .c ----------------------------------------------------------
    line_space = fh + max(2, fh // 8)
    cap_height = fh
    def carr(name, b):
        s = [f"static const unsigned char {name}[] = {{"]
        for i in range(0, len(b), 16):
            s.append("  " + ", ".join(f"0x{x:02X}" for x in b[i:i+16]) + ",")
        s.append("};")
        return "\n".join(s)

    out = []
    out.append("// Auto-generated from sumotoy tFont by tfont_to_ili9341.py — DO NOT EDIT.")
    out.append(f"// Source font: {fontname}  (height {fh}, {len(codes)} glyphs)")
    out.append("// Round-trip verified: every glyph bitmap decodes identically to the source.")
    out.append('#include "kcm_ili9341_font.h"')
    out.append("")
    out.append(carr(f"{fontname}_data", bytes(data)))
    out.append("")
    out.append(carr(f"{fontname}_index", bytes(index)))
    out.append("")
    out.append(f"const ILI9341_t3_font_t {fontname} = {{")
    out.append(f"  {fontname}_index,")
    out.append("  0, // unicode")
    out.append(f"  {fontname}_data,")
    out.append("  1, // version")
    out.append("  0, // reversed")
    out.append(f"  {i1f}, // index1_first")
    out.append(f"  {i1l}, // index1_last")
    out.append(f"  {i2f}, // index2_first")
    out.append(f"  {i2l}, // index2_last")
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
    with open(os.path.join(outdir, f"{fontname}.c"), "w") as f:
        f.write("\n".join(out))
    return fontname, fh, len(codes), len(data), len(index), bits_index

def main():
    srcdir, outdir = sys.argv[1], sys.argv[2]
    files = sorted(glob.glob(os.path.join(srcdir, "*.c")))
    print(f"Converting {len(files)} font(s) from {srcdir}\n")
    rows = []
    for p in files:
        try:
            r = convert(p, outdir)
            rows.append(r)
            print(f"  OK  {r[0]:<20} h={r[1]:<3} glyphs={r[2]:<4} data={r[3]:<6}B index={r[4]:<4}B bits_index={r[5]}")
        except Exception as e:
            print(f"  FAIL {os.path.basename(p)}: {e}")
            raise
    print(f"\nAll {len(rows)} fonts converted and round-trip verified -> {outdir}")

if __name__ == "__main__":
    main()
