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

/* Extent of Doom's mutable data, measured from engine/doom_vars.S:
   416 variables, lowest 0x00001A84 (finetangent), highest 0x00075CA0
   (volume_val). The section grows to the highest address, so 0x75CA0 is the
   whole region as far as ACE is concerned. */
#define DOOM_VAR_LO  0x00001A84u
#define DOOM_VAR_HI  0x00075CA0u
#define DOOM_REGION  (DOOM_VAR_HI - DOOM_VAR_LO)	/* 475676 bytes */

extern uint32_t doom_data_segment;	/* engine/utils.h     */
extern void    *mainzone;		/* engine/doom_vars.h */
extern uint8_t *screen_buffer;		/* engine/doom_vars.h */

extern uint8_t _bss_start[];		/* engine/linker.ld   */
extern uint8_t _bss_end[];

uint32_t mtx_dos_wire_register(void)
{
	/* 1. Doom's mutable data: everything doom_vars.S addresses. */
	mtx_dos_add_region((void *)(doom_data_segment + DOOM_VAR_LO), DOOM_REGION);

	/* 2. The engine's own writable state. .bss has linker symbols; .data does
	   not, so it is deliberately left out here - it needs a
	   PROVIDE(_data_start/_data_end) pair in engine/linker.ld to be included.
	   Do not forget it: mod_config and friends live there. */
	mtx_dos_add_region(_bss_start, (uint32_t)(_bss_end - _bss_start));

	/* 3. The two pointer globals each depth must own. */
	mtx_dos_set_pin(&mainzone, MTX_PIN_MAINZONE);
	mtx_dos_set_pin(&screen_buffer, MTX_PIN_SCREEN);

	return mtx_dos_snapshot_bytes();
}

void mtx_dos_wire_values(uint8_t max_depth, void **heaps, void **screens)
{
	uint8_t d;

	for(d = 0; d < max_depth; d++)
	{
		mtx_dos_set_value(d, MTX_PIN_MAINZONE, heaps[d]);
		mtx_dos_set_value(d, MTX_PIN_SCREEN, screens[d]);
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
