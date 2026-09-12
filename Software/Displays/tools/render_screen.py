#!/usr/bin/env python3
"""
render_screen.py -- render a KCMk1 display panel's screens on the host, as PNGs,
by running the real firmware drawing code against a framebuffer.

host_compile.py already reproduces the Arduino IDE's build of a panel sketch on
the host (tab concatenation, hoisted prototypes, the in-repo libraries on the
include path) but only to check syntax: its display stub draws nothing. This tool
does the same build, LINKS it -- the sketch, KerbalDisplayCommon, KCM_Touch and
KerbalDisplayAudio -- against the stubs in host_stubs_fb/, whose RA8876_t41_p
keeps a 1024x600 RGB565 framebuffer and whose SD reads the real art from
../assets, then runs a scenario that sets the panel's AppState and calls the
firmware's own loop(). What comes out is what the panel draws for that state:
same layout, same fonts (the ILI9341_t3 glyph renderer is reproduced in the stub
for the text drawButton() prints through), same colours, same BMPs, same C&W
logic. It exists to make user-manual graphics that are true renderings rather
than mockups.

Scenarios live in render_scenarios/<sketch>.inc, appended to the concatenated
sketch so they can reach every tab's globals; see the Annunciator one for the
shape. Each takes a scenario name and writes a PPM the tool converts to PNG.

Usage:
    python3 tools/render_screen.py                       # every Annunciator scenario
    python3 tools/render_screen.py MainOrbit SOI         # just these
    python3 tools/render_screen.py --sketch KCMk1_InfoDisp ...   (once it has a scenario file)
    python3 tools/render_screen.py --out /tmp/shots ...  # default: Documents/User/assets
    python3 tools/render_screen.py --keep                # leave the build dir for inspection

Output: <out>/<Panel>_<Scenario>.png (e.g. Annunciator_MainOrbit.png). Plain
Python 3 + g++, no third-party modules.
"""

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

import host_compile as hc

HERE      = os.path.dirname(os.path.abspath(__file__))
STUBS_FB  = os.path.join(HERE, "host_stubs_fb")
SCENARIOS = os.path.join(HERE, "render_scenarios")
ASSETS    = os.path.join(hc.DISPLAYS, "assets")
REPO      = os.path.dirname(os.path.dirname(hc.DISPLAYS))
DEFAULT_OUT = os.path.join(REPO, "Documents", "User", "assets")

# Library translation units that must be linked, not just parsed.
LIB_CPPS = [
    "KerbalDisplayCommon/src/KerbalDisplayCommon.cpp",
    "KCM_Touch/src/KCM_Touch.cpp",
    "KerbalDisplayAudio/src/KerbalDisplayAudio.cpp",
    "KerbalDisplayAudio/src/KCM_DFPlayer.cpp",
]

# The scenario names each scenario file accepts (its argv[1] switch).
SCENARIO_NAMES = {
    "KCMk1_Annunciator": ["MainOrbit", "MainReentry", "MainLampTest", "SOI", "Standby"],
    "KCMk1_InfoDisp": ["SCFT", "ACFT", "ROVR", "VEH", "LNCHPRE", "LNCH", "LNCHCIRC", "ORB", "ORBADV", "MNVR",
                       "TGT", "DOCK", "NAV", "LNDG", "LNDGRE", "LNCHAP", "ORBTAP", "LNDGAP", "ACFTAP", "ROVRAP",
                       "Standby"],
}


def png_from_ppm(ppm_path, png_path):
    """Binary PPM (P6, maxval 255) -> 8-bit RGB PNG, standard library only."""
    d = open(ppm_path, "rb").read()
    m = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", d)
    if not m:
        raise ValueError("%s: not a P6 PPM" % ppm_path)
    w, h = int(m.group(1)), int(m.group(2))
    raw = d[m.end():]
    rows = b"".join(b"\x00" + raw[y * w * 3:(y + 1) * w * 3] for y in range(h))  # filter 0 per row

    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)

    with open(png_path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(rows, 9)))
        f.write(chunk(b"IEND", b""))


