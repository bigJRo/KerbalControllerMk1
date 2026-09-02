#!/usr/bin/env python3
"""
host_compile.py -- compile a KCMk1 display sketch on the host, the way the Arduino
IDE will, without a Teensy toolchain.

The IDE concatenates the tabs (main .ino first, then the rest alphabetically), then
HOISTS A PROTOTYPE FOR EVERY FUNCTION above the whole thing. That second step is what
panel_lint cannot see and what a host g++ run of the tabs in isolation misses: a
function whose signature names a struct defined inside a .ino compiles fine tab by tab
and fails in the IDE with "'MeterStyle' does not name a type". This tool reproduces
the hoisting, then runs g++ -fsyntax-only against stand-ins for the display, touch,
Simpit and Arduino libraries (tools/host_stubs/). The stubs draw nothing; the palette
and font metrics are generated from the real KerbalDisplayCommon sources so a colour
or font name that does not exist fails here too.

Usage:
    python3 tools/host_compile.py                      # KCMk1_ResourceDisp
    python3 tools/host_compile.py KCMk1_ResourceDisp   # one sketch by name
    python3 tools/host_compile.py --keep               # leave the build dir for inspection

Exit status is g++'s. The stubs currently cover the ResourceDisp's surface; a new
symbol on another panel is a one-line addition to the matching stub header.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE     = os.path.dirname(os.path.abspath(__file__))
DISPLAYS = os.path.dirname(HERE)
STUBS    = os.path.join(HERE, "host_stubs")
KDC_SRC  = os.path.join(DISPLAYS, "libraries", "KerbalDisplayCommon", "src")
SYSCFG   = os.path.join(DISPLAYS, "libraries", "KCMk1_SystemConfig", "src")

DEFAULT_SKETCHES = ["KCMk1_ResourceDisp"]


def tabs(sketch_dir):
    name = os.path.basename(sketch_dir.rstrip("/"))
    inos = sorted(f for f in os.listdir(sketch_dir) if f.endswith(".ino"))
    main = name + ".ino"
    if main in inos:
        inos.remove(main)
        inos.insert(0, main)
    return [os.path.join(sketch_dir, f) for f in inos]


def strip_comments(src):
    src = re.sub(r'//[^\n]*', '', src)
    return re.sub(r'/\*.*?\*/', '', src, flags=re.S)


# A function definition: optional storage/qualifiers, a return type, a name, a
# parameter list with no nested parentheses, then the opening brace.
DEF = re.compile(
    r'^((?:static\s+|inline\s+|const\s+|constexpr\s+)*[A-Za-z_][\w:<>]*(?:\s*[\*&])?\s+&?\s*\w+\s*\(([^;{}()]*)\))\s*\{',
    re.M)
KEYWORDS = {'if', 'for', 'while', 'switch', 'return', 'else', 'do'}


def hoisted_prototypes(files):
    protos = []
    for fp in files:
        src = strip_comments(open(fp, encoding='utf-8', errors='replace').read())
        for m in DEF.finditer(src):
            sig  = m.group(1)
            head = sig.split('(')[0].split()
            if head[0] in KEYWORDS or 'struct' in head or 'enum' in head or 'class' in head:
                continue
            protos.append(' '.join(sig.split()) + ';')
    return protos


def gen_colors(out_dir):
    """Every TFT_* define from the real KerbalDisplayCommon.h, verbatim."""
    lines = ["#pragma once"]
    for line in open(os.path.join(KDC_SRC, "KerbalDisplayCommon.h"), encoding='utf-8', errors='replace'):
        if re.match(r'\s*#define\s+TFT_\w+\s+0x[0-9A-Fa-f]+', line):
            lines.append(line.rstrip())
    open(os.path.join(out_dir, "gen_colors.h"), "w").write("\n".join(lines) + "\n")


def gen_fonts(out_dir):
    """One ILI9341_t3_font_t per font .c in the library, with its real cap_height and
    line_space, so font-name typos and size assumptions fail on the host too."""
    lines = ["#pragma once", "#include <cstdint>",
             "struct ILI9341_t3_font_t { uint8_t version, reserved, index1_first, index1_last, "
             "index2_first, index2_last, bits_index, bits_width, bits_height, bits_xoffset, "
             "bits_yoffset, bits_delta, line_space, cap_height; };",
             "typedef ILI9341_t3_font_t tFont;"]
    fdir = os.path.join(KDC_SRC, "fonts_ili")
    for f in sorted(os.listdir(fdir)):
        if not f.endswith(".c"):
            continue
        src = open(os.path.join(fdir, f), encoding='utf-8', errors='replace').read()
        cap = re.search(r'(\d+),\s*//\s*cap_height', src)
        ls  = re.search(r'(\d+),\s*//\s*line_space', src)
        name = f[:-2]
        lines.append("static const ILI9341_t3_font_t %s = {1,0,32,126,0,0,0,0,0,0,0,0,%s,%s};"
                     % (name, ls.group(1) if ls else 0, cap.group(1) if cap else 0))
    open(os.path.join(out_dir, "gen_fonts.h"), "w").write("\n".join(lines) + "\n")


def build(sketch, keep):
    sketch_dir = os.path.join(DISPLAYS, sketch)
    files = tabs(sketch_dir)
    if not files:
        print("no tabs found in", sketch_dir)
        return 2
    out = tempfile.mkdtemp(prefix="kcm_host_")
    try:
        gen_colors(out)
        gen_fonts(out)
        open(os.path.join(out, "Serial_impl.cpp"), "w").write(
            "#include <Arduino.h>\nHostSerial Serial, SerialUSB1, Serial2;\nHostWire Wire, Wire1, Wire2;\n"
            "const unsigned char TEXT_BORDER = 8;\n")
        # Concatenate like the IDE: header include first (so the hoisted prototypes
        # see the sketch's own types), then the prototypes, then every tab in order.
        parts = ['#include <Arduino.h>', '#include "%s.h"' % sketch]
        parts += hoisted_prototypes(files)
        for fp in files:
            parts.append('#line 1 "%s"' % fp)
            parts.append(open(fp, encoding='utf-8', errors='replace').read())
        cpp = os.path.join(out, sketch + ".cpp")
        open(cpp, "w").write("\n".join(parts) + "\n")
        cmd = ["g++", "-std=gnu++17", "-fsyntax-only", "-Wall", "-Wextra",
               "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-unused-variable",
               "-Wno-vla",
               "-I" + out, "-I" + STUBS, "-I" + SYSCFG, "-I" + sketch_dir,
               "-x", "c++", cpp]
        print("== %s: %d tabs, %d prototypes hoisted" % (sketch, len(files), len(parts) - 2 - len(files) * 2))
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.stdout:
            print(r.stdout, end="")
        if r.stderr:
            print(r.stderr, end="")
        print("== %s: %s" % (sketch, "OK" if r.returncode == 0 else "FAILED (%d)" % r.returncode))
        if keep:
            print("build dir kept:", out)
        return r.returncode
    finally:
        if not keep:
            shutil.rmtree(out, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sketches", nargs="*", default=DEFAULT_SKETCHES)
    ap.add_argument("--keep", action="store_true", help="keep the generated build directory")
    args = ap.parse_args()
    rc = 0
    for s in args.sketches:
        rc |= build(s, args.keep)
    sys.exit(rc)


if __name__ == "__main__":
    main()
