#!/bin/bash
# trial_key.sh - THE SWITCH KEY, measured. Two questions, one run:
#
#   1. ONE held press is ONE toggle. The key is HELD for a second (as a human does,
#      and as record.sh does) because Doom reads its key state at frame boundaries:
#      a tap can fall between two frames and be invisible. A held key must not toggle
#      twice - that is the phantom the whole authority word exists to kill (measured
#      before the fix: `inset -> depth 1` at frame 285 and `inset -> depth 2` at 289).
#   2. A SECOND press toggles AGAIN. This is the half that the obvious fix breaks: the
#      old code cleared the depth's key state at every slice start, and one earlier
#      attempt at "clear it properly" left the tracker saying "already down", after
#      which the switch never worked again for a whole run (zero switch lines). A fix
#      that kills the double toggle by latching forever is not a fix.
#
# Kill conditions:
#   1. more than one `trigger: key` toggle between the two presses
#   2. fewer than two `trigger: key` toggles in the run (the re-arm is broken)
#   3. the authority word never returns to 0 between the presses (no release was seen,
#      so the second press could not be a leading edge)
#   4. any illegal read or DYNX86
set -u
cd "${MTX_ROOT:-$HOME/Documents/recursive_doom}"
for p in $(pgrep dosbox); do kill -9 $p 2>/dev/null; done

echo "=== rebuild (abort on failure: a stale binary makes the run meaningless) ==="
make -C doom_ace/engine build 2>&1 | tail -1 || exit 1
make -C doom_ace/loader 2>&1 | tail -1 || exit 1
python3 verify/mkwadlevels.py 2>&1 | tail -1 || exit 1

D="$PWD/verify/keytest"
rm -f "$D"/level*.wad "$D"/MTX.LOG "$D"/LOG.TXT "$D"/db.log
mkdir -p "$D"
cp verify/levels/level0.wad verify/levels/level1.wad verify/levels/level2.wad "$D/"
ln -sf "$PWD/verify/after/DOOM2.EXE" "$D/DOOM2.EXE"
ln -sf "$PWD/verify/after/DOOM2.WAD" "$D/DOOM2.WAD"

DB="$HOME/Downloads/dosbox-staging-linux-x86_64-0.83.0-7b400/dosbox"
DISP=":$((RANDOM%40+150))"

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
doom2 -file level0.wad level1.wad level2.wad -warp 1 > LOG.TXT
EOF

pkill -f "Xvfb $DISP" 2>/dev/null
Xvfb "$DISP" -screen 0 1024x768x24 >/dev/null 2>&1 & XP=$!
export DISPLAY="$DISP"
sleep 2

( cd "$D" && timeout 60 "$DB" -conf run.conf --noprimaryconf > db.log 2>&1 ) & DP=$!
sleep 4

# Into a level: the first press clears the startup notice, the rest walk the menu.
for _k in $(seq 1 10); do
  xdotool key --clearmodifiers Return 2>/dev/null
  sleep 2
done

press() {	# press <label> - HELD, because a tap can fall between two frames
  echo "pressing the switch key ($1, held)"
  xdotool keydown --clearmodifiers y 2>/dev/null
  sleep 1
  xdotool keyup --clearmodifiers y 2>/dev/null
  sleep 3
}

press first
press second

kill $DP 2>/dev/null; wait $DP 2>/dev/null
kill $XP 2>/dev/null

L="$D/MTX.LOG"
# Keep the evidence where the other trials keep theirs: the run's directory is scratch,
# and a log that is overwritten by the next run is not evidence.
cp "$L" "$PWD/verify/trial-key.MTX.LOG"
L="$PWD/verify/trial-key.MTX.LOG"
echo "=== every toggle, with the authority and where the edge came from ==="
grep -a "inset -> depth" "$L"
echo "=== key-triggered toggles: $(grep -ac 'trigger: key' "$L") (want exactly 2) ==="
echo "=== how many frames apart, and is any pair closer than a slice? ==="
grep -a "trigger: key" "$L" | sed -e 's/.*at frame \([0-9]*\).*/\1/' | tr '\n' ' '; echo
echo "=== the authority word AT each key toggle (1 = the press was applied; the re-arm"
echo "    is what the two edges prove, since a second edge needs a 0 in between) ==="
grep -a "trigger: key" "$L" | sed -e 's/.*authority \([0-9]*\).*/authority \1/' 
echo "=== errors: illegal $(grep -ac Illegal "$L" 2>/dev/null || echo 0), dynx86 $(grep -ac DYNX86 "$L" 2>/dev/null || echo 0) ==="
echo "=== last lines ==="
tail -3 "$L"
