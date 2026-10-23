// kgsws' ACE Engine / matryoshka
////
// Engine-side glue for the nested-instance host: boot it once, on the first
// frame, with real memory, then drive a small auto-switch policy so the
// machinery is live rather than inert.
//
// Boot is deferred to the first frame on purpose: by then the engine has taken
// over, Z_Init has run and screen_buffer is known, so the snapshot taken for
// depth 0 is a fully initialised frame state - which is the correct thing for a
// nested instance to inherit.
//
// A depth no longer runs a hand-written stub: it loads a real code image out of
// the WAD, into memory of its own, and runs it - through the reentrant loaders
// (ldr0N announces and hands off, ldr1n does the reading). See mtx_ldr.c.
//
// Evidence goes to a file (MTX.LOG) rather than stdout: once the engine takes
// over, Doom's stdout redirection stops being captured, so printf is not a
// usable probe. The engine's own doom_open/doom_write are.
//
// The policy here is a demo stand-in for real input. It switches into depth 1
// every MTX_DEMO_INTERVAL frames; binding this to a key is the remaining work.

#include "sdk.h"
#include "engine.h"
#include "doom_vars.h"
#include "utils.h"

#include "context.h"
#include "dos_state.h"
#include "dos_alloc.h"
#include "dos_wire.h"
#include "mtx_sched.h"
#include "mtx_host.h"
#include "mtx_ldr.h"
#include "mtx_zone.h"
#include "mtx_hooks.h"
#include "mtx_params.h"
#include "mtx_boot.h"		// own declarations: the diagnostics below take &mtx_engine_alloc

/* A depth's ZONE. Was 256 KiB, and that is why a nested instance died in
   Z_Malloc with "failed on allocation of 29164 bytes" the moment its texture
   lookups started really reading lumps: Doom's own V_Init allocates its screens
   (192 KB of the 256 right there), and after that a 28 KB patch lump has nowhere
   to go. The root's own zone is ~8 MiB (Doom's default request), so a depth that
   is meant to run a real instance cannot be two orders of magnitude smaller.
   4 MiB is the starting figure; the host reports what the depth actually uses,
   so it gets confirmed or corrected from a live run. */
#define MTX_ZONE_PER_DEPTH  (4u * 1024u * 1024u)
/* A child's raw (non-zone) pool. See mtx_raw.h and the raw-allocation block
   further down: the zone was half of what a depth needs, this is the other
   half. 2 MiB against a measured 209,184 B single request - the pool logs its
   peak and the size of any refusal, so the figure is checked by a live run. */
#define MTX_RAW_PER_DEPTH   (2u * 1024u * 1024u)
/* SLICE LENGTH, in frames of the level that is running.
 *
 * 1 was the value for a two-body ping-pong, and it silently stops the switch key from
 * working in a RING. Measured, three levels, interval 1: not one switch line in a
 * 30 s run. The reason is the guards: both sides ignore the key for the first
 * MTX_KEY_GUARD_FRAMES frames of a slice (the restored snapshot key state reads as a
 * fresh press), and at a one-frame slice EVERY frame is a slice start, so the dead
 * window is permanent on both sides - the child never checks the key, and the root's
 * guard is re-armed by a slice start every frame.
 *
 * 4 gives each level four frames in a row, so the guard fits INSIDE the slice and
 * there are three frames left to see a press. It is also what gives a level a run of
 * frames rather than one frame per lap, which is what makes its own motion visible.
 * Measured in the two-depth days: at 4-frame slices the outer view ran 13.8/s and a
 * depth's own animation 4.4/s; at 1 frame, 7.3/s and 8.7/s - so 4 is the better outer
 * view and 1 the better inner one. A ring pays a third of either per level, which is
 * the price of three levels instead of two. */
#define MTX_DEMO_INTERVAL   4u

/* HOW MANY CHILDREN THE RING BOOTS. Two children make a three-level nest - the root
   plus two instances - which is what the ring needs to be a ring: with one child it
   is the two-body ping-pong it always was, and the wrap has nothing to close. Each
   child costs a full arena slot (see mtx_engine_reserve), so this number and the
   arena size are ONE decision, not two. */
#define MTX_RING_CHILDREN   2u

/* Frames at the start of a slice during which the switch key is ignored, on both
   sides. Doom's key array lives in Doom's data, so it is snapshot state: a
   switched-in depth restores the value it last saw, and without this window that
   restored value reads as a press on the first frame of every slice (measured:
   one held key toggling the inset once per slice). 3 frames is a fraction of the
   shortest human press and a small fraction of a slice. */
/* The guard scales with the slice: it exists to ignore the frames a slice starts on,
   so a fixed 3 frames would swallow a 3-frame slice whole and no press would ever
   be seen. A quarter of the slice, never less than one frame. */
#define MTX_KEY_GUARD_FRAMES ((MTX_DEMO_INTERVAL / 4u) ? (MTX_DEMO_INTERVAL / 4u) : 1u)

/* Dead time after a toggle, in host frames, during which no new edge is honoured.
   This is what makes ONE press one switch once the key state is being distrusted
   across slices: a key that is still physically held RE-ASSERTS itself through a
   depth's own key processing a few frames into the slice (DOS typematic repeats),
   and without a dead time that re-assertion reads as a fresh press - measured as
   ON at 469 and OFF at 484 from a single 1 s press. ~1.7 s at the rates seen here,
   which is longer than a typematic re-assertion and shorter than a deliberate
   second press would be. */
#define MTX_KEY_DEBOUNCE_FRAMES 30u

/* TEMPORARY (measurement): stack painting, to size MTX_STACK_SIZE from a number.
   See the code that paints and the code that scans it, further down. */
#define MTX_STACK_PAINT 0xC5C5C5C5u
static uint32_t mtx_stack_lo, mtx_stack_hi;
static uint32_t mtx_stack_painted;

/* Which code image depth 1 runs.
 *
 *  0 = the purpose-built guest. Proves the loader, the zone, the screen and the
 *      isolation paths without depending on an engine instance coming up. Shipped as
 *      the default, because it is the one that works.
 *  1 = the REAL engine image. The same loader and the same depth, but the entry is
 *      ACE's own `_start`, which relocates itself against the address it is handed,
 *      clears ITS OWN .bss (this depth's slot, not the root's) and calls ace_main.
 *
 * Set to 1 and the attempt gets a long way - see the README's "Entering the real
 * engine image" section. Measured state of it: the image loads, is entered with the
 * right register contract, and ace_main runs at depth 1 all the way into
 * init_render(), where the first LARGE raw allocation fails:
 *
 *     free DPMI after root Z_Init: 61440 B
 *     LOADER: Render memory allocation failed! (209184B)
 *
 * The root's Z_Init consumes all of DPMI except a 32 KiB reserve (60 KiB measured), so
 * a depth has no source for its raw - non-zone - allocations. The zone is per-depth;
 * the engine's doom_malloc is not. That is the next piece of work, not a bug in this
 * switch. */
#define MTX_CHILD_ENGINE_IMAGE 1

/* The code images the host loads. Both are reachable by name through ACE's own
   WAD layer - the descriptors LDR1 threw away on first boot were Doom's, and ACE
   rebuilt its own, which is what makes this lookup non-destructive. */
#define MTX_GUEST_LUMP  ((const uint8_t *)"MTXGUEST")
#define MTX_ACE_LUMP    ((const uint8_t *)"ACE_CODE")

/* Engine-side glue state. In mtx_hs so it is excluded from every guest
   snapshot: a restored `booted`/`frame_no`/`log_fd` would re-boot the host or
   truncate the log from inside a guest's frame. */
#define mtx_booted      (mtx_hs.booted)
#define mtx_boot_failed (mtx_hs.boot_failed)
#define mtx_child_live  (mtx_hs.child_live)
#define mtx_frame_no    (mtx_hs.frame_no)
#define mtx_log_fd      (mtx_hs.log_fd)
#define mtx_arena       (mtx_hs.arena)
#define mtx_arena_size  (mtx_hs.arena_size)
#define mtx_heaps       (mtx_hs.heaps)
#define mtx_screens     (mtx_hs.screens)

/*
 * ---- whose copy of the engine is this? -------------------------------------
 *
 * `mtx_hs` is the host's control state: one object in the ROOT engine's .bss,
 * excluded from every guest snapshot. A nested ENGINE instance is a relocated
 * COPY of the engine with its own .bss, so its `mtx_hs` is a different, empty
 * object - measured, and the reason a nested instance used to ask DPMI for its
 * raw memory while its own pool sat unused.
 *
 * The way out is the parameter block the host hands over in %ebp at the entry,
 * which the engine's own `_start` stores here (asm.S) after validating it. So:
 *
 *   mtx_params == 0   -> this IS the host. The depth is whatever the scheduler
 *                        is running, which is also correct for the GUEST path:
 *                        a guest reaches the host through its service table, so
 *                        it runs HOST code, and the host's scheduler knows.
 *   mtx_params != 0   -> a nested instance, with its own depth, zone, screen,
 *                        pool and log descriptor - all of them in host memory,
 *                        so the host can see the same accounting the instance
 *                        uses.
 *
 * THE BLOCK POINTER LIVES IN .data, NOT .bss, and that is not cosmetic. Measured:
 * an instance's .bss is cleared MORE THAN ONCE during its boot - the engine's own
 * entry is re-reached while the boot continues into Doom's code - and a pointer
 * held in .bss is wiped by the later clear, so the instance forgets what it is
 * halfway through starting up. .data survives it (which is also why `mainzone`,
 * a Doom variable, was still correct while this was not). The entry only ever
 * writes it when validation passes, so a re-entry with a stale %ebp leaves the
 * good value alone. `mtx_starts` counts the entries: the number is in the log.
 */
__attribute__((section(".data"))) void *mtx_params;
__attribute__((section(".data"))) uint32_t mtx_starts;

/* The ESP this image was FIRST entered on, captured before anything can move it.
   For a nested instance that is the stack the scheduler seeded for it (its own
   arena slot), and `mtx_engine_stack` hands it back to the entry so the boot
   continues on the depth's stack instead of on the root's. In .data for the same
   reason as `mtx_params` above: a later .bss clear must not lose it. */
__attribute__((section(".data"))) uint32_t mtx_entry_esp;

static mtx_depth_params_t *mtx_self(void);
static void mtx_log(const char *s);
static uint8_t mtx_glue_depth(void);

/*
 * THE RING: which framebuffer the screen-walls show.
 *
 * A level shows the NEXT live depth, and the deepest level wraps back to the root -
 * the modulo IS the wrap. That is what turns the nest into a cycle: without it the
 * ladder's last rung has nothing to show but blankness, and with it every level's
 * picture contains the whole nest, one lap older per frame.
 *
 * With two live depths this is EXACTLY the rule that was hardcoded here before (root
 * -> screens[1], a depth -> the root), which is what makes the change checkable
 * rather than a matter of opinion: the diegetic log of a two-depth run has to come
 * out identical, and it does (verify/trial_levels.sh compares them).
 *
 * A depth cannot read mtx_hs.screens[] itself: that array lives in the HOST's state
 * object and a nested instance has its own copy of that object, empty. So the host
 * hands it the pointer through the parameter block (screen_src), exactly the way it
 * already hands it the presentation buffer.
 */
static uint8_t mtx_ring_next(uint8_t d)
{
	uint8_t k;

	for(k = 1; k < MTX_MAX_DEPTH; k++)
	{
		uint8_t c = (uint8_t)((d + k) % MTX_MAX_DEPTH);

		if(mtx_alive(c))
			return c;
	}
	return d;                       /* nothing else live: nothing to show */
}

uint8_t *mtx_world_screen(void)
{
	mtx_depth_params_t *p = mtx_self();
	uint8_t             next;

	if(p)
		return (uint8_t *)(uintptr_t)p->screen_src;

	next = mtx_ring_next(MTX_ROOT_DEPTH);
	if(next == MTX_ROOT_DEPTH)
		return 0;               /* no child yet: an empty screen, not garbage */
	return (uint8_t *)mtx_hs.screens[next];
}

/*
 * DIEGETIC RENDERING: one wall texture IS the other instance's screen.
 *
 * Two halves, on purpose. mtx_screen_seg_note (called from the seg loop) only
 * RECORDS the screen rectangle of a screen-textured wall; mtx_screen_paint (called
 * from the frame hook, once the frame is finished) fills it with the other
 * instance's framebuffer. The first version drew columns from inside the seg loop,
 * where Doom's clipping state is mid-flight, and a ylookup[] indexed with an
 * unclipped row is a wild address: measured, 112 writes outside video memory and a
 * black screen. Recording is one comparison per column; painting has every bound in
 * one place.
 */
static uint8_t  mtx_seg_top[320];
static uint8_t  mtx_seg_bot[320];
static uint8_t  mtx_seg_mark[320];
static uint32_t mtx_seg_cols;

