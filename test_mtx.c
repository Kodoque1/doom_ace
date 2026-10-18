/*
 * matryoshka/test_mtx.c
 *
 * Host-side proof that the context switch and scheduler actually run: a
 * 32-bit freestanding binary (no libc, raw int 0x80 syscalls) that exercises
 * nesting, yield/go-back, per-depth state isolation, the depth ceiling and the
 * save/load ordering - then reports PASS/FAIL on stdout.
 *
 * This is the same mtx_core.c + mtx_switch.S that will be linked into ACE; only
 * the runtime shim at the top is host-specific.
 */
#include "context.h"
#include "dos_state.h"
#include "dos_alloc.h"
#include "ldr1n.h"
#include "mtx_sched.h"

/* ---------------------------------------------------------------- runtime */
/* int 0x80 clobbers ecx and edx (Linux i386 ABI), so declare them modified or
   gcc will keep live values in them across the syscall. ebx/esi/edi/ebp are
   preserved by the kernel and need no clobber. */
static int sys_write(int fd, const char *b, unsigned n)
{
	int r;
	__asm__ volatile ("int $0x80"
	                  : "=a"(r), "+c"(b), "+d"(n)
	                  : "a"(4), "b"(fd)
	                  : "memory", "cc");
	return r;
}
static void sys_exit(int c)
{
	__asm__ volatile ("int $0x80" :: "a"(1), "b"(c) : "memory", "cc");
}
static void putc_(char c)       { sys_write(1, &c, 1); }
static void puts_(const char *s){ unsigned n = 0; while(s[n]) n++; sys_write(1, s, n); }
static void putu(unsigned v)
{
	char b[12]; int i = 0;
	if(!v){ putc_('0'); return; }
	while(v){ b[i++] = (char)('0' + (v % 10)); v /= 10; }
	while(i) putc_(b[--i]);
}

/* ------------------------------------------------------------------ trace */
#define MAXT 512
static char TR[MAXT];
static int  TN;
static void tr_reset(void)          { TN = 0; }
static void t(char c)               { if(TN < MAXT) TR[TN++] = c; }
static int  tr_eq(const char *exp)
{
	int i = 0;
	while(exp[i]) { if(i >= TN || TR[i] != exp[i]) return 0; i++; }
	return i == TN;
}

/* ------------------------------------------------ test 1: plain nesting ---- */
static void t1_grand(void *a) { (void)a; t('E'); mtx_finish(); }
static void t1_child(void *a) { (void)a; t('C'); mtx_push(t1_grand, 0); t('D'); mtx_finish(); }
static int  test1(void)
{
	tr_reset();
	t('A');
	mtx_push(t1_child, 0);      /* child runs E, D, then returns here */
	t('B');
	return tr_eq("ACEDB");
}

/* ---------------------------- test 2: switch_to both ways + state kept ---- */
static void t2_b(void *a)
{
	int n = 0, i;
	(void)a;
	for(i = 0; i < 3; i++) { n++; t('b'); t((char)('0' + n)); mtx_switch_to(1); }
	mtx_finish();
}
static void t2_a(void *a)
{
	int n = 0, i;
	(void)a;
	mtx_push(t2_b, 0);          /* b starts, then immediately switches back */
	for(i = 0; i < 3; i++) { n++; t('a'); t((char)('0' + n)); mtx_switch_to(2); }
	mtx_finish();
}
static int test2(void)
{
	char sa[8], sb[8];
	int ia = 0, ib = 0, i, ok;
	tr_reset();
	mtx_push(t2_a, 0);
	for(i = 0; i < TN; i++)
	{
		if(TR[i] == 'a' && i + 1 < TN) sa[ia++] = TR[i + 1];
		if(TR[i] == 'b' && i + 1 < TN) sb[ib++] = TR[i + 1];
	}
	sa[ia] = 0; sb[ib] = 0;
	ok = ia == 3 && ib == 3
	  && sa[0] == '1' && sa[1] == '2' && sa[2] == '3'
	  && sb[0] == '1' && sb[1] == '2' && sb[2] == '3';
	return ok;
}

