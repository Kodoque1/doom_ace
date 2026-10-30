#!/bin/sh
# build.sh - the browser page's files, in docs/challenge/:
#   cc.wasm as.wasm check.wasm sim.wasm   the kit's own C programs, for WebAssembly
#   disk.json                             TinyDoom, its levels, the kit's sources
#   index.html kit.js worker.js           the page, and what runs the programs
# Needs clang with the wasm32 target and wasm-ld (no other C library: web/libc is it).
set -e
cd "$(dirname "$0")/.."
OUT=../docs/challenge
mkdir -p "$OUT"
make -s game
CF="--target=wasm32 -O2 -std=c99 -ffreestanding -nostdlib -nostdlibinc -isystem web/libc -Wall -Wno-unused-parameter"
LF="-Wl,--no-entry -Wl,--export-dynamic -Wl,-z,stack-size=1048576"
clang $CF $LF -o "$OUT/cc.wasm" tools/cc.c web/libc/libc.c
clang $CF $LF -o "$OUT/as.wasm" tools/as.c tools/vm.c web/libc/libc.c
clang $CF $LF -o "$OUT/check.wasm" tools/check.c tools/vm.c web/libc/libc.c
clang $CF $LF -o "$OUT/sim.wasm" web/sim.c tools/vm.c web/libc/libc.c
# the game's listing, for showing which line of tinydoom.c is running
build/as -o /dev/null -l build/game.map build/game.s
python3 web/pack.py "$OUT/disk.json"
cp web/kit.js "$OUT/kit.js"
cp web/worker.js "$OUT/worker.js"
cp web/page.html "$OUT/index.html"
echo "built $OUT"
