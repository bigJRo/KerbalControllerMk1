#!/usr/bin/env python3
"""
host_compile.py -- compile Controller_Main on the host, the way the Arduino IDE will,
without a Teensy toolchain.

Same method as Software/Displays/tools/host_compile.py (whose tab ordering and
prototype hoisting it reuses): the tabs are concatenated main-first, a prototype is
hoisted for every function, and g++ -fsyntax-only runs with the real in-repo headers
on the include path (Software/Common, KerbalModuleCommon) and stand-ins only for what
the repo does not carry: the Arduino/Teensy core and Wire (the displays' host_stubs),
and Bounce, Adafruit_EMC2101, Adafruit_INA219 and Watchdog_t4 (tools/host_stubs here).

KerbalSimpit: by default the displays' KerbalSimpit stand-in. Pass --simpit DIR to
compile against the real library instead (its src/ directory, e.g. a clone of
Simpit-team/KerbalSimpitRevamped-Arduino), which checks every message id and payload
field the master uses against the plugin's own definitions.

The sketch includes Software/Common headers by absolute Windows path (the IDE copies a
sketch to a temporary build folder, so a relative path would not resolve); the build
copy rewrites those to plain includes.

Usage:
    python3 Software/Controller_Main/tools/host_compile.py [--simpit DIR] [--keep]

Exit status is g++'s.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE     = os.path.dirname(os.path.abspath(__file__))
SKETCH   = os.path.dirname(HERE)
SOFTWARE = os.path.dirname(SKETCH)
DISPLAY_TOOLS = os.path.join(SOFTWARE, "Displays", "tools")
# The displays' tool has the same module name as this one, so load it by path.
import importlib.util   # noqa: E402
_spec = importlib.util.spec_from_file_location("display_host_compile", os.path.join(DISPLAY_TOOLS, "host_compile.py"))
_disp = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_disp)
tabs, hoisted_prototypes = _disp.tabs, _disp.hoisted_prototypes

STUBS_LOCAL   = os.path.join(HERE, "host_stubs")
STUBS_DISPLAY = os.path.join(DISPLAY_TOOLS, "host_stubs")
COMMON        = os.path.join(SOFTWARE, "Common")
KMC           = os.path.join(SOFTWARE, "Modules", "libraries", "KerbalModuleCommon", "src")

ABS_COMMON = re.compile(r'#include\s+"[A-Za-z]:\\[^"]*\\Common\\([^"\\]+)"')


def sketch_source():
    """The sketch as the IDE compiles it: tabs concatenated, the hoisted prototypes in the
    main tab right after its last #include (so they see the master's own headers, some of
    which, like module_variables.h, define globals and must not be included twice), and
    the absolute Software/Common includes made plain."""
    files = tabs(SKETCH)
    protos = hoisted_prototypes(files)
    parts = ['#include <Arduino.h>']
    for i, fp in enumerate(files):
        src = ABS_COMMON.sub(r'#include "\1"', open(fp, encoding="utf-8", errors="replace").read())
        if i == 0:
            lines = src.splitlines()
            last = max(n for n, l in enumerate(lines) if l.lstrip().startswith("#include"))
            parts.append('#line 1 "%s"' % fp)
            parts.append("\n".join(lines[:last + 1]))
            parts += protos
            parts.append('#line %d "%s"' % (last + 2, fp))
            parts.append("\n".join(lines[last + 1:]))
        else:
            parts.append('#line 1 "%s"' % fp)
            parts.append(src)
    return "\n".join(parts) + "\n", len(files)


def include_dirs(simpit):
    # The real library splits its definitions over three headers; the displays'
    # stand-in is one, so stub mode forwards the other two to it.
    return [STUBS_LOCAL, simpit if simpit else os.path.join(HERE, "simpit_stub_fwd"),
            STUBS_DISPLAY, COMMON, KMC, SKETCH]


GXX_FLAGS = ["-std=gnu++17", "-Wall", "-Wextra",
             "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-unused-variable",
             "-Wno-unused-but-set-variable", "-Wno-narrowing"]


def build(simpit, keep):
    out = tempfile.mkdtemp(prefix="kcm_master_")
    try:
        src, ntabs = sketch_source()
        cpp = os.path.join(out, "Controller_Main.cpp")
        open(cpp, "w").write(src)
        cmd = ["g++", "-fsyntax-only"] + GXX_FLAGS + \
              ["-I" + d for d in include_dirs(simpit)] + ["-x", "c++", cpp]
        print("== Controller_Main: %d tabs, Simpit: %s" % (ntabs, simpit or "stub"))
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.stdout:
            print(r.stdout, end="")
        if r.stderr:
            print(r.stderr, end="")
        print("== Controller_Main: %s" % ("OK" if r.returncode == 0 else "FAILED (%d)" % r.returncode))
        if keep:
            print("build dir kept:", out)
        return r.returncode
    finally:
        if not keep:
            shutil.rmtree(out, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--simpit", help="KerbalSimpit library src/ directory (default: the displays' stub)")
    ap.add_argument("--keep", action="store_true", help="keep the generated build directory")
    args = ap.parse_args()
    sys.exit(build(args.simpit, args.keep))


if __name__ == "__main__":
    main()
