#!/bin/bash
# plain_doom_rate.sh [secs]
#
# WHY: before blaming the ring or the host for 16 frames/s, measure what this DOSBox
# can do for ONE Doom with nothing of ours in it - no ACE loader, no ring, no depths,
# no per-frame policy. Plain DOOM2.EXE boots into its attract mode, which plays the
# demo loop: a MOVING view, so the clip's distinct-picture rate IS the render rate.
# (A still scene would under-report it - that mistake was already made once on the
# nested clip.)
#
#   plain Doom at ~35/s  -> the emulator is fine; the ring's three-way division and
#                           the engine's per-frame work are what cost the frames
#   plain Doom at ~16/s  -> this DOSBox on this host cannot run one Doom at full
#                           speed, and no amount of engine work will fix that
set -u
SECS="${1:-20}"
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"
REF="$ROOT/verify/after"
D="$ROOT/verify/rec-plain"
DB="$HOME/Downloads/dosbox-staging-linux-x86_64-0.83.0-7b400/dosbox"
DISP=":$((RANDOM%40+150))"

pkill -x dosbox 2>/dev/null; pkill -x Xvfb 2>/dev/null; sleep 1
mkdir -p "$D" "$D/cap"
rm -f "$D"/cap/*.avi "$D"/*.mp4
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
cpu_cycles=${MTX_CYCLES:-max}
[capture]
capture_dir=$D/cap
[autoexec]
mount c "$D"
c:
doom2 > LOG.TXT
EOF

Xvfb "$DISP" -screen 0 1024x768x24 >/dev/null 2>&1 & XP=$!
export DISPLAY="$DISP"
sleep 2
( cd "$D" && timeout $((SECS + 40)) "$DB" -conf run.conf --noprimaryconf > db.log 2>&1 ) & DP=$!
sleep 8

WID=""
for i in $(seq 1 20); do
  WID=$(xdotool search --name '.' 2>/dev/null | head -1)
  [ -n "$WID" ] && break
  sleep 0.5
done
if [ -z "$WID" ]; then echo "NO WINDOW"; kill $DP $XP 2>/dev/null; exit 1; fi
xdotool windowfocus --sync "$WID" 2>/dev/null
# let the title screen give way to the attract-mode demo (a moving view)
sleep 6
echo "starting the capture (plain Doom, attract demo)"
xdotool key --clearmodifiers ctrl+F7 2>/dev/null
sleep "$SECS"
xdotool key --clearmodifiers ctrl+F7 2>/dev/null
sleep 2
kill $DP 2>/dev/null; wait $DP 2>/dev/null; kill $XP 2>/dev/null

AVI=$(ls -t "$D"/cap/*.avi 2>/dev/null | head -1)
if [ -z "$AVI" ]; then echo "NO CAPTURE FILE"; exit 1; fi
ffmpeg -loglevel error -y -i "$AVI" -c:v libx264 -preset veryfast -crf 20 -pix_fmt yuv420p \
       "$D/plain.mp4" || echo "CONVERT FAILED"
echo "avi: $AVI"
ffprobe -v error -select_streams v:0 -show_entries stream=r_frame_rate,nb_frames,duration \
        -of default=noprint_wrappers=1 "$AVI"

# THE CALIBRATION. The AVI is stamped at the emulated refresh (70.086 Hz), so its duration
# IS emulated time; db.log's capture lines give the wall time the recorder ran. The ratio
# says whether the host delivered the requested cycles: ~1.0 means it kept up (the host
# has at least this much headroom), well below 1.0 means the emulated clock fell behind
# and the host is saturated at roughly ratio x the requested cycles.
WALL=$(python3 - "$D/db.log" <<'PY'
import re, sys, datetime
ts = []
for line in open(sys.argv[1], errors='ignore'):
    if 'CAPTURE: Capturing' in line or 'CAPTURE: Stopped' in line:
        m = re.match(r'(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d+)', line)
        if m:
            ts.append(datetime.datetime.strptime(m.group(1), '%Y-%m-%d %H:%M:%S.%f'))
if len(ts) >= 2:
    print((ts[-1] - ts[0]).total_seconds())
else:
    print(0)
PY
)
EMU=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$AVI" 2>/dev/null)
echo "requested cycles: ${MTX_CYCLES:-max}   emulated ${EMU}s over ${WALL}s of wall"
python3 -c "
e=float('$EMU' or 0); w=float('$WALL' or 0)
print('  ratio emulated/wall: %.3f' % (e/w) if w else '  ratio: n/a')
"
