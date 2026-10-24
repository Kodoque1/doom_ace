#!/usr/bin/env python3
"""mkwadlevels.py - build one WAD per nesting level: the SAME exploit, its own map,
and a screen surface planted in front of the player's start.

Why a WAD per level: the exploit is a WAD (crafted PNAMES + TEXTURE1 read out of
bounds by the texture loader), so an instance that loads a WAD is exploited AGAIN to
reach the next depth. Each level's WAD therefore carries
  - the exploit lumps, copied BYTE-IDENTICAL from loader.wad (hashes are asserted, so
    "the same exploit" is a checked claim and not a comment), and
  - a map, so the instances are visibly different places rather than mirrors, and
  - the screen texture it wants painted with the deeper instance's frame, recorded in
    an MTXLEVEL lump so the engine does not have to guess.

The map is BORROWED from DOOM2.WAD (MAPxx lumps, including NODES) and only its sidedef
TEXTURE NAMES are changed: geometry is untouched, so the BSP stays valid. That is the
whole trick of planting a screen without a map editor.

A level's map is written under ITS OWN NAME (MAP02 in level1.wad, MAP03 in level2.wad),
never renamed to MAP01. Two reasons, both structural:

  - the root loads all three level WADs ONCE, with a repeated `-file`, and
    `wad_check_lump` searches BACKWARD (so the last copy of a name wins). Three files
    that each carry a "MAP01" would make the deepest one override the others, and
    every depth would load the same room - which is exactly what "the nest is a hall
    of mirrors" was.
  - with distinct names, the depth picks its own map by number (its own `startmap`),
    and no file can silently override another. The root keeps the IWAD's untouched
    MAP01, so the root's view stays the measured baseline.

The marker lump is named per level too - MTXLEV00, MTXLEV01, ... - for the same
backward-search reason: a single "MTXLEVEL" would resolve to the last WAD loaded at
every depth, i.e. every level would read the deepest level's marker.

Usage:
    mkwadlevels.py --iwad verify/base/DOOM2.WAD --loader doom_ace/loader/loader.wad \
                   --out verify/levels
"""
import argparse
import hashlib
import os
import struct
import sys

MAP_LUMPS = ["THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS",
             "SSECTORS", "NODES", "SECTORS", "REJECT", "BLOCKMAP"]

# The lumps that ARE the exploit plus the engine payload and the instance's own
# parameter/config lumps. Copied verbatim; a level WAD that lacked any one of them
# would not be the same exploit.
EXPLOIT_LUMPS = ["ACE_CODE", "ACE_LDR0", "ACE_LDR1", "PNAMES", "TEXTURE1",
                 "MTXGUEST", "ACE_CONF"]

# Level 0 is the root: it keeps the IWAD's own MAP01 (a map here would change the
# root's level too, since the root loads this file at boot), and its screen is the
# surface already measured in that view. Levels >= 1 are for instances, and their
# map keeps the name it was borrowed from - see the module docstring for why a
# shared MAP01 name is the bug this avoids.
LEVELS = [
    dict(idx=0, src=None,     map=None,     screen="PIPE4",    faces=0,  note="root, IWAD MAP01"),
    dict(idx=1, src="MAP02",  map="MAP02",  screen="BIGBRIK2", faces=12, note="instance 1"),
    dict(idx=2, src="MAP03",  map="MAP03",  screen="MARBFAC2", faces=12, note="instance 2"),
]