/* ------------------------------------------------- test 3: depth ceiling -- */
static int maxd_seen;
static void t3_deep(void *a)
{
	(void)a;
	if(mtx_depth() > maxd_seen) maxd_seen = mtx_depth();
	if(mtx_push(t3_deep, 0) != MTX_OK)
		t('L');                 /* refused at the ceiling */
	mtx_finish();
}
static int test3(void)
{
	int i, lc = 0;
	maxd_seen = 0;
	tr_reset();
	mtx_push(t3_deep, 0);
	for(i = 0; i < TN; i++)
		if(TR[i] == 'L') lc++;
	return maxd_seen == (int)mtx_max_depth() - 1 && lc == 1;
}

/* ------------------------------------------- test 4: save/load ordering --- */
static char OT[64];
static int  ON;
static void ops_save(mtx_context_t *o) { if(ON < 62) { OT[ON++] = 'S'; OT[ON++] = (char)('0' + o->depth); } }
static void ops_load(mtx_context_t *i) { if(ON < 62) { OT[ON++] = 'L'; OT[ON++] = (char)('0' + i->depth); } }
static const mtx_host_ops_t OPS = { ops_save, ops_load };

static void t4_child(void *a) { (void)a; mtx_finish(); }
static int test4(void)
{
	ON = 0;
	mtx_init(MTX_MAX_DEPTH, &OPS, 0, 0);
	mtx_push(t4_child, 0);
	/* down: save(0) load(1); on the child's return: save(1) load(0) */
	return ON == 8
	    && OT[0] == 'S' && OT[1] == '0' && OT[2] == 'L' && OT[3] == '1'
	    && OT[4] == 'S' && OT[5] == '1' && OT[6] == 'L' && OT[7] == '0';
}

/* --------------------------------------- test 5: genuinely separate stacks */
static unsigned esp_of(void)
{
	unsigned e;
	__asm__ volatile ("mov %%esp, %0" : "=r"(e));
	return e;
}
static unsigned e_child, e_grand;
static void t5_grand(void *a) { (void)a; e_grand = esp_of(); mtx_finish(); }
static void t5_child(void *a) { (void)a; e_child = esp_of(); mtx_push(t5_grand, 0); t('K'); mtx_finish(); }
static int test5(void)
{
	unsigned e_root;
	tr_reset();
	e_root = esp_of();
	mtx_push(t5_child, 0);
	return e_root != e_child && e_child != e_grand && e_root != e_grand && tr_eq("K");
}

/* --------------------------- test 6: DOS state snapshot + real isolation -- */
static uint8_t  fake_doomdata[64];
static void    *fake_mainzone;
static void    *fake_screen;
static uint32_t fake_hookA, fake_hookB;

static uint8_t heap_root[16], heap_1[16], heap_2[16];
static uint8_t fb_root[16],   fb_1[16],   fb_2[16];

static int s6_fail;
static void chk(int cond, char tag)
{
	if(!cond) { s6_fail++; t('!'); t(tag); }
}

static void t6_child2(void *a)
{
	(void)a;
	chk(fake_doomdata[0] == 0xB1, 'a');	/* clone of parent (child1) */
	chk(fake_mainzone   == heap_2, 'b');	/* but its OWN heap ... */
	chk(fake_screen     == fb_2,   'c');	/* ... and its OWN framebuffer */
	chk(fake_hookA      == 0xBBBB, 'd');	/* cloned patch bytes */
	fake_doomdata[0] = 0xC2;
	fake_hookA = 0xCCCC;
	mtx_finish();
}

