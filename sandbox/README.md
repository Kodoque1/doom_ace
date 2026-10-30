# The Doomception sandbox

Several copies of a tiny Doom, one 64 KiB machine, no operating system to help.
Write the scheduler that makes them share it.

It is [Doomception](https://kodoque1.github.io/doom_ace/) at a size you can hold in
your head: the same problems (data, patches, pointers, a hostile level, the ring)
on a made-up computer where everything, from the instruction set to the C
compiler, fits in this folder.

## Start

You need a C compiler and `make`.

```
make check        # which milestone you have reached
make check M=3    # one milestone
make watch        # two copies taking turns, as text
make play L=5     # level 5 alone; its last frame is build/level5.ppm
```

Your file is [`you/sched.c`](you/sched.c): four functions, each a `TODO` with a
hint. Everything else is given, and the checker builds it fresh, so change only
your file.

## What you are working with

- **The machine** ([`MACHINE.md`](MACHINE.md)): 64 KiB of memory, eight registers,
  33 instructions, no protection, no interrupts. Every instruction costs cycles.
- **The game** ([`game/game.h`](game/game.h)): TinyDoom's code and globals sit at
  fixed addresses. Each copy has its own framebuffer, heap and level script; the
  globals point at them. Level scripts patch the game's code through a hook table.
- **The kit** ([`kit/kit.h`](kit/kit.h)): starting a copy, the hook table, where the
  globals are, copying memory. Your scheduler is level 0's script, so it runs only
  when the game calls one of its hooks.

## Milestones

The checker first runs every level alone, then runs copies under your scheduler.
Each copy's globals, hooks, heap and framebuffer must match its solo run at every
frame.

| # | Milestone | The copies | Passes when |
|---|---|---|---|
| M1 | Take turns | levels 1 and 2 | both draw 100 frames |
| M2 | Separate data | levels 1 and 2 | their globals match their solo runs |
| M3 | Separate patches | levels 3 and 4, which patch hooks | their globals and hooks match |
| M4 | Separate pointers | both pairs | their heaps and framebuffers match |
| M5 | Survive | level 1 and level 5, which is hostile | M1–M4 hold |
| M6 | The ring | levels 6, 7 and 8 | each copy's screen-wall shows the next copy's screen |

Then three scores: cycles your scheduler runs per switch, bytes it writes per
switch, and memory it uses. The reference solution copies everything on every
switch: 2,239 cycles and 1,017 bytes. You can do better.

## Spoilers

[`spoilers/`](spoilers/) holds a reference solution, and broken ones the checker's
self-test uses (`make selftest`). Look after you solve it.

## How it maps to Doomception

| Sandbox | Doomception |
|---|---|
| TinyDoom at fixed addresses | DOOM2.EXE's code and data |
| level scripts and the hook table | ACE, loaded through the WAD, patching 406 places |
| a copy's framebuffer, heap and stack | a depth's own memory |
| the globals pointing at them | the pinned pointers |
| your scheduler's state | `mtx_hs`, outside every snapshot |
| the hostile level | the ESP = 0 bug |
| the scores | the copyless measurements: 1,336 of 577,494 bytes |

The design notes are in [`DESIGN.md`](DESIGN.md).
