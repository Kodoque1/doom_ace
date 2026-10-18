// kgsws' ACE Engine / matryoshka
////
// Engine-side entry points for the nested-instance host. Both are declared with
// the same call convention as the engine functions that call them
// (regparm(2)/no_caller_saved_registers), so calling them cannot break Doom's
// assumption that eax/ecx/edx survive.
#include "mtx_sched.h"

// Called from the top of Z_Init, BEFORE the zone consumes the rest of RAM.
void mtx_engine_reserve(void) MTX_FRAME_TICK_ABI;

// Called once per frame from the per-frame hook targets.
void mtx_engine_frame(void) MTX_FRAME_TICK_ABI;