static void t6_child1(void *a)
{
	(void)a;
	chk(fake_doomdata[0] == 0xA0, 'e');
	chk(fake_mainzone   == heap_1, 'f');
	chk(fake_screen     == fb_1,   'g');
	chk(fake_hookA      == 0xAAAA, 'h');
	fake_doomdata[0] = 0xB1;
	fake_hookA = 0xBBBB;

	mtx_push(t6_child2, 0);

	/* child2 must not have leaked into us */
	chk(fake_doomdata[0] == 0xB1, 'i');
	chk(fake_mainzone   == heap_1, 'j');
	chk(fake_screen     == fb_1,   'k');
	chk(fake_hookA      == 0xBBBB, 'l');
	mtx_finish();
}

static int test6(void)
{
	mtx_host_ops_t ops;

	s6_fail = 0;
	tr_reset();

	mtx_dos_init(MTX_MAX_DEPTH, 0, 0);
	mtx_dos_add_region(fake_doomdata, sizeof(fake_doomdata));
	mtx_dos_add_hook(&fake_hookA, 4);
	mtx_dos_add_hook(&fake_hookB, 4);
	mtx_dos_set_pin(&fake_mainzone, MTX_PIN_MAINZONE);
	mtx_dos_set_pin(&fake_screen,   MTX_PIN_SCREEN);
	mtx_dos_set_value(0, MTX_PIN_MAINZONE, heap_root);
	mtx_dos_set_value(1, MTX_PIN_MAINZONE, heap_1);
	mtx_dos_set_value(2, MTX_PIN_MAINZONE, heap_2);
	mtx_dos_set_value(0, MTX_PIN_SCREEN, fb_root);
	mtx_dos_set_value(1, MTX_PIN_SCREEN, fb_1);
	mtx_dos_set_value(2, MTX_PIN_SCREEN, fb_2);

	/* root establishes the state the children will inherit */
	fake_doomdata[0] = 0xA0;
	fake_hookA   = 0xAAAA;
	fake_mainzone = heap_root;
	fake_screen   = fb_root;

	ops.save_state = mtx_dos_save_state;
	ops.load_state = mtx_dos_load_state;
	mtx_init(MTX_MAX_DEPTH, &ops, 0, 0);

	mtx_push(t6_child1, 0);

	/* back at the root: nothing the children did may survive */
	chk(fake_doomdata[0] == 0xA0, 'm');
	chk(fake_mainzone   == heap_root, 'n');
	chk(fake_screen     == fb_root,   'o');
	chk(fake_hookA      == 0xAAAA, 'p');

	return s6_fail == 0;
}

/* --------------------- test 7: arena + budget plan + one-call boot -------- */
#define T7_ARENA (8u * 1024u * 1024u)
static uint8_t arena7[T7_ARENA];
static uint8_t region7[2048];
static void   *heaps7[MTX_MAX_DEPTH], *screens7[MTX_MAX_DEPTH];

static uint32_t wire7(void)
{
	mtx_dos_add_region(region7, sizeof(region7));
	mtx_dos_add_hook(&fake_hookA, 4);
	mtx_dos_set_pin(&fake_mainzone, MTX_PIN_MAINZONE);
	mtx_dos_set_pin(&fake_screen, MTX_PIN_SCREEN);
	return mtx_dos_snapshot_bytes();
}

