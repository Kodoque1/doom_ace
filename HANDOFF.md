# Matryoshka — the frame-rate problem

Performance handoff. Written 2026-10-24. Everything below is either a measured
number (with the instrument that produced it) or explicitly marked as inference.

## 0. What is being asked

A nested-Doom ring renders, but the picture updates too slowly to look smooth.
Make it smooth. The acceptance test is the **video**, not a bench: a clip in which
the nesting is legible and the motion is fluid.

Current state, measured: **~16 frames/s per instance**, aggregate **48 frames/s**
across three instances; the picture a viewer sees changes **9.1–13.9 times/s**;
**vanilla Doom in the same emulator does 35.6 frames/s**.

## 1. What you do NOT need to touch

**The exploit entry is done and frozen. Do not spend time on it, and do not
re-derive it.** Specifically, all of this works and is out of scope:

- The ACE bootstrap: `doom_ace/loader/loader.wad` (crafted PNAMES + TEXTURE1),
  the loader stubs (`ldr0.S`, `ldr1.S`, `loader.b0/b1`), and how stock
  `DOOM2.EXE` is made to give control to the engine carried in the WAD.
- The level-WAD planting (`verify/mkwadlevels.py`) and how each depth resolves its
  own map and its own screen texture at run time.
- The transport/teleport switch effect and the single-word key authority.
- The nesting itself — each instance's screen-textured walls showing the next
  instance's frame — is the product and must stay visible. *How it is rendered* is
  in scope only if it is what costs the frame time (that is hypothesis H1 below).

Two consequences of that boundary:

- **The running engine lives in the WADs** (`code.lmp`, `loader.wad`,
  `level*.wad`), NOT in `DOOM2.EXE`. Byte-editing `DOOM2.EXE` on disk does
  nothing: two patches were applied to its timer code and both were inert by
  construction (measured, section 5). Fixes belong in the engine, applied by the
  loader at load time. (Project owner's directive, verbatim: *"when I said patch,
  I mean the loader should patch the host, not that you patch the original
  doom2.exe"*.)
- The exploit is also the reason the frame path is reachable at all — treat it as
  a working black box that gets the engine running and the ring up.

## 2. The system, in one paragraph (only what this problem needs)

Several copies of the engine run as sibling *depths* inside one DOS process, each
with its own heap, screen buffer and `.bss`, all sharing Doom's code. They take
turns: a depth runs `MTX_DEMO_INTERVAL` (=4) frames, then hands the frame back to
the host through the host's own service (`mtx_host_take_frame`), and the host
switches to the next depth. **The frame boundary is the only switch point.** On
each switch the outgoing depth's guest state is saved and the incoming depth's is
restored (`nested/dos_state.c`). Each instance renders its own 320x200 frame; each
instance's screen-textured walls sample the next live depth's frame, which is what
makes the nesting visible inside the nested view as well as around it.

## 3. Repos, build, run

| tree | branch | remote |
|---|---|---|
| `doom_ace/` | `inset-wip` | github.com/Kodoque1/doom_ace (PRIVATE) — HEAD `649dfa6`, pushed |
| `nested/` | `master` | github.com/Kodoque1/nested (PRIVATE) — HEAD `c68cfbe`, pushed |

`doom_ace/engine/makefile` has `MTX_DIR ?= ../../nested`, so **clone both side by
side** (`X/doom_ace`, `X/nested`) or build with
`make -C doom_ace/engine MTX_DIR=/path/to/nested`.

```
make -C doom_ace/engine
make -C doom_ace/loader          # repackages code.lmp into loader/loader.wad
python3 verify/mkwadlevels.py    # regenerates level*.wad
```

The harness scripts that produced these numbers are committed under
`verify/` (the 39 probes plus `NEXT.md`, 954 lines). They assume the working tree
is `$HOME/Documents/recursive_doom` with `doom_ace/` and `nested/` side by side;
`record.sh` hardcodes that in `ROOT`.

**Build trap (cost hours once):** `make -C engine` alone leaves `loader/loader.wad`
stale and the game silently runs the PREVIOUS engine. Detect it by checking that a
string you just added is actually present in the built image before believing a
null result.

Run recipe (this is what `verify/record.sh` does, condensed so it can be
reproduced without the scratch harness). DOSBox is
`$HOME/Downloads/dosbox-staging-linux-x86_64-0.83.0-7b400/dosbox`. Write a conf:

```
[sdl]      output=texture, fullscreen=false
[dosbox]   machine=svga_s3, memsize=63        ; 63 MiB is required; 16/24 boot nothing
[cpu]      cpu_cycles=max
[sblaster] sbtype=none                        ; sound OFF: see the pitfall in section 8
[capture]  capture_dir=<dir>/cap
[autoexec] mount c <dir> / c: / doom2 -file level0.wad level1.wad level2.wad -warp 1 > LOG.TXT
```

Then: start Xvfb on a private display, launch `dosbox -conf run.conf --noprimaryconf`,
press Return until the engine's own log says a level is up (the log is the
readiness signal, not a press count), give the window input focus explicitly
(there is no window manager under Xvfb, so nothing assigns it), start DOSBox's own
recorder with Ctrl+F7, optionally hold Up+Right to make the view run, press the
switch key held, and stop the recorder with Ctrl+F7.

