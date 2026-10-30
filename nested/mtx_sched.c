/*
 * matryoshka/mtx_sched.c - request/consume, serviced at the frame boundary.
 */
#include "mtx_sched.h"
#include "mtx_host.h"

/* Pending request. In mtx_hs so a guest snapshot restore cannot re-arm a
   request that was already consumed at a frame boundary. */
#define sched_kind  (mtx_hs.req_kind)
#define sched_depth (mtx_hs.req_depth)

void mtx_request_yield(void)
{
	sched_depth = 0;
	sched_kind = MTX_REQ_YIELD;
}

void mtx_request_switch(uint8_t depth)
{
	sched_depth = depth;
	sched_kind = MTX_REQ_SWITCH;
}

void mtx_request_cancel(void)
{
	sched_kind = MTX_REQ_NONE;
}

int mtx_request_pending(void)   { return sched_kind != MTX_REQ_NONE; }
uint8_t mtx_request_kind(void)  { return sched_kind; }

void MTX_FRAME_TICK_ABI mtx_frame_tick(void)
{
	uint8_t k = sched_kind;

	if(k == MTX_REQ_NONE)
		return;

	/* Consume BEFORE switching: neither the incoming depth nor this one when it
	   is resumed later may re-service the same request. */
	sched_kind = MTX_REQ_NONE;

	if(k == MTX_REQ_YIELD)
		mtx_yield();
	else if(k == MTX_REQ_SWITCH)
		mtx_switch_to(sched_depth);
	/* a request for a depth that is not live is silently dropped by
	   mtx_switch_to, which is the right behaviour at a frame boundary */
}
