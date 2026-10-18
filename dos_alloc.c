/*
 * matryoshka/dos_alloc.c - arena, budget planner and one-call boot.
 * Engine-agnostic: the registration step arrives as a callback.
 */
#include "dos_alloc.h"

void mtx_arena_init(mtx_arena_t *a, void *mem, uint32_t size)
{
	a->base = (uint8_t *)mem;
	a->size = size;
	a->used = 0;
}

void *mtx_arena_alloc(mtx_arena_t *a, uint32_t size, uint32_t align)
{
	uint32_t off;

	if(!a || !a->base || !size)
		return 0;
	if(align < 1)
		align = 1;

	off = (a->used + (align - 1)) & ~(align - 1);
	if(off > a->size || size > a->size - off)
		return 0;			/* would overflow the arena */

	a->used = off + size;
	return a->base + off;
}

uint32_t mtx_arena_used(const mtx_arena_t *a) { return a->used; }

mtx_budget_t mtx_plan(uint32_t arena_bytes, uint32_t snapshot, uint32_t zone,
                      uint32_t screen, uint32_t stack)
{
	mtx_budget_t b;

	b.snapshot  = snapshot;
	b.zone      = zone;
	b.screen    = screen;
	b.stack     = stack;
	b.per_depth = snapshot + zone + screen + stack;
	b.depths_fit = b.per_depth ? (arena_bytes / b.per_depth) : 0;

	b.depth = (b.depths_fit > MTX_MAX_DEPTH) ? MTX_MAX_DEPTH : (uint8_t)b.depths_fit;
	if(b.depths_fit > 255)
		b.depths_fit = 255;
	return b;
}

int mtx_dos_boot(void *arena_mem, uint32_t arena_bytes, uint32_t zone_per_depth,
                 mtx_wire_fn wire_register,
                 void **heaps, void **screens, mtx_boot_t *out)
{
	mtx_arena_t a;
	uint32_t snap, i;
	mtx_budget_t b;

	if(!arena_mem || !wire_register || !heaps || !screens || !out)
		return -1;

	/* Phase 1: register against no pool, purely to learn the snapshot size. */
	mtx_dos_init(MTX_MAX_DEPTH, 0, 0);
	snap = wire_register();

	b = mtx_plan(arena_bytes, snap, zone_per_depth, MTX_SCREEN_BYTES, MTX_STACK_SIZE);
	out->budget = b;
	if(b.depth < 2)
		return -1;			/* two is the minimum that nests */

	/* Phase 2: carve. Depths past b.depth stay NULL and are never reachable -
	   mtx_push refuses beyond mtx_max_depth anyway. */
	mtx_arena_init(&a, arena_mem, arena_bytes);
	for(i = 0; i < b.depth; i++)
	{
		heaps[i]   = mtx_arena_alloc(&a, zone_per_depth, 16);
		screens[i] = mtx_arena_alloc(&a, MTX_SCREEN_BYTES, 16);
		if(!heaps[i] || !screens[i])
			return -1;
	}
	out->stack_base    = mtx_arena_alloc(&a, (uint32_t)b.depth * MTX_STACK_SIZE, 32);
	out->snapshot_base = mtx_arena_alloc(&a, (uint32_t)b.depth * snap, 32);
	out->stack_bytes       = (uint32_t)b.depth * MTX_STACK_SIZE;
	out->snapshot_per_depth = snap;
	if(!out->stack_base || !out->snapshot_base)
		return -1;

	/* Phase 3: real init against the real pool, re-register, hand out values. */
	mtx_dos_init(b.depth, out->snapshot_base, snap);
	wire_register();
	for(i = 0; i < b.depth; i++)
	{
		mtx_dos_set_value((uint8_t)i, MTX_PIN_MAINZONE, heaps[i]);
		mtx_dos_set_value((uint8_t)i, MTX_PIN_SCREEN, screens[i]);
	}

	out->depth = b.depth;
	return 0;
}
