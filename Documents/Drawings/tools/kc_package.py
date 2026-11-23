#!/usr/bin/env python3
"""
kc_package.py -- build a board's drawing package (SCH, PWB, BOM, PLACE) in the
common A3 frame (kc_frame.py) from the files EasyEDA Pro exports.

    python3 kc_package.py boards/KC-01-2700.json --out <dir> [--docs 1,2,3,4]

The board config (JSON) names the input files and the title-block fields:
    sch_pdf      EasyEDA schematic PDF (all pages)
    gerber_dir   unzipped Gerber + drill files
    bom_csv      EasyEDA BOM export (UTF-16, tab separated)
    pnp_csv      EasyEDA pick-and-place export (UTF-16, tab separated)
Paths in the config are relative to the config file.

Needs: gerbonara, numpy, scikit-image, pillow (pip), poppler (pdftocairo) and
Playwright with Chromium (node) for printing to PDF.
"""
import argparse
import csv
import glob
import io
import json
import math
import os
import re
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kc_frame import (DOC_TYPES, GREY, HI, H, INK, M_IN, W, Sheet, esc, file_name,  # noqa: E402
                      frame, notes_box, wrap_text)

# --------------------------------------------------------------------------- printing


def print_pdf(sheets, out_pdf, work):
    """sheets: list of Sheet. Writes one HTML page per sheet and prints with Chromium."""
    os.makedirs(work, exist_ok=True)
    body = "".join(f'<div class="pg">{s.svg()}</div>' for s in sheets)
    hp = os.path.join(work, "doc.html")
    open(hp, "w", encoding="utf-8").write(
        '<!doctype html><html><head><meta charset="utf-8"><style>@page{size:420mm 297mm;margin:0}'
        'html,body{margin:0}.pg{width:420mm;height:297mm;page-break-after:always;overflow:hidden}'
        '.pg:last-child{page-break-after:auto}</style></head><body>' + body + '</body></html>')
    js = """const { chromium } = require('playwright');
(async () => { const b = await chromium.launch(); const p = await b.newPage();
await p.goto('file://' + process.argv[2], {waitUntil: 'load'});
await p.pdf({path: process.argv[3], width: '420mm', height: '297mm', printBackground: true});
await b.close(); })();"""
    jp = os.path.join(work, "print.js")
    open(jp, "w").write(js)
    subprocess.run(["node", jp, os.path.abspath(hp), os.path.abspath(out_pdf)], check=True)


def png_href(img, path):
    img.save(path, optimize=True)
    return "file://" + os.path.abspath(path)

# --------------------------------------------------------------------------- Gerber rendering


def _arc_pts(p1, p2, cw, c, step):
    a1 = math.atan2(p1[1] - c[1], p1[0] - c[0])
    a2 = math.atan2(p2[1] - c[1], p2[0] - c[0])
    r = math.hypot(p1[0] - c[0], p1[1] - c[1])
    if abs(p1[0] - p2[0]) < 1e-9 and abs(p1[1] - p2[1]) < 1e-9:
        a2 = a1 + (-2 * math.pi if cw else 2 * math.pi)
    elif cw:
        if a2 >= a1:
            a2 -= 2 * math.pi
    elif a2 <= a1:
        a2 += 2 * math.pi
    n = max(2, int(abs(a2 - a1) * r / step) + 1)
    return [(c[0] + r * math.cos(a1 + (a2 - a1) * k / n), c[1] + r * math.sin(a1 + (a2 - a1) * k / n))
            for k in range(1, n + 1)]


def _poly(prim, step=0.03):
    # gerbonara's arc "clockwise" flag is in y-down (SVG) terms; Gerber data here is y-up
    ap = prim if hasattr(prim, "outline") else prim.to_arc_poly()
    out = list(ap.outline)
    ac = list(ap.arc_centers) if ap.arc_centers else [None] * len(out)
    pts = [out[0]]
    for k in range(len(out)):
        p1, p2 = out[k], out[(k + 1) % len(out)]
        seg = ac[k] if k < len(ac) else None
        if seg:
            pts += _arc_pts(p1, p2, not seg[0], seg[1], step)
        else:
            pts.append(p2)
    return pts


