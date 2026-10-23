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
extern void    *mainzone;		/* engine/doom_vars.h */
extern uint8_t *screen_buffer;		/* engine/doom_vars.h */

extern uint8_t _bss_start[];		/* engine/linker.ld   */
extern uint8_t _bss_end[];

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

uint32_t mtx_dos_wire_register(void)
{
	/* 1. Doom's mutable data: everything doom_vars.S addresses. */
	mtx_dos_add_region((void *)(doom_data_segment + DOOM_VAR_LO), DOOM_REGION);

	/* 2. The engine's own writable state, with mtx_hs carved out. .data has no
	   linker symbols yet, so it is deliberately left out here - it needs a
	   PROVIDE(_data_start/_data_end) pair in engine/linker.ld. When that
	   happens it must be split around mtx_hs in exactly the same way, because
	   the host state must never be snapshot. */
	if(wire_engine_bss() != 0)
	{
		/* Fail closed: 0 means "no usable snapshot", and mtx_dos_boot turns
		   that into a refusal to nest. */
		return 0;
	}

	/* 3. The pointer globals each depth must own: its heap, ACE's screen, and
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

void mtx_dos_wire_hook(void *addr)
{
	mtx_dos_add_hook(addr, 4);
}
