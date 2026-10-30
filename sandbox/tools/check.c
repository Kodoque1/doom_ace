/* check.c - the milestones.
 *
 *   check [-d disk] [-s game.sym] [-m milestone] [-f frames]
 *
 * Every level is first run alone ("solo"), and its state is recorded after every
 * frame: the game's globals, the hooks, its heap and its framebuffer. Then your
 * scheduler (level0.o) runs copies of those levels together, and each copy's state
 * at each of its frames must equal its solo run's at the same frame.
 *
 * Addresses differ between a solo run and a copy, so pointers are compared by what
 * they point into: the copy's own framebuffer, heap or level script.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"

#define HEAP_SIZE 512      /* game/game.h */
#define FB_SIZE   (FB_W * FB_H)
#define MAXF      400
#define MAXLEVEL  16

static const char *disk = "build/disk", *symfile = "build/game.sym";
static int nframes = 100;

/* --- the game's symbols --- */
static struct { char name[64]; int addr; } syms[4096];
static int nsyms;
static int a_screen, a_heap, a_script, a_map, a_wall, a_script_size, a_ds, a_de;
static int a_hook[4];            /* think, damage, wall_color, frame_end */
static const char *hook_name[4] = { "hook_think", "hook_damage", "hook_wall_color", "hook_frame_end" };

static int sym(const char *n)
{
	for (int i = 0; i < nsyms; i++)
		if (!strcmp(syms[i].name, n)) return syms[i].addr;
	fprintf(stderr, "check: %s is not in %s\n", n, symfile);
	exit(2);
}

/* the name of the global at address a */
static const char *global_at(int a, int *off)
{
	int best = -1;
	for (int i = 0; i < nsyms; i++)
		if (syms[i].addr <= a && syms[i].addr >= a_ds && syms[i].name[0] != '_' && syms[i].name[0] != '.' &&
		    (best < 0 || syms[i].addr > syms[best].addr))
			best = i;
	if (best < 0) { *off = a - a_ds; return "globals"; }
	*off = a - syms[best].addr;
	return syms[best].name;
}

static void load_syms(void)
{
	FILE *f = fopen(symfile, "r");
	if (!f) { perror(symfile); exit(2); }
	unsigned a;
	while (nsyms < 4096 && fscanf(f, "%63s %x", syms[nsyms].name, &a) == 2)
		syms[nsyms++].addr = (int)(a + VM_GAME_BASE);
	fclose(f);
	a_screen = sym("screen"); a_heap = sym("heap"); a_script = sym("script"); a_map = sym("map");
	a_wall = sym("wall_tex"); a_script_size = sym("script_size");
	a_ds = sym("__data_start"); a_de = sym("__data_end");
	for (int h = 0; h < 4; h++) a_hook[h] = sym(hook_name[h]);
}

/* --- one frame's state, as compared --- */
typedef struct {
	uint8_t g[1024];           /* the globals, pointers normalised */
	int hooks[3];              /* where think, damage, wall_color point, normalised */
	uint8_t heap[HEAP_SIZE];
	uint8_t fb[FB_SIZE];
	int wall;                  /* raw wall_tex */
} State;

static int gsize;

static uint32_t rd(Vm *vm, int a) { return vm_rd32(vm, (uint16_t)a); }

static uint32_t norm(Vm *vm, uint32_t p)
{
	uint32_t s = rd(vm, a_script), n = rd(vm, a_script_size);
	if (p == 0) return 0;
	if (p >= s && p < s + n) return 0x10000000u + (p - s);
	if (p == rd(vm, a_screen)) return 0x20000000u;
	if (p == rd(vm, a_heap)) return 0x30000000u;
	return p;
}

static void capture(Vm *vm, State *st, int ring)
{
	for (int i = 0; i < gsize; i++) st->g[i] = vm->mem[a_ds + i];
	int ptrs[5] = { a_screen, a_heap, a_script, a_map, a_wall };
	for (int k = 0; k < 5; k++) {
		uint32_t v = (ring && ptrs[k] == a_wall) ? 0 : norm(vm, rd(vm, ptrs[k]));
		memcpy(st->g + (ptrs[k] - a_ds), &v, 4);
	}
	for (int h = 0; h < 3; h++) st->hooks[h] = (int)norm(vm, rd(vm, a_hook[h]) >> 16);
	uint16_t hp = (uint16_t)rd(vm, a_heap), fb = (uint16_t)rd(vm, a_screen);
	for (int i = 0; i < HEAP_SIZE; i++) st->heap[i] = vm->mem[(uint16_t)(hp + i)];
	for (int i = 0; i < FB_SIZE; i++) st->fb[i] = vm->mem[(uint16_t)(fb + i)];
	st->wall = (int)rd(vm, a_wall);
}

