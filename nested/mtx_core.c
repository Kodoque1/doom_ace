/*
 * matryoshka/mtx_core.c
 *
 * Cooperative scheduler core: context stack, fresh-stack seeding, depth limits
 * and the save/load hand-off. Portable C only - no libc, no DOS, no engine
 * dependency - so the same object builds into ACE and into the host test.
 */
#include "context.h"
#include "mtx_host.h"

extern void mtx_trampoline(void);

/* Control state lives in mtx_hs (mtx_host.h) so it can be excluded from every
   guest snapshot - see the long comment there for why restoring it mid-switch
   is fatal. These aliases keep the code below readable. */
#define mtx_ctx        (mtx_hs.ctx)
#define mtx_cur        (mtx_hs.cur)
#define mtx_max        (mtx_hs.max)
#define mtx_ops        (mtx_hs.ops)
#define mtx_pool       (mtx_hs.stack_pool)
#define mtx_slot_words (mtx_hs.slot_words)

#ifndef MTX_DOS_NO_STATIC_POOL
static uint32_t mtx_static_pool[MTX_MAX_DEPTH * MTX_STACK_WORDS];
#endif

/* no-tree-loop-distribute-patterns: keep gcc from turning this into a memset()
   call - there is no libc inside ACE. */
static void __attribute__((optimize("no-tree-loop-distribute-patterns")))
mtx_zero(void *p, uint32_t n)
{
	uint8_t *b = (uint8_t *)p;
	while(n--)
		*b++ = 0;
}

void mtx_init(uint8_t max_depth, const mtx_host_ops_t *ops,
              void *stack_base, uint32_t stack_bytes)
{
	if(max_depth > MTX_MAX_DEPTH)
		max_depth = MTX_MAX_DEPTH;
	if(max_depth < 2)
		max_depth = 2;

	mtx_max = max_depth;

	/* Copy the table rather than keeping the caller's pointer. mtx_init is
	   normally called once, from an init path whose frame is long gone by the
	   first real switch - and a dangling table means the switch calls through
	   recycled stack memory. Owning a copy makes that impossible instead of
	   merely documented. */
	if(ops)
	{
		mtx_hs.host_ops = *ops;
		mtx_ops = &mtx_hs.host_ops;
	}
	else
	{
		mtx_hs.host_ops.save_state = 0;
		mtx_hs.host_ops.load_state = 0;
		mtx_ops = 0;
	}

	mtx_zero(mtx_ctx, (uint32_t)sizeof(mtx_ctx));
	mtx_cur = MTX_ROOT_DEPTH;
	mtx_ctx[MTX_ROOT_DEPTH].used  = 1;
	mtx_ctx[MTX_ROOT_DEPTH].depth = MTX_ROOT_DEPTH;

	if(stack_base && stack_bytes)
	{
		mtx_pool       = (uint32_t *)stack_base;
		mtx_slot_words = (stack_bytes / 4u) / max_depth;
	}
#ifndef MTX_DOS_NO_STATIC_POOL
	else
	{
		mtx_pool       = mtx_static_pool;
		mtx_slot_words = MTX_STACK_WORDS;
	}
#else
	else
	{
		/* no built-in pool: the caller must supply one. mtx_push fails closed. */
		mtx_pool       = 0;
		mtx_slot_words = 0;
	}
#endif
}

/* Lay out a brand-new depth's stack so the first mtx_switch_ctx on it pops the
   callee-saved slots, returns into mtx_trampoline, and finds its argument at
   (%esp). Low to high: edi, esi, ebx, ebp, trampoline, arg. */
static uint32_t *mtx_seed(uint8_t d, mtx_entry_t entry, void *arg)
{
	uint32_t *top = mtx_pool + (uint32_t)(d + 1u) * mtx_slot_words;
	uint32_t *sp  = top;

	*--sp = (uint32_t)(uintptr_t)arg;
	*--sp = (uint32_t)(uintptr_t)mtx_trampoline;
	*--sp = 0;	/* ebp */
	*--sp = 0;	/* ebx */
	*--sp = 0;	/* esi */
	*--sp = 0;	/* edi */

	mtx_ctx[d].entry = entry;
	mtx_ctx[d].arg   = arg;
	return sp;
}

/* The single switch point. save_state(outgoing) -> load_state(incoming) ->
   stack swap. Symmetric: the outgoing depth later resumes right after its own
   mtx_switch_ctx, with no per-site bookkeeping. */
