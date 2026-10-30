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
`mtx_engine_zone_init`, the nested half of `Z_Init`, + the video presentation the
host owns), `mtx_ldr.c` (the three ldr1n services wired to the engine, plus image
validation), and `main.c`'s `Z_Init`, which branches on depth.

**Build trap worth knowing:** `engine/makefile` now carries header dependencies
(`${OBJ}: ${HDRS}`, every object on every header). Without them, editing
`nested/mtx_host.h` leaves the other objects compiled against the old `mtx_hs`
offsets while `mtx_boot.o` is rebuilt against the new ones — measured as
`ace image: load FAILED (-1)` with a perfectly good WAD and a clean boot. The rule
must stay after `build:` or make takes its first target as the default goal.

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
      [13] depth screen budget .... PASS
    ALL PASS

[1] three depths nest and unwind in order (A C E D B).
[2] two depths ping-pong via `mtx_switch_to`; each keeps its own local counter.
[3] pushing past the ceiling is refused exactly once, at depth max_depth-1.
[4] `save_state`/`load_state` fire in the right order at each crossing.
[5] each depth runs on a genuinely distinct stack (ESP values differ).
[6] three-deep state isolation: child2 sees child1's globals and hook bytes but
    its *own* heap, framebuffer and Doom-screen pointer; after it returns child1's
    values are intact; after that returns the root's values are intact. No leak in
    either direction. Three pins, not two: the third is the one Doom's renderer
    actually writes through.
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
[13] the depth screen budget covers what the engine puts behind `screen_buffer`.
    The engine's appetite is written as a literal (64000 + 69632 + 0x28000), not
    recomputed from the constants under test — a test that derives its expectation
    from the thing it checks passes whatever that thing becomes. Verified to FAIL
    with `MTX_SCREEN_BYTES` set back to one frame.

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
    [MTX] host up: depths 2 snap 540041 img 355428
    [MTX] screen region 297472 B/depth (frame 64000 + loader 69632 + parse 163840)
    [MTX] isolation: 3 regions, 405 patches, snapshot 540041 B
    [MTX] probe site 2022787, byte there now 232
    [MTX] doom screens[0] at 2572228 = 36528; ace screen_buffer = 36528
    [MTX] ace image: 355396 B at 3635952 entry 3635960 bss 22148
    [MTX] depth1 entered
    [MTX] depth1: guest alloc 128 B at 4551032, entering
    [MTX] depth1 zone 262144 B at 3991392 build 0 alloc 3991448 probe 0
    [MTX] hook isolation: depth1 wrote 204 at 2022787 (read back 204); root now 232, was 232 => ISOLATED
    [MTX] doom screens[0] -> 4253536: marker in depth1 buffer YES, in root buffer NO
    [MTX] frame 1 child=1
    ...
    [MTX] frame 30 child=1
    [MTX] request switch -> 1        <- frame boundary, depth 0 -> 1
    [MTX] present -> depth 1, buffer 4250976 (blit runs this frame; checked next frame)
    [MTX] screen check: depth 1 CONFIRMED (vga 16843009 vs buffer 16843009)
    [MTX] guest OK: magic 1297373255 addr 4548472 loaded 4548472 zone 3988832
    [MTX] present -> depth 0, buffer 36528 (blit runs this frame; checked next frame)
    [MTX] screen check: depth 0 CONFIRMED (vga 16843014 vs buffer 16843014)
    [MTX] frame 400 child=1          <- root keeps running on its own frame loop

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

## Video: which depth the screen shows

A frame reaches the screen through ONE asm routine, `I_FinishUpdate`
(`engine/asm.S`): it copies 64000 bytes to 0xA0000. Its copy SOURCE is a dword baked
into its own code by `init_draw()` — the value of `screen_buffer` at init, i.e. the
ROOT's buffer. Nothing about that is per-depth, so before this change every depth
presented the root's buffer: a nested instance's frames could never reach the screen,
and there was no way to choose between depths.