/* --- running the machine --- */
typedef struct {
	int ncopies, ring, levels[8];          /* what to boot; ncopies 0: a solo run of levels[0] */
	State *solo[MAXLEVEL];
	/* results */
	int frames[8];                         /* frames each copy drew      */
	char fail[4][256];                     /* globals, hooks, heap+fb, ring: the first failure */
	char ended[128];                       /* why the run stopped early  */
	uint64_t sched_cycles, sched_bytes, cycles;
	int switches, sched_lo, sched_hi, sched_alloc;
} Run;

static Run *cur;
static State *solo_out;
static int last_heap;
static int nstk, stk_lo[8], stk_hi[8], stk_id[8], last_copy = -1, last_fb[8];
static State tmp;

static int copy_of(Vm *vm)
{
	int sp = (int)(vm->r[D32_SP] & 0xffff);
	for (int i = 0; i < nstk; i++)
		if (sp >= stk_lo[i] && sp <= stk_hi[i]) return stk_id[i];
	return 0;
}

static void first(char *slot, int c, int lvl, int f, const char *what)
{
	if (!slot[0]) snprintf(slot, 256, "copy %d (level %d), frame %d: %s", c, lvl, f + 1, what);
}

static void compare(Vm *vm, int c, int f)
{
	int lvl = cur->levels[c - 1];
	State *ref = &cur->solo[lvl][f];
	char what[200];
	capture(vm, &tmp, cur->ring);
	for (int i = 0; i < gsize; i++)
		if (tmp.g[i] != ref->g[i]) {
			int off;
			const char *n = global_at(a_ds + i, &off);
			if (off) snprintf(what, sizeof what, "the game's globals differ from its solo run, at %s+%d", n, off);
			else snprintf(what, sizeof what, "the game's globals differ from its solo run, at %s", n);
			first(cur->fail[0], c, lvl, f, what);
			break;
		}
	for (int h = 0; h < 3; h++)
		if (tmp.hooks[h] != ref->hooks[h]) {
			snprintf(what, sizeof what, "%s points somewhere else than in its solo run", hook_name[h]);
			first(cur->fail[1], c, lvl, f, what);
			break;
		}
	if (memcmp(tmp.heap, ref->heap, HEAP_SIZE))
		first(cur->fail[2], c, lvl, f, "its heap differs from its solo run");
	else if (!cur->ring && memcmp(tmp.fb, ref->fb, FB_SIZE))
		first(cur->fail[2], c, lvl, f, "its framebuffer differs from its solo run");
	if (cur->ring) {
		int next = c % cur->ncopies + 1;
		if (last_fb[next] && tmp.wall != last_fb[next]) {
			snprintf(what, sizeof what, "its screen-wall shows %s, not copy %d's screen",
			         tmp.wall ? "something else" : "nothing", next);
			first(cur->fail[3], c, lvl, f, what);
		}
	}
}

static void on_sys(Vm *vm, int n, void *user)
{
	(void)user;
	if (n == SYS_LOAD && cur && cur->ncopies && !cur->sched_hi) {
		/* the first file loaded is level 0: the scheduler */
		cur->sched_lo = (int)(vm->r[1] & 0xffff);
		cur->sched_hi = cur->sched_lo + (int)vm->r[0];
	}
	if (n == SYS_ALLOC) {
		/* on_sys runs after the call: what the allocator moved by is this block */
		if (vm->pc - 4 >= cur->sched_lo && vm->pc - 4 < cur->sched_hi) cur->sched_alloc += vm->heap - last_heap;
		last_heap = vm->heap;
	}
	if (n == SYS_COPY && nstk < 8) {
		stk_id[nstk] = (int)vm->r[0];
		stk_lo[nstk] = (int)(vm->r[1] & 0xffff);
		stk_hi[nstk] = (int)(vm->r[2] & 0xffff);
		nstk++;
	}
	if (n != SYS_PRESENT) return;
	if (!cur->ncopies) {
		/* a solo run */
		if (cur->frames[0] < nframes) capture(vm, &solo_out[cur->frames[0]], 0);
		if (++cur->frames[0] >= nframes) vm->halted = 1;
		return;
	}
	int c = copy_of(vm);
	if (c != last_copy && last_copy >= 0) cur->switches++;
	last_copy = c;
	if (c < 1 || c > cur->ncopies) return;     /* the boot copy */
	last_fb[c] = (int)(vm->r[0] & 0xffff);
	if (cur->frames[c - 1] < nframes) compare(vm, c, cur->frames[c - 1]);
	cur->frames[c - 1]++;
	int done = 1;
	for (int i = 0; i < cur->ncopies; i++) if (cur->frames[i] < nframes) done = 0;
	if (done) vm->halted = 1;
}

