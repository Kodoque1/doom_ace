/* kit.c - the plumbing behind kit.h. */
#include "kit.h"

struct Copy copies[MAX_COPIES + 1];
int ncopies;

char *hook_sites[NHOOKS] = { hook_think, hook_damage, hook_wall_color, hook_frame_end };
int hook_defaults[NHOOKS];
char **shared_pointers[NSHARED] = { &screen, &heap, &script, &map, &wall_tex };

/* level 0's own map: the room the boot copy plays in */
char kit_map[] =
	"1111111111111111"
	"1..............1"
	"1..............1"
	"1..............1"
	"1..............1"
	"1..............1"
	"1..............1"
	"1..............1"
	"1..............1"
	"1..............1"
	"1..............1"
	"1..............1"
	"1..............1"
	"1..............1"
	"1..............1"
	"1111111111111111";

int wanted_copies(void)
{
	int n = __sys(SYS_ARG, 1);
	return n < 0 ? 0 : n > MAX_COPIES ? MAX_COPIES : n;
}
int wanted_ring(void) { return __sys(SYS_ARG, 2); }
int wanted_level(int i) { return __sys(SYS_ARG, 2 + i); }

void kit_print(char *s) { while (*s) __sys(SYS_PUTC, *s++); }
void kit_print_int(int n)
{
	if (n < 0) { __sys(SYS_PUTC, '-'); n = -n; }
	if (n >= 10) kit_print_int(n / 10);
	__sys(SYS_PUTC, '0' + n % 10);
}

char *kit_alloc(int n)
{
	char *p = (char *)__sys(SYS_ALLOC, n);
	if (!p) {
		kit_print("kit: out of memory\n");
		__sys(SYS_EXIT);
	}
	return p;
}

void block_copy(char *dst, char *src, int n)
{
	asm("\tldw r1, [fp+8]");
	asm("\tldw r2, [fp+12]");
	asm("\tldw r3, [fp+16]");
	asm("\tadd r3, r2, r3");
	asm(".Lkit_copy:");
	asm("\tseq r4, r2, r3");
	asm("\tbnz r4, .Lkit_copied");
	asm("\tldw r4, [r2]");
	asm("\tstw r4, [r1]");
	asm("\taddi r1, r1, 4");
	asm("\taddi r2, r2, 4");
	asm("\tjmp .Lkit_copy");
	asm(".Lkit_copied:");
}

int block_equal(char *a, char *b, int n)
{
	asm("\tldw r1, [fp+8]");
	asm("\tldw r2, [fp+12]");
	asm("\tldw r3, [fp+16]");
	asm("\tadd r3, r2, r3");
	asm("\tldi r0, 1");
	asm(".Lkit_cmp:");
	asm("\tseq r4, r2, r3");
	asm("\tbnz r4, .Lkit_cmped");
	asm("\tldw r4, [r1]");
	asm("\tldw r5, [r2]");
	asm("\taddi r1, r1, 4");
	asm("\taddi r2, r2, 4");
	asm("\tseq r4, r4, r5");
	asm("\tbnz r4, .Lkit_cmp");
	asm("\tldi r0, 0");
	asm(".Lkit_cmped:");
}

void kit_boot(void)
{
	map = kit_map;
	/* the boot copy: what is running now */
	copies[0].level = 0;
	copies[0].screen = screen;
	copies[0].heap = heap;
	copies[0].started = 1;
	for (int h = 0; h < NHOOKS; h++) hook_defaults[h] = hook_get(hook_sites[h]);
}

/* where a new copy begins, on its own stack */
void copy_entry(struct Copy *c)
{
	c->started = 1;
	copy_main(c->level, c->screen, c->heap);
}

struct Copy *copy_start(int level)
{
	if (ncopies == MAX_COPIES) {
		kit_print("kit: too many copies\n");
		__sys(SYS_EXIT);
	}
	ncopies++;
	struct Copy *c = &copies[ncopies];
	c->id = ncopies;
	c->level = level;
	c->stack = kit_alloc(STACK_SIZE);
	c->screen = kit_alloc(FB_SIZE);
	c->heap = kit_alloc(HEAP_SIZE);
	c->started = 0;
	/* a suspended frame of switch_to(): sp -> [fp], and that fp's frame returns into
	   copy_entry(c) */
	int *top = (int *)(c->stack + STACK_SIZE);
	top[-1] = (int)c;              /* copy_entry's argument      */
	top[-2] = 0;                   /* copy_entry's return: never */
	top[-3] = (int)copy_entry;     /* switch_to's return address */
	top[-4] = 0;                   /* the caller's fp            */
	top[-5] = (int)&top[-4];       /* switch_to's fp             */
	c->sp = (int)&top[-5];
	__sys(SYS_COPY, c->id, c->stack, c->stack + STACK_SIZE);   /* for the checker */
	return c;
}
