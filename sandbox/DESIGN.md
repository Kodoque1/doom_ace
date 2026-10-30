# Doomception sandbox: design

**Status:** the local kit is built and its checker passes its self-test; how to
play is in [`README.md`](README.md). The browser page (section 8) is next.

A puzzle kit for experienced developers: run several copies of a tiny Doom-like game
inside one program, on a tiny computer with no operating system support, by writing
the scheduler yourself. It mirrors the real project's lessons at a size you can hold
in your head. A checker tells you which milestone you have reached.

**Principles.** Self-contained: the machine, compiler, game and checker ship in the
kit, written in portable C, and need nothing but a C compiler to build. Deterministic:
same inputs, same run, bit for bit. Checkable: every milestone is a test, not a
judgement.

## 1. The machine: D32

- **Registers:** `r0`–`r7` (32-bit; `r7` is `sp`, `r6` is `fp`) and `pc`. No flags,
  no privileged mode, no memory protection, no interrupts. Switching is cooperative,
  as in the real project.
- **Memory:** 64 KiB, flat and byte-addressed. Everything shares it: code, data,
  stacks, the scheduler, every copy of the game.
- **Instructions:** fixed 32-bit encoding, 33 of them.
  - load and store (byte and word, `[reg+imm]`), `ldi`, `lui`, `mov`;
  - `add sub mul div mod and or xor shl shr addi`;
  - `seq sne slt sle` (compare into a register), `bz bnz jmp jmpr`;
  - `call callr ret push pop`, `sys n` (OS call), `halt`.

  The spec fits on one page, in `MACHINE.md`.
- **Cycle counting:** every instruction costs a fixed number of cycles. That is what
  the scores measure.

## 2. The OS: DOS-like, outside the machine's memory

`sys` traps to the emulator, which provides:

- `load(name)`: copy a file from the kit's disk into memory at a given address;
- `alloc(n)`: a bump allocator;
- `key()`, `present(addr)`: show a 64×40 framebuffer;
- `ticks()`, `print`.

There are no processes and no threads. The OS keeps no state on your behalf, so the
scheduler has nowhere to hide its own state but the shared 64 KiB.

## 3. The game: TinyDoom

- A raycaster drawing 64×40, with a health counter, a level timer and a frame loop:
  input, update, draw, present.
- **Fixed addresses:** code at `0x1000`, the globals right after it. Framebuffer,
  heap and level script come from `alloc` when a copy starts, and their addresses
  are stored in globals, so they are shared pointers.
- **It plays itself:** the player walks on its own (straight on, turning at walls,
  from a seeded random number), so every run is the same run.
- **Map scripts, a feature:** a level file may carry a script section. The game loads
  it at a fixed address and calls it once at level start, with full access to memory.
  Scripts may patch the game's code: a documented hook table at known addresses,
  including the call at the end of every frame. This is the sandbox's ACE, made
  intentional instead of an exploit.
- **Screen-walls:** one wall texture is read through a pointer, so another copy's
  framebuffer can appear on it.

## 4. The compiler: a C subset

The kit's own compiler is `cc` (the same idea as c4; it emits D32 assembly) plus
`as`. TinyDoom is built with it too, so everything in the kit is built by tools in
the kit.

- **Supported:** `int`, `char`, pointers, arrays, `struct`, functions, globals, `if`,
  `while`, `for`, `return`, `#define` and `#include`.
- **Inline assembly** via `asm("...")`, for the context switch.
- **Not supported:** floats and varargs.

## 5. The exercise

You write `you/sched.c`, compiled into a map script. Starting from one running copy
of TinyDoom, it must:

- start more copies (new stack, new heap, new framebuffer, then call the game's
  `level_start`);
- take turns at frame end;
- keep them from trampling each other.

How is up to you.

## 6. The checker: milestones, then scores

**Reference traces.** Each copy is first run alone, and its state is hashed every
frame (globals, its heap, its framebuffer). Under your scheduler, each copy's trace
must match its solo trace exactly. The first frame where they differ, and the bytes
that differ, are reported.

| # | Milestone | Passes when |
|---|---|---|
| M1 | Take turns | two copies both advance at least 100 frames |
| M2 | Separate data | each copy's globals trace matches its solo run |
| M3 | Separate patches | M2 still holds while the copies' scripts patch the hook table differently |
| M4 | Separate pointers | each copy's framebuffer and heap trace matches |
| M5 | Survive | a hostile level script writes everywhere a script may (every hook, chaining `frame_end`; its globals; its free stack), and M1–M4 still hold |
| M6 | The ring | three copies; each one's wall shows the next copy's latest frame, and the last shows the first |

