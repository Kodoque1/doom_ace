// kgsws' ACE Engine
////
#include "sdk.h"
#include "utils.h"
#include "engine.h"
#include "wadfile.h"
#include "mtx_boot.h"	// matryoshka: depth-aware raw allocation

//

static num64_t range_defs[4];

/* How many lumps the IWAD contributed, and how many PWAD copies of a lump
   directory have been hidden. Both exist for `wad_hide_lump` below: the hide has
   to remove every PWAD's copy of TEXTURE1/PNAMES and keep the IWAD's, because
   the exploit is delivered as a crafted TEXTURE1+PNAMES in a PWAD. */
uint32_t wad_iwad_lumps;
uint32_t wad_dir_hidden;

//
// funcs

/*
 * Add a WAD - or any file that is not one, which becomes a single lump - to the
 * lump table by APPENDING to it: `wad_check_lump` searches backward, so the
 * newest copy of a name wins, and that is what lets a file added at run time
 * bring its own map under its own name (see mtx_level_wad_add in mtx_boot.c).
 *
 * It was `static` while the only caller was `wad_init`'s command-line loop.
 * MATRYOSHKA's option B has a nested INSTANCE add its own level WAD from inside
 * the level-load path, so the add is an API now.
 *
 * WHO MUST TOUCH `lumpcache` AFTERWARDS. Doom's `lumpcache` is indexed by lump
 * number and is sized from `numlumps` by the caller, never here: during
 * `wad_init` it does not exist yet (it is allocated after the loop), so this
 * function has no business resizing it. An add that happens LATER does, and the
 * caller that knows it is post-init does it - `mtx_level_wad_add`, and only when
 * the running depth's own pool owns the cache. Growing the table without growing
 * the cache would be a write past its end the first time a patch from the new
 * file went through the cache (font.c, stbar.c read it by index).
 */
void wad_add(uint8_t *name)
{
	int32_t fd;
	uint32_t tmp;
	wadhead_t head;
	lumpinfo_t *info;
	wadlump_t *lump = (wadlump_t*)d_drawsegs; // temporary memory
	uint32_t totalsize;
	uint_fast8_t is_ignored = 0;

	doom_printf("   %s ", name);

	fd = doom_open_RD(name);
	if(fd < 0)
	{
		doom_printf("- failed to open\n");
		return;
	}

	totalsize = strlen(name);
	if(totalsize > 4 && (*((uint32_t*)(name + totalsize - 4)) | 0x20202020) != 0x6461772E)
	{
		// load entire file as a single lump
		uint8_t *ptr, *dst;

		tmp = numlumps;

		// realloc lump info
		numlumps++;
		lumpinfo = ldr_realloc(lumpinfo, numlumps * sizeof(lumpinfo_t));
		info = lumpinfo + tmp;

		// copy file name
		ptr = name + totalsize - 4;

		while(ptr > name && ptr[-1] != '\\' && ptr[-1] != '/')
			*ptr--;

		info->wame = 0;
		dst = info->name;
		while(*ptr && *ptr != '.' && ptr < info->name + 8)
		{
			uint8_t in = *ptr++;
			if(in >= 'a' && in <= 'z')
				in &= ~0x20;
			*dst++ = in;
		}

		// create entry
		info->fd = fd;
		info->offset = 0;
		info->size = doom_filelength(fd);

		doom_printf("- file, %u bytes; %.8s\n", info->size, info->name);

		return;
	}

	tmp = doom_read(fd, &head, sizeof(wadhead_t));
	if(	tmp != sizeof(wadhead_t) ||
		(head.id != 0x44415749 && head.id != 0x44415750)
	){
		doom_printf("- invalid\n");
		doom_close(fd);
		return;
	}

	tmp = numlumps;

	// realloc lump info
	numlumps += head.numlumps;
	lumpinfo = ldr_realloc(lumpinfo, numlumps * sizeof(lumpinfo_t));
	info = lumpinfo + tmp;

	doom_lseek(fd, head.diroffs, SEEK_SET);

	// load lumps
	totalsize = 0;
	for(uint32_t i = 0; i < head.numlumps; i++, info++)
	{
		wadlump_t *lmp = lump + (i & 511);

		if(lmp == lump)
		{
			uint32_t size = (head.numlumps - i) * sizeof(wadlump_t);

			if(size > 512 * sizeof(wadlump_t))
				size = 512 * sizeof(wadlump_t);

			tmp = doom_read(fd, lump, size);
			if(tmp != size)
				// reading failed; fill with dummy values
				memset(lump, 0, size);
		}

		info->wame = 0; // apparently, there are some WADs that have broken lump names
		strncpy(info->name, lmp->name, 8);
		info->fd = fd;
		info->offset = lmp->offset;
		info->size = lmp->size;

		// check for ignore
		if(is_ignored)
		{
			if(info->wame == 0x534B49505F454341) // ACE_PIKS
				is_ignored = 0;
			info->wame |= 0x80;
		} else
		{
			if(info->wame == 0x50494B535F454341) // ACE_SKIP
			{
				is_ignored = 1;
				info->wame |= 0x80;
			}
		}

		totalsize += lmp->size;
	}

	doom_printf("- %u entries, %u bytes\n", head.numlumps, totalsize);
}

