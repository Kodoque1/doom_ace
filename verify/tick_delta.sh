#!/bin/bash
# tick_delta.sh - does the tick counter advance EXACTLY one per frame, or does it vary?
#
# WHY THIS IS THE DECIDING QUESTION. Everything measured so far is consistent with two
# different worlds:
#   (a) the ENGINE DRIVES THE CLOCK - the ring advances the game's tick counter once per
#       frame it renders, so the frame rate is the tick rate by construction, and the
#       pace (16/s) comes from whatever paces the engine's loop
#   (b) AN INDEPENDENT CLOCK DRIVES THE ENGINE - the machine's timer ticks at ~16 Hz and
#       the engine renders one frame per tick, waiting for each
# A 50-frame sample cannot tell them apart: the average is 1.0 either way. The per-frame
# DELTA can:
#   every delta == 1        -> (a): the engine advances it, nothing external paces it
#   deltas of 0 and 2+      -> (b): an external clock runs, and the engine waits on it
#   deltas all 0            -> the counter is frozen and the 1.0 average was aliasing
set -u
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"; V="$ROOT/verify"
SECS="${SECS:-40}"
OUT="$V/tick_delta.out"
: > "$OUT"

export MTX_SWITCHES=0 MTX_NOCAP=1
( "$V/record.sh" delta "$((SECS + 20))" --nobuild >"$V/rec-delta.probe.out" 2>&1 ) &
RP=$!
sleep "$SECS"
for p in $(pgrep -x dosbox); do kill -9 $p 2>/dev/null; done
wait $RP 2>/dev/null

python3 - "$V/rec-delta/MTX.LOG" "$OUT" <<'PY' | tee -a "$OUT"
import re, sys, collections
txt = open(sys.argv[1], errors='ignore').read()
s = [(int(a), int(b)) for a, b in re.findall(r'\[MTX\] frame (\d+) child=\d+ t=(\d+)', txt)]
out = open(sys.argv[2], "a")
if len(s) < 20:
    out.write(f"only {len(s)} frame lines - engine not rebuilt?\n"); out.close(); raise SystemExit
out.write(f"frame lines: {len(s)}\n")
# deltas between consecutive samples, in frame order
d = collections.Counter()
prev_f = prev_t = None
for f, t in s:
    if prev_f is not None and f > prev_f:
        d[(f - prev_f, t - prev_t)] += 1
    prev_f, prev_t = f, t
out.write("delta (frames, ticks) histogram, top 10:\n")
for (df, dt), n in d.most_common(10):
    out.write(f"   +{df} frames -> +{dt} ticks : {n} times\n")
tick_deltas = collections.Counter()
prev_f = prev_t = None
for f, t in s:
    if prev_f is not None and f - prev_f == 1:
        tick_deltas[t - prev_t] += 1
    prev_f, prev_t = f, t
out.write(f"\nticks advanced per SINGLE frame: {dict(sorted(tick_deltas.items()))}\n")
if tick_deltas:
    ones = tick_deltas.get(1, 0)
    tot = sum(tick_deltas.values())
    out.write(f"   exactly 1 tick on {ones}/{tot} frames ({100.0*ones/tot:.1f}%)\n")
    if ones == tot:
        out.write("   => the engine advances the counter itself; no external clock paces it\n")
    else:
        out.write("   => an INDEPENDENT clock is running and the engine waits on it\n")
out.close()
PY
