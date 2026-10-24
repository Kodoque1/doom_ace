#!/bin/bash
# cycles_sweep.sh - is the ring CPU-bound, or is it waiting on the clock?
#
# WHY THIS EXISTS. "The host CPU sits at 99.9% during a ring run" was read as proof
# that the guest is CPU-bound. It proves nothing of the sort on its own: Doom's main
# loop BUSY-WAITS on the timer (it has no sleep - that is why a 486 was pegged too),
# so a pegged host core is equally consistent with a guest that spends its time
# spinning on a timer it cannot advance. The two have opposite fixes.
#
# THE DISCRIMINATOR. Cap the emulated CPU with cpu_cycles (and cpu_cycles_protected -
# Doom is DOS/4GW protected mode, and that setting is separate; with cpu_cycles=max
# the protected setting is forced to 'auto', so a cap must be stated for BOTH).
#
#   rate SCALES DOWN with the cap  -> the machine's instruction throughput is the
#                                     binding constraint: real, useful work
#   rate FLAT while the cap drops  -> the guest is not short of instructions, it is
#                                     gated on time; the CPU has headroom and the
#                                     clock (tic rate) is what to fix
#
# The sweep is descending, and a second baseline run closes it: host load drifts
# (measured: the SAME build gave 16.0 frames/s earlier and 8-12/s later in one
# session), so an unbracketed before/after comparison is not evidence.
set -u
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"; V="$ROOT/verify"
SECS="${SECS:-30}"
OUT="$V/cycles_sweep.out"
: > "$OUT"
say() { echo "$@" | tee -a "$OUT"; }

run() { # run <label> <cycles|max>
  local label="$1" cyc="$2"
  if [ "$cyc" = max ]; then
    unset MTX_CONF_EXTRA
  else
    export MTX_CONF_EXTRA=$'[cpu]\ncpu_cycles='"$cyc"$'\ncpu_cycles_protected='"$cyc"
  fi
  local rep="$V/cyc-$label.fps.txt" d="$V/rec-cyc-$label"
  say "### $label (cpu_cycles=$cyc)"
  "$V/fps_probe.sh" "cyc-$label" "$SECS" >/dev/null 2>&1
  # The report states its own conditions; echo them, so this file can contradict a
  # later claim about how the numbers were produced.
  say "  probe: $(sed -n 1p "$rep" 2>/dev/null)"
  say "  dosbox: $(grep -aiE 'CPU: (Switched|Invalid|Using)|cycles' "$d/db.log" 2>/dev/null | tail -1)"
  say "  steady state (one-second windows, last 8):"
  grep -E "^ +[0-9]+ +[0-9]+ +[0-9]+\.[0-9]" "$rep" 2>/dev/null | tail -8 | sed 's/^/   /' | tee -a "$OUT" >/dev/null
  python3 - "$rep" <<'PY' | tee -a "$OUT"
import re, sys
rows = []
for l in open(sys.argv[1]):
    m = re.match(r'\s+(\d+)\s+(\d+)\s+(\d+)\.(\d+)', l)
    if m:
        rows.append((int(m.group(2)), float(f"{m.group(3)}.{m.group(4)}")))
tail = [r[1] for r in rows[-8:]]
if tail:
    tail.sort()
    med = tail[len(tail)//2]
    print(f"  -> MEDIAN last {len(tail)} windows: {med:.2f} frames/s   (root frame counter reached {rows[-1][0]})")
else:
    print("  -> no samples (guest produced no frames in the window)")
PY
  for p in $(pgrep -x dosbox); do kill -9 $p 2>/dev/null; done
  sleep 2
}

say "cycles_sweep: ${SECS}s per run, descending cap, baseline repeated at the end"
run baseline max
run c2000k  2000000
run c500k   500000
run c125k   125000
run c30k    30000
run baseline2 max
say "done -> $OUT"
