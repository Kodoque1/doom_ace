/*
 * matryoshka/dos_state.c
 *
 * Implements the save_state/load_state pair described in dos_state.h. No libc,
 * no engine dependency: the wiring layer registers whatever has to move.
 */
#include "dos_state.h"
#include "mtx_host.h"

#ifndef MTX_DOS_STATIC_PER_DEPTH
#define MTX_DOS_STATIC_PER_DEPTH (640u * 1024u)	/* host pool: 640 KiB/depth */
#endif

/* The registry is control state, so it lives in mtx_hs and is excluded from the
   very snapshots it describes (see mtx_host.h). mtx_dregion_t/mtx_dhook_t are
   declared there too. */
#define rgn       (mtx_hs.rgn)
#define hok       (mtx_hs.hok)
#define nrgn      (mtx_hs.nrgn)
#define nhok      (mtx_hs.nhok)
#define pin_addr  (mtx_hs.pin_addr)
#define pin_val   (mtx_hs.pin_val)
#define pool      (mtx_hs.snap_pool)
#define per_depth (mtx_hs.snap_per_depth)
#define inited    (mtx_hs.inited)
#define maxd      (mtx_hs.maxd)

#ifndef MTX_DOS_NO_STATIC_POOL
static uint8_t static_pool[MTX_MAX_DEPTH * MTX_DOS_STATIC_PER_DEPTH];
#endif

/* libc-free copy; guarded so gcc cannot turn it back into memcpy() */
static void __attribute__((optimize("no-tree-loop-distribute-patterns")))
mtx_bcopy(void *dst, const void *src, uint32_t n)
{
	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;
	while(n--)
		*d++ = *s++;
}

void mtx_dos_init(uint8_t max_depth, void *snapshot_mem, uint32_t per_depth_bytes)
{
	int i, j;

	maxd = (max_depth > MTX_MAX_DEPTH) ? MTX_MAX_DEPTH : max_depth;
	nrgn = 0;
	nhok = 0;

	for(i = 0; i < MTX_PIN_COUNT; i++)
		pin_addr[i] = 0;
	for(i = 0; i < MTX_MAX_DEPTH; i++)
	{
		inited[i] = 0;
		for(j = 0; j < MTX_PIN_COUNT; j++)
			pin_val[i][j] = 0;
	}

	if(snapshot_mem && per_depth_bytes)
	{
		pool      = (uint8_t *)snapshot_mem;
		per_depth = per_depth_bytes;
	}
#ifndef MTX_DOS_NO_STATIC_POOL
	else
	{
		pool      = static_pool;
		per_depth = MTX_DOS_STATIC_PER_DEPTH;
	}
#else
	else
	{
		pool      = 0;
		per_depth = 0;
	}
#endif
}

/* Regions first, then patched locations; order of registration does not matter
   because offsets are assigned here, not at add time. */
static uint32_t mtx_layout(void)
{
	uint32_t o = 0;
	int i;

	for(i = 0; i < nrgn; i++) { rgn[i].off = o; o += rgn[i].size; }
	for(i = 0; i < nhok; i++) { hok[i].off = o; o += hok[i].len; }
	return o;
}

uint32_t mtx_dos_snapshot_bytes(void) { return mtx_layout(); }

uint32_t mtx_dos_region_count(void) { return (uint32_t)nrgn; }
uint32_t mtx_dos_hook_count(void)   { return (uint32_t)nhok; }

int mtx_dos_add_region(void *base, uint32_t size)
{
	if(nrgn >= MTX_DOS_MAX_REGIONS || !base || !size)
		return -1;
	rgn[nrgn].base = base;
	rgn[nrgn].size = size;
	nrgn++;
	return 0;
}

int mtx_dos_add_hook(void *addr, uint32_t len)
{
	if(nhok >= MTX_DOS_MAX_HOOKS || !addr || !len || len > 16)
		return -1;
	hok[nhok].addr = addr;
	hok[nhok].len  = len;
	nhok++;
	return 0;
}

int mtx_dos_set_pin(void *var_addr, int pin)
{
	if(pin < 0 || pin >= MTX_PIN_COUNT || !var_addr)
		return -1;
	pin_addr[pin] = var_addr;
	return 0;
}

void mtx_dos_set_value(uint8_t depth, int pin, void *value)
{
	if(depth >= MTX_MAX_DEPTH || pin < 0 || pin >= MTX_PIN_COUNT)
		return;
	pin_val[depth][pin] = value;
}

