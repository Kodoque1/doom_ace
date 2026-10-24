#!/bin/bash
# vbl_test.sh - is the VGA retrace wait what paces the ring?
#
# WHAT I_WaitVBL ACTUALLY IS (file 0x5bc74, load 0x19C60 - the trampoline target):
#
#   5bc74: 53 51 52            push ebx, ecx, edx
#   5bc77: 89 c3               mov  ebx,eax          ; n = number of retraces to wait
#   5bc79: 83 3d 00 91 02 00 00  cmpl $0,0x29100      ; <-- a FLAG
#   5bc80: 75 20               jne  0x5bca2         ; if set, return WITHOUT waiting
#   5bc82: b9 da 03 00 00      mov  ecx,0x3da        ; VGA status port
#   5bc8e: in al,dx / test al,0x8 / je ...            ; spin until retrace STARTS
#   5bc97: in al,dx / test al,0x8 / je ...            ; spin until it ENDS
#   ...                                               ; once per n
#
# So the machine has a software flag at [0x29100] that disables the retrace wait, and the
# wait itself is a busy spin on port 0x3DA - which is why the host core is pegged at 100%
# even though the ring has spare capacity.
#
# THE PATCH: the conditional jump at file 0x5bc80 becomes unconditional (75 -> eb), so
# I_WaitVBL always returns immediately. One byte.
#   frame rate jumps  -> the retrace wait was the pace, and the machine has been idling on
#                        a display clock; the fix is to stop waiting (or to find the flag)
#   frame rate flat   -> the wait is not called on the frame path, and the pace is elsewhere
#
# Stock runs before and after: host load drifts within a session.
set -u
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"; V="$ROOT/verify"
SECS="${SECS:-45}"
OUT="$V/vbl_test.out"
: > "$OUT"
say() { echo "$@" | tee -a "$OUT"; }

python3 - <<'PY' | tee -a "$OUT"
import os
R = os.environ.get("MTX_ROOT", "/home/kodoque/Documents/recursive_doom")
src = f"{R}/verify/after/DOOM2.EXE"
d = bytearray(open(src, "rb").read())
off = 0x5bc80
assert d[off-7:off] == b"\x83\x3d\x00\x91\x02\x00\x00", "expected the cmpl at 0x5bc79"
assert d[off:off+2] == b"\x75\x20", f"expected jne 0x20, found {d[off:off+2].hex()}"
d[off] = 0xEB                      # jne -> jmp: always skip the retrace wait
dst = f"{R}/verify/after-nowait"
os.makedirs(dst, exist_ok=True)
open(f"{dst}/DOOM2.EXE", "wb").write(bytes(d))
wad = f"{dst}/DOOM2.WAD"
if os.path.islink(wad) or os.path.exists(wad):
    os.remove(wad)
os.symlink(f"{R}/verify/after/DOOM2.WAD", wad)
chk = open(f"{dst}/DOOM2.EXE", "rb").read()
diffs = [i for i in range(len(chk)) if chk[i] != open(src, "rb").read()[i]]
print(f"I_WaitVBL: jne -> jmp at file 0x{off:05x} ({len(diffs)} byte(s) changed: {[hex(i) for i in diffs]})")
PY

run() { # run <label> <MTX_REF>
  local label="$1" ref="$2"
  say ""
  say "### $label   (MTX_REF=$(basename "$ref"))"
  MTX_REF="$ref" "$V/fps_probe.sh" "v-$label" "$SECS" >/dev/null 2>&1
  say "  $(sed -n 1p "$V/v-$label.fps.txt")"
  say "  $(grep -aE 'host up: depths' "$V/rec-v-$label/MTX.LOG" | head -1)"
  python3 - "$V/v-$label.fps.txt" "$V/rec-v-$label/MTX.LOG" "$OUT" <<'PY'
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
txt = open(sys.argv[2], errors='ignore').read()
s2 = [(int(a), int(b)) for a, b in re.findall(r'\[MTX\] frame (\d+) child=\d+ t=(\d+)', txt)]
if len(s2) >= 3:
    (f0, k0), (f1, k1) = s2[0], s2[-1]
    if f1 > f0:
        r = (k1 - k0) / (f1 - f0)
        out.write(f"  ticks/frame {r:.3f}  => tick rate {fps*r:.2f} ticks/s\n")
out.close()
PY
  for p in $(pgrep -x dosbox); do kill -9 $p 2>/dev/null; done
  sleep 2
}

run stock1 "$ROOT/verify/after"
run nowait "$ROOT/verify/after-nowait"
run stock2 "$ROOT/verify/after"
say ""
say "done"
