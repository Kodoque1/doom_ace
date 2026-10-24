# recursive_doom — where the work stands, and what to do next

Written at the end of a long session (doom_ace HEAD + working tree, nested c585b94) so a
fresh session can continue without re-deriving anything. Short on purpose: the code and
the commits carry the detail, this carries the STATE and the ORDER.

## What is done and measured

- **Three live levels.** `MTX_RING_CHILDREN 2u` in `doom_ace/engine/mtx_boot.c`
  (root + 2 instances). Each child gets its own arena slot — own image, zone (4 MiB),
  screen, raw pool (2 MiB), stack. Arena is 28 MiB (`mtx_engine_reserve`), which is what
  makes the planner plan 3 depths: `per_depth ~7.2 MiB`, it FLOORS the division.
- **The ring is closed in the pictures.** Level d's screen-walls show level d+1's
  frame, and the deepest shows the ROOT's — the wrap. `mtx_ring_next()` in `mtx_boot.c`
  drives the picture AND the turn order, so the schedule is 0 -> 1 -> 2 -> 0.
- **OPTION A IS DONE AND MEASURED: each level opens its OWN MAP.**
  - One command line, three WADs: `doom2 -file level0.wad level1.wad level2.wad -warp 1`
    (`verify/run.sh`). Each level's map keeps the name it was borrowed from — MAP02 in
    level1.wad, MAP03 in level2.wad — so no file overrides another
    (`wad_check_lump` searches backward).
  - The root loads them once; each depth picks its own by number. `mtx_demo_child()`
    sets Doom's `startmap = depth + 1` at its first entry, while that depth is the
    running one, and `do_autostart_game()` (map.c) turns it into `MAPnn`.
  - The engine hides EVERY PWAD copy of `TEXTURE1`/`TEXTURE2`/`PNAMES`, never the
    IWAD's (`wad_hide_lump` in wadfile.c). Hiding only the winning copy revealed the
    next PWAD's crafted decoy table and the boot died in R_Init with
    "Texture OVERADDR is missing patch!" — measured, and the reason this had to change.
  - Each level resolves its OWN screen texture from its own marker lump
    (`MTXLEV00/01/02`): 189 PIPE4 on MAP01, 11 BIGBRIK2 on MAP02, 140 MARBFAC2 on MAP03.
    `mkwadlevels.py` FAILS AT BUILD TIME if a screen texture is not defined by the
    IWAD (an unknown name loads, draws as "unknown texture" and silently paints 0).
  - **Evidence, picture:** `verify/rec-levelsA/levelsA.mp4` (20 s). Frames 4.5 s / 8.5 s
    / 16.5 s show three visibly different rooms, and at 16.5 s the inset contains ANOTHER
    inset — the nest is two levels deep on screen, not a hall of mirrors.
  - `verify/trial_levels3.sh` re-runs the whole check and prints the kill conditions.
