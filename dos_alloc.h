/*
 * matryoshka/dos_alloc.h
 *
 * The DOS-side caller: an arena over zone memory, a budget planner, and a
 * one-call boot that carves and wires everything. Kept engine-agnostic and
 * libc-free so it is both compilable into ACE and testable on the host.
 *
 * The zone-sizing question lives here. Doom asks for an 8 MiB zone at boot;
 * the host cannot afford that per depth (12 x 8 MiB ~ 96 MiB against ~60 MiB
 * of DPMI memory). mtx_plan() turns that into arithmetic, so the depth ceiling
 * becomes a sizing decision rather than a guess.
 */
#ifndef MATRYOSHKA_DOS_ALLOC_H
#define MATRYOSHKA_DOS_ALLOC_H

#include <stdint.h>
#include "context.h"
#include "dos_state.h"

/* ---- arena: bump allocator over caller-owned memory ---------------------- */
typedef struct {
	uint8_t *base;
	uint32_t size;
	uint32_t used;
} mtx_arena_t;

void     mtx_arena_init(mtx_arena_t *a, void *mem, uint32_t size);
void    *mtx_arena_alloc(mtx_arena_t *a, uint32_t size, uint32_t align);
uint32_t mtx_arena_used(const mtx_arena_t *a);

/*
 * A depth's screen region, sized from what the ENGINE actually puts behind the
 * `screen_buffer` pointer - not from one framebuffer. Measured from the engine's
 * own source:
 *
 *   [0, 64000)                  the 320x200 frame the renderer draws into
 *   [64000, 133632)             loader progress-bar patch (main.c reads 69632 B
 *                               at screen_buffer + 64000)
 *   [133632, 133632 + 0x28000)  TP_MEMORY_ADDR, the text/script parse arena
 *                               (textpars.h reads 0x28000 B at screen_buffer +
 *                               64000 + 69632)
 *
 * Sizing a depth's screen at one frame (64000) would let a nested instance's
 * engine write ~233 KiB past its own buffer, straight into the next depth's
 * zone. One frame is the minimum a depth can render into; it is not what the
 * engine needs. The three parts stay separate constants so the host test can
 * assert the total against the engine's measured appetite, i.e. shrinking this
 * becomes a test failure rather than a silent overrun.
 */
#define MTX_SCREEN_FRAME_BYTES   (320u * 200u)      /* 64000  */
#define MTX_SCREEN_LOADER_BYTES  (69632u)
#define MTX_SCREEN_PARSE_BYTES   (0x28000u)         /* 163840 */
#define MTX_SCREEN_BYTES  (MTX_SCREEN_FRAME_BYTES + MTX_SCREEN_LOADER_BYTES \
                           + MTX_SCREEN_PARSE_BYTES)

#define MTX_ZONE_DEFAULT  (1u * 1024u * 1024u)

typedef struct {
	uint32_t snapshot;	/* mtx_dos_snapshot_bytes() */
	uint32_t zone;
	uint32_t screen;
	uint32_t stack;
	uint32_t image;		/* per-depth copy of the code image to run */
	uint32_t per_depth;	/* sum of the five */
	uint32_t depths_fit;	/* floor(arena / per_depth) */
	uint8_t  depth;		/* min(MTX_MAX_DEPTH, depths_fit) usable depths */
} mtx_budget_t;

mtx_budget_t mtx_plan(uint32_t arena_bytes, uint32_t snapshot, uint32_t zone,
                      uint32_t screen, uint32_t stack, uint32_t image);

/*
 * Registration callback: registers whatever has to move for the CURRENT
 * mtx_dos_init() and returns the per-depth snapshot size. Injecting it keeps
 * this file free of engine dependencies (the engine passes
 * mtx_dos_wire_register; the host test passes its own).
 */
typedef uint32_t (*mtx_wire_fn)(void);

typedef struct {
	mtx_budget_t budget;
	void        *stack_base;
	uint32_t     stack_bytes;
	void        *snapshot_base;
	uint32_t     snapshot_per_depth;
	uint8_t      depth;		/* usable depth; 0 on failure */
} mtx_boot_t;

/*
 * Carve the arena and wire the state layer. `heaps`, `screens` and `images` are
 * caller-owned arrays of MTX_MAX_DEPTH pointers, filled in. `images` receives
 * each depth's own code-image slot (NULL when image_per_depth is 0). Returns 0,
 * or -1 if the arena cannot hold at least two depths (two is the minimum that
 * nests).
 */
int mtx_dos_boot(void *arena_mem, uint32_t arena_bytes, uint32_t zone_per_depth,
                 uint32_t image_per_depth,
                 mtx_wire_fn wire_register,
                 void **heaps, void **screens, void **images, mtx_boot_t *out);

#endif /* MATRYOSHKA_DOS_ALLOC_H */
