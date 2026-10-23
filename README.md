# matryoshka — nested-instance host for the ACE engine

Goal: N live ACE/Doom instances coexisting inside one running `DOOM2.EXE`,
switched at runtime, without ever re-launching the executable. Depth 0 is the
root and owns the real hardware; depth d+1 runs inside depth d. Ceiling: 12.

The ACE chain (PNAMES/TEXTURE1 → ACE_LDR0 → ACE_LDR1 → ACE_CODE) is only the
bootstrap that gets ACE resident. Nesting is a context-management problem on top
of it, and that is all this directory is.

## The mechanism

    mtx_switch_ctx(&ctx[prev].esp, ctx[d].esp)      [mtx_switch.S]

Save ESP + callee-saved regs on the current stack, publish ESP, load the new
stack, restore, `ret`. Symmetric: a depth that switches away later resumes right
after its own `mtx_switch_ctx` call. No per-site bookkeeping, no libc, no
setjmp — nothing that assumes a C runtime, because ACE has none and `D_DoomLoop`
never returns.

A fresh depth's stack is pre-seeded by `mtx_seed()` so the final `ret` lands on
`mtx_trampoline` with the entry argument at `(%esp)`:

    low → high:  edi | esi | ebx | ebp | &mtx_trampoline | arg

Every switch runs `save_state(outgoing)` then `load_state(incoming)` around the
stack swap. That pair is the *only* place the DOS layer has to live.

Depth model: depth index == slot index; slots are reused on return; `mtx_push`
from depth `max_depth-1` returns `MTX_EDEPTH` rather than corrupting anything.

## What is per-depth (`dos_state.c`)

Three kinds of state, all registered by the wiring layer so the core stays
engine-agnostic:

1. **Shared global regions** — Doom's data segment and the engine's writable
   sections. Copied out on switch-away, copied in on switch-in.
2. **Patched locations** — every address ACE hooked, saved/restored as raw
   bytes, so depth N's hook set never bleeds into depth N+1.
3. **Pinned pointer globals** — `mainzone` and `screen_buffer` are ordinary Doom
   variables, but each depth needs its *own* zone and its *own* framebuffer. They
   come back from the snapshot and are then overwritten with the depth's values.

A depth's snapshot is seeded by cloning the parent's **live** state the first
time the depth is entered — a child starts as a copy of its parent, then
diverges. That is what a nested instance wants.

## The second stage (`ldr1n.c`)

Stock ACE_LDR1 is one-shot by construction: it frees the stdin buffer, lumpcache,
lumpinfo and mainzone, closes every WAD descriptor, then jumps into the engine.
Replay it and the parent's world is already gone.

LDR1N inverts that structurally. The service table is the loader's entire
authority, and it deliberately has **no `free()` and no `close()`**:

    typedef struct {
        int32_t  (*read)(void *ctx, int32_t fd, void *dst, uint32_t n);
        void    *(*alloc)(void *ctx, uint32_t n);
        void     (*enter)(void *ctx, void *code, uint32_t code_base,
                          uint32_t data_base, uint32_t ace_base);
    } mtx_ldr1n_ops_t;

The loader cannot destroy the parent's state because it has no way to. It reads
the `.bss` size, allocates from the *depth's own* memory, reads the rest of the
code, builds the descriptor list into per-depth scratch, and hands off. The
descriptor walk mirrors ACE_LDR1 exactly (fd field at lumpinfo+8, consecutive
dedup, previous seeded to -1 so leading `-1` entries drop too, `-1` terminator)
— and is unit-tested against those semantics.

## The yield point (`mtx_sched.c`)

A cooperative switch only happens if the running instance reaches the yield
point — and the running instance is Doom's frame loop, which never returns. So a
request is set from anywhere (input handler, debugger, timer) and serviced at the
next frame boundary, never mid-render or mid-think:

    mtx_request_yield();          /* or mtx_request_switch(depth) */
    ...
    mtx_frame_tick();             /* once per frame: consumes and switches */

