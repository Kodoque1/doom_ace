#!/bin/bash
# run.sh <workdir> <wad> <outprefix> [secs]
# <wad> may be a LIST: the ring's levels are three WADs on one command line
# (`-file level0.wad level1.wad level2.wad`), each carrying its own map under its
# own name, so no file overrides another.
# Headless DOSBox Staging: boots DOOM2.EXE -file <wad>, presses Enter past the
# startup notice, captures the DOSBox window and saves DOS stdout to LOG.TXT.
set -u
WD="$(realpath "$1")"; WAD="$2"; PREFIX="$(realpath -m "$3")"; SECS="${4:-22}"
DB="$HOME/Downloads/dosbox-staging-linux-x86_64-0.83.0-7b400/dosbox"
DISP=":$((RANDOM%40+150))"

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
doom2 -file $WAD -warp 1 > LOG.TXT
EOF

rm -f "$WD/LOG.TXT" "${PREFIX}"_*.png
pkill -f "Xvfb $DISP" 2>/dev/null
Xvfb "$DISP" -screen 0 1024x768x24 >/dev/null 2>&1 & XP=$!
export DISPLAY="$DISP"
sleep 2
( cd "$WD" && timeout "$SECS" "$DB" -conf run.conf --noprimaryconf > db.log 2>&1 ) & DP=$!
sleep 4
# Bounded key sequence: 1st clears the startup notice, then title -> main menu
# -> New Game -> skill, so we reach GS_LEVEL where the per-frame hook fires.
# Once in a level, Return is just "use", so extra presses are harmless; the risk
# is only that the notice appears late and eats the first press, which leaves the
# run sitting on the title. Widen the window with MTX_PRESS/MTX_GAP when needed.
PRESSES="${MTX_PRESS:-4}"; GAP="${MTX_GAP:-1}"
for _k in $(seq 1 "$PRESSES"); do
  xdotool key --clearmodifiers Return 2>/dev/null
  sleep "$GAP"
done
WID=""
for i in $(seq 1 24); do
  WID=$(xdotool search --name '.' 2>/dev/null | head -1)
  [ -n "$WID" ] && break
  sleep 0.5
done
sleep 2
[ -n "$WID" ] && import -window "$WID" "${PREFIX}_a.png" 2>/dev/null
# The switch key, if asked for, between the a and b captures: the two then differ
# by exactly one key press, which is what makes a before/after comparison mean
# something. HOLD it: `xdotool key` presses and releases within a millisecond, and
# Doom's key state is only read at frame boundaries, so a tap can fall between two
# frames and be invisible to the game. A held key is what a human does anyway.
if [ -n "${MTX_KEY:-}" ]; then
  xdotool keydown --clearmodifiers "$MTX_KEY" 2>/dev/null
  sleep 1
  xdotool keyup --clearmodifiers "$MTX_KEY" 2>/dev/null
  sleep 1
fi
[ -n "$WID" ] && import -window "$WID" "${PREFIX}_b.png" 2>/dev/null
# Third point, so a run that changes what it presents mid-flight can be caught in
# both states by one execution.
sleep 5
[ -n "$WID" ] && import -window "$WID" "${PREFIX}_c.png" 2>/dev/null
kill $DP 2>/dev/null; wait $DP 2>/dev/null
kill $XP 2>/dev/null
echo "WID=[$WID]"
echo "=== LOG.TXT ==="; cat -v "$WD/LOG.TXT" 2>/dev/null
ls -la "${PREFIX}"_*.png 2>/dev/null