//
// API

void wad_init()
{
	int32_t arg;

	doom_printf("[ACE] wad_init\n");

	ldr_alloc_message = "WAD";

	arg = M_CheckParm("-iwad");
	if(arg > 0 && arg < myargc-1)
		wadfiles[0] = myargv[arg+1];

	lumpinfo = ldr_malloc(1);
	numlumps = 0;

	for(uint32_t i = 0; i < MAXWADFILES && wadfiles[i]; i++)
	{
		wad_add(wadfiles[i]);
		/* wadfiles[0] is the IWAD (Doom's own argument handling puts it there,
		   and `-iwad` can only replace index 0), so what the first file added is
		   exactly the IWAD's lump range. Everything after it is a PWAD. */
		if(i == 0)
			wad_iwad_lumps = numlumps;
	}

	if(!numlumps)
		engine_error("WAD", "Umm. WADs are empty ...");

	if(numlumps >= 65535)
		engine_error("WAD", "Wow. Soo many lumps ...");

	lumpcache = ldr_malloc(numlumps * sizeof(void*));
	memset(lumpcache, 0, numlumps * sizeof(void*));
}

uint64_t wad_name64(const uint8_t *name)
{
	union
	{
		uint64_t w;
		uint8_t b[8];
	} nm;

	nm.w = 0;
	for(uint32_t i = 0; i < 8; i++)
	{
		register uint8_t in = name[i];
		if(!in)
			break;
		if(in >= 'a' && in <= 'z')
			in &= ~0x20; // uppercase only
		nm.b[i] = in;
	}

	return nm.w;
}

int32_t wad_check_lump(const uint8_t *name)
{
	uint64_t wame;
	uint32_t idx;
	lumpinfo_t *li = lumpinfo;

	// search as 64bit number
	wame = wad_name64(name);

	// do a backward search
	idx = numlumps;
	do
	{
		idx--;
		if(li[idx].wame == wame)
			return idx;
	} while(idx);

	// not found
	return -1;
}

int32_t wad_get_lump(const uint8_t *name)
{
	int32_t idx;

	idx = wad_check_lump(name);
	if(idx < 0)
		engine_error("WAD", "Can't find lump %s!", name);

	return idx;
}

