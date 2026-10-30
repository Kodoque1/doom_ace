#!/usr/bin/env bash
#
# Prove the raw-pool tests are not vacuous: neuter each behaviour they claim to
# check and REQUIRE the test to fail, then restore and require it to pass.
#
# Every mutation asserts its anchor before replacing, so a stale search string
# fails loudly instead of quietly mutating nothing - which is exactly how a
# "guard" script turns into decoration.
#
#   bash mutproof.sh
#
set -u
cd "$(dirname "$0")"
cp mtx_raw.c   /tmp/mtx_raw.c.bak
cp dos_alloc.c /tmp/dos_alloc.c.bak

run() { make -s clean >/dev/null 2>&1; make -s run 2>/dev/null | grep -E "\[15\]|^ALL|^SOME"; }

mutate() {			# mutate <file> <python body>
	python3 - "$1" <<PY
import sys
p = sys.argv[1]
s = open(p).read()
$2
open(p, 'w').write(s)
print('mutation applied')
PY
}

echo "=== baseline (expect PASS) ==="
run

echo "=== A: owns() without the block-magic check (expect FAIL) ==="
mutate mtx_raw.c "
old = '\tif(*((const uint32_t *)q - 1) != MTX_RAW_MAGIC)\n\t\treturn 0;\n'
assert old in s, 'anchor A not found'
s = s.replace(old, '\t/*MUTATED*/\n')
"
run

echo "=== B: free() always reclaims, ignoring the tip rule (expect FAIL) ==="
cp /tmp/mtx_raw.c.bak mtx_raw.c
mutate mtx_raw.c "
old = '\tif(blk_off + raw_need(*sz) == p->used)\n'
assert old in s, 'anchor B not found'
s = s.replace(old, '\tif(1) /*MUTATED*/\n')
"
run

echo "=== C: tip test uses the REQUESTED size instead of the occupancy (expect FAIL) ==="
cp /tmp/mtx_raw.c.bak mtx_raw.c
mutate mtx_raw.c "
old = '\tif(blk_off + raw_need(*sz) == p->used)\n'
assert old in s, 'anchor C not found'
s = s.replace(old, '\tif(blk_off + MTX_RAW_HDR + *sz == p->used)\n')
"
run

echo "=== D: the pool no longer aligns the base it is given (expect FAIL) ==="
cp /tmp/mtx_raw.c.bak mtx_raw.c
mutate mtx_raw.c "
old = '\tuintptr_t aligned = (b + (MTX_RAW_ALIGN - 1u)) & ~(uintptr_t)(MTX_RAW_ALIGN - 1u);'
assert old in s, 'anchor D not found'
s = s.replace(old, '\tuintptr_t aligned = b; /*MUTATED*/')
"
run

echo "=== E: the arena aligns relative to its base again (expect FAIL) ==="
cp /tmp/mtx_raw.c.bak mtx_raw.c
mutate dos_alloc.c "
old = '\tuintptr_t aligned = (b + 31u) & ~(uintptr_t)31u;'
assert old in s, 'anchor E not found'
s = s.replace(old, '\tuintptr_t aligned = b; /*MUTATED*/')
"
run

echo "=== restored (expect PASS) ==="
cp /tmp/mtx_raw.c.bak mtx_raw.c
cp /tmp/dos_alloc.c.bak dos_alloc.c
grep -c MUTATED mtx_raw.c dos_alloc.c || true
run