The host owns that dword now (`mtx_present`, `engine/mtx_boot.c`). The offset is
verified against the built code rather than assumed — the routine starts with three
1-byte pushes, so the `mov $imm32,%esi` immediate begins exactly at
`I_FinishUpdate+4`, which is the offset `init_draw()` already patches:

    1141:  57                   push %edi
    1142:  56                   push %esi
    1143:  51                   push %ecx
    1144:  be 78 56 34 12       mov $0x12345678,%esi   <- the source, patched at +4
    1149:  bf 00 00 0a 00       mov $0xa0000,%edi
    114e:  b9 80 3e 00 00       mov $0x3e80,%ecx
    1153:  f3 a5                rep movsl %ds:(%esi),%es:(%edi)

**Setting the source is not evidence, so the screen is read back.** `mtx_present`
compares the first dword of VGA memory with the depth's own buffer. On the frame of
the change the answer is expected to be "not yet" — the blit for that frame has not
run when the policy does — so the check is repeated one frame later. That is the
difference between "I pointed the pointer" and "the screen changed":

    [MTX] present -> depth 1, buffer 4250976 (blit runs this frame; checked next frame)
    [MTX] screen check: depth 1 CONFIRMED (vga 16843009 vs buffer 16843009)
    [MTX] present -> depth 0, buffer 36528 (blit runs this frame; checked next frame)
    [MTX] screen check: depth 0 CONFIRMED (vga 16843014 vs buffer 16843014)

16843009 is `0x01010101`, the first dword of the depth's colour-bar pattern;
16843014 is `0x01010106`, the root's first pixel. Both directions are checked, so the
screen really did change and really did change back — 0 illegal reads, 0 `DYNX86`.

The pattern is drawn by the DEPTH, not by the host: the guest calls a service that
fills `screen_buffer`, and at depth 1 `screen_buffer` is that depth's own region
(pinned on switch-in). One run captures both states: the `_a` capture is the nested
depth's bars, `_b` and `_c` are the root's MAP01 after the switch back. (That run's
directory was not kept; the same mechanism is what the engine-image path above now
uses, and `verify/trial-confirm/` is its record.)

### A depth's screen region is bigger than a frame

`MTX_SCREEN_BYTES` used to be one 320x200 framebuffer (64000). That is the minimum a
depth can render into, not what the engine needs behind the pointer: `main.c` reads
69632 bytes at `screen_buffer + 64000` (the loader progress bar) and `textpars.h`
reads 0x28000 at `screen_buffer + 64000 + 69632` (`TP_MEMORY_ADDR`). A depth sized at
one frame would let a nested engine write ~233 KiB past its own buffer — into the next
depth's zone. The region is now the sum of those three parts (297472 B), and test [13]
asserts the total against the engine's measured appetite as a **literal**, so shrinking
it fails the tests instead of silently corrupting a neighbour.

### Doom's OWN screen pointer, not just ACE's copy

