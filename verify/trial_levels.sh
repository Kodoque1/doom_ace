#!/bin/bash
# trial_levels.sh - prove the level WADs end to end, in DOSBox, both ways.
#
# level0.wad must be indistinguishable from loader.wad: same exploit, plus one
# marker lump the engine ignores. level1.wad carries the same exploit and a
# DIFFERENT map, so the engine must boot identically and then report a different
# diegetic surface (the texture it painted before is a MAP01 texture, and MAP01 is
# no longer what is loaded).
set -u
cd /home/kodoque/Documents/recursive_doom
for p in $(pgrep dosbox); do kill -9 $p 2>/dev/null; done

runone() {   # runone <wad> <tag>
	local wad="$1" tag="$2"
	./verify/run.sh verify/base "$wad" "verify/trial-$tag" 20 > "/tmp/$tag.out" 2>&1
	cp verify/base/MTX.LOG "verify/trial-$tag.MTX.LOG"
	echo "--- $tag ($wad): illegal $(grep -ac Illegal verify/trial-$tag.MTX.LOG), dynx86 $(grep -ac DYNX86 verify/trial-$tag.MTX.LOG)"
}

runone level0.wad lvl0
runone level1.wad lvl1

for t in lvl0 lvl1; do
	echo "=== $t: boot + diegetic ==="
	grep -aE "reserved|isolation|diegetic|switch:" "verify/trial-$t.MTX.LOG" | head -8
done

echo "=== boot lines side by side ==="
grep -a MTX verify/trial-lvl0.MTX.LOG | head -20 > /tmp/boot0.txt
grep -a MTX verify/trial-lvl1.MTX.LOG | head -20 > /tmp/boot1.txt
if diff -q /tmp/boot0.txt /tmp/boot1.txt > /dev/null; then
	echo "IDENTICAL: both WADs booted the same exploit the same way"
else
	diff /tmp/boot0.txt /tmp/boot1.txt
fi
ls -la verify/trial-lvl0_*.png verify/trial-lvl1_*.png 2>/dev/null