class Raster:
    """Rasterise Gerber layers into a window that covers the board (mm, y up)."""

    def __init__(self, bounds, res=0.05, pad=1.0):
        (self.x0, self.y0), (x1, y1) = bounds
        self.x0 -= pad
        self.y1 = y1 + pad
        self.res = res
        self.nx = int((x1 + pad - self.x0) / res) + 1
        self.ny = int((self.y1 - (self.y0 - pad)) / res) + 1

    def layer(self, path):
        from gerbonara import GerberFile
        from gerbonara.utils import MM
        from skimage.draw import polygon as skpoly
        a = np.zeros((self.ny, self.nx), dtype=bool)
        for obj in GerberFile.open(path).objects:
            for prim in obj.to_primitives(MM):
                pts = _poly(prim)
                if len(pts) < 3:
                    continue
                xs = np.array([(p[0] - self.x0) / self.res for p in pts]) - 0.5
                ys = np.array([(self.y1 - p[1]) / self.res for p in pts]) - 0.5
                rr, cc = skpoly(ys, xs, shape=a.shape)
                a[rr, cc] = bool(prim.polarity_dark)
        return a

    def mm_box(self):
        """(x, y_top, width, height) of the raster in board mm (y up)."""
        return self.x0, self.y1, self.nx * self.res, self.ny * self.res


def colorize(layers, bg=(255, 255, 255)):
    """layers: list of (bool array, (r,g,b)) painted in order."""
    h, w = layers[0][0].shape
    img = np.zeros((h, w, 3), dtype=np.uint8)
    img[:] = bg
    for a, col in layers:
        img[a] = col
    return Image.fromarray(img)

# --------------------------------------------------------------------------- board data


def read_utf16_tsv(path):
    raw = open(path, "rb").read()
    txt = raw.decode("utf-16") if raw[:2] in (b"\xff\xfe", b"\xfe\xff") else raw.decode("utf-8-sig")
    return list(csv.DictReader(io.StringIO(txt), delimiter="\t"))


def outline_info(gdir):
    from gerbonara import GerberFile
    from gerbonara.utils import MM
    g = GerberFile.open(glob.glob(os.path.join(gdir, "*.GKO"))[0])
    (x0, y0), (x1, y1) = g.bounding_box(MM)
    ap = 0.0
    radii = []
    for o in g.objects:
        if type(o).__name__ == "Line":
            ap = o.aperture.diameter
        if type(o).__name__ == "Arc":
            radii.append(round(math.hypot(o.cx, o.cy), 3))
    x0 += ap / 2; y0 += ap / 2; x1 -= ap / 2; y1 -= ap / 2
    return {"bounds": ((x0, y0), (x1, y1)), "w": x1 - x0, "h": y1 - y0, "radius": max(radii) if radii else 0.0}


def drills(gdir):
    from gerbonara import ExcellonFile
    import warnings
    warnings.simplefilter("ignore")
    holes = []
    for f in sorted(glob.glob(os.path.join(gdir, "*.DRL"))):
        via = "via" in os.path.basename(f).lower()
        for o in ExcellonFile.open(f).objects:
            if hasattr(o, "x"):
                d = round(o.tool.diameter, 2)
                holes.append({"x": o.x, "y": o.y, "d": d, "plated": o.tool.plated is not False, "via": via})
    return holes

# --------------------------------------------------------------------------- symbols


SYMS = ["circle", "square", "triangle", "diamond", "cross", "hex", "star", "xcircle"]


def symbol(s, kind, x, y, r=0.9, sw=0.18, c=INK):
    if kind == "circle":
        s.circle(x, y, r, sw, c=c)
    elif kind == "square":
        s.rect(x - r, y - r, 2 * r, 2 * r, sw, c=c)
    elif kind == "triangle":
        s.add(f'<polygon points="{x},{y - r} {x + r},{y + r * 0.8} {x - r},{y + r * 0.8}" fill="none" stroke="{c}" stroke-width="{sw}"/>')
    elif kind == "diamond":
        s.add(f'<polygon points="{x},{y - r} {x + r},{y} {x},{y + r} {x - r},{y}" fill="none" stroke="{c}" stroke-width="{sw}"/>')
    elif kind == "cross":
        s.line(x - r, y - r, x + r, y + r, sw, c)
        s.line(x - r, y + r, x + r, y - r, sw, c)
    elif kind == "hex":
        pts = " ".join(f"{x + r * math.cos(math.pi / 3 * k):.3f},{y + r * math.sin(math.pi / 3 * k):.3f}" for k in range(6))
        s.add(f'<polygon points="{pts}" fill="none" stroke="{c}" stroke-width="{sw}"/>')
    elif kind == "star":
        s.line(x - r, y, x + r, y, sw, c)
        s.line(x, y - r, x, y + r, sw, c)
        s.line(x - r * 0.7, y - r * 0.7, x + r * 0.7, y + r * 0.7, sw, c)
        s.line(x - r * 0.7, y + r * 0.7, x + r * 0.7, y - r * 0.7, sw, c)
    else:
        s.circle(x, y, r, sw, c=c)
        s.line(x - r * 0.7, y - r * 0.7, x + r * 0.7, y + r * 0.7, sw, c)

