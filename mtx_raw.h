/*
 * matryoshka/mtx_raw.h
 *
 * A depth's RAW allocation pool - the memory an engine instance asks for
 * OUTSIDE its zone.
 *
 * WHY THIS EXISTS
 * ---------------
 * The zone was the first half: a depth builds its own heap in its arena slot,
 * so Doom's Z_Malloc walks that depth's memblock ring. Measured wall after
 * that (see README, "Entering the real engine image"): the engine still cannot
 * start an instance, because ACE's own `ace_main` asks the RAW allocator for
 * memory and the raw allocator is machine-global.
 *
 *     [MTX] free DPMI after root Z_Init: 61440 B
 *     LOADER: Render memory allocation failed! (209184B)
 *
 * The root's Z_Init walks dpmi_get_ram() down until an allocation succeeds, so
 * it eats the whole DPMI heap but a 32 KiB reserve. Raising the arena does not
 * help - the root consumes whatever is left either way - so a depth needs a
 * pool of its own that is NOT the DPMI heap.
 *
 * WHAT IT IS, HONESTLY
 * --------------------
 * A bump allocator. It never compacts, and a free only returns memory when the
 * block being freed is the newest one (the tip): loaders allocate a buffer,
 * fill it, hand it on and free it, so the tip case is the common one and it does
 * reclaim. A free of anything older is COUNTED, not pretended away - that count
 * (`leaked` / `leaked_bytes`) is reported, so "the pool is big enough" is a
 * measurement rather than an assumption. This is deliberate: the alternative is
 * a real free list, which is a bigger change than the thing it would enable, and
 * nothing here needs to survive a level change yet.
 *
 * Every block carries an 8-byte header, which buys three things: the payload is
 * 8-aligned, free()/realloc() can find the size without being told it, and a
 * pointer that did not come from THIS pool is detectable rather than silently
 * corrupting a heap. That last one is what lets the engine decide free/realloc
 * by the pointer's PROVENANCE (the pool's range plus the block magic) instead of
 * by the current depth.
 */
#ifndef MATRYOSHKA_RAW_H
#define MATRYOSHKA_RAW_H

#include <stdint.h>

#define MTX_RAW_MAGIC       0x4D545852u	/* 'MTXR', block in use */
#define MTX_RAW_DEAD_MAGIC  0x4D545846u	/* 'MTXF', block reclaimed */
#define MTX_RAW_ALIGN       8u
#define MTX_RAW_HDR         8u		/* size, magic -> payload stays 8-aligned */

typedef struct {
	uint8_t *base;
	uint32_t size;		/* pool capacity, bytes */
	uint32_t used;		/* the bump pointer */
	uint32_t peak;		/* high-water mark of `used` */
	uint32_t served;	/* allocations answered */
	uint32_t refusals;	/* allocations the pool could not answer */
	uint32_t want_bytes;	/* the largest of those - the sizing number */
	uint32_t reclaimed;	/* frees that gave the tip back to the pool */
	uint32_t leaked;	/* frees that could not (older than the tip) */
	uint32_t leaked_bytes;
	uint32_t invalid;	/* free/realloc of a pointer that is not ours */
} mtx_raw_t;

void     mtx_raw_init(mtx_raw_t *p, void *mem, uint32_t size);
void    *mtx_raw_alloc(mtx_raw_t *p, uint32_t size);
void    *mtx_raw_realloc(mtx_raw_t *p, void *ptr, uint32_t size);
void     mtx_raw_free(mtx_raw_t *p, void *ptr);

/* Does `ptr` come from THIS pool? Range AND block magic, so a stale or foreign
   pointer is refused instead of being walked as a block. */
int      mtx_raw_owns(const mtx_raw_t *p, const void *ptr);

uint32_t mtx_raw_used(const mtx_raw_t *p);
uint32_t mtx_raw_avail(const mtx_raw_t *p);

#endif /* MATRYOSHKA_RAW_H */
