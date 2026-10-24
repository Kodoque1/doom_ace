#!/bin/bash
# rawclock.sh - I_GetTime() versus the game's raw tick counter, read straight from memory.
#
# WHY. The engine's trampolines are REWRITTEN at load time - asm.S says of its own
# I_FinishUpdate placeholder "this address will be overwritten by init_draw()". So the
# question "is the tick rate 16/s because the machine's clock is 16 Hz, or because the
# engine substituted its own clock?" cannot be answered by calling I_GetTime() alone: that
# call may not be the game's function any more.
#
# The frame line now carries BOTH:
#   t=    I_GetTime()                              (whatever that resolves to at runtime)
#   raw=  *(uint32_t*)0x2913c                      (the game's tick counter, read directly)
#
#   0x2913c is the address the game's own I_GetTime reads: at file 0x5bc20,
#     a1 3c 91 02 00   mov eax,[0x2913c] ; c3 ret
#   and the IRQ0 handler (DPMI vector 8) increments it and EOIs the PIC.
#
#   t and raw advance together -> I_GetTime IS the game's counter; the machine's clock
#                                 really is ~16 Hz and the ring waits on it
#   raw runs faster than t     -> the engine substituted its own clock; the pace is the
#                                 engine's own loop rate, and the game's clock is not it
set -u
ROOT="$HOME/Documents/recursive_doom"; V="$ROOT/verify"
SECS="${SECS:-40}"
OUT="$V/rawclock.out"
: > "$OUT"

export MTX_SWITCHES=0 MTX_NOCAP=1
( "$V/record.sh" raw "$((SECS + 20))" --nobuild >"$V/rec-raw.probe.out" 2>&1 ) &
RP=$!
sleep "$SECS"
for p in $(pgrep -x dosbox); do kill -9 $p 2>/dev/null; done
wait $RP 2>/dev/null

python3 - "$V/rec-raw/MTX.LOG" "$OUT" <<'PY' | tee -a "$OUT"
import re, sys
txt = open(sys.argv[1], errors='ignore').read()
o = open(sys.argv[2], "a")
s = [(int(a), int(b), int(c)) for a, b, c in
     re.findall(r'\[MTX\] frame (\d+) child=\d+ t=(\d+) raw=(\d+)', txt)]
if len(s) < 5:
    o.write(f"only {len(s)} samples with raw= - engine not rebuilt?\n"); o.close(); raise SystemExit
(f0, t0, r0), (f1, t1, r1) = s[0], s[-1]
df, dt, dr = f1 - f0, t1 - t0, r1 - r0
o.write(f"samples: {len(s)}   frame {f0}->{f1} (+{df})\n")
o.write(f"  I_GetTime()  {t0} -> {t1}   (+{dt})   = {dt/df:.3f} per frame\n")
o.write(f"  raw 0x2913c  {r0} -> {r1}   (+{dr})   = {dr/df:.3f} per frame\n")
if df:
    o.write(f"  raw/t ratio: {(dr/df)/(dt/df) if dt else float('nan'):.3f}\n")
    if dt and dr and abs((dr/df) - (dt/df)) < 0.15:
        o.write("  => they advance together: I_GetTime IS the game's counter\n")
    elif dt and dr:
        o.write("  => raw advances differently: the engine substituted its own clock\n")
# per-frame deltas of the raw counter, if the line is logged densely enough
import collections
d = collections.Counter()
pf = pt = pr = None
for f, t, r in s:
    if pf is not None and f - pf == 1:
        d[r - pr] += 1
    pf, pt, pr = f, t, r
o.write(f"  raw deltas per single frame: {dict(sorted(d.items()))}\n")
o.close()
PY