static void on_write(Vm *vm, uint16_t a, int bytes, void *user)
{
	(void)a;
	(void)user;
	if (vm->pc >= cur->sched_lo && vm->pc < cur->sched_hi) cur->sched_bytes += (uint64_t)bytes;
}

static void run(Run *r)
{
	static Vm vm;
	cur = r;
	nstk = 0;
	last_copy = -1;
	last_heap = VM_HEAP_START;
	memset(last_fb, 0, sizeof last_fb);
	vm_init(&vm, disk);
	vm.on_sys = on_sys;
	vm.on_write = r->ncopies ? on_write : NULL;
	if (r->ncopies) {
		vm.args[0] = 0;
		vm.args[1] = r->ncopies;
		vm.args[2] = r->ring;
		for (int i = 0; i < r->ncopies; i++) vm.args[3 + i] = r->levels[i];
	} else {
		vm.args[0] = r->levels[0];
	}
	if (vm_boot(&vm, "game.o") < 0) { fprintf(stderr, "check: cannot load %s/game.o\n", disk); exit(2); }
	uint64_t limit = (uint64_t)nframes * (uint64_t)(r->ncopies ? r->ncopies : 1) * 400000u + 20000000u;
	while (!vm.halted && vm.cycles < limit) {
		uint16_t pc = vm.pc;
		uint64_t c0 = vm.cycles;
		vm_step(&vm);
		if (pc >= r->sched_lo && pc < r->sched_hi) r->sched_cycles += vm.cycles - c0;
	}
	r->cycles = vm.cycles;
	if (vm.halted == 2) snprintf(r->ended, sizeof r->ended, "the machine stopped: %s", vm.fault);
	else if (vm.cycles >= limit) {
		int never = 0;
		for (int i = r->ncopies - 1; i >= 0; i--) if (!r->frames[i]) never = i + 1;
		if (never) snprintf(r->ended, sizeof r->ended, "copy %d (level %d) never ran: on_frame_end has to switch to it", never, r->levels[never - 1]);
		else snprintf(r->ended, sizeof r->ended, "the copies stopped advancing (does every copy's frame_end still lead to on_frame_end?)");
	}
	else {
		int done = 1;
		for (int i = 0; i < (r->ncopies ? r->ncopies : 1); i++) if (r->frames[i] < nframes) done = 0;
		if (!done) snprintf(r->ended, sizeof r->ended, "the machine stopped (SYS_EXIT or halt)");
	}
	cur = NULL;
}

static State *solo_of(int level)
{
	static State *cache[MAXLEVEL];
	if (cache[level]) return cache[level];
	cache[level] = calloc((size_t)nframes, sizeof(State));
	Run r;
	memset(&r, 0, sizeof r);
	r.levels[0] = level;
	solo_out = cache[level];
	run(&r);
	if (r.frames[0] < nframes) { fprintf(stderr, "check: level %d alone did not reach %d frames: %s\n", level, nframes, r.ended); exit(2); }
	return cache[level];
}

static Run *scenario(int n, int ring, int l1, int l2, int l3)
{
	Run *r = calloc(1, sizeof *r);
	int ls[3] = { l1, l2, l3 };
	r->ncopies = n;
	r->ring = ring;
	for (int i = 0; i < n; i++) { r->levels[i] = ls[i]; r->solo[ls[i]] = solo_of(ls[i]); }
	run(r);
	return r;
}

