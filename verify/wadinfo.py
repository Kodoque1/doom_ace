#!/usr/bin/env python3
"""wadinfo.py - list the lumps of a WAD, or one map's lumps, with sizes.

Standard library only, same as the other tools in verify/.

Usage:
    wadinfo.py <file.wad>                 # every lump
    wadinfo.py <file.wad> --map MAP02     # just that map's 11 lumps
    wadinfo.py <file.wad> --grep TEXTURE  # lumps whose name contains a string
"""
import re
import struct
import sys

# The 11 lumps Doom 2 expects between a map marker and the next thing. NODES and
# friends are what a map needs to be loadable at all; the tool prints them so a
# generated map can be checked against the IWAD's own list, lump for lump.
MAP_LUMPS = ["THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS",
             "SSECTORS", "NODES", "SECTORS", "REJECT", "BLOCKMAP"]


def read_dir(path):
    with open(path, "rb") as fh:
        data = fh.read()
    sig, num, off = struct.unpack("<4sii", data[:12])
    out = []
    for i in range(num):
        o, s, n = struct.unpack("<ii8s", data[off + i * 16:off + i * 16 + 16])
        out.append((n.decode("latin1").rstrip("\0"), o, s, data[o:o + s]))
    return sig.decode("latin1"), out


def map_lumps(lumps, name):
    """The lumps of `name`, in order, stopping at the next map marker or the end."""
    names = [n for n, _, _, _ in lumps]
    if name not in names:
        return []
    start = names.index(name)
    got = [lumps[start]]
    for entry in lumps[start + 1:]:
        if re.fullmatch(r"(MAP\d\d|E\dM\d)", entry[0]):
            break
        got.append(entry)
        if len(got) == 1 + len(MAP_LUMPS):
            break
    return got


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    path = argv[1]
    want_map = None
    want_grep = None
    if "--map" in argv:
        want_map = argv[argv.index("--map") + 1].upper()
    if "--grep" in argv:
        want_grep = argv[argv.index("--grep") + 1].upper()

    sig, lumps = read_dir(path)
    print("%s: %s, %d lumps" % (path, sig, len(lumps)))

    if want_map:
        got = map_lumps(lumps, want_map)
        if not got:
            print("  no map marker %s" % want_map)
            return 1
        for n, o, s, _ in got:
            print("  %-10s off=%-9d size=%d" % (n, o, s))
        return 0

    for i, (n, o, s, _) in enumerate(lumps):
        if want_grep and want_grep not in n.upper():
            continue
        print("  %3d  %-12s off=%-9d size=%d" % (i, n, o, s))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
