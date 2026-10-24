#!/bin/bash
# trial_levels3.sh - OPTION A, measured: three level WADs on ONE command line.
#
# What this asks that the previous trials could not: with every level's WAD loaded
# by the root at once, does each DEPTH resolve ITS OWN map and ITS OWN screen
# texture? The markers are per level (MTXLEV00/01/02) precisely because
# wad_check_lump searches backward and a shared name would resolve to the last
# WAD at every depth.
#
# Kill conditions, in order:
#   1. the three-WAD command line does not boot at all (texture directory clash:
#      each level WAD carries the exploit's crafted TEXTURE1/PNAMES)
#   2. depth 1 / depth 2 report the same map as the root
#   3. a level reports "no MTXLEVnn marker" or "NOT FOUND" for its screen texture
#   4. depth 1 / depth 2 paint 0 pixels on their screen wall (they are on a map
#      whose screen surface was never planted)
#   5. any illegal read or DYNX86
set -u
cd "${MTX_ROOT:-$HOME/Documents/recursive_doom}"
for p in $(pgrep dosbox); do kill -9 $p 2>/dev/null; done

echo "=== rebuild (abort on failure: a stale binary makes the run meaningless) ==="
make -C doom_ace/engine build 2>&1 | tail -2 || exit 1
make -C doom_ace/loader 2>&1 | tail -2 || exit 1
python3 verify/mkwadlevels.py | tail -1 || exit 1

# The level WADs must be regenerated AFTER the loader, or they carry the previous
# engine's exploit lumps. mkwadlevels.py does that; this only copies them.
rm -f verify/base/level0.wad verify/base/level1.wad verify/base/level2.wad
cp verify/levels/level0.wad verify/levels/level1.wad verify/levels/level2.wad verify/base/

MTX_KEY=y ./verify/run.sh verify/base "level0.wad level1.wad level2.wad" \
	verify/trial-levels3 30 > /tmp/levels3.out 2>&1
cp verify/base/MTX.LOG verify/trial-levels3.MTX.LOG
L=verify/trial-levels3.MTX.LOG

echo "=== illegal $(grep -ac Illegal $L), dynx86 $(grep -ac DYNX86 $L) ==="
echo "=== command line actually used ==="
grep -a "doom2 " verify/base/run.conf
echo "=== per-level markers and maps (the point of the trial) ==="
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
echo "=== if the maps differ, the textures they paint on differ too ==="
grep -a "level: screen texture" $L
echo "=== instances hand-back (both children alive) ==="
grep -ac "instance hand-back at frame" $L
echo "=== last lines ==="
tail -4 $L
