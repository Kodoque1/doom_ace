/* SPOILER: a reference solution to the sandbox. The checker's own tests build it.
 * Solve the puzzle first: you/sched.c is yours.
 *
 * Every switch copies all of the game's globals out and in, and every hook. */
#include "../kit/kit.h"

struct Copy *current;
char *saved_globals[MAX_COPIES + 1];
int saved_hooks[MAX_COPIES + 1][NHOOKS];

void on_frame_end(void);

void switch_to(struct Copy *from, struct Copy *to)
{
	asm("\tldw r1, [fp+8]");
	asm("\tldw r2, [fp+12]");
	asm("\tpush fp");
	asm("\tstw sp, [r1]");     /* from->sp */
	asm("\tldw sp, [r2]");     /* to->sp   */
	asm("\tpop fp");
}

void save(struct Copy *c)
{
	block_copy(saved_globals[c->id], GLOBALS_START, GLOBALS_SIZE);
	for (int h = 0; h < NHOOKS; h++) saved_hooks[c->id][h] = hook_get(hook_sites[h]);
}

struct Copy *next_of(struct Copy *c) { return &copies[c->id % ncopies + 1]; }

void load(struct Copy *c)
{
	block_copy(GLOBALS_START, saved_globals[c->id], GLOBALS_SIZE);
	for (int h = 0; h < NHOOKS; h++) hook_set(hook_sites[h], (void *)saved_hooks[c->id][h]);
	/* a pin: the ring, whatever the copy's own state says. (Not frame_end: a level
	   script may chain it, and its chain already ends in on_frame_end.) */
	if (wanted_ring()) wall_tex = next_of(c)->screen;
}

void on_frame_end(void)
{
	if (!ncopies) return;
	struct Copy *from = current, *to = next_of(current);
	save(from);
	load(to);
	current = to;
	switch_to(from, to);
}

void script_main(void)
{
	kit_boot();
	for (int i = 0; i <= wanted_copies(); i++) {
		if (i) copy_start(wanted_level(i));
		/* first entry: seed the copy's snapshot from what is live now */
		saved_globals[i] = kit_alloc(GLOBALS_SIZE);
		block_copy(saved_globals[i], GLOBALS_START, GLOBALS_SIZE);
		for (int h = 0; h < NHOOKS; h++) saved_hooks[i][h] = hook_defaults[h];
		saved_hooks[i][HOOK_FRAME_END] = (int)on_frame_end;   /* so the copy comes back */
	}
	current = &copies[0];
	hook_set(hook_frame_end, on_frame_end);
}
