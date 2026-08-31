#!/usr/bin/env python3
"""
panel_lint.py -- static checks for the KCMk1 display sketches.

Four checks, all of which have caught real defects in this repo:

  stale-comments   A constant's trailing comment asserts a value the constant no
                   longer has. These are not cosmetic. The ACFT/SCFT geometry
                   comments went stale when the attitude disc was enlarged, and two
                   later layout decisions were made off those stale numbers and came
                   out wrong -- a rate bar placed under the roll readout, and a
                   readout cell sized for a string 16 px narrower than the real one.

  dead-constants   A file-scope constant nothing reads. Worse than clutter here:
                   40 of them were a shadow copy of the shared EADI_* PFD geometry,
                   so editing SCFT_PTAPE_MRK_HW moved nothing and told you nothing.
                   A dead THRESHOLD is worse still -- STALL_SPEED_MS and
                   TGT_VCLOSURE_WARN_MS each documented a colour band the code did
                   not have.

  duplicate-consts The same suffix declared under two or more screen prefixes with
                   identical values. Sometimes legitimate (MNVR and TGT genuinely
                   both have a BAR_W). Sometimes one instrument's geometry written
                   out twice, waiting to diverge.

  unreset-statics  A mutable file-scope static that no chrome/reset function
                   assigns. ADVISORY ONLY, and it false-positives a lot: the usual
                   correct pattern here is a separate `*Valid` flag that IS reset,
                   with the coordinates left alone. Read each hit before believing
                   it.

Usage:
    python3 tools/panel_lint.py                     # all three sketches
    python3 tools/panel_lint.py KCMk1_InfoDisp      # one sketch
    python3 tools/panel_lint.py --check stale-comments --check dead-constants

Exit status is non-zero if stale-comments or dead-constants find anything; the
other two are informational and never fail the run.

The constant evaluator understands the arithmetic these sketches actually use
(integer and float literals, the four operators, parentheses, and C casts). A
constant whose initialiser it cannot evaluate is skipped rather than guessed at,
so a clean run means "nothing contradicted", not "everything proved".
"""

import argparse
import collections
import os
import re
import sys

HERE      = os.path.dirname(os.path.abspath(__file__))
DISPLAYS  = os.path.dirname(HERE)
SKETCHES  = ["KCMk1_InfoDisp", "KCMk1_Annunciator", "KCMk1_ResourceDisp"]

# Headers outside the sketch that seed the evaluator (KCM_SCREEN_W/H et al).
SEED_HEADERS = [
    os.path.join(DISPLAYS, "libraries", "KerbalDisplayCommon", "src", "KerbalDisplayCommon.h"),
    os.path.join(DISPLAYS, "libraries", "KCMk1_SystemConfig", "src", "KCMk1_SystemConfig.h"),
]

NUMTYPE = r'(?:u?int(?:8|16|32|64)_t|unsigned\s+\w+|int|byte|short|long|float|double)'


class CInt(int):
    """An integer that divides the way C does. `(SCREEN_H - PANEL_Y) / NROWS` is 76 in
    the firmware and 76.714... in Python, and the difference is not academic: evaluating
    it Python's way made this tool report a correct comment as stale. Division and
    modulus truncate toward zero when both operands are integral, and promote to float
    the moment either side is a float -- which is exactly C's rule for these expressions.
    """
    def _wrap(v):
        return CInt(v) if isinstance(v, int) and not isinstance(v, bool) else v

    def __add__(s, o):      return CInt._wrap(int(s) + o)
    def __radd__(s, o):     return CInt._wrap(o + int(s))
    def __sub__(s, o):      return CInt._wrap(int(s) - o)
    def __rsub__(s, o):     return CInt._wrap(o - int(s))
    def __mul__(s, o):      return CInt._wrap(int(s) * o)
    def __rmul__(s, o):     return CInt._wrap(o * int(s))
    def __neg__(s):         return CInt(-int(s))
    def __pos__(s):         return s

    def __truediv__(s, o):
        if isinstance(o, float):
            return int(s) / o
        q = int(s) / int(o)
        return CInt(int(q))          # int(): truncation toward zero, as C does
    def __rtruediv__(s, o):
        if isinstance(o, float):
            return o / int(s)
        q = int(o) / int(s)
        return CInt(int(q))
    def __mod__(s, o):
        if isinstance(o, float):
            return int(s) % o
        return CInt(int(s) - int(o) * int(int(s) / int(o)))
    def __rmod__(s, o):
        if isinstance(o, float):
            return o % int(s)
        return CInt(int(o) - int(s) * int(int(o) / int(s)))
