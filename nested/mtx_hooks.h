/*
 * matryoshka/mtx_hooks.h
 *
 * Which bytes ACE overwrote in DOOM's memory - so a depth's copy can be saved and
 * restored.
 *
 * WHY THIS EXISTS
 * ---------------
 * ACE does not patch its own code: it patches DOOM's. Its hook table describes 419
 * patches, of which 405 modify Doom's memory - and 399 of those land in Doom's CODE
 * segment, which no snapshot region covers (the region snapshot is Doom's *data*
 * range). So the patches are the one piece of shared machine state that per-depth
 * snapshots cannot see.
 *
 * That matters the moment a real instance runs at a depth: `ace_main` re-installs
 * the engine's hooks, and without isolation one depth's patch is visible to every
 * other depth - including the root's world. The save/restore machinery already
 * exists and is unit-tested; what was missing was knowing WHERE to apply it.
 *
 * The table is walked rather than transcribed into a hand-written list: a list would
 * silently rot the first time the engine gains a hook, and the failure would look
 * like memory corruption rather than a missing entry.
 *
 * The constants below MIRROR engine/utils.h's HOOK_ enum, in the same spirit as
 * mtx_zone.h mirroring memzone_t and ldr1n.h mirroring lumpinfo_t: this file also has
 * to build on the host. The engine build asserts the two agree, so a drift in
 * utils.h is a compile error rather than a wrong restore length.
 */
#ifndef MATRYOSHKA_HOOKS_H
#define MATRYOSHKA_HOOKS_H

#include <stdint.h>

/* mirrored from engine/utils.h */
#define MTX_HOOK_JMP_ACE       0
#define MTX_HOOK_JMP_DOOM      1
#define MTX_HOOK_CALL_ACE      2
#define MTX_HOOK_CALL_DOOM     3
#define MTX_HOOK_UINT32        4
#define MTX_HOOK_UINT16        5
#define MTX_HOOK_UINT8         6
#define MTX_HOOK_ABSADDR_CODE  7
#define MTX_HOOK_ABSADDR_DATA  8
#define MTX_HOOK_SET_NOPS      9
#define MTX_HOOK_MEM_COPY     10
#define MTX_HOOK_IMPORT       11	/* modifies ACE memory, not Doom */
#define MTX_HOOK_READ8        12	/* ditto */
#define MTX_HOOK_READ16       13
#define MTX_HOOK_READ32       14
#define MTX_HOOK_CODE  0x40000000u
#define MTX_HOOK_DATA  0x80000000u

/* An entry of the engine's hook table (engine/utils.h's hook_t: 12 bytes). */
typedef struct
{
	uint32_t addr;
	uint32_t type;
	uint32_t value;
} mtx_hook_t;

/*
 * How many bytes the engine overwrote at this hook's address - i.e. how many have to
 * be saved and restored. Returns 0 for hooks that do not modify Doom's memory
 * (IMPORT / READ*), which have nothing to restore: they COPY from Doom into ACE, and
 * ACE's own memory is already per-depth through the region snapshot.
 *
 * `value` is the entry's third field, and it is needed: HOOK_SET_NOPS fills `value`
 * bytes with 0x90, so the length is not derivable from the type alone.
 */
uint32_t mtx_hook_patch_len(uint32_t type, uint32_t value);

/*
 * Where the patch landed, in absolute terms. Mirrors the engine's own installer:
 * DATA_HOOK adds the data segment, CODE_HOOK the code segment. A type with neither
 * flag would address something other than Doom, and this returns 0 for it so a caller
 * cannot register a patch outside Doom's memory by accident.
 */
uint32_t mtx_hook_patch_addr(uint32_t addr, uint32_t type,
                             uint32_t code_seg, uint32_t data_seg);

typedef int (*mtx_hook_add_fn)(void *ctx, void *addr, uint32_t len);

/* Returned instead of a count when `add` refused an entry. An explicit sentinel
   rather than something like "return ~n+1": that encoding collides with 0 when the
   refusal happens at the first entry, i.e. exactly when a caller most needs to
   notice. A truncated registry isolates some patches and not others, which is worse
   than isolating none, because it looks like it works. */
#define MTX_HOOKS_SCAN_REFUSED  0xFFFFFFFFu

/*
 * Walk a hook table and hand every Doom-memory patch to `add`. Stops at an entry
 * whose addr is 0, exactly as the engine's own installer does, and returns how many
 * patches it registered - or MTX_HOOKS_SCAN_REFUSED if `add` rejected one, so a
 * registry that is full is reported rather than silently truncating the isolation.
 */
uint32_t mtx_hooks_scan(const void *table, uint32_t count,
                        uint32_t code_seg, uint32_t data_seg,
                        mtx_hook_add_fn add, void *ctx);

#endif /* MATRYOSHKA_HOOKS_H */