### What you must supply yourself (deliberately not in the repo)

- **Doom 2 version 1.9**: `DOOM2.EXE` and `DOOM2.WAD`. The exploit targets 1.9
  exactly. They are copyrighted game data and are not committed. Put them in
  `verify/after/`, or point `MTX_REF` at whatever directory holds them.
- **DOSBox Staging** (the harness was written against 0.83.0), plus `ffmpeg`,
  `xdotool` and `Xvfb` for the headless capture, and `python3` (standard library
  only — no pip installs).
- The harness locates the tree through **`MTX_ROOT`** (default
  `$HOME/Documents/recursive_doom`), so it runs from anywhere once that is set.

### First 30 minutes

```
git clone https://github.com/Kodoque1/doom_ace
git clone https://github.com/Kodoque1/nested          # must be a SIBLING of doom_ace
cp <your>/DOOM2.EXE <your>/DOOM2.WAD doom_ace/verify/after/
make -C doom_ace/engine && make -C doom_ace/loader
python3 doom_ace/verify/mkwadlevels.py
export MTX_ROOT=$PWD

./doom_ace/verify/trial_levels3.sh                    # sanity: ring up, three distinct maps
MTX_WALK=1 ./doom_ace/verify/record.sh walk 30        # a clip and its log
python3 doom_ace/verify/clip_content_rate.py \
        doom_ace/verify/rec-walk/cap/*.avi            # distinct pictures/s, lossless
```

`trial_levels3.sh` is the regression that must stay green: three depths, each on
its own map, each with its own screen texture, 0 illegal reads, 0 DYNX86.

Measure with `verify/clip_content_rate.py <clip>`: it counts how often the
**picture changes**, which is the number a viewer perceives. Read the EXACT figure
from the lossless AVI DOSBox writes, never from the H.264 re-encode — compression
noise makes every "identical" frame differ in low bits (measured: the same clip
scores 8.47/s exact on the AVI and 28.45/s exact on its mp4).

## 4. The problem, with its numbers

- The engine renders **exactly one frame per game tic**: 1.000 tics/frame, deltas
  `{0: 1, 1: 215}` over 216 frames. The frame rate IS the tic rate.
- The frame boundary advances the tic counter itself — **0/44 frames** had the
  counter move *during* the frame's work (intra-frame delta `{0: 44}`). The frame
  path is its own clock; there is no external tick to raise.
- Emulated time (PIT latch at 0x43/0x40, BIOS tick at 0040:006C): 16.13 Hz =
  1.128 BIOS ticks/frame, R ≈ 0.99, jittery (0–4 ticks/frame).
- Distinct pictures/s: **8.47 standing**, **13.90 walking** (`MTX_WALK=1`), and a
  re-measurement this session gave **9.09/s** walking (lossless AVI, 1319 decoded
  frames, 171 distinct). Session-to-session spread is ~40% — do not chase small
  differences.