void mtx_screen_seg_note(uint32_t x, uint32_t yl, uint32_t yh)
{
	/* Doom's clipping arithmetic is INPUT here, not something already validated. */
	if(x >= 320u || yl > 199u || yh > 199u || yh < yl)
		return;
	mtx_seg_top[x] = (uint8_t)yl;
	mtx_seg_bot[x] = (uint8_t)yh;
	mtx_seg_mark[x] = 1;
	mtx_seg_cols++;
}

/*
 * Paint the recorded rectangle with the other instance's frame.
 *
 * The whole 320x200 frame is mapped across the wall (a screen shows its picture; it
 * does not tile its texture), with the horizontal span taken from the marked columns
 * so a wall that is partly off-screen still maps correctly. No colormap: a screen is
 * emissive, and one that darkened with distance would read as a texture rather than
 * as a picture. Writes go to THIS copy's framebuffer, so a depth's own screens show
 * up inside the inset that composites its frame - the nesting is visible from both
 * sides.
 */
void mtx_screen_paint(void)
{
	const uint8_t *src = mtx_world_screen();
	uint32_t x, y, x0 = 0xFFFFFFFFu, x1 = 0u, span, done = 0;

	if(!mtx_seg_cols)
		return;

	for(x = 0; x < 320u; x++)
		if(mtx_seg_mark[x])
		{
			if(x < x0)
				x0 = x;
			x1 = x;
		}

	if(src && x1 >= x0)
	{
		span = x1 - x0 + 1u;
		for(x = x0; x <= x1; x++)
		{
			uint32_t u, top, h, vstep, vfrac;

			if(!mtx_seg_mark[x])
				continue;

			u = ((x - x0) * 320u) / span;
			if(u > 319u)
				u = 319u;

			top = mtx_seg_top[x];
			h = (uint32_t)mtx_seg_bot[x] - top + 1u;
			vstep = (200u << 16) / h;
			vfrac = 0;

			for(y = 0; y < h; y++)
			{
				uint32_t v = vfrac >> 16;

				if(v > 199u)
					v = 199u;
				if(screen_buffer && (top + y) < 200u)
					((uint8_t *)screen_buffer)[(top + y) * 320u + x] = src[v * 320u + u];
				vfrac += vstep;
				done++;
			}
		}
	}

	for(x = 0; x < 320u; x++)
		mtx_seg_mark[x] = 0;
	mtx_seg_cols = 0;

	/* Evidence, sampled: a demo must not be claimable from a log that only says the
	   code exists. */
	{
		static uint32_t frames;
		uint8_t buf[160];

		frames++;
		if(frames <= 3u || (frames % 300u) == 0u)
		{
			doom_sprintf(buf, (const uint8_t *)
			             "[MTX] diegetic: depth %u painted texture %u over screen x %u..%u "
			             "(%u pixels) [frame %u]\n",
			             mtx_glue_depth(), MTX_SCREEN_TEX,
			             x0 == 0xFFFFFFFFu ? 0u : x0, x1, done, frames);
			mtx_log((const char *)buf);
		}
	}
}

/*
 * Called from the engine's entry (asm.S) with the value it was handed in %ebp,
 * before ace_main runs. See mtx_params.h for what the block is and asm.S for why
 * the store happens here rather than there.
 *
 * A ROOT boot reaches this too, with whatever the ACE loader left in %ebp (a
 * Doom code address, since that is what the loader last put there) - and the
 * block can NOT be recognised by address: the blocks live in the HOST's mtx_hs,
 * while a nested instance's own `&mtx_hs` is a different object at a different
 * address, so a range test against it would reject the very block being handed
 * over. Measured, not assumed. So the test is on the block's own contents, and
 * it is two fields that must agree with each other: the magic word, and the
 * block's address written inside it. A stray code address satisfies neither, and
 * it is readable, so the test cannot fault on it.
 */
void mtx_params_adopt(uint32_t candidate)
{
	mtx_depth_params_t *p;

	/* Incremented before any test: "how many times was this image entered" is
	   evidence about the boot, and a rejected candidate is still an entry. */
	mtx_starts++;

	if(!candidate)
		return;

	p = (mtx_depth_params_t *)(uintptr_t)candidate;
	if(p->magic != MTX_PARAMS_MAGIC)
		return;
	if(p->self != candidate)
		return;

	/* Only ever overwrite with something verified: this runs again on every
	   re-entry, and a stale %ebp must not wipe a good block. */
	mtx_params = p;

	/* The stack this image was entered on, taken HERE - before ace_main, before
	   the jump back into Doom's startup, i.e. before anything in this image can
	   move ESP - and only on the first entry, so a later re-entry (which the
	   .bss note above says really happens) cannot replace the seeded stack with
	   whatever ESP is by then. Only a validated block sets it, so the root -
	   whose %ebp is whatever the ACE loader left there - never claims one. */
	if(!mtx_entry_esp)
	{
		uint32_t sp;
		uint8_t msg[96];

		__asm__ __volatile__("mov %%esp, %0" : "=r"(sp));
		mtx_entry_esp = sp;
		doom_sprintf(msg, (const uint8_t *)
		             "[MTX] d%u entered on stack %u (first entry, kept)\n",
		             (uint32_t)p->depth, sp);
		mtx_log((const char *)msg);
	}
}


/*
 * TEMPORARY: which memory is this copy running its stack on?
 *
 * Measured: the two depths END UP on the same stack memory (Doom's stack region),
 * which is what lets the running depth's frames overwrite the suspended depth's
 * frames and turns a resume into a wild jump. The planner already reserves 64 KiB
 * per depth in the arena, and the FIRST entry into a depth really does run there,
 * so something during the boot moves ESP. This prints ESP from inside the depth at
 * named stages: the first stage whose ESP is no longer in the depth's own arena
 * slot (`[MTX] stack pool ...` in the boot log gives the slot's bounds) is the
 * stage that moved it.
 */
void mtx_esp_probe(const uint8_t *tag)
{
	uint32_t sp;
	uint8_t buf[128];
	mtx_depth_params_t *p = mtx_self();

	__asm__ __volatile__("mov %%esp, %0" : "=r"(sp));
	doom_sprintf(buf, (const uint8_t *)"[MTX] d%u esp at %s: %u\n",
	             p ? (uint32_t)p->depth : (uint32_t)mtx_depth(), tag, sp);
	mtx_log((const char *)buf);
}


/*
 * The stack the continued boot must run on.
 *
 * ACE's entry ends by setting ESP to `doom_data_segment + 0x85BF4` (Doom's own
 * stack, inside Doom's data segment) before jumping back into Doom's startup -
 * which is right for the ROOT: ACE was entered from inside Doom's startup, on
 * that very stack. It is wrong for a nested instance. Its ACE image is entered on
 * the stack the scheduler seeded for it (the arena slot reserved per depth), and
 * that line then throws the depth's stack away and puts the depth on the ROOT's
 * stack. Both depths then execute on the same memory, each one's frames
 * overwriting the other's suspended frames - measured: the suspended depth's
 * saved frame held Doom-ish words with no return address into the engine, while
 * its own live frame held its arena pointers, and the resume jumped to nothing.
 *
 * So an instance keeps the stack it was entered on (captured once, at its first
 * entry, in .data so a later .bss clear cannot lose it) and the root keeps the
 * arithmetic it has always used.
 */
uint32_t mtx_engine_stack(void)
{
	mtx_depth_params_t *p = mtx_self();

	if(p && mtx_entry_esp)
		return mtx_entry_esp;

	return doom_data_segment + 0x00085BF4u;
}

static mtx_depth_params_t *mtx_self(void)
{
	mtx_depth_params_t *p = (mtx_depth_params_t *)mtx_params;

	/* The entry already validated, but this pointer is read on every
	   allocation: a second check costs four comparisons and turns a corrupt
	   block into "no block" instead of a wild pointer. */
	if(!p || p->magic != MTX_PARAMS_MAGIC)
		return 0;
	if(p->self != (uint32_t)(uintptr_t)p)
		return 0;
	if(p->depth == MTX_ROOT_DEPTH || p->depth >= MTX_MAX_DEPTH)
		return 0;
	if(!p->zone || !p->raw)
		return 0;
	return p;
}

/* The depth whose engine-side glue is running: an instance's own, or the
   scheduler's for host code. */
static uint8_t mtx_glue_depth(void)
{
	mtx_depth_params_t *p = mtx_self();

	return p ? (uint8_t)p->depth : mtx_depth();
}

/* Fill depth `d`'s parameter block from the host's own state. Everything in it
   is an address or a value the HOST already holds, so the instance can neither
   invent nor lose track of any of it - including the pool, whose counters live
   in host memory and are therefore readable from both sides. */
static void mtx_params_fill(uint8_t d)
{
	mtx_depth_params_t *p = &mtx_hs.params[d];

	p->magic      = MTX_PARAMS_MAGIC;
	p->self       = (uint32_t)(uintptr_t)p;
	p->depth      = d;
	p->log_fd     = mtx_log_fd;
	p->zone       = mtx_hs.heaps[d];
	p->zone_bytes = MTX_ZONE_PER_DEPTH;
	p->screen     = mtx_hs.screens[d];
	p->raw        = &mtx_hs.raw[d];
	/* The host's own frame service. A depth hands the frame back by calling
	   THIS, not its own copy: the contexts, the registries and the switch live
	   in the host's state object, and this function's address is in the host's
	   image, so every reference inside it resolves to the host's globals. A depth
	   could not do it with its own copy of the same code - that copy's `mtx_hs`
	   is a different, empty object (the whole reason this block exists). */
	p->host_yield = mtx_host_take_frame;
}

/* The pool the CALLING code must allocate from. Host code uses the pool of the
   depth the scheduler runs; an instance uses the pool its block names. */
static mtx_raw_t *mtx_self_pool(void)
{
	mtx_depth_params_t *p = mtx_self();

	return p ? p->raw : &mtx_hs.raw[mtx_glue_depth()];
}

/* Public: the pool the calling code allocates from, for diagnostics that need to
   report what the pool itself thinks happened. */
mtx_raw_t *mtx_engine_raw_pool(void)
{
	return mtx_self_pool();
}

/* TEMPORARY: record a lump-cache call with an index that cannot be valid, so the
   caller and the table it came from are identified rather than guessed at.
   Carries MTX_FRAME_TICK_ABI because its callers are ACE hook functions declared
   regparm(2)/no_caller_saved_registers: a plain C function called from one of
   those breaks Doom's assumption that eax/ecx/edx survive. Everything it calls
   (doom_open/doom_write/doom_close) carries the same contract. */
void MTX_FRAME_TICK_ABI mtx_lump_probe(uint32_t site, uint32_t idx, uint32_t value,
                                       uint32_t aux)
{
	uint32_t v[8];
	int32_t fd;

	v[0] = 0x4D54584Cu;				/* 'MTXL' */
	v[1] = site;
	v[2] = idx;
	v[3] = value;
	v[4] = numlumps;
	v[5] = aux;
	v[6] = mtx_hs.cur;				/* the scheduler's view, read directly */
	v[7] = 0x4D54584Cu;
	fd = doom_open_WR("MTXL.LOG");
	if(fd >= 0)
	{
		doom_write(fd, v, sizeof(v));
		doom_close(fd);
	}
}

/* TEMPORARY: record an absurd allocation request with the address it was made
   from, so the caller is identified rather than guessed at. */
void mtx_alloc_probe(uint32_t size, uint32_t caller, uint32_t self)
{
	uint32_t v[8];
	int32_t fd;

	v[0] = 0x4D545841u;				/* 'MTXA' */
	v[1] = size;
	v[2] = caller;
	v[3] = self;
	v[4] = numlumps;
	v[5] = (uint32_t)(uintptr_t)mainzone;
	v[6] = mtx_hs.cur;
	v[7] = 0x4D545841u;
	fd = doom_open_WR("MTXA.LOG");
	if(fd >= 0)
	{
		doom_write(fd, v, sizeof(v));
		doom_close(fd);
	}
}

/* Public form of mtx_glue_depth(). The engine's own init branches on it (Z_Init:
	 a nested instance must build its zone in its slot instead of walking DPMI),
   and it has to agree with what the allocator uses - one answer, one place. */
uint8_t mtx_engine_depth(void)
{
	return mtx_glue_depth();
}

/* TEMPORARY: what led to the error screen. hook_bluescreen jumps here with
   I_Error's own arguments still on the stack, so the words above them are the
   frames of whoever called I_Error - which is exactly what is not known: the
   screen says an index was out of range, not who passed it.
   A plain C function on purpose: the asm caller pushes its arguments, so this
   must NOT carry MTX_FRAME_TICK_ABI (that is regparm(2), i.e. arguments in
   eax/edx). Addresses are resolved offline - an ACE address against the engine
   ELF, a Doom address as an offset from doom_code_segment, which is why both
   bases are recorded. */
