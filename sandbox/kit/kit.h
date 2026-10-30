/* kit.h - everything the scheduler needs that is not the interesting part.
 *
 * The scheduler is level 0's script. The game boots level 0, loads level0.o (your
 * sched.c linked with kit.c) and calls script_main() once, and after that the
 * scheduler only runs when the game calls into it through a hook. Starting copies,
 * finding the game's globals, the hook table and copying memory are all here, so
 * that sched.c is left with the actual decisions.
 */
#ifndef KIT_H
#define KIT_H
#include "../game/script.h"

#define MAX_COPIES 4

/* One copy of the game. copies[0] is the boot copy, the one playing level 0, which
   is where script_main() runs; copies[1] to copies[ncopies] are the ones you start. */
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

/* Call this first in script_main(). It gives level 0 its map, fills in copies[0]
   from the running copy, and records where every hook points before anyone patches
   it, which is what a fresh copy should start with. */
void kit_boot(void);

/* What the checker asked for, read from the boot arguments (run -A sets them by hand). */
int wanted_copies(void);         /* how many copies to start            */
int wanted_level(int i);         /* the level of copy i, from 1         */
int wanted_ring(void);           /* 1: each screen-wall shows the next
                                    copy's screen (milestone M6)        */

/* Start a copy: allocate its stack, framebuffer and heap, and lay its stack out as if
 * it had been suspended inside switch_to() (described below). Nothing runs yet. The
 * first switch_to() into the copy "returns" into copy_main(level, screen, heap),
 * which loads the level and starts playing, so a new copy and an old one are resumed
 * the same way. */
struct Copy *copy_start(int level);

/* What a suspended copy looks like, which is the contract switch_to() has to keep:
 *
 *    copy->sp  ->  [ its saved fp ]       the rest of its stack above
 *
 * Load that sp, pop that fp, and returning from switch_to() resumes the copy where it
 * left. That is the whole context because the kit's compiler keeps nothing in
 * registers across a call: sp, fp and pc are all there is, and pc is already on the
 * stack as the return address. */

/* The hook table: the call sites in the game's code that level scripts patch, and
   where each one pointed at boot. frame_end is the one the scheduler uses. */
#define NHOOKS 4
extern char *hook_sites[NHOOKS];      /* hook_think, hook_damage, hook_wall_color,
                                         hook_frame_end                           */
extern int hook_defaults[NHOOKS];
#define HOOK_FRAME_END 3              /* hook_sites[3]: yours */

/* The game's globals are every byte between these two addresses. */
extern char GAME_DATA_START[], GAME_DATA_END[];
#define GLOBALS_START GAME_DATA_START
#define GLOBALS_SIZE (GAME_DATA_END - GAME_DATA_START)   /* a multiple of 4 */

/* The globals that hold the address of one of a copy's own blocks. They are part of
   the globals above; they are listed separately because a few schedulers need to
   treat them differently (the ring does). */
#define NSHARED 5
extern char **shared_pointers[NSHARED];   /* &screen, &heap, &script, &map, &wall_tex */

/* Copying and comparing work a word at a time, so n must be a multiple of 4 and the
   blocks 4-aligned; the game's globals are both. */
void block_copy(char *dst, char *src, int n);
int block_equal(char *a, char *b, int n);   /* 1 if the same */
char *kit_alloc(int n);                      /* stops the machine when memory runs out */

void kit_print(char *s);
void kit_print_int(int n);

#endif
