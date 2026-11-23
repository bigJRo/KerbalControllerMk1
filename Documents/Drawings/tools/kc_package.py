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
from kc_frame import (DOC_TYPES, GREY, HI, H, INK, M_IN, PAPER, W, Sheet, esc, file_name,  # noqa: E402
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

_LAYER_N = [0]


def layer_svg(path, bounds, fg, bg="white", pad=1.0):
    """A Gerber layer as an inline SVG fragment (vector), drawn by gerbonara.
    Returns (svg_body_with_unique_ids, (x0, y_top, width, height)) in board mm, y up."""
    from gerbonara import GerberFile
    from gerbonara.utils import MM
    (x0, y0), (x1, y1) = bounds
    fb = ((x0 - pad, y0 - pad), (x1 + pad, y1 + pad))
    svg = str(GerberFile.open(path).to_svg(force_bounds=fb, fg=fg, bg=bg, arg_unit=MM, svg_unit=MM))
    _LAYER_N[0] += 1
    pre = f"g{_LAYER_N[0]}-"
    body = svg[svg.index(">", svg.index("<svg")) + 1: svg.rindex("</svg>")]
    body = re.sub(r'id="([^"]+)"', lambda m: f'id="{pre}{m.group(1)}"', body)
    body = re.sub(r'(xlink:href|href)="#([^"]+)"', lambda m: f'{m.group(1)}="#{pre}{m.group(2)}"', body)
    body = re.sub(r"url\(#([^)]+)\)", lambda m: f"url(#{pre}{m.group(1)})", body)
    vb = re.search(r'viewBox="([^"]+)"', svg).group(1)
    return body, vb, (x0 - pad, y1 + pad, x1 - x0 + 2 * pad, y1 - y0 + 2 * pad)


def place_layer(s, path, bounds, x, y, k, fg, bg="white", mirror=False):
    """Draw a Gerber layer on the sheet with its window's top-left at (x, y), scale k."""
    body, vb, (wx, wy, ww, wh) = layer_svg(path, bounds, fg, bg)
    w, h = ww * k, wh * k
    inner = f'<svg x="0" y="0" width="{w:.3f}" height="{h:.3f}" viewBox="{vb}" overflow="hidden">{body}</svg>'
    if mirror:
        s.add(f'<g transform="translate({x + w:.3f} {y:.3f}) scale(-1 1)">{inner}</g>')
    else:
        s.add(f'<g transform="translate({x:.3f} {y:.3f})">{inner}</g>')
    return wx, wy, ww, wh

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
                d = round(round(o.tool.diameter * 20) / 20, 2)
                holes.append({"x": o.x, "y": o.y, "d": d, "dt": o.tool.diameter,
                              "plated": o.tool.plated is not False, "via": via})
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
                doc_type=DOC_TYPES[d][1], status=cfg.get("doc_status", {}).get(d, cfg.get("status", "DRAFT")), created_by=cfg["created_by"],
                approved_by=cfg["approved_by"], date=cfg["date"], board_version=cfg["board_version"],
                scale=scale, sheet=sheet, sheets=sheets, revisions=cfg.get("revisions"))


PWB_K = 1.5             # sheet 1 board view scale (3:2)
LAYER_K = 1.75          # copper layer sheets scale (7:4), one layer per sheet
LAYER_COLOURS = ["#c0392b", "#d68910", "#1e8449", "#2e5fa8"]  # top red, inner 1 amber, inner 2 green, bottom blue


def _ratio(k):
    from fractions import Fraction
    f = Fraction(k).limit_denominator(8)
    return f"{f.numerator}:{f.denominator}"