# --------------------------------------------------------------------------- dimensions


def dim_h(s, x1, x2, y, ytext, label, ext_from=None, size=2.6):
    """Horizontal dimension between x1 and x2 drawn at y; extension lines from ext_from (y)."""
    if ext_from is not None:
        for x in (x1, x2):
            s.line(x, ext_from + (0.8 if y > ext_from else -0.8), x, y + (1.2 if y > ext_from else -1.2), 0.15)
    s.line(x1, y, x2, y, 0.18)
    for x, d in ((x1, 1), (x2, -1)):
        s.add(f'<polygon points="{x},{y} {x + d * 2.2},{y - 0.6} {x + d * 2.2},{y + 0.6}" fill="{INK}"/>')
    s.text((x1 + x2) / 2, ytext, label, size=size, anchor="middle")


def dim_v(s, y1, y2, x, label, ext_from=None, size=2.6):
    if ext_from is not None:
        for y in (y1, y2):
            s.line(ext_from + (0.8 if x > ext_from else -0.8), y, x + (1.2 if x > ext_from else -1.2), y, 0.15)
    s.line(x, y1, x, y2, 0.18)
    for y, d in ((y1, 1), (y2, -1)):
        s.add(f'<polygon points="{x},{y} {x - 0.6},{y + d * 2.2} {x + 0.6},{y + d * 2.2}" fill="{INK}"/>')
    tx = x - 1.4 if x < ext_from else x + 3.0
    s.text(tx, (y1 + y2) / 2, label, size=size, anchor="middle", rot=-90)

# --------------------------------------------------------------------------- documents


def fields(cfg, d, sheet, sheets, scale):
    return dict(dwg_no=cfg["base"] + d, rev=cfg.get("rev", "-"), title=cfg["title"], subtitle=cfg["subtitle"],
                doc_type=DOC_TYPES[d][1], status=cfg.get("status", "DRAFT"), created_by=cfg["created_by"],
                approved_by=cfg["approved_by"], date=cfg["date"], board_version=cfg["board_version"],
                scale=scale, sheet=sheet, sheets=sheets, revisions=cfg.get("revisions"))