static int test7(void)
{
	mtx_boot_t   bt;
	mtx_budget_t b, exp;
	mtx_arena_t  ar;
	uint8_t     *p1, *p2, *p3;
	uint32_t     snap;
	int          i, j, ok = 1;

	/* planner arithmetic, checked against hand figures:
	   (a) 8 MiB arena, 256 KiB zone -> per_depth 464388 -> 18 fit -> capped 12
	   (b) 1 MiB arena, 538 KiB snapshot -> per_depth 1000860 -> only 1 fits   */
	b = mtx_plan(8u * 1024u * 1024u, 2048u + 4u, 262144u, 320u * 200u * 3u, 8192u);
	ok &= (b.per_depth == 464388u);
	ok &= (b.depths_fit == 18u);
	ok &= (b.depth == (uint8_t)MTX_MAX_DEPTH);

	b = mtx_plan(1u * 1024u * 1024u, 538524u, 262144u, 192000u, 8192u);
	ok &= (b.per_depth == 1000860u);
	ok &= (b.depths_fit == 1u);
	ok &= (b.depth == 1u);

	/* arena: alignment respected, no overlap, overflow refused */
	mtx_arena_init(&ar, arena7, 4096u);
	p1 = (uint8_t *)mtx_arena_alloc(&ar, 100u, 16);
	p2 = (uint8_t *)mtx_arena_alloc(&ar, 100u, 32);
	p3 = (uint8_t *)mtx_arena_alloc(&ar, 100u, 1);
	ok &= (((uintptr_t)p1 & 15u) == 0u);
	ok &= (((uintptr_t)p2 & 31u) == 0u);
	ok &= (p1 && p2 && p3 && p1 < p2 && p2 < p3);
	ok &= ((uintptr_t)p2 - (uintptr_t)p1 >= 100u);
	ok &= (mtx_arena_alloc(&ar, 4000u, 1) == 0);
	ok &= (mtx_arena_used(&ar) <= 4096u);

	/* an arena too small to hold two depths must be refused outright */
	ok &= (mtx_dos_boot(arena7, 1u * 1024u * 1024u, MTX_ZONE_DEFAULT, wire7,
	                    heaps7, screens7, &bt) == -1);

	/* real boot: carve and wire, then check the layout */
	mtx_dos_init(MTX_MAX_DEPTH, 0, 0);
	snap = wire7();
	exp = mtx_plan(T7_ARENA, snap, MTX_ZONE_DEFAULT, MTX_SCREEN_BYTES, MTX_STACK_SIZE);

	if(mtx_dos_boot(arena7, T7_ARENA, MTX_ZONE_DEFAULT, wire7,
	                heaps7, screens7, &bt) != 0)
		return 0;

	ok &= (bt.depth == exp.depth);
	ok &= (bt.budget.per_depth == exp.per_depth);
	ok &= (bt.budget.snapshot == snap);
	for(i = 0; i < bt.depth; i++)
	{
		ok &= (heaps7[i] && screens7[i] && heaps7[i] != screens7[i]);
		for(j = 0; j < i; j++)
			ok &= (heaps7[i] != heaps7[j] && screens7[i] != screens7[j]);
	}
	ok &= ((uint8_t *)bt.stack_base >= arena7);
	ok &= ((uint8_t *)bt.stack_base + bt.stack_bytes <= arena7 + T7_ARENA);
	ok &= ((uint8_t *)bt.snapshot_base >= (uint8_t *)bt.stack_base + bt.stack_bytes);
	return ok;
}

/* ----------------------------------------- test 8: reentrant 2nd stage ---- */
static uint8_t  ldr_file[64];
static uint32_t ldr_pos;
static int      ldr_reads, ldr_allocs, ldr_entered;
static uint32_t ldr_alloc_size;
static uint8_t  ldr_mem[4096];
static int32_t  ldr_fdscratch[16];
static uint8_t  ldr_lumpinfo[8 * 20];

static void bcopy_(void *d, const void *s, uint32_t n)
{ uint8_t *a = (uint8_t *)d; const uint8_t *b = (const uint8_t *)s; while(n--) *a++ = *b++; }
static void bzero_(void *d, uint32_t n)
{ uint8_t *a = (uint8_t *)d; while(n--) *a++ = 0; }

static int32_t ldr_read(void *ctx, int32_t fd, void *dst, uint32_t n)
{
	(void)ctx; (void)fd;
	ldr_reads++;
	if(ldr_pos + n > sizeof(ldr_file))
		return -1;
	bcopy_(dst, ldr_file + ldr_pos, n);
	ldr_pos += n;
	return (int32_t)n;
}
static void *ldr_alloc(void *ctx, uint32_t n)
{ (void)ctx; ldr_allocs++; ldr_alloc_size = n; return ldr_mem; }
static void ldr_enter(void *ctx, void *code, uint32_t cb, uint32_t db, uint32_t ab)
{ (void)ctx; (void)cb; (void)db; (void)ab; ldr_entered = (code != 0); }