def doc_pwb(cfg, work):
    gdir = cfg["gerber_dir"]
    ol = outline_info(gdir)
    holes = drills(gdir)
    (bx0, by0), (bx1, by1) = ol["bounds"]
    layers = cfg["layers"]
    total = 1 + len(layers)
    sheets = []

    # ---- sheet 1: outline, holes, drill chart, stackup, notes
    k = cfg.get("pwb_scale", PWB_K)
    s = Sheet()
    a = frame(s, fields(cfg, "2", 1, total, _ratio(k)))
    r = ol["radius"]
    bw, bh = ol["w"] * k, ol["h"] * k
    ox = a["x0"] + 16.0                        # board top-left on the sheet
    oy = a["rev_y"] + 21.0

    def P(x, y):                               # board mm (y up) -> sheet mm
        return ox + (x - bx0) * k, oy + (by1 - y) * k
    s.text(ox - 10, a["y0"] + 8, "BOARD OUTLINE AND HOLES (TOP VIEW)", size=3.4, weight="bold")
    s.text(ox - 10, a["y0"] + 13, f"Scale {_ratio(k)} at {PAPER}. Datum: board corner at top left. All holes are drilled "
           f"from the top. Corners R{r:.2f} (4 places). Outline tolerance ±0.2.", size=2.3, c=GREY)
    s.rect(ox, oy, bw, bh, sw=0.5, rx=r * k)
    tools = sorted({(h["d"], h["plated"]) for h in holes})
    sym = {t: SYMS[n % len(SYMS)] for n, t in enumerate(tools)}
    for hl in holes:
        x, y = P(hl["x"], hl["y"])
        if hl["d"] >= 2.0:
            s.circle(x, y, hl["d"] / 2 * k, 0.2)
        symbol(s, sym[(hl["d"], hl["plated"])], x, y, r=0.75 if hl["d"] < 0.5 else 1.2, sw=0.18)
    s.circle(ox, oy, 1.6, 0.25)
    s.line(ox - 3, oy, ox + 3, oy, 0.25)
    s.line(ox, oy - 3, ox, oy + 3, 0.25)
    s.text(ox - 2.4, oy - 3.0, "DATUM 0,0", size=2.2, anchor="end")
    dim_h(s, ox, ox + bw, oy - 6, oy - 7.2, f"{ol['w']:.2f}", ext_from=oy)
    dim_v(s, oy, oy + bh, ox - 7, f"{ol['h']:.2f}", ext_from=ox)
    big = max(h["d"] for h in holes)
    mh = sorted([h for h in holes if h["d"] == big], key=lambda h: (-h["y"], h["x"]))
    if mh:
        hx, hy = P(mh[0]["x"], mh[0]["y"])
        dx, dy = mh[0]["x"] - bx0, by1 - mh[0]["y"]
        s.line(hx + 1.5, hy + 1.2, hx + 16, hy + 11, 0.15)
        s.text(hx + 16.5, hy + 11.8, f"{len(mh)}× Ø{big:.2f} {'PTH' if mh[0]['plated'] else 'NPTH'}"
               + (f", {cfg['mount_hole_note']}" if cfg.get("mount_hole_note") else ""), size=2.6)
        s.text(hx + 16.5, hy + 15.2, f"{dx:.2f} from the side edges, {dy:.2f} from the top and bottom edges",
               size=2.3, c=GREY)
        xs = sorted({round(h["x"], 2) for h in mh})
        ys = sorted({round(h["y"], 2) for h in mh})
        if len(xs) > 1:
            ax_, _ = P(xs[0], 0)
            bx_, _ = P(xs[-1], 0)
            dim_h(s, ax_, bx_, oy + bh + 7, oy + bh + 11.2, f"{xs[-1] - xs[0]:.2f}", ext_from=oy + bh - 4)
        if len(ys) > 1:
            _, ay_ = P(0, ys[-1])
            _, by_ = P(0, ys[0])
            dim_v(s, ay_, by_, ox + bw + 7, f"{ys[-1] - ys[0]:.2f}", ext_from=ox + bw - 4)

    # drill chart and stackup, right of the board
    tx = ox + bw + 16
    tw = a["x1"] - 2 - tx
    ty = a["rev_y"] + 9
    s.text(tx, ty - 2, "DRILL CHART", size=3.0, weight="bold")
    rows = []
    for t in tools:
        n = sum(1 for h in holes if (h["d"], h["plated"]) == t)
        vias = sum(1 for h in holes if (h["d"], h["plated"]) == t and h["via"])
        kind = "Via" if vias == n else ("Component / via" if vias else ("Mounting" if t[0] == big else "Component"))
        rows.append(["", f"{t[0]:.2f}", "PTH" if t[1] else "NPTH", str(n), kind])
    f = tw / 90
    cols = [("SYM", 9 * f, "middle"), ("DIA (mm)", 15 * f, "middle"), ("PLATING", 15 * f, "middle"),
            ("QTY", 11 * f, "middle"), ("USE", 40 * f, "start")]
    yb = s.table(tx, ty, cols, rows, rh=5.0)
    for n, t in enumerate(tools):
        symbol(s, sym[t], tx + 4.5 * f, ty + 5.2 + 5.0 * n + 2.5, r=1.1, sw=0.2)
    s.text(tx, yb + 4, f"Total holes: {len(holes)}.  Finished hole sizes; tolerance ±0.08.", size=2.2, c=GREY)
    sy = yb + 13
    s.text(tx, sy - 2, "LAYER STACKUP", size=3.0, weight="bold")
    srows = [[l["layer"], l["name"], l["material"], l["thickness"], l.get("use", "")] for l in cfg["stackup"]]
    scols = [("No.", 8 * f, "middle"), ("NAME", 18 * f, "start"), ("MATERIAL", 21 * f, "start"),
             ("THK (mm)", 13 * f, "middle"), ("USE", 30 * f, "start")]
    yb2 = s.table(tx, sy, scols, srows, rh=4.6, size=2.2, wrap=wrap_text)
    for n, part in enumerate(wrap_text(cfg.get("stackup_note", ""), tw, 2.2)):
        s.text(tx, yb2 + 4 + 3 * n, part, size=2.2, c=GREY)

    notes_box(s, a["x0"] + 2, a["tb_y"], a["tb_x"] - a["x0"] - 6, a["y1"] - a["tb_y"] - 2, cfg["fab_notes"])
    sheets.append(s)

    # ---- sheets 2..: one copper layer per sheet
    k = cfg.get("layer_scale", LAYER_K)
    for n, lay in enumerate(layers, 2):
        s = Sheet()
        a = frame(s, fields(cfg, "2", n, total, _ratio(k)))
        pad = 1.0
        ww, wh = (ol["w"] + 2 * pad) * k, (ol["h"] + 2 * pad) * k
        lx = a["x0"] + (a["x1"] - a["x0"] - ww) / 2
        ly = a["rev_y"] + 2 + ((a["tb_y"] - 2) - (a["rev_y"] + 2) - wh) / 2
        s.text(a["x0"] + 6, a["y0"] + 8, f"COPPER LAYER {lay['layer']} OF {len(layers)}  ·  {lay['name'].upper()}  "
               f"(TOP VIEW, LOOKING DOWN THROUGH THE BOARD)", size=3.4, weight="bold")
        colour = lay.get("color", LAYER_COLOURS[(n - 2) % len(LAYER_COLOURS)])
        s.rect(a["x0"] + 6, a["y0"] + 10.6, 6, 3.2, sw=0.2, fill=colour)
        s.text(a["x0"] + 14, a["y0"] + 13, f"Scale {_ratio(k)} at {PAPER}. Layer {lay['layer']} copper is shown in "
               f"this colour; clearances, anti-pads and gaps are white. {lay.get('net', '')}.", size=2.3, c=GREY)
        bx, by_, _, _ = place_layer(s, os.path.join(gdir, lay["file"]), ol["bounds"], lx, ly, k, colour)
        s.rect(lx + (bx0 - bx) * k, ly + (by_ - by1) * k, ol["w"] * k, ol["h"] * k, sw=0.35, rx=r * k)
        # drilled holes on top of the copper: white = no material. Plated: solid outline; NPTH: dashed.
        for hl in holes:
            hx, hy = lx + (hl["x"] - bx) * k, ly + (by_ - hl["y"]) * k
            dash = "" if hl["plated"] else ' stroke-dasharray="0.6 0.4"'
            s.add(f'<circle cx="{hx:.3f}" cy="{hy:.3f}" r="{hl["dt"] / 2 * k:.3f}" fill="white" '
                  f'stroke="{INK}" stroke-width="0.1"{dash}/>')
        has_npth = any(not h["plated"] for h in holes)
        notes = [f"1. Layer {lay['layer']} of {len(layers)}, {lay['name']}, {lay.get('copper', '')} copper: "
                 f"{lay.get('net', '')}."]
        notes += [f"{i}. {t}" for i, t in enumerate(lay.get("notes", []), 2)]
        notes.append(f"{len(notes) + 1}. Drilled holes are drawn white over the copper, from the drill files: "
                     + ("solid outline = plated, dashed outline = non-plated." if has_npth
                        else f"all {len(holes)} holes are plated (solid outline)."))
        notes_box(s, a["x0"] + 2, a["tb_y"], a["tb_x"] - a["x0"] - 6, a["y1"] - a["tb_y"] - 2, notes,
                  title="LAYER NOTES")
        sheets.append(s)
    return sheets


