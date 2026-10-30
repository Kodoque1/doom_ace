# The Doomception sandbox

Several copies of a tiny Doom-like game have to share one 64 KiB machine that has no
operating system, and the exercise is to write the scheduler that makes it work.

This is [Doomception](https://kodoque1.github.io/doom_ace/) cut down to a size you can
hold in your head. The problems are the same ones, shared data, code patches,
pointers, a level that misbehaves, and the ring of screens, but they happen on a
made-up computer whose instruction set, C compiler, game and checker all fit in this
folder and build with nothing but a C compiler. The same kit also runs [in the
browser](https://kodoque1.github.io/doom_ace/challenge/).

## Start

You need a C compiler and `make`.

```
make check        # which milestone you have reached
make check M=3    # one milestone
make watch        # two copies taking turns, as text
make play L=5     # level 5 alone; its last frame is build/level5.ppm
```

Your file is [`you/sched.c`](you/sched.c), which has four functions to write, each a
`TODO` with a hint. Everything else is provided, and the checker rebuilds it from
scratch on every run, so changes anywhere but your file have no effect.

## What you are working with

- **The machine** ([`MACHINE.md`](MACHINE.md)) has 64 KiB of memory, eight registers
  and 33 instructions, with no memory protection and no interrupts, so any code can
  write anywhere and nothing takes control away from a running program. Every
  instruction costs a fixed number of cycles, which is what the scores count.
- **The game** ([`game/game.h`](game/game.h)) keeps its code and globals at fixed
  addresses, while each copy's framebuffer, heap and level script are allocated for
  it, with globals pointing at them. Level scripts change the game's behaviour by
  patching call instructions in its code, through a hook table.
- **The kit** ([`kit/kit.h`](kit/kit.h)) starts copies, knows where the globals and
  the hooks are, and copies memory. Your scheduler is level 0's script, so after boot
  it only ever runs when the game calls one of its hooks.

## Milestones

The checker first runs every level alone, then runs copies of them under your
scheduler, and each copy's globals, hooks, heap and framebuffer have to match its
solo run at every frame.

| # | Milestone | The copies | Passes when |
|---|---|---|---|
| M1 | Take turns | levels 1 and 2 | both draw 100 frames |
| M2 | Separate data | levels 1 and 2 | their globals match their solo runs |
| M3 | Separate patches | levels 3 and 4, which patch hooks | their globals and hooks match |
| M4 | Separate pointers | both pairs | their heaps and framebuffers match |
| M5 | Survive | level 1 and level 5, which is hostile | M1–M4 hold |
| M6 | The ring | levels 6, 7 and 8 | each copy's screen-wall shows the next copy's screen |

A scheduler that passes is then scored on the cycles it runs per switch, the bytes
it writes per switch, and the memory it uses. The reference solution copies every
global and every hook on every switch, which costs 2,228 cycles and 1,005 bytes, and
there is a lot of room below that.

## Spoilers

[`spoilers/`](spoilers/) holds the reference solution, along with the deliberately
broken ones that the checker's self-test (`make selftest`) uses to prove each
milestone catches what it should. It is worth solving the puzzle before reading them.

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