The request is cleared *before* switching, so neither the incoming depth nor this
one on resume re-services it. A request naming a non-live depth is dropped by
`mtx_switch_to` rather than crashing.

**The hook.** In ACE the frame boundary is `swap_buffers()` in `engine/vesa.c`,
which *is* the engine's replacement for Doom's `I_FinishUpdate`, hooked at
`0x0001D4A6` and `0x0001D4FC`. It runs at least once per frame, after the frame is
rendered and before it is presented. (`P_Ticker`, `0x0002081A`, is the
once-per-tic alternative for a logic-only yield.)

**The call-convention trap.** `swap_buffers` is declared
`__attribute((regparm(2),no_caller_saved_registers))` — it promises not to clobber
eax/ecx/edx. A plain C function called from inside it would break that promise and
could corrupt Doom. `mtx_frame_tick` therefore carries the same attribute, so gcc
preserves those registers around the switch for us. (This also forces
`-mgeneral-regs-only`, since a `no_caller_saved_registers` function may not touch
x87 — which the engine already used.)

## ABI

    void mtx_init(uint8_t max_depth, const mtx_host_ops_t *ops,
                  void *stack_base, uint32_t stack_bytes);
    int  mtx_push(mtx_entry_t entry, void *arg);
    void mtx_yield(void);
    void mtx_switch_to(uint8_t depth);
    void mtx_finish(void);

    void mtx_dos_init(uint8_t max_depth, void *snapshot_mem, uint32_t per_depth);
    int  mtx_dos_add_region(void *base, uint32_t size);
    int  mtx_dos_add_hook(void *addr, uint32_t len);
    int  mtx_dos_set_pin(void *var_addr, int pin);
    void mtx_dos_set_value(uint8_t depth, int pin, void *value);

    int  mtx_dos_boot(void *arena_mem, uint32_t arena_bytes, uint32_t zone_per_depth,
                      mtx_wire_fn wire_register,
                      void **heaps, void **screens, mtx_boot_t *out);

`mtx_dos_boot` is the one-call entry: it registers once just to learn the
snapshot size, plans, carves the arena, then re-inits against the real pool and
hands each depth its zone and framebuffer.

`ldr0N.S` is the reentrant replacement for `ACE_LDR0` (base from `sub $0x34615,
%esi` is only true once, which is what makes the stock one un-reenterable). LDR0N
takes the bases as parameters: `%esi` = code base, `%edi` = data base, `%ebx` =
continuation, `4(%esp)` = banner.

## Files

    context.h        interface: context, host ops, depth limits
    mtx_switch.S     context-switch primitive + fresh-stack trampoline
    mtx_core.c       scheduler: seeding, switch, depth limits, save/load hand-off
    mtx_host.h/.c    the host's own control state, in ONE object (`mtx_hs`)
                     so no guest snapshot can ever cover it
    dos_state.h/.c   engine-agnostic per-depth state: regions, hooks, pins
    dos_wire.h/.c    concrete ACE wiring (regions, pins, hook registration),
                     including the `.bss` split that keeps `mtx_hs` out
    dos_alloc.h/.c   arena, budget planner, one-call boot
    mtx_sched.h/.c   yield point: request queue, serviced at the frame boundary
    ldr0n.h/ldr0N.S  reentrant first stage: bases as data, announces, hands off
    ldr1n.h/.c       reentrant second stage: reads a code image, allocates it
                     from the depth's own memory; service table has no free()
                     and no close(), so it cannot damage the parent
    ldr1N.S          its final hand-off (ESI/EDI/ECX, jmp *%ecx)
    mtx_zone.h/.c    a depth's OWN zone: build it in the depth's arena slot and
                     check it with Doom's own heap invariants; layout mirrored
                     from engine.h and size-asserted at build time
    guest.S          the payload a depth loads from the WAD and runs; built and
                     installed as the MTXGUEST lump by `make guest`
    test_mtx.c       host proof (32-bit freestanding, raw syscalls)
    Makefile         make run | make doscheck | make ldr0n | make guest