# Doom records which sidedef faces which way by its vertex order: for the right
# (first) sidedef the wall's visible face normal is (dy, -dx). A player standing on
# the normal's side sees that face.
def face_normal(verts, line):
    v1 = verts[line["v1"]]
    v2 = verts[line["v2"]]
    dx = v2[0] - v1[0]
    dy = v2[1] - v1[1]
    return (dy, -dx), ((v1[0] + v2[0]) // 2, (v1[1] + v2[1]) // 2)


def read_wad(path):
    with open(path, "rb") as fh:
        data = fh.read()
    sig, num, off = struct.unpack("<4sii", data[:12])
    lumps = []
    for i in range(num):
        o, s, n = struct.unpack("<ii8s", data[off + i * 16:off + i * 16 + 16])
        lumps.append([n.decode("latin1").rstrip("\0"), data[o:o + s]])
    return sig.decode("latin1"), lumps


def find_map(lumps, name):
    names = [n for n, _ in lumps]
    if name not in names:
        raise SystemExit("map %s not in the IWAD" % name)
    i = names.index(name)
    out = {}
    for k in range(len(MAP_LUMPS)):
        n, d = lumps[i + 1 + k]
        if n != MAP_LUMPS[k]:
            raise SystemExit("map %s: lump %d is %s, expected %s" % (name, k, n, MAP_LUMPS[k]))
        out[n] = d
    return out


def iwad_texture_names(lumps):
    """Every texture name DOOM2.WAD actually defines (TEXTURE1 then TEXTURE2).

    This is the list the ENGINE's own texture array is built from, in this order,
    which is why an index printed by the engine ("screen texture 189") matches the
    position in TEXTURE1. The check exists because a screen texture that the IWAD
    does not define is not an error anywhere in the pipeline: the sidedef carries
    the name, the map loads, and the wall is drawn with Doom's "unknown texture"
    instead - measured, as level 1 asking for COMPUTE2 (a Doom 1 name), the engine
    answering "screen texture COMPUTE2 NOT FOUND", and that level painting 0
    pixels while level 2 painted 29057.
    """
    names = []
    have = dict(lumps)
    for lump in ("TEXTURE1", "TEXTURE2"):
        b = have.get(lump)
        if not b:
            continue
        count = struct.unpack_from("<i", b, 0)[0]
        for i in range(count):
            off = struct.unpack_from("<i", b, 4 + 4 * i)[0]
            names.append(b[off:off + 8].rstrip(b"\0").decode("latin1"))
    return names


def texture_names(sidedefs):
    """Every texture name a sidedef references, as a set, for the 'don't collide' check."""
    got = set()
    for i in range(len(sidedefs) // 30):
        e = sidedefs[i * 30:i * 30 + 30]
        for off in (4, 12, 20):
            nm = e[off:off + 8].decode("latin1").rstrip("\0")
            if nm and nm != "-":
                got.add(nm)
    return got


def plant_screens(map_lumps, screen, count):
    """Give the `count` closest visible one-sided walls the screen texture.

    One-sided walls on purpose: the wall's middle texture IS its whole surface, so
    the picture lands on a solid surface with no transparent parts and no upper/lower
    pieces left over. Chooses by distance from the player start, requires the face to
    point at the player (otherwise the wall is seen from behind) and a minimum length
    (a screen nobody can read is not a screen).

    Returns (patched, candidates) where candidates is what was considered, so the
    caller can report WHY these walls were picked.
    """
    things = map_lumps["THINGS"]
    verts_d = map_lumps["VERTEXES"]
    linedefs_d = map_lumps["LINEDEFS"]
    sidedefs_d = bytearray(map_lumps["SIDEDEFS"])

    verts = [struct.unpack_from("<hh", verts_d, i * 4) for i in range(len(verts_d) // 4)]
    start = None
    for i in range(len(things) // 10):
        x, y, ang, typ, flg = struct.unpack_from("<hhHHH", things, i * 10)
        if typ == 1:                      # Player 1 start
            start = (x, y)
            break
    if start is None:
        raise SystemExit("no player 1 start in THINGS")

    cands = []
    for i in range(len(linedefs_d) // 14):
        v1, v2, flags, special, tag, s0, s1 = struct.unpack_from("<HHHHHhh", linedefs_d, i * 14)
        if s0 < 0 or s1 >= 0:
            continue                      # need a one-sided line (right side only)
        (nx, ny), mid = face_normal(verts, {"v1": v1, "v2": v2})
        vx, vy = start
        to_player = (vx - mid[0], vy - mid[1])
        if to_player[0] * nx + to_player[1] * ny <= 0:
            continue                      # seen from behind
        length = ((verts[v2][0] - verts[v1][0]) ** 2 + (verts[v2][1] - verts[v1][1]) ** 2) ** 0.5
        dist = (to_player[0] ** 2 + to_player[1] ** 2) ** 0.5
        cands.append((dist, length, i, s0, mid, nx, ny))
    cands.sort(key=lambda c: (c[0] - c[1] * 0.35, c[0]))

    patched = []
    for dist, length, i, s0, mid, nx, ny in cands:
        if len(patched) >= count:
            break
        if length < 64:
            continue
        off = s0 * 30 + 20                # the sidedef's middle texture
        old = bytes(sidedefs_d[off:off + 8]).decode("latin1").rstrip("\0")
        sidedefs_d[off:off + 8] = screen.encode("ascii").ljust(8, b"\0")
        patched.append(dict(line=i, side=s0, mid=mid, dist=int(dist), length=int(length),
                            old=old, new=screen))

    map_lumps["SIDEDEFS"] = bytes(sidedefs_d)
    return patched, cands[:20]


def marker_name(idx):
    """The marker lump a DEPTH looks up: MTXLEV00, MTXLEV01, ... (8 chars, Doom's max).

    Per level on purpose: wad_check_lump searches backward, so one shared name would
    resolve to the last WAD loaded at every depth.
    """
    return "MTXLEV%02d" % idx


def level_lump(idx, screen, src, patched, map_name):
    """The marker the engine reads. Text, so it is readable in a hex dump and by eye."""
    lines = ["MTXLEVEL", "idx=%d" % idx,
             "map=%s" % (map_name or "MAP01"),
             "screen=%s" % screen,
             "src=%s" % (src or "IWAD"), "faces=%d" % len(patched)]
    return ("\n".join(lines) + "\n").encode("ascii")


def write_wad(path, lumps):
    off = 12
    body = b""
    for _, d in lumps:
        off += (len(d) + 3) & ~3
    with open(path, "wb") as fh:
        fh.write(struct.pack("<4sii", b"PWAD", len(lumps), off))
        for _, d in lumps:
            fh.write(d)
            fh.write(b"\0" * (((len(d) + 3) & ~3) - len(d)))
        o = 12
        for n, d in lumps:
            fh.write(struct.pack("<ii8s", o, len(d), n.encode("ascii")))
            o += (len(d) + 3) & ~3


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--iwad", default="verify/base/DOOM2.WAD")
    ap.add_argument("--loader", default="doom_ace/loader/loader.wad")
    ap.add_argument("--out", default="verify/levels")
    args = ap.parse_args(argv)

    _, loader_lumps = read_wad(args.loader)
    have = dict(loader_lumps)
    missing = [n for n in EXPLOIT_LUMPS if n not in have]
    if missing:
        raise SystemExit("loader.wad is missing exploit lumps: %s" % missing)
    exploit = [(n, have[n]) for n in EXPLOIT_LUMPS]
    ref = {n: hashlib.sha256(d).hexdigest() for n, d in exploit}

    _, iwad_lumps = read_wad(args.iwad)
    iwad_tex = iwad_texture_names(iwad_lumps)
    os.makedirs(args.out, exist_ok=True)

    # A screen texture the IWAD does not define is a silent failure: the map still
    # loads, the wall is drawn as "unknown texture", and the level simply never
    # paints the nest. Fail at BUILD time instead, with the name in the message.
    for spec in LEVELS:
        if spec["src"] and spec["screen"] not in iwad_tex:
            raise SystemExit("level %d: screen texture %s is not defined by the IWAD "
                             "(engine index would not exist)" % (spec["idx"], spec["screen"]))
        if spec["src"]:
            print("level %d screen %-8s -> engine texture index %d"
                  % (spec["idx"], spec["screen"], iwad_tex.index(spec["screen"])))

    print("exploit lumps (must be byte-identical in every level WAD):")
    for n in EXPLOIT_LUMPS:
        print("  %-9s %7d B  sha256 %s" % (n, len(have[n]), ref[n][:16]))

    summaries = []
    for spec in LEVELS:
        lumps = list(exploit)
        patched, cands = [], []
        src_map = None

        if spec["src"]:
            src_map = find_map(iwad_lumps, spec["src"])
            used = texture_names(src_map["SIDEDEFS"])
            if spec["screen"] in used:
                raise SystemExit("level %d: %s already used in %s"
                                 % (spec["idx"], spec["screen"], spec["src"]))
            patched, cands = plant_screens(src_map, spec["screen"], spec["faces"])
            # the map MARKER is a zero-size lump NAMED AFTER THE MAP, then the ten
            # lumps in Doom's order; renaming THINGS to the map name would leave the
            # map with no THINGS at all and every lump one place off.
            assert spec["map"] == spec["src"], "a level's map keeps the name it borrows"
            lumps.append((spec["map"], b""))
            for n in MAP_LUMPS:
                lumps.append((n, src_map[n]))
        lumps.append((marker_name(spec["idx"]),
                      level_lump(spec["idx"], spec["screen"], spec["src"], patched,
                                 spec["map"])))

        path = os.path.join(args.out, "level%d.wad" % spec["idx"])
        write_wad(path, lumps)

        # read it back with a fresh reader: the file on disk is the thing that ships
        sig, back = read_wad(path)
        back_d = dict(back)
        for n in EXPLOIT_LUMPS:
            assert hashlib.sha256(back_d[n]).hexdigest() == ref[n], "exploit lump %s changed" % n
        if src_map:
            assert back_d[spec["map"]] == b"", "map marker lump is not empty"
            for n in MAP_LUMPS:
                assert back_d[n] == src_map[n], "map lump %s does not round-trip" % n
            got = texture_names(back_d["SIDEDEFS"])
            assert spec["screen"] in got, "screen texture not in the written sidedefs"
        marker = back_d[marker_name(spec["idx"])].decode("ascii")
        assert "screen=%s" % spec["screen"] in marker
        assert "map=%s" % (spec["map"] or "MAP01") in marker
        # no level may carry the SHARED marker name: with a backward search it would
        # shadow every per-level marker
        assert "MTXLEVEL" not in back_d, "a shared marker name would shadow every level"

        summaries.append(dict(spec=spec, path=path, size=os.path.getsize(path),
                              lumps=len(back), patched=patched, cands=cands, marker=marker))

    for s in summaries:
        spec, patched = s["spec"], s["patched"]
        print("\n%s  (%d lumps, %d B)  %s" % (s["path"], s["lumps"], s["size"], spec["note"]))
        print("  screen: %s   map lump: %s   geometry: %s"
              % (spec["screen"], spec["map"] or "(none: IWAD MAP01, untouched)",
                 spec["src"] or "IWAD MAP01"))
        if patched:
            print("  planted on %d one-sided walls nearest the start:" % len(patched))
            for p in patched:
                print("    line %4d side %3d  %-8s -> %-8s  %5d u from start, %4d u long"
                      % (p["line"], p["side"], p["old"], p["new"], p["dist"], p["length"]))
        else:
            print("  no map: the root keeps the IWAD's MAP01; its screen surface was")
            print("  chosen by measurement (see the diegetic log), not planted here.")
    print("\nall level WADs verified: exploit byte-identical, maps round-trip under "
          "their own names, screens present in the written sidedefs, per-level markers")
    return 0


if __name__ == "__main__":
    sys.exit(main())
