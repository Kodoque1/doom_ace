#!/usr/bin/env python3
"""Which of the error-screen strings exist on disk, and where?"""
import sys

NEEDLES = [b"w_wad.c", b"w_uad.c", b"z_zone.c", b"Z_CT at", b"Z_ChangeTag"]
FILES = [
    "verify/base/DOOM2.EXE",
    "verify/after/DOOM2.EXE",
    "doom_ace/engine/code.bin",
    "doom_ace/engine/code.lmp",
    "doom_ace/loader/loader.wad",
    "doom_ace/loader/loader.l1",
    "verify/levels/level1.wad",
]

for path in FILES:
    try:
        d = open(path, "rb").read()
    except OSError as e:
        print("%-28s MISSING (%s)" % (path, e))
        continue
    hits = []
    for n in NEEDLES:
        c = d.count(n)
        if c:
            i = d.find(n)
            hits.append("%s x%d @%#x" % (n.decode(), c, i))
    print("%-28s %8d B  %s" % (path, len(d), "; ".join(hits) or "(none)"))
