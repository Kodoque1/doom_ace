#!/usr/bin/env python3
"""Which texture should become the "screen"?  (Doom 2 v1.9 WAD, stdlib only)

Reads DOOM2.WAD, builds the texture list the way Doom does (TEXTURE1 then TEXTURE2,
indices are positions in that concatenation), then counts how MAP01's sidedefs use
them, per slot. The most-used texture in the first map is the one that is guaranteed
to be in view wherever the demo clip starts - and the count is what says so, instead
of picking a number and hoping.
"""
import struct
import os, sys

WAD = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.environ.get("MTX_ROOT", os.path.expanduser("~/Documents/recursive_doom")), "verify/after/DOOM2.WAD")
MAP = (sys.argv[2] if len(sys.argv) > 2 else "MAP01").encode()

raw = open(WAD, "rb").read()
magic, numlumps, infotableofs = struct.unpack_from("<4sii", raw, 0)
assert magic in (b"IWAD", b"PWAD"), magic

lumps = {}
order = []
for i in range(numlumps):
    pos, size, name = struct.unpack_from("<ii8s", raw, infotableofs + i * 16)
    name = name.rstrip(b"\0").decode("ascii", "replace")
    lumps[name] = (pos, size)
    order.append(name)


def texture_names():
    names = []
    for lump in ("TEXTURE1", "TEXTURE2"):
        if lump not in lumps:
            continue
        pos, size = lumps[lump]
        n = struct.unpack_from("<i", raw, pos)[0]
        offs = struct.unpack_from("<%di" % n, raw, pos + 4)
        for o in offs:
            names.append(raw[pos + o:pos + o + 8].split(b"\0")[0].decode("ascii", "replace"))
    return names


tex = texture_names()
index = {n: i for i, n in enumerate(tex)}
print("textures:", len(tex))
print("TEXTURE1 covers indices 0..%d" % (len(tex) - 1))

# MAP01 sidedefs: 30 bytes each - xoff, yoff, top, bottom, mid, sector
pos, size = lumps[MAP.decode()]
i = order.index(MAP.decode())
# lumps of the map follow the marker, in fixed order
want = ["SIDEDEFS"]
counts = {"top": {}, "mid": {}, "bottom": {}}
for name in want:
    j = order.index(name, i)
    p, s = lumps[name]
    n = s // 30
    for k in range(n):
        o = p + k * 30
        tops = raw[o + 4:o + 12].rstrip(b"\0")
        bots = raw[o + 12:o + 20].rstrip(b"\0")
        mids = raw[o + 20:o + 28].rstrip(b"\0")
        for tag, val in (("top", tops), ("bottom", bots), ("mid", mids)):
            if val and val != b"-":
                nm = val.decode("ascii", "replace")
                counts[tag][nm] = counts[tag].get(nm, 0) + 1

total = {}
for tag in counts:
    for nm, c in counts[tag].items():
        total[nm] = total.get(nm, 0) + c

print("\nMAP01 texture usage (all slots), top 12:")
for nm, c in sorted(total.items(), key=lambda kv: -kv[1])[:12]:
    idx = index.get(nm, -1)
    print("  %-9s used %4d times   texture index %d" % (nm, c, idx))

print("\nmid (wall) textures most used in MAP01:")
for nm, c in sorted(counts["mid"].items(), key=lambda kv: -kv[1])[:6]:
    print("  %-9s %4d   index %d" % (nm, c, index.get(nm, -1)))