def _parts_library(cfg):
    lib = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "parts.json"), encoding="utf-8"))
    lib.update(cfg.get("bom_parts", {}))      # board overrides, keyed by LCSC number or first designator
    return lib


def doc_bom(cfg, work):
    """Bill of materials. Descriptions, packages and values come from parts.json (keyed by LCSC
    number) where present, so each line states its ratings instead of a supplier category."""
    rows_in = read_utf16_tsv(cfg["bom_csv"])
    pnp = {r["Designator"].strip('"'): r for r in read_utf16_tsv(cfg["pnp_csv"])}
    lib = _parts_library(cfg)
    rows = []
    for k, r in enumerate(rows_in, 1):
        des = r["Designator"]
        first = des.split(",")[0]
        lcsc = r.get("Supplier Part", "")
        p = lib.get(lcsc) or lib.get(first) or {}
        smd = pnp.get(first, {}).get("SMD", "").strip('"')
        side = pnp.get(first, {}).get("Layer", "").strip('"')
        asm = ("SMT" if smd == "Yes" else "THT") + (f" ({'top' if side == 'T' else 'bottom'})" if side else "")
        later = set(cfg.get("after_delivery", []))
        by = "After delivery" if any(d.strip() in later for d in des.split(",")) else cfg.get("assembler", "JLCPCB")
        desc = p.get("desc") or r.get("Secondary Category") or r.get("Primary Category", "")
        mpn = p.get("mpn") or re.sub(r"[^\x00-\x7f]+", "", r.get("Manufacturer Part", "")).strip() or "-"
        mfr = p.get("mfr") or r.get("Manufacturer", "").split("(")[0] or "-"
        pkg = p.get("pkg") or r.get("Footprint", "").split("_")[0]
        val = re.sub(r"^([\d.]+)\s*([pnuµkM]?)(F|Ω|H)$", lambda m: f"{m.group(1)} {m.group(2).replace('u', 'µ')}{m.group(3)}",
                     r.get("Value", "")) or p.get("value", "") or "-"
        rows.append([str(k), r["Quantity"], des.replace(",", ", "), val, pkg, desc, mfr, mpn,
                     lcsc or p.get("lcsc", "-"), (r.get("JLCPCB Part Class", "") or "-").replace(" Part", ""), asm, by])
    s = Sheet()
    a = frame(s, fields(cfg, "3", 1, 1, "NTS"))
    x = a["x0"] + 2
    s.text(x, a["y0"] + 8, "BILL OF MATERIALS  ·  PRINTED CIRCUIT CARD ASSEMBLY", size=3.4, weight="bold")
    s.text(x, a["y0"] + 13, f"Board {cfg['base']}2 {cfg['board_version']}. Quantities per assembled card. "
           f"{len(rows)} line items, {sum(int(r[1]) for r in rows)} parts.", size=2.3, c=GREY)
    base = [("ITEM", 10, "middle"), ("QTY", 9, "middle"), ("DESIGNATORS", 62, "start"), ("VALUE", 16, "start"),
            ("PACKAGE", 21, "start"), ("DESCRIPTION", 86, "start"), ("MANUFACTURER", 27, "start"),
            ("MFR PART No.", 44, "start"), ("LCSC", 18, "start"), ("CLASS", 16, "middle"), ("PROCESS", 22, "start"),
            ("INSTALLED BY", 24, "start")]
    tw = a["x1"] - 2 - x
    f = tw / sum(c[1] for c in base)
    cols = [(t, w * f, al) for t, w, al in base]
    size, rh = 2.75, 6.6
    yb = s.table(x, a["rev_y"] + 6, cols, rows, rh=rh, hh=6.4, size=size, hsize=2.5, wrap=wrap_text)
    if yb > a["tb_y"] - 2:
        raise SystemExit(f"BOM table runs into the title block ({yb:.1f} > {a['tb_y'] - 2:.1f}); reduce rh")
    notes_box(s, a["x0"] + 2, a["tb_y"], a["tb_x"] - a["x0"] - 6, a["y1"] - a["tb_y"] - 2, cfg["bom_notes"])
    return [s]


