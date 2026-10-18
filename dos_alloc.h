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
 * Per-depth costs. `screen` and `zone` are policy inputs: the engine's real
 * screen_buffer usage and a level's real working set both need measuring on
 * hardware, so they are pass-throughs, not constants baked in here.
 * 320*200 is one 8-bit framebuffer - the minimum a depth needs to render into.
 */
#define MTX_SCREEN_BYTES  (320u * 200u)
#define MTX_ZONE_DEFAULT  (1u * 1024u * 1024u)

typedef struct {
	uint32_t snapshot;	/* mtx_dos_snapshot_bytes() */
	uint32_t zone;
	uint32_t screen;
	uint32_t stack;
	uint32_t per_depth;	/* sum of the four */
	uint32_t depths_fit;	/* floor(arena / per_depth) */
	uint8_t  depth;		/* min(MTX_MAX_DEPTH, depths_fit) usable depths */
} mtx_budget_t;

mtx_budget_t mtx_plan(uint32_t arena_bytes, uint32_t snapshot, uint32_t zone,
                      uint32_t screen, uint32_t stack);

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
 * Carve the arena and wire the state layer. `heaps` and `screens` are
 * caller-owned arrays of MTX_MAX_DEPTH pointers, filled in. Returns 0, or -1 if
 * the arena cannot hold at least two depths (two is the minimum that nests).
 */
int mtx_dos_boot(void *arena_mem, uint32_t arena_bytes, uint32_t zone_per_depth,
                 mtx_wire_fn wire_register,
                 void **heaps, void **screens, mtx_boot_t *out);

#endif /* MATRYOSHKA_DOS_ALLOC_H */
