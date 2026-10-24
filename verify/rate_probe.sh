#!/bin/bash
# rate_probe.sh - the MACHINE's tick rate and the ENGINE's frame rate, both against the
# host's wall clock, in one run.
#
# WHY. The frame log line now carries I_GetTime(), which the disassembly shows is a raw
# read of the game's tick counter at [0x2913c]:
#
#   5bc20 (load 0x19C10, the I_GetTime trampoline target):
#     8d 80 00 00 00 00   lea eax,[eax+0]        ; padding
#     8d 92 00 00 00 00   lea edx,[edx+0]
#     90                  nop
#     a1 3c 91 02 00      mov eax,[0x2913c]      <-- the counter
#     c3                  ret
#
# and the IRQ0 handler that owns that counter (installed on DPMI vector 8, load 0x1a8b0)
# increments it and EOIs the PIC. So I_GetTime's rate IS the machine's tick rate, and the
# frame counter's rate is the engine's render rate. Sampling BOTH against the host clock
# in the same run is what separates them:
#
#   tick rate == frame rate  -> the engine is one frame per tick, and the TICK rate is the
#                               thing to raise (the lever is the machine's clock)
#   tick rate >  frame rate  -> the engine renders slower than the clock allows, frames
#                               repeat, and raising the clock cannot help
#
# The game's PIT divisor is 34000 (1193182/34000 = 35.09 Hz), so a tick rate near 35 is
# what the hardware was asked for; anything else is a measurement of what it actually got.
set -u
ROOT="$HOME/Documents/recursive_doom"; V="$ROOT/verify"
SECS="${SECS:-45}"
OUT="$V/rate_probe.out"
: > "$OUT"
say() { echo "$@" | tee -a "$OUT"; }

export MTX_SWITCHES=0 MTX_NOCAP=1
say "rate_probe: ${SECS}s, capture OFF, no switches"

# record.sh takes <label> <secs> [--nobuild] - passing --nobuild as the SECOND argument
# made it treat "nobuild" as the seconds value and die on an unbound variable ("NO LEVEL:
# the deepest level never came up"), which looks exactly like an engine failure.
( "$V/record.sh" rate "$((SECS + 20))" --nobuild >"$V/rec-rate.probe.out" 2>&1 ) &
RP=$!

poll() {
  python3 - "$V/rec-rate/MTX.LOG" <<'PY'
import re, sys
try:
    t = open(sys.argv[1], errors='ignore').read()
except Exception:
    print("0 0"); raise SystemExit
fr = [int(m) for m in re.findall(r'\[MTX\] frame (\d+) child=', t)]
tk = [int(m) for m in re.findall(r'\[MTX\] frame \d+ child=\d+ t=(\d+)', t)]
print(f"{max(fr) if fr else 0} {max(tk) if tk else 0}")
PY
}

t0=$(date +%s); pf=0; pt=0; pt_t=0
say "  t(s)   frame    tick    frame/s   tick/s"
while :; do
  now=$(( $(date +%s) - t0 ))
  [ "$now" -ge "$SECS" ] && break
  read -r f k < <(poll)
  if [ "$pt_t" -gt 0 ] && { [ "$f" -gt "$pf" ] || [ "$k" -gt "$pt" ]; }; then
    dt=$(( now - pt_t )); df=$(( f - pf )); dk=$(( k - pt ))
    printf "  %4d  %6d  %6d   %7d   %7d\n" "$now" "$f" "$k" "$(( df / dt ))" "$(( dk / dt ))" | tee -a "$OUT"
  fi
  pf=$f; pt=$k; pt_t=$now
  sleep 1
done

wait $RP 2>/dev/null
say ""
say "--- engine's own view ---"
python3 - "$V/rec-rate/MTX.LOG" "$OUT" <<'PY'
import re, sys
t = open(sys.argv[1], errors='ignore').read()
s = [(int(a), int(b)) for a, b in re.findall(r'\[MTX\] frame (\d+) child=\d+ t=(\d+)', t)]
o = open(sys.argv[2], "a")
if len(s) >= 3:
    (f0, t0), (f1, t1) = s[0], s[-1]
    o.write(f"  frame {f0} -> {f1} (+{f1-f0})   tick {t0} -> {t1} (+{t1-t0})"
            f"   => {(t1-t0)/(f1-f0):.3f} ticks per frame\n")
else:
    o.write(f"  only {len(s)} samples with t=\n")
o.close()
PY
for p in $(pgrep -x dosbox); do kill -9 $p 2>/dev/null; done
say "done"
