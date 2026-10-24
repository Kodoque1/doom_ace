#!/bin/bash
set -u
WD="${MTX_ROOT:-$HOME/Documents/recursive_doom}/verify/base"
DB="$HOME/Downloads/dosbox-staging-linux-x86_64-0.83.0-7b400/dosbox"
DISP=":94"
pkill -f "Xvfb $DISP" 2>/dev/null
Xvfb "$DISP" -screen 0 1024x768x24 >/dev/null 2>&1 & XP=$!
export DISPLAY="$DISP"
sleep 2
echo "xvfb pid=$XP"
xdotool search --name '.' ; echo "search rc=$?"
( cd "$WD" && timeout 20 "$DB" -conf run.conf --noprimaryconf > db.log 2>&1 ) & DP=$!
sleep 3; xdotool key Return 2>/dev/null
sleep 1; xdotool key Return 2>/dev/null
sleep 4
echo "--- xdotool search --name . ---"; xdotool search --name '.' 2>&1 | head
echo "--- xdotool search --class dosbox ---"; xdotool search --class 'dosbox' 2>&1 | head
echo "--- xwininfo tree ---"; xwininfo -root -tree 2>&1 | head -15
WID=$(xwininfo -root -tree 2>/dev/null | grep -oE '0x[0-9a-f]+ +"DOOM' | head -1 | awk '{print $1}')
echo "WID=[$WID]"
sleep 3
if [ -n "$WID" ]; then import -window "$WID" "$WD/win.png"; echo "import rc=$?"; fi
wait $DP 2>/dev/null
kill $XP 2>/dev/null
ls -la "$WD"/win.png 2>/dev/null