def doc_place(cfg, work):
    gdir = cfg["gerber_dir"]
    ol = outline_info(gdir)
    (bx0, by0), (bx1, by1) = ol["bounds"]
    holes = drills(gdir)
    r = ol["radius"]
    sheets = []
    s = Sheet()
    a = frame(s, fields(cfg, "4", 1, 2, "1:1"))
    views = [("BOTTOM SIDE  ·  VIEWED FROM THE BOTTOM", "b", True, 24, 46), ("TOP SIDE  ·  VIEWED FROM THE TOP", "t", False, 220, 46)]
    pnp = read_utf16_tsv(cfg["pnp_csv"])
    for title, side, mirror, vx, vy in views:
        mask = glob.glob(os.path.join(gdir, "*.GTS" if side == "t" else "*.GBS"))[0]
        silk = glob.glob(os.path.join(gdir, "*.GTO" if side == "t" else "*.GBO"))[0]
        bx, by_, bw, bh = place_layer(s, mask, ol["bounds"], vx, vy, 1.0, "#bec4cc", mirror=mirror)
        place_layer(s, silk, ol["bounds"], vx, vy, 1.0, "#141414", bg="white", mirror=mirror)
        s.text(vx, vy - 3, title, size=3.0, weight="bold")
        ox = vx + (bx0 - bx)
        oyy = vy + (by_ - by1)
        s.rect(ox, oyy, ol["w"], ol["h"], sw=0.4, rx=r)
        for hl in holes:
            hx = hl["x"] - bx0
            if mirror:
                hx = ol["w"] - hx
            s.circle(ox + hx, oyy + (by1 - hl["y"]), hl["d"] / 2, 0.1, fill="#fff", c="#555")
        on_side = [p for p in pnp if p["Layer"].strip('"') == side.upper()]
        for p in on_side:
            px_ = float(p["Mid X"].strip('"').replace("mm", "")) - bx0
            py_ = by1 - float(p["Mid Y"].strip('"').replace("mm", ""))
            if mirror:
                px_ = ol["w"] - px_
            s.text(ox + px_, oyy + py_ + 0.6, p["Designator"].strip('"'), size=1.7, anchor="middle", weight="bold", c="#1f4e9a")
        n = len(on_side)
        s.text(vx, vy + ol["h"] + 6, f"{n} part{'s' if n != 1 else ''} on this side. Reference designators in blue."
               + (" Board shown flipped left to right." if mirror else ""), size=2.3, c=GREY)
        # datum marker at the board origin (top-left in the top view)
        dxs = ox + (ol["w"] if mirror else 0)
        s.circle(dxs, oyy, 1.2, 0.25)
        s.line(dxs - 2, oyy, dxs + 2, oyy, 0.25)
        s.line(dxs, oyy - 2, dxs, oyy + 2, 0.25)
        s.text(dxs - 2.5, oyy - 2.6, "DATUM 0,0", size=2.0, anchor="end")
    notes_box(s, M_IN + 4, a["tb_y"], 196, H - M_IN - a["tb_y"], cfg["place_notes"])
    sheets.append(s)

    # sheet 2: placement table
    s = Sheet()
    a = frame(s, fields(cfg, "4", 2, 2, "NTS"))
    s.text(M_IN + 6, M_IN + 9, "PLACEMENT TABLE", size=3.4, weight="bold")
    s.text(M_IN + 6, M_IN + 14, "X, Y: part centre from the datum (board top-left corner, top view), mm; Y is negative into the board.", size=2.3, c=GREY)
    s.text(M_IN + 6, M_IN + 18, "Rotation in degrees as exported by EasyEDA Pro (machine-ready pick-and-place file of the same revision).", size=2.3, c=GREY)

    def key(p):
        d = p["Designator"].strip('"')
        m = re.match(r"([A-Z]+)(\d+)", d)
        return (m.group(1), int(m.group(2))) if m else (d, 0)
    rows = [[p["Designator"].strip('"'), re.sub(r"[^\x00-\x7f]+", "", p["Device"].strip('"')), p["Footprint"].strip('"').split("_")[0],
             "Top" if p["Layer"].strip('"') == "T" else "Bottom", p["Mid X"].strip('"').replace("mm", ""),
             p["Mid Y"].strip('"').replace("mm", ""), p["Rotation"].strip('"'), "SMT" if p["SMD"].strip('"') == "Yes" else "Hand"]
            for p in sorted(pnp, key=key)]
    cols = [("REF", 12, "start"), ("PART", 40, "start"), ("PKG", 26, "start"), ("SIDE", 14, "middle"),
            ("X", 17, "end"), ("Y", 17, "end"), ("ROT", 11, "end"), ("ASSY", 12, "middle")]
    top = a["rev_y"] + 5
    per = int((a["tb_y"] - 6 - top - 5.2) / 4.6)
    for c, chunk in enumerate([rows[i:i + per] for i in range(0, len(rows), per)][:2]):
        s.table(M_IN + 6 + c * 200, top, cols, chunk, rh=4.6, size=2.2)
    sheets.append(s)
    return sheets