void mtx_error_probe(uint32_t esp, uint32_t ret, uint32_t fmt, uint32_t value)
{
	uint32_t v[32];
	int32_t fd;
	uint32_t i;

	v[0] = 0x4D545853u;			/* 'MTXS' */
	v[1] = esp;
	v[2] = ret;					/* where I_Error was called from */
	v[3] = fmt;					/* the message the caller passed */
	v[4] = value;				/* its first argument */
	v[5] = (uint32_t)mtx_engine_depth();
	v[6] = numlumps;
	v[7] = doom_code_segment;
	v[8] = doom_data_segment;
	v[9] = ace_segment;
	v[10] = (uint32_t)(uintptr_t)lumpinfo;
	v[11] = (uint32_t)(uintptr_t)mainzone;
	for(i = 0; i < 20; i++)
		v[12 + i] = ((const uint32_t *)(uintptr_t)esp)[i];

	fd = doom_open_WR("MTXS.LOG");
	if(fd >= 0)
	{
		doom_write(fd, v, sizeof(v));
		doom_close(fd);
	}
}

/* TEMPORARY: where the texture loop STARTS. Upstream ACE's tex_init_data()
   declares its loop counter uninitialised (`for(uint32_t i; i < tmp_count; i++)`,
   commit "new texture loading"), so where it starts is whatever the register
   happened to hold. That is the difference between a depth that renders and one
   that does not: a start index past the end skips every texture's lookup, and a
   start index inside garbage calls R_GenerateLookup on a texture that is not
   there. One line per engine copy, into a file named for the depth. */
void MTX_FRAME_TICK_ABI mtx_tex_probe(uint32_t first, uint32_t count)
{
	static uint8_t done;
	uint32_t v[8];
	int32_t fd;
	char name[12] = "MTXT0.LOG";
	uint8_t d = mtx_glue_depth();

	if(done)
		return;
	done = 1;

	name[4] = (char)('0' + (d % 10));
	v[0] = 0x4D545854u;			/* 'MTXT' */
	v[1] = first;
	v[2] = count;
	v[3] = numtextures;
	v[4] = (uint32_t)d;
	v[5] = (uint32_t)(uintptr_t)textures;
	v[6] = (uint32_t)(uintptr_t)mtx_self();
	v[7] = 0x4D545854u;
	fd = doom_open_WR(name);
	if(fd >= 0)
	{
		doom_write(fd, v, sizeof(v));
		doom_close(fd);
	}
}

/* TEMPORARY: what count_textures() actually read. A nested instance's snapshot
   starts as a clone of the root's finished state, so anything the loader does not
   reset can carry over; and if the texture COUNT doubles, the loop in
   tex_init_data walks past the end of textures[] and hands R_GenerateLookup
   memory that is not a texture - which is the out-of-range lump index seen on the
   error screen. Records the two lumps it looked up, their headers, the names it
   looked up them BY, and the two counts it added. */
void MTX_FRAME_TICK_ABI mtx_texcount_probe(uint32_t t1, uint32_t t2, uint32_t idx1, uint32_t idx2)
{
	uint32_t v[24];
	int32_t fd;
	char name[12] = "MTXC0.LOG";
	uint8_t d = mtx_glue_depth();

	name[4] = (char)('0' + (d % 10));
	v[0] = 0x4D545843u;			/* 'MTXC' */
	v[1] = (uint32_t)d;
	v[2] = t1;
	v[3] = t2;
	v[4] = idx1;
	v[5] = idx2;
	v[6] = (idx1 >= 0) ? lumpinfo[idx1].size : 0;
	v[7] = (idx1 >= 0) ? lumpinfo[idx1].offset : 0;
	v[8] = (idx1 >= 0) ? (uint32_t)lumpinfo[idx1].fd : 0;
	v[9] = (idx2 >= 0) ? lumpinfo[idx2].size : 0;
	v[10] = (idx2 >= 0) ? lumpinfo[idx2].offset : 0;
	v[11] = (idx2 >= 0) ? (uint32_t)lumpinfo[idx2].fd : 0;
	v[12] = *((const uint32_t *)dtxt_texture1);
	v[13] = *((const uint32_t *)(dtxt_texture1 + 4));
	v[14] = *((const uint32_t *)dtxt_texture2);
	v[15] = *((const uint32_t *)(dtxt_texture2 + 4));
	v[16] = *((const uint32_t *)dtxt_pnames);
	v[17] = *((const uint32_t *)(dtxt_pnames + 4));
	v[18] = (uint32_t)numlumps;
	v[19] = (uint32_t)(uintptr_t)lumpinfo;
	v[20] = 0;					/* reserved */
	v[21] = 0;					/* reserved */
	v[22] = (uint32_t)(uintptr_t)mtx_self();
	v[23] = 0x4D545843u;

	fd = doom_open_WR(name);
	if(fd >= 0)
	{
		doom_write(fd, v, sizeof(v));
		doom_close(fd);
	}
}

/*
 * Reserve the arena for the nested instances. MUST run before ACE's Z_Init takes
 * the rest of RAM: Z_Init walks dpmi_get_ram() down until malloc succeeds, i.e.
 * it consumes everything except a 32 KiB reserve, so an arena requested later
 * can never be satisfied. This is called from the top of Z_Init.
 */
void MTX_FRAME_TICK_ABI mtx_engine_reserve(void)
{
	/* Sized for what is actually wanted rather than for the depth ceiling: the
	   root plus the instances THIS video needs. A depth costs
	   snapshot + zone + screen + stack + image + raw (MTX_ZONE_PER_DEPTH and
	   MTX_RAW_PER_DEPTH are the two biggest terms), i.e. ~7.2 MiB each, so two
	   depths need ~14.5 MiB and 20 leaves the pair room to spare while the root
	   still gets the rest of DPMI for its own zone.

	   RAISED to 28 for the RING. Three levels (the root plus two instances) cost
	   ~21.6 MiB, and the planner takes the FLOOR of the division, so at 20 it
	   plans two depths and refuses the third at mtx_push time. The ladder below
	   keeps the pair working wherever 28 cannot be had, so a smaller machine
	   loses the ring rather than the nest. */
	static const uint32_t sizes[] = {28u, 24u, 20u, 16u, 0u};	// MiB
	uint32_t i;

	/* A nested depth reaches this too: its ACE image runs ace_main, which calls
	   Z_Init. The arena belongs to the MACHINE, not to a depth - the root has
	   already claimed it, so a second claim must be a no-op. Re-claiming would
	   fail (the memory is gone) and a depth would silently get a smaller arena
	   than the one the planner budgeted against. */
	if(mtx_hs.arena)
		return;

	/* A nested INSTANCE must never claim an arena either: it already IS one,
	   and asking DPMI for 8 MiB it does not have would eat the little the root
	   left behind. Its parameter block is how it knows. */
	if(mtx_self())
		return;

	/* mtx_hs is zero-filled, so the "log not open yet" sentinel - which used to
	   come from a static initialiser - has to be set explicitly. Runs before any
	   mtx_log() call. */
	mtx_log_fd = -1;

	for(i = 0; sizes[i]; i++)
	{
		mtx_arena = doom_malloc(sizes[i] * 1024u * 1024u);
		if(mtx_arena)
		{
			mtx_arena_size = sizes[i] * 1024u * 1024u;
			break;
		}
	}

	// stdout is still captured this early, so this line shows up in the log
	doom_printf("[MTX] reserved %u MiB for nested instances\n",
	            mtx_arena ? (mtx_arena_size / 1024u / 1024u) : 0u);
}

// no libc in here: length by hand
//
// A nested instance logs into the HOST's file rather than opening its own: DOS
// descriptors are process-wide, so the descriptor in its parameter block writes
// to the same MTX.LOG at the same offset, and the instance's own view of what it
// did lands in one place with the host's. (Without that, an instance's
// `mtx_log_fd` is its own zeroed .bss and the whole of its boot is silent.)
static void mtx_log(const char *s)
{
	mtx_depth_params_t *sp = mtx_self();
	const char *p = s;
	uint32_t n = 0;
	int32_t fd = sp ? sp->log_fd : mtx_log_fd;

	if(fd < 0)
	{
		if(sp)
			return;			/* an instance never opens its own */
		mtx_log_fd = doom_open_WR("MTX.LOG");
		fd = mtx_log_fd;
	}
	if(fd < 0)
		return;

	while(p[n])
		n++;
	doom_write(fd, (void *)s, n);
}

/* The zone layout the nested side builds must be byte-for-byte the one Doom's
   Z_Malloc walks. mtx_zone.h mirrors engine.h (the same way ldr1n.h mirrors
   lumpinfo_t) because it also has to build on the host, where engine.h does not
   exist. So assert the mirror rather than trust it: a drift in engine.h becomes a
   compile error instead of a corrupted heap at runtime. */
typedef char mtx_zone_layout_must_match[
	(sizeof(mtx_memzone_t)  == sizeof(memzone_t)
	 && sizeof(mtx_memblock_t) == sizeof(memblock_t)) ? 1 : -1];

/* The hook classification mirrors engine/utils.h's HOOK_ enum and its 12-byte table
   (mtx_hooks.h has to build on the host too). Assert the mirror: a drift would make a
   depth save and restore the wrong number of bytes at a patch site, which is memory
   corruption rather than a warning. */
typedef char mtx_hook_constants_must_match[
	(MTX_HOOK_JMP_ACE  == HOOK_JMP_ACE
	 && MTX_HOOK_CALL_DOOM == HOOK_CALL_DOOM
	 && MTX_HOOK_UINT16 == HOOK_UINT16
	 && MTX_HOOK_ABSADDR_DATA == HOOK_ABSADDR_DATA
	 && MTX_HOOK_SET_NOPS == HOOK_SET_NOPS
	 && MTX_HOOK_MEM_COPY == HOOK_MEM_COPY
	 && MTX_HOOK_READ32 == HOOK_READ32
	 && MTX_HOOK_CODE == CODE_HOOK
	 && MTX_HOOK_DATA == DATA_HOOK
	 && sizeof(mtx_hook_t) == sizeof(hook_t)) ? 1 : -1];

/*
 * Give the CALLING depth a zone of its own, inside the arena slot reserved for it.
 *
 * This is the nested half of Z_Init. The root walks DPMI RAM because it is first
 * and the memory is still free; a child cannot, since the root's Z_Init already
 * took it - and a depth taking memory away from the root would defeat the point.
 * Same layout, different memory, which is the only difference Doom's allocator
 * can see.
 *
 * Called from two places on purpose: Z_Init's nested branch, and the loaded guest
 * through its service table. So the heap a guest gets here is produced by exactly
 * the path a real nested instance uses - not a test-only shortcut.
 *
 * It also proves the zone is USABLE, not merely well-formed: a real Z_Malloc/Z_Free
 * round trip with mainzone pointed at this depth's zone, then mainzone put back so
 * the caller's own heap is untouched. Z_Malloc is Doom's, so a failure is the
 * engine's verdict on the zone rather than this file's opinion of its own work.
 */
void *mtx_engine_zone_init(void)
{
	/* TEMPORARY: the FIRST Doom-side code this depth runs (Z_Init is called from
	   Doom's own startup, right after the engine's entry jumps back into it), so
	   this is the probe that says whether the stack already moved by then. */
	mtx_esp_probe((const uint8_t *)"Z_Init, first Doom-side code");
	mtx_depth_params_t *sp = mtx_self();
	uint8_t d = mtx_glue_depth();
	memzone_t *save;
	void *mem, *p = 0, *q = 0;
	uint32_t bytes;
	int bad, after;
	uint8_t buf[176];

	if(d >= MTX_MAX_DEPTH)
		return 0;

	/* An instance's zone slot and its size come from its parameter block; host
	   code takes them from the arena the host carved. Either way the object is
	   the same one the planner budgeted for. */
	mem = sp ? sp->zone : mtx_hs.heaps[d];
	bytes = sp ? sp->zone_bytes : MTX_ZONE_PER_DEPTH;
	if(!mem || !bytes)
		return 0;

	mtx_zone_build(mem, bytes);
	bad = mtx_zone_check(mem);

	save = mainzone;
	mainzone = (memzone_t *)mem;
	p = Z_Malloc(1024, PU_STATIC, NULL);
	q = Z_Malloc(64, PU_STATIC, NULL);
	if(p)
		Z_Free(p);
	if(q)
		Z_Free(q);
	mainzone = save;

	after = mtx_zone_check(mem);

	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] depth%u zone %u B at %u build %d alloc %u probe %d%s\n",
	             (uint32_t)d, bytes, (uint32_t)(uintptr_t)mem,
	             bad, (uint32_t)(uintptr_t)p, after,
	             sp ? " (instance)" : "");
	mtx_log((const char *)buf);

	if(bad || after || !p || !q)
		return 0;
	return mem;
}

