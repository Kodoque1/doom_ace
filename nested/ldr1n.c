/*
 * matryoshka/ldr1n.c - the reentrant second stage, in C.
 * Engine-agnostic: Doom services arrive through mtx_ldr1n_ops_t.
 */
#include "ldr1n.h"

uint32_t mtx_ldr1n_fdlist(const uint8_t *lumpinfo, uint32_t numlumps,
                          int32_t *out, uint32_t cap)
{
	const uint8_t *p = lumpinfo;
	int32_t last = -1;		/* matches ACE_LDR1's "xor edx,edx; dec edx" */
	uint32_t i, n = 0;

	if(!lumpinfo || !out || !cap)
		return 0;

	for(i = 0; i < numlumps; i++, p += MTX_LDR1N_LUMPINFO_SIZE)
	{
		int32_t fd = *(const int32_t *)(const void *)(p + MTX_LDR1N_FD_OFFSET);

		if(fd == last)
			continue;
		if(n + 1 >= cap)	/* keep room for the -1 terminator */
			return 0;
		out[n++] = fd;
		last = fd;
	}
	out[n] = -1;
	return n;
}

int mtx_ldr1n_load(const mtx_ldr1n_ops_t *ops, void *ctx,
                   const mtx_ldr1n_src_t *src,
                   void **out_mem, uint32_t *out_size)
{
	uint32_t bss, total;
	void *mem;

	if(!ops || !ops->read || !ops->alloc || !src || !out_mem || !out_size)
		return -1;
	if(src->lump_size < 4)
		return -2;		/* must hold at least the .bss size */

	/* the engine stores the .bss size in the lump's first 4 bytes */
	if(ops->read(ctx, src->fd, &bss, 4) != 4)
		return -3;

	/* +32 mirrors ACE_LDR1's headroom. Allocation comes from the depth's own
	   memory, never from freeing the parent's. */
	total = bss + src->lump_size + 32u;
	mem = ops->alloc(ctx, total);
	if(!mem)
		return -4;

	*(uint32_t *)mem = bss;

	/* file position is already 4 bytes past the lump start after the pre-read */
	if(ops->read(ctx, src->fd, (uint8_t *)mem + 4, src->lump_size - 4)
	   != (int32_t)(src->lump_size - 4))
		return -5;

	*out_mem = mem;
	*out_size = total;
	return 0;
}

int mtx_ldr1n_prepare(const mtx_ldr1n_ops_t *ops, void *ctx,
                      const mtx_ldr1n_src_t *src,
                      const uint8_t *lumpinfo, uint32_t numlumps,
                      int32_t *fdlist_scratch, uint32_t fdlist_cap,
                      mtx_ldr1n_out_t *out)
{
	uint32_t n;
	int r;

	if(!out)
		return -1;
	out->code = 0;
	out->code_size = 0;
	out->fdlist = 0;
	out->fdlist_len = 0;

	n = mtx_ldr1n_fdlist(lumpinfo, numlumps, fdlist_scratch, fdlist_cap);
	if(!n)
		return -2;
	out->fdlist = fdlist_scratch;
	out->fdlist_len = n;

	r = mtx_ldr1n_load(ops, ctx, src, &out->code, &out->code_size);
	if(r)
		return r;
	return 0;
}
