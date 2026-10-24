#!/bin/bash
# tic_patch_test.sh - does RAISING THE TIC RATE raise the ring's frame rate?
#
# THE HYPOTHESIS UNDER TEST. The ring's pace does not respond to the emulated CPU's speed
# (measured: cpu_cycles=250000 gives the same 16.00 frames/s as max), so it is a clock.
# If that clock is the game's tic rate, then raising the tic rate raises the frame rate,
# and the judder is fixable in our own code rather than in the emulator.
#
# THE PATCH. verify/after-fast/DOOM2.EXE is the shipped image with exactly two bytes
# changed - the timer divisor at file offset 0x25fd, 2130 (0x0852) -> 710 (0x02c6):
#
#   25f9: c7 06 20 00 52 08   movw $0x852,[0x20]   ; 1193182/2130 = 560 Hz, /16 = 35 tics
#   ...                      patched to 710     ; 1193182/710  = 1680 Hz, /16 = 105 tics
#
# So the guest's clock runs 3x faster in wall time. Nothing else in the image differs
# (verified: 2 bytes changed in the whole 709,905-byte file).
#
# WHAT EACH OUTCOME MEANS
#   frame rate rises ~3x  -> the tic clock WAS the gate; the fix is ours, and the machine
#                            then caps it (predicted ~96 frames/s total = ~32/s per instance)
#   frame rate unchanged  -> the pace is not the tic clock; the PIT is the wrong lever and
#                            the gate is elsewhere in the engine's own loop
# Stock is run BEFORE and AFTER the patched run: host load drifts (measured: the same build
# gave 16.0 frames/s early in a session and 8-12/s later), so an unbracketed A/B is not evidence.
set -u
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"; V="$ROOT/verify"
SECS="${SECS:-50}"
OUT="$V/tic_patch_test.out"
: > "$OUT"

run() { # run <label> <MTX_REF>
  local label="$1" ref="$2"
  echo "### $label   (MTX_REF=$ref)" >> "$OUT"
  MTX_REF="$ref" "$V/fps_probe.sh" "tic-$label" "$SECS" >/dev/null 2>&1
  sed -n 1p "$V/tic-$label.fps.txt" >> "$OUT"
  grep -aE "host up: depths|refuses|boot failed|transport depth" "$V/rec-tic-$label/MTX.LOG" 2>/dev/null | head -3 >> "$OUT"
  python3 - "$V/tic-$label.fps.txt" "$OUT" <<'PY'
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

run stock1 "$ROOT/verify/after"
run patched "$ROOT/verify/after-fast"
run stock2 "$ROOT/verify/after"
echo "done" >> "$OUT"