def doc_pwb(cfg, work):
    gdir = cfg["gerber_dir"]
    ol = outline_info(gdir)
    holes = drills(gdir)
    (bx0, by0), (bx1, by1) = ol["bounds"]
    sheets = []

    # ---- sheet 1: outline, holes, drill chart, stackup, notes
    s = Sheet()
    a = frame(s, fields(cfg, "2", 1, 2, "1:1"))
    ox, oy = 34.0, 58.0           # sheet position of the board's top-left corner (1:1)

    def P(x, y):                  # board mm (y up, origin top-left) -> sheet mm
        return ox + (x - bx0), oy + (by1 - y)
    s.text(ox, oy - 15, "BOARD OUTLINE AND HOLES (TOP VIEW)", size=3.4, weight="bold")
    s.text(ox, oy - 10.5, f"Scale 1:1. Datum: board corner at top left. All holes are drilled from the top.", size=2.3, c=GREY)
    r = ol["radius"]
    x0s, y0s = P(bx0, by1)
    s.rect(x0s, y0s, ol["w"], ol["h"], sw=0.5, rx=r)
    tools = sorted({(h["d"], h["plated"]) for h in holes})
    sym = {t: SYMS[k % len(SYMS)] for k, t in enumerate(tools)}
    for hl in holes:
        x, y = P(hl["x"], hl["y"])
        symbol(s, sym[(hl["d"], hl["plated"])], x, y, r=0.55 if hl["d"] < 0.5 else 0.9, sw=0.15)
    # overall dimensions
    dim_h(s, x0s, x0s + ol["w"], y0s - 5, y0s - 6.2, f"{ol['w']:.2f}", ext_from=y0s)
    dim_v(s, y0s, y0s + ol["h"], x0s - 6, f"{ol['h']:.2f}", ext_from=x0s)
    # mounting holes (largest tool)
    big = max(h["d"] for h in holes)
    mh = sorted([h for h in holes if h["d"] == big], key=lambda h: (-h["y"], h["x"]))
    if mh:
        hx, hy = P(mh[0]["x"], mh[0]["y"])
        dx, dy = mh[0]["x"] - bx0, by1 - mh[0]["y"]
        s.line(hx, hy, hx + 14, hy + 9, 0.15)
        s.text(hx + 14.5, hy + 9.8, f"{len(mh)}× Ø{big:.2f} {'PTH' if mh[0]['plated'] else 'NPTH'}", size=2.4)
        s.text(hx + 14.5, hy + 12.8, f"{dx:.2f} from the side edges, {dy:.2f} from the top and bottom edges", size=2.2, c=GREY)
        if len(mh) > 1:
            xs = sorted({round(h["x"], 2) for h in mh})
            ys = sorted({round(h["y"], 2) for h in mh})
            if len(xs) > 1:
                ax, _ = P(xs[0], 0)
                bx_, _ = P(xs[-1], 0)
                dim_h(s, ax, bx_, y0s + ol["h"] + 6, y0s + ol["h"] + 10.2, f"{xs[-1] - xs[0]:.2f}", ext_from=y0s + ol["h"] - 3)
            if len(ys) > 1:
                _, ay = P(0, ys[-1])
                _, by_ = P(0, ys[0])
                dim_v(s, ay, by_, x0s + ol["w"] + 6, f"{ys[-1] - ys[0]:.2f}", ext_from=x0s + ol["w"] - 3)
    s.text(x0s + 2, y0s + ol["h"] + 16.5, f"Corners R{r:.2f} (4 places).  Outline tolerance ±0.2.", size=2.4)

    # drill chart
    tx, ty = 248.0, a["rev_y"] + 8
    s.text(tx, ty - 2, "DRILL CHART", size=3.0, weight="bold")
    rows = []
    for t in tools:
        n = sum(1 for h in holes if (h["d"], h["plated"]) == t)
        vias = sum(1 for h in holes if (h["d"], h["plated"]) == t and h["via"])
        kind = "Via" if vias == n else ("Component / via" if vias else ("Mounting" if t[0] == big else "Component"))
        rows.append(["", f"{t[0]:.2f}", "PTH" if t[1] else "NPTH", str(n), kind])
    cols = [("SYM", 12, "middle"), ("DIA (mm)", 22, "middle"), ("PLATING", 20, "middle"), ("QTY", 14, "middle"), ("USE", 52, "start")]
    yb = s.table(tx, ty, cols, rows, rh=5.0)
    for k, t in enumerate(tools):
        symbol(s, sym[t], tx + 6, ty + 5.2 + 5.0 * k + 2.5, r=1.1, sw=0.2)
    s.text(tx, yb + 4, f"Total holes: {len(holes)}.  Finished hole sizes; tolerance ±0.08.", size=2.3, c=GREY)

    # stackup
    sy = yb + 12
    s.text(tx, sy - 2, "LAYER STACKUP", size=3.0, weight="bold")
    srows = [[l["layer"], l["name"], l["material"], l["thickness"], l.get("use", "")] for l in cfg["stackup"]]
    scols = [("LAYER", 18, "middle"), ("NAME", 24, "start"), ("MATERIAL", 34, "start"), ("THK (mm)", 16, "middle"), ("USE", 66, "start")]
    yb2 = s.table(tx, sy, scols, srows, rh=4.6, size=2.2, wrap=wrap_text)
    s.text(tx, yb2 + 4, cfg.get("stackup_note", ""), size=2.2, c=GREY)

    notes_box(s, M_IN + 4, a["tb_y"], 196, H - M_IN - a["tb_y"], cfg["fab_notes"])
    sheets.append(s)

    # ---- sheet 2: copper layers and planes
    s = Sheet()
    a = frame(s, fields(cfg, "2", 2, 2, "3:4"))
    s.text(M_IN + 6, M_IN + 9, "COPPER LAYERS AND PLANES (TOP VIEW, LOOKING DOWN THROUGH THE BOARD)", size=3.4, weight="bold")
    s.text(M_IN + 6, M_IN + 14, "Copper shown grey. Planes are drawn as made: clearances (anti-pads) are the white rings.", size=2.3, c=GREY)
    R = Raster(ol["bounds"], res=0.05)
    k = 0.75
    pos = [(24, 50), (176, 50), (24, 140), (176, 140)]
    for (lx, ly), lay in zip(pos, cfg["layers"]):
        arr = R.layer(os.path.join(gdir, lay["file"]))
        img = colorize([(arr, (130, 133, 138))])
        bx, by_, bw, bh = R.mm_box()
        href = png_href(img, os.path.join(work, f"layer_{lay['file']}.png"))
        s.text(lx, ly - 3, f"{lay['layer']}  ·  {lay['name']}", size=3.0, weight="bold")
        s.text(lx + bw * k, ly - 3, lay.get("net", ""), size=2.4, anchor="end", c=GREY)
        s.image(lx, ly, bw * k, bh * k, href)
        s.rect(lx + (bx0 - bx) * k, ly + (by_ - by1) * k, ol["w"] * k, ol["h"] * k, sw=0.3, rx=r * k)
    # plane table on the right
    px, py = 330, 50
    s.text(px, py - 3, "LAYERS", size=3.0, weight="bold")
    trows = [[l["layer"], l["name"], l.get("net", ""), l.get("copper", "")] for l in cfg["layers"]]
    tcols = [("No.", 9, "middle"), ("NAME", 21, "start"), ("NET / USE", 30, "start"), ("Cu", 12, "middle")]
    yb = s.table(px, py, tcols, trows, rh=5.2, size=2.2, wrap=wrap_text)
    yy = yb + 6
    for n in cfg.get("plane_notes", []):
        for part in wrap_text(n, 72, 2.2):
            s.text(px, yy, part, size=2.2)
            yy += 3.0
        yy += 1.2
    sheets.append(s)
    return sheets