/*
 * ---- raw allocations: a depth's non-zone memory -----------------------------
 *
 * The zone was half the job. A depth can build its own heap, and Doom's
 * allocator works in it - but ACE's own `ace_main` also asks the RAW allocator
 * for memory, and the raw allocator is machine-global: the root's Z_Init walks
 * dpmi_get_ram() down until an allocation succeeds, so it eats everything but a
 * 32 KiB reserve. Measured, that left a depth 61,440 B against a single
 * 209,184 B request at init_render(), and no arena increase helps because the
 * root consumes the remainder either way.
 *
 * So the engine's own allocation entry points go through these three functions:
 *
 *   mtx_engine_alloc     decides by the CURRENT DEPTH - the root keeps using
 *                        DPMI (its own memory, plenty of it), a child draws on
 *                        the pool carved for it at boot.
 *   mtx_engine_free /
 *   mtx_engine_realloc   decide by the POINTER'S PROVENANCE - the pool that
 *                        contains it, found by range plus block magic. Not by
 *                        the current depth, because the depth that frees an
 *                        object is not always the one that allocated it, and
 *                        freeing pool memory into the DPMI heap (or the reverse)
 *                        corrupts a heap silently. A pointer in no pool is a
 *                        DPMI pointer and goes back to doom_free.
 *
 * Only the sites that hand memory across these two worlds are routed here: the
 * loader's own ldr_malloc/ldr_realloc (which is where the texture, flat, sprite,
 * render and WAD-cache loaders all end up), the two wad_cache_* lump readers,
 * the string helpers, and the frees that match them. The root-only paths inside
 * Z_Init keep calling doom_malloc directly - they cannot run at a depth.
 */

/* Which pool, if any, does this pointer come from? Scanning every pool (rather
   than trusting the current depth) is what makes the decision provenance. The
   current copy's own pool is tested first, so an instance answers for its own
   memory without a scan. */
static mtx_raw_t *mtx_raw_pool_of(void *ptr)
{
	mtx_depth_params_t *sp = mtx_self();
	uint32_t i;

	if(!ptr)
		return 0;
	if(sp && mtx_raw_owns(sp->raw, ptr))
		return sp->raw;
	for(i = 1; i < MTX_MAX_DEPTH; i++)		/* the root has no pool */
		if(mtx_raw_owns(&mtx_hs.raw[i], ptr))
			return &mtx_hs.raw[i];
	return 0;
}

/* One refusal line per pool: a per-allocation log would be far larger than the
   log file, and the interesting fact is the SIZE - that is what says whether the
   pool is too small. */
static void mtx_raw_report(mtx_raw_t *p, uint8_t depth, uint32_t want, uint32_t ra)
{
	uint8_t buf[192];

	if(p->refusals != 1)			/* only the first one is news */
		return;

	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] depth%u raw pool: request for %u B refused, %u of %u used, peak %u (caller offset %u)\n",
	             (uint32_t)depth, want, p->used, p->size, p->peak,
	             ra - (uint32_t)(uintptr_t)&mtx_engine_alloc);
	mtx_log((const char *)buf);
}

void *mtx_engine_alloc(uint32_t size)
{
	uint8_t d = mtx_glue_depth();
	mtx_raw_t *p;
	void *ret;
	/* TEMPORARY: who called. The image is the same file at a different base, so
	   the offset from &mtx_engine_alloc resolves against the root's ELF. */
	uint32_t ra = (uint32_t)(uintptr_t)__builtin_return_address(0);

	/* TEMPORARY diagnostic: independent evidence of what a nested instance's own
	   copy of this engine believes. Writes its own file, so it does not depend
	   on the parameter block working (which is the thing under test). */
	{
		static uint8_t probed;
		if(!probed)
		{
			uint32_t v[16];
			int32_t fd;

			probed = 1;
			v[0] = 0x4D54584Eu;			/* 'MTXN' */
			v[1] = (uint32_t)(uintptr_t)mtx_params;
			v[2] = (uint32_t)mtx_depth();
			v[3] = (uint32_t)(uintptr_t)&mtx_params;
			v[4] = (uint32_t)(uintptr_t)mtx_engine_alloc;
			v[5] = (uint32_t)size;
			v[6] = (uint32_t)(uintptr_t)mainzone;
			v[7] = mtx_starts;
			v[8] = (uint32_t)mtx_engine_depth();
			v[9] = ace_segment;
			v[10] = doom_code_segment;
			v[11] = doom_data_segment;
			v[12] = (uint32_t)(uintptr_t)&mtx_engine_reserve;
			v[13] = 0;
			v[14] = 0;
			v[15] = 0x4D54584Eu;
			fd = doom_open_WR("MTXN.LOG");
			if(fd >= 0)
			{
				doom_write(fd, v, sizeof(v));
				doom_close(fd);
			}
		}
	}

	/* The root allocates from the DPMI heap it owns; a depth - whether it
	   reached here as the host's code serving a guest, or as an instance's own
	   copy of the engine - allocates from its own pool. */
	if(d == MTX_ROOT_DEPTH || d >= MTX_MAX_DEPTH)
		return doom_malloc(size);

	p = mtx_self_pool();
	if(!p || !p->base)
		return doom_malloc(size);	/* no pool carved: do not pretend */

	ret = mtx_raw_alloc(p, size);
	if(!ret)
		mtx_raw_report(p, d, size, ra);

	return ret;
}

void *mtx_engine_realloc(void *ptr, uint32_t size)
{
	mtx_raw_t *p;
	void *ret;

	if(!ptr)
		return mtx_engine_alloc(size);

	p = mtx_raw_pool_of(ptr);
	if(!p)
		return doom_realloc(ptr, size);	/* a DPMI pointer, back to DPMI */

	ret = mtx_raw_realloc(p, ptr, size);
	if(!ret)
		mtx_raw_report(p, mtx_glue_depth(), size,
		               (uint32_t)(uintptr_t)__builtin_return_address(0));

	return ret;
}

void mtx_engine_free(void *ptr)
{
	mtx_raw_t *p;

	if(!ptr)
		return;

	p = mtx_raw_pool_of(ptr);
	if(!p)
		doom_free(ptr);			/* a DPMI pointer */
	else
		mtx_raw_free(p, ptr);
}

/*
 * Called at the top of Z_Init, BEFORE the root/child branch - because the block
 * this reads is what decides which branch this is (see mtx_self).
 *
 * It is also where a nested instance first says anything at all: the log line
 * below is written by the instance, through the host's descriptor, and repeats
 * back the depth, zone, screen and pool it read out of its block. The host logs
 * the block it FILLED; this logs the block the instance RECEIVED. If those two
 * lines agree, the hand-off is proven rather than assumed, and if they differ
 * the instance's whole idea of itself is wrong and should be visible rather than
 * guessed at.
 */
void mtx_engine_apply_params(void)
{
	mtx_depth_params_t *p = mtx_self();
	uint8_t buf[192];

	if(!p)
		return;				/* the host: nothing to apply */

	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] instance: depth %u zone %u screen %u raw %u log %d\n",
	             p->depth, (uint32_t)(uintptr_t)p->zone,
	             (uint32_t)(uintptr_t)p->screen,
	             (uint32_t)(uintptr_t)p->raw, p->log_fd);
	mtx_log((const char *)buf);

	/* Render into THIS instance's framebuffer rather than the root's. The
	   renderer keeps writing through Doom's screens[0] and through the engine's
	   own copy of it, and `init_draw()` bakes whichever `screen_buffer` holds
	   into the presentation routine's immediate - all of which happens after
	   Z_Init, so setting the pointer here is what makes an instance's frames
	   land in its own region and nowhere else. */
	if(p->screen && screen_buffer != (uint8_t *)p->screen)
	{
		screen_buffer = (uint8_t *)p->screen;

		doom_sprintf(buf, (const uint8_t *)
		             "[MTX] instance: screen_buffer -> %u\n",
		             (uint32_t)(uintptr_t)screen_buffer);
		mtx_log((const char *)buf);
	}
}

/*
 * ---- video: which depth the screen shows ----------------------------------
 *
 * In this engine a frame reaches the screen through ONE asm routine,
 * `I_FinishUpdate` (engine/asm.S): it copies 64000 bytes to 0xA0000. Its copy
 * SOURCE is a dword baked into its own code by `init_draw()` - the value of
 * `screen_buffer` at init, i.e. the ROOT's buffer. Nothing about that is
 * per-depth, so without the code below every depth would present the root's
 * buffer: a nested instance's frames could never reach the screen, and there
 * would be no way to choose between depths.
 *
 * So the host owns that dword from here on. The offset is verified against the
 * built code rather than assumed: the routine starts with three 1-byte pushes,
 * so the `mov $imm32,%esi` immediate begins exactly at I_FinishUpdate+4 - which
 * is the offset `init_draw()` already patches.
 */
#define MTX_PRESENT_SRC ((uint32_t *)((uint8_t *)(void *)I_FinishUpdate + 4))

/* Present depth 1 for a window, then the root, so a capture can catch both. The
   window is long on purpose: the captures happen at fixed wall-clock times, so a
   short window is a coin flip on which depth a given capture catches. */
/* How often a depth reads video memory back and compares it with its own frame
   (see mtx_engine_self_check): often enough to catch a depth that stops being
   presented, rare enough that scanning 64000 bytes does not show up in a frame. */
#define MTX_SELF_CHECK_EVERY  100u

/* The key that switches the inset on and off, in Doom's own key numbering (its
   key state is indexed by character). 'Y' is not a default binding in a level and
   is not the first letter of any cheat, so pressing it does nothing else. */
#define MTX_SWITCH_KEY  'y'

/*
 * Point the presentation at `depth`. Returns whether the screen ALREADY agrees.
 *
 * Setting the source is not evidence on its own - the blit has not run yet, and
 * may copy from somewhere else entirely - so this reads the first dword of VGA
 * memory back and compares it with the depth's own buffer. On the frame the
 * source changes the answer is expected to be "not yet": the blit happens later
 * in the frame. The answer is checked again on the next frame, which is what
 * makes "the visible screen is this depth's" a measurement instead of a claim.
 * (The engine itself reads 0xA0000 in net.c, so this is an ordinary access here.)
 */
static uint32_t mtx_present(uint8_t depth)
{
	uint8_t *buf;

	if(depth >= MTX_MAX_DEPTH || !mtx_hs.screens[depth])
		return 0;

	buf = (uint8_t *)mtx_hs.screens[depth];
	*MTX_PRESENT_SRC = (uint32_t)(uintptr_t)buf;
	mtx_hs.present_depth = depth;
	mtx_hs.present_frame = mtx_hs.frame_no;

	return (*(uint32_t *)(uintptr_t)0xA0000 == *(uint32_t *)buf) ? 1u : 0u;
}

/*
 * Second half of the check above, one frame later. Same-frame would be useless:
 * the blit for this frame has not run yet when the policy does, so the screen
 * still holds the previous depth's pixels - measured, and exactly what a
 * same-frame check reported. A frame later the blit has happened, and the answer
 * is about the screen rather than about the order of the code.
 */
static void mtx_present_confirm(void)
{
	uint8_t buf[144];
	uint8_t d = mtx_hs.present_depth;
	uint32_t vga, mine;

	if(!mtx_hs.present_frame || mtx_frame_no <= mtx_hs.present_frame)
		return;

	mtx_hs.present_frame = 0;
	vga  = *(uint32_t *)(uintptr_t)0xA0000;
	mine = *(uint32_t *)mtx_hs.screens[d];

	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] screen check: depth %u %s (vga %u vs buffer %u)\n",
	             (uint32_t)d, (vga == mine) ? "CONFIRMED" : "MISMATCH", vga, mine);
	mtx_log((const char *)buf);
}

/*
 * Fill the CALLING depth's own screen region with vertical colour bars.
 *
 * Runs at whichever depth calls it, so it writes THAT depth's buffer: at depth 1,
 * `screen_buffer` is the depth's own region (pinned on switch-in). Two things
 * follow, and both are the point of the exercise: the pattern is unmistakable on
 * screen, so a capture says which depth is being presented; and it must NOT appear
 * in the root's buffer, which is what makes the two screens genuinely separate.
 */
static void *mtx_engine_screen_fill(void)
{
	uint8_t *buf = screen_buffer;
	uint32_t i;

	if(!buf)
		return 0;

	for(i = 0; i < MTX_SCREEN_FRAME_BYTES; i++)
		buf[i] = (uint8_t)(((i % 320u) / 20u) * 16u + 1u);	/* 16 bars, first
		                                                       index 1 so offset 0
		                                                       is non-zero: "not
		                                                       drawn" and "drawn"
		                                                       must be distinguishable
		                                                       from zeroed memory */

	return buf;
}

/*
 * Doom's OWN screen pointer, `screens[0]`. ACE copies it once at init into
 * `screen_buffer`, but Doom's RENDERER keeps writing through this variable - so
 * pinning only ACE's copy leaves a nested depth painting over the root's frame.
 * Both are pinned now (see dos_wire.c), and this is where the claim gets checked
 * instead of assumed.
 */
#define MTX_DOOM_SCREEN_VAR  0x00074FC4u
#define MTX_DOOM_SCREEN_MARK 0x4D545853u	/* 'MTXS' */

/*
 * Write a marker THROUGH Doom's own pointer, at the calling depth.
 *
 * Not on trust: the variable is read back first and the write is REFUSED unless it
 * points at this depth's own screen region. The guard is not decoration - a wrong
 * write here lands in the root's visible frame, which is the exact failure this pin
 * exists to prevent. Returns where the write landed, or 0 if it was refused.
 */