The engine side lives in `doom_ace/engine/`: `mtx_boot.c` (boot + demo policy +
`mtx_engine_zone_init`, the nested half of `Z_Init`), `mtx_ldr.c` (the three ldr1n
services wired to the engine, plus image validation), and `main.c`'s `Z_Init`, which
branches on depth.

## Verified (real execution, not description)

`make run` — 32-bit freestanding binary, executed:

    matryoshka host test
      depth ceiling: 12
      [1] nesting order .......... PASS
      [2] switch_to + per-depth state PASS
      [3] depth ceiling ......... PASS
      [4] save/load ordering .... PASS
      [5] distinct stacks ....... PASS
      [6] state snapshot + isolation PASS
      [7] arena + budget + boot .. PASS
      [8] reentrant 2nd stage .... PASS
      [9] yield point (frame tick) PASS
      [10] ops table lifetime .... PASS
      [11] reentrant 1st stage ... PASS
      [12] nested depth zone ...... PASS
    ALL PASS

[1] three depths nest and unwind in order (A C E D B).
[2] two depths ping-pong via `mtx_switch_to`; each keeps its own local counter.
[3] pushing past the ceiling is refused exactly once, at depth max_depth-1.
[4] `save_state`/`load_state` fire in the right order at each crossing.
[5] each depth runs on a genuinely distinct stack (ESP values differ).
[6] three-deep state isolation: child2 sees child1's globals and hook bytes but
    its *own* heap and framebuffer; after it returns child1's values are intact;
    after that returns the root's values are intact. No leak in either direction.
[7] planner arithmetic against hand figures; arena alignment, no overlap,
    overflow refused; an arena too small to nest is refused; the full boot
    carves distinct per-depth zones/screens and a non-overlapping stack and
    snapshot pool.
[8] descriptor-list dedup semantics (including leading `-1`), refusal instead of
    overflow on a too-small output, and the load path in exactly two reads and
    one allocation with `[bss_size][code…]` laid out correctly.
[9] the yield point. Depth 1 runs two frames, requests an upward yield, and the
    next `mtx_frame_tick` services it; the root then requests a switch back down
    and the child resumes mid-loop and finishes. Interleaving is frame-aligned
    (`1,1,0,1,1,0`) with each depth holding its own frame count, the request is
    consumed exactly once, and a request for a non-live depth is dropped rather
    than crashing.
[10] the save/load table must not dangle. `mtx_init` is called from a `noinline`
    helper whose frame is dead and then clobbered with `0x4D4D4D4D`; the test
    asserts the scheduler is still holding *our* callbacks, that the table is
    off-stack, and that a subsequent switch really calls them (save/load exactly
    twice each, child ran once). Fails against the old retain-the-pointer code.
[11] the reentrant first stage's hand-off contract. `code_base` points at a
    stand-in for Doom's printf (which ldr0N reaches at `code_base + 0x3FE40`); the
    stage records what it was handed and `ret`s. Checks the banner went through the
    given base and that ESI/EDI/EBX/ECX arrive as code_base / data_base /
    next_stage / arg. Catches both the EBP-clobber and the
    registers-loaded-before-the-call bugs.
[12] the nested zone builder and checker. Asserts the built zone's exact shape and
    that its one block spans precisely the rest of the region, then breaks the three
    heap invariants Doom's `Z_CheckHeap` enforces (back link, size touching the next
    block, two consecutive free blocks) and requires the checker to name each one.
    Verified to FAIL against a checker neutered to always report "intact" — a
    checker that cannot see corruption would pass every zone ever built.

### Wired into the engine, and verified not to regress

`engine/vesa.c` calls `mtx_frame_tick()` as the first statement of
`swap_buffers()`, and `engine/makefile` links the host (`MTX_DIR ?= ../../nested`,
overridable; `-DMTX_DOS_NO_STATIC_POOL` so the 7.5 MiB static pool is compiled
out).

Building the engine with that in place, then booting the same WAD in DOSBox:

    code size  286048 -> 286496   (+448 B)
    .bss size   53792 ->  54054   (+262 B)
    boot log   identical to baseline except the WAD byte count
    screenshot byte-identical to baseline

