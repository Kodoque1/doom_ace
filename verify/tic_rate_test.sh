#!/bin/bash
# tic_rate_test.sh - raise the PIT tick rate 3x and see whether the ring's frame rate follows.
#
# WHAT IS BEING PATCHED, and why this site and not the one before.
# The first attempt patched file offset 0x25fd (a divisor constant). It moved nothing.
# Disassembling showed why: that block READS the PIT (in al,0x40) and stores a constant in
# a variable - it is not the live install. The live one is this, at file 0x5c8f4:
#
#   5c8f8: 53 52            push ebx, edx
#   5c8fa: 89 c3            mov  ebx,eax        ; EAX = the PIT divisor
#   5c8fc: 85 c0            test eax,eax
#   5c8fe: 7e 15            jle  skip            ; divisor <= 0
#   5c900: 3d 96 00 00 00   cmp  eax,0x96
#   5c905: 7d 0e            jge  skip            ; divisor >= 150 -> "rate too high" warning
#   5c911: ba 43 00 00 00   mov  edx,0x43
#   5c916: b0 36            mov  al,0x36        ; PIT ch0, lobyte/hibyte, mode 3
#   5c918: ee               out  al,dx
#   5c919: ba 40 00 00 00   mov  edx,0x40
#   5c91e: 88 d8 / c1 fb 08 / 88 d8 / ee / ee    ; write divisor low then high
#
# and it is called exactly once, from the timer init at 0x5c95e:
#
#   5c95e: b8 d0 84 00 00   mov  eax,0x84d0     ; 34000  -> 1193182/34000 = 35.09 Hz
#   5c963: e8 8c ff ff ff   call 0x5c8f4
#
# The IRQ0 handler right before it increments a counter at [0x2913c] and EOIs the PIC, so
# the divisor IS the machine's tick rate: 35 Hz, Doom's own TICRATE.
#
# THE PATCH: 0x84d0 (34000, 35.09 Hz) -> 0x2c45 (11333, 105.3 Hz). Three times the tics.
#   frame rate ~3x -> the tick clock was the gate, and the ring becomes smooth
#   frame rate flat -> the frame rate is not tick-gated, and the gate is elsewhere
#
# Stock is run BEFORE and AFTER: host load drifts within a session (measured repeatedly),
# so an unbracketed A/B is not evidence. Both runs log gametic, so tics-per-frame comes
# out of the same runs.
set -u
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"; V="$ROOT/verify"
SECS="${SECS:-45}"
OUT="$V/tic_rate_test.out"
: > "$OUT"
say() { echo "$@" | tee -a "$OUT"; }

python3 - <<'PY' | tee -a "$OUT"
import os
R = os.environ.get("MTX_ROOT", "/home/kodoque/Documents/recursive_doom")
src = f"{R}/verify/after/DOOM2.EXE"
d = bytearray(open(src, "rb").read())
off = 0x5c95f          # the imm32 of `mov eax,0x84d0`
assert d[0x5c95e:0x5c963] == b"\xb8\xd0\x84\x00\x00", \
    f"expected mov eax,0x84d0 at 0x5c95e, found {d[0x5c95e:0x5c963].hex()}"
old = int.from_bytes(d[off:off+4], "little")
new = 11333            # 1193182/11333 = 105.3 Hz
d[off:off+4] = new.to_bytes(4, "little")
dst = f"{R}/verify/after-tic"
os.makedirs(dst, exist_ok=True)
open(f"{dst}/DOOM2.EXE", "wb").write(bytes(d))
wad = f"{dst}/DOOM2.WAD"
if os.path.islink(wad) or os.path.exists(wad):
    os.remove(wad)
os.symlink(f"{R}/verify/after/DOOM2.WAD", wad)
chk = open(f"{dst}/DOOM2.EXE", "rb").read()
diffs = [i for i in range(len(chk)) if chk[i] != open(src, "rb").read()[i]]
print(f"patched: divisor {old} ({1193182/old:.2f} Hz) -> {new} ({1193182/new:.2f} Hz)")
print(f"         bytes changed in the whole image: {len(diffs)} at {[hex(i) for i in diffs]}")
PY

run() { # run <label> <MTX_REF>
  local label="$1" ref="$2"
  say ""
  say "### $label   (MTX_REF=$(basename "$ref"))"
  MTX_REF="$ref" "$V/fps_probe.sh" "t-$label" "$SECS" >/dev/null 2>&1
  say "  $(sed -n 1p "$V/t-$label.fps.txt")"
  say "  $(grep -aE 'host up: depths' "$V/rec-t-$label/MTX.LOG" | head -1)"
  python3 - "$V/t-$label.fps.txt" "$V/rec-t-$label/MTX.LOG" "$OUT" <<'PY'
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
out.write(f"  frame rate (median of last {len(t)} windows): {fps:.2f} frames/s\n")
# THE TICK RATE, from the engine's own counter, calibrated by the frame rate against the
# wall clock. This is the measurement that says whether the divisor patch reached the
# machine's clock and whether the frames follow it.
txt = open(sys.argv[2], errors='ignore').read()
s2 = [(int(a), int(b)) for a, b in re.findall(r'\[MTX\] frame (\d+) child=\d+ t=(\d+)', txt)]
if len(s2) >= 3:
    (f0, k0), (f1, k1) = s2[0], s2[-1]
    df, dk = f1 - f0, k1 - k0
    if df > 0:
        ratio = dk / df
        out.write(f"  machine tick counter: frame +{df}, tick +{dk}  =>  {ratio:.3f} ticks/frame"
                  f"   =>  TICK RATE {fps*ratio:.2f} ticks/s\n")
    else:
        out.write("  frame counter did not advance\n")
else:
    out.write(f"  only {len(s2)} frame lines with t= - engine not rebuilt?\n")
txt = open(sys.argv[2], errors='ignore').read()
pat = re.compile(r'\[MTX\] depth (\d+) frame (\d+) gametic (\d+) leveltime (\d+):')
per = {}
for d_, f, g, lt in pat.findall(txt):
    per.setdefault(int(d_), []).append((int(f), int(g)))
for d_ in sorted(per):
    s = per[d_]
    if len(s) < 3:
        out.write(f"  depth {d_}: only {len(s)} samples\n"); continue
    (f0, g0), (f1, g1) = s[0], s[-1]
    df, dg = f1 - f0, g1 - g0
    if df <= 0:
        out.write(f"  depth {d_}: frame counter did not advance\n"); continue
    out.write(f"  depth {d_}: frames +{df}, gametic +{dg}  =>  {dg/df:.3f} tics per frame"
              f"   (implied {fps*dg/df:.1f} tics/s)\n")
out.close()
PY
  for p in $(pgrep -x dosbox); do kill -9 $p 2>/dev/null; done
  sleep 2
}

run stock1 "$ROOT/verify/after"
run tic3x  "$ROOT/verify/after-tic"
run stock2 "$ROOT/verify/after"
say ""
say "done"
