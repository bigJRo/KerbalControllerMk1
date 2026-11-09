#!/usr/bin/env python3
"""
make_sd_cards.py -- build one SD-card folder per display firmware.

Each folder under ../SD_Cards/<firmware>/ holds exactly the files that
firmware reads from its microSD card, laid out as they go on the card (copy the
folder's CONTENTS to the card root). The source of every file is ../assets/,
which stays the single place art is generated or edited; this script copies from
it, so rerun it after changing anything there.

    python3 tools/make_sd_cards.py           # rebuild every card folder
    python3 tools/make_sd_cards.py --check   # verify the folders are complete and in
                                             # sync, and that every literal "/....bmp"
                                             # path in each sketch is on its card

Firmware with no card content (Sys Info Display: optional splash only; the master
and the ATtiny modules have no card) is described in ../SD_Cards/README.md.
"""
import argparse
import filecmp
import os
import re
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DISPLAYS = os.path.normpath(os.path.join(HERE, ".."))
ASSETS = os.path.join(DISPLAYS, "assets")
CARDS = os.path.join(DISPLAYS, "SD_Cards")
BODY_PARAMS = os.path.normpath(os.path.join(DISPLAYS, "..", "Common", "body_params.h"))


def body_images():
    """The SOI globe BMPs, read from the shared body table so a new body cannot be missed."""
    txt = open(BODY_PARAMS).read()
    return sorted(set(m.lstrip("/") for m in re.findall(r'"(/[A-Za-z0-9_-]+-Display_236x164\.bmp)"', txt)))


def manifest():
    rover = sorted("Rover/" + f for f in os.listdir(os.path.join(ASSETS, "Rover")) if f.endswith(".bmp"))
    return {
        "KCMk1_Annunciator": body_images() + [
            "KASA_Meatball_236x164.bmp",
            "StandbySplash_1024x600.bmp",
        ] + ["VIcon_%02d.bmp" % i for i in range(17)],
        "KCMk1_InfoDisp": [
            "StandbySplash_1024x600.bmp",
        ] + ["VIcon60_%02d.bmp" % i for i in range(17)] + rover,
        "KCMk1_ResourceDisp": [
            "StandbySplash_1024x600.bmp",
        ],
    }


def card_files(card):
    root = os.path.join(CARDS, card)
    found = []
    for d, _, files in os.walk(root):
        for f in files:
            found.append(os.path.relpath(os.path.join(d, f), root).replace(os.sep, "/"))
    return sorted(found)


def sketch_paths(card):
    """Literal "/name.bmp" paths in the sketch's own tabs (formatted names are in the manifest)."""
    paths = set()
    sk = os.path.join(DISPLAYS, card)
    for f in os.listdir(sk):
        if f.endswith((".ino", ".h")):
            paths |= set(re.findall(r'"/([A-Za-z0-9_./-]+\.bmp)"', open(os.path.join(sk, f), errors="replace").read()))
    return paths


def build(man):
    for card, files in man.items():
        root = os.path.join(CARDS, card)
        want = set(files)
        for f in card_files(card):          # drop files no longer in the manifest
            if f not in want:
                os.remove(os.path.join(root, f))
        for f in files:
            src, dst = os.path.join(ASSETS, f), os.path.join(root, f)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            if not os.path.exists(dst) or not filecmp.cmp(src, dst, shallow=False):
                shutil.copyfile(src, dst)
        for d, dirs, files_ in os.walk(root, topdown=False):   # prune empty folders
            if d != root and not os.listdir(d):
                os.rmdir(d)
        size = sum(os.path.getsize(os.path.join(root, f)) for f in files)
        print("%-20s %4d files  %5.1f MB" % (card, len(files), size / 1e6))


def check(man):
    bad = 0
    for card, files in man.items():
        root = os.path.join(CARDS, card)
        have = set(card_files(card))
        for f in files:
            if not os.path.exists(os.path.join(ASSETS, f)):
                print("%s: %s missing from assets/" % (card, f)); bad += 1
            elif f not in have:
                print("%s: %s missing from the card folder" % (card, f)); bad += 1
            elif not filecmp.cmp(os.path.join(ASSETS, f), os.path.join(root, f), shallow=False):
                print("%s: %s differs from assets/" % (card, f)); bad += 1
        for f in sorted(have - set(files)):
            print("%s: %s is not in the manifest" % (card, f)); bad += 1
        for p in sorted(sketch_paths(card)):
            if p not in files and not p.startswith("Rover/"):
                print("%s: the sketch reads /%s, which is not on its card" % (card, p)); bad += 1
    # the shared library's standby splash
    for card in man:
        if "StandbySplash_1024x600.bmp" not in man[card]:
            print("%s: no standby splash" % card); bad += 1
    print("check: %s" % ("OK" if not bad else "%d problem(s)" % bad))
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    man = manifest()
    if a.check:
        sys.exit(1 if check(man) else 0)
    build(man)
    check(man)


if __name__ == "__main__":
    main()
