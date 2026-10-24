#!/bin/bash
# record-game.sh <label> [pre_secs] [rec_secs]
# Boot the game in DOSBox under Xvfb and record the DOSBox window with ffmpeg
# x11grab, cropped to the window, then report the file and a couple of frames.
set -u
LABEL="${1:-rec}"
PRE="${2:-8}"
RECT="${3:-20}"
ROOT="$HOME/Documents/recursive_doom"
D="$ROOT/verify/$LABEL"
SRC="$ROOT/verify/trial-doomscr"
DB="$HOME/Downloads/dosbox-staging-linux-x86_64-0.83.0-7b400/dosbox"
DISP=":$((RANDOM%40+150))"
OUT="$D/$LABEL.mp4"

mkdir -p "$D"
for f in DOOM2.EXE DOOM2.WAD; do
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
[autoexec]
mount c "$D"
c:
doom2 -file loader.wad -warp 1 > LOG.TXT
EOF

rm -f "$D"/LOG.TXT "$D"/MTX.LOG "$OUT" "$D"/frame_*.png
pkill -f "Xvfb $DISP" 2>/dev/null
Xvfb "$DISP" -screen 0 1024x768x24 >/dev/null 2>&1 & XP=$!
export DISPLAY="$DISP"
sleep 2
( cd "$D" && timeout $((PRE + RECT + 20)) "$DB" -conf run.conf --noprimaryconf > db.log 2>&1 ) & DP=$!
sleep 4
for _k in 1 2 3 4; do xdotool key --clearmodifiers Return 2>/dev/null; sleep 1; done

WID=""
for i in $(seq 1 20); do WID=$(xdotool search --name '.' 2>/dev/null | head -1); [ -n "$WID" ] && break; sleep 0.5; done
[ -z "$WID" ] && { echo "NO WINDOW"; kill $DP $XP 2>/dev/null; exit 1; }
eval "$(xdotool getwindowgeometry --shell "$WID")"
echo "WID=[$WID] geometry ${WIDTH}x${HEIGHT}+${X}+${Y}"

sleep "$PRE"
echo "--- recording ${RECT}s with ffmpeg x11grab ---"
# libx264 needs even dimensions; the DOSBox window is 758x569 (odd height), so pad
# by at most one pixel rather than resampling the game.
ffmpeg -hide_banner -loglevel error -y \
  -f x11grab -framerate 20 -video_size "${WIDTH}x${HEIGHT}" -i "${DISP}+${X},${Y}" \
  -t "$RECT" -vf "pad=ceil(iw/2)*2:ceil(ih/2)*2" \
  -c:v libx264 -preset veryfast -crf 20 -pix_fmt yuv420p "$OUT"
echo "ffmpeg exit=$?"

sleep 1
kill $DP 2>/dev/null; wait $DP 2>/dev/null
kill $XP 2>/dev/null

echo "=== result ==="
ls -la "$OUT" 2>/dev/null || echo "NO VIDEO"
ffprobe -v error -show_entries format=duration,size -show_entries stream=codec_name,width,height,r_frame_rate,nb_frames -of default=noprint_wrappers=1 "$OUT" 2>/dev/null
echo "=== frames sampled across the clip ==="
for t in 2 6 10 14 18; do
  ffmpeg -hide_banner -loglevel error -y -ss "$t" -i "$OUT" -frames:v 1 "$D/frame_${t}s.png" 2>/dev/null
done
ls -la "$D"/frame_*.png 2>/dev/null
echo "=== engine log present? ==="
[ -s "$D/MTX.LOG" ] && grep -cE "present ->|screen check" "$D/MTX.LOG" || echo "(no MTX.LOG)"
grep -E "present ->|screen check" "$D/MTX.LOG" 2>/dev/null
