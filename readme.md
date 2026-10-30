# Doomception

**Three Doom games running inside one DOOM2.EXE, each showing the next on a wall.**

DOS has no processes and no threads, so the switching is done by hand, inside the
game: every few frames the running game's data, code patches and pointers are put
away, the next one's are put back, and the CPU is handed over. Only what differs
is copied: about 1,300 of 577,494 bytes per switch.

- **[Read the article](https://kodoque1.github.io/doom_ace/)**: the video, how it
  works, its anatomy, and a map of the code.
- **[Try the challenge](https://kodoque1.github.io/doom_ace/challenge/)**: write
  the scheduler yourself, for a tiny Doom on a tiny made-up computer, in your
  browser. The same kit runs locally: see [`sandbox/`](sandbox/).

By [@KODOQUE](https://x.com/KODOQUE). Built on
[kgsws' ACE Engine](https://github.com/kgsws/doom_ace), which loads new engine code
into DOS Doom 2 through a WAD. Doomception wouldn't exist without it.

## What is where

| | |
|---|---|
| [`nested/`](nested/) | the switch: the scheduler, saving and loading a depth, the loaders ([README](nested/README.md)) |
| [`engine/`](engine/) | the ACE engine, with Doomception's boot, video and ring in `mtx_boot.c` |
| [`loader/`](loader/) | ACE's loader and the WAD it builds |
| [`sandbox/`](sandbox/) | the challenge: a machine, a C compiler, TinyDoom and a checker ([README](sandbox/README.md)) |
| [`docs/`](docs/) | the article and the challenge page, served by GitHub Pages |
| [`verify/`](verify/) | the harness that runs the real game in DOSBox and measures it |
| [`doc/`](doc/) | ACE's own documentation; [`doc/dev/`](doc/dev/) has the development notes |

## Building and running it

You need your own copy of Doom 2 v1.9 (DOOM2.EXE and DOOM2.WAD): no game data is in
this repository. Build `engine` (it builds `nested/` too) and `loader`, then run the
WAD as ACE describes below. The harness in `verify/` does it end to end in DOSBox;
[`verify/HANDOFF.md`](verify/HANDOFF.md) says what a clone needs.

## Licence

ACE Engine is released under the GNU LGPL 2.1 (see [LICENSE](LICENSE)), and so is
everything this fork adds.

---

# ACE Engine for DOOM 2

This is a DOS Doom2 enhancement contained in a single WAD file.

## Version

This code only works in Doom 2 version 1.9.
That is, no The Ultimate Doom, no The Final Doom, no Anthology.
The same version is distributed with SHAREWARE.

(Why are there so many different 1.9 versions?)

## How to run this

Compile `loader` and `engine`.
Engine is the main thing. After compiling you get `code.lmp`.
Loader generates a WAD file with multiple entries. Replace `ACE_CODE` with generated `code.lmp` in any WAD editor.

Resulting WAD file is run just like any other WAD, using command `doom2 -file loader.wad`.

## Documentation

[read here](doc/main.md)

## Code

Code is split into two distinct parts.

### Loader

Two engine quirks are used. First is negative-size lump allocation in ZONE, second is a read past the stack region.
This lets a specially built WAD file load new code into the running game.
TODO: explain the load chain

### Engine

This is a source of the entire ACE Engine. Resulting binary `code.lmp` has to be placed into generated WAD as `ACE_CODE`.  
NOTE: Everytime you pull new version make sure you use command `make clean` before building.
