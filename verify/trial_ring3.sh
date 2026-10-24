#!/bin/bash
# trial_ring3.sh - the three-level nest: does a SECOND child boot, does the ring appear
# in the pictures, and does the switch key still reach the host?
#
# What to look for, in order of what would kill the idea:
#   1. depth 2 entering and loading its own image in its OWN slot (a second instance)
#   2. the picture rule per level: depth 0 paints the frame of 1, depth 1 the frame of
#      2, depth 2 the ROOT's - the wrap. Steady state matters, not the first frames:
#      a level paints nothing while it is still booting.
#   3. the key still switching (one press -> one line)
#   4. 0 illegal, 0 DYNX86 throughout
set -u
cd /home/kodoque/Documents/recursive_doom
for p in $(pgrep dosbox); do kill -9 $p 2>/dev/null; done

echo "=== rebuild (abort on failure: a stale binary would make the run meaningless) ==="
make -C doom_ace/engine build 2>&1 | tail -2 || exit 1
make -C doom_ace/loader 2>&1 | tail -2 || exit 1
python3 verify/mkwadlevels.py | tail -1 || exit 1
cp verify/levels/level0.wad verify/base/

MTX_KEY=y ./verify/run.sh verify/base level0.wad verify/trial-ring3 30 > /tmp/ring3.out 2>&1
cp verify/base/MTX.LOG verify/trial-ring3.MTX.LOG
L=verify/trial-ring3.MTX.LOG

echo "=== illegal $(grep -ac Illegal $L), dynx86 $(grep -ac DYNX86 $L) ==="
echo "=== plan, arena, children ==="
grep -aE "reserved|host up|could not create" $L | head -5
echo "=== each level booting its own image ==="
grep -aE "depth[12] entered|depth[12]: engine image|depth[12]: entering" $L | head -6
echo "=== the key and the inset ==="
grep -a "switch: key" $L | head -8
echo "=== the RING in the pictures ==="
for k in 0 1 2; do
	echo "--- depth $k: diegetic lines"
	grep -ac "diegetic: depth $k " $L
	echo "--- of those, 0-pixel (level not rendering yet / no screen wall in view)"
	grep -acE "diegetic: depth $k .*\(0 pixels\)" $L
done
echo "--- last 6 diegetic lines (steady state, one per level per lap) ---"
grep -a diegetic $L | tail -6
echo "=== instances handing frames back (both children alive) ==="
grep -ac "instance hand-back at frame" $L
echo "=== last lines of the log ==="
tail -4 $L