DECL    = re.compile(r'^\s*(?:static\s+)?const\s+' + NUMTYPE + r'\s+(\w+)\s*=\s*([^;]+);'
                     r'\s*(?://\s*(.*))?$')
# Any file-scope constant, numeric or not — used by the dead-constant check.
ANYDECL = re.compile(r'^\s*(?:static\s+)?const\s+(?:' + NUMTYPE + r'|char|tFont|\w+)\s*\*?\s*'
                     r'(\w+)\s*(?:\[[^\]]*\])?\s*(?:\w+\s*)*=')
DEFINE  = re.compile(r'^\s*#define\s+(\w+)\s+(-?\d+)\s*$')


# ── source helpers ────────────────────────────────────────────────────────────────────

def tabs(sketch_dir):
    """Sketch tabs in Arduino's own concatenation order: the main .ino, then the
    rest alphabetically, then the headers."""
    name = os.path.basename(sketch_dir.rstrip("/"))
    inos = sorted(f for f in os.listdir(sketch_dir) if f.endswith(".ino"))
    main = name + ".ino"
    if main in inos:
        inos.remove(main)
        inos.insert(0, main)
    hdrs = sorted(f for f in os.listdir(sketch_dir) if f.endswith(".h"))
    return [os.path.join(sketch_dir, f) for f in inos + hdrs]


def strip_comments(src):
    """Remove comments without being fooled by // or /* inside a string literal."""
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if c == '/' and i + 1 < n and src[i + 1] == '/':
            j = src.find('\n', i)
            i = n if j < 0 else j
        elif c == '/' and i + 1 < n and src[i + 1] == '*':
            j = src.find('*/', i + 2)
            i = n if j < 0 else j + 2
        elif c in '"\'':
            q = c
            out.append(c)
            i += 1
            while i < n and src[i] != q:
                out.append(src[i])
                i += 2 if src[i] == '\\' else 1
                if src[i - 1] == '\\':
                    out.append(src[i - 1])
            out.append(q)
            i += 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


# ── constant evaluation ───────────────────────────────────────────────────────────────

def evaluate(files, seeds=()):
    """-> (values, comments, where). Iterates to a fixed point so a constant may
    reference one declared in a later-concatenated tab."""
    values   = {}
    comments = {}
    where    = {}
    aliases  = set()
    for path in seeds:
        if not os.path.exists(path):
            continue
        for line in open(path, encoding='utf-8', errors='replace'):
            m = DEFINE.match(line)
            if m:
                values[m.group(1)] = int(m.group(2))


    raw = []
    for fp in files:
        lines = open(fp, encoding='utf-8', errors='replace').read().split('\n')
        for ln, line in enumerate(lines, 1):
            # An initialiser may wrap. Join continuation lines up to the ';' before
            # matching, or a declaration like
            #     static const int16_t X = A +
            #                             B;
            # is silently skipped and its comment never checked.
            if ';' not in line and re.match(r'\s*(?:static\s+)?const\s+' + NUMTYPE + r'\s+\w+\s*=', line):
                joined, k = line, ln
                while ';' not in joined and k < len(lines) and k - ln < 6:
                    joined += ' ' + lines[k].strip()
                    k += 1
                line = joined
            m = DECL.match(line.rstrip())
            if m:
                raw.append((fp, ln, m.group(1), m.group(2).strip(), (m.group(3) or '').strip()))

    for _ in range(16):
        progress = False
        for fp, ln, name, expr, comment in raw:
            if name in values:
                continue
            e = re.sub(r'\b(\d+(?:\.\d+)?)[fFuUlL]+\b', r'\1', expr)     # 6.0f -> 6.0
            # A (float)/(double) cast promotes what follows it, so the rest of the
            # expression divides in floating point. Integer casts only narrow, which this
            # evaluator does not model, so they are simply dropped.
            e = re.sub(r'\(\s*(?:float|double)\s*\)\s*', '1.0*', e)
            e = re.sub(r'\(\s*(?:' + NUMTYPE + r')\s*\)', '', e)
            if set(re.findall(r'[A-Za-z_]\w*', e)) - set(values):
                continue
            env = {k: (CInt(x) if isinstance(x, int) else x) for k, x in values.items()}
            e_c = re.sub(r'(?<![\w.])(\d+)(?![\d.\w])', r'CInt(\1)', e)   # int literals too
            try:
                v = eval(e_c, {'__builtins__': {}, 'CInt': CInt}, env)    # noqa: S307
            except Exception:
                continue
            if not isinstance(v, (int, float)):
                continue
            v = int(v) if isinstance(v, int) else v
            values[name]   = v
            comments[name] = comment
            where[name]    = (fp, ln, expr)
            # A bare-identifier initialiser is an ALIAS, not an independent re-derivation
            # — SCFT_PTAPE_X = EADI_PTAPE_X cannot drift, so duplicate-consts skips it.
            if re.fullmatch(r'[A-Za-z_]\w*', expr.strip()):
                aliases.add(name)
            progress = True
        if not progress:
            break
    resolved = sum(1 for _, _, n, _, _ in raw if n in values)
    return values, comments, where, (resolved, len(raw)), aliases