static void *mtx_engine_doom_screen(void)
{
	uint32_t *var = (uint32_t *)(uintptr_t)(doom_data_segment + MTX_DOOM_SCREEN_VAR);
	uint8_t *mine = screen_buffer;
	uint8_t *doom = (uint8_t *)(uintptr_t)*var;

	if(!mine || doom != mine)
		return 0;

	*(uint32_t *)doom = MTX_DOOM_SCREEN_MARK;
	return doom;
}

/*
 * Check the pin by where the marker LANDED, not by intent: it must be in the
 * depth's memory and must NOT be in the root's frame. Both are read straight out
 * of the arena, so this cannot be satisfied by the variable merely looking right.
 */
static void mtx_engine_doom_screen_check(void)
{
	uint8_t buf[176];
	uint32_t root, deep;

	if(!mtx_hs.ldr_report[4] || mtx_hs.doom_screen_ok)
		return;

	mtx_hs.doom_screen_ok = 1;
	root = *(uint32_t *)mtx_hs.screens[0];
	deep = *(uint32_t *)mtx_hs.screens[1];

	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] doom screens[0] -> %u: marker in depth1 buffer %s, "
	             "in root buffer %s\n",
	             mtx_hs.ldr_report[4],
	             (deep == MTX_DOOM_SCREEN_MARK) ? "YES" : "NO",
	             (root == MTX_DOOM_SCREEN_MARK) ? "YES" : "NO");
	mtx_log((const char *)buf);
}

/*
 * Hook isolation probe.
 *
 * The registry being populated is not the same as it working: 405 addresses and a
 * save/restore routine that never runs on the right bytes would look identical from
 * outside. So take the first patch site the engine registered, record what is there
 * at the root, and have a depth write over it. Then read it again at the root: it must
 * be back to the original. A write that survives would mean a nested instance can
 * change Doom's code for everyone - which is precisely what entering a real engine
 * image at a depth will start doing.
 */
extern uint8_t _hooks_start[];		/* engine/linker.ld */
extern uint8_t _hooks_end[];

static int probe_find(void *ctx, void *addr, uint32_t len)
{
	(void)ctx; (void)len;
	mtx_hs.probe_addr = (uint32_t)(uintptr_t)addr;
	mtx_hs.probe_orig = *(uint8_t *)addr;
	return 1;			/* stop: only the first site is wanted */
}

static void mtx_engine_probe_setup(void)
{
	uint32_t n = (uint32_t)(_hooks_end - _hooks_start) / (uint32_t)sizeof(mtx_hook_t);

	/* the scan reports "refused" because the callback stopped it - that is the
	   point, and the return value is not used here */
	mtx_hooks_scan(_hooks_start, n, doom_code_segment, doom_data_segment,
	               probe_find, 0);
}

/*
 * Called by the guest through its service table: overwrite the probe site and read it
 * back, from inside the depth. Returns the address it wrote to.
 */
static void *mtx_engine_probe_patch(void)
{
	uint8_t *p = (uint8_t *)(uintptr_t)mtx_hs.probe_addr;

	if(!mtx_hs.probe_addr)
		return 0;

	mtx_hs.probe_wrote = 0xCCu;
	*p = (uint8_t)mtx_hs.probe_wrote;
	mtx_hs.probe_read = *p;		/* prove the write landed, at this depth */

	return p;
}

/*
 * Back at the root: the byte must be the one that was there before the depth wrote.
 * `probe_read` proves the write happened at depth 1; this proves it did not survive the
 * switch back. Both halves are needed - a write that never happened would otherwise
 * look exactly like isolation.
 */
static void mtx_engine_probe_check(void)
{
	uint8_t buf[176];
	uint32_t now;

	if(!mtx_hs.probe_addr || mtx_hs.probe_logged)
		return;
	if(!mtx_hs.probe_read)		/* the depth has not run its write yet */
		return;

	mtx_hs.probe_logged = 1;
	now = *(uint8_t *)(uintptr_t)mtx_hs.probe_addr;

	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] hook isolation: depth1 wrote %u at %u (read back %u); "
	             "root now %u, was %u => %s\n",
	             mtx_hs.probe_wrote, mtx_hs.probe_addr, mtx_hs.probe_read,
	             now, mtx_hs.probe_orig,
	             (now == mtx_hs.probe_orig) ? "ISOLATED" : "LEAKED");
	mtx_log((const char *)buf);
}

// A child depth's entry. It loads a code image of its own out of the WAD and runs it.
//
// The image is a real WAD lump, read through the engine's WAD layer, and it lands
// in this depth's own arena slot - so the bytes that execute here belong to this
// depth alone. That matters: ACE patches code at run time (its hook tables), so
// two depths sharing one code page would fight over the same bytes.
//
// WHICH DEPTH IS NOT AN ARGUMENT. The scheduler knows - it is the depth this
// context was created for - and taking it from there is what lets ONE entry boot
// every level of the ring. The literal `1` that used to be scattered through this
// function is the same literal the ring replaced everywhere else: correct while
// there was exactly one child, and wrong the moment there are two. Note that the
// host object read here is the HOST's: this entry runs in the root's context,
// before the instance's own image is entered.
static void mtx_demo_child(void *arg)
{
	uint8_t buf[176];
	uint32_t entry = 0, total = 0;
	int32_t r;
	uint8_t d = (uint8_t)mtx_depth();

	(void)arg;
	doom_sprintf(buf, (const uint8_t *)"[MTX] depth%u entered\n", (uint32_t)d);
	mtx_log((const char *)buf);

#if MTX_CHILD_ENGINE_IMAGE
	/*
	 * The milestone: the REAL engine image, in this depth's own slot, entered with
	 * the engine's own register convention (ECX = entry, ESI/EDI = Doom's segments).
	 *
	 * Its `_start` relocates itself against that address, rewrites the code-address
	 * tables, clears ITS OWN .bss - this depth's image slot, not the root's - and
	 * calls ace_main. So what runs here is a second instance's initialisation, on
	 * this depth's heap (256 KiB, mtx_engine_zone_init) and this depth's stack.
	 */
	r = mtx_ldr_load(MTX_ACE_LUMP, d, ace_git_version, &entry, &total);
	if(r != 0)
	{
		doom_sprintf(buf, (const uint8_t *)
		             "[MTX] depth%u: engine image load FAILED (%d)\n",
		             (uint32_t)d, r);
		mtx_log((const char *)buf);
		mtx_finish();
	}

	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] depth%u: engine image %u B at %u entry %u bss %u\n",
	             (uint32_t)d, total, (uint32_t)(uintptr_t)mtx_hs.images[d], entry,
	             ((uint32_t *)mtx_hs.images[d])[0]);
	mtx_log((const char *)buf);

	/* The instance's parameter block, filled by the host and read by the
	   instance. The instance's own .bss is empty (it was never booted) and its
	   `mtx_hs` is a different object, so this block is the only thing that can
	   tell it its depth, its zone slot, its screen, its raw pool and the log
	   descriptor. It travels in %ebp (see mtx_params.h), and the block itself
	   lives in mtx_hs - HOST memory - so the accounting the instance updates is
	   the same accounting the host reports. */
	mtx_params_fill(d);
	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] depth%u params at %u: depth %u zone %u screen %u raw %u log %d\n",
	             (uint32_t)d, (uint32_t)(uintptr_t)&mtx_hs.params[d],
	             mtx_hs.params[d].depth,
	             (uint32_t)(uintptr_t)mtx_hs.params[d].zone,
	             (uint32_t)(uintptr_t)mtx_hs.params[d].screen,
	             (uint32_t)(uintptr_t)mtx_hs.params[d].raw,
	             mtx_hs.params[d].log_fd);
	mtx_log((const char *)buf);

	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] depth%u: entering, ECX=%u ESI=%u EDI=%u EBP=%u, free DPMI %u B\n",
	             (uint32_t)d, entry, doom_code_segment, doom_data_segment,
	             (uint32_t)(uintptr_t)&mtx_hs.params[d], dpmi_get_ram());
	mtx_log((const char *)buf);

	mtx_hs.ldr_guest_entry = entry;
	mtx_hs.ldr_guest_total = total;

	mtx_ldr_enter_engine(entry, (uint32_t)(uintptr_t)&mtx_hs.params[d]);	// never returns
#else
	r = mtx_ldr_load(MTX_GUEST_LUMP, 1, MTX_LDR_GUEST_VERSION, &entry, &total);
	if(r != 0)
	{
		doom_sprintf(buf, (const uint8_t *)
		             "[MTX] depth1: guest image load FAILED (%d)\n", r);
		mtx_log((const char *)buf);
		mtx_finish();
	}
	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] depth1: guest alloc %u B at %u, entering\n", total, entry);
	mtx_log((const char *)buf);

	mtx_hs.ldr_guest_entry = entry;
	mtx_hs.ldr_guest_total = total;
	mtx_hs.ldr_report[0] = 0;
	mtx_hs.ldr_report[1] = 0;
	mtx_hs.ldr_report[2] = 0;
	mtx_hs.ldr_report[3] = 0;
	mtx_hs.ldr_report[4] = 0;

	/* The whole authority the loaded image has over the machine: leave evidence,
	   ask for a zone of its own, draw on its OWN screen, overwrite a patch site,
	   yield, finish. No free, no close, no arbitrary allocation - the same
	   reasoning as ldr1n's service table, so a guest cannot damage the parent. */
	mtx_hs.ldr_svc[0] = (uint32_t)(uintptr_t)mtx_hs.ldr_report;
	mtx_hs.ldr_svc[1] = (uint32_t)(uintptr_t)mtx_yield;
	mtx_hs.ldr_svc[2] = (uint32_t)(uintptr_t)mtx_finish;
	mtx_hs.ldr_svc[3] = (uint32_t)(uintptr_t)mtx_engine_zone_init;
	mtx_hs.ldr_svc[4] = (uint32_t)(uintptr_t)mtx_engine_screen_fill;
	mtx_hs.ldr_svc[5] = (uint32_t)(uintptr_t)mtx_engine_doom_screen;
	mtx_hs.ldr_svc[6] = (uint32_t)(uintptr_t)mtx_engine_probe_patch;

	mtx_ldr_enter(entry);		// hands off through ldr0N; never returns
#endif

	mtx_log("[MTX] depth1: enter returned (cannot happen)\n");
	mtx_finish();
}