def doc_bom(cfg, work):
    rows_in = read_utf16_tsv(cfg["bom_csv"])
    pnp = {r["Designator"].strip('"'): r for r in read_utf16_tsv(cfg["pnp_csv"])}
    rows = []
    for k, r in enumerate(rows_in, 1):
        des = r["Designator"]
        first = des.split(",")[0]
        smd = pnp.get(first, {}).get("SMD", "").strip('"')
        side = pnp.get(first, {}).get("Layer", "").strip('"')
        asm = ("SMT" if smd == "Yes" else "Hand") + (f" ({'top' if side == 'T' else 'bottom'})" if side else "")
        desc = r.get("Secondary Category") or r.get("Primary Category", "")
        mpn = re.sub(r"[^\x00-\x7f]+", "", r.get("Manufacturer Part", "")).strip()
        pkg = r.get("Footprint", "").split("_")[0]
        rows.append([str(k), r["Quantity"], des.replace(",", ", "), r.get("Value", ""), pkg, desc,
                     r.get("Manufacturer", "").split("(")[0], mpn,
                     r.get("Supplier Part", "") or "-", (r.get("JLCPCB Part Class", "") or "-").replace(" Part", ""), asm])
    s = Sheet()
    a = frame(s, fields(cfg, "3", 1, 1, "NTS"))
    x, y = M_IN + 6, M_IN + 9
    s.text(x, y, "BILL OF MATERIALS  ·  PRINTED CIRCUIT CARD ASSEMBLY", size=3.4, weight="bold")
    s.text(x, y + 5, f"Board {cfg['base']}2 {cfg['board_version']}. Quantities per assembled card. Generated from the EasyEDA BOM export.", size=2.3, c=GREY)
    cols = [("ITEM", 10, "middle"), ("QTY", 9, "middle"), ("DESIGNATORS", 78, "start"), ("VALUE", 16, "start"),
            ("PACKAGE", 26, "start"), ("DESCRIPTION", 78, "start"), ("MANUFACTURER", 32, "start"),
            ("MFR PART No.", 46, "start"), ("LCSC", 20, "start"), ("CLASS", 15, "middle"), ("ASSEMBLY", 24, "start")]
    yb = s.table(x, a["rev_y"] + 5, cols, rows, rh=4.6, size=2.2, wrap=wrap_text)
    notes_box(s, M_IN + 4, a["tb_y"], 196, H - M_IN - a["tb_y"], cfg["bom_notes"])
    return [s]