SCH_K = 0.50            # mm per schematic pt (config "sch_scale"): one scale on every sheet
SCH_BAND = 5.0          # mm label band at the bottom of each block
SCH_GAP = 2.0           # mm clearance to the frame, revision block and title block
SCH_DASH = (2.5, 1.5)   # separator dash pattern, mm
SCH_LABEL = "#a10000"   # label colour (EasyEDA symbol red)


def _sch_page_body(cfg, page, work, cache={}):
    """EasyEDA page as SVG, with EasyEDA's heavy black block borders removed."""
    if page in cache:
        return cache[page]
    svgp = os.path.join(work, f"sch_p{page}.svg")
    subprocess.run(["pdftocairo", "-svg", "-f", str(page), "-l", str(page), cfg["sch_pdf"], svgp],
                   check=True, stderr=subprocess.DEVNULL)
    svg = open(svgp, encoding="utf-8").read()
    body = svg[svg.index(">", svg.index("<svg")) + 1: svg.rindex("</svg>")]
    body = re.sub(r'<path(?=[^>]*stroke="rgb\(0%, 0%, 0%\)")(?=[^>]*stroke-width="1\.44")[^>]*/>', "", body)
    pre = f"p{page}-"
    body = re.sub(r'id="([^"]+)"', lambda m: f'id="{pre}{m.group(1)}"', body)
    body = re.sub(r'(xlink:href|href)="#([^"]+)"', lambda m: f'{m.group(1)}="#{pre}{m.group(2)}"', body)
    body = re.sub(r"url\(#([^)]+)\)", lambda m: f"url(#{pre}{m.group(1)})", body)
    cache[page] = body
    return body


