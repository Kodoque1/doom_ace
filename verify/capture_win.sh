#!/bin/bash
# capture_win.sh <workdir> <wad> <outprefix> [secs]
# Runs DOOM2.EXE -file <wad> headless and captures the DOSBox window itself.
set -u
WD="$(realpath "$1")"; WAD="$2"; PREFIX="$(realpath -m "$3")"; SECS="${4:-30}"
DB="$HOME/Downloads/dosbox-staging-linux-x86_64-0.83.0-7b400/dosbox"
DISP=":$((RANDOM%40+140))"

cat > "$WD/run.conf" <<EOF
[sdl]
output=texture
fullscreen=false
[dosbox]
machine=svga_s3
memsize=63
[cpu]
cpu_cycles=max
[sblaster]
sbtype=none
[autoexec]
mount c "$WD"
c:
doom2 -file $WAD > LOG.TXT
EOF

rm -f "$WD/LOG.TXT" "${PREFIX}"_*.png
pkill -f "Xvfb $DISP" 2>/dev/null
Xvfb "$DISP" -screen 0 1024x768x24 >/dev/null 2>&1 & XP=$!
export DISPLAY="$DISP"
sleep 2
( cd "$WD" && timeout "$SECS" "$DB" -conf run.conf --noprimaryconf > db.log 2>&1 ) & DP=$!
sleep 4; xdotool key Return 2>/dev/null
sleep 1; xdotool key Return 2>/dev/null

WID=""
for i in $(seq 1 40); do
  WID=$(xdotool search --name 'DOOM2' 2>/dev/null | head -1)
  [ -n "$WID" ] && break
  sleep 0.5
done
echo "WID=[$WID]  dosbox_alive=$(kill -0 $DP 2>/dev/null && echo yes || echo no)"
sleep 5
[ -n "$WID" ] && import -window "$WID" "${PREFIX}_t8.png" 2>/dev/null
echo "after t8: dosbox_alive=$(kill -0 $DP 2>/dev/null && echo yes || echo no)"
sleep 6
[ -n "$WID" ] && import -window "$WID" "${PREFIX}_t14.png" 2>/dev/null
kill $DP 2>/dev/null; wait $DP 2>/dev/null
kill $XP 2>/dev/null
echo "=== LOG.TXT ==="; cat -v "$WD/LOG.TXT" 2>/dev/null
ls -la "${PREFIX}"_*.png 2>/dev/null
