#!/bin/bash
# depth_vs_rate.sh - how the frame rate depends on the NUMBER OF INSTANCES.
#
# WHY. The ring runs three instances and the root advances 16 frames/s. Two models:
#   SHARED CLOCK  the engine's own rate is 3x that and the ring divides it: 1 instance
#                 -> 48/s, 2 -> 24/s, 3 -> 16/s   (a clean 1/N)
#   OWN LOOP      the engine's own rate is 16/s and the ring does not divide it:
#                 1, 2 and 3 instances all give 16/s
# The engine scales its depth count to the memory it finds (its own "ladder", and the
# log line "host up: depths N" reports what it decided), so `memsize` is the knob.
# MTX_LEVELS=0 does NOT work for this: the depths get their maps from the loaded WADs,
# so the ring came up at "depths 3" anyway (measured - both runs 16.00/s, identical).
#
# The [dosbox] section is RESTATED IN FULL, not just memsize: a later section replaces
# the earlier one, so leaving out machine= would silently drop svga_s3.
set -u
ROOT="$HOME/Documents/recursive_doom"; V="$ROOT/verify"
SECS="${SECS:-45}"
OUT="$V/depth_vs_rate.out"
: > "$OUT"

run() { # run <label> <memsize>
  local label="$1" ms="$2"
  echo "### $label  (memsize=$ms)" >> "$OUT"
  export MTX_CONF_EXTRA=$'[dosbox]\nmachine=svga_s3\nmemsize='"$ms"
  "$V/fps_probe.sh" "dep-$label" "$SECS" >/dev/null 2>&1
  sed -n 1p "$V/dep-$label.fps.txt" >> "$OUT"
  grep -aE "host up: depths|refuses|arena too small|boot failed" "$V/rec-dep-$label/MTX.LOG" 2>/dev/null | head -2 >> "$OUT"
  python3 - "$V/dep-$label.fps.txt" "$OUT" <<'PY'
import re, sys
rows = []
for l in open(sys.argv[1]):
    m = re.match(r'\s+(\d+)\s+(\d+)\s+(\d+)\.(\d+)', l)
    if m:
        rows.append((int(m.group(1)), int(m.group(2)), float(f"{m.group(3)}.{m.group(4)}")))
out = open(sys.argv[2], "a")
if rows:
    t = sorted(r[2] for r in rows[-10:])
    out.write(f"  windows={len(rows)} last t={rows[-1][0]}s frame={rows[-1][1]}"
              f"  MEDIAN last {len(t)}: {t[len(t)//2]:.2f} frames/s\n")
else:
    out.write("  no samples in the window\n")
out.close()
PY
  for p in $(pgrep -x dosbox); do kill -9 $p 2>/dev/null; done
  sleep 2
}

run m16 16
run m24 24
run m63 63
echo "done" >> "$OUT"