static uint8_t *mtx_slot(uint8_t depth)
{
	if(!pool || depth >= maxd)
		return 0;
	return pool + (uint32_t)depth * per_depth;
}

/*
 * --- copyless: save and restore only the blocks that actually change -----------
 *
 * A switch used to copy every registered byte twice: the outgoing depth's whole
 * state out, the incoming depth's whole state in. Measured, that is ~1.08 MB per
 * switch and it costs real throughput at fine slice widths (14% of it going from
 * 30-frame slices to 4-frame ones, which is what the nested view's rate is traded
 * against).
 *
 * Most of those bytes are the same for every depth: Doom's data segment is full of
 * tables that are set up once and never written again, and the renderer's static
 * data. What differs per depth is a small part of it. So:
 *
 *   1. WARM-UP (MTX_WARMUP switches): copy everything, but COMPARE at the same time
 *      and record which 32-byte blocks ever differ. This is the scan that costs the
 *      time, and it is paid once.
 *   2. STEADY STATE: copy only the blocks known to be hot. The cold bytes are left
 *      alone in both directions - no depth writes them, so the single shared copy of
 *      the live region is already correct for all of them.
 *   3. CANARY (every MTX_CANARY switches): compare the COLD region while a depth is
 *      switched in, and PROMOTE any block that has changed. That is the safety net
 *      for "cold was measured, not proven": a block that turns out to be written is
 *      never silently shared, it just joins the hot set and takes the normal path
 *      from then on.
 *
 * The risk this leaves is real and is stated in the log rather than hidden: between
 * two canaries, a block that becomes hot can be shared by both depths. Promotion
 * happens at the next canary, and the counts are reported so the warm-up can be
 * judged instead of trusted (a cold fraction that is not small means the idea does
 * not apply to what the depth is doing, and the numbers say so).
 */
#define MTX_BLK      32u    /* bytes per tracked block: 8 pointers, fine enough */

/* The switch itself, kept so the two behaviours can be measured against each other
   rather than argued about: 1 = compare and copy only what differs, 0 = copy the
   whole registered set the way it always did. */
#define MTX_COPYLESS 1

/*
 * WHICH MODE, AND WHY IT IS THIS ONE
 *
 * The measured win is large: comparing every block on every switch shows that
 * 4,000 of 572,945 bytes (0.6%) actually differ, so copying only the differences
 * replaces ~1.1 MB of writes per switch with ~4 KB of them.
 *
 * The first version of this tried to CACHE which blocks are hot - full copies
 * during a warm-up, only the hot blocks afterwards, with a canary over the cold
 * region every so often. Measured, and it is wrong: a block can be cold for the
 * whole warm-up and still be written later, and the canary is far too late. A run
 * in that mode produced 1000 illegal reads from one wild pointer and a depth whose
 * frame stopped reaching the inset, with only 7 blocks promoted by the time the
 * damage was visible.
 *
 * So the hot set here is REPORTING ONLY. Every switch compares, and only the bytes
 * that differ are copied - correctness does not depend on any cached guess about
 * what a depth writes. The cost this pays is the comparison itself: ~573 KB read
 * twice instead of ~573 KB read and written once. Whether that is a win in an
 * emulator is an empirical question, and the throughput measurement is what
 * answers it (see the numbers in the commit and the README).
 *
 * Setting MTX_WARMUP to a finite number would bring the cached path back, and it
 * is kept for exactly one purpose: measuring the two against each other.
 */
#define MTX_WARMUP   0xFFFFFFFFu
#define MTX_CANARY   64u    /* only reachable with a finite MTX_WARMUP */

static uint8_t  *blk_hot;      /* one byte per block, region-relative */
static uint32_t  blk_total;
static uint32_t *rgn_blk;      /* first block index of each region */
static uint32_t  cl_warm;      /* switches spent in warm-up */
static uint32_t  cl_canary;    /* canary rounds done */
static uint32_t  cl_bad;       /* cold blocks found changed by a canary */
static uint32_t  cl_save_bytes;/* bytes actually copied by the last save */
static uint32_t  cl_load_bytes;/* bytes actually copied by the last load */
static uint32_t *cl_bytes;     /* which of the two the current pass feeds */

static uint32_t mtx_blk_sum(uint32_t bytes)
{
	return (bytes + MTX_BLK - 1u) / MTX_BLK;
}

/* Layout the block map over the regions. Called from mtx_layout(), after the
   regions are known, and deliberately not allocating: `blk_hot` and `rgn_blk` are
   static storage, so this works in the host test too. */