def _dashed_segments(s, boxes):
    """Draw the edges of all boxes once each: shared edges are merged, and the dash
    pattern is phased to absolute position so collinear runs line up."""
    runs = {}
    for x0, y0, x1, y1 in boxes:
        for key, a, b in ((("h", round(y0, 2)), x0, x1), (("h", round(y1, 2)), x0, x1),
                          (("v", round(x0, 2)), y0, y1), (("v", round(x1, 2)), y0, y1)):
            runs.setdefault(key, []).append((a, b))
    period = sum(SCH_DASH)
    for (axis, c), segs in runs.items():
        segs.sort()
        merged = [list(segs[0])]
        for a, b in segs[1:]:
            if a <= merged[-1][1] + 1e-6:
                merged[-1][1] = max(merged[-1][1], b)
            else:
                merged.append([a, b])
        for a, b in merged:
            x1, y1, x2, y2 = (a, c, b, c) if axis == "h" else (c, a, c, b)
            s.add(f'<line x1="{x1:.3f}" y1="{y1:.3f}" x2="{x2:.3f}" y2="{y2:.3f}" stroke="#7a7a7a" '
                  f'stroke-width="0.25" stroke-dasharray="{SCH_DASH[0]} {SCH_DASH[1]}" '
                  f'stroke-dashoffset="{a % period:.3f}"/>')