static void mtx_switch(uint8_t d)
{
	uint8_t prev = mtx_cur;

	if(mtx_ops && mtx_ops->save_state)
		mtx_ops->save_state(&mtx_ctx[prev]);

	mtx_cur = d;

	if(mtx_ops && mtx_ops->load_state)
		mtx_ops->load_state(&mtx_ctx[d]);

	mtx_switch_ctx(&mtx_ctx[prev].esp, mtx_ctx[d].esp);
	/* resumed here when some depth switches back to `prev` */
}

int mtx_push(mtx_entry_t entry, void *arg)
{
	uint8_t d;

	if(!mtx_pool)
		return MTX_EDEPTH;		/* no stack pool: fail closed */

	/* THE FIRST SLOT THAT IS NOT LIVE, not `cur + 1`.
	 *
	 * `cur + 1` is right for depth-first nesting - a depth pushing its own child -
	 * which is how this was written, and it is still what happens for that case.
	 * It is wrong for SIBLINGS: a ring boots all of its levels from the root, so
	 * the second push happens while `cur` is the ROOT again, `cur + 1` names the
	 * slot the FIRST child already owns, and the second level silently re-boots
	 * inside the first one's slot. Measured exactly that: two "depth1 entered"
	 * lines with identical image, params, zone and screen addresses, and no depth 2
	 * anywhere in the log.
	 *
	 * "Not live" and not merely "not used", because context.h's contract is that a
	 * slot is REUSED when a depth returns: mtx_finish leaves `used` set, so a
	 * finished slot is free again and the old code relied on that by overwriting
	 * `cur + 1` anyway. Taking only `!used` broke exactly that and hung the host
	 * test - the loop simply never found the slot the test expected - so the rule
	 * has to say BOTH things.
	 *
	 * With slots allocated in order this is `cur + 1` in the nesting case, so
	 * depth-first behaviour cannot change; the host tests assert it. */
	for(d = 1; d < mtx_max; d++)
		if(!mtx_ctx[d].used || mtx_ctx[d].finished)
			break;
	if(d >= mtx_max)
		return MTX_EDEPTH;

	mtx_ctx[d].used     = 1;
	mtx_ctx[d].finished = 0;
	mtx_ctx[d].depth    = d;
	mtx_ctx[d].esp      = mtx_seed(d, entry, arg);

	mtx_switch(d);
	return MTX_OK;
}

void mtx_yield(void)
{
	if(mtx_cur > MTX_ROOT_DEPTH)
		mtx_switch((uint8_t)(mtx_cur - 1u));
}

void mtx_switch_to(uint8_t depth)
{
	if(depth >= mtx_max)
		return;
	if(depth == mtx_cur)
		return;
	if(!mtx_ctx[depth].used)
		return;
	if(depth != MTX_ROOT_DEPTH && mtx_ctx[depth].finished)
		return;

	mtx_switch(depth);
}

void mtx_finish(void)
{
	uint8_t d = mtx_cur;

	mtx_ctx[d].finished = 1;

	if(d == MTX_ROOT_DEPTH)
		for(;;) { }				/* root has no parent */

	mtx_switch((uint8_t)(d - 1u));

	for(;;) { }					/* a finished depth is never resumed */
}

/* Reached via mtx_trampoline (mtx_switch.S) on a fresh depth. Never returns. */
void mtx_run_entry(void *arg)
{
	mtx_context_t *c = &mtx_ctx[mtx_cur];

	if(c->entry)
		c->entry(arg);
	mtx_finish();
}

uint8_t  mtx_depth(void)      { return mtx_cur; }
uint8_t  mtx_max_depth(void)  { return mtx_max; }
void   *mtx_arg(void)        { return mtx_ctx[mtx_cur].arg; }
void   *mtx_state(void)      { return mtx_ctx[mtx_cur].state; }

int mtx_alive(uint8_t depth)
{
	if(depth >= mtx_max || !mtx_ctx[depth].used)
		return 0;
	if(depth != MTX_ROOT_DEPTH && mtx_ctx[depth].finished)
		return 0;
	return 1;
}

/* Diagnostics only: lets an engine-side probe assert that a suspended depth's
   saved stack pointer is still sane. Not used by the scheduler itself. */
void   *mtx_debug_esp(uint8_t depth) { return (depth < mtx_max) ? mtx_ctx[depth].esp : 0; }

/* Diagnostics only: the save/load table the scheduler will actually call.
   Used by the regression test that mtx_init copies its argument. */
const void *mtx_debug_ops(void) { return (const void *)mtx_ops; }