#define MTX_MAX_RGN 8u
#define MTX_MAX_BLK 32768u
static uint8_t  blk_hot_store[MTX_MAX_BLK];
static uint32_t rgn_blk_store[MTX_MAX_RGN];

static uint8_t blk_ready;
static void mtx_blk_layout(void);

/* Regions are registered after mtx_dos_init, so the map is built on the first
   save or load - and only then: rebuilding it per switch would throw the hot set
   away every time. */
static void mtx_blk_ensure(void)
{
	if(blk_ready)
		return;
	mtx_blk_layout();
	blk_ready = 1;
}

static void mtx_blk_layout(void)
{
	uint32_t i, b = 0;

	for(i = 0; i < (uint32_t)nrgn && i < MTX_MAX_RGN; i++)
	{
		rgn_blk_store[i] = b;
		b += mtx_blk_sum(rgn[i].size);
		if(b > MTX_MAX_BLK)
			b = MTX_MAX_BLK;
	}
	rgn_blk  = rgn_blk_store;
	blk_hot  = blk_hot_store;
	blk_total = b;
	for(i = 0; i < blk_total; i++)
		blk_hot[i] = 0;
	cl_warm = cl_canary = cl_bad = cl_save_bytes = cl_load_bytes = 0;
}

/*
 * One region, one direction, one mode.
 *   MTX_CP_ALL: every block (warm-up: also COMPARE and mark what differs)
 *   MTX_CP_HOT: only the hot blocks
 *   MTX_CP_COLD_CHECK: only the cold blocks, compare and report what differs
 */
#define MTX_CP_ALL        0
#define MTX_CP_HOT        1
#define MTX_CP_COLD_CHECK 2
#define MTX_CP_SEED       3	/* unconditional: a depth's snapshot is uninitialised */



static void mtx_copy_region(uint32_t r, uint8_t *s, int dir, int mode)
{
	uint8_t  *base = (uint8_t *)rgn[r].base;
	uint32_t  size = rgn[r].size;
	uint32_t  off, first = rgn_blk ? rgn_blk[r] : 0u;
	uint8_t  *snap = s + rgn[r].off;

	for(off = 0; off < size; off += MTX_BLK)
	{
		uint32_t n = size - off < MTX_BLK ? size - off : MTX_BLK;
		uint32_t b = first + off / MTX_BLK;
		uint32_t k;
		uint32_t same = 1;

		if(off / MTX_BLK >= 0u && b >= blk_total)
			break;

		if(mode == MTX_CP_HOT && !blk_hot[b])
			continue;
		if(mode == MTX_CP_COLD_CHECK && blk_hot[b])
			continue;

		if(mode != MTX_CP_HOT && mode != MTX_CP_SEED)
		{
			/* compare: this is the only place anything is read without being
			   written, and it is why warm-up and canaries cost more than a copy */
			for(k = 0; k + 4u <= n; k += 4u)
				if(*(uint32_t *)(base + off + k) != *(uint32_t *)(snap + off + k))
				{
					same = 0;
					break;
				}
			if(same && (n & 3u))
				for(; k < n; k++)
					if(base[off + k] != snap[off + k])
					{
						same = 0;
						break;
					}
		}

		if(mode == MTX_CP_COLD_CHECK)
		{
			if(!same)
			{
				blk_hot[b] = 1;    /* promote: from now on it takes the normal path */
				cl_bad++;
				if(dir == 0)
					mtx_bcopy(snap + off, base + off, n);
				else
					mtx_bcopy(base + off, snap + off, n);
			}
			continue;
		}

		if(mode == MTX_CP_ALL)
		{
			if(!same)
				blk_hot[b] = 1;    /* hot from now on, and copied below */
			else
				continue;          /* identical: copying it would be the old cost */
		}

		if(dir == 0)
			mtx_bcopy(snap + off, base + off, n);
		else
			mtx_bcopy(base + off, snap + off, n);
		if(cl_bytes)
			*cl_bytes += n;
	}
}

/*
 * The block-level pass, in the three modes above. `dir` 0 = save (live -> snapshot),
 * 1 = load (snapshot -> live). Returns the number of bytes copied, which is the
 * figure the log reports: it is the whole point of the change.
 */
static uint32_t mtx_copy_pass(uint8_t *s, int dir, int mode)
{
	uint32_t i, before;

	if(!blk_hot || !blk_total || !rgn_blk)
		return 0;

	before = cl_bytes ? *cl_bytes : 0u;
	for(i = 0; i < (uint32_t)nrgn; i++)
		if(i < MTX_MAX_RGN)
			mtx_copy_region(i, s, dir, mode);
	return (cl_bytes ? *cl_bytes : 0u) - before;
}

