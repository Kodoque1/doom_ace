#!/bin/bash
# intra.sh - does the game's tick counter move WHILE a frame is being computed?
#
# WHY THIS IS THE LAST QUESTION. The counter advances exactly one per frame (measured),
# which is consistent with two different mechanisms, and they have different fixes:
#   IRQ0 drives it   -> the machine's timer interrupt is live, and it fires ~16 times a
#                       second instead of the 35 the PIT was programmed for; the lever is
#                       the interrupt source
#   the engine does  -> the counter only ever moves at a frame boundary, so the ring's own
#                       frame path advances it and the lever is the engine's loop (our code)
#
# The frame policy now samples the counter at the start of the frame's work and again at
# the log point, and reports the difference as d=. An interrupt firing mid-frame shows up
# as d >= 1 on SOME frames and 0 on others (interrupts arrive at arbitrary phases). An
# engine bump shows up as a constant.
set -u
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"; V="$ROOT/verify"
SECS="${SECS:-40}"
OUT="$V/intra.out"
: > "$OUT"

export MTX_SWITCHES=0 MTX_NOCAP=1
( "$V/record.sh" intra "$((SECS + 20))" --nobuild >"$V/rec-intra.probe.out" 2>&1 ) &
RP=$!
sleep "$SECS"
for p in $(pgrep -x dosbox); do kill -9 $p 2>/dev/null; done
wait $RP 2>/dev/null

python3 - "$V/rec-intra/MTX.LOG" "$OUT" <<'PY' | tee -a "$OUT"
import re, sys, collections
txt = open(sys.argv[1], errors='ignore').read()
o = open(sys.argv[2], "a")
d = [int(x) for x in re.findall(r'\[MTX\] intra frame \d+ d=(\d+)', txt)]
if not d:
    o.write("no intra lines - engine not rebuilt?\n"); o.close(); raise SystemExit
c = collections.Counter(d)
o.write(f"intra-frame tick deltas over {len(d)} samples: {dict(sorted(c.items()))}\n")
nz = sum(v for k, v in c.items() if k >= 1)
o.write(f"  frames where the counter moved DURING the frame's work: {nz}/{len(d)}"
        f" ({100.0*nz/len(d):.1f}%)\n")
if nz == 0:
    o.write("  => it NEVER moves mid-frame: the counter is advanced at a frame boundary,\n"
            "     i.e. by the ring's own frame path. The lever is the engine's loop.\n")
else:
    o.write("  => it DOES move mid-frame: IRQ0 is live and firing while the guest runs,\n"
            "     so the interrupt source is the lever (and it is running well below the\n"
            "     35 Hz the PIT was programmed for).\n")
o.close()
PY
