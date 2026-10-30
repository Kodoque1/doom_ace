"""pack.py - the browser page's disk: every file the page's cc, as, check and machine
read, as one JSON object of path -> base64 (binary) or text."""
import base64, json, os, sys

ROOT = os.path.join(os.path.dirname(__file__), "..")
files = {}

def text(path, as_path=None):
    with open(os.path.join(ROOT, path)) as f:
        files[as_path or path] = {"t": f.read()}

def binary(path, as_path):
    with open(os.path.join(ROOT, path), "rb") as f:
        files[as_path] = {"b": base64.b64encode(f.read()).decode()}

# what the machine boots from
binary("build/disk/game.o", "build/disk/game.o")
for n in range(1, 9):
    binary("build/disk/level%d.o" % n, "build/disk/level%d.o" % n)
# what cc and as need to build level 0 from your sched.c
for p in ["kit/kit.h", "kit/kit.c", "game/game.h", "game/script.h", "you/sched.c",
          "game/tinydoom.c", "MACHINE.md", "build/game.inc", "build/game.sym",
          "build/game.s", "build/game.map"]:
    text(p)
with open(sys.argv[1], "w") as f:
    json.dump(files, f, separators=(",", ":"))
print("packed %d files" % len(files))