**Scores after a pass:** cycles per switch, bytes copied per switch, and memory used,
each shown on a histogram against reference solutions. Correctness is binary; the
optimisation is open-ended.

## 7. Kit layout

```
tools/     vm.c  run.c  as.c  cc.c  check.c   (one C compiler builds them)
game/      tinydoom.c  game.h  script.h  levels/level1.c ... level8.c
kit/       kit.h  kit.c                       (the plumbing)
you/       sched.c                            (yours, from a starter)
spoilers/  sched.c  broken/                   (the reference, and the self-test's)
make check  ·  make check M=3  ·  make watch  ·  make play L=5  ·  make selftest
```

## 8. The browser page: the same kit, with step controls

`docs/challenge/` on the article's site runs the same C code compiled to WebAssembly,
so it gives the same verdicts as `make check`. It keeps the step-control system of the
first article's 3D machine (`nested/docs/doomception-src`, and the old page's
`ov.js`), applied to the real machine instead of a re-enactment.

- **Controls:**
  - `◀` one instruction back;
  - `❚❚` pause and run;
  - `▶|` one instruction forward;
  - **one frame**: run until the current copy's next frame boundary;
  - **speed**: ½×, 1×, 2× and as fast as possible.
- **Timeline:** a strip of events, coloured by copy (frames, switches, hook calls, the
  first divergence the checker found). Click an event to rewind to it.
- **Rewind:**
  - The machine is deterministic, so rewinding is exact: restore the nearest
    checkpoint, a full 64 KiB plus registers taken every few thousand cycles, then
    replay to the chosen instruction.
  - The old page's per-step checkpoint and instant replay, made exact.
- **Views:**
  - each copy's framebuffer;
  - the memory map, coloured by owner, the last write flashing;
  - the registers, `pc` with the source line from `sched.c`;
  - the checker's milestones.

## 9. How it maps to the real project

| Sandbox | Real project |
|---|---|
| TinyDoom at fixed addresses | DOOM2.EXE's code and data |
| level scripts and the hook table | ACE, loaded through the WAD, patching 406 places |
| `alloc`'d heap, stack, framebuffer | a depth's own memory |
| globals holding those addresses | the pinned pointers (mainzone, screens) |
| your scheduler's state | `mtx_hs`, outside every snapshot |
| M5's hostile script | the ESP = 0 bug |
| scores | the copyless measurements (1,336 of 577,494 bytes) |

## Out of scope for v1

Preemption and timer interrupts, memory protection, the browser version (section 8:
the same `vm.c` compiled to WebAssembly, next), sound.

## Decisions

- **Map scripts are per copy**, as ACE is per depth. Each copy loads its own script
  code at its own address, and each script patches the shared hook table with calls
  into itself. That conflict is M3.
- **64 KiB, on purpose.** The constraint is part of the puzzle: three copies, their
  stacks, heaps and framebuffers, saved copies of the shared data, and the scheduler
  all have to fit.
- **A starter `sched.c`: concept, not grunt work.** The kit provides the plumbing in
  `kit.h`:
  - `copy_start(level)`, which allocates a stack, heap and framebuffer and calls
    `level_start` on the new stack;
  - `hook_get` and `hook_set` for the hook table;
  - `block_copy` and `block_equal`;
  - `GLOBALS_START` and `GLOBALS_END`;
  - the list of shared pointers.

  The starter compiles and runs one copy. What is left are four functions with the
  concepts in them, each a TODO with a one-line hint:
  - `switch_to()`, the registers and the stack pointer;
  - `save(copy)` and `load(copy)`, the data and the patches;
  - `on_frame_end()`, when to switch and to whom.
- **Learned while building it:**
  - A level script may chain `frame_end` (call whatever was there before). So the
    scheduler cannot simply pin `frame_end` to itself on every load: that cuts the
    chain. It seeds each new copy's `frame_end` with itself, and restores each
    copy's own patches, chain included.
  - A pin that must hold from a copy's first frame (the ring's `wall_tex`) only
    works if the game's start-up leaves it alone, so `copy_main` does not reset it.
  - The hostile level changes a patch at the end of its frame, after it has
    drawn: a scheduler that saves patches once, or only at level start, misses it.
