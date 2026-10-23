/*
 * matryoshka/mtx_host.h
 *
 * The hypervisor's own control state, in ONE object.
 *
 * WHY THIS FILE EXISTS
 * --------------------
 * Every guest (depth) snapshot restores the engine's whole writable .bss on
 * switch-in. The host's scheduler state lives in that same .bss, so if it were
 * covered by a snapshot the restore would overwrite the very variables that are
 * mid-switch. The order in mtx_switch(prev -> d) is:
 *
 *   1. save_state(prev)   copies the live .bss into prev's snapshot slot
 *   2. mtx_cur = d        a control write
 *   3. load_state(d)      copies d's slot back over the live .bss
 *   4. mtx_switch_ctx(&ctx[prev].esp, ctx[d].esp)
 *
 * At step 3 the restore puts ctx[prev].esp back to the value captured before
 * step 4 could publish the real one, so step 4 loads ESP = 0 and the primitive's
 * final `ret` jumps through address 0. Same story for inited[] (re-seeding a
 * slot after its ESP was published loses the ESP), pool/per_depth, and the
 * engine-side booted flag (a restored 0 re-runs mtx_dos_boot mid-frame).
 *
 * Measured failure without this split: DOSBox "Can't run code in this page!"
 * plus a flood of illegal reads/writes at CS:IP 170:4d4d4d4d ("MMMM"), on the
 * very first switch, where the HEAD baseline has zero.
 *
 * So the invariant is: this object must lie OUTSIDE every registered snapshot
 * region. dos_wire.c enforces it by splitting the engine's .bss region around
 * mtx_hs, and refuses to nest at all if the block is not a single contiguous
 * object inside .bss.
 *
 * The big storage pools (per-depth stacks, snapshot arena, static test pools)
 * are NOT here on purpose: they are caller-owned memory or storage rather than
 * control state, and they never move during a switch.
 */
#ifndef MATRYOSHKA_HOST_H
#define MATRYOSHKA_HOST_H

#include <stdint.h>
#include "context.h"
#include "dos_state.h"
#include "ldr0n.h"
#include "ldr1n.h"

/* A registered snapshot region: where it lives and (assigned by mtx_layout)
   where it sits inside a depth's snapshot slot. */
typedef struct { void *base; uint32_t size; uint32_t off; } mtx_dregion_t;
/* A patched location (an ACE hook), saved as raw bytes. */
typedef struct { void *addr; uint32_t len;  uint32_t off; } mtx_dhook_t;

typedef struct mtx_host_state {
	/* --- mtx_core.c: scheduler control --- */
	mtx_context_t         ctx[MTX_MAX_DEPTH];
	uint8_t               cur;
	uint8_t               max;
	const mtx_host_ops_t *ops;      /* -> host_ops below, never a caller local */
	uint32_t             *stack_pool;
	uint32_t              slot_words;

	/* The save/load table. mtx_init() COPIES whatever it is handed into here
	   and calls this copy on every later switch, so a caller may register from
	   a short-lived frame. Retaining the caller's pointer instead was a real
	   bug: the init frame is long gone by the first switch, so the call went
	   through recycled stack memory - measured in DOSBox as "exception 06h
	   (invalid opcode)" at a garbage EIP, with ESP still on the root's stack
	   because the bad call happens before the stack swap. */
	mtx_host_ops_t        host_ops;

	/* --- mtx_sched.c: the pending frame-boundary request --- */
	volatile uint8_t      req_kind;
	volatile uint8_t      req_depth;

	/* --- dos_state.c: the snapshot registry (control state only) --- */
	mtx_dregion_t         rgn[MTX_DOS_MAX_REGIONS];
	mtx_dhook_t           hok[MTX_DOS_MAX_HOOKS];
	int32_t               nrgn, nhok;
	void                 *pin_addr[MTX_PIN_COUNT];
	void                 *pin_val[MTX_MAX_DEPTH][MTX_PIN_COUNT];
	uint8_t              *snap_pool;
	uint32_t              snap_per_depth;
	uint8_t               inited[MTX_MAX_DEPTH];
	uint8_t               maxd;

	/* --- mtx_boot.c: engine-side glue. Declared here so it is excluded from
	       every guest snapshot too; a restored `booted` would re-boot the host
	       from inside a frame. --- */
	uint8_t               booted, boot_failed, child_live;
	uint8_t               present_depth;	/* depth the screen currently shows */
	uint32_t              present_frame;	/* frame the source changed (0 = none) */
	uint32_t              frame_no;
	int32_t               log_fd;
	void                 *arena;
	uint32_t              arena_size;
	void                 *heaps[MTX_MAX_DEPTH];
	void                 *screens[MTX_MAX_DEPTH];

	/* --- mtx_ldr.c: loader state. Also host control state, for the same
	       reason: a guest snapshot must never cover it. --- */
	void                 *images[MTX_MAX_DEPTH];
	uint32_t              image_bytes;
	mtx_ldr1n_ops_t       ldr_ops;
	mtx_ldr0n_desc_t      ldr0_desc;
	uint32_t              ldr_svc[5];	/* the guest's service table */
	uint32_t              ldr_report[4];	/* what the loaded image reported */
	uint32_t              ldr_guest_entry;
	uint32_t              ldr_guest_total;
	uint32_t              ldr_ace_total;
	uint32_t              ldr_ace_bss;
	uint32_t              ldr_ace_ver;
	uint8_t               ldr_guest_ok;
	uint8_t               ldr_ace_ok;
} mtx_host_state_t;

extern mtx_host_state_t mtx_hs;

#endif /* MATRYOSHKA_HOST_H */