static void ldr_putfd(uint32_t idx, int32_t fd)
{ *(int32_t *)(void *)(ldr_lumpinfo + idx * 20u + 8u) = fd; }

static int test8(void)
{
	mtx_ldr1n_ops_t ops;
	mtx_ldr1n_src_t src;
	mtx_ldr1n_out_t out;
	uint32_t i;
	int ok = 1;

	ops.read  = ldr_read;
	ops.alloc = ldr_alloc;
	ops.enter = ldr_enter;

	/* descriptor list: consecutive dedup, leading -1 dropped */
	bzero_(ldr_lumpinfo, sizeof(ldr_lumpinfo));
	{ int32_t seq[8] = {3, 3, 3, 5, 5, 7, 7, 7};
	  for(i = 0; i < 8; i++) ldr_putfd(i, seq[i]); }
	ok &= (mtx_ldr1n_fdlist(ldr_lumpinfo, 8, ldr_fdscratch, 16) == 3);
	ok &= (ldr_fdscratch[0] == 3 && ldr_fdscratch[1] == 5
	    && ldr_fdscratch[2] == 7 && ldr_fdscratch[3] == -1);

	bzero_(ldr_lumpinfo, sizeof(ldr_lumpinfo));
	{ int32_t seq[4] = {-1, -1, 4, 4};
	  for(i = 0; i < 4; i++) ldr_putfd(i, seq[i]); }
	ok &= (mtx_ldr1n_fdlist(ldr_lumpinfo, 4, ldr_fdscratch, 16) == 1);
	ok &= (ldr_fdscratch[0] == 4 && ldr_fdscratch[1] == -1);

	/* a too-small output must be refused, not overflowed */
	ok &= (mtx_ldr1n_fdlist(ldr_lumpinfo, 4, ldr_fdscratch, 1) == 0);

	/* load: [bss_size][code...] in exactly two reads and one allocation */
	{
		uint32_t bss = 0x1000u, lump = 12u;

		*(uint32_t *)(void *)ldr_file = bss;
		for(i = 4; i < 12; i++)
			ldr_file[i] = (uint8_t)(0xA0u + i);
		ldr_pos = 0; ldr_reads = 0; ldr_allocs = 0; ldr_entered = 0;
		bzero_(ldr_mem, sizeof(ldr_mem));

		src.fd = 7; src.lump_size = lump; src.offset = 0;
		if(mtx_ldr1n_prepare(&ops, 0, &src, ldr_lumpinfo, 4,
		                     ldr_fdscratch, 16, &out) != 0)
			ok = 0;

		ok &= (ldr_reads == 2);
		ok &= (ldr_allocs == 1);
		ok &= (ldr_alloc_size == bss + lump + 32u);
		ok &= (out.code == ldr_mem);
		ok &= (out.code_size == bss + lump + 32u);
		ok &= (*(uint32_t *)out.code == bss);
		ok &= (((uint8_t *)out.code)[4]  == ldr_file[4]);
		ok &= (((uint8_t *)out.code)[11] == ldr_file[11]);
		ok &= (out.fdlist_len == 1 && out.fdlist[0] == 4 && out.fdlist[1] == -1);

		/* hand-off sets the flag */
		ops.enter(0, out.code, 0x1000u, 0x2000u, 0x3000u);
		ok &= (ldr_entered == 1);

		/* a missing service must be reported, not ignored */
		{
			mtx_ldr1n_ops_t bad = ops;
			bad.alloc = 0;
			ok &= (mtx_ldr1n_load(&bad, 0, &src, &out.code, &out.code_size) == -1);
		}
	}
	return ok;
}

/* ------------------------------ test 9: yield point / frame-boundary ---- */
static int t9_ord[64];
static int t9_n;
static int t9_frames[MTX_MAX_DEPTH];