It is that small because nothing calls `mtx_init` yet: `--gc-sections` keeps only
`mtx_frame_tick` and the switch path it reaches (`mtx_yield`, `mtx_switch_to`,
`mtx_switch`, `mtx_switch_ctx`, `mtx_ctx`) and strips `mtx_init` entirely
(`nm` confirms). So the yield point is installed and inert until a host is booted
— which is what makes it a safe first integration.

### Booted live in DOOM2.EXE

The host boots for real inside DOOM2.EXE in DOSBox, and depth 1 exists as a running
instance. `engine/mtx_boot.c` boots it once on the first frame with a reserved
arena, creates depth 1 — which loads a code image out of the WAD and runs it (see
the loaders section) — and drives an auto-switch policy. Evidence goes to `MTX.LOG`
because stdout capture stops once the engine takes over:

    [MTX] first frame
    [MTX] host up: depths 2 snap 529468 img 353464
    [MTX] ace image: 353432 B at 3633988 entry 3633996 bss 22376
    [MTX] depth1 entered
    [MTX] depth1: guest alloc 94 B at 4313612, entering
    [MTX] depth1 zone 262144 B at 3987460 build 0 alloc 3987516 probe 0
    [MTX] frame 1 child=1
    ...
    [MTX] frame 30 child=1
    [MTX] request switch -> 1        <- frame boundary, depth 0 -> 1
    [MTX] guest OK: magic 1297373255 addr 4313612 loaded 4313612 zone 3987460
    [MTX] frame 250 child=1          <- root keeps running on its own frame loop

529,468 B/depth is the measured figure (475,676 Doom data + ~53 KiB engine .bss,
minus `mtx_hs` which is deliberately outside the snapshot). Depth 1 genuinely ran
on its own stack with its own snapshotted state, yielded back to the root, was
resumed, and completed — with zero illegal reads and zero `DYNX86` faults. The same
run with `MTX_DEMO_INTERVAL` lowered to 3 shows the full cycle back-to-back (see
`### Live switching` below).

Three things had to be right, each found the hard way:

1. **The arena must be reserved BEFORE `Z_Init`.** ACE's `Z_Init` walks
   `dpmi_get_ram()` down until `doom_malloc` succeeds — it consumes *everything*
   except a 32 KiB reserve. An arena requested at frame time can never be
   satisfied; the first attempts logged "no arena available" at every size down to
   1 MiB. `mtx_engine_reserve()` therefore runs at the top of `Z_Init` and the
   zone simply gets less memory — exactly the sizing trade the planner describes.

2. **The yield point must be a site that actually runs.** `swap_buffers()` is only
   hooked when VESA activates, and VESA is opt-in (`-vesa`) and did not activate
   in DOSBox's S3 for a 320x200 mode. `hook_draw_stbar` only runs in GS_LEVEL —
   the title screen is GS_DEMOSCREEN. The one site that runs in every gamestate is
   ACE's `I_FinishUpdate` in `asm.S`, but calling C from there **crashes the game**
   (`Illegal read ... CS:IP 170:b02c7d5c`, black screen): it is entered from a Doom
   path where the C calling convention does not hold, unlike ACE's other asm
   hooks. The working hook is `hook_draw_stbar`.

3. **A pending request waits, it is not lost.** Because the hook is
   gamestate-conditional, the request/consume pair is what makes it safe: a request
   sits until the next frame that reaches the yield point.

### Live switching, and the illegal-read fix

Booting stopped being the interesting part: the crash came on the *first real
switch*. Two bugs, and they are the same bug twice — host state that was not
actually under the host's control.

**1. The host's control state sat inside the snapshot that was running.**
Every switch restores the engine's whole writable `.bss` over the live copy, and
the scheduler's own variables (`mtx_ctx`, `mtx_cur`, `inited[]`, `pool`) lived in
that same `.bss`. So `load_state()` overwrote the state mid-switch: `mtx_ctx[prev].esp`
was put back to the value captured *before* `mtx_switch_ctx` published the real
one, and the next `mtx_switch_ctx` loaded `ESP = 0`. Measured as 2000 illegal
reads/writes plus `DYNX86: Can't run code in this page` on the very first switch,
where the HEAD baseline has zero.

