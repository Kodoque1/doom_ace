#!/bin/bash
# solo_vs_ring.sh - the engine's OWN frame rate vs the ring's, measured in ONE session.
#
# WHY. The ring produces 16 frames/s per instance (48/s total) and does not respond to
# the emulated machine's speed (measured: cpu_cycles=250000 gives the same 16.00 as max).
# So the pace is a clock. But WHICH clock? Two candidates, and they have different fixes:
#   - the engine's own frame/tic loop is 16/s and the ring does not divide it
#   - the engine's own loop is 3x that and the ring's three instances divide it
# Staging only level0.wad removes the depths (they find their level by opening
# level<d>.wad), so the root runs alone and its rate IS the engine's own rate.
# Both runs are in this one session, because host load drifts ~40% between sessions.
set -u
ROOT="$HOME/Documents/recursive_doom"; V="$ROOT/verify"
SECS="${SECS:-60}"
OUT="$V/solo_vs_ring.out"
: > "$OUT"

run() { # run <label> <MTX_LEVELS>
  local label="$1" lv="$2"
  echo "### $label  (MTX_LEVELS=$lv)" >> "$OUT"
  MTX_LEVELS="$lv" "$V/fps_probe.sh" "$label" "$SECS" >/dev/null 2>&1
  sed -n 1p "$V/$label.fps.txt" >> "$OUT"
  # the ring's own evidence of how many instances came up
  grep -aE "host up|depths|no code image|boot failed|instance live" "$V/rec-$label/MTX.LOG" 2>/dev/null | head -3 >> "$OUT"
  python3 - "$V/$label.fps.txt" "$OUT" <<'PY'
import re, sys
rows = []
for l in open(sys.argv[1]):
    m = re.match(r'\s+(\d+)\s+(\d+)\s+(\d+)\.(\d+)', l)
    if m:
        rows.append((int(m.group(1)), int(m.group(2)), float(f"{m.group(3)}.{m.group(4)}")))
out = open(sys.argv[2], "a")
if rows:
    t = sorted(r[2] for r in rows[-10:])
    out.write(f"  windows={len(rows)}  last t={rows[-1][0]}s frame={rows[-1][1]}"
              f"  MEDIAN last {len(t)}: {t[len(t)//2]:.2f} frames/s\n")
else:
    out.write("  no samples in the window\n")
out.close()
PY
  for p in $(pgrep -x dosbox); do kill -9 $p 2>/dev/null; done
  sleep 2
}

run solo 0
run ring "0 1 2"
echo "done" >> "$OUT"
