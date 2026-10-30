"""Render text in Doom's own HUD font (STCFN lumps) and Doom-style card backgrounds."""
import struct, sys
from PIL import Image

import os
WAD = os.environ.get("MTX_IWAD", os.path.join(os.environ.get("MTX_ROOT", os.path.expanduser("~/Documents/recursive_doom")), "verify/after/DOOM2.WAD"))

def lumps(path):
    d = open(path, "rb").read()
    n, off = struct.unpack("<ii", d[4:12])
    out = {}
    for i in range(n):
        o, s, nm = struct.unpack("<ii8s", d[off + 16 * i: off + 16 * i + 16])
        out.setdefault(nm.rstrip(b"\0").decode("latin1"), d[o:o + s])
    return out

L = lumps(WAD)
PAL = L["PLAYPAL"][:768]
RGB = [tuple(PAL[3 * i:3 * i + 3]) for i in range(256)]

def patch(data):
    w, h, lo, to = struct.unpack("<hhhh", data[:8])
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = img.load()
    for x in range(w):
        p = struct.unpack("<I", data[8 + 4 * x: 12 + 4 * x])[0]
        while data[p] != 0xFF:
            top, n = data[p], data[p + 1]
            for j in range(n):
                px[x, top + j] = RGB[data[p + 3 + j]] + (255,)
            p += n + 4
    return img

GLYPH, YOFF = {}, {}
for c in range(33, 96):
    nm = "STCFN%03d" % c
    if nm in L:
        GLYPH[chr(c)] = patch(L[nm])
        YOFF[chr(c)] = -struct.unpack("<h", L[nm][6:8])[0]   # Doom draws at y - topoffset

TINTS = {
    "red": None,                       # Doom's own colour
    "white": (255, 255, 255),
    "gold": (255, 210, 80),
    "green": (110, 255, 110),
    "grey": (170, 170, 170),
}

def tint(img, t):
    if TINTS[t] is None:
        return img
    r0, g0, b0 = TINTS[t]
    out = img.copy(); px = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = px[x, y]
            if a:
                k = max(r, g, b) / 255.0          # keep the glyph's own shading
                k = 0.35 + 0.65 * k
                px[x, y] = (int(r0 * k), int(g0 * k), int(b0 * k), a)
    return out

def text(s, scale=3, color="red", shadow=True):
    s = s.upper()
    h = max(g.height + YOFF[c] for c, g in GLYPH.items())
    w = sum(GLYPH[c].width if c in GLYPH else 4 for c in s) + len(s)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    x = 0
    for c in s:
        if c in GLYPH:
            img.alpha_composite(tint(GLYPH[c], color), (x, YOFF[c]))
            x += GLYPH[c].width + 1
        else:
            x += 5
    img = img.resize((w * scale, h * scale), Image.NEAREST)
    if shadow:
        sh = Image.new("RGBA", img.size, (0, 0, 0, 0))
        sh.paste((0, 0, 0, 255), (0, 0), img)
        out = Image.new("RGBA", (img.width + scale, img.height + scale), (0, 0, 0, 0))
        out.alpha_composite(sh, (scale, scale))
        out.alpha_composite(img, (0, 0))
        img = out
    return img

def flat_bg(name, size, dim=0.35, scale=4):
    raw = L[name]
    tile = Image.new("RGB", (64, 64))
    tile.putdata([RGB[b] for b in raw[:4096]])
    tile = tile.resize((64 * scale, 64 * scale), Image.NEAREST)
    bg = Image.new("RGB", size)
    for y in range(0, size[1], tile.height):
        for x in range(0, size[0], tile.width):
            bg.paste(tile, (x, y))
    return bg.point(lambda v: int(v * dim))

def card(lines, out, size=(1280, 1080), bg="RROCK14"):
    """lines: [(text, scale, colour, y)] ; centred."""
    img = flat_bg(bg, size).convert("RGBA")
    for s, sc, col, y in lines:
        t = text(s, sc, col)
        img.alpha_composite(t, ((size[0] - t.width) // 2, y))
    img.convert("RGB").save(out)

def overlay(lines, out, size=(1280, 1080)):
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    for s, sc, col, x, y in lines:
        t = text(s, sc, col)
        if x == "c":
            x = (size[0] - t.width) // 2
        elif isinstance(x, tuple):          # centred on a column
            x = x[0] - t.width // 2
        img.alpha_composite(t, (x, y))
    img.save(out)