def build(sketch, out_dir, defines=()):
    """Concatenate the sketch like the IDE, append its scenario file, link
    against the framebuffer stubs. Returns the executable path."""
    sketch_dir = os.path.join(hc.DISPLAYS, sketch)
    scenario_inc = os.path.join(SCENARIOS, sketch + ".inc")
    if not os.path.isfile(scenario_inc):
        sys.exit("no scenario file for %s (expected %s)" % (sketch, scenario_inc))
    files = hc.tabs(sketch_dir)

    # As host_compile.py: a copy of KerbalDisplayCommon.h with its absolute
    # Windows include of body_params.h rewritten. The library .cpp includes the
    # header by quoted name, so a copy of it next to the patched header picks
    # that one up rather than the original.
    kdc = open(hc.KDC_HDR, encoding="utf-8", errors="replace").read()
    kdc = re.sub(r'#include\s+"[A-Za-z]:\\[^"]*body_params\.h"', '#include <body_params.h>', kdc)
    open(os.path.join(out_dir, "KerbalDisplayCommon.h"), "w").write(kdc)
    lib_cpps = []
    for rel in LIB_CPPS:
        src = os.path.join(hc.LIB_ROOT, rel)
        if os.path.basename(rel) == "KerbalDisplayCommon.cpp":
            dst = os.path.join(out_dir, "KerbalDisplayCommon.cpp")
            shutil.copy(src, dst)
            lib_cpps.append(dst)
        else:
            lib_cpps.append(src)

    parts = ['#include <Arduino.h>', '#include "%s.h"' % sketch] + hc.hoisted_prototypes(files)
    for fp in files:
        parts += ['#line 1 "%s"' % fp, open(fp, encoding="utf-8", errors="replace").read()]
    parts += ['#line 1 "%s"' % scenario_inc, open(scenario_inc, encoding="utf-8").read()]
    cpp = os.path.join(out_dir, sketch + ".cpp")
    open(cpp, "w").write("\n".join(parts) + "\n")

    exe = os.path.join(out_dir, "render_" + sketch)
    # -O0: at -O1+ glibc's fortified strlcpy/strlcat collide with the stub's inline
    # ones. Framebuffer stubs first on the path so they shadow host_stubs/.
    cmd = ["g++", "-std=gnu++17", "-O0", "-w"] + ["-D" + d for d in defines] + [
           "-I" + out_dir, "-I" + STUBS_FB, "-I" + hc.STUBS] + \
          ["-I" + d for d in hc.library_include_dirs()] + \
          ["-I" + sketch_dir, "-o", exe, cpp] + lib_cpps
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        sys.stdout.write(r.stdout)
        sys.stderr.write(r.stderr)
        sys.exit("== %s: build FAILED (%d)" % (sketch, r.returncode))
    return exe


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("scenarios", nargs="*", help="scenario names (default: all for the sketch)")
    ap.add_argument("--sketch", default="KCMk1_Annunciator")
    ap.add_argument("--out", default=DEFAULT_OUT, help="output directory for the PNGs")
    ap.add_argument("--keep", action="store_true", help="keep the generated build directory")
    ap.add_argument("--define", "-D", action="append", default=[],
                    help="extra preprocessor define, e.g. -D INFO_DISP_UNIT=2 (may repeat)")
    ap.add_argument("--suffix", default="", help="tag appended to the panel name in output files")
    args = ap.parse_args()

    scenarios = args.scenarios or SCENARIO_NAMES.get(args.sketch, [])
    if not scenarios:
        sys.exit("no scenarios named for %s" % args.sketch)
    panel = args.sketch.replace("KCMk1_", "")
    os.makedirs(args.out, exist_ok=True)
    build_dir = tempfile.mkdtemp(prefix="kcm_render_")
    try:
        exe = build(args.sketch, build_dir, args.define)
        env = dict(os.environ, KCM_SD_ROOT=ASSETS)
        for sc in scenarios:
            ppm = os.path.join(build_dir, sc + ".ppm")
            r = subprocess.run([exe, sc, ppm], env=env, capture_output=True, text=True)
            if r.returncode:
                sys.stderr.write(r.stderr)
                sys.exit("== %s: scenario %s FAILED (%d)" % (args.sketch, sc, r.returncode))
            png = os.path.join(args.out, "%s%s_%s.png" % (panel, args.suffix, sc))
            png_from_ppm(ppm, png)
            print("%s -> %s" % (r.stderr.strip(), os.path.relpath(png, REPO)))
        if args.keep:
            print("build dir kept:", build_dir)
    finally:
        if not args.keep:
            shutil.rmtree(build_dir, ignore_errors=True)


if __name__ == "__main__":
    main()