Fix: all host control state now lives in a single object, `mtx_hs` (`mtx_host.h`),
and `dos_wire.c` splits the registered `.bss` region *around* it so no guest
snapshot can cover it. The split is exact because `mtx_hs` is one object, and it
refuses to nest at all if that ever stops holding. The boot log proves it: `mtx_hs`
ends precisely at `_bss_end`, and the snapshot dropped from 536,262 to
529,468 B/depth — exactly `sizeof(mtx_hs)`.

**2. The save/load table was a dangling stack pointer.** `mtx_init()` kept the
pointer it was handed, and the engine handed it the address of a *local* in
`mtx_engine_boot` — a frame long dead by the first switch. The switch then called
through recycled stack memory: `exception 06h (invalid opcode)` at a garbage EIP,
with `ESP` still on the *root's* stack. That last detail was the tell — the bad
call happens *before* the stack swap, so the fault could not be in the resumed
depth.

Fix: `mtx_init()` copies the table into `mtx_hs.host_ops` rather than retaining a
pointer, so owning the copy makes the mistake impossible instead of merely
documented. Regression-tested as host test [10]. Worth noting how the first
version of that test lied: it compared the retained pointer against the dead
frame's address, but returning a dead local's address is undefined, so gcc folded
it to NULL and the test passed *against the buggy code*. The working test inspects
the table the scheduler will actually call, and asserts it still holds our
callbacks.

**How the two were found**, in case this has to be done again: the frame-path log
stopped at `tick in`, which localised the fault to the switch itself rather than
the engine; the DOS/4GB register dump gave `ESP` in the root's stack, which ruled
out the resumed depth; probing `_bss_start`/`&mtx_hs`/`esp1` and the *contents* of
the suspended stack (callee-saved slots plus return address, checked against
logged code addresses) then ruled out both a broken stack and the state restore.
That left the call targets — and a function-pointer table pointing at a dead frame.

`make doscheck` — all nine DOS-target sources (`mtx_core.c`, `mtx_switch.S`,
`mtx_host.c`, `dos_state.c`, `dos_alloc.c`, `dos_wire.c`, `ldr1n.c`, `ldr1N.S`,
`mtx_sched.c`) compile clean with the exact flags from
`doom_ace/engine/makefile` (`-m32 -march=i386 -mgeneral-regs-only …`), with the
static pool compiled out as ACE needs.

`make ldr0n` — assembles to a 39-byte image, well inside the 256-byte ACE_LDR0
patch budget. (`make guest` builds and installs the 45-byte guest lump.)

## The loaders: a depth runs its own code image

`ldr0N.S` (first stage) and `ldr1n.c` (second stage) exist so a depth can fetch,
allocate for and run a code image of its own. They now do, end to end:

    depth 1 → mtx_ldr_load("MTXGUEST")   engine WAD layer → this depth's image slot
            → mtx_ldr_enter(entry)       → ldr0N: banner, bases, jmp
            → the guest runs, yields, resumes, finishes

Live evidence (`MTX.LOG`, real DOOM2.EXE in DOSBox):

    [MTX] host up: depths 2 snap 529468 img 352684
    [MTX] ace image: 352652 B at 3633208 entry 3633216 bss 22420
    [MTX] depth1: guest alloc 77 B at 4312064, entering
    [MTX] request switch -> 1
    [MTX] guest OK: magic 1297373255 addr 4312064 loaded 4312064

Three things that line proves:

* **The real engine image loads.** 352,652 B of `ACE_CODE`, fetched by name through
  ACE's own WAD layer into the root's depth-owned slot, the entry checked at
  payload + 8 and the version dword checked against `ace_git_version` — which is
  what distinguishes the real image from an unrelated lump.
