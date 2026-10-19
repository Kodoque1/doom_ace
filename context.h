/*
 * matryoshka/context.h
 *
 * Nested-instance host for the ACE engine ("Doom inside Doom").
 *
 * Goal: N live engine instances coexisting inside ONE DOOM2.EXE, switched at
 * runtime, never re-launching the executable. Depth 0 is the root instance and
 * owns the real hardware. Depth d+1 runs inside depth d.
 *
 * This header is the interface only. The mechanism is a cooperative context
 * switch (save ESP/EIP+callee-saved regs, swap stacks) plus a per-depth
 * save/load of machine-visible shared state (Doom globals, ACE hook table,
 * active video buffer). The scheduler core (mtx_core.c) is deliberately free of
 * any DOS or engine dependency so it can be exercised on the host; the DOS
 * wiring plugs in through mtx_host_ops_t.
 *
 * Depth semantics: depth index == slot index. Slots are reused when a depth
 * returns. The practical ceiling in DOSBox is 12; raise MTX_MAX_DEPTH only with
 * the memory budget in mind (every live depth costs a stack + a state snapshot).
 */
#ifndef MATRYOSHKA_CONTEXT_H
#define MATRYOSHKA_CONTEXT_H

#include <stdint.h>

#define MTX_MAX_DEPTH   12u                                 /* live-depth ceiling */
#define MTX_STACK_SIZE  8192u                               /* per-depth stack, bytes */
#define MTX_STACK_WORDS (MTX_STACK_SIZE / 4u)

#define MTX_ROOT_DEPTH  0u

/* return codes */
#define MTX_OK          0
#define MTX_EDEPTH     -1   /* would exceed MTX_MAX_DEPTH */
#define MTX_EBADSLOT   -2   /* target slot is not a live depth */

typedef void (*mtx_entry_t)(void *arg);

typedef struct mtx_context {
	uint32_t     *esp;      /* saved stack pointer; NULL on an unused slot */
	mtx_entry_t   entry;    /* child entry point (NULL for the root) */
	void         *arg;
	void         *state;    /* DOS layer's per-depth payload (snapshot, hooks, video) */
	uint8_t       used;
	uint8_t       finished;
	uint8_t       depth;
} mtx_context_t;

/*
 * Called around every switch: save_state(outgoing) then load_state(incoming).
 * The host never interprets `state`; the DOS layer does:
 *   save_state: memcpy the live Doom/engine globals into ctx->state,
 *               push the current ACE hook table,
 *               remember which video buffer this depth owns.
 *   load_state: restore the same three for the incoming depth.
 * Hook isolation and global snapshotting therefore live entirely here, not in
 * the scheduler.
 */
typedef struct mtx_host_ops {
	void (*save_state)(mtx_context_t *outgoing);
	void (*load_state)(mtx_context_t *incoming);
} mtx_host_ops_t;

/*
 * Initialise the host. stack_base/stack_bytes describe a caller-owned pool used
 * for the per-depth stacks (in DOS this is zone memory; pass NULL/0 to use the
 * built-in static pool, which is fine for the host test).
 */
void     mtx_init(uint8_t max_depth, const mtx_host_ops_t *ops,
                  void *stack_base, uint32_t stack_bytes);

/* Create the next depth and switch into it. Returns MTX_OK once the child has
   returned (execution resumes after the push), or an error if it could not be
   created. */
int      mtx_push(mtx_entry_t entry, void *arg);

void     mtx_yield(void);            /* hand control back to the parent depth */
void     mtx_switch_to(uint8_t depth); /* hand control to any live depth */
void     mtx_finish(void);           /* current depth is done; never returns */

uint8_t  mtx_depth(void);
uint8_t  mtx_max_depth(void);
void    *mtx_arg(void);
void    *mtx_state(void);            /* current depth's opaque DOS payload */

/* Is this slot a depth that can still be switched to? False for an unused or
   already-finished slot, so a caller does not keep requesting a dead depth. */
int      mtx_alive(uint8_t depth);

/* Diagnostics: saved stack pointer of a suspended depth (NULL if none). */
void    *mtx_debug_esp(uint8_t depth);
/* Diagnostics: the save/load table the scheduler will actually call. */
const void *mtx_debug_ops(void);

/* Low level primitive, implemented in mtx_switch.S:
   stores the current ESP into *save_esp, loads load_esp, restores callee-saved
   registers and returns on the loaded stack. */
void     mtx_switch_ctx(uint32_t **save_esp, uint32_t *load_esp);

#endif /* MATRYOSHKA_CONTEXT_H */
