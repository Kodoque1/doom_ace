/*
 * matryoshka/mtx_hooks.c
 *
 * Classifying the engine's patches into "what has to be restored". No libc, no engine
 * dependency - the table arrives as bytes and the segment bases as arguments.
 */
#include "mtx_hooks.h"

uint32_t mtx_hook_patch_len(uint32_t type, uint32_t value)
{
	switch(type & 0xFF)
	{
		/* opcode + rel32: a call or jmp overwrites five bytes */
		case MTX_HOOK_JMP_ACE:
		case MTX_HOOK_JMP_DOOM:
		case MTX_HOOK_CALL_ACE:
		case MTX_HOOK_CALL_DOOM:
			return 5;

		case MTX_HOOK_UINT32:
		case MTX_HOOK_ABSADDR_CODE:
		case MTX_HOOK_ABSADDR_DATA:
			return 4;

		case MTX_HOOK_UINT16:
			return 2;

		case MTX_HOOK_UINT8:
			return 1;

		/* memset(addr, 0x90, value): the count is the entry's value field */
		case MTX_HOOK_SET_NOPS:
			return value;

		/* memcpy(addr, value, (type >> 16) & 0x0FFF) - the length is in the type */
		case MTX_HOOK_MEM_COPY:
			return (type >> 16) & 0x0FFFu;

		/* IMPORT / READ*: they write into ACE's memory, not Doom's, so there is
		   nothing here to restore. ACE's own memory is per-depth through the region
		   snapshot. */
		default:
			return 0;
	}
}

uint32_t mtx_hook_patch_addr(uint32_t addr, uint32_t type,
                             uint32_t code_seg, uint32_t data_seg)
{
	if(type & MTX_HOOK_DATA)
		return addr + data_seg;
	if(type & MTX_HOOK_CODE)
		return addr + code_seg;
	return 0;			/* not Doom's memory: refuse */
}

uint32_t mtx_hooks_scan(const void *table, uint32_t count,
                        uint32_t code_seg, uint32_t data_seg,
                        mtx_hook_add_fn add, void *ctx)
{
	const mtx_hook_t *h = (const mtx_hook_t *)table;
	uint32_t n = 0;

	if(!table || !add)
		return 0;

	while(count--)
	{
		uint32_t len, at;

		if(!h->addr)		/* the engine's own terminator */
			break;

		len = mtx_hook_patch_len(h->type, h->value);
		at  = mtx_hook_patch_addr(h->addr, h->type, code_seg, data_seg);

		if(len && at)
		{
			if(add(ctx, (void *)(uintptr_t)at, len) != 0)
				return MTX_HOOKS_SCAN_REFUSED;
			n++;
		}

		h++;
	}

	return n;
}
