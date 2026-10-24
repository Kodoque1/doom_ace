#!/bin/bash
# gametic_probe.sh - is the frame rate gated by Doom's TIC CLOCK, or not?
#
# WHY THIS AND NOT MORE BINARY ARCHAEOLOGY. The 3x timer-divisor patch changed the frame
# rate by nothing (16.00 / 16.00 / 16.00, bracketed). But that patch's own effect on the
# guest clock could not be confirmed: the block it lives in READS the PIT counter
# (in al,0x40 at file 0x25d0/0x25d5) and stores a divisor in a variable, so the patched
# constant may be a delay constant rather than the live tick divisor. An inconclusive
# lever is not a falsified hypothesis.
#
# THE DIRECT QUESTION: how many TICS does the engine advance per frame?
#   tics/frame = 1  -> each frame IS a tic, so the frame rate = the tic rate: the clock
#                      is the gate, and raising the tic rate raises the frame rate
#   tics/frame > 1  -> the engine is rendering SLOWER than the clock allows, the clock is
#                      not the gate, and a tic patch cannot help
# The frame counter alone cannot tell these apart - a frame that repeats a tic looks
# identical to one that advanced. So the self-check line now carries Doom's own `gametic`
# and `leveltime` beside `frame`, and this script reads the ratio out of the log and
# multiplies it by the measured frame rate to get the tic rate in tics per second.
#
# The same ratio ALSO verifies the divisor patch: if the patch really moved the guest's
# clock, the patched run advances ~3x more gametics per frame.
set -u
ROOT="$HOME/Documents/recursive_doom"; V="$ROOT/verify"
SECS="${SECS:-45}"
OUT="$V/gametic_probe.out"
: > "$OUT"
say() { echo "$@" | tee -a "$OUT"; }

say "gametic_probe: building the engine with the gametic log line, then regenerating the level WADs"
# BOTH makes, exactly as record.sh does them. The engine make produces code.lmp, but the
# level WADs are packed from loader/loader.wad - and loader.wad is what actually carries
# the engine into the game. Building only the engine leaves loader.wad stale and the run
# silently executes the PREVIOUS engine: measured, a log line added to mtx_boot.c did not
# appear in the run at all (loader.wad mtime 18:15 vs code.lmp 19:04), which looks exactly
# like "the change had no effect".
( cd "$ROOT/doom_ace" && make -C engine >/dev/null 2>&1 ) || { say "ENGINE BUILD FAILED"; exit 1; }
( cd "$ROOT/doom_ace" && make -C loader >/dev/null 2>&1 ) || { say "LOADER BUILD FAILED"; exit 1; }
( cd "$ROOT" && python3 verify/mkwadlevels.py --iwad "$V/after/DOOM2.WAD" \
    --loader doom_ace/loader/loader.wad --out verify/levels >/dev/null ) || { say "LEVEL WAD BUILD FAILED"; exit 1; }
say "built."

# Re-apply the 2-byte divisor patch to a FRESH copy - the rebuild just replaced the image,
# so the old after-fast copy is stale and must not be reused.
python3 - <<'PY' | tee -a "$OUT"
import os
R = "/home/kodoque/Documents/recursive_doom"
src = f"{R}/verify/after/DOOM2.EXE"
d = bytearray(open(src, "rb").read())
off = 0x25fd
assert d[off:off+2] == b"\x52\x08", f"expected divisor 2130 at 0x25fd, found {d[off:off+2].hex()}"
d[off:off+2] = (710).to_bytes(2, "little")
os.makedirs(f"{R}/verify/after-fast", exist_ok=True)
open(f"{R}/verify/after-fast/DOOM2.EXE", "wb").write(bytes(d))
print(f"re-patched fresh image: 0x25fd 2130 -> 710 (2 bytes)")
PY

run() { # run <label> <MTX_REF>
  local label="$1" ref="$2"
  say ""
  say "### $label   (MTX_REF=$ref)"
  MTX_REF="$ref" "$V/fps_probe.sh" "g-$label" "$SECS" >/dev/null 2>&1
  say "  $(sed -n 1p "$V/g-$label.fps.txt")"
  say "  $(grep -aE 'host up: depths' "$V/rec-g-$label/MTX.LOG" | head -1)"
  python3 - "$V/g-$label.fps.txt" "$V/rec-g-$label/MTX.LOG" "$OUT" <<'PY'
import re, sys
# frame rate from the probe's own report
rows = []
for l in open(sys.argv[1]):
    m = re.match(r'\s+(\d+)\s+(\d+)\s+(\d+)\.(\d+)', l)
    if m:
        rows.append(float(f"{m.group(3)}.{m.group(4)}"))
out = open(sys.argv[3], "a")
if not rows:
    out.write("  no probe samples\n"); out.close(); raise SystemExit
t = sorted(rows[-10:]); fps = t[len(t)//2]
out.write(f"  frame rate (median of last {len(t)} windows): {fps:.2f} frames/s\n")

# tics per frame, per depth, from the self-check lines
txt = open(sys.argv[2], errors='ignore').read()
pat = re.compile(r'\[MTX\] depth (\d+) frame (\d+) gametic (\d+) leveltime (\d+):')
per = {}
for d, f, g, lt in pat.findall(txt):
    per.setdefault(int(d), []).append((int(f), int(g), int(lt)))
if not per:
    out.write("  NO self-check lines with gametic - was the engine really rebuilt?\n")
    out.close(); raise SystemExit
for d in sorted(per):
    s = per[d]
    if len(s) < 3:
        out.write(f"  depth {d}: only {len(s)} samples\n"); continue
    (f0, g0, l0), (f1, g1, l1) = s[0], s[-1]
    df, dg, dl = f1 - f0, g1 - g0, l1 - l0
    if df <= 0:
        out.write(f"  depth {d}: frame counter did not advance\n"); continue
    ratio = dg / df
    out.write(f"  depth {d}: frames {f0}->{f1} (+{df}), gametic {g0}->{g1} (+{dg}), "
              f"leveltime +{dl}  =>  {ratio:.3f} tics per frame\n")
    out.write(f"           implied TIC RATE for this instance: {fps * ratio:.1f} tics/s "
              f"(at {fps:.2f} frames/s)\n")
out.close()
PY
  for p in $(pgrep -x dosbox); do kill -9 $p 2>/dev/null; done
  sleep 2
}

run stock "$ROOT/verify/after"
run patched "$ROOT/verify/after-fast"
say ""
say "done"