* **Loaded code executes.** The guest is a purpose-built WAD lump (45 B, installed
  by `make guest`), not something linked into the engine. It reports 0x4D545847
  ('MTXG') and the entry address it computed **for itself**, and that is exactly
  the address the loader jumped to: WAD bytes landed at a depth-owned address,
  were relocated by nothing but their own position independence, and ran there.
* **It is a coroutine, not a one-shot call.** The guest yields between its two
  reports: control leaves the loaded image, the root runs a frame, and the image
  resumes inside itself with its stack intact.

**Why the bases arrive in a descriptor.** ldr0N cannot take ESI/EDI/EBX from its
caller: a depth entry is dispatched through C (`mtx_run_entry`), which has already
used those registers by then. So the bases travel as data — which is also exactly
the reentrant property the original LDR0 lacks, since it re-derives its base from
the startup stack and therefore only ever works once.

**Two bugs the host test caught**, both invisible in DOS because ldr0N never
returns there:

* It used EBP to hold the printf address — as the original LDR0 did, which never
  returned either. Fine for the hand-off, but it means the function destroys its
  caller's frame pointer, so nothing that intends to continue can call it. It now
  saves and restores EBP (two instructions, still 39 bytes total).
* It loaded the hand-off registers *before* calling printf. printf is a C function,
  so it clobbers eax/ecx/edx, and the next stage received a garbage argument in
  ECX. The registers are now loaded after the call. Note the shape of this one: in
  the live run the guest ignores ECX and its siblings, so the bug silently
  corrupted a value nobody was looking at — which is precisely the class a test
  notices and a DOS run does not.

Test [11] covers both by pointing `code_base` at a stand-in for Doom's printf
(ldr0N reaches it at `code_base + 0x3FE40`) and having the stage record what it was
handed.

**What is deliberately NOT done: entering the real image.** ACE's entry relocates
itself, clears `.bss` and calls `ace_main`, which allocates the whole of remaining
RAM as its zone and re-initialises video. Both are per-depth work that does not
exist yet, so the engine image is load-and-verify while the guest is load-and-run.

**The image slot costs depth ceiling.** Budgeting one image per depth is the honest
accounting, and it drops the current fit from 3 depths to 2 in a 3 MiB arena —
which is why the boot reports `depths 2`. Sharing one image would be cheaper but
wrong: ACE patches code at run time, so two depths on one code page would fight
over the same bytes.

## A depth's own zone (`Z_Init`, per depth)

A depth is not an instance until it has a heap of its own. Doom's allocator walks a
`memzone_t` header and a ring of `memblock_t` blocks, so handing a depth raw arena
memory is not enough: the first `Z_Malloc` would read an uninitialised block header.
`Z_Init` is now depth-aware.

    depth 0   walks dpmi_get_ram() down and takes what is left, as before
    depth >0  builds its zone inside the arena slot reserved for it

Both go through the same construction (`mtx_zone_build`, `nested/mtx_zone.c`), and
`mtx_engine_reserve()` became idempotent: a nested depth's ACE image runs
`ace_main` → `Z_Init` too, and the arena belongs to the *machine* rather than to a
depth. A second claim would fail — the memory is already gone — and the depth would
silently end up with less than the planner budgeted against.

The layout is mirrored rather than shared: `mtx_zone.h` restates
`memzone_t`/`memblock_t` the way `ldr1n.h` restates `lumpinfo_t`, because the nested
side also has to build on the host. The engine build asserts the two sizes match, so
a drift in `engine.h` is a compile error instead of a corrupted heap.

**Verified live, in DOOM2.EXE** (0 illegal reads, 0 `DYNX86`, 250+ frames):

    [MTX] depth1 zone 262144 B at 3987460 build 0 alloc 3987516 probe 0
    [MTX] guest OK: magic 1297373255 addr 4313612 loaded 4313612 zone 3987460

Three separate claims are packed into that line:

* `build 0` — the zone passed Doom's own heap checks: block touches the next block,
  back links intact, no two consecutive free blocks.
