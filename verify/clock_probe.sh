#!/bin/bash
# clock_probe.sh - what is the machine's TICK RATE, and does the frame rate follow it?
#
# WHY. The ring renders 16.00 frames/s per instance, dead stable, and it does not move
# when the emulated CPU's speed changes (cpu_cycles 250000 vs max) - so a clock sets it.
# But it also did not move for two patches of the base image's PIT divisor, and the reason
# turned out to be structural: the RUNNING engine is the ACE image loaded out of the level
# WADs, and DOOM2.EXE is only the host game. Patching the host's timer cannot reach a
# clock the ACE image owns.
#
# THE MEASUREMENT. The frame log line now carries I_GetTime() (the engine's own time
# source), gametic and leveltime. The probe gives the frame rate against the WALL clock;
# the log gives how t, gametic and leveltime move per frame. Together:
#
#   t and frame advance together   -> one frame per tick; the tick rate IS the frame rate,
#                                     and raising the tick rate is the fix
#   t advances faster than frame   -> frames are duplicates; the engine is not tick-gated
#                                     and the tick rate is not the lever
#
# Builds engine AND loader (the level WADs are packed from loader.wad - building only the
# engine leaves the game running the PREVIOUS engine, which looks exactly like "no effect").
set -u
ROOT="$HOME/Documents/recursive_doom"; V="$ROOT/verify"
SECS="${SECS:-45}"
OUT="$V/clock_probe.out"
: > "$OUT"
say() { echo "$@" | tee -a "$OUT"; }

say "building engine + loader, then regenerating the level WADs"
( cd "$ROOT/doom_ace" && make -C engine >/dev/null 2>&1 ) || { say "ENGINE BUILD FAILED"; exit 1; }
( cd "$ROOT/doom_ace" && make -C loader >/dev/null 2>&1 ) || { say "LOADER BUILD FAILED"; exit 1; }
( cd "$ROOT" && python3 verify/mkwadlevels.py --iwad "$V/after/DOOM2.WAD" \
    --loader doom_ace/loader/loader.wad --out verify/levels >/dev/null ) || { say "WAD BUILD FAILED"; exit 1; }
say "built."

MTX_REF="$ROOT/verify/after" "$V/fps_probe.sh" clock "$SECS" >/dev/null 2>&1
say "  $(sed -n 1p "$V/clock.fps.txt")"

python3 - "$V/clock.fps.txt" "$V/rec-clock/MTX.LOG" "$OUT" <<'PY'
import re, sys
rows = []
for l in open(sys.argv[1]):
    m = re.match(r'\s+(\d+)\s+(\d+)\s+(\d+)\.(\d+)', l)
    if m:
        rows.append(float(f"{m.group(3)}.{m.group(4)}"))
out = open(sys.argv[3], "a")
if not rows:
    out.write("  no probe samples\n"); out.close(); raise SystemExit
t = sorted(rows[-10:]); fps = t[len(t)//2]
out.write(f"  frame rate (median of last {len(t)} windows): {fps:.2f} frames/s\n\n")

txt = open(sys.argv[2], errors='ignore').read()
pat = re.compile(r'\[MTX\] frame (\d+) child=\d+ t=(\d+) gametic=(\d+) lt=(\d+)')
s = [(int(a), int(b), int(c), int(d)) for a, b, c, d in pat.findall(txt)]
if len(s) < 3:
    out.write(f"  only {len(s)} frame lines with t= - was the engine really rebuilt?\n")
    out.close(); raise SystemExit
(f0, t0, g0, l0), (f1, t1, g1, l1) = s[0], s[-1]
df, dt, dg, dl = f1 - f0, t1 - t0, g1 - g0, l1 - l0
out.write(f"  root frame {f0} -> {f1}   (+{df} frames)\n")
out.write(f"  root t     {t0} -> {t1}   (+{dt} ticks)   => {dt/df:.3f} ticks per frame\n")
out.write(f"  root gametic +{dg}, leveltime +{dl}\n\n")
out.write(f"  frame rate measured against the wall clock: {fps:.2f} frames/s\n")
out.write(f"  => implied TICK RATE: {fps*dt/df:.2f} ticks/s\n")
out.write(f"  => ticks per SECOND from the counter alone cannot be had without wall time;\n"
          f"     the ratio above is the sound part, and it is what decides the question.\n")
out.close()
PY
for p in $(pgrep -x dosbox); do kill -9 $p 2>/dev/null; done
say ""
say "done"
