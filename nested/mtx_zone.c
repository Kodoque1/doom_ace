/*
 * matryoshka/mtx_zone.c
 *
 * Building and checking a nested depth's zone. No libc, no engine dependency.
 */
#include "mtx_zone.h"

uint32_t mtx_zone_overhead(void)
{
	return (uint32_t)sizeof(mtx_memzone_t);
}

void mtx_zone_build(void *mem, uint32_t size)
{
	mtx_memzone_t *z = (mtx_memzone_t *)mem;
	mtx_memblock_t *b;

	if(!mem || size <= (uint32_t)sizeof(mtx_memzone_t))
		return;

	/* The first (and only) block starts right after the zone header and spans
	   the rest of the region. This is the same construction Z_Init performs for
	   the root, minus the split-zone fixup: a depth's zone is one contiguous
	   arena slot, so there is no second block to stitch in. */
	b = (mtx_memblock_t *)((uint8_t *)mem + sizeof(mtx_memzone_t));

	z->size = (int32_t)size;
	z->blocklist.next = b;
	z->blocklist.prev = b;
	z->blocklist.user = (void **)z;
	z->blocklist.tag = MTX_PU_STATIC;
	z->blocklist.id = MTX_ZONEID;
	z->rover = b;

	b->prev = &z->blocklist;
	b->next = &z->blocklist;
	b->user = 0;
	b->tag = 0;
	b->id = 0;
	b->size = (int32_t)(size - (uint32_t)sizeof(mtx_memzone_t));
}

int mtx_zone_check(const void *mem)
{
	const mtx_memzone_t *z = (const mtx_memzone_t *)mem;
	const mtx_memblock_t *b;

	if(!mem)
		return 1;

	/* Header sanity: Doom's Z_Malloc reads blocklist.id, and Z_CheckHeap relies
	   on the sentinel's user being set (otherwise the first free block would look
	   like two consecutive free blocks). */
	if(z->blocklist.id != MTX_ZONEID)
		return 2;
	if((void *)z->blocklist.user != (void *)z)
		return 3;
	if(!z->blocklist.next || !z->blocklist.prev)
		return 4;

	/* The chain must be a ring. */
	if(z->blocklist.next->prev != &z->blocklist)
		return 5;
	if(z->blocklist.prev->next != &z->blocklist)
		return 6;

	for(b = z->blocklist.next; ; b = b->next)
	{
		if(!b)
			return 7;
		/* the last block is where Z_CheckHeap stops, so stop here too */
		if(b->next == &z->blocklist)
			break;
		if((const uint8_t *)b + b->size != (const uint8_t *)b->next)
			return 8;
		if(b->next->prev != b)
			return 9;
		if(!b->user && !b->next->user)
			return 10;
	}

	/* A zone with a header and nothing else would satisfy the loop above while
	   being useless, so require at least one usable byte of block. */
	if(z->blocklist.next->size <= 0)
		return 11;

	return 0;
}
