/*
 * matryoshka/dos_wire.c
 *
 * Concrete wiring between the matryoshka host and the ACE engine: tells the
 * state layer what to snapshot and hands each depth its own zone and screen.
 *
 * The externs below mirror the engine's own declarations (engine/doom_vars.h,
 * engine/utils.h and the linker script). They are repeated instead of included
 * so this file stays compilable on its own.
 */
#include "context.h"
#include "dos_state.h"
#include "dos_wire.h"
#include "mtx_host.h"
#include "mtx_hooks.h"

/* Extent of Doom's mutable data, measured from engine/doom_vars.S:
   416 variables, lowest 0x00001A84 (finetangent), highest 0x00075CA0
   (volume_val). The section grows to the highest address, so 0x75CA0 is the
   whole region as far as ACE is concerned. */
#define DOOM_VAR_LO  0x00001A84u
#define DOOM_VAR_HI  0x00075CA0u
#define DOOM_REGION  (DOOM_VAR_HI - DOOM_VAR_LO)	/* 475676 bytes */

/* Doom's OWN screen pointer - `screens[0]`, the variable the renderer writes
   through. ACE reads it exactly once at init into its `screen_buffer` (main.c's
   hook list: 0x00074FC4 -> screen_buffer), so pinning ACE's copy alone still
   leaves Doom's renderer drawing into the ROOT's frame. Both are pinned now.
   The address is ACE's own, not a guess: it sits between `usegamma` (0x74FC0) and
   `mainzone` (0x74FE0), where vanilla Doom keeps `byte *screens[5]`. Nothing in
   the engine ever reassigns it (Doom allocates the screen once at startup), which
   is what makes forcing a depth's value on every switch-in safe. */
#define DOOM_SCREEN_VAR 0x00074FC4u

extern uint32_t doom_data_segment;	/* engine/utils.h     */
extern uint32_t doom_code_segment;	/* engine/utils.h     */
extern void    *mainzone;		/* engine/doom_vars.h */
extern uint8_t *screen_buffer;		/* engine/doom_vars.h */

extern uint8_t _bss_start[];		/* engine/linker.ld   */
extern uint8_t _bss_end[];
extern uint8_t _data_start[];		/* engine/linker.ld   */
extern uint8_t _data_end[];
extern uint8_t _hooks_start[];		/* engine/linker.ld   */
extern uint8_t _hooks_end[];

/*
 * Register the engine's writable .bss for per-depth snapshotting, minus the
 * host's own control state.
 *
 * The exclusion is the whole point. A snapshot restore copies a slot over the
 * live .bss, and mtx_hs lives in that .bss; if it were covered, the restore
 * would overwrite mtx_cur / mtx_ctx[].esp / inited[] in the middle of
 * mtx_switch - the scheduler would then resume on a stale (zero) ESP and jump
 * into unmapped memory. See mtx_host.h.
 *
 * mtx_hs is a single object, so splitting the region at its bounds is exact:
 * the linker may put other .bss data before or after it, never inside.
 * Refuses (returns -1) rather than snapshot the scheduler if that ever stops
 * holding - failing to nest beats corrupting the root instance.
 */
static int wire_engine_bss(void)
{
	uintptr_t b0 = (uintptr_t)(void *)_bss_start;
	uintptr_t b1 = (uintptr_t)(void *)_bss_end;
	uintptr_t h0 = (uintptr_t)(void *)&mtx_hs;
	uintptr_t h1 = h0 + (uintptr_t)sizeof(mtx_hs);

	if(h0 >= h1 || h0 < b0 || h1 > b1)
		return -1;

	/* .bss below the host block */
	if(h0 > b0 && mtx_dos_add_region((void *)b0, (uint32_t)(h0 - b0)) != 0)
		return -1;
	/* .bss above it */
	if(h1 < b1 && mtx_dos_add_region((void *)h1, (uint32_t)(b1 - h1)) != 0)
		return -1;

	return 0;
}

/*
 * Register the engine's .data - initialised writable data, which holds real state
 * (`mod_config` among it). .data is a section of its own, so mtx_hs (a .bss object)
 * cannot be inside it; checked anyway, because registering a region that covered the
 * host's own control state would reintroduce the wild jump the .bss split exists to
 * prevent, and "cannot happen" is exactly the kind of claim that stops being true.
 */