/* --- the milestones --- */
static int advanced(Run *r, char *why)
{
	for (int i = 0; i < r->ncopies; i++)
		if (r->frames[i] < nframes) {
			if (strstr(r->ended, "never ran")) { snprintf(why, 400, "%s", r->ended); return 0; }
			snprintf(why, 400, "copy %d (level %d) drew %d of %d frames%s%s", i + 1, r->levels[i], r->frames[i], nframes,
			         r->ended[0] ? ": " : "", r->ended);
			return 0;
		}
	return 1;
}

static int clean(Run *r, int which, char *why)
{
	if (!advanced(r, why)) return 0;
	for (int k = 0; k < 4; k++)
		if ((which >> k & 1) && r->fail[k][0]) { snprintf(why, 400, "%s", r->fail[k]); return 0; }
	return 1;
}

int main(int argc, char **argv)
{
	int only = 0;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d") && i + 1 < argc) disk = argv[++i];
		else if (!strcmp(argv[i], "-s") && i + 1 < argc) symfile = argv[++i];
		else if (!strcmp(argv[i], "-m") && i + 1 < argc) only = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-f") && i + 1 < argc) nframes = atoi(argv[++i]);
		else { fprintf(stderr, "usage: check [-d disk] [-s game.sym] [-m milestone] [-f frames]\n"); return 2; }
	}
	if (nframes < 1 || nframes > MAXF) nframes = 100;
	load_syms();
	gsize = a_de - a_ds;
	if (gsize > 1024) { fprintf(stderr, "check: the globals are too big\n"); return 2; }

	static const char *title[7] = { "", "Take turns", "Separate data", "Separate patches", "Separate pointers",
	                                 "Survive", "The ring" };
	int pass[7] = { 0 };
	char why[7][400];
	memset(why, 0, sizeof why);
	Run *a = NULL, *b = NULL, *c = NULL, *d = NULL;
	int need_a = !only || only == 1 || only == 2 || only == 4, need_b = !only || only == 3 || only == 4;
	if (need_a) a = scenario(2, 0, 1, 2, 0);
	if (need_b) b = scenario(2, 0, 3, 4, 0);
	if (!only || only == 5) c = scenario(2, 0, 1, 5, 0);
	if (!only || only == 6) d = scenario(3, 1, 6, 7, 8);

	if (a) pass[1] = advanced(a, why[1]);
	if (a) pass[2] = clean(a, 1, why[2]);
	if (b) pass[3] = clean(b, 3, why[3]);
	if (a && b) pass[4] = clean(a, 4, why[4]) && clean(b, 4, why[4]);
	if (c) pass[5] = clean(c, 7, why[5]);
	if (d) pass[6] = clean(d, 15, why[6]);     /* framebuffers are not compared in the ring */

	printf("Doomception sandbox: %d frames per copy\n\n", nframes);
	int reached = 0, gap = 0;
	for (int m = 1; m <= 6; m++) {
		if (only && m != only) continue;
		printf("  M%d  %-18s %s", m, title[m], pass[m] ? "pass" : "FAIL");
		if (!pass[m]) {
			int same = 0;
			for (int k = 1; k < m; k++) if (!pass[k] && !strcmp(why[k], why[m]) && (!only || k == only)) same = 1;
			printf("   %s", same ? "(the same)" : why[m]);
		}
		printf("\n");
		if (pass[m] && !gap) reached = m; else gap = 1;
	}
	if (!only) printf("\nReached: %s%d\n", reached ? "M" : "", reached);
	Run *s = a ? a : b ? b : c ? c : d;
	if (s && pass[only ? only : 1] && s->switches) {
		printf("\nScores (levels %d and %d, %d frames each):\n", s->levels[0], s->levels[1], nframes);
		printf("  cycles in the scheduler per switch  %llu\n", (unsigned long long)(s->sched_cycles / (uint64_t)s->switches));
		printf("  bytes it wrote per switch           %llu\n", (unsigned long long)(s->sched_bytes / (uint64_t)s->switches));
		printf("  memory it uses, beyond the copies   %d bytes (level0.o: %d)\n",
		       s->sched_hi - s->sched_lo + s->sched_alloc - s->ncopies * (768 + FB_SIZE + HEAP_SIZE), s->sched_hi - s->sched_lo);
	}
	return only ? !pass[only] : reached < 6;
}
