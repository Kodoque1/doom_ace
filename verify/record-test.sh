#!/bin/bash
# record-test.sh - can DOSBox Staging record a movie, headless, under Xvfb?
# Reuses the staged trial dir so the WAD/EXE are already in place.
set -u
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"
D="$ROOT/verify/rec-test"
SRC="$ROOT/verify/trial-doomscr"
DB="$HOME/Downloads/dosbox-staging-linux-x86_64-0.83.0-7b400/dosbox"
DISP=":$((RANDOM%40+150))"
CAP="$D/capture"

mkdir -p "$D" "$CAP"
for f in DOOM2.EXE DOOM2.WAD loader.wad; do
  [ -e "$SRC/$f" ] && ln -sf "$(realpath "$SRC/$f")" "$D/$f"
done
cp "$ROOT/doom_ace/loader/loader.wad" "$D/loader.wad"

cat > "$D/run.conf" <<EOF
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
[capture]
capture_dir=$CAP
default_image_capture_formats=png
[autoexec]
mount c "$D"
c:
doom2 -file loader.wad -warp 1 > LOG.TXT
EOF

rm -f "$CAP"/* "$D"/LOG.TXT
pkill -f "Xvfb $DISP" 2>/dev/null
Xvfb "$DISP" -screen 0 1024x768x24 >/dev/null 2>&1 & XP=$!
export DISPLAY="$DISP"
sleep 2
( cd "$D" && timeout 40 "$DB" -conf run.conf --noprimaryconf > db.log 2>&1 ) & DP=$!
sleep 4
for _k in 1 2 3 4; do xdotool key --clearmodifiers Return 2>/dev/null; sleep 1; done
WID=""
for i in $(seq 1 20); do WID=$(xdotool search --name '.' 2>/dev/null | head -1); [ -n "$WID" ] && break; sleep 0.5; done
echo "WID=[$WID]"
sleep 3
echo "--- try video capture: ctrl+alt+F5 ---"
xdotool key --clearmodifiers ctrl+alt+F5 2>/dev/null
sleep 10
xdotool key --clearmodifiers ctrl+alt+F5 2>/dev/null
sleep 2
echo "--- also try a plain screenshot: ctrl+F5 ---"
xdotool key --clearmodifiers ctrl+F5 2>/dev/null
sleep 2
echo "--- capture dir after keys ---"
ls -la "$CAP" 2>/dev/null
kill $DP 2>/dev/null; wait $DP 2>/dev/null
sleep 1
kill $XP 2>/dev/null
echo "--- capture dir final ---"
ls -la "$CAP" 2>/dev/null
for f in "$CAP"/*.avi; do
  [ -e "$f" ] || continue
  echo "=== ffprobe $f ==="
  ffprobe -v error -show_entries format=duration,size,format_name -show_entries stream=codec_name,width,height,r_frame_rate,nb_frames -of default=noprint_wrappers=1 "$f"
done
echo "--- db.log tail ---"
tail -5 "$D/db.log"
