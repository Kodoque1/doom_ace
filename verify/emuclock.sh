#!/bin/bash
# emuclock.sh - the frame period in EMULATED time, read from inside the guest.
#
# WHY THIS IS THE MEASUREMENT THAT ENDS THE SEARCH.
# Known: the engine renders exactly one frame per game tick; the tick rate is ~16/s; the
# pace scales with the EMULATED clock (cpu_cycles=2000000 -> exactly 1/4 the rate); and it
# cannot be the PIT's interrupt rate (PIT minimum 18.2 Hz, observed 16 Hz, and two divisor
# patches were inert). So something waits on an emulated timebase, and I have not named it.
#
# The PIT's channel-0 COUNTER is that timebase itself: 1.193182 MHz, independent of the
# divisor (which is exactly why patching the divisor did nothing). Reading it at frame
# boundaries gives the frame period in emulated units, and the number names the pace:
#
#   counts/frame ~ 65536  -> the pace is one PIT wrap: the BIOS tick, 18.2 Hz emulated
#   counts/frame ~ 74575  -> a software wait of that many counts: 16 Hz emulated
#   counts/frame ~ 17000  -> 70 Hz emulated: a retrace-counted pace
#
# and the ratio against the WALL frame period gives the guest's realtime ratio directly.
#
# The counter counts DOWN and wraps, so the delta is (prev - now) mod 65536.
set -u
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"; V="$ROOT/verify"
SECS="${SECS:-40}"
OUT="$V/emuclock.out"
: > "$OUT"

export MTX_SWITCHES=0 MTX_NOCAP=1
( "$V/record.sh" emu "$((SECS + 20))" --nobuild >"$V/rec-emu.probe.out" 2>&1 ) &
RP=$!
sleep "$SECS"
for p in $(pgrep -x dosbox); do kill -9 $p 2>/dev/null; done
wait $RP 2>/dev/null

python3 - "$V/rec-emu/MTX.LOG" "$OUT" <<'PY' | tee -a "$OUT"
import re, sys
txt = open(sys.argv[1], errors='ignore').read()
o = open(sys.argv[2], "a")
s = [(int(a), int(b), int(c), int(d)) for a, b, c, d in
     re.findall(r'\[MTX\] frame (\d+) child=\d+ t=(\d+) raw=(\d+) pit=(\d+)', txt)]
if len(s) < 5:
    o.write(f"only {len(s)} samples with pit= - engine not rebuilt?\n"); o.close(); raise SystemExit

PIT_HZ = 1193182.0
o.write(f"samples: {len(s)}\n")
# per-frame deltas of the PIT counter (it counts down and wraps)
deltas = []
for (f0, t0, r0, p0), (f1, t1, r1, p1) in zip(s, s[1:]):
    if f1 - f0 == 1:
        deltas.append((p0 - p1) % 65536)
if not deltas:
    o.write("no consecutive frames\n"); o.close(); raise SystemExit
deltas.sort()
med = deltas[len(deltas)//2]
lo, hi = deltas[0], deltas[-1]
o.write(f"PIT counts per frame: min {lo}, median {med}, max {hi}\n")
o.write(f"  frame period in EMULATED time: {med/PIT_HZ*1000:.3f} ms  (median)\n")
o.write(f"  => EMULATED frame/tick rate: {PIT_HZ/med:.3f} Hz\n")
# wall period, from the probe's own report if present, else the log's frame span
fps = None
try:
    import re as _re
    rows = []
    for l in open(f"{sys.argv[1]}/../../emuclock.fps.txt" if False else sys.argv[1].replace("rec-emu/MTX.LOG","emu.fps.txt")):
        m = _re.match(r'\s+(\d+)\s+(\d+)\s+(\d+)\.(\d+)', l)
        if m: rows.append(float(f"{m.group(3)}.{m.group(4)}"))
    if rows:
        t = sorted(rows[-10:]); fps = t[len(t)//2]
except Exception:
    pass
if fps:
    o.write(f"  wall frame rate (probe): {fps:.2f} frames/s  => wall period {1000.0/fps:.3f} ms\n")
    o.write(f"  guest realtime ratio R = {(med/PIT_HZ)/(1.0/fps):.3f}\n")
o.close()
PY