static void mtx_engine_boot(void)
{
	mtx_boot_t bt;
	mtx_host_ops_t ops;	// taken by value by mtx_init, so a local is fine
	uint8_t buf[176];
	uint32_t entry = 0, total = 0;

	mtx_booted = 1;
	mtx_log("[MTX] first frame\n");

	if(!mtx_arena)
	{
		mtx_log("[MTX] no reserved arena; host disabled\n");
		mtx_boot_failed = 1;
		return;
	}

	// depth 0 keeps what the engine already has; only children get fresh zone
	// and framebuffer from the arena
	mtx_heaps[0]   = mainzone;
	mtx_screens[0] = screen_buffer;

	/* Size each depth's image slot from the image it will actually hold - the
	   engine's own lump. Reading its .bss dword beats guessing, so the planner is
	   handed the real requirement and the depth ceiling stays honest. */
	mtx_hs.image_bytes = mtx_ldr_image_bytes(MTX_ACE_LUMP);
	if(!mtx_hs.image_bytes)
		mtx_log("[MTX] no code image lump: depths get no image slot\n");

	if(mtx_dos_boot(mtx_arena, mtx_arena_size, MTX_ZONE_PER_DEPTH,
	                mtx_hs.image_bytes, MTX_RAW_PER_DEPTH,
	                mtx_dos_wire_register, mtx_heaps, mtx_screens,
	                mtx_hs.images, mtx_hs.raw, &bt) != 0)
	{
		mtx_log("[MTX] boot failed (arena too small for two depths)\n");
		mtx_boot_failed = 1;
		return;
	}

	// mtx_init takes the table by value; see the note in mtx_host.h about why a
	// retained pointer to a caller local would be a wild jump.
	ops.save_state = mtx_dos_save_state;
	ops.load_state = mtx_dos_load_state;
	mtx_init(bt.depth, &ops, bt.stack_base, bt.stack_bytes);

	/* Depth 1's own stack slot, for the high-water measurement below and in the
	   policy: slot d is [base + d*MTX_STACK_SIZE, base + (d+1)*MTX_STACK_SIZE). */
	mtx_stack_lo = (uint32_t)(uintptr_t)bt.stack_base + MTX_STACK_SIZE;
	mtx_stack_hi = mtx_stack_lo + MTX_STACK_SIZE;

	doom_sprintf(buf, (const uint8_t *)"[MTX] host up: depths %u snap %u img %u\n",
	             (uint32_t)bt.depth, bt.snapshot_per_depth, mtx_hs.image_bytes);
	mtx_log((const char *)buf);

	doom_sprintf(buf, (const uint8_t *)\
	             "[MTX] stack pool %u B at %u, %u B per depth (depth 1 at %u)\n",
	             bt.stack_bytes, (uint32_t)(uintptr_t)bt.stack_base,
	             MTX_STACK_SIZE,
	             (uint32_t)(uintptr_t)bt.stack_base + MTX_STACK_SIZE);
	mtx_log((const char *)buf);

	doom_sprintf(buf, (const uint8_t *)\
	             "[MTX] screen region %u B/depth (frame %u + loader %u + parse %u)\n",
	             MTX_SCREEN_BYTES, MTX_SCREEN_FRAME_BYTES,
	             MTX_SCREEN_LOADER_BYTES, MTX_SCREEN_PARSE_BYTES);
	mtx_log((const char *)buf);

	/* The pool's own arithmetic, in the log: what a child was given, where it
	   is, and how much of the arena the ask actually took. */
	doom_sprintf(buf, (const uint8_t *)\
	             "[MTX] raw pool %u B/depth (child 1 at %u), arena used %u of %u\n",
	             MTX_RAW_PER_DEPTH, (uint32_t)(uintptr_t)mtx_hs.raw[1].base,
	             mtx_hs.raw[1].size, mtx_arena_size);
	mtx_log((const char *)buf);

	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] isolation: %u regions, %u patches, snapshot %u B\n",
	             mtx_dos_region_count(), mtx_dos_hook_count(),
	             bt.snapshot_per_depth);
	mtx_log((const char *)buf);

	/* TEMPORARY: what the .bss split actually registered, and what the snapshot
	   total adds up to. The split exists so the scheduler's own state object is
	   never restored over itself; the totals below say whether it is in effect at
	   runtime, which is a different question from whether the code looks right. */
	{
		extern uint8_t _bss_start[], _bss_end[];
		uint32_t b0 = (uint32_t)(uintptr_t)_bss_start;
		uint32_t b1 = (uint32_t)(uintptr_t)_bss_end;
		uint32_t h0 = (uint32_t)(uintptr_t)&mtx_hs;
		uint32_t h1 = h0 + (uint32_t)sizeof(mtx_hs);

		doom_sprintf(buf, (const uint8_t *)
		             "[MTX] bss [%u, %u) size %u | mtx_hs [%u, %u) size %u | "
		             "halves %u + %u | snapshot %u\n",
		             b0, b1, b1 - b0, h0, h1, h1 - h0,
		             (h0 > b0) ? h0 - b0 : 0u,
		             (h1 < b1) ? b1 - h1 : 0u,
		             bt.snapshot_per_depth);
		mtx_log((const char *)buf);
	}

	/* TEMPORARY: does any registered region cover the LIVE stack? Measured
	   NO (regions end at 274B60, the stack sits at 284B40), so the resume is not
	   being clobbered by a snapshot restore. Kept because it is the check that
	   answered it, and cheap. */
	{
		uint32_t sp = (uint32_t)(uintptr_t)&sp;
		uint32_t k;

		for(k = 0; k < (uint32_t)mtx_hs.nrgn; k++)
		{
			uint32_t b = (uint32_t)(uintptr_t)mtx_hs.rgn[k].base;
			uint32_t e = b + mtx_hs.rgn[k].size;

			doom_sprintf(buf, (const uint8_t *)
			             "[MTX] region %u: [%u, %u) size %u | esp %u -> %s\n",
			             k, b, e, mtx_hs.rgn[k].size, sp,
			             (sp >= b && sp < e) ? "STACK INSIDE THIS REGION"
			                                 : "outside");
			mtx_log((const char *)buf);
		}
	}

	/* What is left of DPMI after the root's own Z_Init. The root's Z_Init walks
	   dpmi_get_ram() down until an allocation succeeds - it consumes everything but a
	   32 KiB reserve - so this number is the budget a depth's engine init has to work
	   with for its raw (non-zone) allocations. Worth having in the log. */
	doom_sprintf(buf, (const uint8_t *)"[MTX] free DPMI after root Z_Init: %u B\n",
	             dpmi_get_ram());
	mtx_log((const char *)buf);

	/* What the engine's entry actually managed to write into its own three
	   segment globals, and where this copy of the code really lives. Needed
	   because the entry's own %ecx-relative stores are recorded as relocations
	   and therefore get the image base added TWICE: these numbers say whether
	   that corrupts them here, rather than leaving it to be inferred. */
	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] root: ace_seg %u code_seg %u data_seg %u boot_fn %u\n",
	             ace_segment, doom_code_segment, doom_data_segment,
	             (uint32_t)(uintptr_t)&mtx_engine_reserve);
	mtx_log((const char *)buf);

	/* Record a patch site and what is there BEFORE any depth runs, so the probe has
	   a "what it was" to compare against afterwards. */
	mtx_engine_probe_setup();
	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] probe site %u, byte there now %u\n",
	             mtx_hs.probe_addr, mtx_hs.probe_orig);
	mtx_log((const char *)buf);

	/* Measure, before pinning: Doom's own screen pointer and the copy ACE took of
	   it at init must be the same address. If they are not, the pin is aimed at
	   the wrong variable and this says so at boot rather than at the first
	   corrupted frame. */
	{
		uint32_t *var = (uint32_t *)(uintptr_t)(doom_data_segment + MTX_DOOM_SCREEN_VAR);
		doom_sprintf(buf, (const uint8_t *)
		             "[MTX] doom screens[0] at %u = %u; ace screen_buffer = %u\n",
		             (uint32_t)(uintptr_t)var, *var,
		             (uint32_t)(uintptr_t)screen_buffer);
		mtx_log((const char *)buf);
	}

	/* The same loader path, against the REAL engine image. A depth can fetch,
	   allocate for and prepare its own copy of ACE - verified by the version
	   dword, which is what tells a real image from an unrelated lump.

	   Entering it is deliberately NOT done yet, and it is worth being exact about
	   why: ACE's entry relocates itself, clears .bss and calls ace_main, which
	   allocates the whole of remaining RAM as its zone and re-initialises video.
	   Both are per-depth work that does not exist yet - see the README. So this is
	   load-and-verify, not run. */
	mtx_hs.ldr_ace_ok = 0;
	{
		int32_t r = mtx_ldr_load(MTX_ACE_LUMP, 0, ace_git_version, &entry, &total);
		if(r == 0)
		{
			doom_sprintf(buf, (const uint8_t *)
			             "[MTX] ace image: %u B at %u entry %u bss %u\n",
			             total, (uint32_t)(uintptr_t)mtx_hs.images[0], entry,
			             ((uint32_t *)mtx_hs.images[0])[0]);
			mtx_hs.ldr_ace_ok = 1;
		}
		else
			doom_sprintf(buf, (const uint8_t *)
			             "[MTX] ace image: load FAILED (%d) want %u slot %u\n",
			             r, ace_git_version, mtx_hs.image_bytes);
		mtx_log((const char *)buf);
	}

	/* TEMPORARY (measurement): how much of a depth's reserved stack does a real
	   nested Doom instance actually use?
	   The slot is painted with a pattern before the depth is first entered, and
	   the first word from the top that is no longer the pattern is the high-water
	   mark. This is what should size MTX_STACK_SIZE with a number instead of a
	   guess: that a depth's stack had to become PRIVATE is a separate fact from
	   how big it has to be, and the 64 KiB -> 256 KiB rise was precautionary. */
	if(mtx_stack_lo)
	{
		uint32_t i;
		for(i = 0; i < MTX_STACK_SIZE / 4u; i++)
			((uint32_t *)(uintptr_t)mtx_stack_lo)[i] = MTX_STACK_PAINT;
		mtx_stack_painted = 1;
		doom_sprintf(buf, (const uint8_t *)
		             "[MTX] depth1 stack slot [%u, %u) painted %u B for a high-water read\n",
		             mtx_stack_lo, mtx_stack_hi, MTX_STACK_SIZE);
		mtx_log((const char *)buf);
	}

	/* THE RING'S CHILDREN. One push per level, in order. mtx_push appends the next
	   slot, and because a child hands the frame back at its own boundary (through
	   the host's service) control RETURNS here between children - so the second
	   child boots exactly the way the first did, from the root's context, and each
	   gets its own arena slot, zone, raw pool, screen and stack. A refusal is
	   reported per child: "the ring is shorter than asked for" must not look like a
	   working nest. */
	{
		uint8_t d;

		for(d = 1; d <= (uint8_t)MTX_RING_CHILDREN; d++)
		{
			if(mtx_push(mtx_demo_child, (void *)(uintptr_t)d) == MTX_OK)
				mtx_child_live = 1;
			else
			{
				uint8_t msg[112];

				doom_sprintf(msg, (const uint8_t *)
				             "[MTX] could not create depth %u - ring is %u level(s)\n",
				             (uint32_t)d, (uint32_t)d - 1u);
				mtx_log((const char *)msg);
				break;
			}
		}
	}
}

/*
 * Video policy: choose which depth the screen shows, and say whether the screen
 * agreed. Runs at a frame boundary (like everything else here), so the source
 * dword is never changed mid-blit.
 */
static void mtx_engine_video(void)
{
	/* The host owns which buffer both depths present - and it names the ROOT's
	   frame for both. The root draws the inset into it; the depth, running its own
	   copy of this code, blits from it too (mtx_params.h: present_src). So the
	   screen shows one stable view with the nested instance inside it, instead of
	   flickering between two views as the depths take turns over the frame. */
	mtx_hs.params[1].present_src = (uint32_t)(uintptr_t)mtx_hs.screens[0];

	/* mtx_present() early-outs when the source already is this depth's, and it also
	   reads the screen back, so a policy that changes this later gets the check for
	   free. */
	if(mtx_hs.present_depth != 0u)
		mtx_present(0);
}

/*
 * TEMPORARY: is the depth really painting, and whose pixels are on the screen?
 *
 * The engine's frame count says the depth reached its frame boundary; this says
 * whether its frame buffer holds a picture and whether what the VGA shows is
 * that picture. Read straight out of the arena and out of video memory, so it
 * cannot be satisfied by a counter that merely increments.
 */
static void mtx_engine_depth_check(void)
{
	uint8_t buf[192];
	uint32_t *deep = (uint32_t *)mtx_hs.screens[1];
	uint32_t nz = 0, i;
	uint32_t vga = *(uint32_t *)(uintptr_t)0xA0000;

	if(!mtx_hs.params[1].frames || !deep || (mtx_frame_no % 50u))
		return;

	for(i = 0; i < MTX_SCREEN_FRAME_BYTES / 4u; i++)
		if(deep[i])
			nz++;

	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] depth1 frames %u, frame[0..1] %u %u, non-zero %u/%u, "
	             "vga %u %s\n",
	             mtx_hs.params[1].frames, deep[0], deep[1], nz,
	             (uint32_t)(MTX_SCREEN_FRAME_BYTES / 4u), vga,
	             (vga == *(uint32_t *)mtx_hs.screens[0]) ? "= root frame" :
	             (vga == deep[0]) ? "= depth1 frame" : "matches neither");
	mtx_log((const char *)buf);

	/* How much of the snapshot a switch actually copies, and whether the cold set
	   held up. Reported here because it is the whole point of the copyless path:
	   the warm-up is judged from these numbers, not trusted. */
	{
		uint32_t sv = 0, ld = 0, ever = 0, sw = 0, bad = 0;
		uint32_t snap = mtx_dos_snapshot_bytes();
		mtx_dos_copyless(&sv, &ld, &ever, &sw, &bad);
		doom_sprintf(buf, (const uint8_t *)
		             "[MTX] copyless: last save copied %u B, last load %u B, of a %u B "
		             "snapshot (%u.%u%%); hot set %u B over %u switches; %u promoted\n",
		             sv, ld, snap,
		             snap ? (sv * 1000u) / snap / 10u : 0u,
		             snap ? (sv * 1000u) / snap % 10u : 0u,
		             ever, sw, bad);
		mtx_log((const char *)buf);
	}
}

static void mtx_engine_switch(void);	/* the key and the inset, further down */
static void mtx_dump_words(const uint8_t *what, uint32_t addr);	/* TEMPORARY */

static void mtx_engine_switch(void);	/* the key and the inset, further down */
static void mtx_dump_words(const uint8_t *what, uint32_t addr);	/* TEMPORARY */

/*
 * TEMPORARY (measurement): the depth's stack high-water mark, read from the paint.
 *
 * The slot is painted before the depth is entered, so the highest word that is no
 * longer the pattern is the deepest this depth's own frames have reached. Frames
 * are never un-written as the stack unwinds, so everything between the deepest
 * point and the top has been written at some time - which is what makes a
 * single downward scan enough. The bottom word is reported separately: if the
 * depth had run off the end of its slot, the word below it would be painted over,
 * and that is the case the number must not be silent about.
 */
