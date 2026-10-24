#!/usr/bin/env python3
"""demo_cards.py <clip.avi> <outdir> - the demo video's title card, caption bars and
closing ring diagram, in Doom's own HUD font (STCFN) on Doom II story-screen flats,
both read from the IWAD at run time (doomtext.py). The ring diagram uses real frames
of the clip, one per depth, at the times given by MTX_RING_T (default "1 14 23")."""
import math, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import doomtext as d
from PIL import Image, ImageDraw

clip, O = sys.argv[1], sys.argv[2]
os.makedirs(O, exist_ok=True)
W, H = 1280, 1080

d.card([('DOOMATRYOSHKA', 9, 'red', 300), ('A RING OF DOOM II INSTANCES', 4, 'gold', 470),
        ("EACH ONE'S WALL IS A LIVE SCREEN", 3, 'white', 590), ('SHOWING THE NEXT ONE', 3, 'white', 640),
        ('AND THE LAST SHOWS THE FIRST', 3, 'white', 690)], os.path.join(O, 'title.png'))

caps = [("DEPTH 0", "ITS WALL IS A LIVE SCREEN SHOWING DEPTH 1"),
        ("INTO DEPTH 1", "ITS SCREEN SHOWS DEPTH 2, WHICH SHOWS DEPTH 0..."),
        ("INTO DEPTH 2", "ITS SCREEN SHOWS DEPTH 0 - THE RING CLOSES"),
        ("BACK IN DEPTH 0", "THREE DOOMS, EACH INSIDE THE NEXT")]
for i, (a, b) in enumerate(caps):
    bar = d.flat_bg('RROCK14', (W, 120), dim=0.45).convert('RGBA')
    ta, tb = d.text(a, 4, 'red'), d.text(b, 3, 'gold')
    bar.alpha_composite(ta, ((W - ta.width) // 2, 14))
    bar.alpha_composite(tb, ((W - tb.width) // 2, 70))
    bar.save(os.path.join(O, 'cap%d.png' % i))

frames = []
for k, t in enumerate(os.environ.get('MTX_RING_T', '1 14 23').split()):
    p = os.path.join(O, 'ring%d.png' % k)
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-ss', t, '-i', clip, '-frames:v', '1', p], check=True)
    frames.append(p)

img = d.flat_bg('RROCK14', (W, H), dim=0.35).convert('RGBA')
t = d.text('THE RING', 7, 'red'); img.alpha_composite(t, ((W - t.width) // 2, 40))
fw, fh = 400, 250
centers = [(W // 2, 330), (W // 2 + 330, 760), (W // 2 - 330, 760)]
dr = ImageDraw.Draw(img)

def arrow(p, q, col=(220, 30, 20)):
    dx, dy = q[0] - p[0], q[1] - p[1]; L = math.hypot(dx, dy); ux, uy = dx / L, dy / L
    a = (p[0] + ux * 230, p[1] + uy * 175); b = (q[0] - ux * 230, q[1] - uy * 175)
    dr.line([a, b], fill=col, width=10)
    for s in (+1, -1):
        ang = math.atan2(uy, ux) + s * 2.6
        dr.line([b, (b[0] + 28 * math.cos(ang), b[1] + 28 * math.sin(ang))], fill=col, width=10)
    return ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)

mids = [arrow(centers[i], centers[(i + 1) % 3]) for i in range(3)]   # depth i shows depth i+1
for i, (cx, cy) in enumerate(centers):
    f = Image.open(frames[i]).convert('RGBA').resize((fw, fh), Image.NEAREST)
    dr.rectangle([cx - fw // 2 - 6, cy - fh // 2 - 6, cx + fw // 2 + 5, cy + fh // 2 + 5], fill=(255, 210, 80))
    img.alpha_composite(f, (cx - fw // 2, cy - fh // 2))
for (mx, my), off in zip(mids, [(40, -40), (0, 40), (-40, -40)]):
    t = d.text('SHOWS', 3, 'gold')
    img.alpha_composite(t, (int(mx - t.width / 2 + off[0]), int(my - t.height / 2 + off[1])))
for s, sc, col, y in [('THREE ENGINE INSTANCES, ONE DOS PROCESS', 3, 'white', 960),
                      ('STOCK DOOM2.EXE V1.9 IN DOSBOX', 2, 'grey', 1010)]:
    t = d.text(s, sc, col); img.alpha_composite(t, ((W - t.width) // 2, y))
img.convert('RGB').save(os.path.join(O, 'end.png'))
