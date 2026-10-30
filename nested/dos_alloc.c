/*
 * matryoshka/dos_alloc.c - arena, budget planner and one-call boot.
 * Engine-agnostic: the registration step arrives as a callback.
 */
#include "dos_alloc.h"

/*
 * The arena's base is aligned ABSOLUTELY, not relative to what the caller
 * handed in.
 *
 * Both matter and only one is obvious. `mtx_arena_alloc` aligns each offset
 * relative to the base, so it only produces aligned pointers if the BASE is
 * aligned - and it is not: the arena comes from Doom's own allocator, which is
 * happy to return an address that is 4-byte aligned but not 8. Measured: the
 * raw pool carved out of such an arena had a base 4 bytes off, every payload
 * inheriting that, and the pool's own ownership test (which requires aligned
 * payloads) then rejected all of its own memory - so an instance's realloc and
 * free fell through to the DPMI heap, which knew nothing about those pointers.
 * Rounding the base up costs at most 31 bytes.
 */
void mtx_arena_init(mtx_arena_t *a, void *mem, uint32_t size)
{
	uintptr_t b = (uintptr_t)mem;
	uintptr_t aligned = (b + 31u) & ~(uintptr_t)31u;
	uint32_t skip = (uint32_t)(aligned - b);

	a->base = mem ? (uint8_t *)aligned : 0;
	a->size = (mem && size > skip) ? (size - skip) : 0;
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
                      uint32_t screen, uint32_t stack, uint32_t image,
                      uint32_t raw)
{
	mtx_budget_t b;

	b.snapshot  = snapshot;
	b.zone      = zone;
	b.screen    = screen;
	b.stack     = stack;
	b.image     = image;
	b.raw       = raw;
	b.per_depth = snapshot + zone + screen + stack + image + raw;
	b.depths_fit = b.per_depth ? (arena_bytes / b.per_depth) : 0;

	b.depth = (b.depths_fit > MTX_MAX_DEPTH) ? MTX_MAX_DEPTH : (uint8_t)b.depths_fit;
	if(b.depths_fit > 255)
		b.depths_fit = 255;
	return b;
}

int mtx_dos_boot(void *arena_mem, uint32_t arena_bytes, uint32_t zone_per_depth,
                 uint32_t image_per_depth, uint32_t raw_per_depth,
                 mtx_wire_fn wire_register,
                 void **heaps, void **screens, void **images, mtx_raw_t *raws,
                 mtx_boot_t *out)
{
	mtx_arena_t a;
	uint32_t snap, i;
	mtx_budget_t b;

	if(!arena_mem || !wire_register || !heaps || !screens || !out)
		return -1;

	/* Phase 1: register against no pool, purely to learn the snapshot size. */
	mtx_dos_init(MTX_MAX_DEPTH, 0, 0);
	snap = wire_register();
	if(!snap)
		return -1;			/* wiring refused (see dos_wire.c) */

	b = mtx_plan(arena_bytes, snap, zone_per_depth, MTX_SCREEN_BYTES, MTX_STACK_SIZE,
	             image_per_depth, raw_per_depth);
	out->budget = b;
	if(b.depth < 2)
		return -1;			/* two is the minimum that nests */

	/* Phase 2: carve. Depths past b.depth stay NULL and are never reachable -
	   mtx_push refuses beyond mtx_max_depth anyway.
	   Depth 0 is special: if the caller pre-filled heaps[0]/screens[0] they are
	   KEPT, because the root must go on using its own live zone and framebuffer.
	   Only the children get freshly carved ones. Every depth, root included,
	   gets an image slot: a depth that wants to run code of its own needs
	   somewhere of its own to put it.
	   Raw pools are CHILDREN ONLY (the root allocates from the DPMI heap it
	   already owns), and every slot is cleared first so a pool from an earlier
	   boot cannot be mistaken for one of this boot's. */
	mtx_arena_init(&a, arena_mem, arena_bytes);
	for(i = 0; i < MTX_MAX_DEPTH; i++)
		if(raws)
			mtx_raw_init(&raws[i], 0, 0);

	for(i = 0; i < b.depth; i++)
	{
		if(!(i == 0 && heaps[i]))
			heaps[i] = mtx_arena_alloc(&a, zone_per_depth, 16);
		if(!(i == 0 && screens[i]))
			screens[i] = mtx_arena_alloc(&a, MTX_SCREEN_BYTES, 16);
		if(images)
		{
			images[i] = image_per_depth
			          ? mtx_arena_alloc(&a, image_per_depth, 32) : 0;
			if(image_per_depth && !images[i])
				return -1;
		}
		if(raws && raw_per_depth && i != 0)
			mtx_raw_init(&raws[i], mtx_arena_alloc(&a, raw_per_depth, 32),
			             raw_per_depth);
		if(!heaps[i] || !screens[i])
			return -1;
		if(raws && raw_per_depth && i != 0 && !raws[i].base)
			return -1;		/* a child MUST have its raw pool */
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
		/* Doom's own screen pointer gets the same region: at a depth the RENDERER
		   must draw into that depth's memory, not into the root's frame. */
		mtx_dos_set_value((uint8_t)i, MTX_PIN_DOOMSCREEN, screens[i]);
	}

	out->depth = b.depth;
	return 0;
}