static void mtx_engine_stack_water(void)
{
	uint32_t *slot = (uint32_t *)(uintptr_t)mtx_stack_lo;
	uint32_t n = MTX_STACK_SIZE / 4u, i, used;
	uint8_t buf[192];

	if(!mtx_stack_painted || !mtx_stack_lo || (mtx_frame_no % 50u))
		return;

	/* Scan UP from the bottom, not down from the top: the top of the slot is
	   written by the SEED before the depth ever runs (edi/esi/ebx/ebp, the
	   trampoline address, the entry argument), so a downward scan stops on its
	   first word and reports 0 - measured, and it looked like the depth had not
	   touched its stack at all. The lowest word that is no longer the pattern is
	   the high-water mark; everything below it has never been written. */
	i = 0;
	while(i < n && slot[i] == MTX_STACK_PAINT)
		i++;
	used = (n - i) * 4u;

	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] depth1 stack: used %u B of %u (%u.%u%%), low-water at %u; "
	             "slot bottom %s\n",
	             used, MTX_STACK_SIZE,
	             (used * 1000u) / MTX_STACK_SIZE / 10u,
	             (used * 1000u) / MTX_STACK_SIZE % 10u,
	             mtx_stack_hi - used,
	             (slot[0] == MTX_STACK_PAINT) ? "intact" : "OVERWRITTEN");
	mtx_log((const char *)buf);
}

static void mtx_engine_policy(void)
{
	uint8_t buf[144];

	mtx_frame_no++;

	/* early frames = the room the demo starts in, which is the room the effect has
	   to be visible in */
	mtx_screen_paint();      /* the world's screens, after the frame is rendered */
	mtx_engine_video();
	mtx_engine_switch();
	mtx_engine_stack_water();
	mtx_present_confirm();
	mtx_engine_depth_check();
	mtx_engine_doom_screen_check();
	mtx_engine_probe_check();

	// instrumentation: prove the frame path keeps running after the first switch
	if(mtx_frame_no <= 40u || (mtx_frame_no % 50u) == 0u)
	{
		doom_sprintf(buf, (const uint8_t *)"[MTX] frame %u child=%u\n",
		             mtx_frame_no, (uint32_t)mtx_child_live);
		mtx_log((const char *)buf);
	}

	/* Once the loaded image has run to the end (it yields once, then finishes),
	   say what it left behind. This is the evidence that a code image fetched
	   from the WAD, parked at a depth-owned address and jumped to really ran
	   there: the guest reports the magic it wrote and the entry address it
	   computed for itself, and both must match what the loader used. */
	if(!mtx_hs.ldr_guest_ok && mtx_child_live && !mtx_alive(1)
	   && mtx_hs.ldr_report[1])
	{
		uint32_t ok = (mtx_hs.ldr_report[0] == MTX_LDR_GUEST_VERSION
		            && mtx_hs.ldr_report[1] == mtx_hs.ldr_guest_entry);
		doom_sprintf(buf, (const uint8_t *)
		             "[MTX] guest %s: magic %u addr %u loaded %u zone %u\n",
		             ok ? "OK" : "MISMATCH",
		             mtx_hs.ldr_report[0], mtx_hs.ldr_report[1],
		             mtx_hs.ldr_guest_entry, mtx_hs.ldr_report[3]);
		mtx_log((const char *)buf);
		mtx_hs.ldr_guest_ok = 1;
	}

	/* Take turns. A level hands the frame back at its own boundary (through the
	   host's service - see mtx_host_take_frame), and the root hands it over again
	   here. The gate is LIVENESS, not the old guest-demo flag: an engine instance
	   never finishes, it yields, so `mtx_child_live` (set when a pushed payload
	   returns) says nothing about it - measured, it stayed 0 while the depth was
	   running frames and handing them back. "Is there anyone to hand a frame to" is
	   the ring's own question, and from the root the answer is the root itself when
	   no child exists. */
	if(mtx_depth() != MTX_ROOT_DEPTH || mtx_ring_next(MTX_ROOT_DEPTH) == MTX_ROOT_DEPTH)
		return;

	if((mtx_frame_no % 10u) == 0u)
		mtx_dump_words("depth stack (ctx[1].esp) from the root",
		               (uint32_t)(uintptr_t)mtx_hs.ctx[1].esp);

	if((mtx_frame_no % MTX_DEMO_INTERVAL) == 0u)
	{
		/* The RING turns: the root hands the frame to the next live level, exactly
		   as every level hands it back to its own next one. */
		mtx_request_switch(mtx_ring_next(MTX_ROOT_DEPTH));
	}
}

/* TEMPORARY: dump a few words at an address, for comparing a depth's stack before
   the switch that saves it with the same stack at the switch that restores it. */
static void mtx_dump_words(const uint8_t *what, uint32_t addr)
{
	uint8_t buf[176];
	const uint32_t *p = (const uint32_t *)(uintptr_t)addr;

	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] %s at %u: %08X %08X %08X %08X %08X %08X\n",
	             what, addr, p[0], p[1], p[2], p[3], p[4], p[5]);
	mtx_log((const char *)buf);
}

/*
 * The host's frame service, as a depth sees it.
 *
 * Called BY a depth through the pointer in its parameter block (mtx_params.h), at
 * the depth's own frame boundary, to hand the frame back to the root. This code is
 * the host's - the pointer points into the root's image - so `mtx_hs` inside it is
 * the ROOT's state object: the contexts, the registries and the switch all belong
 * to the root, which is exactly what a depth cannot reach with its own copy of the
 * same functions.
 *
 * It is the same request/service pair the root uses when it wants the depth, so a
 * hand-back is not a special case: request, then service at the boundary. When the
 * root later switches back, this call RETURNS, on the depth's own stack, exactly
 * where it left off - that is what makes the two instances take turns.
 */
void mtx_host_take_frame(void)
{
	/* TEMPORARY: how far a hand-back gets. The host function runs on the DEPTH's
	   stack - that is the point of it - so if the switch it performs faults, these
	   lines are the only record of which part was reached: the depth's own logging
	   is the instance's copy of this engine, and it does not come back to say. */
	mtx_log("[MTX] hand-back: entered the host service\n");
	/* The RING, from a level's side: hand the frame to the NEXT live level, which
	   at the deepest level is the wrap back to the root. Hardcoding the root here
	   is what made this a two-body ping-pong rather than a ring of N. */
	mtx_request_switch(mtx_ring_next((uint8_t)mtx_depth()));
	mtx_log("[MTX] hand-back: request set, servicing\n");
	mtx_frame_tick();
	mtx_log("[MTX] hand-back: returned (the host handed the frame back)\n");
}

/*
 * The switch, and the inset it turns on.
 *
 * The switch is a key the ROOT reads while it runs - Doom's own key state, which
 * is per-depth, so the root reading it does not disturb the depth. It toggles on
 * the LEADING edge: a key held down is one switch, not one per frame.
 *
 * What it turns on is the picture that makes the nesting legible. A depth's frames
 * already reach the screen when that depth is the active one, and then they are
 * simply a Doom frame - the same level, the same status bar, and nothing on a
 * recording to say which of the two instances produced it. So instead the root
 * composites the depth's last frame INTO its own frame, at half scale in a boxed
 * corner: the outer instance renders the inner instance, which is what "Doom
 * inside Doom" has to look like to be worth recording.
 *
 * Runs at the frame boundary, i.e. after the 3D view has been rendered into this
 * depth's buffer and before the status bar is drawn at the bottom - so the inset
 * lands in the frame that gets blitted and nothing paints over it.
 */
/* The inset box in the depth whose frame is on screen: 320x200 space, with its
   border. Named here rather than repeated, because two functions now have to agree
   about exactly which rectangle is the host's. */
#define MTX_INSET_X  8u
#define MTX_INSET_Y  8u
#define MTX_INSET_W  160u
#define MTX_INSET_H  100u
#define MTX_INSET_BORDER 2u

/*
 * Spread ONE key observation to every participant.
 *
 * A press that spans a root slice and two child slices must be seen as one press by
 * all three: a tracker per side is exactly the double-toggle this replaced (measured
 * with two depths: one held key, inset ON at 384 and OFF at 391). The host's own copy
 * is kept in mtx_hs.key_down, the children's in their parameter blocks.
 */
static void mtx_key_track(uint32_t pressed)
{
	uint8_t k;

	for(k = 1; k < MTX_MAX_DEPTH; k++)
		if(mtx_alive(k))
			mtx_hs.params[k].key_down = pressed ? 1u : 0u;
	mtx_hs.key_down = pressed ? 1u : 0u;
}

static void mtx_engine_switch(void)
{
	uint8_t *src = mtx_hs.inset_depth ? (uint8_t *)mtx_hs.screens[mtx_hs.inset_depth] : 0;
	uint8_t *dst = (uint8_t *)screen_buffer;
	uint32_t y, x;
	uint8_t  dl;
	const uint32_t ix = 8u, iy = 8u, iw = 160u, ih = 100u;
	/* The key, from either side: the root's own leading edge, or a press the DEPTH
	   saw while it was the active one and recorded in its parameter block.
	   ONE shared tracker, in the depth's block: a physical press that spans a
	   depth slice and a root slice is seen by BOTH sides here (each one runs a
	   copy of this code), and with a tracker per side that was one press toggling
	   twice - measured: inset ON at frame 384, OFF at frame 391, from a single
	   held key. The block is host memory that both sides already read and write,
	   so it is the natural place for the one flag that says "the key was down
	   when I last looked". */
	/* EVERY live depth, not one fixed child. With a ring, any of the children can
	   have begun a slice since the host last looked, and each has its own stamp;
	   a single shared stamp let one child's slice start consume the other's, i.e.
	   one child's key state would never be distrusted. Note this code runs only in
	   the ROOT's copy of the engine - an instance returns earlier in the frame path
	   (its own branch) - so `mtx_alive` here is the host's answer, not an empty
	   object's. */
	for(dl = 1; dl < MTX_MAX_DEPTH; dl++)
	{
		if(!mtx_alive(dl) || mtx_hs.slice_seen[dl] == mtx_hs.params[dl].slice_gen)
			continue;

		mtx_hs.slice_seen[dl] = mtx_hs.params[dl].slice_gen;

		/* The slice-start window, but never LOWERING the dead time left by a
		   toggle: assigning it outright is what let a 30-frame dead time become 1
		   frame the moment a slice began, and one held key toggled again 8 frames
		   later (measured: ON 439, OFF 447). */
		if(mtx_hs.key_guard < MTX_KEY_GUARD_FRAMES)
			mtx_hs.key_guard = MTX_KEY_GUARD_FRAMES;

		/* Doom's key array is snapshot state, so what is restored here is what THIS
		   side last saw - and the events that changed it since were consumed by the
		   OTHER side. Measured, and it is why a held key toggled once per slice: the
		   keyup was processed by whichever depth ran then, so the other copied state
		   kept saying "down" forever and re-fired an edge at the start of every one
		   of its slices. Clearing the entry the slice starts on throws that stale
		   "down" away; a key that really is held re-asserts itself through the
		   depth's own key processing, and the shared tracker (which is NOT reset) is
		   what stops that from counting as a second press. */
		gamekeydown[MTX_SWITCH_KEY] = 0;
		mtx_hs.params[dl].key_down = 0;

		/* State and tracker have to be cleared TOGETHER: clearing only the state
		   left the tracker saying "already down", so the held key re-asserting
		   itself could never produce an edge again and the switch stopped working
		   altogether (measured: zero switches for a whole run). */
	}

	/* The key, from either side and from ANY depth: the root's own leading edge, or
	   a press a child saw while it was the active one and recorded in its own
	   block. Every live depth is checked - with two children, looking at one of
	   them means that child's presses are simply lost. */
	{
		uint8_t req = 0;

		for(dl = 1; dl < MTX_MAX_DEPTH; dl++)
			if(mtx_alive(dl) && mtx_hs.params[dl].switch_req)
			{
				mtx_hs.params[dl].switch_req = 0;
				req = 1;
			}

		{
			uint32_t pressed = gamekeydown[MTX_SWITCH_KEY] ? 1u : 0u;

			/* A REQUEST FROM A LEVEL IS NOT SUBJECT TO THE HOST'S GUARD. The level
			   already validated it against the same guard on its own side, and it
			   can only fire once per press of the key (its own tracker is set by the
			   same block). Dropping it here because the host happened to be inside
			   its window is how the ring lost the key entirely: measured, three
			   levels, not one switch line in a 30 s run, with the host's window up
			   on one frame in four - the one frame the child's edge landed on. */
			if(req || (!mtx_hs.key_guard && pressed && !mtx_hs.key_down))
			{
				mtx_hs.key_guard = MTX_KEY_DEBOUNCE_FRAMES;
				mtx_hs.inset_frame = mtx_frame_no;

				/* CYCLE the inset through the live depths: off, then each level, then
				   off again. The innermost level is the view worth reaching - its own
				   walls show the level outside it - and a bool cannot say which one
				   that is. */
				{
					uint8_t next = 0, d2;

					for(d2 = (uint8_t)(mtx_hs.inset_depth + 1u); d2 < MTX_MAX_DEPTH; d2++)
						if(mtx_alive(d2))
						{
							next = d2;
							break;
						}
					mtx_hs.inset_depth = next;
				}
				mtx_hs.inset_on = mtx_hs.inset_depth ? 1u : 0u;
				{
					uint8_t buf[160];
					doom_sprintf(buf, (const uint8_t *)
					             "[MTX] switch: key '%c' -> inset depth %u at frame %u\n",
					             (uint32_t)MTX_SWITCH_KEY,
					             (uint32_t)mtx_hs.inset_depth, mtx_frame_no);
					mtx_log((const char *)buf);
				}

				/* the press is consumed for everyone, on the toggle frame too */
				mtx_key_track(pressed);
			}
			else if(mtx_hs.key_guard)
			{
				/* Inside the window the key state is what a depth's snapshot restored,
				   not what this side last saw: writing the tracker from it is what
				   produced a phantom second press (measured: ON at 381 and OFF at 394
				   from one held key). Leave the trackers ALONE - that is the whole
				   point of the window - and just run it down. */
				mtx_hs.key_guard--;
			}
			else
				mtx_key_track(pressed);
		}
	}

	/* Tell every running depth what the picture is: which buffer is the canvas,
	   whether an inset is up, and WHICH level its screen-walls show (mtx_params.h).
	   Host memory the instances already read, so this is only the host keeping the
	   shared description current - without it a depth cannot know that its own view
	   is supposed to be on screen, and leaves the canvas showing the suspended
	   root's last frame. Every live depth, because the answers now DIFFER per
	   depth: the ring shows level d the frame of the next level, and the deepest
	   level the root's. */
	for(dl = 1; dl < MTX_MAX_DEPTH; dl++)
	{
		if(!mtx_alive(dl))
			continue;
		mtx_hs.params[dl].canvas     = (uint32_t)(uintptr_t)mtx_hs.screens[0];
		mtx_hs.params[dl].inset_on   = mtx_hs.inset_on;
		mtx_hs.params[dl].screen_src =
			(uint32_t)(uintptr_t)mtx_hs.screens[mtx_ring_next(dl)];
	}

	if(!mtx_hs.inset_depth || !src || !dst || !mtx_hs.params[mtx_hs.inset_depth].frames)
		return;

	/* half scale: every other pixel of the depth's 320x200 frame */
	for(y = 0; y < ih; y++)
		for(x = 0; x < iw; x++)
			dst[(iy + y) * 320u + (ix + x)] = src[(y * 2u) * 320u + (x * 2u)];

	/* a border, so the inset reads as an inset and not as part of the level */
	for(x = ix - 2u; x < ix + iw + 2u; x++)
	{
		dst[(iy - 2u) * 320u + x] = 4u;
		dst[(iy - 1u) * 320u + x] = 4u;
		dst[(iy + ih) * 320u + x] = 4u;
		dst[(iy + ih + 1u) * 320u + x] = 4u;
	}
	for(y = iy - 2u; y < iy + ih + 2u; y++)
	{
		dst[y * 320u + (ix - 2u)] = 4u;
		dst[y * 320u + (ix - 1u)] = 4u;
		dst[y * 320u + (ix + iw)] = 4u;
		dst[y * 320u + (ix + iw + 1u)] = 4u;
	}

	mtx_hs.inset_logged++;
}