- `cpu_cycles` sweep: `max` 16.00, `250000` 16.00, `250000` 16.00, `500000` 12.00,
  `2000000` 4.00, `max` 16.00.
- Per-switch state cost: **572,945 B registered per depth, 4,000 B actually differ
  (0.6%)**. Each switch compares all 572,945 B twice (save + load) and copies only
  the differing blocks. 3 switches per 12-frame round ⇒ ~72k guest instructions per
  frame — a few percent of a frame.
- Paint: ~46,000 px per 12-frame round. Self-check: a 16,000-pixel sample every 50
  frames.

## 5. Levers eliminated (each with its measurement)

| hypothesis | test | result |
|---|---|---|
| emulated CPU too slow | `cycles=max`, dynamic core, fastest CPU type | already at ceiling |
| CPU-bound | `cpu_cycles=250000` (half the machine) | frame rate identical: 16.00 |
| host PIT divisor | patch `mov eax,0x84d0` (34000 = 35.09 Hz) → 11333 | inert, 16.00/s |
| host timer install | patch divisor 2130 → 710 | inert, 16.00/s |
| VGA retrace wait | force `I_WaitVBL`'s skip conditional (`75`→`eb`) | inert, 16.00/s |
| context-switch cost | slice 4 → 16 frames | throughput identical |
| recorder overhead | capture off vs on | identical |
| host load | sampled under a cap | core pegged, rate unchanged |

**The `cpu_cycles` anomaly, resolved.** The sweep fits `rate = 16 ×
min(1, host/cap)`: DOSBox paces the *emulated clock* by the cap, so above the
host's throughput everything scales down. Fitting the two scaled points puts the
host at ≈375–500k cycles/ms — i.e. `250000` sat right at the host's throughput,
which is why capping appeared to do nothing. Corrected conclusion: **no single
instance is CPU-bound, but the machine is.** Vanilla Doom at 35.6/s is
*clock*-bound (its 35 Hz PIT) and uses roughly a third of the host; the ring is
past the clock and into the host's limit.

## 6. The open question, and the two hypotheses

Per-instance frames cost **≈2.2× a vanilla frame** (35.6 / 16), yet the ring's own
bookkeeping accounts for only a few percent (section 4). So the cost is elsewhere.

- **H1 (favourite, unmeasured):** the per-frame **composite**. Each instance
  composites its parent's framebuffer into its screen-textured walls every frame,
  so an instance's frame is one Doom frame plus a full-view composite — paid three
  times per round. If this is it, the lever is in the screen-wall path (composite
  less often, or at a coarser granularity), and the nesting stays visible.
