// kgsws' ACE Engine / matryoshka
////
// Engine-side entry points for the nested-instance host. Both are declared with
// the same call convention as the engine functions that call them
// (regparm(2)/no_caller_saved_registers), so calling them cannot break Doom's
// assumption that eax/ecx/edx survive.
#include "mtx_sched.h"

// Called from the top of Z_Init, BEFORE the zone consumes the rest of RAM.
// Idempotent: a nested depth reaches Z_Init too, and the arena belongs to the
// machine, so a second call is a no-op rather than a second claim.
void mtx_engine_reserve(void) MTX_FRAME_TICK_ABI;

// Build the CALLING depth's own zone in its arena slot and prove Doom's allocator
// can work in it. Returns the zone, or 0. This is the nested half of Z_Init - the
// root takes DPMI RAM, a child takes its own slot - and it is also the call the
// loaded guest makes through its service table, so both paths are the same one.
void *mtx_engine_zone_init(void);

// Called once per frame from the per-frame hook targets.
void mtx_engine_frame(void) MTX_FRAME_TICK_ABI;