/*
 * WHY A DEPTH DOES NOT PAINT THE MAIN VIEW (measured, and it is a trade-off with
 * only one good answer at fine slices).
 *
 * The main area of the canvas has to come from ONE instance. With a depth painting
 * its own view there, the picture alternates between the two instances every time
 * control does - which was fine at 30-frame slices and wrong at 1-frame slices:
 * measured on adjacent captured frames (0.05 s apart), 50% of the main view and 67%
 * of the status bar differed, i.e. the numbers on the status bar were switching
 * between two sessions faster than anyone can read them.
 *
 * What that function was fixing - the screen freezing for the whole of a depth's
 * slice - is 3.90 s at 30-frame slices and ONE FRAME at 1-frame slices, i.e.
 * invisible. So: fine slices make the freeze irrelevant, the main view stays the
 * root's, and the depth's frames reach the screen through the host's inset, which
 * composites the depth's newest frame on every root frame.
 */

/*
 * Is the depth really painting, and whose pixels are on the screen?
 *
 * Runs in the instance's own copy of the engine (its frame boundary reaches here),
 * so it compares what video memory holds with the very buffer this depth draws
 * into - and counts how many of the 16000 dwords agree. Nothing roundabout: if the
 * screen were showing the root's frame instead, the count would be small, and it is
 * the instance's own memory being compared either way.
 *
 * The three addresses say the same thing from the other side: the region the HOST
 * set aside, the engine's own screen_buffer as pinned for this depth, and Doom's
 * own screens[0] - the renderer writes through the last one. If they ever disagree,
 * the picture on the screen is not coming from where the host thinks it is.
 */
static void MTX_FRAME_TICK_ABI mtx_engine_self_check(mtx_depth_params_t *me)
{
	uint8_t buf[256];
	uint8_t *mine = (uint8_t *)me->screen;
	uint32_t *sb = (uint32_t *)screen_buffer;
	uint32_t *dv = (uint32_t *)(uintptr_t)(doom_data_segment + MTX_DOOM_SCREEN_VAR);
	const uint32_t ix = 8u, iy = 8u, iw = 160u, ih = 100u;
	uint32_t same = 0, y, x;
	const uint8_t *vga = (const uint8_t *)(uintptr_t)0xA0000;

	if(!mine || (me->frames % MTX_SELF_CHECK_EVERY) != 0u)
		return;

	/* Where this depth's pixels are on the screen: the host composites this
	   depth's frame into the ROOT's frame as a half-scale inset, and the host has
	   both depths present the root's frame - so the inset is the one place the
	   nested picture appears, and this reads it back from video memory. Every other
	   pixel of the inset area must equal the sampled pixel of this depth's frame;
	   if the inset were showing something else, this count would not be near-full. */
	for(y = 0; y < ih; y++)
		for(x = 0; x < iw; x++)
			if(vga[(iy + y) * 320u + (ix + x)] == mine[(y * 2u) * 320u + (x * 2u)])
				same++;

	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] depth %u frame %u: screen region %u, engine screen_buffer %u, "
	             "doom screens[0] %u; the host's inset shows THIS depth's frame in "
	             "%u/%u pixels (exact while neither view moves; this depth's own "
	             "buffer has moved on by the time it is read back)\n",
	             (uint32_t)me->depth, me->frames,
	             (uint32_t)(uintptr_t)mine, (uint32_t)(uintptr_t)sb,
	             (uint32_t)(uintptr_t)*dv, same, iw * ih);
	mtx_log((const char *)buf);
}

/* Single entry point for the frame path (engine/vesa.c). Same call convention as
   the function that calls it, so it is safe inside swap_buffers(). */
void MTX_FRAME_TICK_ABI mtx_engine_frame(void)
{
	/* An INSTANCE is not a host, and it reaches this same line every frame: a
	   depth installs the same Doom patches, so Doom's frame boundary calls the
	   depth's own copy of this function. Booting a host from in there is
	   nonsense - it has no arena and must never claim one (its own boot said so:
	   "no reserved arena; host disabled").
	   What it can do instead is count its frames where the host can see them.
	   The parameter block it was entered with lives in the HOST's memory, so one
	   increment here is the host's evidence that this depth is not merely
	   booted: it is drawing. */
	mtx_depth_params_t *me = mtx_self();

	if(me)
	{
		me->frames++;

		/* TEMPORARY: the first frame this depth draws, said out loud. Everything
		   else about a stuck depth is guesswork - the screen looks the same
		   whether it never reached its frame path or reached it and did nothing -
		   so this is the line that separates the two. */
		if(me->frames == 1u)
		{
			uint8_t msg[160];
			doom_sprintf(msg, (const uint8_t *)
			             "[MTX] depth %u drew its first frame: screen %u, present_src "
			             "%u, host_yield %u\n",
			             (uint32_t)me->depth, (uint32_t)(uintptr_t)me->screen,
			             me->present_src, (uint32_t)(uintptr_t)me->host_yield);
			mtx_log((const char *)msg);
		}

		/* TEMPORARY: ESP at the frame boundary, and where the depth's own stack
		   slot is. The boot trace shows ESP staying in the arena from the entry
		   through init_map, so whatever moves it does so between the engine's
		   entry and this line - and this line says which side of that it is. */
		if(me->frames <= 2u)
			mtx_esp_probe((const uint8_t *)"frame_hook");

		mtx_engine_self_check(me);

		/* The HOST owns the presentation. This depth blits from whatever buffer the
		   host named - its own copy of the code, so it can do it itself, and no
		   address arithmetic is needed to find it. Both depths naming the root's
		   frame is what keeps the screen from flickering between two views while
		   they take turns: the running depth renders into its own buffer and
		   presents the root's, and the nested picture reaches the screen where the
		   host puts it - in the inset. */
		/* This depth's own screen is a world too: paint its screen-textured walls
		   from its parent's frame, so the nesting is visible inside the nested
		   view as well as around it. */
		mtx_screen_paint();

		if(me->present_src)
			*((uint32_t *)((uint8_t *)(void *)I_FinishUpdate + 4)) = me->present_src;

		/* The switch key: Doom's key state is per-depth, so a press that lands
		   during THIS depth's slice is invisible to the host. Record it here, in
		   host memory, for the host to apply - and track this depth's own previous
		   state so one press is one switch, not one per frame it is held.

		   The guard is not decoration. Doom's key array is Doom's DATA, so it is
		   part of every depth's snapshot and the switch restores it: the first
		   frames of each slice therefore show whatever this depth last saw, which
		   reads as a fresh press - measured, one held key toggling the inset once
		   per slice (ON 372, OFF 391, ON 421, OFF 451...), i.e. a flicker instead
		   of a switch. A real press lasts far longer than the guard window. */
		if(((me->frames - 1u) % MTX_DEMO_INTERVAL) >= MTX_KEY_GUARD_FRAMES)
		{
			if(gamekeydown[MTX_SWITCH_KEY] && !me->key_down)
				me->switch_req = 1u;
			me->key_down = gamekeydown[MTX_SWITCH_KEY] ? 1u : 0u;
		}

		/* This depth's slice begins when its own frame count starts a new run - and
		   that is exactly where its key state has to be distrusted: the array is
		   snapshot state, so it says what THIS depth last saw, and the events since
		   were consumed by the other one. Measured: without this, a held key
		   toggled once per slice, forever (the other side never processed the keyup,
		   so the copy kept saying "down"). Clearing the one entry we care about is
		   enough, and the shared key_down tracker is deliberately kept, so a key
		   that re-asserts itself through this depth's own key processing does not
		   read as a fresh press. */
		if((me->frames % MTX_DEMO_INTERVAL) == 1u)
		{
			me->slice_gen++;
			gamekeydown[MTX_SWITCH_KEY] = 0;
			me->key_down = 0;	/* clear with the state, not instead of it */
		}

		/* Hand the frame back, every MTX_DEMO_INTERVAL frames, through the HOST's
		   own service (see the parameter block). This is what makes the two
		   instances take turns instead of one of them winning forever - and taking
		   turns is what keeps the outer instance's view animating while the inset
		   shows the inner one. A depth cannot switch with its own copy of the
		   scheduler: the contexts are in the host's state object. */
		if(me->host_yield && (me->frames % MTX_DEMO_INTERVAL) == 0u)
		{
			/* TEMPORARY: what a hand-back looks like from inside the depth. The
			   instance's own frame counter is invisible to a host that is not
			   running, so without this there is no way to tell "the depth never
			   reached its frame path" from "the depth reached it and the hand-back
			   did nothing" - and the screen looks the same either way. */
			uint8_t msg[96];
			uint32_t sp;

			doom_sprintf(msg, (const uint8_t *)
			             "[MTX] instance hand-back at frame %u\n", me->frames);
			mtx_log((const char *)msg);

			/* TEMPORARY: the LIVE stack of the depth, taken here so it can be
			   compared with ctx[1].esp as the root sees it a moment later. If the
			   saved frame is intact, these agree; the root's own frames grow down
			   from a nearby address, so this is the check that says whether the two
			   depths are sharing stack memory. */
			__asm__ __volatile__("mov %%esp, %0" : "=r"(sp));
			mtx_dump_words("instance live esp", sp);

			me->host_yield();

			doom_sprintf(msg, (const uint8_t *)
			             "[MTX] instance resumed, frame %u\n", me->frames);
			mtx_log((const char *)msg);
		}

		return;
	}

	if(!mtx_booted)
	{
		/* Boot where the host has always booted: the first frame of a LEVEL. This
		   hook now fires in every gamestate (see draw_c.c), and booting from a title
		   screen was measured to leave a nested instance's own boot stuck - it
		   reached count_textures() and never reached tex_init_data(), with no error
		   anywhere. The frame PATH must not depend on the gamestate, which is the
		   whole reason for hooking here; only the boot waits for the state the rest
		   of this file was measured in. */
		if(gamestate != GS_LEVEL)
			return;

		mtx_engine_boot();
	}

	if(!mtx_boot_failed)
		mtx_engine_policy();

	mtx_frame_tick();
}
