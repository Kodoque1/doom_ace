/*
 * matryoshka/mtx_params.h
 *
 * The block a nested ENGINE instance is entered with: everything an engine image
 * copy cannot work out for itself.
 *
 * WHY IT IS NEEDED
 * ----------------
 * The guest stub reaches the host through its service table, so it never has to
 * know anything. A real engine image is different: it is a COPY of the engine,
 * relocated into its own arena slot, with its OWN `.bss`. So `mtx_hs` - the
 * host's control state, one object in the root's engine .bss, deliberately
 * excluded from every snapshot - is a different object inside a nested instance,
 * and it is zero there. Measured consequence: the instance's `ldr_malloc`
 * consulted an empty `mtx_hs`, decided it was the root, asked DPMI for 209,184 B
 * and died at init_render() with only 61,440 B left - while its own raw pool sat
 * unused two addresses away.
 *
 * A nested instance cannot read the host's state, and the host cannot rely on
 * the instance's state being anything. So the host hands it a POINTER to this
 * block, which lives in the HOST's `mtx_hs` (stable across switches, and visible
 * to the host's own reporting): the instance reads its depth, its zone, its
 * screen and its raw pool through one pointer, and every byte of accounting
 * stays in host memory where the host can see it.
 *
 * HOW IT ARRIVES, AND WHY THAT WAY
 * -------------------------------
 * In %ebp, at the entry, and the engine's own `_start` stores it into a global
 * immediately after clearing `.bss`. Two reasons for a register rather than a
 * memory location the host writes:
 *
 *  - A register needs no address arithmetic. Working out where an instance's
 *    global lives would mean mapping its image layout onto its slot, and the
 *    loaders' placement is not something to guess at. `_start` already takes its
 *    bases in ESI/EDI and writes them into its own relocated globals; EBP rides
 *    the same, proven channel. It is the one register the entry does not consume.
 *  - The `.bss` clear happens at the entry, so anything the host wrote into the
 *    instance's `.bss` beforehand would be wiped. A register survives it.
 *
 * %ebp is untrustworthy at a ROOT boot (the ACE loader leaves whatever it likes
 * there), so the entry validates before storing: the magic word AND the block's
 * own address inside it. A root therefore ends up with a null block, which is
 * exactly the signal that this copy of the engine IS the host.
 */
#ifndef MATRYOSHKA_PARAMS_H
#define MATRYOSHKA_PARAMS_H

#include <stdint.h>
#include "mtx_raw.h"

#define MTX_PARAMS_MAGIC 0x4D545850u	/* 'MTXP' */

typedef struct {
	uint32_t   magic;	/* MTX_PARAMS_MAGIC */
	uint32_t   self;	/* == the address of this block: the second check */
	uint32_t   depth;	/* which depth this instance runs at */
	int32_t    log_fd;	/* the host's MTX.LOG descriptor; descriptors are
	                           process-wide, so the instance writes into the
	                           SAME log rather than opening its own */
	void      *zone;	/* this depth's zone (heap) */
	uint32_t   zone_bytes;
	void      *screen;	/* this depth's framebuffer */
	mtx_raw_t *raw;		/* this depth's raw pool, in HOST memory */
	uint32_t   frames;	/* the instance's own frame count, written HERE, in
	                           host memory, by the instance itself. Its copy of
	                           the engine reaches the frame boundary too (every
	                           depth patches the same Doom code), so this is how
	                           an instance says "I am drawing" without a service
	                           table or a report back: the host reads it as the
	                           difference between an instance that booted and one
	                           that boots and paints. */
	void     (*host_yield)(void);	/* the HOST's frame service, for a depth that
	                           wants to hand the frame back. It cannot switch with
	                           its own copy of the scheduler - the contexts live
	                           in the host's state object, and this pointer's code
	                           is the host's, so calling it is calling the host.
	                           It returns when the host hands the frame back to
	                           this depth again. */
	uint32_t   switch_req;	/* the switch key, seen at THIS depth. Doom's key state
	                           is per-depth, so a press that lands during this
	                           depth's slice is invisible to the host; the depth
	                           that saw it records the request here, in host
	                           memory, and the host consumes it. */
	uint32_t   key_down;	/* this depth's own view of the key, so one press is one
	                           switch rather than one per frame it is held */
	uint32_t   slice_gen;	/* incremented by a depth when ITS slice starts. The
	                           host uses it as the same guard on its own side: a
	                           slice boundary must not be read as a key press (see
	                           MTX_KEY_GUARD_FRAMES in the engine). */
	uint32_t   canvas;	/* the buffer the picture on screen is built in. It is
	                           the ROOT's framebuffer, and BOTH depths present it
	                           (present_src), so whichever one runs is the one that
	                           keeps it up to date. */
	uint32_t   inset_on;	/* mirrors the host's inset flag. With the inset on,
	                           the running depth paints its own live view into the
	                           canvas - otherwise the screen shows the root's
	                           unchanged buffer for the whole of a depth's slice,
	                           which measured as 3.9 s of a completely still
	                           screen. The inset rectangle itself is left alone by
	                           the running depth: it belongs to the host's
	                           composite, and overwriting it would blink it off. */
	uint32_t   present_src;	/* what this depth's frame blit should copy FROM. The
	                           engine bakes that source into its own code at init
	                           (the value of `screen_buffer` then), and each depth
	                           has its OWN copy of that code - so a depth can point
	                           its own blit at any buffer, and the HOST says which.
	                           Both depths pointing at the root's frame is what
	                           makes the screen stable: the depth that is running
	                           renders into its own buffer and presents the root's,
	                           so the nested picture reaches the screen only where
	                           the host puts it - in the inset. */
	uint32_t   screen_src;	/* what this depth's SCREEN-WALLS show: the frame of
	                           the NEXT live depth, and at the deepest depth the
	                           wrap back to the root. Appended at the END of the
	                           block deliberately - every field above keeps its
	                           offset, so a purpose-built guest compiled against
	                           the older layout is unaffected. Without the wrap the
	                           chain is a ladder whose last rung shows nothing;
	                           with it the levels close into a cycle, and each
	                           level's picture contains the whole nest, one lap
	                           older per frame. */
} mtx_depth_params_t;

#endif /* MATRYOSHKA_PARAMS_H */
