#!/bin/bash
# trial.sh <label> [secs] [--nobuild]
# Rebuild engine+loader, stage into verify/trial-<label>/, run headless, report.
# Keeps every trial's db.log / MTX.LOG / LOG.TXT so experiments are comparable.
set -u
LABEL="$1"; SECS="${2:-22}"; NOBUILD="${3:-}"
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"
REF="$ROOT/verify/after"
D="$ROOT/verify/trial-$LABEL"

mkdir -p "$D"
ln -sf "$REF/DOOM2.EXE" "$D/DOOM2.EXE"
ln -sf "$REF/DOOM2.WAD" "$D/DOOM2.WAD"

if [ "$NOBUILD" != "--nobuild" ]; then
  ( cd "$ROOT/doom_ace" && make -C engine >/dev/null 2>&1 && make -C loader >/dev/null 2>&1 ) \
    || { echo "BUILD FAILED"; exit 1; }
fi
cp "$ROOT/doom_ace/loader/loader.wad" "$D/loader.wad"

"$ROOT/verify/run.sh" "$D" loader.wad "$D/$LABEL" "$SECS" >/dev/null 2>&1

echo "===== TRIAL $LABEL ====="
echo "WAD: $(stat -c%s "$D/loader.wad") B"
echo "ILLEGAL: $(grep -c Illegal "$D/db.log" 2>/dev/null || echo 0)"
echo "DYNX86 : $(grep -c 'DYNX86' "$D/db.log" 2>/dev/null || echo 0)"
echo "--- MTX.LOG (tail) ---"
tail -12 "$D/MTX.LOG" 2>/dev/null || echo "(no MTX.LOG)"
echo "--- LOG.TXT (tail) ---"
tail -3 "$D/LOG.TXT" 2>/dev/null
echo "--- crash CS:IP ---"
grep -o "DYNX86.*" "$D/db.log" 2>/dev/null | head -2
grep -o "CS:IP *[0-9a-f]*: *[0-9a-f]*" "$D/db.log" 2>/dev/null | sort | uniq -c | sort -rn | head -3
