"""build.py - builds the Doomception article into docs/index.html, one self-contained page.

    python3 nested/docs/doomception-src/build.py

What you edit:
    body.html     the text and the page's structure
    v2.css        the article's own styles
    v2.js         the hero, the 3D scene ("How it works") and its legend
    anat.js       the Anatomy figures (a depth, a switch, the pictures)
    drawview.js   the little procedural game view the scenes draw
What it is built on (rarely edited): parts/head.html (fonts, colours, base styles),
parts/base.css, parts/scene.css, parts/helpers.js, parts/k3d.js (the 3D toolkit).
The numbers on the page come from ../mtx-run.json. three.js r128 is loaded from cdnjs.
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
R = lambda *p: open(os.path.join(HERE, *p), encoding="utf-8").read()

META = """<title>Doomception</title>
<meta name="description" content="Three Doom games running inside one DOOM2.EXE, each showing the next on a wall: how it works, and a challenge to write the switch yourself.">
<meta property="og:type" content="article">
<meta property="og:title" content="Doomception">
<meta property="og:description" content="Three Doom games in one DOOM2.EXE. Each one shows the next on a wall.">
<meta property="og:url" content="https://kodoque1.github.io/doom_ace/">
<meta property="og:image" content="https://kodoque1.github.io/doom_ace/og.png">
<meta property="og:image:width" content="1200">
<meta property="og:image:height" content="630">
<meta name="twitter:card" content="summary_large_image">
<meta name="twitter:creator" content="@KODOQUE">
<meta name="twitter:image" content="https://kodoque1.github.io/doom_ace/og.png">"""

head = R("parts", "head.html").replace("<title>DOOMATRYOSHKA</title>", META)
style = R("parts", "base.css") + R("parts", "scene.css") + R("v2.css")
js = "\n".join([R("parts", "helpers.js"), R("parts", "k3d.js"), R("drawview.js"), R("v2.js"), R("anat.js")]) + "\n})();\n"
run = R("..", "mtx-run.json").replace("</", "<\\/")

page = ("<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">\n"
        + head + style + "</style>\n</head>\n<body>\n" + R("body.html")
        + '\n<script type="application/json" id="mtxrun">' + run + "</script>\n"
        + '<script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>\n'
        + "<script>" + js + "</script>\n</body>\n</html>\n")
out = os.path.join(ROOT, "docs", "index.html")
with open(out, "w", encoding="utf-8") as f:
    f.write(page)
print("wrote %s (%d bytes)" % (os.path.relpath(out, ROOT), len(page)))