Pinning ACE's `screen_buffer` was not enough. ACE reads Doom's `screens[0]` exactly
once, at init (main.c's hook list: `0x00074FC4 -> screen_buffer`); after that the two
are separate variables, and Doom's RENDERER keeps writing through ITS own — so a
nested depth would still paint over the root's frame. Both are pinned now
(`MTX_PIN_DOOMSCREEN`, registered in `dos_wire.c` against
`doom_data_segment + 0x74FC4`).

The address is ACE's own, not a guess: it sits between `usegamma` (0x74FC0) and
`mainzone` (0x74FE0), where vanilla Doom keeps `byte *screens[5]`, and `textpars.c`
says outright that "memory used for text is in `screens[]`". Nothing in the engine
ever reassigns it — Doom allocates the screen once at startup — which is what makes
forcing a depth's value on every switch-in safe. Measured at boot, before pinning:

    [MTX] doom screens[0] at 2572228 = 36528; ace screen_buffer = 36528

Same address, same value: the pin is aimed at the variable the renderer uses.

**Checked by where the marker lands, not by intent.** The guest writes a marker
through Doom's own pointer, and the engine refuses that write unless the variable
already points at the depth's own buffer — a wrong write there would land in the
root's *visible* frame, which is the exact failure the pin prevents. Then both
buffers are read straight out of the arena:

    [MTX] doom screens[0] -> 4251936: marker in depth1 buffer YES, in root buffer NO

4251936 is depth 1's own region. So a write through Doom's `screens[0]` at depth 1
lands in depth 1's memory and nowhere near the root's frame. The marker is
`0x4D545853` ('MTXS'), which is also why the next screen check reads
`vga 1297373267 vs buffer 1297373267` — the thing on screen is that write.

## Per-depth isolation: what a depth can change, and what it cannot

A depth is only an instance if the state it shares with the others is exactly the state
it is allowed to share. Four kinds of machine state move per depth now, and the fourth
is the one that had to be hunted down.

  1. **Regions.** Doom's data range (475,676 B, from `doom_vars.S`), the engine's
     `.bss` split around `mtx_hs` so the host's own control state is never covered, and
     now the engine's `.data` (9,056 B, which holds `mod_config` among others).
     `.data` needed a `_data_start`/`_data_end` pair in `engine/linker.ld`; it is a
     section of its own, so `mtx_hs` cannot be inside it — checked anyway, because a
     region covering the host's control state is the wild jump the `.bss` split exists
     to prevent.
  2. **Pinned pointers.** `mainzone`, ACE's `screen_buffer`, and Doom's own
     `screens[0]`. Three pins; the third is the one Doom's renderer writes through.
  3. **Patched locations.** See below.
  4. **Per-depth zone and screen.** Not snapshotted, because they are separate memory
     at separate addresses; that is what makes them per-depth for free.

### 405 patches, and why they were the missing piece

ACE does not patch its own code: it patches DOOM's. Measured from the built engine's
hook table (419 entries, `_hooks_start`..`_hooks_end`):

    JMP_ACE 24  JMP_DOOM 24  CALL_ACE 111  CALL_DOOM 1
    UINT32  87  UINT16   84  UINT8     40  ABSADDR_DATA 8  SET_NOPS 26
    IMPORT  10  READ32    4

`IMPORT` and `READ*` copy *from* Doom *into* ACE, so they have nothing to restore —
ACE's own memory is already per-depth through the region snapshot. That leaves **405
patches that modify Doom's memory, totalling 1,517 bytes**, and **399 of them land in
Doom's CODE segment, which no snapshot region covers.** Those 399 are the whole reason
this exists: a nested instance re-patching Doom's code at run time would otherwise be
visible to every other depth, root included.

The addresses are read out of the engine's OWN hook table rather than transcribed into
a list. A list would rot the first time the engine gains a hook, and the failure would
look like memory corruption instead of a missing entry. `mtx_hooks.c` mirrors
`utils.h`'s `HOOK_` enum and the engine build asserts the mirror, so a drift there is a
compile error rather than a wrong restore length. The patch lengths come from what the
installer actually writes (opcode + rel32 for call/jmp, the `value` field as a memset
count for `SET_NOPS`, the length packed in the type's high bits for `MEM_COPY`) — and a
registry that cannot take them all is *reported*, not silently truncated, because an
isolation that quietly drops some patches looks exactly like one that works.

**Verified live, not asserted** (0 illegal reads, 0 `DYNX86`):

    [MTX] isolation: 3 regions, 405 patches, snapshot 540041 B
    [MTX] probe site 2022787, byte there now 232
    [MTX] hook isolation: depth1 wrote 204 at 2022787 (read back 204);
          root now 232, was 232 => ISOLATED

Read it as two claims, because either alone would be worthless. The depth wrote 0xCC
over an `0xE8` (a call opcode ACE had installed) and read it back — so the write really
happened, inside the depth. Then the root read the same address after the switch back
and found `0xE8` again — so it did not survive. A write that never happened would look
identical to perfect isolation, which is why both halves are logged.

The snapshot figure is an independent check on the arithmetic: 529,468 + 9,056 + 1,517
= 540,041 exactly.

## Entering the real engine image: a depth boots an instance, and what it renders

`MTX_CHILD_ENGINE_IMAGE` in `engine/mtx_boot.c` switches depth 1 from the purpose-built
guest to the real ACE image, and it is **on** in the shipped build: a depth now boots a
second engine instance, builds its own heap, draws into its own framebuffer and reaches
the screen. Reproduce with `verify/trial.sh <label> 30` and look at the capture.

Four things had to be right, and each was found by measurement rather than reasoning:

  - **The entry contract.** ACE's `_start` needs `%ecx` = the entry address, not the
    guest's service table: it does `sub $0x1008,%ecx` to recover its base and relocates
    everything relative to that. Hence two functions — `mtx_ldr_enter` (ECX = service
    table) and `mtx_ldr_enter_engine` (ECX = entry).
  - **Per-depth isolation**, above: 405 patches and `.data`, without which a second
    instance's `ace_main` re-patching Doom's shared code would corrupt the root.
  - **A depth's raw (non-zone) memory.** `nested/mtx_raw.*` is the pool; the router in
    `engine/mtx_boot.c` (`mtx_engine_alloc` / `_realloc` / `_free`) sends the engine's
    loader allocations to it, and the root's behaviour is unchanged.
  - **A way for an instance to know what it is.** `nested/mtx_params.h`: a block the
    host fills and hands over in `%ebp` at the entry.

### Giving a depth its own raw memory

The zone was half the job. A depth could build its own heap, and Doom's allocator worked
in it — but `ace_main` also asks the *raw* allocator for memory, and that one is
machine-global. Measured: the root's `Z_Init` walks `dpmi_get_ram()` down until an
allocation succeeds, so it consumes everything except a 32 KiB reserve, and the instance
then died in `init_render()` asking for 209,184 B with `free DPMI after root Z_Init:
61440 B` left. Raising the arena does not help — the root consumes the remainder either
way.

So the engine's own allocation entry points go through a depth-aware router, and a depth
gets a pool carved out of the arena (`MTX_RAW_PER_DEPTH`, 2 MiB). The pool is a bump
allocator with a per-block header, and its semantics are deliberately honest:

  - **A free only returns memory when the block is the newest one.** Loaders allocate a
    buffer, fill it, hand it on and free it, so that case is the common one and it does
    reclaim. A free of anything older is *counted* (`leaked` / `leaked_bytes`), so "the
    pool is big enough" is a number rather than an assumption.
  - **A zero-byte request is served**, like Doom's own `malloc(0)` — and it occupies a
    minimum. Refusing it turned a working call into `Texture memory allocation failed!
    (0B)` and a bluescreen, measured.
  - **The tip test compares OCCUPANCY, not the requested size.** A 1-byte request
    occupies 8 bytes; comparing against 1 meant such a block was never the tip, so the
    engine's lump-table pattern (`lumpinfo = ldr_malloc(1)` grown into tens of kB) moved
    the block, leaked the original, and the instance's lump table silently stopped
    growing — which is what the "PLAYFAL"/`W_CacheLumpNum` failures below really were.
  - **The pool aligns the base it is handed, and so does the arena.** `mtx_arena_alloc`
    aligns offsets *relative* to its base, and the base comes from Doom's allocator, which
    is happy to return 4-byte alignment. Measured: the pool landed 4 bytes off 8, its own
    ownership test rejected all of its own memory, and `free`/`realloc` fell through to
    the DPMI heap — where Doom's `realloc` read the block magic as a size, decided the
    block was already big enough, and returned the pointer untouched.

### Telling an instance what it is

An instance is a relocated COPY of the engine with its own `.bss`, so its `mtx_hs` is a
different, empty object: it cannot read the host's control state, and the host cannot
assume anything about the instance's. Measured consequence: the instance's `ldr_malloc`
consulted an empty `mtx_hs`, decided it was the root, and asked DPMI for 209,184 B while
its own pool sat unused.

`nested/mtx_params.h` is the answer: the host fills one block per depth (inside `mtx_hs`,
so it is stable across switches and readable by the host's own reporting) and hands its
address over in `%ebp`, which `ldr0N` loads and the engine's entry stores. Two traps,
both measured:

  - **The store cannot be done in `asm.S`.** `mov %eax,mtx_params(%ecx)` is the idiom the
    rest of the entry uses, but the assembler records that displacement as a relocation,
    so the entry's own relocator adds the image base to it a *second* time and the store
    lands elsewhere. The pointer stayed null through a whole nested boot. Keeping the
    entry free of new `%ecx`-relative global accesses and doing the store from C (a
    normally-relocated absolute reference) fixes it.
  - **The value must not be recognised by address.** The blocks live in the HOST's
    `mtx_hs` while an instance's own `&mtx_hs` is somewhere else entirely, so a range test
    would reject the very block being handed over. The check is on the block's contents:
    a magic word, and the block's own address inside it.

The result is verifiable from both ends, because the instance logs through the host's
descriptor (DOS descriptors are process-wide, so its `log_fd` writes into the same
MTX.LOG): the host logs the block it filled, and the instance logs the block it received.

How far it gets, from the log:

    [MTX] depth1 params at 3039412: depth 1 zone 11432720 screen 11694864 raw 3038896 log 7
    [MTX] depth1: entering, ECX=11992360 ESI=1802240 EDI=2093056 EBP=3039412, free DPMI 61440 B
    [MTX] instance: depth 1 zone 11432720 screen 11694864 raw 3038896 log 7
    [MTX] depth1 zone 262144 B at 11432720 build 0 alloc 11432776 probe 0 (instance)

The block the host filled and the block the instance read agree, line for line. The
`(instance)` line is the instance's own: `build 0` / `probe 0` are Doom's own zone
invariants passing, and `alloc 11432776` is a real `Z_Malloc` inside the instance's heap.
The instance's pool served its whole boot out of its own 2 MiB (peak ≈ 591 KB measured).

Then it draws. `verify/trial-confirm/` shows the instance in mode 13h with Doom's imagery
tiled across the screen: wrong content, but its own frames, produced by a second engine
instance — the rendering path end to end (load → zone → pool → init → draw → VGA).

**What is still wrong: the instance's lump data.** When it takes a lump-caching path it
fails in a way that is unmistakably the pool's bytes read as data:

    Module that caused this error is WAD
    Lump PLAYFAL read failed!                      (name mangled: PLAYPAL)
    W_CacheLumpNum: 19796 >= numlumps              (19796 = 0x4D54 = "MT", the top half
                                                    of the pool's 'MTXR' block magic)
    raw pool: request for 1525682085 B refused      (1.5 GB, same corruption, other end)

So something hands Doom a pointer into pool metadata. It is not the zero-size/minimum
problem above (that was fixed and the failure survived it), and it is not ACE's own
lump-cache call sites: probes were added to all four (`ldr_flat`, `ldr_sprite`,
`ldr_texture`, `font.c`) and none fired, so the suspect is Doom's own code. It is also not
reproducible on every run — the instance only takes that path when a key press reaches its
menu, which happens in roughly two runs out of six.

The instruments for it are left in place, each marked `TEMPORARY`, each writing its own
file next to `MTX.LOG` (dump them with `python3 nested/dump_probe.py <file>`):

  - `MTXN.LOG` — what an instance's own copy of the engine believes about itself
  - `MTXW.LOG` — a failing lump read: the table entry, the fd, the offset, the destination
  - `MTXL.LOG` — an invalid lump index at ACE's own cache sites, with the table it came from
  - `MTXA.LOG` — an absurd allocation request, with the caller's return address (its offset
    from a known symbol resolves against the root's ELF, because both copies are the same
    image at different bases)

Also still open: a fatal error *inside a depth* terminates the whole program (the
bluescreen is `dos_exit`). One instance must not be able to kill the others, and the host
has no policy for that yet. The depth stack is no longer a suspicion — it was raised from
8 KiB to 64 KiB (`MTX_STACK_SIZE`), since an instance runs its whole boot on it.

## State footprint and the zone-sizing decision (measured)

From `engine/doom_vars.S` (416 variables, 0x1A84 `finetangent` … 0x75CA0
`volume_val`), the built engine ELF and the live boot log:

    Doom data region        475,676 B   465 KiB
    engine .bss              53,792 B    53 KiB
    engine .data              9,056 B     9 KiB
    ------------------------------------------------
    snapshot per depth      540,041 B   527 KiB  <- live: "snapshot 540041 B"
                                                   = 529,468 + 9,056 (.data)
                                                   + 1,517 (405 hook bytes)
    + zone (policy)         262,144 B   256 KiB  <- MTX_ZONE_PER_DEPTH
    + screen                297,472 B   290 KiB  <- the engine's real appetite
                                                   for the screen_buffer pointer
    + stack                   8,192 B     8 KiB
    + code image            355,428 B   347 KiB  <- bss + ACE_CODE lump
    ------------------------------------------------
    per depth             1,463,277 B  1.40 MiB

A 3 MiB arena divides into two of those, which is what the live run reports
(`depths 2`) — still enough to nest, and now arithmetic rather than observed luck.

The levers, in order of effect:

* **zone per depth** — Doom asks for 8 MiB for its own single instance. Sizing a
  nesting depth's zone at 256 KiB is what makes nesting fit at all.
* **the image slot** — 344 KiB per depth, the largest item after the snapshot, and
  per-depth on purpose (see the loaders section).
* **snapshot** — 529 KiB, dominated by Doom's data region. Shrinking it means
  knowing which Doom globals a nested depth actually needs.

`screen` used to be an estimate and was honest about it: `MTX_SCREEN_BYTES` was one
320x200x8 framebuffer, while the engine also uses `screen_buffer + 64000 + 69632` as
a parse arena (`textpars.h`). It is now measured from the engine's source rather
than guessed — see the video section — and it is the third largest item per depth.

## Not done yet (the honest remainder)

* **Entering the real engine image: it boots, and then it renders wrong.** A depth now
  loads the engine image, enters it, reads its parameter block, builds its own heap and
  serves its whole boot out of its own raw pool (peak ≈ 591 KB of 2 MiB), and draws into
  its own framebuffer — see the section above, which ends with the one thing still
  broken: something hands Doom a pointer into the pool's own metadata, which shows up as
  a mangled lump name, a lump index of `0x4D54` ('MT') and a 1.5 GB allocation request.
  The four probe files (`MTXN`/`MTXW`/`MTXL`/`MTXA`, all marked `TEMPORARY` in the source)
  are the instruments for it, and none of ACE's own lump-cache sites fired — so the
  suspect is Doom's own code.
* **Video: the remaining gap is VESA and mode set.** Both screen pointers now follow
  the depth (ACE's `screen_buffer` and Doom's `screens[0]`), each depth's screen
  region is sized for the engine's real appetite, and the presentation source is the
  host's decision — so a depth's frames reach the screen and the visible one is
  chosen. Not covered: VESA mode is a different presentation path entirely
  (`swap_buffers` page-flip, and `R_InitBuffer`'s framebuffer patch); and entering a
  real engine image re-runs `I_InitGraphics`, i.e. a real mode set at a depth, which
  is not virtualised.
* `.data` is now in the snapshot region list (9,056 B, split-checked against `mtx_hs`).
* Hook isolation is no longer empty. `nhok` is 405, populated from the engine's own
  hook table, and the isolation is verified in DOS by writing over a patch site at a
  depth and confirming the root never sees it. This was the prerequisite for entering a
  real engine image: `ace_main` re-patches Doom's shared code at run time.
* Nothing sets a switch request from the game yet: there is no key bound to
  `mtx_request_yield()` / `mtx_request_switch()`, so the policy is a frame counter.
  (And see the input-routing item below — a key alone would not be enough.)
* **The depth ceiling is a reservation policy, not a wall.** The arena is requested
  at 3 MiB (`mtx_engine_reserve`, sizes `{3, 2, 0}`) and a depth costs 1,463,277 B,
  so the live run reports 2. The arithmetic, against ~60 MiB of DPMI RAM: 8 MiB → 5
  depths, 12 MiB → 8, 16 MiB → 11, 20 MiB → 12. What that costs is the ROOT's zone,
  which gets whatever is left — so this is a trade to measure, not a limit. Nothing
  has been tried above 3 MiB yet.
* Hook registration is manual and nothing registers anything yet (`nhok` is 0).
  The per-depth hook save/restore exists and is tested; it has no occupants.
* Nothing sets a switch request from the game yet: there is no key bound to
  `mtx_request_yield()` / `mtx_request_switch()`, so the policy is a frame counter.
  (And see the input-routing item below — a key alone would not be enough.)
* The guest is a purpose-built 96-byte payload, not a second Doom. It proves the
  loader, the zone, the screen and the pin work; it is not an instance.
* `zone_per_depth` (256 KiB) is still a policy number rather than a measurement of a
  real level's working set. `screen_per_depth` is no longer a guess — it is derived
  from the engine's own use of the pointer (above).
* Input is not routed per depth at all: the keyboard belongs to the root, and a
  nested instance has no way to be the one that receives keys.