def doc_place(cfg, work):
    gdir = cfg["gerber_dir"]
    ol = outline_info(gdir)
    (bx0, by0), (bx1, by1) = ol["bounds"]
    holes = drills(gdir)
    R = Raster(ol["bounds"], res=0.05)
    gl = {k: R.layer(glob.glob(os.path.join(gdir, f"*{ext}"))[0]) for k, ext in
          (("ts", ".GTS"), ("bs", ".GBS"), ("to", ".GTO"), ("bo", ".GBO"))}
    bx, by_, bw, bh = R.mm_box()
    r = ol["radius"]
    sheets = []
    s = Sheet()
    a = frame(s, fields(cfg, "4", 1, 2, "1:1"))
    views = [("BOTTOM SIDE  ·  VIEWED FROM THE BOTTOM", "b", True, 24, 46), ("TOP SIDE  ·  VIEWED FROM THE TOP", "t", False, 220, 46)]
    pnp = read_utf16_tsv(cfg["pnp_csv"])
    for title, side, mirror, vx, vy in views:
        img = colorize([(gl[side + "s"], (190, 196, 204)), (gl[side + "o"], (20, 20, 20))])
        if mirror:
            img = img.transpose(Image.FLIP_LEFT_RIGHT)
        href = png_href(img, os.path.join(work, f"place_{side}.png"))
        s.text(vx, vy - 3, title, size=3.0, weight="bold")
        s.image(vx, vy, bw, bh, href)
        ox = vx + (bx0 - bx)
        oyy = vy + (by_ - by1)
        s.rect(ox, oyy, ol["w"], ol["h"], sw=0.4, rx=r)
        for hl in holes:
            hx = hl["x"] - bx0
            if mirror:
                hx = ol["w"] - hx
            s.circle(ox + hx, oyy + (by1 - hl["y"]), hl["d"] / 2, 0.1, fill="#fff", c="#555")
        n = sum(1 for p in pnp if p["Layer"].strip('"') == side.upper())
        s.text(vx, vy + ol["h"] + 6, f"{n} parts on this side." + (" Board shown flipped left to right." if mirror else ""), size=2.3, c=GREY)
        # datum marker at the board origin (top-left in the top view)
        dxs = ox + (ol["w"] if mirror else 0)
        s.circle(dxs, oyy, 1.2, 0.25)
        s.line(dxs - 2, oyy, dxs + 2, oyy, 0.25)
        s.line(dxs, oyy - 2, dxs, oyy + 2, 0.25)
        s.text(dxs + (-2.5 if mirror else 2.5), oyy - 2.2, "DATUM 0,0", size=2.0, anchor="end" if mirror else "start")
    notes_box(s, M_IN + 4, a["tb_y"], 196, H - M_IN - a["tb_y"], cfg["place_notes"])
    sheets.append(s)

    # sheet 2: placement table
    s = Sheet()
    a = frame(s, fields(cfg, "4", 2, 2, "NTS"))
    s.text(M_IN + 6, M_IN + 9, "PLACEMENT TABLE", size=3.4, weight="bold")
    s.text(M_IN + 6, M_IN + 14, "X, Y: part centre from the datum (board top-left corner), mm, Y negative into the board. "
                                "Rotation in degrees as exported by EasyEDA; bottom-side rotation is as viewed from the top.", size=2.3, c=GREY)

    def key(p):
        d = p["Designator"].strip('"')
        m = re.match(r"([A-Z]+)(\d+)", d)
        return (m.group(1), int(m.group(2))) if m else (d, 0)
    rows = [[p["Designator"].strip('"'), p["Device"].strip('"'), p["Footprint"].strip('"').split("_")[0],
             "Top" if p["Layer"].strip('"') == "T" else "Bottom", p["Mid X"].strip('"').replace("mm", ""),
             p["Mid Y"].strip('"').replace("mm", ""), p["Rotation"].strip('"'), "SMT" if p["SMD"].strip('"') == "Yes" else "Hand"]
            for p in sorted(pnp, key=key)]
    cols = [("REF", 12, "start"), ("PART", 40, "start"), ("PKG", 26, "start"), ("SIDE", 14, "middle"),
            ("X", 17, "end"), ("Y", 17, "end"), ("ROT", 11, "end"), ("ASSY", 12, "middle")]
    per = 36
    for c, chunk in enumerate([rows[i:i + per] for i in range(0, len(rows), per)][:2]):
        s.table(M_IN + 6 + c * 200, M_IN + 20, cols, chunk, rh=4.6, size=2.2)
    sheets.append(s)
    return sheets


