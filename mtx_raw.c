/*
 * matryoshka/mtx_raw.c - the per-depth raw pool (see mtx_raw.h).
 *
 * No libc: this is linked into ACE, so every copy is a hand-written loop. The
 * optimize attribute stops gcc turning one of them into a memset/memcpy call.
 */
#include "mtx_raw.h"

static uint32_t raw_align8(uint32_t v)
{
	return (v + (MTX_RAW_ALIGN - 1u)) & ~(MTX_RAW_ALIGN - 1u);
}

/*
 * HOW MUCH A BLOCK OF `size` OCCUPIES - one function, because getting this wrong
 * in one place and right in another is how the two bugs below happened.
 *
 *  - A zero-size block still occupies a minimum. Its payload would otherwise sit
 *    exactly at the pool's bump, so a caller that writes even one byte past a
 *    0-byte request lands in the NEXT block's header - and the next owner then
 *    reads a size that is really the block magic. Measured live: a nested
 *    instance's Doom asked for lump 19796, which is 0x4D54, the top half of the
 *    word 'MTXR' from a raw-pool header. Doom's own malloc(0) has usable space
 *    for exactly this reason.
 *  - The tip test (is this block the newest one?) has to use the OCCUPANCY, not
 *    the requested size. A 1-byte request occupies 8, so comparing against 1
 *    never matched, and every free or realloc of such a block went down the
 *    "not the tip" path: memory leaked and growth moved blocks for no reason.
 */
static uint32_t raw_need(uint32_t size)
{
	uint32_t a = raw_align8(size);

	if(!a)
		a = MTX_RAW_ALIGN;
	return a + MTX_RAW_HDR;
}

/* the header just before a payload: [size][magic] */
static uint32_t *raw_size_of(void *ptr)  { return (uint32_t *)ptr - 2; }
static uint32_t *raw_magic_of(void *ptr) { return (uint32_t *)ptr - 1; }

/*
 * The pool's base is aligned ABSOLUTELY here, and the pool works for any base a
 * caller hands it as a result.
 *
 * It has to: the pool's block format assumes payloads are aligned, and the
 * ownership test refuses a pointer that is not - which is what makes free() safe
 * against a foreign pointer. So if the base wandered, every payload wandered with
 * it and the pool would reject its own memory. Measured in DOSBox: an arena base
 * that is only 4-byte aligned put the pool 4 bytes off, `mtx_raw_owns` answered
 * "not mine" for everything, and an instance's realloc fell through to the DPMI
 * heap - which returned the pointer untouched, so a 58 KiB lump table quietly
 * stopped growing. Aligning here means no caller can get that wrong.
 */
void mtx_raw_init(mtx_raw_t *p, void *mem, uint32_t size)
{
	uintptr_t b = (uintptr_t)mem;
	uintptr_t aligned = (b + (MTX_RAW_ALIGN - 1u)) & ~(uintptr_t)(MTX_RAW_ALIGN - 1u);
	uint32_t skip = (uint32_t)(aligned - b);

	if(!p)
		return;

	p->base = mem ? (uint8_t *)aligned : 0;
	p->size = (mem && size > skip) ? (size - skip) : 0;
	p->used = 0;
	p->peak = 0;
	p->served = 0;
	p->refusals = 0;
	p->want_bytes = 0;
	p->reclaimed = 0;
	p->leaked = 0;
	p->leaked_bytes = 0;
	p->invalid = 0;
}

__attribute__((optimize("no-tree-loop-distribute-patterns")))
static void raw_copy(uint8_t *dst, const uint8_t *src, uint32_t n)
{
	uint32_t i;

	for(i = 0; i < n; i++)
		dst[i] = src[i];
}

/*
 * A ZERO-byte request is SERVED, not refused - a block of just its header.
 * Doom's own malloc(0) returns a valid pointer, and the engine really does ask
 * for zero: measured live, a nested instance's texture loader asked for 0 B and
 * refusing it turned a working call into "Texture memory allocation failed! (0B)"
 * and a bluescreen. A pool that is stricter than the allocator it replaces is a
 * pool that breaks its callers.
 */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *mtx_raw_alloc(mtx_raw_t *p, uint32_t size)
{
	uint32_t need, off;
	uint8_t *blk;

	if(!p || !p->base)
	{
		if(p)
		{
			p->refusals++;
			if(size > p->want_bytes)
				p->want_bytes = size;
		}
		return 0;
	}

	need = raw_need(size);
	if(need < size)			/* overflow: the aligned size wrapped */
	{
		p->refusals++;
		if(size > p->want_bytes)
			p->want_bytes = size;
		return 0;
	}

	off = p->used;
	if(off > p->size || need > p->size - off)
	{
		/* Exhaustion is a REPORTED event, not a NULL the caller has to
		   interpret: the requested size is what says whether the pool is
		   too small or the code is asking for something absurd. */
		p->refusals++;
		if(size > p->want_bytes)
			p->want_bytes = size;
		return 0;
	}

	blk = p->base + off;
	*(uint32_t *)blk = size;
	*(uint32_t *)(blk + 4) = MTX_RAW_MAGIC;

	p->used = off + need;
	if(p->used > p->peak)
		p->peak = p->used;
	p->served++;

	return blk + MTX_RAW_HDR;
}