def doc_sch(cfg, work):
    """Schematic sheets built from blocks cut out of the EasyEDA pages.

    Each sheet is {"rows": [row, ...]}; a row is a list of columns and a column is a list of
    blocks stacked top to bottom. Block: {page, crop:[x0,y0,x1,y1] in page pt (top-down), label}.
    Every block is drawn at one scale (sch_scale, mm per pt) on every sheet. Rows are stretched
    to the full drawing width, columns to the row height, and each circuit is centred in its
    box with its label at the bottom left. The block group is centred vertically between the
    revision block and the title block."""
    k = cfg.get("sch_scale", SCH_K)
    sheets = []
    total = len(cfg["sch_sheets"])
    for n, spec in enumerate(cfg["sch_sheets"], 1):
        s = Sheet()
        a = frame(s, fields(cfg, "1", n, total, "NTS"))
        ax0, ax1 = a["x0"] + SCH_GAP, a["x1"] - SCH_GAP
        ay0, ay1 = a["rev_y"] + SCH_GAP, a["tb_y"] - SCH_GAP

        def nat(b):
            x0, y0, x1, y1 = b["crop"]
            return (x1 - x0) * k, (y1 - y0) * k + SCH_BAND
        rows = []
        for row in spec["rows"]:
            cols = [(max(nat(b)[0] for b in col), sum(nat(b)[1] for b in col)) for col in row]
            rows.append((row, cols, max(h for _, h in cols)))
        total_h = sum(r[2] for r in rows)
        if total_h > ay1 - ay0 + 1e-6:
            raise SystemExit(f"SCH sheet {n}: blocks need {total_h:.1f} mm of height, "
                             f"{ay1 - ay0:.1f} mm available; lower sch_scale")
        y = ay0 + ((ay1 - ay0) - total_h) / 2
        boxes = []
        pages = sorted({b["page"] for row in spec["rows"] for col in row for b in col})
        defs = "".join(f'<g id="sch-page-{pg}">{_sch_page_body(cfg, pg, work)}</g>' for pg in pages)
        s.add(f"<defs>{defs}</defs>")
        for row, cols, rh in rows:
            nat_w = sum(w for w, _ in cols)
            if nat_w > ax1 - ax0 + 1e-6:
                raise SystemExit(f"SCH sheet {n}: a row needs {nat_w:.1f} mm of width, "
                                 f"{ax1 - ax0:.1f} mm available; lower sch_scale")
            x = ax0
            for col, (cw, ch) in zip(row, cols):
                cw *= (ax1 - ax0) / nat_w            # stretch columns to the full width
                by = y
                for b in col:
                    bw, bh = nat(b)
                    bh *= rh / ch                    # stretch blocks to the row height
                    boxes.append((x, by, x + cw, by + bh))
                    x0, y0, x1, y1 = b["crop"]
                    w, h = (x1 - x0) * k, (y1 - y0) * k
                    cx, cy = x + (cw - w) / 2, by + (bh - SCH_BAND - h) / 2
                    s.add(f'<svg x="{cx:.3f}" y="{cy:.3f}" width="{w:.3f}" height="{h:.3f}" '
                          f'viewBox="{x0} {y0} {x1 - x0} {y1 - y0}" overflow="hidden">'
                          f'<use href="#sch-page-{b["page"]}" xlink:href="#sch-page-{b["page"]}"/></svg>')
                    s.text(x + 2.0, by + bh - 1.7, b["label"], size=3.0, c=SCH_LABEL)
                    by += bh
                x += cw
            y += rh
        _dashed_segments(s, boxes)
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
