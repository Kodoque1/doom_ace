/*
 * matryoshka/mtx_zone.h
 *
 * A nested depth's OWN zone.
 *
 * WHY THIS FILE EXISTS
 * --------------------
 * The root instance's Z_Init walks DPMI RAM downwards until malloc succeeds, so
 * it consumes everything except a 32 KiB reserve. A depth cannot do that: the
 * memory is already gone, and a depth must not be able to take memory away from
 * the root anyway. What a depth CAN have is a zone of its own inside the arena
 * slot the planner reserved for it - and a zone is not just "some memory": Doom's
 * allocator walks a memzone_t header and a doubly-linked chain of memblock_t
 * blocks. Hand a depth raw arena memory and the first Z_Malloc reads an
 * uninitialised block header.
 *
 * So this is the tail of Z_Init, extracted and made engine-agnostic:
 *
 *   mtx_zone_build(mem, size)   lay out a one-block zone exactly as Doom expects
 *   mtx_zone_check(mem)         walk it with Doom's own heap checks, as a value
 *
 * The layout is MIRRORED from engine.h (memzone_t/memblock_t), in the same spirit
 * as ldr1n.h mirroring lumpinfo_t: this file has to build on the host, where the
 * engine's headers are not available. The engine build asserts the two sizes
 * match, so a drift in engine.h is a compile error rather than a runtime crash.
 *
 * A depth's zone memory is NOT part of its snapshot, and does not need to be:
 * snapshots cover the shared code/data segment, while each depth's zone lives at
 * its own address in the arena. Different depths therefore keep different heaps
 * for free, which is exactly what a nested instance wants.
 */
#ifndef MATRYOSHKA_ZONE_H
#define MATRYOSHKA_ZONE_H

#include <stdint.h>

/* Doom's heap constants, mirrored (engine.h: ZONEID / PU_STATIC). */
#define MTX_ZONEID    0x1D4A11
#define MTX_PU_STATIC 1

typedef struct mtx_memblock_s
{
	int32_t size;
	void **user;
	int32_t tag;
	int32_t id;
	struct mtx_memblock_s *next;
	struct mtx_memblock_s *prev;
} mtx_memblock_t;

typedef struct
{
	int32_t size;
	mtx_memblock_t blocklist;
	mtx_memblock_t *rover;
} mtx_memzone_t;

/* Bytes of header a zone needs before it can hold anything. A zone must be
   larger than this or it has no room for a single block. */
uint32_t mtx_zone_overhead(void);

/* Build a self-contained one-block zone spanning `size` bytes at `mem`. A size
   of mtx_zone_overhead() or less is refused: nothing is written, because there
   would be no room for even one block. */
void mtx_zone_build(void *mem, uint32_t size);

/* Walk the zone exactly as Doom's Z_CheckHeap (and the engine's zone_info) does.
   Returns 0 when intact, or a non-zero code naming the invariant that failed -
   so a caller can report WHICH check broke instead of just "bad zone". */
int mtx_zone_check(const void *mem);

#endif /* MATRYOSHKA_ZONE_H */
