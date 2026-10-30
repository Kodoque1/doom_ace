/* sched.c - your scheduler. Make several copies of TinyDoom take turns in one
 * machine, without trampling each other.
 *
 *   make check        which milestone you have reached
 *   make check M=3    one milestone
 *   make watch        the copies' frames, as text
 *
 * The four functions below are yours. kit.h has the plumbing; game/game.h has the
 * game; MACHINE.md has the machine. Everything else is up to you: add globals,
 * helpers, fields of your own.
 */
#include "../kit/kit.h"

struct Copy *current;     /* the copy running now */

/* Suspend `from` and resume `to`: the registers and the stack pointer.
 * Hint: kit.h shows what a suspended copy looks like. A few lines of asm() do it,
 * between this function's prologue and its epilogue; the arguments are at [fp+8]
 * and [fp+12]. */
void switch_to(struct Copy *from, struct Copy *to)
{
	/* TODO */
}

/* Put away what `c` must find again when it resumes: the data and the patches.
 * Hint: GLOBALS_START, GLOBALS_SIZE, hook_sites[] and block_copy() in kit.h. */
void save(struct Copy *c)
{
	/* TODO */
}

/* Put back what save() put away, so `c` can run. */
void load(struct Copy *c)
{
	/* TODO */
}

/* The game calls this at the end of every frame, in whichever copy is running:
 * when to switch, and to whom. */
void on_frame_end(void)
{
	/* TODO */
}

/* Runs once, at boot, as level 0's script, in copies[0]. */
void script_main(void)
{
	kit_boot();
	for (int i = 1; i <= wanted_copies(); i++)
		copy_start(wanted_level(i));
	current = &copies[0];
	hook_set(hook_frame_end, on_frame_end);
}
