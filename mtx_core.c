/*
 * matryoshka/mtx_core.c
 *
 * Cooperative scheduler core: context stack, fresh-stack seeding, depth limits
 * and the save/load hand-off. Portable C only - no libc, no DOS, no engine
 * dependency - so the same object builds into ACE and into the host test.
 */
#include "context.h"

extern void mtx_trampoline(void);

static mtx_context_t         mtx_ctx[MTX_MAX_DEPTH];
static uint8_t               mtx_cur;
static uint8_t               mtx_max;
static const mtx_host_ops_t *mtx_ops;
static uint32_t             *mtx_pool;
static uint32_t              mtx_slot_words;   /* words per live depth */

static uint32_t mtx_static_pool[MTX_MAX_DEPTH * MTX_STACK_WORDS];

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
	mtx_ops = ops;

	mtx_zero(mtx_ctx, (uint32_t)sizeof(mtx_ctx));
	mtx_cur = MTX_ROOT_DEPTH;
	mtx_ctx[MTX_ROOT_DEPTH].used  = 1;
	mtx_ctx[MTX_ROOT_DEPTH].depth = MTX_ROOT_DEPTH;

	if(stack_base && stack_bytes)
	{
		mtx_pool       = (uint32_t *)stack_base;
		mtx_slot_words = (stack_bytes / 4u) / max_depth;
	}
	else
	{
		mtx_pool       = mtx_static_pool;
		mtx_slot_words = MTX_STACK_WORDS;
	}
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
	uint8_t d = (uint8_t)(mtx_cur + 1u);

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

uint8_t mtx_depth(void)      { return mtx_cur; }
uint8_t mtx_max_depth(void)  { return mtx_max; }
void   *mtx_arg(void)        { return mtx_ctx[mtx_cur].arg; }
void   *mtx_state(void)      { return mtx_ctx[mtx_cur].state; }
