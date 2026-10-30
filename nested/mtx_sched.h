/*
 * matryoshka/mtx_sched.h
 *
 * Turning a request into an actual switch.
 *
 * A cooperative switch only happens if the running instance reaches the yield
 * point, and the running instance is Doom's frame loop, which never returns.
 * So the pattern is: somebody sets a request (input handler, debugger, a timer),
 * and the request is serviced at the next frame boundary - never mid-render,
 * mid-think, or inside a critical section.
 *
 * The frame boundary in ACE is `swap_buffers()` in engine/vesa.c, which IS the
 * engine's replacement for Doom's `I_FinishUpdate` (hooked at 0x0001D4A6 and
 * 0x0001D4FC). It runs at least once per frame, after the frame is rendered and
 * before it is presented. `P_Ticker` (0x0002081A) is the once-per-tic
 * alternative if a logic-only yield is wanted.
 *
 * CALL CONVENTION: `swap_buffers` is declared
 * `__attribute((regparm(2),no_caller_saved_registers))`, i.e. it promises not to
 * clobber eax/ecx/edx. mtx_frame_tick carries the same attribute so calling it
 * from there is contract-safe; gcc then preserves those registers around the
 * switch for us. Calling a plain C function from inside swap_buffers would break
 * Doom's assumption instead.
 */
#ifndef MATRYOSHKA_SCHED_H
#define MATRYOSHKA_SCHED_H

#include <stdint.h>
#include "context.h"

#define MTX_REQ_NONE   0u
#define MTX_REQ_YIELD  1u	/* go back to the parent depth */
#define MTX_REQ_SWITCH 2u	/* go to a named live depth */

/* Safe to call from anywhere, including interrupt-ish context: it only stores. */
void mtx_request_yield(void);
void mtx_request_switch(uint8_t depth);
void mtx_request_cancel(void);
int  mtx_request_pending(void);
uint8_t mtx_request_kind(void);

/* The yield point. Call once per frame (see the note above). Self-preserving,
   so it may be called from ACE's regparm/no_caller_saved hook targets. */
#define MTX_FRAME_TICK_ABI __attribute__((regparm(2), no_caller_saved_registers))
void MTX_FRAME_TICK_ABI mtx_frame_tick(void);

#endif /* MATRYOSHKA_SCHED_H */
