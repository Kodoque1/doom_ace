// kgsws' ACE Engine / matryoshka
////
// Engine-side entry points for the nested-instance host. Both are declared with
// the same call convention as the engine functions that call them
// (regparm(2)/no_caller_saved_registers), so calling them cannot break Doom's
// assumption that eax/ecx/edx survive.
#include "mtx_sched.h"
#include "mtx_raw.h"		// mtx_raw_t, for mtx_engine_raw_pool()

// Called from the top of Z_Init, BEFORE the zone consumes the rest of RAM.
// Idempotent: a nested depth reaches Z_Init too, and the arena belongs to the
// machine, so a second call is a no-op rather than a second claim.
void mtx_engine_reserve(void) MTX_FRAME_TICK_ABI;

// Build the CALLING depth's own zone in its arena slot and prove Doom's allocator
// can work in it. Returns the zone, or 0. This is the nested half of Z_Init - the
// root takes DPMI RAM, a child takes its own slot - and it is also the call the
// loaded guest makes through its service table, so both paths are the same one.
void *mtx_engine_zone_init(void);

// The engine's raw (non-zone) allocation entry points. The zone was half of what
// a depth needs; these are the other half. alloc decides by the current depth,
// free/realloc decide by the pointer's provenance - see the block comment in
// mtx_boot.c for why those are two different questions.
void *mtx_engine_alloc(uint32_t size);
void *mtx_engine_realloc(void *ptr, uint32_t size);
void  mtx_engine_free(void *ptr);

// Read the parameter block a nested instance was entered with (%ebp at the entry,
// stored by asm.S) and act on it: report what the instance got, and point its
// rendering at its own framebuffer. Called at the top of Z_Init, before the
// root/child branch, because that block is what decides the branch. A no-op when
// this copy of the engine IS the host.
void mtx_engine_apply_params(void);

// Which depth this copy of the engine is running as: a nested instance's own
// (from its parameter block), or the scheduler's when this is the host. The
// engine's init branches on this, so it must be the same answer the allocator
// gets - hence one function, used by both.
uint8_t mtx_engine_depth(void);

// The raw pool the calling code allocates from (see mtx_raw.h), for diagnostics
// that need the pool's own account of what it did.
mtx_raw_t *mtx_engine_raw_pool(void);

// TEMPORARY diagnostic: report an absurd allocation request, with its caller.
void mtx_alloc_probe(uint32_t size, uint32_t caller, uint32_t self);

// TEMPORARY diagnostic: the stack an error screen was reached from. Called from
// the I_Error hook in asm.S, which pushes its arguments - so this is a plain C
// function and must not carry MTX_FRAME_TICK_ABI.
void mtx_error_probe(uint32_t esp, uint32_t ret, uint32_t fmt, uint32_t value);

// TEMPORARY diagnostic: the first index the texture loop processes, per depth.
void MTX_FRAME_TICK_ABI mtx_tex_probe(uint32_t first, uint32_t count);

// TEMPORARY diagnostic: what the texture count was derived from, per depth.
void MTX_FRAME_TICK_ABI mtx_texcount_probe(uint32_t t1, uint32_t t2, uint32_t idx1,
                                           uint32_t idx2);

// TEMPORARY diagnostic: report a lump-cache call whose index cannot be valid.
// Same call convention as the ACE hook functions that call it.
void MTX_FRAME_TICK_ABI mtx_lump_probe(uint32_t site, uint32_t idx, uint32_t value,
                                       uint32_t aux);

// Called once per frame from the per-frame hook targets.
void mtx_engine_frame(void) MTX_FRAME_TICK_ABI;
