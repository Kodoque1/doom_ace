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

#define MTX_ZONE_PER_DEPTH  (256u * 1024u)
#define MTX_DEMO_INTERVAL   30u

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
 * Reserve the arena for the nested instances. MUST run before ACE's Z_Init takes
 * the rest of RAM: Z_Init walks dpmi_get_ram() down until malloc succeeds, i.e.
 * it consumes everything except a 32 KiB reserve, so an arena requested later
 * can never be satisfied. This is called from the top of Z_Init.
 */
void MTX_FRAME_TICK_ABI mtx_engine_reserve(void)
{
	static const uint32_t sizes[] = {3u, 2u, 0u};	// MiB
	uint32_t i;

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
static void mtx_log(const char *s)
{
	const char *p = s;
	uint32_t n = 0;

	if(mtx_log_fd < 0)
		mtx_log_fd = doom_open_WR("MTX.LOG");
	if(mtx_log_fd < 0)
		return;

	while(p[n])
		n++;
	doom_write(mtx_log_fd, (void *)s, n);
}

// Depth 1's entry. It loads a code image of its own out of the WAD and runs it.
//
// The image is a real WAD lump, read through the engine's WAD layer, and it lands
// in this depth's own arena slot - so the bytes that execute here belong to this
// depth alone. That matters: ACE patches code at run time (its hook tables), so
// two depths sharing one code page would fight over the same bytes.
static void mtx_demo_child(void *arg)
{
	uint8_t buf[128];
	uint32_t entry = 0, total = 0;

	(void)arg;
	mtx_log("[MTX] depth1 entered\n");

	if(mtx_ldr_load(MTX_GUEST_LUMP, 1, MTX_LDR_GUEST_VERSION,
	                &entry, &total) != 0)
	{
		mtx_log("[MTX] depth1: guest image load FAILED\n");
		mtx_finish();
	}

	doom_sprintf(buf, (const uint8_t *)"[MTX] depth1: guest alloc %u B at %u, entering\n",
	             total, entry);
	mtx_log((const char *)buf);

	mtx_hs.ldr_guest_entry = entry;
	mtx_hs.ldr_guest_total = total;
	mtx_hs.ldr_report[0] = 0;
	mtx_hs.ldr_report[1] = 0;

	/* The whole authority the loaded image has over the machine: leave evidence,
	   yield, finish. No free, no close, no way to allocate - the same reasoning
	   as ldr1n's service table, so a guest cannot damage the parent. */
	mtx_hs.ldr_svc[0] = (uint32_t)(uintptr_t)mtx_hs.ldr_report;
	mtx_hs.ldr_svc[1] = (uint32_t)(uintptr_t)mtx_yield;
	mtx_hs.ldr_svc[2] = (uint32_t)(uintptr_t)mtx_finish;

	mtx_ldr_enter(entry);		// hands off through ldr0N; never returns

	mtx_log("[MTX] depth1: enter returned (cannot happen)\n");
	mtx_finish();
}

static void mtx_engine_boot(void)
{
	mtx_boot_t bt;
	mtx_host_ops_t ops;	// taken by value by mtx_init, so a local is fine
	uint8_t buf[128];
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
	                mtx_hs.image_bytes,
	                mtx_dos_wire_register, mtx_heaps, mtx_screens,
	                mtx_hs.images, &bt) != 0)
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

	doom_sprintf(buf, (const uint8_t *)"[MTX] host up: depths %u snap %u img %u\n",
	             (uint32_t)bt.depth, bt.snapshot_per_depth, mtx_hs.image_bytes);
	mtx_log((const char *)buf);

	/* The same loader path, against the REAL engine image. A depth can fetch,
	   allocate for and prepare its own copy of ACE - verified by the version
	   dword, which is what tells a real image from an unrelated lump.

	   Entering it is deliberately NOT done yet, and it is worth being exact about
	   why: ACE's entry relocates itself, clears .bss and calls ace_main, which
	   allocates the whole of remaining RAM as its zone and re-initialises video.
	   Both are per-depth work that does not exist yet - see the README. So this is
	   load-and-verify, not run. */
	if(mtx_ldr_load(MTX_ACE_LUMP, 0, ace_git_version, &entry, &total) == 0)
	{
		doom_sprintf(buf, (const uint8_t *)
		             "[MTX] ace image: %u B at %u entry %u bss %u\n",
		             total, (uint32_t)(uintptr_t)mtx_hs.images[0], entry,
		             ((uint32_t *)mtx_hs.images[0])[0]);
		mtx_log((const char *)buf);
		mtx_hs.ldr_ace_ok = 1;
	}
	else
		mtx_log("[MTX] ace image: load FAILED\n");

	if(mtx_push(mtx_demo_child, 0) == MTX_OK)
		mtx_child_live = 1;
	else
		mtx_log("[MTX] could not create depth 1\n");
}

static void mtx_engine_policy(void)
{
	uint8_t buf[80];

	mtx_frame_no++;

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
		doom_sprintf(buf, (const uint8_t *)"[MTX] guest %s: magic %u addr %u loaded %u\n",
		             ok ? "OK" : "MISMATCH",
		             mtx_hs.ldr_report[0], mtx_hs.ldr_report[1],
		             mtx_hs.ldr_guest_entry);
		mtx_log((const char *)buf);
		mtx_hs.ldr_guest_ok = 1;
	}

	if(!mtx_child_live)
		return;

	/* the guest ends the depth when it finishes; once it is gone there is
	   nothing left to switch to, so stop asking */
	if(mtx_depth() == MTX_ROOT_DEPTH && !mtx_alive(1))
		return;

	if((mtx_frame_no % MTX_DEMO_INTERVAL) == 0u)
	{
		mtx_log("[MTX] request switch -> 1\n");
		mtx_request_switch(1);		// serviced by mtx_frame_tick below
	}
}

/* Single entry point for the frame path (engine/vesa.c). Same call convention as
   the function that calls it, so it is safe inside swap_buffers(). */
void MTX_FRAME_TICK_ABI mtx_engine_frame(void)
{
	if(!mtx_booted)
		mtx_engine_boot();

	if(!mtx_boot_failed)
		mtx_engine_policy();

	mtx_frame_tick();
}
