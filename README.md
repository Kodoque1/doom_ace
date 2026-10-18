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
    dos_state.h/.c   engine-agnostic per-depth state: regions, hooks, pins
    dos_wire.h/.c    concrete ACE wiring (regions, pins, hook registration)
    dos_alloc.h/.c   arena, budget planner, one-call boot
    ldr0N.S          reentrant first-stage loader
    ldr1n.h/.c       reentrant second stage (continuation-based, non-destructive)
    ldr1N.S          its final hand-off (ESI/EDI/ECX, jmp *%ecx)
    test_mtx.c       host proof (32-bit freestanding, raw syscalls)
    Makefile         make run | make doscheck | make ldr0n

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

`make doscheck` — all seven DOS-target sources (`mtx_core.c`, `mtx_switch.S`,
`dos_state.c`, `dos_alloc.c`, `dos_wire.c`, `ldr1n.c`, `ldr1N.S`) compile clean
with the exact flags from `doom_ace/engine/makefile` (`-m32 -march=i386
-mgeneral-regs-only …`), with the static pool compiled out as ACE would need.

`make ldr0n` — assembles to a 24-byte image, well inside the 256-byte ACE_LDR0
patch budget.

## State footprint and the zone-sizing decision (measured)

From `engine/doom_vars.S` (416 variables, 0x1A84 `finetangent` … 0x75CA0
`volume_val`) and the built engine ELF:

    Doom data region        475,676 B   465 KiB
    engine .bss              53,792 B    53 KiB
    engine .data (see below)  9,056 B     9 KiB
    ------------------------------------------------
    snapshot per depth      538,524 B   526 KiB
    + screen (320x200x3)    192,000 B   188 KiB
    + stack                   8,192 B     8 KiB
    fixed overhead/depth                721 KiB

Against the 60.4 MiB of DPMI memory the ACE boot log reports
(`DPMI memory: 0x3c72000`):

    zone/depth    per-depth    depths that fit
      8 MiB        8,913 KiB      6      <- Doom's own default
      4 MiB        4,817 KiB     12      <- the threshold for depth 12
      2 MiB        2,769 KiB     12      (32.5 MiB used)
    512 KiB        1,233 KiB     12      (14.5 MiB used)

**The ceiling is memory-bound, not context-bound, and the lever is the per-depth
zone.** Doom asks for 8 MiB at boot, which caps nesting at 6. Sizing each depth's
zone at 4 MiB or below lets all 12 fit. Doom's 8 MiB is a generous default for a
single instance, not a per-level requirement — so this is a sizing decision, not
a wall. The decision belongs in the wiring layer's `zone_per_depth` argument.
`screen` is an estimate (framebuffer + loader patch scratch) and should be
measured; `snapshot` is reported by `mtx_dos_snapshot_bytes()`.

## Not done yet (the honest remainder)

* `.data` is not yet in the region list — it needs a
  `PROVIDE(_data_start/_data_end)` pair in `engine/linker.ld`. Don't forget it:
  `mod_config` lives there. (`dos_wire.c` says so inline.)
* The DOS-side *caller* exists (`mtx_dos_boot`) but nothing invokes it from the
  engine yet: the engine's init has to own the arena and call it.
* No per-frame yield point. The switch only happens if the running instance
  reaches it, so the host needs a hook (ACE already hooks draw/think) that calls
  `mtx_yield()` / services a pending `mtx_switch_to()`.
* Zone/heap grooming for nested loads — ZONE's split logic differs once ACE owns
  it, so the original 8192-byte assumption does not carry.
* Hook registration is manual (`mtx_dos_wire_hook`); it should be called from
  `utils_install_hooks()` for every `hook_t->addr`.
* `screen_per_depth` and `zone_per_depth` need measuring on hardware.