static void t9_child(void *a)
{
	int i;
	(void)a;
	for(i = 0; i < 4; i++)
	{
		t9_frames[mtx_depth()]++;
		t9_ord[t9_n++] = (int)mtx_depth();
		if(i == 1)
			mtx_request_yield();	/* serviced at the NEXT frame boundary */
		mtx_frame_tick();
	}
	mtx_finish();
}

static int test9(void)
{
	int ok = 1;

	t9_n = 0;
	t9_frames[0] = t9_frames[1] = 0;
	mtx_request_cancel();

	mtx_init(MTX_MAX_DEPTH, 0, 0, 0);

	/* depth 1 runs two frames, then requests an upward yield, which the next
	   frame_tick services - so control is back here after the push */
	mtx_push(t9_child, 0);
	ok &= (t9_frames[1] == 2);
	ok &= (!mtx_request_pending());		/* consumed exactly once */

	/* root frame, then ask to go back down */
	t9_frames[0]++;
	t9_ord[t9_n++] = 0;
	mtx_request_switch(1);
	ok &= (mtx_request_pending());
	mtx_frame_tick();			/* -> depth 1 resumes here */

	/* depth 1 finished its remaining two frames and returned */
	ok &= (t9_frames[1] == 4);
	t9_frames[0]++;
	t9_ord[t9_n++] = 0;
	ok &= (!mtx_request_pending());

	/* interleaving must be frame-aligned: 1,1,0,1,1,0 */
	ok &= (t9_n == 6);
	ok &= (t9_ord[0] == 1 && t9_ord[1] == 1 && t9_ord[2] == 0 &&
	       t9_ord[3] == 1 && t9_ord[4] == 1 && t9_ord[5] == 0);
	ok &= (t9_frames[0] == 2 && t9_frames[1] == 4);

	/* a request naming a depth that is not live must be dropped, not crash */
	mtx_request_switch(7);
	mtx_frame_tick();
	ok &= (!mtx_request_pending());
	ok &= (mtx_depth() == 0);

	/* a snapshot taken now must show nothing pending */
	ok &= (mtx_request_kind() == MTX_REQ_NONE);
	return ok;
}

/* ------------------------------------------------------------------- main */
void _start(void)
{
	int ok = 1, r;

	mtx_init(MTX_MAX_DEPTH, 0, 0, 0);

	puts_("matryoshka host test\n");
	puts_("  depth ceiling: "); putu(mtx_max_depth()); puts_("\n");

	r = test1(); puts_("  [1] nesting order .......... "); puts_(r ? "PASS" : "FAIL"); puts_("\n"); ok &= r;
	r = test2(); puts_("  [2] switch_to + per-depth state "); puts_(r ? "PASS" : "FAIL"); puts_("\n"); ok &= r;
	r = test3(); puts_("  [3] depth ceiling ......... "); puts_(r ? "PASS" : "FAIL"); puts_("\n"); ok &= r;
	r = test4(); puts_("  [4] save/load ordering .... "); puts_(r ? "PASS" : "FAIL"); puts_("\n"); ok &= r;
	r = test5(); puts_("  [5] distinct stacks ....... "); puts_(r ? "PASS" : "FAIL"); puts_("\n"); ok &= r;
	r = test6(); puts_("  [6] state snapshot + isolation "); puts_(r ? "PASS" : "FAIL"); puts_("\n"); ok &= r;
	r = test7(); puts_("  [7] arena + budget + boot .. "); puts_(r ? "PASS" : "FAIL"); puts_("\n"); ok &= r;
	r = test8(); puts_("  [8] reentrant 2nd stage .... "); puts_(r ? "PASS" : "FAIL"); puts_("\n"); ok &= r;
	r = test9(); puts_("  [9] yield point (frame tick) "); puts_(r ? "PASS" : "FAIL"); puts_("\n"); ok &= r;

	puts_(ok ? "ALL PASS\n" : "SOME FAILED\n");
	sys_exit(ok ? 0 : 1);
}
