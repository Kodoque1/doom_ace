#!/bin/bash
# trial_levelsB.sh - OPTION B, measured: the instance exploits itself.
#
# What this asks that trial_levels3.sh (option A) could not: with ONLY the WAD
# that bootstraps ACE on the command line, does each DEPTH reach its level by
# opening a WAD ITSELF, from inside its own copy of the engine, at its first
# level load?
#
# The two trials share everything except the command line, which is the point:
# option A put level0+level1+level2 on it and the ROOT loaded them all once;
# option B puts level0.wad alone on it and a depth that finds its map missing
# opens level<d>.wad. Every level WAD carries the exploit lumps byte-identical
# (verify/mkwadlevels.py asserts the hashes), so a depth that levels up has
# loaded the exploit AGAIN - which is what "the instance exploits itself" means
# here, and it is checked, not asserted: the log line names the file, the number
# of lumps it appended, and where ACE_CODE and the depth's own marker landed.
#
# Kill conditions, in order:
#   1. no "added level<d>.wad ITSELF" line from depth 1 / depth 2 (the mechanism
#      did not fire; the depths are on the command line's WAD or on nothing)
#   2. a depth reports "is already in the lump table (command line)" - that is
#      option A's path, so the command line was not what this trial intends
#   3. ACE_CODE at a negative index in that line (the file was opened, but it is
#      not the exploit)
#   4. depth 1 / depth 2 report the same map as the root, or "no MTXLEVnn marker"
#      / "screen texture ... NOT FOUND"
#   5. depth 1 / depth 2 paint 0 pixels on their screen wall
#   6. any illegal read or DYNX86
set -u
cd "${MTX_ROOT:-$HOME/Documents/recursive_doom}"
for p in $(pgrep dosbox); do kill -9 $p 2>/dev/null; done

echo "=== rebuild (abort on failure: a stale binary makes the run meaningless) ==="
make -C doom_ace/engine build 2>&1 | tail -2 || exit 1
make -C doom_ace/loader 2>&1 | tail -2 || exit 1
python3 verify/mkwadlevels.py | tail -1 || exit 1

# The level WADs must be regenerated AFTER the loader, or they carry the previous
# engine's exploit lumps. mkwadlevels.py does that; this only copies them.
# ALL THREE are copied into the mounted directory, but only level0.wad is on the
# command line: level1.wad and level2.wad are there for the DEPTHS to find, which
# is the whole trial.
rm -f verify/base/level0.wad verify/base/level1.wad verify/base/level2.wad
cp verify/levels/level0.wad verify/levels/level1.wad verify/levels/level2.wad verify/base/
ls -la verify/base/level*.wad

MTX_KEY=y ./verify/run.sh verify/base "level0.wad" \
	verify/trial-levelsB 30 > /tmp/levelsB.out 2>&1
cp verify/base/MTX.LOG verify/trial-levelsB.MTX.LOG
L=verify/trial-levelsB.MTX.LOG

echo "=== illegal $(grep -ac Illegal $L), dynx86 $(grep -ac DYNX86 $L) ==="
echo "=== command line actually used (ONE wad: the bootstrap) ==="
grep -a "doom2 " verify/base/run.conf
echo "=== THE MECHANISM: the add, made by the depth, of the depth's own file ==="
grep -aE "own marker .* (is NOT in|is already in) the lump table|added level|brought in NO lumps" $L
echo "=== did the level's map become the ADDED one? (index must move into the appended range) ==="
grep -a "added level" $L
echo "=== per-level markers and maps ==="
grep -aE "level: marker|level UP|no MTXLEV|screen texture .* NOT FOUND" $L
echo "=== how many distinct maps did the depths open? ==="
grep -a "level UP" $L | sed -e 's/.*map \([A-Z0-9]*\) .*/\1/' | sort | uniq -c
echo "=== diegetic painting, per depth (name and pixels) ==="
grep -a diegetic $L | tail -12
for k in 0 1 2; do
	printf "  depth %s: %s lines, %s with 0 pixels\n" "$k" \
		"$(grep -ac "diegetic: depth $k " $L)" \
		"$(grep -acE "diegetic: depth $k .*\(0 pixels\)" $L)"
done
echo "=== each depth's minutes: ACE_CODE visible to the DEPTH's own table ==="
for k in 1 2; do
	printf "  depth %s added its own wad: %s\n" "$k" "$(grep -ac "depth $k level: added level$k.wad ITSELF" $L)"
done
echo "=== if the maps differ, the textures they paint on differ too ==="
grep -a "level: screen texture" $L
echo "=== instances hand-back (both children alive) ==="
grep -ac "instance hand-back at frame" $L
echo "=== last lines ==="
tail -4 $L