- **OPTION B IS DONE AND MEASURED: THE INSTANCE EXPLOITS ITSELF.** This is the version
  where reaching level 3 means level 2 loaded the exploit again.
  - Command line is the BOOTSTRAP ONLY: `doom2 -file level0.wad -warp 1`. Each depth
    opens `level<d>.wad` ITSELF, from inside its own copy of the engine, at its first
    level load — `mtx_level_wad_add()` called from map.c:1203, before the marker read and
    before the map lump lookup, so the appended file supplies BOTH the depth's MTXLEV0d
    marker and its MAPnn. `wad_add` in wadfile.c is an API for this (it was `static`).
  - The trigger is a TEST, not a mode: the depth adds its file only when its OWN marker
    is not in its lump table. So option A is untouched by the code (`trial_levels3.sh`
    still green, and its log says per depth "own marker MTXLEV01 is already in the lump
    table (command line); no own WAD added"), and the whole difference between the two
    versions is one command line — which is what makes their measurements comparable.
    The test is the MARKER, not the map: `MAP02`/`MAP03` are REAL MAPS IN DOOM2.WAD, so
    "the map is present" is true whether the depth has anything of its own or not.
  - Every level WAD still carries the exploit lumps byte-identical (mkwadlevels.py
    asserts the hashes), so the add is "the exploit, loaded again, by the depth".
  - **Evidence, log:** `verify/trial-levelsB.MTX.LOG` — per depth
    `own marker MTXLEV0d is NOT in the lump table; adding its own WAD`,
    `added level<d>.wad ITSELF: +19 lumps (now 2946), map MAP02 17 -> 2934 (the added
    copy wins), ACE_CODE at 2927, MTXLEV0d at 2945`. The map index MOVING out of the
    IWAD's range is the point: index 17 was the IWAD's own MAP02 and the level would
    have loaded it.
  - **Evidence, log:** three distinct maps (MAP01/02/03), `gamemap 1/2/3` (identical to
    A), three textures (189/11/140), and at steady state all three depths paint their
    own surface: frame 30/60/90 → depth 0 12332 px, depth 1 3352 px, depth 2 29057 px.
    45 hand-backs, 0 illegal, 0 DYNX86.
  - **Evidence, picture:** `verify/rec-levelsB/levelsB.mp4` (24 s) and its frames in
    `verify/rec-levelsB/f_*.png`; `f_8.png` shows the inset containing a second inset
    (the recursion, legible). Recorded with `MTX_WADS="level0.wad" MTX_WALK=1
    ./verify/record.sh levelsB 24` — `record.sh` now takes the command line's WAD list
    from `MTX_WADS` (default = option A's three).
  - `verify/trial_levelsB.sh` re-runs the whole check and prints the kill conditions.
- **Host tests:** `make -C nested run` = 15/15. `make -C nested doscheck` clean.
- The 4 `Illegal write to 12345680` lines in db.log are PRE-EXISTING in every recording
  (the `I_FinishUpdate` placeholder in asm.S before `init_draw()` patches it); they are
  not a regression from the level work.

## THE SWITCH IS A TRANSPORT NOW, NOT AN OVERLAY

The screen shows ONE instance's frame, full screen, and the key MOVES the viewer: from
the root into depth 1, then depth 2, then back to the root - the ring's own order, the
one its screen-walls already show (`mtx_ring_next`). Nothing is composited on top of
anything.

- **The inset is gone.** `mtx_engine_switch` no longer draws a rectangle: the host names
  ONE buffer for every depth's blit (`mtx_engine_video`), and WHICH buffer that is, is
  what the switch changes (`mtx_hs.transport`). The old inset constants, `inset_on`,
  `inset_depth`, `inset_frame` and `inset_logged` are gone with it; the depths' block
  carries `transport` instead. The nested views are not lost - they are the
  screen-textured walls INSIDE each level, which the level WADs plant and
  `mtx_screen_paint` fills from the ring's next depth.
- **The teleport effect** (`mtx_fx_build`, `MTX_FX_FRAMES 8`): a bright band sweeps down
  the screen, rows above it coming from the frame being ENTERED and rows below from the
  frame being LEFT, and it ends exactly on the new frame so the hand-over to the live
  view is invisible. It is built into a buffer of its own (`mtx_hs.fx_buf`, from the
  root's zone), NOT into either frame: the frame being entered is live (its instance is
  rendering into it) and the frame being left is still shown on a wall.
- **Evidence, clip:** `verify/rec-clip/clip.mp4` (18 s, DOSBox's own capture), three
  presses, three transports: `transport depth 0 -> 1 at frame 137`, `1 -> 2 at 198`,
  `2 -> 0 at 250` - the loop closes. The contact sheet shows the root's room, then
  MAP02's dark hall, then MAP03's wooden corridor, then the root's room again, with no
  overlay in any frame; `sw_03.png` catches the transition with the bright band and two
  different pictures above and below it.
- **The tour is GONE.** It was the stand-in for an untrusted key; the transport is
  user-driven now, so an automatic cycle would move the viewer without a press.

## THE RECORDING NO LONGER TEARS - AND THE TEAR WAS THE CAPTURE, NOT THE GAME

The clips used to be captured with ffmpeg's x11grab on the DOSBox window. The emulated
screen is written by a plain `rep movsl` into video memory and DOSBox presents the window
from that memory at its own frame boundary, so an X11 grab can read the window while it
is being updated - that is the horizontal tear, and it is a capture artifact rather than
a display bug. DOSBox Staging can record the EMULATED FRAMEBUFFER at frame boundaries
instead (`Ctrl+F7`, lossless AVI/ZMBV), which cannot contain a half-updated screen, so
`verify/record.sh` now uses that and converts the AVI with ffmpeg. The frame-accurate
capture is also what lets the effect be inspected frame by frame.

The harness needed three fixes of its own, each from a measurement:
- **The window must be GIVEN the input focus** (`xdotool windowfocus`). Under Xvfb there
  is no window manager, so nothing assigns it and SDL takes it whenever it likes:
  measured, whole runs with the ring running normally (102 hand-backs, three levels up)
  and ZERO key sightings - the game never saw a press.
- **The readiness signal is the DEEPEST level being up**, not the root's: the root's
  level is up at frame 1, so waiting for it started the capture while the game was still
  on the title (measured: a switch at frame 1).
- **Each switch is retried until the ENGINE logs it**, and a full press/release cycle is
  sent between attempts. The key release is what gets lost (measured: three presses, one
  switch, with the depth key sightings running continuously from the first press to the
  end - the release never arrived, so the next press was not a leading edge). The harness
  now reports "switches asked N, switches the engine logged M" and never presents a
  clip that silently shows fewer switches than were asked for.

## THE TELEPORT SOUND: IMPLEMENTED, AND THE EMULATED DEVICE IS THE BLOCKER

The switch plays Doom's own teleport sound, resolved by NAME out of Doom's own sound
table (`sfx_id_by_name` in sound.c, "telept" - no magic index) and started by the HOST
(`mtx_fx_start`), because the host is what decided the switch and the device is the
root's. A nested instance is explicitly silent (`init_sound` zeroes its `snd_sfxdevice`),
so the machine's device is not reset three times.

**What does not work: the emulated Sound Blaster in a three-instance run.** With the
device enabled every attempt died before the children booted, with `NetUpdate:
netbuffer->numtics > BACKUPTICS` - Doom's tic buffer overflowing, which is what a frame
loop that cannot keep up looks like from inside the game. Measured on four runs, with and
without the video capture, and with the depths' own device already zeroed - so the ROOT's
device alone is enough. The likely mechanism is that a Sound Blaster is IRQ-driven: its
DMA completion handler runs at interrupt time in WHICHEVER depth's context is loaded, and
per-depth context switching under an interrupt handler is exactly what this engine has
not isolated. That is the next real piece of work for sound, not a harness flag.

So sound is OPT-IN (`MTX_SOUND=1` in record.sh, which enables the SB in DOSBox and writes
Doom's own `default.cfg`): the delivered clip is silent, and the sound is evidenced by the
log (`sound 35` on every switch - the resolved id passed to `S_StartSound`). Note the
config lesson: a WAD-carried `sound.device.sfx` in ACE_CONF does NOT reach `snd_sfxdevice`
(ACE only fills it in when Doom's is zero, and it was measured to stay 0); Doom's own
config file is what works.

## THE SWITCH KEY IS FIXED, AND THE INPUT PATH IS MEASURED

**The measurement first, because the fix follows from it.** The BIOS keyboard ring at
0040:001A does NOT move while the key is held: sampled every frame of a press, head and
tail stayed equal (both 32). So nothing machine-global carries the input at all — Doom
1.9 installs its own INT 9 handler and does not chain to the BIOS one, and the ring is
dead after that. The only channel is Doom's own key array, and that array is Doom's
DATA: it is snapshot state, so **each depth has its own copy** and a press lands in
whichever copy was loaded when the interrupt arrived. "Is the key down" therefore has no
single answer unless one is kept outside the snapshots.

**The fix: one authority word.** `mtx_hs.key_state` — host memory, reachable from the
root directly and from a depth through the new `params->key_auth` — and the same rule on
both sides:
1. at the FIRST frame of a slice, the loaded copy is FORCED to agree with the authority
   (`mtx_key_adopt()`), before anything reads it. The restored value is what this depth
   last saw and the events since went to another copy, so it is not merely
   untrustworthy — it is knowably wrong, and the authority is the one value that is not.
2. every frame, an edge is just `copy != authority` (`mtx_key_observe()`). Whoever sees
   it writes the authority and reports it — the host toggles, a depth sets `switch_req`.
   Because the authority is shared, the other side cannot report the same press again.

Both older mechanisms are GONE, and they were the same mistake in different clothes: a
per-depth tracker CLEARED at each slice start (a still-held key then re-asserts itself
into the restored copy and fires a second edge) and a dead-time window (which only
delays that second edge, and at a 4-frame slice leaves barely a frame that may see the
key at all). `MTX_KEY_GUARD_FRAMES`, `MTX_KEY_DEBOUNCE_FRAMES` and `mtx_key_track` no
longer exist. Note `MTX_DEMO_INTERVAL` stays 4 — the key is no longer the reason, the
motion is.

**Evidence.** `verify/trial_key.sh`, two HELD presses in one run (a tap can fall between
two frames, so the recorder holds the key):
- exactly TWO `trigger: key` toggles, 70 frames apart — one per press, so one held press
  is one toggle AND the re-arm works (the half that "clear the state properly" broke
  earlier: measured then as zero switches for a whole run);
- the authority word reads 0 between the presses, i.e. the release was observed and the
  second press was a genuine leading edge;
- 0 illegal, 0 DYNX86.
Both paths are covered by evidence: in that run the HOST's own copy saw both edges
(`own edge 1`), and in `verify/rec-keyfix` a CHILD saw it (`child req 1`) — the two
columns of the toggle log line say which, per toggle:
`inset -> depth N at frame F (trigger: key|TOUR; authority A, child req R, own edge E)`.
Before the fix the same one-press recording read `inset -> depth 1` at frame 285 and
`inset -> depth 2` at 289.

**The tour is gone** (see the transport section): with the key trustworthy and the switch
user-driven, an automatic cycle would move the viewer without a press. The toggle log line
is now `transport depth A -> B at frame F (authority ..., child req ..., own edge ...,
effect 8 frames, sound 35)`.

## WHY THE CLIP LOOKS LOW-FRAMERATE - MEASURED, AND IT IS THE RING'S PRICE

The container is not the problem: `ship-final.mp4` holds 2101 frames at 70.09 fps over
30 s. That rate is the EMULATED MODE'S REFRESH (VGA mode 13h is 70 Hz), because DOSBox's
recorder writes one frame per PRESENTED frame - so the file claims ~70 fps no matter how
fast the game actually renders, and ffprobe can never see the difference.

What is low is the PICTURE-CHANGE rate: **8.47 distinct pictures/s** in that clip (mean
8.24 identical frames between changes). Two instruments, both in `verify/`:
- `clip_content_rate.py <clip|avi>` - decodes every frame and counts how often the
  picture really changes. The number a viewer perceives; ffprobe cannot see it. It
  reports TWO counts, and the difference matters: the EXACT one (any byte differs) is
  the truth on the lossless AVI DOSBox writes, and is NOISE on an mp4 re-encode -
  measured, the same clip scores 8.47/s exact on the AVI and 28.45/s on its mp4, i.e.
  quoting the exact number off a lossy file is a false measurement. The TOLERANT one
  (pixels differing by more than compression noise) is encoding-independent: 4.14/s on
  the AVI against 3.84/s on its mp4, so it is the one to compare across clips.
- `fps_probe.sh <label> [secs]` - polls the ENGINE's own frame clock (the root's
  `[MTX] frame N` lines, incremented once per game frame in `mtx_engine_policy`)
  against the host's wall clock, live. `MTX_CAP=1` adds the recorder to the run.

Measured, and each one kills a hypothesis:
- **The recorder is INNOCENT.** The root's frame clock runs at 16 frames/s with the
  recorder OFF (`fps-off.fps.txt`) and 16/s with it ON (`fps-on.fps.txt`, whose report
  records its own conditions and whose DOSBox log shows the recorder really ran: 470
  frames over 6.7 s). In that run the rate is 16/s both while the recorder is capturing
  and after it stops, so the comparison holds inside one run as well. ZMBV compression
  at 320x200 is not what costs the frames.
- **The ring divides the machine, and divides it evenly.** In one sample the root, depth
  1 and depth 2 counters all stood at frame 300 - each instance gets an equal slice,
  ~16 frames/s each.
- **Doom's own loop wants 35 frames/s**, so each instance runs at about half its normal
  rate: three levels means a third of the machine per level. The block comment on
  `MTX_DEMO_INTERVAL` already recorded this from the two-depth days ("the outer view ran
  13.8/s" at 4-frame slices, "a ring pays a third of either per level"); this measurement
  reproduces it independently.
- **The emulated machine is the limit, not a setting.** DOSBox's CPU emulation is pegged
  at 100% of one core during a run, and `cpu_cycles=max` is already there.
- **Half the rendered frames are pixel-identical because the player never moves.**
  Walking (`MTX_WALK=1`) raises the distinct-picture rate from 8.47 to **13.90/s**
  (+64%) - `verify/rec-walk/walk.mp4`, 34 s, three transports (frames 101/167/326) -
  against a render rate of ~16/s.

So the clip is a faithful 70 fps record of a machine rendering ~16 frames/s on a scene
that changes ~8.5 times/s. Not a capture bug and not an encoding bug: the nest's cost.
Levers, cheapest first: walk in the clip (free, measured +64%); fewer levels (breaks the
ring - `MTX_RING_CHILDREN` and the arena size are ONE decision); presentation only (drop
the repeated frames and stamp the content rate, or interpolate - that changes what the
file claims, not what the machine does).

## CAN THE DOSBOX MACHINE BE "OVERCLOCKED"? NO - IT IS ALREADY AT THE CEILING

Asked after the frame-rate finding. The answer is in DOSBox's own help text, then tested:

- `cpu_cycles=max` is already set, and DOSBox defines it as "Emulate as many cycles as
  your host CPU can handle on a single core". That IS the ceiling; there is no higher
  setting to reach for.
- The key that governs the GAME is `cpu_cycles_protected`, because Doom runs in
  protected mode (DOS/4GW). db.log complains that its default 60000 is invalid with
  `cycles=max` and falls back to 'auto' - and 'auto' means "Use the `cpu_cycles'
  setting", i.e. max. So the protected-mode path is NOT throttled; it already inherits
  max. (This was the most promising lead and it turned out to be a non-issue.)
- `core=auto` (the default) = "'normal' core for real mode programs, 'dynamic' core for
  protected mode programs" - the game already runs on the dynamic core, which the same
  help says is 3-5x cheaper on the host than the interpreter.
- CPU type default 'auto' = "the fastest and most compatible setting" (386_fast plus 486
  CPUID, CR behaviour and extras).
- The host core is saturated: 100% of one core for the whole dosbox process in a run.

Two config variants measured against the 16 frames/s baseline, both no-ops:
- `output=surface` (the Xvfb host has no GPU, so the texture path may be software):
  12-16/s, unchanged.
- `machine=svga_et4000`: 12/s, unchanged.
So the video path is not what costs the frames either.

And the FALSIFICATION test, which is what makes this a measurement rather than a story:
`core=normal` forces the interpreter for protected mode and is documented to cost 3-5x
more host CPU per instruction. It did not merely slow the game down - the guest never
produced a SINGLE frame in the same window (no `[MTX] frame` line at all in 60 s, against
856 lines of engine log in the baseline run; db.log shows it still in mode 13h, then back
to text mode when the harness killed it). A rate that collapses when host work per
instruction rises, and does not move when the video path changes, is host-CPU-bound.

So there is no overclock to hand over. The levers are only about needing fewer host cycles
per second: fewer levels (the ring divides the machine three ways), less per-frame work in
the engine (the nested screen paint is the effect's own cost; the instrumentation is
already gated to every 50 frames), a faster host core (DOSBox's CPU emulation is
single-threaded), or - for the delivered clip - walking the player, which is free and
measured at +64% distinct pictures.

The hook that made the variants one-liners: `MTX_CONF_EXTRA` in record.sh (extra config
verbatim, inserted before [autoexec]; DOSBox lets a later section override an earlier one,
so a variant can restate a whole section). Reports: `verify/var-surface.fps.txt`,
`verify/var-et4000.fps.txt`, `verify/var-normalcore.fps.txt`.

## THE EMULATED CPU IS *NOT* THE SCARCE RESOURCE - RETRACTED (measured with Doom's own benchmark)

Asked after the frame-rate finding, because "a 1994 game lagging on a 2020 computer" is
a fair complaint. The measurements, in the order that makes them mean something:

- **Plain Doom alone (no ACE, no ring, no engine work): 35.59 distinct pictures/s**
  (`verify/rec-plain`, attract-mode demo so the picture rate IS the render rate). That
  is Doom's own 35 Hz tic cap exactly - the emulator runs ONE Doom at full speed, with
  headroom to spare.
- **The 3-instance ring: ~16 frames/s per instance** (probe, twice) = ~48 frames/s of
  machine throughput = ~1.35 Dooms. The ring divides that three ways.
- So the emulated CPU is what is scarce, and DOSBox is already at its documented
  ceiling (see the overclock section). The host core is saturated.
- **Cutting the emulated CPU stops the machine dead.** With `cpu_cycles=8000` fixed, or
  with `core=normal`, the guest reaches "host up: depths 3" and then never logs a single
  frame in the window (56 lines of engine log, 0 frame lines; the baseline logs 856).
  Caveat, stated because it is true: that symptom is also what the occasional stall
  looks like, so this pair of runs is corroboration and not proof on its own - the
  primary evidence is plain-Doom-at-full-speed versus the ring at a third.

What this means for a faster emulator: DOSBox's dynamic core is an x86-to-x86 JIT, and
on this host it delivers about 1.35 Dooms. **Hardware virtualization is not available
here** (`/dev/kvm` missing, zero `vmx`/`svm` in /proc/cpuinfo), so QEMU would fall back
to TCG, which for DOS games is same-class or worse than DOSBox - it is not a lever
without VT-x. With VT-x, the guest CPU runs natively and the whole CPU shortage
disappears; the ceiling then becomes Doom's own 35 frames/s per instance, which is the
30+ the project wants.

Porting the exploit to a real DOS (FreeDOS in QEMU) is a research task, not a config
change, and the risks are specific:
- The guest writes to linear addresses far outside its RAM - db.log logs
  `Illegal write to 12345682` four times per run, at boot, in EVERY recording. DOSBox
  logs and ignores these; a real CPU with paging raises #PF and DOS/4GW's handler would
  most likely terminate the program. This is the first thing the port must survive.
  (Cheap mitigation if it bites: `asm.S`'s `mov $0x12345678,%esi` placeholder in
  `I_FinishUpdate` is patched by `init_draw()` anyway, so it can be initialised to a
  valid address - e.g. `0xA0000`, a harmless self-copy - instead of an unmapped one.)
- The DPMI host is DOS/4GW in both environments (DOOM2.EXE carries its own extender), so
  ACE's INT 31h calls should behave the same.
- VESA: DOSBox emulates an S3 Trio64; QEMU's std VGA offers Bochs VBE. The engine's mode
  request may or may not find its exact mode.
- Capture: DOSBox's frame-accurate Ctrl+F7 recorder has no QEMU equivalent; QEMU would
  need a screendump loop over QMP or a VNC grab, and that path is not frame-accurate for
  free.

**RETRACTED, and by a measurement of Doom's own.** `verify/timedemo.sh` runs
`doom2 -timedemo demo1`, which bypasses the tic clock and runs the tics as fast as the machine
can - so it measures the emulated machine's Doom-rendering throughput with nothing of ours in
it. Result: **"timed 1205 gametics in 205 realtics" = 1205*35/205 = ~206 frames/s**. (A first
run polluted by a leftover DOSBox gave 124; the clean run is the number.)

One Doom at full speed is 35 frames/s; three instances at 35 each need ~105. The machine
delivers ~206. **Three emulated Dooms are not a problem for this host**, the ring's 16 frames/s
per instance is not a CPU shortage, and there is no case for VT-x or QEMU. The earlier
"1.35 Dooms" figure came from assuming the ring's rate was CPU-bound, which -timedemo refutes.

**ALSO FALSIFIED: the cycle calibration in the overclock section.** Asking for a FIXED
1,000,000 cycles/ms - a count no host can deliver, ~2.75 GHz of 486-equivalent work through a
JIT on a 4 GHz core - still returned an emulated/wall ratio of 1.000. DOSBox paces the guest's
clock to realtime whatever the cycles setting, so that ratio measures nothing at all, and the
"emulated 0.96x wall" quoted earlier as evidence of saturation was meaningless too.

## THE TIC RATE IS THE LEVER, AND DOOM'S TIMER CODE IS IN THE BINARY

Doom renders at most once per tic: `TryRunTics` blocks until a tic is available. So the tic
rate IS the motion granularity - 35 tics/s means at most 35 distinct pictures/s, and no amount
of CPU changes that.

In the ring each instance holds the machine about a third of the time, so it consumes about a
third of the tics: ~12-16 distinct pictures/s (measured: 13.90/s for the transported instance in
the walk clip, against plain Doom's 35.59/s). Each rendered frame therefore advances 2-3 tics of
that instance's world, unevenly - which is the judder, as distinct from a merely low rate.

That makes a tic-rate patch the right lever, in either direction:
- **UP (35 -> ~105 for three instances)**: each instance gets its full 35 tics/s of world time,
  so ~35 distinct frames/s at NORMAL game speed. The machine can afford it (~206/s of capacity
  against the ~105 needed).
- **DOWN (35 -> ~14)**: each rendered frame is exactly one tic, so the motion is uniform -
  smooth slow motion instead of choppy near-real-time.

**Feasibility.** The DOS timing code was never open-sourced (the release is linuxdoom-1.10,
whose `I_GetTime` is a `gettimeofday` stub - the README says the DOS code could not be released
because of a copyrighted sound library), so it has to be read out of the binary. It is there:
16-bit real-mode code at file offset ~0x25c0-0x2620 programs PIT channel 0 with divisor
**2130 = 0x0852 = 1193182/560** - a 560 Hz timer, which is 16 x 35, so the 35 Hz tics are that
divided by 16 in software. The matching 18.2 Hz restore path sits at ~0x260d. ACE is the right
place for the patch: it already writes raw bytes into Doom's code (`main.c:260` patches a byte
at 0x0003FE40) and owns a 444-entry hook table.
**NOT YET VERIFIED**: which immediate is the divisor rather than a saved tick value, and where
the /16 happens. Do not claim a working patch until that is measured.

**Caveat that must be measured before promising anything.** The ring produces only ~42-48
distinct pictures/s in total against the machine's ~206/s, so most of the machine is going
somewhere in our own engine - prime suspect the context switch (once per machine frame at a
4-frame slice) plus the per-frame screen paint (~46,000 pixels per round, from the diegetic
log). If that overhead is what binds, raising the tic rate alone will NOT raise the picture
rate. The cheap discriminator: vary `MTX_DEMO_INTERVAL` (4 -> 16) and watch the picture rate.

## (a) WHAT THE CONTEXT SWITCH COSTS - ANSWERED, AND IT IS NOT THE COST

`MTX_DEMO_INTERVAL` 4 -> 16 (a quarter as many switches) and both numbers again, same build
otherwise, same probe:

| slice | frame clock per instance | distinct pictures/s (transported, walking) |
|-------|--------------------------|--------------------------------------------|
| 4 (shipped) | 16.0/s | 13.90/s (`rec-walk`) |
| 16 (probe)  | 16.0/s | 7.04/s (`rec-slice16clip`) |

**The throughput does not move at all** - 16.0 frames/s per instance at both, i.e. 48/s total.
Quadrupling the slice length changes nothing, so the context switch is NOT what costs the
frames, and the earlier estimate that a switch costs ~6-13 frames' worth of work is wrong
(it was derived from assuming the ring was CPU-saturated, which -timedemo refuted).

**What DOES change is how many of those frames are new.** At a 4-frame slice 13.90 of the 16
frames/s carry a new picture; at 16 only 7.04 do. A longer slice makes an instance render a
burst of 16 frames while its world has only advanced a few tics, so most of the burst is
duplicate pictures. So the slice length is a trade in the *other* direction than expected:
**shorter slices give more distinct pictures per second**, longer ones give longer runs of the
same picture. (This also revises the note on MTX_DEMO_INTERVAL: 4 is not "for the motion" in
the sense of more new pictures per render - it is already the better of the two measured.)

**What is still unexplained, and is now the only thing left.** The render rate is pinned at
48/s total while the machine renders plain Doom at ~206/s (timedemo). The switch is out; the
tic clock cannot be the cap on the *render* rate either, because 48 > 35. So the remaining
suspects are per-frame work in our own engine (the screen paint is ~46,000 pixels per round,
from the diegetic log) or a *shared* cost that does not care how often we switch - a prime
candidate being the dynamic core's translation cache: three copies of Doom's code at three
addresses is three times the code to translate, and a cache too small for all three would
thrash on every switch while being completely insensitive to the slice length, which is
exactly the signature measured here.

Two cheap experiments localise it, both a rebuild:
1. `mtx_screen_paint()` disabled (one early return): does the rate move? That prices the paint.
2. `MTX_RING_CHILDREN` 2 -> 1 (two instances): if the *total* stays ~48/s with two instances
   (i.e. 24/s each), the cap is shared and the translation cache is the leading explanation;
   if the total falls to ~32/s, the cap is per-instance work.

## (A)+(B) WHERE THE RING'S FRAMES GO: ACE'S OWN WORKLOAD, NOT OUR PROBES

**(A) The emulation thread is SATURATED.** `top -H` during a ring run: the main dosbox thread
sits at **99.9%** (llvmpipe ~10% x2 and PulseAudio ~10% on top). The guest's clock is paced to
realtime, so a pegged thread while the clock keeps pace means the guest is doing as much work as
the host can deliver: **the ring is CPU-bound.**

**(B) The screen paint is exonerated.** `mtx_screen_paint()` early-returned, same probe, same
session: 8-12 frames/s - the SAME as the shipped build measured in the same session (8-12/s).
Removing the paint changes nothing, so it is not the bottleneck. The seg-note was already known
to be *cheaper* than the column draw it replaces (a screen wall skips `colfunc()` entirely -
render.c:817). Our own instrumentation is not what costs the frames.

**A NOISE FLOOR THAT MATTERS, measured the hard way.** The shipped build measured a steady
**16.0 frames/s** per instance earlier in this session and **8-12/s** now - same build, same
probe. The host's delivered throughput varies by ~40% with what else is running (the browser,
gnome-shell, and DOSBox's own software-GL presentation path, which is ~20% of a core on top of
the saturated emulation thread). Every absolute rate in this file therefore carries that
uncertainty, and fine comparisons (paint on/off, surface vs texture) must be made *within one
session*, never across sessions. The within-session comparisons in this file are sound; the
cross-session ones are not.

**What this does to the CPU conclusion - the retraction above was itself wrong.** The ring's
workload is ~4-6x heavier per frame than vanilla Doom's: the machine renders **206** vanilla
Doom frames/s (timedemo) but only **30-48** ring frames/s, with the same machine, measured the
same way. Comparing the two as if a ring frame were a vanilla frame was the error. With three
instances of ACE (its own renderer and 444 hooks, three copies of the game's state) the host
really is the constraint, and more host CPU really would buy ring frames.

**So both ceilings are real and both must be lifted for 30+ new pictures/s per instance:**
- **the tic rate**: 35 tics/s shared three ways caps each instance at ~11.7 NEW pictures/s
  whatever the CPU does (the PIT divisor patch);
- **the CPU**: at ~30-48 ring frames/s total, each instance gets 10-16 render-calls/s, so
  without more CPU there is nothing to make distinct in the first place (VT-x / QEMU, which
  becomes the right move after all - and this time for the ring's own workload, not vanilla).

A free lever first: the measurements above were taken with a browser and gnome-shell competing.
Re-baselining on a quiet host may recover a good part of 16 -> 20+ frames/s before anything else
is changed, and it costs nothing but a closed browser.

## WHAT THE MACHINE ACTUALLY COSTS: 500M GUEST CYCLES/S, AND THE RING SPENDS ALL OF IT

Question asked: a 4-6x workload should be nil for a 4 GHz CPU, so why does the frame
rate collapse? Answer, with the measurement that settles it.

### The sweep that measures the emulator's ceiling (verify/cycles_sweep.sh)

`cpu_cycles` fixed (and `cpu_cycles_protected` with it - Doom is DOS/4GW protected
mode, and with `cpu_cycles=max` the protected setting is FORCED to 'auto', so a cap
has to be stated twice). A fixed value does not cap a workload, it PACES THE GUEST'S
CLOCK: the guest is told it has that many cycles per emulated millisecond, and if the
host cannot deliver them, the guest's whole world runs in slow motion. That is what
makes the sweep a measurement of the host rather than of the guest.

| cpu_cycles | ring rate (wall) | note |
|---|---|---|
| max (baseline) | 16.00 frames/s | = 48/s total, 16 per instance |
| 2,000,000 | 4.00 frames/s | guest at exactly 1/4 realtime |
| 500,000 | 12.00 frames/s | |
| 125,000 | (no steady-state window) | boot+load alone outran the 30 s probe |
| 30,000 | (never left the load) | log shows depth-1 still at its init stages |
| max (again) | 16.00 frames/s | closes the drift bracket - no drift this time |

The 2M point solves for the host: the guest's frame rate IN EMULATED TIME is 16/s
(established: at max the guest runs at realtime, ratio 1.000), so a wall rate of 4/s
means emulated time advanced at 4/16 = 1/4 of realtime, i.e. the host delivered
2,000,000/4 = **500,000 cycles per wall millisecond**. So:

  host throughput  ~500,000 cycles/ms = 500M guest cycles/s
  vanilla Doom     206 frames/s  ->  500M/206 = 2.43M cycles per vanilla frame
  ring             48 frames/s   ->  500M/48  = 10.4M cycles per ring frame
  ring frame cost  10.4M / 2.43M = 4.3x a vanilla frame  (measured ratio 206/48)

Cross-check on the 2.43M figure: a 486DX2-66 was the machine that ran Doom at ~30 fps,
66M/30 = 2.2M cycles per frame. Independent route, same answer, so the 2.43M is not an
artifact of my model of DOSBox's pacing.

### THE DECISIVE POINT: THE FRAME RATE DOES NOT RESPOND TO THE MACHINE'S SPEED

Two independent runs at `cpu_cycles=250000` (HALF the host's ~500k - the cap confirmed
by DOSBox's own log line "Setting fixed 250000 cycles", and a fixed value paces the
guest's CLOCK, which the 2M point proves):

  cpu_cycles=max      16.00 frames/s per instance   (48/s total)
  cpu_cycles=250000   16.00 frames/s per instance   (run 1: 14 windows, median 16.00)
  cpu_cycles=250000   16.00 frames/s per instance   (run 2: 14 windows, median 16.00)

Halving the emulated machine's speed changes the frame rate by ZERO. So the frame rate
is NOT set by the machine's instruction throughput: it is set in GUEST TIME, by a wait
whose period the emulated clock defines. The ring has at least 2x headroom.

That also kills the 4.3x figure I derived a few paragraphs above, and it kills the
"CPU-bound" conclusion:

  ring per-frame cost <= 250M cycles/s / 48 frames/s = 5.2M cycles
  vanilla Doom frame  = 2.43M cycles
  -> the ring's frames cost AT MOST ~2.2x a vanilla frame, not 4.3x
  -> the ring spends <= 250M of the machine's ~500M cycles/s: at most HALF the machine

### Why the "99.9% host core" reading was wrong (this is the lesson)

A pegged host core was read as saturation. It is not evidence of useful work, for two
reasons, both measured:
  1. Doom's main loop BUSY-WAITS on the timer - it has no sleep (a 486 was pegged too).
  2. DOSBox's fixed-cycles pacing ALSO spins rather than sleeping: at cpu_cycles=250000
     the host core still measured 102%, even though the guest was getting half the
     cycles and producing exactly the same frames.
So "the core is at 100%" only ever meant "the guest is executing or spinning", which is
true in both the CPU-bound and the clock-bound case. The flat line under a 2x cap is
what actually discriminates, and it says clock-bound.

### The gate is not simply the tic clock - identify it before patching

48 root frames/s total is ABOVE the 35 tics/s the shared PIT produces, so the ring is
not merely rendering one frame per tic. The remaining candidate is Doom's own per-frame
path plus the ring's handover (mtx_engine_policy / MTX_DEMO_INTERVAL / the host service).
Read that path and find the wait before changing the PIT divisor, otherwise the patch
may move a clock the frame rate does not actually follow.

### Prediction to test with the PIT patch (the next build)

If the pace is derived from the emulated PIT: 35 Hz -> ~105 Hz scales the frame rate ~3x
-> ~144 frames/s total attempted, and the machine's work ceiling (500M / 5.2M = ~96
frames/s) would then cap it at ~32/s per instance - right at the 30 fps goal, with the
CPU becoming the NEXT limit rather than the current one. If instead the frame rate does
not move with the tic rate, the gate is in Doom's own frame path and the PIT is the wrong
lever. Either way the experiment is decisive, and it is cheap: one constant, one rebuild.

### Artifacts and limits of the sweep (do not quote it without these)

- At 125k and 30k there is NO steady-state window: the boot+level load is itself
  CPU-work-bound and outran the 30 s probe. Those two points say nothing about the
  steady state - they are startup-limited, not frame-limited.
- A fixed cycles value is not the same code path as `max` (max runs unthrottled), so
  do not read the fixed-value points as a clean throttle curve; only the 2M point is
  used above, and it is used for its CLOCK ratio, not as a work measurement.
- Still NOT measured: the split of the 4.3x between ACE's renderer, the hook table and
  three-copy cache pressure. Only the total is measured.

### The spin question is still open, and the tic patch is the discriminator

The 99.9% host core does NOT prove useful saturation: Doom's main loop busy-waits on
the timer (it has no sleep; a 486 was pegged too), so a pegged core is equally
consistent with a guest spinning on a clock it cannot advance. A cycles cap cannot
separate these - it slows work AND clock together. The clean test is to RAISE the tic
clock (the PIT divisor patch, 2130 -> ~710 for 105 Hz): if the ring's frame rate rises
with it, the clock was binding; if it does not move, the CPU is. That experiment is
also the fix for the judder, so it is the next thing to build either way.

## TESTING THE TIC CLOCK: A NEGATIVE RESULT, AND A BUILD TRAP THAT FAKED ONE

### The divisor patch (verify/tic_patch_test.sh) - no effect

`verify/after-fast/DOOM2.EXE` is the shipped image with exactly two bytes changed at file
offset 0x25fd: the timer divisor 2130 (0x0852) -> 710 (0x02c6). If that constant is the
live tick divisor, 1193182/710 = 1680 Hz and (with the /16 in software) 105 tics/s - 3x.

  stock1   16.00 frames/s per instance
  patched  16.00 frames/s per instance
  stock2   16.00 frames/s per instance

Bracketed in one session, so drift is not the explanation: the divisor change moved
nothing. BUT - and this is the part that matters - **the patch's own effect on the guest
clock was never confirmed**, so this is not a falsification of the tic hypothesis:
disassembling the block it lives in shows it READS the PIT counter (`in al,0x40` at file
0x25d0 and 0x25d5), stores a divisor in a variable, and writes 0x10 to the PIT data port -
which is a mode-0 one-shot, not a 560 Hz mode-2 programming (that is `out 0x43,0x34`, at
0x260b, on the restore path). So 0x25fd may be a delay constant, not the tick divisor.
An unverified lever proves nothing when it does not move.

### The build trap that made a real change look like no change

The first gametic run reported "NO self-check lines with gametic" - the new log line was
simply absent from the run. Cause: the script ran `make -C engine` only. The engine make
produces `code.lmp`, but the level WADs are packed from **loader/loader.wad**, and
loader.wad is what carries the engine into the game. Measured: loader.wad mtime 18:15
while code.lmp was 19:04, and the run's log had the OLD line shape.

  RULE: rebuilding the engine is `make -C engine` AND `make -C loader`, then regenerate
  the level WADs. Only the engine -> the game silently runs the PREVIOUS engine. A log
  line that does not appear, or a change that "has no effect", is this trap first.

### Dead ends, recorded so they are not retried

- `MTX_LEVELS=0` does NOT isolate one instance: the depths get their maps from the loaded
  WADs, so the ring still came up at "depths 3" (both runs 16.00/s, identical logs).
- `memsize` is NOT a way to dial the instance count either: at 16 and 24 MiB the engine
  produces ZERO frames (no "host up" line at all) - it needs the full 63 MiB to come up.
- A fixed `cpu_cycles` does not throttle a workload, it PACES THE GUEST CLOCK, and the
  fixed-value path spins rather than sleeping (host core still 102% at 250000).

### New instruments (all in verify/, scratch)

`cycles_sweep.sh` (CPU cap sweep), `solo_vs_ring.sh`, `depth_vs_rate.sh`,
`tic_patch_test.sh`, `gametic_probe.sh` (tics-per-frame from the engine's own log),
`record.sh` gained `MTX_REF` (run a variant game image) and `MTX_LEVELS` (which level WADs
are staged).

### The measurement that settles it: tics per frame

The frame counter alone cannot distinguish "a frame that repeated a tic" from "a frame
that advanced one". The self-check line now carries Doom's own `gametic` and `leveltime`
beside `frame`, so the log yields tics-per-frame, and multiplied by the measured frame
rate it yields the TIC RATE in tics/s:

  tics/frame ~ 1  -> each frame is a tic: the clock is the gate, raising it raises the
                     frame rate
  tics/frame > 1  -> the engine renders slower than the clock allows: the clock is not the
                     gate and a tic patch cannot help
  tics/frame < 1  -> frames are FASTER than the clock: duplicate frames, and the thing to
                     fix is the tic rate for smoothness (distinct pictures/s), not the
                     frame rate

The same ratio also verifies the divisor patch: if it moved the guest clock, the patched
run advances ~3x more gametics per frame.

## THE TICK RATE TEST, AND THE STRUCTURAL REASON IT COULD NOT HAVE WORKED

### What was measured (all in one session, stock / patched / stock)

The engine's frame log now carries I_GetTime(), gametic and leveltime. The disassembly
shows what I_GetTime is (file 0x5bc20 = the trampoline target for I_GetTime, load 0x19C10):

    lea eax,[eax+0] / lea edx,[edx+0] / nop        ; padding
    a1 3c 91 02 00   mov eax,[0x2913c]             ; the game's tick counter
    c3               ret

and that counter is incremented by the IRQ0 handler installed on DPMI vector 8 (load
0x1a8b0 = file 0x5c8c0), which also EOIs the PIC. So I_GetTime IS the machine's tick
counter, and the measurement is direct:

  stock1   16.00 frames/s   frame +199, tick +198  => 0.995 ticks/frame  => 15.92 ticks/s
  tic3x    16.00 frames/s   frame +199, tick +198  => 0.995 ticks/frame  => 15.92 ticks/s
  stock2   16.00 frames/s   frame +199, tick +198  => 0.995 ticks/frame  => 15.92 ticks/s

and per-frame, from the every-frame run (verify/tick_delta.sh):

  ticks advanced per single frame: {0: 1, 1: 215}  => exactly 1 tick on 215/216 frames

CONCLUSIONS, in order of confidence:
1. The engine renders ONE frame per tick. The frame rate IS the tick rate, ~16/s per
   instance. So the game's world advances 16 tics/s where Doom's own rate is 35: each
   instance runs at ~46% of Doom's designed speed, and the frames it draws are the only
   ones it draws.
2. The tick counter advances in lockstep with the frames, with essentially no jitter
   (215 ones, 1 zero in 216 frames). An independent clock that the engine waits on would
   jitter (0s and 2s); this does not. So the pace is not an external clock the engine is
   polling - it is the engine's own loop rate.
3. The divisor patch moved NOTHING: 34000 (35.09 Hz) -> 11333 (105.28 Hz), 2 bytes, and
   the tick rate stayed 15.92/s. Not because the patch failed to run (verified: the run's
   rec-*/DOOM2.EXE symlink pointed at the patched image), but because of the next point.

### THE STRUCTURAL REASON: the running engine is not in DOOM2.EXE

My format string "gametic %u leveltime %u" appears in

  doom_ace/engine/code.lmp        1
  doom_ace/loader/loader.wad      1
  verify/levels/level0.wad        1
  verify/levels/level1.wad        1
  verify/after/DOOM2.EXE          0     <-- the image that gets patched

So the RUNNING engine is the ACE image carried by the level WADs, and DOOM2.EXE is the
host game around it. Patching the host's timer code cannot reach a clock the ACE image
owns - both timer patches (0x25fd and 0x5c95f) were inert by construction, and the second
one was aimed at the live PIT install (mov eax,0x84d0 / call 0x5c8f4).

  RULE: before patching an image, prove the running engine is IN it. `strings` for a
  string you just added to the engine is the cheap proof. It takes one command and it
  would have saved this entire detour.

### The lever, restated honestly

The pace is inside the engine's own frame path - which is OUR code (doom_ace/engine and
nested/), not the emulator and not the host game's timer. What has been ELIMINATED, each
with a measurement: the emulated CPU's speed (cpu_cycles 250000 vs max: identical 16.00),
the host game's PIT divisor (two patches), the slice length (MTX_DEMO_INTERVAL 4 vs 16),
the recorder (capture on/off), and host load (bracketed stock runs, all 16.00).

The next step is therefore to READ the frame path - vesa.c's swap_buffers (which calls
mtx_engine_frame), mtx_engine_policy, and the ring's host service in nested/mtx_host.c -
looking for what makes it 16/s rather than as fast as the machine allows. The engine's
loop is where the 16 comes from, and it is ours to change.

### Instruments added this round (verify/, scratch)

`tic_rate_test.sh` (stock/patched/stock with the tick rate), `clock_probe.sh`,
`rate_probe.sh`, `tick_delta.sh`, `gametic_probe.sh`, `cycles_sweep.sh`, `solo_vs_ring.sh`,
`depth_vs_rate.sh`, plus `MTX_REF` and `MTX_LEVELS` in record.sh. The engine's frame log
line carries t= (I_GetTime), gametic and leveltime; the depth self-check line carries
gametic and leveltime.

## THE CLOCK, PINNED: THE GAME'S OWN COUNTER, ONE TICK PER FRAME

### The measurement (verify/rawclock.sh)

The frame log line now carries the game's tick counter read STRAIGHT OUT OF THE GAME'S
MEMORY, bypassing I_GetTime entirely, because the engine's trampolines are rewritten at
load time (asm.S: the I_FinishUpdate placeholder "will be overwritten by init_draw()") and
so I_GetTime might not be the game's function by then:

  frame 1 -> 200  (+199 frames)
  I_GetTime()    41 -> 239   (+198)   = 0.995 per frame
  raw 0x2913c    41 -> 239   (+198)   = 0.995 per frame      raw/t ratio: 1.000
  raw deltas per single frame: {0: 1, 1: 38}

So:
1. I_GetTime() IS the game's own tick counter - not an engine substitute. The two agree
   exactly, frame for frame.
2. The engine renders ONE frame per tick, with essentially no jitter (38 of 39 consecutive
   frames advanced exactly one tick). The frame rate IS the tick rate, ~16/s per instance,
   against Doom's own 35/s.
3. The machine's clock - the game's counter, incremented by the host game's IRQ0 handler -
   advances at ~16 Hz, NOT the 35.09 Hz its own timer install asked the PIT for
   (mov eax,0x84d0 / call 0x5c8f4), and not the 105 Hz of the patched image either.

### The mistake that nearly hid it

The first attempt read flat 0x2913c and got a constant 3452816845 = 0xCDCDCDCD - fill
memory. That is the WRONG ADDRESS, not a frozen counter, and it would have been very easy
to report as "the counter does not advance". The game's image is RELOCATED at load, so its
link-time addresses are not its runtime addresses: the counter must be read as
`doom_data_segment + 0x2913c`, exactly as the engine already reads the game's screen
variable (MTX_DOOM_SCREEN_VAR = 0x74FC4, `doom_data_segment + offset`). One constant,
0xCDCDCDCD, is the tell.

  RULE: a memory read that returns 0xCDCDCDCD (or any repeating fill) is a bad address,
  not a bad measurement. Check the value before believing the conclusion.

### Where this leaves the lever

The clock that matters is the game's counter, and the engine's frame rate is welded to it
one-to-one. Everything that does NOT move it is now measured: the emulated CPU's speed
(cpu_cycles 250000 vs max), the host image's PIT divisor (two patches: 2130->710 and
34000->11333), the slice length (MTX_DEMO_INTERVAL 4 vs 16), the recorder, host load.

The next step is to pin WHICH mechanism advances that counter, and it is cheap: log the
counter at the START and the END of the same frame. If it moves WITHIN a frame, an
interrupt fired - IRQ0 is live and running at ~16 Hz despite the 35 Hz programming, and
the lever is the interrupt source. If it only ever moves BETWEEN frames, the engine's own
frame path advances it, and the lever is the engine's loop - our code, in
doom_ace/engine and nested/.

## THE CLOCK, PINNED FOR REAL: THE FRAME WAITS FOR THE TICK, AND THE TICK IS SLOW

### The measurement (verify/intra.sh)

The frame policy samples the game's tick counter at the START of the frame's work and
again at the log point, and reports the difference:

  intra-frame tick deltas over 44 samples: {0: 44}
  frames where the counter moved DURING the frame's work: 0/44 (0.0%)

Combined with the earlier result (the counter advances exactly one per frame, 38 of 39
consecutive frames), the mechanism is now:

  the frame's work never overlaps a tick -> the loop WAITS for the tick, then computes
  exactly one tick per frame             -> the frame rate IS the tick rate
  the frame's work fits inside the period -> the machine has SPARE CAPACITY

That last line is the one that matters for the goal: the ring is not short of CPU during
a frame - it spends most of the tick period waiting. It independently confirms the
cpu_cycles result (250000 vs max: identical 16.00 frames/s) by a completely different
route, and it means raising the tick rate is affordable.

### What the clock is, exactly

  - I_GetTime() and a DIRECT memory read of the game's counter agree exactly, frame for
    frame (41 -> 239 for both over 199 frames, ratio 1.000). So I_GetTime really is the
    game's own counter, not a clock the engine substituted at load time.
  - The counter advances at ~16/s. The game's own timer install asked the PIT for 35.09 Hz
    (mov eax,0x84d0 / call 0x5c8f4). So the effective clock is ~2.2x slower than the
    game's own design, and Doom's intended 35 tics/s is not what the ring gets.

### The near-miss worth remembering

The first direct read of the counter returned a constant 3452816845 = 0xCDCDCDCD - fill
memory. That is a WRONG ADDRESS, not a frozen counter, and it was one line away from being
reported as "the counter never advances". The game's image is RELOCATED at load, so its
link-time addresses are not its runtime addresses; the counter has to be read as
`doom_data_segment + 0x2913c`, the same way the engine already reads the game's screen
variable (MTX_DOOM_SCREEN_VAR = 0x74FC4).

  RULE: a memory read that returns a repeating fill (0xCDCDCDCD, 0x00, 0xFF) is a bad
  address, not a measurement. Look at the VALUE before believing the conclusion.

### Next, precisely

1. Measure how much of the ~62.5 ms tick period the frame's work actually occupies. That
   sets the ceiling on how far the tick rate can be raised before the ring becomes
   CPU-bound again, and it is one instrumentation away (the counter is already sampled at
   frame start; sampling it at frame end gives the work's share of the period).
2. Then raise the tick rate - and re-measure the FRAME rate, not just the tick rate, since
   the two are welded together one-to-one.
3. The instruments to do both are already in the engine's frame line (t=, raw=, gametic,
   lt=, intra d=). NOTE: that changes the diegetic log, so the level trials' golden-log
   comparisons must be re-run before this is committed.

## THE VGA RETRACE WAIT, EXONERATED - AND THE PACE IS MEASURED IN EMULATED TIME

### I_WaitVBL found and neutralised (verify/vbl_test.sh)

The trampoline target for I_WaitVBL is file 0x5bc74, and it is the classic retrace spin:

  5bc74: push ebx/ecx/edx
  5bc77: mov  ebx,eax          ; n = how many retraces
  5bc79: cmpl $0,0x29100       ; a software flag
  5bc80: jne  0x5bca2          ; if set, return WITHOUT waiting
  5bc82: mov  ecx,0x3da        ; VGA status port
  5bc8e: in al,dx / test al,0x8 / je ...   ; spin until the retrace starts
  5bc97: in al,dx / test al,0x8 / je ...   ; spin until it ends
  ...

The flag at [0x29100] disables the wait, so I forced the conditional jump at file 0x5bc80
to be unconditional (75 -> eb, one byte) and ran stock / patched / stock:

  stock1  16.00 frames/s, 0.995 ticks/frame
  nowait  16.00 frames/s, 1.000 ticks/frame
  stock2  16.00 frames/s, 0.995 ticks/frame

No effect. The retrace wait is NOT on the frame path (ACE had already NOP'd calls inside
I_FinishUpdate - draw_c.c hooks 0x1D31D and 0x1D4F2 with HOOK_SET_NOPS). Exonerated.

### Also ruled out this round

- ACE has no PIT counter read: its one `e4 40` is `f6 45 e4 40` = testb $0x40,-0x1c(%ebp),
  a false positive on the byte search.
- The game's "bump the tick counter" function (file 0x5c372: read [0x2913c], inc, store,
  ret) is referenced by NOTHING in either image - no immediate 0x1a362 anywhere in the
  host EXE or in ACE's code. It is dead or reached indirectly.
- The frame path's hook is draw_c.c's hook_finish_update, installed at 0x1D4A6 / 0x1D4FC
  inside the host game's I_FinishUpdate.

### The finding that constrains the search: the pace is EMULATED time

At cpu_cycles=2000000 the tick rate was 4.00/s - exactly one quarter of 16.00/s. So the
pace scales with the guest's emulated clock, not with wall time: whatever waits, waits on
something measured in emulated time. And the PIT cannot be its source, because the PIT's
slowest rate is 18.2 Hz (divisor 65536) and the observed pace is 16 Hz, while two patches
of the divisor changed nothing at all.

### Next, precisely

Read an EMULATED clock from inside the guest at frame boundaries - the PIT counter at port
0x40 (1.193182 MHz) is the natural one, latched with `out 0x43,0` and read twice. The
frame period in PIT units then names the pace directly: a count of PIT units (which
constant?), a count of retraces, or a count of BIOS ticks. Until that number is in hand,
every further candidate is a guess; with it, the constant to change is obvious.

## Next, in order

0. THE TIC RATE, because it is what the motion granularity is: (a) measure the context
   switch's cost by varying MTX_DEMO_INTERVAL (4 -> 16) and watching the picture rate; (b) verify
   and implement the PIT divisor patch at file offset ~0x25f9/0x2608 (~2130 -> ~710 for 105 Hz);
   (c) then set the rate to what the ring can actually deliver per instance.
1. SOUND: isolate the Sound Blaster's IRQ from the context switch. Until that is done the
   teleport sound is silent in any run with the device on, and the emulated SB is what
   fails, not the call (see the sound section).
2. Composition/dynamics: the ring is still a *schedule*, and the views do not yet walk
   together as one gesture. `MTX_WALK=1` moves both because both read the same key array
   — worth a measurement of its own, not an assumption.
3. The probe pruning pass: the temporary probes are the evidence behind the numbers, so
   they go last, with the numbers carried over.

## Discipline that this project has repeatedly needed

- Rebuild before recording: `loader.wad` is the exploit image, and a stale one silently
  tests the previous build (`make -C doom_ace/engine build && make -C doom_ace/loader`).
- `verify/mkwadlevels.py` must be re-run after any engine rebuild: it copies the
  exploit lumps into the level WADs, so they go stale with the binary.
- Re-authoring the level WADs makes the "verified in DOSBox" claim stale — re-verify or
  say so.
- Kill stray DOSBox before a batch: `for p in $(pgrep dosbox); do kill -9 $p; done`.
  Two trials at once is not a thing: every trial script kills stray DOSBox first.
- The trial scripts are NOT executable in this tree — `chmod +x verify/*.sh` if a run
  exits 126 with "Permission denied", which a piped `sed`/`tail` will hide.
- A screen texture must EXIST in the IWAD's texture list: an unknown name loads, draws
  as "unknown texture" and silently paints nothing (now a build-time check).
- **A MID-RUN WAD ADD COSTS THREE THINGS, all measured** (see the block comment on
  `mtx_level_wad_add`): `lumpcache` must grow (guard it by pool provenance — Doom's data
  segment is shared through the snapshots, so `lumpcache` can hold ANOTHER depth's
  pointer); Doom's cache has BACK-POINTERS into the array (`Z_Malloc(..., &lumpcache[n])`
  stores the entry's address in the block, Z_Free uses it to clear the entry), so a moved
  array must be repointed or the engine dies later with **`Z_CT at w_wad.c:510`** in
  W_CacheLumpNum's cache-hit branch, with a healthy-looking table, no MTXW.LOG and no
  illegal access; and `map_info` is built at BOOT with a lump NUMBER per map, so the
  table must be re-resolved by name or the level silently loses its levelhack
  (`gamemap 1` instead of 2/3 — level number, title lump, sky, music, par time).
- **A SCREENSHOT/CLIP GRAB OF THE DOSBOX WINDOW CAN TEAR, AND THAT IS THE CAPTURE.**
  The emulated screen is written by a `rep movsl` into video memory and the window is
  presented from that memory at DOSBox's own frame boundary, so an X11 grab can read a
  half-updated window. Use DOSBox's own recorder (Ctrl+F7, emulated-framebuffer,
  frame-accurate) when the frames are the evidence. Measured before/after on the same
  build: a tear line with x11grab, none with the DOSBox capture.
- **A MEASUREMENT REPORT MUST RECORD ITS OWN CONDITIONS, IN THE FILE.** Measured the
  hard way: `fps-on.fps.txt` was quoted as the capture-on run while the capture was
  actually OFF, because the "capture: ON/OFF" line was printed to the terminal and the
  report kept only the rates - a report that cannot contradict a claim about how it was
  produced is worse than no report. `fps_probe.sh` now writes its capture state into the
  report, and refuses to present a capture-on run without the recorder's own AVI in it
  (a flag is an intent; the AVI is the fact).
- **DO NOT EDIT A SCRIPT WHILE IT IS RUNNING.** Bash re-reads a running script by byte
  offset, so a patch mid-run can make the tail execute garbage. It happened here: the
  probe was patched while a probe run was in flight, and the run had to be discarded and
  redone.
- **A NESTED RUN'S KEYS NEED THE WINDOW'S INPUT FOCUS, AND UNDER XVFB NOBODY GIVES IT.**
  Without a window manager SDL takes the focus when it likes; whole runs were measured
  with the ring running normally and ZERO key sightings. `xdotool windowfocus --sync
  <wid>` before typing, and key the readiness signal on the DEEPEST level being up (the
  root's level is up at frame 1).
- **A LOST KEYUP LOOKS EXACTLY LIKE A LATCH THAT IGNORES THE RELEASE.** In
  `verify/rec-levelsBkey` the authority word stayed 1 to the last frame, which reads as
  "the release was never observed". It was not a latch failure: the depths' own
  sightings (`key: depth N SEES the key down`) also continued to the last frame, i.e.
  the game's own array still said the key was down, so the keyup never arrived from X
  (`xdotool keyup`) into DOSBox. Tell the two apart by the depth sightings, not by the
  authority: the authority mirrors the game, and the game is what it mirrors.
- Temporary probes are still in the tree (stack high-water, word dumps, key sightings,
  `key_seen` in the params block). They are the evidence behind the numbers; prune only
  with the numbers carried over.
