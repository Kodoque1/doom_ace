/* sched.c - your scheduler, which has to let several copies of TinyDoom share one
 * machine without any of them noticing the others.
 *
 *   make check        which milestone you have reached
 *   make check M=3    one milestone
 *   make watch        the copies' frames, as text
 *
 * The four functions below are the whole exercise. Starting copies, finding the
 * game's globals and copying memory are already done in kit.h, the game's side is
 * described in game/game.h, and MACHINE.md is the instruction set. You can add any
 * globals, helpers or fields you need; the checker only cares about what the copies
 * see.
 */
#include "../kit/kit.h"

struct Copy *current;     /* the copy running now */

/* Suspend `from` and resume `to`, which comes down to the registers and the stack
 * pointer. kit.h describes the layout a suspended copy has to have, and a few lines
 * of asm() between this function's prologue and epilogue are enough to produce it;
 * the two arguments are at [fp+8] and [fp+12]. */
void switch_to(struct Copy *from, struct Copy *to)
{
	/* TODO */
}

/* Put away everything `c` has to find unchanged when it resumes. Its heap, stack and
 * screen are its own memory and nobody else touches them, so what is left is what the
 * copies share: the game's globals and the hook call sites. GLOBALS_START,
 * GLOBALS_SIZE, hook_sites[] and block_copy() in kit.h are the tools for it. */
void save(struct Copy *c)
{
	/* TODO */
}

/* Put back what save() put away, so that `c` runs as if it had never stopped. */
void load(struct Copy *c)
{
	/* TODO */
}

/* The game calls this at the end of every frame, in whichever copy happens to be
 * running, which makes it the only place a switch can happen: decide whether to
 * switch now, and to which copy. */
void on_frame_end(void)
{
	/* TODO */
}

/* Runs once at boot, as level 0's script, while copies[0] is the only thing running.
 * It starts the copies the checker asked for, which do not run until something
 * switches to them, and points the frame_end hook at on_frame_end so the scheduler
 * gets control back every frame. */
void script_main(void)
{
	kit_boot();
	for (int i = 1; i <= wanted_copies(); i++)
		copy_start(wanted_level(i));
	current = &copies[0];
	hook_set(hook_frame_end, on_frame_end);
}