void mtx_dos_copyless(uint32_t *save_bytes, uint32_t *load_bytes,
                      uint32_t *ever_hot_bytes, uint32_t *switches,
                      uint32_t *promoted)
{
	uint32_t i, hot = 0;

	for(i = 0; i < blk_total; i++)
		if(blk_hot && blk_hot[i])
			hot++;
	if(save_bytes)
		*save_bytes = cl_save_bytes;      /* what the last save actually copied */
	if(load_bytes)
		*load_bytes = cl_load_bytes;      /* what the last load actually copied */
	if(ever_hot_bytes)
		*ever_hot_bytes = hot * MTX_BLK;  /* union of blocks ever seen differing */
	if(switches)
		*switches = cl_warm;              /* switch count the map is based on */
	if(promoted)
		*promoted = cl_bad;
}

void mtx_dos_save_state(mtx_context_t *outgoing)
{
	uint8_t *s = mtx_slot(outgoing->depth);
	int i;

	if(!s)
		return;
	mtx_layout();
	mtx_blk_ensure();

	/* A depth whose snapshot has never been written cannot be treated block by
	   block: "identical" would be a comparison against uninitialised memory, and
	   every block that happened to match would stay garbage. Copy it whole. */
	cl_save_bytes = 0;
	cl_bytes = &cl_save_bytes;
#if !MTX_COPYLESS
	for(i = 0; i < nrgn; i++)
		mtx_bcopy(s + rgn[i].off, rgn[i].base, rgn[i].size);
	cl_save_bytes = mtx_dos_snapshot_bytes();
	inited[outgoing->depth] = 1;
#else
	if(!inited[outgoing->depth])
		mtx_copy_pass(s, 0, MTX_CP_SEED);
	else if(cl_warm < MTX_WARMUP)
	{
		cl_warm++;
		mtx_copy_pass(s, 0, MTX_CP_ALL);
	}
	else
		mtx_copy_pass(s, 0, MTX_CP_HOT);	/* cached path: measurement only */
#endif

	for(i = 0; i < nhok; i++)
		mtx_bcopy(s + hok[i].off, hok[i].addr, hok[i].len);
	inited[outgoing->depth] = 1;
}

void mtx_dos_load_state(mtx_context_t *incoming)
{
	uint8_t *s = mtx_slot(incoming->depth);
	uint8_t  d = incoming->depth;
	int i;

	if(!s)
		return;
	mtx_layout();
	mtx_blk_ensure();

	/* First entry into this depth: seed its snapshot from the live state, i.e.
	   the child starts as a copy of its parent and then diverges. */
	if(!inited[d])
	{
		for(i = 0; i < nrgn; i++)
			mtx_bcopy(s + rgn[i].off, rgn[i].base, rgn[i].size);
		for(i = 0; i < nhok; i++)
			mtx_bcopy(s + hok[i].off, hok[i].addr, hok[i].len);
		inited[d] = 1;
	}

	cl_load_bytes = 0;
	cl_bytes = &cl_load_bytes;
#if !MTX_COPYLESS
	for(i = 0; i < nrgn; i++)
		mtx_bcopy(rgn[i].base, s + rgn[i].off, rgn[i].size);
	cl_load_bytes = mtx_dos_snapshot_bytes();
#else
	if(cl_warm < MTX_WARMUP)
		mtx_copy_pass(s, 1, MTX_CP_ALL);
	else
	{
		mtx_copy_pass(s, 1, MTX_CP_HOT);

		/* the canary: is the cold region still cold? A block that says no is
		   promoted, so the answer costs one round of comparisons every
		   MTX_CANARY switches instead of a full copy on every switch. */
		if((++cl_canary % MTX_CANARY) == 0u)
			mtx_copy_pass(s, 1, MTX_CP_COLD_CHECK);
	}
#endif

	for(i = 0; i < nhok; i++)
		mtx_bcopy(hok[i].addr, s + hok[i].off, hok[i].len);

	/* Pinned pointers last: the depth's own heap and framebuffer must win over
	   whatever its snapshot carried, since different depths share the code but
	   never the zone or the screen. */
	for(i = 0; i < MTX_PIN_COUNT; i++)
		if(pin_addr[i])
			*(void **)pin_addr[i] = pin_val[d][i];
}
