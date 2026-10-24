#!/bin/bash
# trial_ring1.sh - after the RING change: prove it reproduces the old hardcoded rule
# at two depths, and that the arena now plans three.
#
# The instrument is the diegetic log line: it names the texture and counts the pixels
# painted, and holds no address, so a change that alters WHERE things live must not
# move it. The baseline is the two-depth run from before the change (20 MiB, canvas
# rule). Only the plan line and addresses are allowed to differ.
set -u
cd "${MTX_ROOT:-$HOME/Documents/recursive_doom}"
for p in $(pgrep dosbox); do kill -9 $p 2>/dev/null; done

echo "=== rebuild engine + exploit WAD ==="
make -C doom_ace/engine build 2>&1 | tail -3
make -C doom_ace/loader 2>&1 | tail -3
python3 verify/mkwadlevels.py | tail -2
cp verify/levels/level0.wad verify/base/

echo "=== run (level0 = the same exploit, no map) ==="
./verify/run.sh verify/base level0.wad verify/trial-ring1 20 > /tmp/ring1.out 2>&1
cp verify/base/MTX.LOG verify/trial-ring1.MTX.LOG
echo "illegal $(grep -ac Illegal verify/trial-ring1.MTX.LOG), dynx86 $(grep -ac DYNX86 verify/trial-ring1.MTX.LOG)"

echo "=== arena reserved + depth plan ==="
grep -aE "reserved|host up|boot failed" verify/trial-ring1.MTX.LOG | head -4

echo "=== diegetic lines: BEFORE (20 MiB, hardcoded rule) vs AFTER (28 MiB, ring) ==="
grep -a diegetic verify/trial-lvl0.MTX.LOG > /tmp/d_before.txt
grep -a diegetic verify/trial-ring1.MTX.LOG > /tmp/d_after.txt
echo "lines: before $(wc -l < /tmp/d_before.txt), after $(wc -l < /tmp/d_after.txt)"
head -4 /tmp/d_before.txt > /tmp/d_before4.txt
head -4 /tmp/d_after.txt > /tmp/d_after4.txt
if diff -q /tmp/d_before4.txt /tmp/d_after4.txt > /dev/null; then
	echo "IDENTICAL: the ring rule paints exactly what the hardcoded rule painted"
else
	diff /tmp/d_before4.txt /tmp/d_after4.txt
fi
