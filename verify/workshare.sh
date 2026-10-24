#!/bin/bash
# workshare.sh - how much of the frame period does the ring's own work occupy?
#
# WHY THIS DECIDES THE NEXT MOVE. Everything measured so far says the ring's frame rate is
# welded to the game's tick counter (1:1, ~16/s), the pace is in emulated time, and no CPU
# speed change moves it. That leaves exactly two possibilities, and they need opposite work:
#
#   the work is a small part of the ~62 ms period -> the ring WAITS most of the time; the
#       lever is whatever it waits on, and there is headroom for a much higher frame rate
#   the work is most of the period             -> the ring is work-bound; the lever is in
#       the ring's own per-frame code (hooks, paint, self-check, the switch itself)
#
# The engine reads the PIT counter (1.193182 MHz, emulated) at the start of the frame
# policy and again at the log point, and reports the difference as counts=.
set -u
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"; V="$ROOT/verify"
SECS="${SECS:-40}"
OUT="$V/workshare.out"
: > "$OUT"

export MTX_SWITCHES=0 MTX_NOCAP=1
( "$V/record.sh" work "$((SECS + 20))" --nobuild >"$V/rec-work.probe.out" 2>&1 ) &
RP=$!
sleep "$SECS"
for p in $(pgrep -x dosbox); do kill -9 $p 2>/dev/null; done
wait $RP 2>/dev/null

python3 - "$V/rec-work/MTX.LOG" "$OUT" <<'PY' | tee -a "$OUT"
import re, sys, collections
txt = open(sys.argv[1], errors='ignore').read()
o = open(sys.argv[2], "a")
w = [int(x) for x in re.findall(r'\[MTX\] work frame \d+ counts=(\d+)', txt)]
if not w:
    o.write("no work lines - engine not rebuilt?\n"); o.close(); raise SystemExit
PIT_HZ = 1193182.0
w.sort()
med = w[len(w)//2]
o.write(f"work samples: {len(w)}   counts: min {w[0]}, median {med}, max {w[-1]}\n")
o.write(f"  median work duration in EMULATED time: {med/PIT_HZ*1000:.3f} ms\n")
# the frame period, from the same run's tick/bios evidence if available
o.write(f"  frame period (from the earlier BIOS-tick measurement): ~62.0 ms\n")
o.write(f"  => the work occupies about {med/PIT_HZ*1000/62.0*100:.1f}% of the period\n")
if med/PIT_HZ*1000 < 62.0 * 0.5:
    o.write("  => THE RING WAITS MOST OF THE TIME: the pace is a wait, not the work, and\n"
            "     there is headroom for a much higher frame rate once the wait is found\n")
else:
    o.write("  => THE RING IS WORK-BOUND: the pace is the ring's own per-frame cost\n")
o.close()
PY