def doc_sch(cfg, work):
    """Each EasyEDA page is converted to SVG and its drawing area (without EasyEDA's
    title block) is placed in the frame. cfg['sch_sheets'] lists the sheets: each is a
    list of {page, crop:[x,y,w,h] in pt, title}."""
    sheets = []
    total = len(cfg["sch_sheets"])
    for n, spec in enumerate(cfg["sch_sheets"], 1):
        s = Sheet()
        a = frame(s, fields(cfg, "1", n, total, "NTS"))
        area = (M_IN + 4, a["rev_y"] + 3, W - M_IN - 4, a["tb_y"] - 3)
        cells = spec["pages"]
        if len(cells) == 1:
            boxes = [area]
        else:
            ax0, ay0, ax1, ay1 = area
            mx, my = (ax0 + ax1) / 2, (ay0 + ay1) / 2
            boxes = [(ax0, ay0, mx - 2, my - 2), (mx + 2, ay0, ax1, my - 2), (ax0, my + 2, mx - 2, ay1), (mx + 2, my + 2, ax1, ay1)]
        for (x0, y0, x1, y1), c in zip(boxes, cells):
            svgp = os.path.join(work, f"sch_p{c['page']}.svg")
            subprocess.run(["pdftocairo", "-svg", "-f", str(c["page"]), "-l", str(c["page"]), cfg["sch_pdf"], svgp],
                           check=True, stderr=subprocess.DEVNULL)
            body = open(svgp, encoding="utf-8").read()
            body = body[body.index(">", body.index("<svg")) + 1: body.rindex("</svg>")]
            pre = f"p{c['page']}-"
            body = re.sub(r'id="([^"]+)"', lambda m: f'id="{pre}{m.group(1)}"', body)
            body = re.sub(r'(xlink:href|href)="#([^"]+)"', lambda m: f'{m.group(1)}="#{pre}{m.group(2)}"', body)
            body = re.sub(r"url\(#([^)]+)\)", lambda m: f"url(#{pre}{m.group(1)})", body)
            cx, cy, cw, ch = c["crop"]
            if c.get("title"):
                s.text(x0, y0 + 3.2, c["title"], size=3.0, weight="bold")
                y0 += 5
            s.add(f'<svg x="{x0:.2f}" y="{y0:.2f}" width="{x1 - x0:.2f}" height="{y1 - y0:.2f}" '
                  f'viewBox="{cx} {cy} {cw} {ch}" preserveAspectRatio="xMidYMid meet">{body}</svg>')
        sheets.append(s)
    return sheets


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("config")
    ap.add_argument("--out", required=True)
    ap.add_argument("--docs", default="1,2,3,4")
    a = ap.parse_args()
    cdir = os.path.dirname(os.path.abspath(a.config))
    cfg = json.load(open(a.config, encoding="utf-8"))
    for k in ("sch_pdf", "gerber_dir", "bom_csv", "pnp_csv"):
        if k in cfg and not os.path.isabs(cfg[k]):
            cfg[k] = os.path.normpath(os.path.join(cdir, cfg[k]))
    os.makedirs(a.out, exist_ok=True)
    builders = {"1": doc_sch, "2": doc_pwb, "3": doc_bom, "4": doc_place}
    for d in a.docs.split(","):
        work = tempfile.mkdtemp(prefix=f"kc_{d}_", dir=a.out)
        sheets = builders[d](cfg, work)
        f = {"dwg_no": cfg["base"] + d, "rev": cfg.get("rev", "-")}
        out = os.path.join(a.out, file_name(f, cfg["name"]))
        print_pdf(sheets, out, work)
        print("wrote", out, f"({len(sheets)} sheet{'s' if len(sheets) > 1 else ''})")


if __name__ == "__main__":
    main()
