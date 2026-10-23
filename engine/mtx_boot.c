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

	/* A nested depth reaches this too: its ACE image runs ace_main, which calls
	   Z_Init. The arena belongs to the MACHINE, not to a depth - the root has
	   already claimed it, so a second claim must be a no-op. Re-claiming would
	   fail (the memory is gone) and a depth would silently get a smaller arena
	   than the one the planner budgeted against. */
	if(mtx_hs.arena)
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
	uint8_t d = mtx_depth();
	memzone_t *save;
	void *mem, *p = 0, *q = 0;
	int bad, after;
	uint8_t buf[176];

	if(d >= MTX_MAX_DEPTH)
		return 0;
	mem = mtx_hs.heaps[d];
	if(!mem)
		return 0;

	mtx_zone_build(mem, MTX_ZONE_PER_DEPTH);
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
	             "[MTX] depth%u zone %u B at %u build %d alloc %u probe %d\n",
	             (uint32_t)d, MTX_ZONE_PER_DEPTH, (uint32_t)(uintptr_t)mem,
	             bad, (uint32_t)(uintptr_t)p, after);
	mtx_log((const char *)buf);

	if(bad || after || !p || !q)
		return 0;
	return mem;
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
#define MTX_PRESENT_FROM   10u
#define MTX_PRESENT_UNTIL  200u

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

	int32_t r;

	(void)arg;
	mtx_log("[MTX] depth1 entered\n");

	r = mtx_ldr_load(MTX_GUEST_LUMP, 1, MTX_LDR_GUEST_VERSION, &entry, &total);
	if(r != 0)
	{
		doom_sprintf(buf, (const uint8_t *)
		             "[MTX] depth1: guest image load FAILED (%d)\n", r);
		mtx_log((const char *)buf);
		mtx_finish();
	}

	doom_sprintf(buf, (const uint8_t *)"[MTX] depth1: guest alloc %u B at %u, entering\n",
	             total, entry);
	mtx_log((const char *)buf);

	mtx_hs.ldr_guest_entry = entry;
	mtx_hs.ldr_guest_total = total;
	mtx_hs.ldr_report[0] = 0;
	mtx_hs.ldr_report[1] = 0;
	mtx_hs.ldr_report[2] = 0;
	mtx_hs.ldr_report[3] = 0;
	mtx_hs.ldr_report[4] = 0;

	/* The whole authority the loaded image has over the machine: leave evidence,
	   ask for a zone of its own, draw on its OWN screen, yield, finish. No free,
	   no close, no arbitrary allocation - the same reasoning as ldr1n's service
	   table, so a guest cannot damage the parent. Both of the middle two are the
	   engine's own nested paths, not extra privilege: a real nested instance gets
	   the same heap by running ace_main and the same screen pointer by the
	   per-depth pin. */
	mtx_hs.ldr_svc[0] = (uint32_t)(uintptr_t)mtx_hs.ldr_report;
	mtx_hs.ldr_svc[1] = (uint32_t)(uintptr_t)mtx_yield;
	mtx_hs.ldr_svc[2] = (uint32_t)(uintptr_t)mtx_finish;
	mtx_hs.ldr_svc[3] = (uint32_t)(uintptr_t)mtx_engine_zone_init;
	mtx_hs.ldr_svc[4] = (uint32_t)(uintptr_t)mtx_engine_screen_fill;
	mtx_hs.ldr_svc[5] = (uint32_t)(uintptr_t)mtx_engine_doom_screen;
	mtx_hs.ldr_svc[6] = (uint32_t)(uintptr_t)mtx_engine_probe_patch;

	mtx_ldr_enter(entry);		// hands off through ldr0N; never returns

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

	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] screen region %u B/depth (frame %u + loader %u + parse %u)\n",
	             MTX_SCREEN_BYTES, MTX_SCREEN_FRAME_BYTES,
	             MTX_SCREEN_LOADER_BYTES, MTX_SCREEN_PARSE_BYTES);
	mtx_log((const char *)buf);

	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] isolation: %u regions, %u patches, snapshot %u B\n",
	             mtx_dos_region_count(), mtx_dos_hook_count(),
	             bt.snapshot_per_depth);
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

	if(mtx_push(mtx_demo_child, 0) == MTX_OK)
		mtx_child_live = 1;
	else
		mtx_log("[MTX] could not create depth 1\n");
}

/*
 * Video policy: choose which depth the screen shows, and say whether the screen
 * agreed. Runs at a frame boundary (like everything else here), so the source
 * dword is never changed mid-blit.
 */
static void mtx_engine_video(void)
{
	uint8_t want;
	uint8_t buf[144];

	/* nothing to show until the guest has drawn on its own screen */
	if(!mtx_hs.ldr_report[2])
		return;

	want = (mtx_frame_no >= MTX_PRESENT_FROM && mtx_frame_no < MTX_PRESENT_UNTIL)
	     ? 1u : 0u;
	if(want == mtx_hs.present_depth)
		return;

	mtx_present(want);
	doom_sprintf(buf, (const uint8_t *)
	             "[MTX] present -> depth %u, buffer %u (blit runs this frame; "
	             "checked next frame)\n",
	             (uint32_t)want, (uint32_t)(uintptr_t)mtx_hs.screens[want]);
	mtx_log((const char *)buf);
}

static void mtx_engine_policy(void)
{
	uint8_t buf[144];

	mtx_frame_no++;

	mtx_engine_video();
	mtx_present_confirm();
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
