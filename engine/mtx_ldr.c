// kgsws' ACE Engine / matryoshka
////
// Engine-side glue for the reentrant loaders.
//
// The loaders themselves (ldr0n.h, ldr1n.h) are engine-agnostic: ldr1n.c reads
// and bootstraps, ldr0N.S announces and hands off, and neither knows what a WAD
// is. This file is where they meet the engine:
//
//   read  -> the engine's own WAD layer (wad_check_lump + doom_lseek/doom_read)
//   alloc -> the depth's own code-image slot, carved out of the arena at boot
//   enter -> ldr0N.S, so the hand-off goes through the reentrant first stage
//
// Non-destructiveness is inherited, not re-implemented: the service table has no
// free() and no close(), and the engine's WAD layer keeps its own lumpinfo and
// descriptors - the ones LDR1 destroyed on first boot were Doom's, not ACE's. So
// loading a child's image cannot damage the root's world, by construction.
//
// Why a depth needs its OWN copy rather than sharing the root's: ACE patches code
// (the hook tables) at run time, so two depths sharing one code page would fight
// over the same bytes. Independence requires a private image.

#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "doom_vars.h"

#include "ldr0n.h"
#include "ldr1n.h"
#include "mtx_host.h"
#include "mtx_ldr.h"

// ---- ldr1n services --------------------------------------------------------

static int32_t ldr_read(void *ctx, int32_t fd, void *dst, uint32_t n)
{
	(void)ctx;
	return doom_read(fd, dst, n);
}

/* The depth's image slot, or NULL if the image would not fit. Failing here is the
   wanted behaviour: the budget reserved exactly one image per depth, so an image
   that does not fit means the planner was told the wrong size. */
static void *ldr_alloc(void *ctx, uint32_t n)
{
	uint32_t depth = (uint32_t)(uintptr_t)ctx;

	if(depth >= MTX_MAX_DEPTH || !mtx_hs.images[depth])
		return 0;
	if(n > mtx_hs.image_bytes)
		return 0;
	return mtx_hs.images[depth];
}

/* Hand-off through the reentrant first stage rather than straight to the loaded
   entry: ldr0N is the part that takes its bases as data instead of re-deriving
   them from the startup stack, which is what makes this repeatable. */
static void ldr_enter(void *ctx, void *code, uint32_t code_base,
                      uint32_t data_base, uint32_t ace_base, uint32_t arg)
{
	(void)ctx; (void)code_base; (void)data_base; (void)ace_base;

	mtx_hs.ldr0_desc.banner     = "- [MTX] guest: image entered\n";
	mtx_hs.ldr0_desc.code_base  = doom_code_segment;
	mtx_hs.ldr0_desc.data_base  = doom_data_segment;
	mtx_hs.ldr0_desc.next_stage = code;
	mtx_hs.ldr0_desc.arg        = arg;

	mtx_ldr0n_entry(&mtx_hs.ldr0_desc);	// never returns
}

void mtx_ldr_enter(uint32_t entry)
{
	ldr_enter(0, (void *)(uintptr_t)entry, 0, 0, 0,
	          (uint32_t)(uintptr_t)mtx_hs.ldr_svc);
}

/* The engine's own convention: ECX carries the entry address (see mtx_ldr.h). */
void mtx_ldr_enter_engine(uint32_t entry)
{
	mtx_hs.ldr0_desc.banner = "- [MTX] engine image: entering (ECX = entry)\n";
	ldr_enter(0, (void *)(uintptr_t)entry, 0, 0, 0, entry);
}

// ---- loading ---------------------------------------------------------------

uint32_t mtx_ldr_image_bytes(const uint8_t *name)
{
	lumpinfo_t *li;
	uint32_t bss;
	int32_t idx;

	idx = wad_check_lump(name);
	if(idx < 0)
		return 0;
	li = lumpinfo + idx;
	if(li->size < MTX_LDR1N_HEADER_SIZE)
		return 0;

	// the image's own header says how much .bss it needs, and ldr1n sizes its
	// allocation as bss + lump + 32. Read that dword rather than guessing.
	if(doom_lseek(li->fd, (int32_t)li->offset, SEEK_SET) != (int32_t)li->offset)
		return 0;
	if(doom_read(li->fd, &bss, 4) != 4)
		return 0;

	return bss + li->size + 32u + 32u;	/* + alignment slack for the arena */
}

int mtx_ldr_load(const uint8_t *name, uint8_t depth, uint32_t expect_ver,
                 uint32_t *out_entry, uint32_t *out_total)
{
	mtx_ldr1n_src_t src;
	lumpinfo_t *li;
	void *mem = 0;
	uint32_t total = 0;
	int32_t idx;

	if(out_entry) *out_entry = 0;
	if(out_total) *out_total = 0;
	if(depth >= MTX_MAX_DEPTH || !mtx_hs.images[depth])
		return -1;

	// find the lump in the ENGINE's own lumpinfo (Doom's was freed by LDR1)
	idx = wad_check_lump(name);
	if(idx < 0)
		return -2;

	li = lumpinfo + idx;
	src.fd        = li->fd;
	src.lump_size = li->size;
	src.offset    = li->offset;

	// ldr1n reads sequentially: [bss size][rest], so position the fd first
	if(doom_lseek(src.fd, (int32_t)src.offset, SEEK_SET) != (int32_t)src.offset)
		return -3;

	mtx_hs.ldr_ops.read  = ldr_read;
	mtx_hs.ldr_ops.alloc = ldr_alloc;
	mtx_hs.ldr_ops.enter = ldr_enter;

	if(mtx_ldr1n_load(&mtx_hs.ldr_ops, (void *)(uintptr_t)depth, &src,
	                  &mem, &total) != 0)
		return -4;

	// the image is [4B .bss size][4B version][payload]; the entry is payload + 8,
	// the same arithmetic engine/asm.S does with `sub $0x1008`.
	if(expect_ver && *(uint32_t *)((uint8_t *)mem + 4) != expect_ver)
		return -5;

	if(out_entry)
		*out_entry = (uint32_t)(uintptr_t)mem + MTX_LDR1N_HEADER_SIZE;
	if(out_total)
		*out_total = total;
	return 0;
}