# ── checks ────────────────────────────────────────────────────────────────────────────

# A trailing comment "asserts" a value when it OPENS with a bare number that is either
# the whole comment or is followed by a dash — the house style, "// 133" and
# "// 82 — same top as the tape's border". Everything else is prose that happens to
# start with a digit and is left alone: "// 10 min", "// 40% wider than the original 96",
# "// 52 entries", "// 24px (KcmTerm_24, cell 24) + 3px leading". The number is pinned
# with a negative lookahead rather than left greedy, or the engine backtracks "10 min"
# down to a bare "1" and reports every one of them.
ASSERTS = re.compile(r'^(\d[\d,]*)(?![\d,\w])\s*(?:$|[\u2014\u2013-]\s)')


def check_stale_comments(values, comments, where):
    hits = []
    for name, v in values.items():
        comment = comments.get(name) or ''
        if abs(v - round(v)) > 1e-6:
            continue
        m = ASSERTS.match(comment)
        if not m:
            continue
        stated = int(m.group(1).replace(',', ''))
        if stated != int(round(v)):
            fp, ln, expr = where[name]
            hits.append((fp, ln, name, expr, stated, int(round(v)), comment))
    return sorted(hits)


def check_dead_constants(files):
    """A constant may be kept deliberately — a protocol enumeration whose members are
    named for the wire format rather than for any call site, say. Mark those in the
    source, not in this tool: `// panel-lint: keep — <reason>` on the declaration, or a
    `panel-lint: keep-begin` / `keep-end` pair around a block. Requiring a reason in the
    source is the point: an unexplained suppression is how the next dead threshold hides."""
    decls = {}
    for fp in files:
        keeping = False
        for ln, line in enumerate(open(fp, encoding='utf-8', errors='replace'), 1):
            if 'panel-lint: keep-begin' in line:
                keeping = True
            elif 'panel-lint: keep-end' in line:
                keeping = False
            if keeping or 'panel-lint: keep' in line:
                continue
            m = ANYDECL.match(line)
            if m:
                decls.setdefault(m.group(1), (fp, ln))
    body  = "\n".join(strip_comments(open(f, encoding='utf-8', errors='replace').read())
                      for f in files)
    uses  = collections.Counter(re.findall(r'[A-Za-z_]\w*', body))
    return sorted((fp, ln, n) for n, (fp, ln) in decls.items() if uses[n] <= 1)


def check_duplicate_consts(values, aliases, prefixes):
    groups = {}
    for name, v in values.items():
        if name in aliases:
            continue
        for p in prefixes:
            if name.startswith(p):
                groups.setdefault(name[len(p):], {})[p] = v
    out = []
    for suffix, d in sorted(groups.items()):
        if len(d) > 1 and len(set(d.values())) == 1:
            out.append((suffix, sorted(d), next(iter(d.values()))))
    return out