int mtx_raw_owns(const mtx_raw_t *p, const void *ptr)
{
	const uint8_t *q = (const uint8_t *)ptr;

	if(!p || !p->base || !ptr)
		return 0;
	/* Range first: a pointer outside the pool must not be dereferenced at all.
	   The upper bound is inclusive because a ZERO-size block's payload sits
	   exactly at the end of the used region - rejecting that would make the
	   pool disown the one block it had just handed out. */
	if(q < p->base + MTX_RAW_HDR || q > p->base + p->used)
		return 0;
	if((uintptr_t)q & (MTX_RAW_ALIGN - 1u))
		return 0;
	if(*((const uint32_t *)q - 1) != MTX_RAW_MAGIC)
		return 0;
	/* the block's own size must also land inside the used region, so a stray
	   word that happens to read MTXR cannot send us off the end */
	{
		const uint32_t sz = *((const uint32_t *)q - 2);
		if(q + sz > p->base + p->used)
			return 0;
	}
	return 1;
}

void mtx_raw_free(mtx_raw_t *p, void *ptr)
{
	uint32_t *sz, *magic;
	uint32_t blk_off;

	if(!ptr || !p || !p->base)
		return;

	if(!mtx_raw_owns(p, ptr))
	{
		/* Not ours. Refusing is the point: freeing a foreign pointer into
		   this pool is how a heap gets corrupted silently. */
		if(p)
			p->invalid++;
		return;
	}

	sz = raw_size_of(ptr);
	magic = raw_magic_of(ptr);
	blk_off = (uint32_t)((uint8_t *)sz - p->base);

	*magic = MTX_RAW_DEAD_MAGIC;

	if(blk_off + raw_need(*sz) == p->used)
	{
		/* the tip: this really does return memory */
		p->used = blk_off;
		p->reclaimed++;
		return;
	}

	/* older than the tip - counted, not pretended */
	p->leaked++;
	p->leaked_bytes += *sz;
}

void *mtx_raw_realloc(mtx_raw_t *p, void *ptr, uint32_t size)
{
	uint32_t *sz, *magic;
	uint32_t blk_off, old, need;
	uint8_t *fresh;

	if(!ptr)
		return mtx_raw_alloc(p, size);

	if(!mtx_raw_owns(p, ptr))
	{
		p->invalid++;
		return 0;
	}

	sz = raw_size_of(ptr);
	magic = raw_magic_of(ptr);
	old = *sz;
	blk_off = (uint32_t)((uint8_t *)sz - p->base);

	need = raw_need(size);
	if(need < size)
	{
		p->refusals++;
		if(size > p->want_bytes)
			p->want_bytes = size;
		return 0;
	}

	/* In place when it is the tip block and the pool has room: a loader that
	   grows the newest buffer should not pay for a second copy of it. */
	if(blk_off + raw_need(old) == p->used)
	{
		if(blk_off > p->size || need > p->size - blk_off)
		{
			p->refusals++;
			if(size > p->want_bytes)
				p->want_bytes = size;
			return 0;
		}
		p->used = blk_off + need;
		if(p->used > p->peak)
			p->peak = p->used;
		*sz = size;
		*magic = MTX_RAW_MAGIC;
		return ptr;
	}

	/* Otherwise: allocate, copy what both sizes have in common, drop the old.
	   The old one only becomes reusable if it happens to be the tip, which is
	   why the leak counters exist - they are how "is the pool big enough"
	   gets answered with a number instead of a hopeful guess. */
	fresh = (uint8_t *)mtx_raw_alloc(p, size);
	if(!fresh)
		return 0;

	raw_copy(fresh, (uint8_t *)ptr, (size < old) ? size : old);
	mtx_raw_free(p, ptr);

	return fresh;
}

uint32_t mtx_raw_used(const mtx_raw_t *p)  { return p ? p->used : 0; }
uint32_t mtx_raw_avail(const mtx_raw_t *p) { return (p && p->size >= p->used) ? p->size - p->used : 0; }