uint32_t wad_read_lump(void *dest, int32_t idx, uint32_t limit)
{
	int32_t ret;
	lumpinfo_t *li = lumpinfo + idx;

	if(li->size < limit)
		limit = li->size;

	doom_lseek(li->fd, li->offset, SEEK_SET);
	ret = doom_read(li->fd, dest, limit);
	if(ret != limit)
	{
		/* TEMPORARY diagnostic: a nested instance's lump read now fails where
		   it used to die earlier. Record what the read was given, from the
		   instance's own memory, in its own file - plus what the pool says it
		   did, since the lump table and the read buffer both come from it. */
		uint32_t v[24];
		int32_t fd;
		mtx_raw_t *pool = mtx_engine_raw_pool();

		v[0] = 0x4D54584Cu;				/* 'MTXL' */
		v[1] = (uint32_t)(uintptr_t)lumpinfo;
		v[2] = (uint32_t)numlumps;
		v[3] = (uint32_t)idx;
		v[4] = (uint32_t)li->fd;
		v[5] = li->offset;
		v[6] = li->size;
		v[7] = (uint32_t)(uintptr_t)dest;
		v[8] = (uint32_t)limit;
		v[9] = (uint32_t)ret;
		v[10] = (uint32_t)(uintptr_t)lumpcache;
		v[11] = (uint32_t)(uintptr_t)li;
		v[12] = (uint32_t)mtx_engine_depth();
		v[13] = (uint32_t)(uintptr_t)mainzone;
		v[14] = (uint32_t)(uintptr_t)li - (uint32_t)(uintptr_t)lumpinfo;
		v[15] = 0x4D54584Cu;
		v[16] = pool ? (uint32_t)(uintptr_t)pool->base : 0;
		v[17] = pool ? pool->size : 0;
		v[18] = pool ? pool->used : 0;
		v[19] = pool ? pool->served : 0;
		v[20] = pool ? pool->refusals : 0;
		v[21] = pool ? pool->reclaimed : 0;
		v[22] = pool ? pool->leaked : 0;
		v[23] = pool ? pool->invalid : 0;
		fd = doom_open_WR("MTXW.LOG");
		if(fd >= 0)
		{
			doom_write(fd, v, sizeof(v));
			doom_close(fd);
		}

		engine_error("WAD", "Lump %.8s read failed!", li->name);
	}

	return limit;
}

void *wad_cache_lump(int32_t idx, uint32_t *size)
{
	uint8_t *data;
	lumpinfo_t *li = lumpinfo + idx;

	if(!li->size)
		engine_error("WAD", "Lump %.8s is empty!");

	/* TEMPORARY: an absurd lump size, or an index off the end of the table,
	   says which of the two is corrupted. */
	if(idx < 0 || idx >= (int32_t)numlumps || li->size > 4u * 1024u * 1024u)
		mtx_lump_probe(5u, (uint32_t)idx, li->size, (uint32_t)(uintptr_t)lumpinfo);

	/* A lump can be large enough to matter to a nested instance, so the cache
	   allocates through the depth-aware wrapper: at the root that is still
	   doom_malloc, at a depth it is that depth's own pool. */
	data = mtx_engine_alloc(li->size + 4); // extra space for text files
	if(!data)
		engine_error("WAD", "Lump %.8s allocation failed!", li->name);

	wad_read_lump(data, idx, li->size);
	data[li->size] = 0; // string terminator

	if(size)
		*size = li->size;

	return data;
}

void *wad_cache_optional(const uint8_t *name, uint32_t *size)
{
	int32_t idx;
	uint8_t *data;
	lumpinfo_t *li;

	idx = wad_check_lump(name);
	if(idx < 0)
		return NULL;

	li = lumpinfo + idx;

	/* TEMPORARY: mirror of the check in wad_cache_lump. */
	if(li->size > 4u * 1024u * 1024u)
		mtx_lump_probe(6u, (uint32_t)idx, li->size, (uint32_t)(uintptr_t)lumpinfo);

	/* A lump can be large enough to matter to a nested instance, so the cache
	   allocates through the depth-aware wrapper: at the root that is still
	   doom_malloc, at a depth it is that depth's own pool. */
	data = mtx_engine_alloc(li->size + 4); // extra space for text files
	if(!data)
		engine_error("WAD", "Lump %.8s allocation failed!", li->name);

	wad_read_lump(data, idx, li->size);
	data[li->size] = 0; // string terminator

	if(size)
		*size = li->size;

	return data;
}