static int wire_engine_data(void)
{
	uintptr_t d0 = (uintptr_t)(void *)_data_start;
	uintptr_t d1 = (uintptr_t)(void *)_data_end;
	uintptr_t h0 = (uintptr_t)(void *)&mtx_hs;
	uintptr_t h1 = h0 + (uintptr_t)sizeof(mtx_hs);

	if(d1 <= d0)
		return 0;			/* empty section: nothing to snapshot */
	if(h0 < d1 && h1 > d0)
		return -1;			/* host state inside .data: refuse */
	return mtx_dos_add_region((void *)d0, (uint32_t)(d1 - d0));
}

static int wire_hook_add(void *ctx, void *addr, uint32_t len)
{
	(void)ctx;
	return mtx_dos_add_hook(addr, len);
}

/*
 * Register every patch ACE installed in Doom's memory, by walking the engine's own
 * hook table. Measured from the built engine: 419 entries, 405 of which modify Doom's
 * memory, 399 of them in Doom's CODE segment - which no snapshot region covers. Those
 * 399 are the reason this exists: without it, a nested instance re-patching Doom's
 * code at run time is visible to every other depth, root included.
 *
 * A hand-written list would rot the first time the engine gains a hook, and the
 * failure would look like memory corruption rather than a missing entry, so the table
 * is read instead. Returns the number registered, or 0 if the registry filled up.
 */
static uint32_t wire_engine_hooks(void)
{
	uint32_t n = (uint32_t)(_hooks_end - _hooks_start) / (uint32_t)sizeof(mtx_hook_t);
	uint32_t got;

	if(!n)
		return 0;

	got = mtx_hooks_scan(_hooks_start, n, doom_code_segment, doom_data_segment,
	                     wire_hook_add, 0);
	/* Fail closed if the registry could not take them all: registering some
	   patches and silently dropping the rest would isolate a depth from some of
	   Doom's code and not the rest, which is harder to diagnose than refusing to
	   nest at all. */
	if(got == MTX_HOOKS_SCAN_REFUSED || got == 0 || got > n)
		return 0;
	return got;
}

uint32_t mtx_dos_wire_register(void)
{
	/* 1. Doom's mutable data: everything doom_vars.S addresses. */
	if(mtx_dos_add_region((void *)(doom_data_segment + DOOM_VAR_LO), DOOM_REGION) != 0)
		return 0;

	/* 2. The engine's own writable state. .bss split around mtx_hs, and .data, which
	   holds mod_config among other things and is a separate section entirely. */
	if(wire_engine_bss() != 0)
		return 0;
	if(wire_engine_data() != 0)
		return 0;

	/* 3. Every patch ACE installed in Doom's memory. The CODE-segment ones are not
	   covered by any region above, so this is the only thing that isolates them. */
	if(!wire_engine_hooks())
		return 0;

	/* 4. The pointer globals each depth must own: its heap, ACE's screen, and
	   Doom's OWN screen pointer. The last one is the one that matters for video:
	   Doom's renderer writes through `screens[0]`, not through ACE's copy. */
	mtx_dos_set_pin(&mainzone, MTX_PIN_MAINZONE);
	mtx_dos_set_pin(&screen_buffer, MTX_PIN_SCREEN);
	mtx_dos_set_pin((void *)(doom_data_segment + DOOM_SCREEN_VAR), MTX_PIN_DOOMSCREEN);

	return mtx_dos_snapshot_bytes();
}

void mtx_dos_wire_values(uint8_t max_depth, void **heaps, void **screens)
{
	uint8_t d;

	for(d = 0; d < max_depth; d++)
	{
		mtx_dos_set_value(d, MTX_PIN_MAINZONE, heaps[d]);
		mtx_dos_set_value(d, MTX_PIN_SCREEN, screens[d]);
		mtx_dos_set_value(d, MTX_PIN_DOOMSCREEN, screens[d]);
	}
}

void mtx_dos_wire(uint8_t max_depth,
                  void *snapshot_mem, uint32_t snapshot_per_depth,
                  void **heaps, void **screens)
{
	mtx_dos_init(max_depth, snapshot_mem, snapshot_per_depth);
	mtx_dos_wire_register();
	mtx_dos_wire_values(max_depth, heaps, screens);
}

void mtx_dos_wire_hook(void *addr, uint32_t len)
{
	mtx_dos_add_hook(addr, len);
}
