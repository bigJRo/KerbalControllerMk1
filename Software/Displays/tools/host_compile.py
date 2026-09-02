#!/usr/bin/env python3
"""
host_compile.py -- compile a KCMk1 display sketch on the host, the way the Arduino
IDE will, without a Teensy toolchain.

The IDE concatenates the tabs (main .ino first, then the rest alphabetically), then
HOISTS A PROTOTYPE FOR EVERY FUNCTION above the whole thing. That second step is what
panel_lint cannot see and what a host g++ run of the tabs in isolation misses: a
function whose signature names a struct defined inside a .ino compiles fine tab by tab
and fails in the IDE with "'MeterStyle' does not name a type". This tool reproduces
the hoisting, then runs g++ -fsyntax-only with the REAL in-repo libraries on the
include path (KerbalDisplayCommon, KCM_Display, KCM_Touch, KerbalDisplayAudio,
KCMk1_SystemConfig, the fonts) and stand-ins only for what the repo does not carry:
the Arduino/Teensy core, the RA8876 driver class, and the KerbalSimpit client
(tools/host_stubs/). So a colour, font, library function or Simpit message id that
does not exist fails here the way it fails in the IDE; the stubs draw nothing.

Usage:
    python3 tools/host_compile.py                      # all three panels
    python3 tools/host_compile.py KCMk1_ResourceDisp   # one sketch by name
    python3 tools/host_compile.py --keep               # leave the build dir for inspection

Exit status is g++'s. A new Arduino, driver or Simpit symbol is a one-line addition
to the matching stub header.
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
LIB_ROOT = os.path.join(DISPLAYS, "libraries")
COMMON   = os.path.join(os.path.dirname(DISPLAYS), "Common")   # Software/Common
KDC_HDR  = os.path.join(LIB_ROOT, "KerbalDisplayCommon", "src", "KerbalDisplayCommon.h")

DEFAULT_SKETCHES = ["KCMk1_Annunciator", "KCMk1_InfoDisp", "KCMk1_ResourceDisp"]


def library_include_dirs():
    """Every in-repo library's src/, plus KerbalDisplayCommon's font directory (its
    header includes the font .c files by relative path and the font header by name)."""
    dirs = []
    for name in sorted(os.listdir(LIB_ROOT)):
        src = os.path.join(LIB_ROOT, name, "src")
        if os.path.isdir(src):
            dirs.append(src)
    dirs.append(os.path.join(LIB_ROOT, "KerbalDisplayCommon", "src", "fonts_ili"))
    dirs.append(COMMON)
    return dirs


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


# An explicit prototype somewhere in the sketch. The IDE does not generate one for a
# function that already has one, which is how a tab declares a function whose
# signature names a type defined further down (the InfoDisp's OrbScene helpers).
DECL = re.compile(
    r'^(?:static\s+|inline\s+|const\s+|constexpr\s+)*[A-Za-z_][\w:<>]*(?:\s*[\*&])?\s+&?\s*(\w+)\s*\([^;{}()]*\)\s*;',
    re.M)


def hoisted_prototypes(files):
    srcs = [strip_comments(open(fp, encoding='utf-8', errors='replace').read()) for fp in files]
    declared = set()
    for src in srcs:
        declared.update(m.group(1) for m in DECL.finditer(src))
    protos = []
    for src in srcs:
        for m in DEF.finditer(src):
            sig  = m.group(1)
            head = sig.split('(')[0].split()
            if head[0] in KEYWORDS or 'struct' in head or 'enum' in head or 'class' in head:
                continue
            name = sig.split('(')[0].split()[-1].lstrip('*&')
            if name in declared:
                continue
            protos.append(' '.join(sig.split()) + ';')
    return protos


def build(sketch, keep):
    sketch_dir = os.path.join(DISPLAYS, sketch)
    files = tabs(sketch_dir)
    if not files:
        print("no tabs found in", sketch_dir)
        return 2
    out = tempfile.mkdtemp(prefix="kcm_host_")
    try:
        # KerbalDisplayCommon.h includes Software/Common/body_params.h by an absolute
        # Windows path. A copy in the build dir (first on the include path) swaps that
        # for a plain <body_params.h>, which Software/Common on the path satisfies.
        kdc = open(KDC_HDR, encoding='utf-8', errors='replace').read()
        kdc = re.sub(r'#include\s+"[A-Za-z]:\\[^"]*body_params\.h"', '#include <body_params.h>', kdc)
        open(os.path.join(out, "KerbalDisplayCommon.h"), "w").write(kdc)
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
               "-Wno-vla", "-Wno-narrowing",
               "-I" + out, "-I" + STUBS] + ["-I" + d for d in library_include_dirs()] + \
              ["-I" + sketch_dir, "-x", "c++", cpp]
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