def check_unreset_statics(files):
    STATIC = re.compile(r'^\s*static\s+(?!const)(?:volatile\s+)?[\w:]+\s*\*?\s*(_\w+)\s*'
                        r'(?:\[[^\]]*\])?\s*=', re.M)
    RESETFN = re.compile(r'^(?:static\s+)?void\s+(\w*(?:hrome|eset|Reset|Chrome)\w*)\s*'
                         r'\([^;{]*\)\s*\{', re.M)
    out = []
    for fp in files:
        src = open(fp, encoding='utf-8', errors='replace').read()
        statics = set(STATIC.findall(src))
        if not statics:
            continue
        assigned = set()
        for m in RESETFN.finditer(src):
            i = src.index('{', m.start())
            depth, j = 0, i
            while j < len(src):
                if src[j] == '{':
                    depth += 1
                elif src[j] == '}':
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            assigned |= set(re.findall(r'(_\w+)\s*(?:=[^=]|\[[^\]]*\]\s*=[^=])', src[i:j]))
        # A `*Valid`/`*Ready` guard flag that IS reset makes its coordinates safe.
        guards = {a for a in assigned if a.endswith(('Valid', 'Ready'))}
        missing = sorted(s for s in statics - assigned
                         if 'Ready' not in s and 'Table' not in s)
        if missing:
            out.append((fp, sorted(missing), sorted(guards)))
    return out


# ── driver ────────────────────────────────────────────────────────────────────────────

ALL_CHECKS = ["stale-comments", "dead-constants", "duplicate-consts", "unreset-statics"]


def run(sketch_dir, checks, prefixes):
    rel   = os.path.relpath(sketch_dir, DISPLAYS)
    files = tabs(sketch_dir)
    values, comments, where, (resolved, total), aliases = evaluate(files, SEED_HEADERS)
    print(f"\n=== {rel}: {len(files)} tabs, {resolved}/{total} numeric constants evaluated")
    failures = 0

    if "stale-comments" in checks:
        hits = check_stale_comments(values, comments, where)
        print(f"\n-- stale-comments: {len(hits)}")
        for fp, ln, name, expr, stated, actual, comment in hits:
            print(f"   {os.path.relpath(fp, DISPLAYS)}:{ln}  {name} = {expr}")
            print(f"       comment says {stated}, value is {actual}   // {comment}")
        failures += len(hits)

    if "dead-constants" in checks:
        hits = check_dead_constants(files)
        print(f"\n-- dead-constants: {len(hits)}")
        for fp, ln, name in hits:
            print(f"   {os.path.relpath(fp, DISPLAYS)}:{ln}: {name}")
        failures += len(hits)

    if "duplicate-consts" in checks:
        hits = check_duplicate_consts(values, aliases, prefixes)
        print(f"\n-- duplicate-consts (advisory): {len(hits)} suffixes independently "
              f"re-derived to the same value under >1 prefix")
        for suffix, prefs, v in hits:
            print(f"   {suffix:24} = {v!r:>12}   under {', '.join(prefs)}")

    if "unreset-statics" in checks:
        hits = check_unreset_statics(files)
        n = sum(len(m) for _, m, _ in hits)
        print(f"\n-- unreset-statics (advisory, expect false positives): {n}")
        for fp, missing, guards in hits:
            print(f"   {os.path.relpath(fp, DISPLAYS)}")
            print(f"      not assigned in any chrome/reset fn: {', '.join(missing)}")
            if guards:
                print(f"      (guard flags that ARE reset: {', '.join(guards)}"
                      f" -- these often make the above safe)")
    return failures


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sketch", nargs="*", default=None,
                    help="sketch directory (default: all three)")
    ap.add_argument("--check", action="append", choices=ALL_CHECKS, default=None,
                    help="run only these checks (repeatable)")
    ap.add_argument("--prefix", action="append", default=None,
                    help="screen prefix for duplicate-consts "
                         "(default: the InfoDisp screen prefixes)")
    args = ap.parse_args()

    checks   = args.check or ALL_CHECKS
    prefixes = args.prefix or ["EADI_", "ACFT_", "SCFT_", "ROVR_", "MNVR_",
                               "TGT_", "DOCK_", "LNDG_", "LNCH_", "ORB_", "NAV_"]
    dirs = args.sketch or [os.path.join(DISPLAYS, s) for s in SKETCHES]

    failures = 0
    for d in dirs:
        d = d if os.path.isabs(d) else os.path.join(DISPLAYS, d)
        if not os.path.isdir(d):
            print(f"skip (not a directory): {d}")
            continue
        failures += run(d, checks, prefixes)

    hard = [c for c in checks if c in ("stale-comments", "dead-constants")]
    if hard:
        print(f"\n{failures} finding(s) from {', '.join(hard)}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
