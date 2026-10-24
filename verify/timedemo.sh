#!/bin/bash
# timedemo.sh [cycles]
#
# THE MEASUREMENT THAT SETTLES "is the emulated CPU fast enough for three Dooms?".
#
# Doom's own -timedemo disables the tic wait and runs the demo's tics as fast as the
# machine can, then prints "timed N gametics in M realtics". Every other frame-rate
# number in this project is capped by Doom's 35 Hz tic clock (one render per tic), so
# none of them can say how much CPU is actually there. This one is uncapped: it is the
# emulated machine's Doom-rendering throughput.
#
#   fps = gametics * 35 / realtics
#
# What the answer means for the ring:
#   fps >> 105  -> three instances at 35 frames/s each is affordable, and the ring's
#                  16 frames/s per instance is the TIC CLOCK being shared three ways,
#                  not a CPU shortage. A tic-rate patch can then buy real smoothness.
#   fps ~  40-60 -> the machine renders about one Doom and a bit. Three instances can
#                  never each reach 35, and only more CPU (KVM) or fewer instances can.
set -u
CYCLES="${1:-max}"
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"
REF="$ROOT/verify/after"
D="$ROOT/verify/rec-timedemo"
DB="$HOME/Downloads/dosbox-staging-linux-x86_64-0.83.0-7b400/dosbox"
DISP=":$((RANDOM%40+150))"

pkill -x dosbox 2>/dev/null; pkill -x Xvfb 2>/dev/null; sleep 1
mkdir -p "$D"
rm -f "$D/LOG.TXT"
ln -sf "$REF/DOOM2.EXE" "$D/DOOM2.EXE"
ln -sf "$REF/DOOM2.WAD" "$D/DOOM2.WAD"

cat > "$D/run.conf" <<EOF
[sdl]
output=texture
fullscreen=false
[dosbox]
machine=svga_s3
memsize=63
[cpu]
cpu_cycles=$CYCLES
[autoexec]
mount c "$D"
c:
doom2 -timedemo demo1 > LOG.TXT
EOF

Xvfb "$DISP" -screen 0 1024x768x24 >/dev/null 2>&1 & XP=$!
export DISPLAY="$DISP"
sleep 2
echo "running -timedemo demo1 with cpu_cycles=$CYCLES ..."
T0=$(date +%s)
timeout 400 "$DB" -conf "$D/run.conf" --noprimaryconf > "$D/db.log" 2>&1
T1=$(date +%s)
echo "wall time for the demo: $((T1-T0)) s"
kill $XP 2>/dev/null

echo "=== Doom's own report ==="
grep -aiE "timed|gametic|realtics|fps" "$D/LOG.TXT" 2>/dev/null | tail -5
echo "=== computed ==="
G=$(grep -aoE "timed [0-9]+ gametics" "$D/LOG.TXT" 2>/dev/null | grep -oE "[0-9]+")
R=$(grep -aoE "in [0-9]+ realtics" "$D/LOG.TXT" 2>/dev/null | grep -oE "[0-9]+")
if [ -n "${G:-}" ] && [ -n "${R:-}" ] && [ "${R:-0}" -gt 0 ]; then
  python3 -c "
g=$G; r=$R
print('  gametics %d in realtics %d  ->  %.1f frames/s' % (g, r, g*35.0/r))
print('  (three instances at 35 frames/s each would need ~105)')
"
else
  echo "  no timedemo line found - see $D/LOG.TXT and $D/db.log"
  tail -5 "$D/LOG.TXT" 2>/dev/null
fi
