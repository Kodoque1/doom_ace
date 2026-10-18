/*
 * matryoshka/dos_state.c
 *
 * Implements the save_state/load_state pair described in dos_state.h. No libc,
 * no engine dependency: the wiring layer registers whatever has to move.
 */
#include "dos_state.h"

#ifndef MTX_DOS_STATIC_PER_DEPTH
#define MTX_DOS_STATIC_PER_DEPTH (640u * 1024u)	/* host pool: 640 KiB/depth */
#endif

typedef struct { void *base; uint32_t size; uint32_t off; } mtx_dregion_t;
typedef struct { void *addr; uint32_t len;  uint32_t off; } mtx_dhook_t;

static mtx_dregion_t rgn[MTX_DOS_MAX_REGIONS];
static mtx_dhook_t   hok[MTX_DOS_MAX_HOOKS];
static int           nrgn, nhok;

static void         *pin_addr[MTX_PIN_COUNT];
static void         *pin_val[MTX_MAX_DEPTH][MTX_PIN_COUNT];

static uint8_t      *pool;
static uint32_t      per_depth;
static uint8_t       inited[MTX_MAX_DEPTH];
static uint8_t       maxd;

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

void mtx_dos_save_state(mtx_context_t *outgoing)
{
	uint8_t *s = mtx_slot(outgoing->depth);
	int i;

	if(!s)
		return;
	mtx_layout();
	for(i = 0; i < nrgn; i++)
		mtx_bcopy(s + rgn[i].off, rgn[i].base, rgn[i].size);
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

	for(i = 0; i < nrgn; i++)
		mtx_bcopy(rgn[i].base, s + rgn[i].off, rgn[i].size);
	for(i = 0; i < nhok; i++)
		mtx_bcopy(hok[i].addr, s + hok[i].off, hok[i].len);

	/* Pinned pointers last: the depth's own heap and framebuffer must win over
	   whatever its snapshot carried, since different depths share the code but
	   never the zone or the screen. */
	for(i = 0; i < MTX_PIN_COUNT; i++)
		if(pin_addr[i])
			*(void **)pin_addr[i] = pin_val[d][i];
}
