#!/usr/bin/env python3
"""
kc_strays.py -- find stray parts in the Fusion views of a drawing package and write white-out
masks for them into the package config.

Fusion drawings of the panel assemblies carry loose single parts (label inserts and the like)
that float next to the views. In each view the assembly itself is one connected drawing:
every line of it is within a short gap of another. A stray part is ink that is neither joined
to that drawing nor inside its outline (holes and engraving inside the panel are enclosed by
the panel outline, so they are kept).

    python3 kc_strays.py CONFIG [--doc 0] [--gap 1.0] [--write]

A balloon view also needs "body": [x0, y0, x1, y1], the part's outline box, because the leaders
close loops through the part that would otherwise count as its outline.

Strays too close to the drawing to separate can be listed by hand in "extra_masks"; they are
added to the masks found.

For every view with "auto_masks": true it prints the masks found; with --write it stores them
in the view's "masks" (replacing what was there) so they stay explicit and reviewable in the
config. Coordinates are source-page mm, as the generator expects.
"""
import argparse
import glob
import json
import os
import subprocess
import tempfile
from collections import OrderedDict

import numpy as np
from PIL import Image
from scipy import ndimage

DPI = 200
S = DPI / 25.4          # px per mm


def page_ink(pdf, page, work):
    out = os.path.join(work, f"p{page}")
    subprocess.run(["pdftoppm", "-r", str(DPI), "-gray", "-png", "-f", str(page), "-l", str(page), pdf, out],
                   check=True, capture_output=True)
    png = glob.glob(out + "*.png")[0]
    return np.asarray(Image.open(png).convert("L")) < 230


def disk(r):
    y, x = np.ogrid[-r:r + 1, -r:r + 1]
    return x * x + y * y <= r * r


def strays(ink, src, gap_mm, body=None, pad_mm=0.4, min_px=4):
    x0, y0, x1, y1 = src
    c = ink[int(y0 * S):int(y1 * S), int(x0 * S):int(x1 * S)]
    joined = ndimage.binary_dilation(c, structure=disk(max(1, int(gap_mm * S / 2))))
    lab, n = ndimage.label(joined)
    if n == 0:
        return [], []
    objs = ndimage.find_objects(lab)
    # the assembly is the drawing that spans the most area (its outline encloses the rest),
    # not necessarily the one with the most ink
    spans = [(o[0].stop - o[0].start) * (o[1].stop - o[1].start) for o in objs]
    main_k = int(np.argmax(spans)) + 1
    main = lab == main_k
    if body:
        # Balloon views: leaders and balloons close loops through the part, so the plain outline
        # would enclose open paper. Keep ink inside the part's body box, plus whatever a closed
        # shape outside the body encloses (balloon numbers).
        bm = np.zeros_like(main)
        bm[max(0, int((body[1] - y0) * S)):int((body[3] - y0) * S), max(0, int((body[0] - x0) * S)):int((body[2] - x0) * S)] = True
        hull = bm | ndimage.binary_fill_holes(main & ~bm)
    else:
        hull = ndimage.binary_fill_holes(main)
    keep_ink = c & main
    masks, skipped = [], []
    for k, sl in enumerate(objs, 1):
        if k == main_k:
            continue
        blob = (lab[sl] == k) & c[sl]
        if blob.sum() < min_px:
            continue
        ys, xs = np.nonzero(blob)
        by0, bx0 = sl[0].start + ys.min(), sl[1].start + xs.min()
        by1, bx1 = sl[0].start + ys.max() + 1, sl[1].start + xs.max() + 1
        if hull[by0:by1, bx0:bx1].all():          # inside the assembly outline: real (hole, engraving)
            continue
        pad = int(pad_mm * S)
        while pad >= 0:
            Y0, X0 = max(0, by0 - pad), max(0, bx0 - pad)
            Y1, X1 = min(c.shape[0], by1 + pad), min(c.shape[1], bx1 + pad)
            if not keep_ink[Y0:Y1, X0:X1].any():
                break
            pad -= 1
        # a stray cut by the crop edge: run the mask 0.5 mm past the edge so no anti-aliased sliver is left
        e = 0.5
        box = [round(float(x0 + X0 / S - (e if X0 == 0 else 0)), 2), round(float(y0 + Y0 / S - (e if Y0 == 0 else 0)), 2),
               round(float(x0 + X1 / S + (e if X1 == c.shape[1] else 0)), 2), round(float(y0 + Y1 / S + (e if Y1 == c.shape[0] else 0)), 2)]
        (masks if pad >= 0 else skipped).append(box)
    return masks, skipped


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("config")
    ap.add_argument("--doc", default=None, help="only this document (e.g. 0)")
    ap.add_argument("--gap", type=float, default=1.0, help="largest gap (mm) inside one drawing")
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()
    cdir = os.path.dirname(os.path.abspath(a.config))
    cfg = json.load(open(a.config, encoding="utf-8"), object_pairs_hook=OrderedDict)
    work = tempfile.mkdtemp(prefix="kc_strays_")
    for d, vd in (cfg.get("view_docs") or {}).items():
        if a.doc is not None and d != a.doc:
            continue
        cache = {}
        for si, sh in enumerate(vd["sheets"], 1):
            for v in sh["views"]:
                if not v.get("auto_masks"):
                    continue
                pdf = os.path.join(cdir, v.get("pdf", vd["pdf"]))
                key = (pdf, v["page"])
                if key not in cache:
                    cache[key] = page_ink(pdf, v["page"], work)
                masks, skipped = strays(cache[key], v["src"], v.get("gap", a.gap), v.get("body"))
                print(f"doc {d} sheet {si} {v.get('label', 'view')!r}: {len(masks)} masks"
                      + (f", {len(skipped)} strays touching the drawing left alone" if skipped else ""))
                for b in skipped:
                    print("   left alone:", b)
                if a.write:
                    # "extra_masks" are hand-placed (strays too close to the drawing to separate)
                    v["masks"] = masks + v.get("extra_masks", [])
    if a.write:
        json.dump(cfg, open(a.config, "w", encoding="utf-8"), indent=2, ensure_ascii=False)
        open(a.config, "a").write("\n")


if __name__ == "__main__":
    main()
