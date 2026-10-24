#!/bin/bash
set -u
WD="$HOME/Documents/recursive_doom/verify/base"
DB="$HOME/Downloads/dosbox-staging-linux-x86_64-0.83.0-7b400/dosbox"
DISP=":93"
pkill -f "Xvfb $DISP" 2>/dev/null
Xvfb "$DISP" -screen 0 1024x768x24 >/dev/null 2>&1 & XP=$!
export DISPLAY="$DISP"
sleep 2
( cd "$WD" && timeout 30 "$DB" -conf run.conf --noprimaryconf > db2.log 2>&1 ) & DP=$!
for i in $(seq 1 22); do
  sleep 1
  ALIVE=$(kill -0 $DP 2>/dev/null && echo Y || echo N)
  CH=$(xwininfo -root -tree 2>/dev/null | grep -c '0x[0-9a-f]* "' )
  BYPID=$(xdotool search --pid $DP 2>/dev/null | tr '\n' ' ')
  BYNAME=$(xdotool search --name '.' 2>/dev/null | tr '\n' ' ')
  echo "t=$i alive=$ALIVE children=$CH bypid=[$BYPID] byname=[$BYNAME]"
  if [ -n "$BYPID" ] && [ ! -f "$WD/probe.png" ]; then
    W=$(echo $BYPID | awk '{print $1}')
    import -window "$W" "$WD/probe.png" 2>/dev/null && echo "  captured probe with $W"
  fi
done
kill $DP 2>/dev/null; kill $XP 2>/dev/null
echo "=== db2 tail ==="; tail -6 "$WD/db2.log"
ls -la "$WD/probe.png" 2>/dev/null