/*
 * Hide a lump NAME from the lookups.
 *
 * This used to hide the single lump wad_check_lump found - the LAST one, because
 * the search is backward. That is the same lump whenever there is exactly one
 * PWAD, which is why it went unnoticed; with several PWADs the reveal is nasty:
 * the exploit ships a CRAFTED `TEXTURE1`/`PNAMES`, the texture loader hides ONE
 * copy after reading its own directory, the next copy of the same name then wins
 * the lookup - and the engine reads the exploit's fake directory as if it were
 * the real one. Measured exactly that with three level WADs loaded:
 *
 *     ACE Engine / An error has occurred...
 *     Module that caused this error is TEXTURE
 *     Texture OVERADDR is missing patch!
 *
 * OVERADDR is the first fake entry in the crafted TEXTURE1, so the failure was
 * the engine trusting the exploit's own decoy table.
 *
 * So hide EVERY copy that lives in a PWAD, and never the IWAD's. The boundary is
 * not a guess: wadfiles[0] is the IWAD and the lumps are appended in file order,
 * so `wad_iwad_lumps` is where the IWAD's range ends. If that is unknown (a file
 * failed to open), fall back to hiding the single winning copy, which is what
 * this always did.
 */
void wad_hide_lump(const uint8_t *name)
{
	uint64_t wame = wad_name64(name);
	uint32_t i;

	if(!numlumps)
		return;

	if(!wad_iwad_lumps || wad_iwad_lumps > numlumps)
	{
		int32_t lump = wad_check_lump(name);

		if(lump >= 0)
			lumpinfo[lump].wame = 0;
		wad_dir_hidden++;
		return;
	}

	/* backward, i.e. newest PWAD first - the same order the lookup uses */
	for(i = numlumps; i-- > wad_iwad_lumps; )
		if(lumpinfo[i].wame == wame)
		{
			lumpinfo[i].wame = 0;
			wad_dir_hidden++;
		}
}

void wad_handle_range(uint16_t ident, void (*cb)(lumpinfo_t*))
{
	lumpinfo_t *li = lumpinfo;
	lumpinfo_t *le = lumpinfo + numlumps;
	uint32_t is_inside = 0;

	if(ident > 255)
	{
		range_defs[0].u64 = (0x0054524154535F00 << 8) | ident;
		range_defs[2].u64 = (0x000000444E455F00 << 8) | ident;
		range_defs[1].u64 = 0xFFFFFFFFFFFFFFFF;
		range_defs[3].u64 = 0xFFFFFFFFFFFFFFFF;
	} else
	{
		range_defs[0].u64 = 0x0054524154535F00 | ident;
		range_defs[2].u64 = 0x000000444E455F00 | ident;
		ident |= ident << 8;
		range_defs[1].u64 = 0x54524154535F0000 | ident;
		range_defs[3].u64 = 0x0000444E455F0000 | ident;
	}

	for( ; li < le; li++)
	{
		if(is_inside)
		{
			if(li->wame == range_defs[2].u64 || li->wame == range_defs[3].u64)
				is_inside = 0;
			else
				cb(li);
		} else
		{
			if(li->wame == range_defs[0].u64)
				is_inside = 1;
			else
			if(li->wame == range_defs[1].u64)
				is_inside = 2;
		}
	}

	if(is_inside)
		engine_error("WAD", "Unclosed range %.8s / %.8s\n", range_defs[is_inside-1].u8, range_defs[is_inside+1].u8);
}

void wad_handle_lump(const uint8_t *name, void (*cb)(lumpinfo_t*))
{
	uint64_t wame;
	lumpinfo_t *li = lumpinfo;

	// search as 64bit number
	wame = wad_name64(name);

	for(uint32_t i = 0; i < numlumps; i++, li++)
	{
		if(li->wame == wame)
			cb(li);
	}
}

//
// hooks
static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// disable 'W_InitMultipleFiles'
	{0x00038A30, CODE_HOOK | HOOK_UINT8, 0xC3},
	// disable call to 'W_Reload' in 'P_SetupLevel'
	{0x0002E858, CODE_HOOK | HOOK_SET_NOPS, 5},
};

