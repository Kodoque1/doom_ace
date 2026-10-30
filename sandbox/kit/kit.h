/* kit.h - the plumbing your scheduler stands on.
 *
 * Your scheduler is level 0's script: the game boots level 0, loads level0.o (your
 * sched.c and kit.c) and calls script_main() once. From then on, it only runs when
 * the game calls it: through the hooks.
 *
 * What you get here is the grunt work. What is left in sched.c is the concept.
 */
#ifndef KIT_H
#define KIT_H
#include "../game/script.h"

#define MAX_COPIES 4

/* One copy of the game. copies[0] is the boot copy, playing level 0: it is where
   your script_main() runs. copies[1] to copies[ncopies] are the copies you start. */
struct Copy {
	int sp;           /* while the copy is suspended: its stack pointer (offset 0:
	                     switch_to's assembly relies on it)                        */
	int id;           /* its index in copies[]                                     */
	int level;
	char *stack;      /* its blocks, from the allocator */
	char *screen;
	char *heap;
	int started;      /* 0 until it first runs */
};
extern struct Copy copies[MAX_COPIES + 1];
extern int ncopies;

/* Call first in script_main(): sets up level 0 and copies[0], and records the
   hooks' defaults. */
void kit_boot(void);

/* What the checker (or you, with run -A) asked for: the boot arguments. */
int wanted_copies(void);         /* how many copies to start            */
int wanted_level(int i);         /* the level of copy i, from 1         */
int wanted_ring(void);           /* 1: each screen-wall shows the next
                                    copy's screen (milestone M6)        */

/* Start a copy: allocate its stack, framebuffer and heap, and lay out its stack
 * as if it were suspended in switch_to() (below). It does not run yet: the first
 * switch_to() into it starts it, in copy_main(level, screen, heap), which loads
 * its level and plays. Returns the new copy. */
struct Copy *copy_start(int level);

/* How a suspended copy looks, so switch_to() can resume any copy, new or old:
 *
 *    copy->sp  ->  [ its saved fp ]       the rest of its stack above
 *
 * and returning from switch_to(), with that fp, resumes it. The kit's compiler
 * keeps nothing else in registers across a call: sp, fp and pc are the whole
 * context (pc is on the stack, as the return address). */

/* The hook table: the call sites level scripts patch, and where they point at boot. */
#define NHOOKS 4
extern char *hook_sites[NHOOKS];      /* hook_think, hook_damage, hook_wall_color,
                                         hook_frame_end                           */
extern int hook_defaults[NHOOKS];
#define HOOK_FRAME_END 3              /* hook_sites[3]: yours */

/* The game's globals: every byte between these two addresses. */
extern char GAME_DATA_START[], GAME_DATA_END[];
#define GLOBALS_START GAME_DATA_START
#define GLOBALS_SIZE (GAME_DATA_END - GAME_DATA_START)   /* a multiple of 4 */

/* The shared pointers: globals that hold the address of a copy's own block. */
#define NSHARED 5
extern char **shared_pointers[NSHARED];   /* &screen, &heap, &script, &map, &wall_tex */

/* Memory. n must be a multiple of 4; blocks must be 4-aligned. */
void block_copy(char *dst, char *src, int n);
int block_equal(char *a, char *b, int n);   /* 1 if the same */
char *kit_alloc(int n);                      /* stops the machine when memory runs out */

void kit_print(char *s);
void kit_print_int(int n);

#endif
