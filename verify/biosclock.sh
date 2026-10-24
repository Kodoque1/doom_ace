#!/bin/bash
# biosclock.sh - the pace in MONOTONIC emulated time: how many 18.2 Hz BIOS ticks per frame?
#
# WHY. The PIT counter aliases (it wraps every divisor counts; with a 65536 divisor that is
# 54.9 ms, close to the ~62.5 ms frame period, so a mod-65536 delta cannot say how many
# wraps passed). The BIOS tick counter at 0040:006C does not have that problem: it is a
# 32-bit count of 18.2 Hz ticks, monotonic, and DOS/4GW maps the low 1 MB flat (the engine
# already reads 0xA0000), so the guest can read it directly.
#
# WHAT THE NUMBER MEANS
#   ~1.00 BIOS ticks/frame  -> the pace IS the BIOS tick: 18.2 Hz emulated, and the wall
#                              rate (16.0/s) then gives the guest's realtime ratio directly
#   ~1.14                   -> 16 Hz emulated: the pace is the PIT (impossible: min 18.2 Hz)
#   much less than 1        -> the pace is faster than the BIOS tick and lives elsewhere
set -u
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"; V="$ROOT/verify"
SECS="${SECS:-40}"
OUT="$V/biosclock.out"
: > "$OUT"

export MTX_SWITCHES=0 MTX_NOCAP=1
( "$V/record.sh" bios "$((SECS + 20))" --nobuild >"$V/rec-bios.probe.out" 2>&1 ) &
RP=$!
sleep "$SECS"
for p in $(pgrep -x dosbox); do kill -9 $p 2>/dev/null; done
wait $RP 2>/dev/null

python3 - "$V/rec-bios/MTX.LOG" "$V/bios.fps.txt" "$OUT" <<'PY' | tee -a "$OUT"
import re, sys, collections
txt = open(sys.argv[1], errors='ignore').read()
o = open(sys.argv[3], "a")
s = [(int(a), int(b), int(c), int(d), int(e)) for a, b, c, d, e in
     re.findall(r'\[MTX\] frame (\d+) child=\d+ t=(\d+) raw=(\d+) pit=(\d+) bios=(\d+)', txt)]
if len(s) < 5:
    o.write(f"only {len(s)} samples with bios= - engine not rebuilt?\n"); o.close(); raise SystemExit
o.write(f"samples: {len(s)}\n")
d = collections.Counter()
for (f0, t0, r0, p0, b0), (f1, t1, r1, p1, b1) in zip(s, s[1:]):
    if f1 - f0 == 1:
        d[b1 - b0] += 1
tot = sum(d.values())
o.write(f"BIOS ticks per frame over {tot} consecutive frames: {dict(sorted(d.items()))}\n")
if tot:
    per = sum(k*v for k, v in d.items()) / tot
    o.write(f"  mean {per:.3f} BIOS ticks per frame  => EMULATED frame period {per/18.2*1000:.2f} ms\n")
    o.write(f"  => emulated frame/tick rate {18.2/per:.2f} Hz\n")
    try:
        rows = []
        for l in open(sys.argv[2]):
            m = re.match(r'\s+(\d+)\s+(\d+)\s+(\d+)\.(\d+)', l)
            if m: rows.append(float(f"{m.group(3)}.{m.group(4)}"))
        if rows:
            t = sorted(rows[-10:]); fps = t[len(t)//2]
            o.write(f"  wall frame rate {fps:.2f} frames/s (period {1000.0/fps:.2f} ms)"
                    f"  => guest realtime ratio R = {(per/18.2)/(1.0/fps):.3f}\n")
    except Exception as e:
        o.write(f"  (probe report unreadable: {e})\n")
o.close()
PY