* `alloc 3987516` — Doom's *real* `Z_Malloc` returned a pointer **inside the depth's
  zone**: `3987516 - 3987460 = 56 = sizeof(memzone_t) (32) + sizeof(memblock_t)
  (24)`. So the allocator walked *this* depth's header and carved *this* depth's
  block. That is the difference between a zone that looks right and one that works.
* `probe 0` — the checks still hold *after* the allocate/free round trip, i.e. the
  allocator left the ring intact.

The guest obtains its zone through its service table (`svc[3]`), which is the same
call `Z_Init`'s nested branch makes — not a test-only shortcut — and it reports the
pointer back (`zone 3987460`, matching what the host built). Meanwhile the root kept
rendering MAP01 with the HUD intact: the probe swaps `mainzone` for the duration of
the round trip and puts it straight back, so the root's heap is never the one being
allocated from.

Test [12] covers the builder and the checker on the host, and deliberately includes
the corruption cases — a checker that cannot see a broken zone would pass every zone
ever built. It was verified to FAIL against a checker neutered to always report
"intact".

## State footprint and the zone-sizing decision (measured)

From `engine/doom_vars.S` (416 variables, 0x1A84 `finetangent` … 0x75CA0
`volume_val`), the built engine ELF and the live boot log:

    Doom data region        475,676 B   465 KiB
    engine .bss              53,792 B    53 KiB
    engine .data              9,056 B     9 KiB
    ------------------------------------------------
    snapshot per depth      529,468 B   517 KiB  <- live: "snap 529468"
    + zone (policy)         262,144 B   256 KiB  <- MTX_ZONE_PER_DEPTH
    + screen                 64,000 B    63 KiB  <- 320x200x8
    + stack                   8,192 B     8 KiB
    + code image            352,684 B   344 KiB  <- bss + ACE_CODE lump
    ------------------------------------------------
    per depth             1,216,488 B  1.16 MiB

A 3 MiB arena divides into two of those, which is what the live run reports
(`depths 2`) — still enough to nest, and now arithmetic rather than observed luck.

The levers, in order of effect:

* **zone per depth** — Doom asks for 8 MiB for its own single instance. Sizing a
  nesting depth's zone at 256 KiB is what makes nesting fit at all.
* **the image slot** — 344 KiB per depth, the largest item after the snapshot, and
  per-depth on purpose (see the loaders section).
* **snapshot** — 529 KiB, dominated by Doom's data region. Shrinking it means
  knowing which Doom globals a nested depth actually needs.

`screen` is still an estimate, and worth being honest about: `MTX_SCREEN_BYTES` is
one 320x200x8 framebuffer, but the engine also uses `screen_buffer + 64000 + 69632`
as loader scratch (`textpars.h`), so 64 KiB is less than the engine's real
appetite for that pointer.

## Not done yet (the honest remainder)

* **Entering the real engine image.** The next real step. The depth half of `Z_Init`
  now exists (above), so `ace_main` at a depth would get a real heap rather than walk
  uninitialised memory — what is left is video (below), plus the fact that
  `ace_main` re-patches Doom's *shared* code at run time, which the per-depth hook
  snapshots exist for but which nothing exercises yet (`nhok` is still 0).
* **Video is not virtualised.** Each depth has its own `screen_buffer` pointer, but
  they all present through the same `swap_buffers` IRQ, so the visible screen is
  whichever depth presented last. A child needs an off-screen buffer with the root
  blitting the active one.
* `.data` is still not in the snapshot region list — it needs a
  `PROVIDE(_data_start/_data_end)` pair in `engine/linker.ld`. `mod_config` lives
  there, and when it is added it needs the same split that keeps `mtx_hs` out.
* Nothing sets a switch request from the game yet: there is no key bound to
  `mtx_request_yield()` / `mtx_request_switch()`, so the policy is a frame counter.
* Hook registration is manual and nothing registers anything yet (`nhok` is 0).
  The per-depth hook save/restore exists and is tested; it has no occupants.
* The guest is a purpose-built 45-byte payload, not a second Doom. It proves the
  loader works; it is not an instance.
* `screen_per_depth` and `zone_per_depth` still need measuring on hardware.