- **H2 (policy, not a bug):** the three instances each get 1/3 of the aggregate,
  by construction of the round-robin. Raising the aggregate (by removing H1's cost)
  raises every instance's share; changing the slice length only changes *apparent*
  smoothness (measured: slice 4 gives 13.90 distinct/s, slice 16 gives 7.04).

**Next measurements, in order:**

1. Instrument `mtx_dos_save_state` / `mtx_dos_load_state` AND `mtx_screen_paint`
   in emulated time, per frame, and compare against a frame with the composite
   suppressed. This is the breakdown the arithmetic cannot give.
2. A **solo baseline** — the engine's own frame rate with no ring — needs a
   rebuild with `MTX_MAX_DEPTH=1`. Do not try `MTX_LEVELS=0`: it does NOT isolate
   an instance. Measured this session: with the staged level WADs absent the depths
   still come up, on `DOOM2.WAD`'s own MAP02/MAP03, and the ring forms as usual.
   The harness comment claiming otherwise is wrong.

## 7. Where the frame path is

- `doom_ace/engine/vesa.c` — `swap_buffers()`, hooked at `0x1D4A6` / `0x1D4FC`,
  calls `mtx_engine_frame()`.
- `doom_ace/engine/mtx_boot.c` — `mtx_engine_frame()` (~line 2977): the per-frame
  path. For an *instance* (`me != NULL`) it runs `mtx_engine_self_check()`,
  `mtx_screen_paint()`, points its blit at `present_src`, handles the switch key,
  and every `MTX_DEMO_INTERVAL` frames calls `me->host_yield()`. For the *root* it
  boots, then `mtx_engine_policy()` → `mtx_frame_tick()`.
- `doom_ace/engine/render.c` — the seg-note hook (`mtx_screen_seg_note`) and the
  screen-textured walls: **this is where H1 lives**.
- `doom_ace/engine/draw_c.c` — `hook_finish_update`; two calls inside the host's
  `I_FinishUpdate` are NOP'd (`0x1D31D`, `0x1D4F2`).
- `nested/mtx_sched.c` — request/consume at the frame boundary (the only switch
  point). `nested/mtx_core.c` — the switch itself. `nested/dos_state.c` —
  save/load, block-tracked at 32 B (`MTX_BLK`), `MTX_COPYLESS=1`,
  `MTX_WARMUP=0xFFFFFFFF` (the full compare runs on every switch by design: the
  cached/hot-set path was measured UNSAFE — 1000 illegal reads and only 7 blocks
  promoted before the damage was visible). `nested/dos_wire.c` — what gets
  registered: Doom's mutable var region (`doom_data_segment + 0x1A84`, 475,676 B),
  the engine's `.bss`/`.data`, and every patch ACE installed.
- `nested/mtx_params.h` — the per-depth parameter block (depth, zone, screen,
  `present_src`, `screen_src`, `host_yield`, frame counter).
- `doom_ace/engine/main.c` (~770–900) — the documented ACE hook table. It never
  touches the timer or the tic loop.
- Instruments already in the engine (`mtx_boot.c`, committed in `649dfa6`): `t=`
  (I_GetTime), `raw=` (direct read at `doom_data_segment + 0x2913c`), `gametic`,
  `leveltime`, `lt=`, `intra d=`, PIT/BIOS emulated-clock helpers, work duration.
  Note in the code: the work-duration window can span the depth's hand-back to the
  host, so it is not a clean work-bound measurement.
- Addresses found by disassembly (file offsets in `DOOM2.EXE`; file offset = load
  address + 0x42010 in the code region): timer install `0x5c8f4`, called from
  `0x5c95e` with `mov eax,0x84d0` (= 34000 = 35.09 Hz, validated 1..149 Hz);
  `I_GetTime` `0x5bc20` (`mov eax,[0x2913c]; ret`); IRQ0 handler `0x5c8c0`;
  `I_WaitVBL` `0x5bc74` (retrace spin on port 0x3DA, skip flag `[0x29100]`).

## 8. Rules that will otherwise cost a day

- **Build:** engine + loader + `mkwadlevels.py`, always (section 3).
- **Image identity:** before patching an image, prove the running engine is in it.
- **A repeating fill is a bad address, not a measurement.** `0xCDCDCDCD` (also
  `0x00`, `0xFF`) means the address is wrong — the game image is relocated at load;
  read its data as `doom_data_segment + offset`.
- **Instruments must record their own conditions.** A flag is an intent; the AVI is
  the fact. A report that cannot contradict a claim about how it was produced is
  not evidence.
- **Measure the AVI, not the mp4** (section 3), and respect the ~40% session noise.
- **Exact-name process kills only:** `pkill -f Xvfb` matched the agent's own shell
  command line and killed it. Use `pkill -x dosbox` / `pkill -x Xvfb`.
- **Sound-on runs die** with `NetUpdate: netbuffer->numtics > BACKUPTICS`. The
  likely mechanism is that a Sound Blaster is IRQ-driven and its DMA handler runs
  in whichever depth's context is loaded — a per-depth context switch under an
  interrupt handler is not isolated yet. Opt in with `MTX_SOUND=1` to reproduce.
- `verify/` is a **scratch tree, not under version control** (39 scripts +
  `NEXT.md`, 954 lines, 260 KB; the 691 MB in that directory is recorded clips).
  `verify/NEXT.md` is the fullest record of what has been measured and eliminated,
  including retractions.
