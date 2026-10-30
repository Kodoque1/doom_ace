/* game.h - TinyDoom's interface, which is everything a level script or the kit is
 * allowed to use.
 *
 * TinyDoom is a tiny raycaster, and one copy of it is a whole program. Its code and
 * its globals sit at fixed addresses, exactly as Doom's do, while the rest of a
 * copy's memory (framebuffer, heap, level script) comes from the operating system's
 * allocator. The globals below hold those blocks' addresses, which makes them the
 * pointers every copy shares the location of but needs its own value in.
 */
#ifndef GAME_H
#define GAME_H

#define SYS_EXIT    0
#define SYS_LOAD    1
#define SYS_ALLOC   2
#define SYS_KEY     3
#define SYS_PRESENT 4
#define SYS_TICKS   5
#define SYS_PUTC    6
#define SYS_COPY    7
#define SYS_ARG     8

#define FB_W        64
#define FB_H        40
#define FB_SIZE     2560        /* FB_W * FB_H, one byte (a colour 0-15) per pixel */
#define MAP_W       16          /* the map is MAP_W x MAP_W cells */
#define CELL        256         /* positions are in 1/256 of a cell */
#define HEAP_SIZE   512
#define MAX_THINGS  8
#define STACK_SIZE  768         /* what one copy's stack needs */

struct Thing { int x, y, hp, kind; };
#define THINGS ((struct Thing *)heap)   /* the things live at the start of the heap */

/* --- the shared pointers: each copy's own blocks --- */
extern char *screen;      /* FB_SIZE bytes: what this copy draws, then presents  */
extern char *heap;        /* HEAP_SIZE bytes: the things                         */
extern char *script;      /* the level script's image (it also holds the map)    */
extern char *map;         /* MAP_W * MAP_W cells: '.' is empty, '1'-'9' walls    */
extern char *wall_tex;    /* what screen-walls (cell '9') show: a framebuffer, or
                             0 for static                                        */
extern int script_size;

/* --- the level's state --- */
extern int level, gametic, health, deaths, px, py, pa, start_x, start_y, start_a;
extern int nthings, turning, turn_dir, rng;   /* the player walks by itself */
extern char visited[MAP_W * MAP_W];   /* the automap: cells the player has seen */

/* --- the game's functions --- */
void copy_main(int level, char *fb, char *heap);   /* set up a copy and play */
void level_start(int n);    /* reset the level state, load and run levelN.o  */
void game_loop(void);       /* never returns: input, update, draw, present   */
void spawn(int x, int y, int kind);
int cell(int cx, int cy);
int isin(int a);            /* angles: 256 to a turn; result: -255..255 */
int icos(int a);

/* --- the hook table ---
 * Each hook is a call instruction in the game's code at a labelled address. As built,
 * it calls the default function below; a level script changes the instruction to call
 * one of its own instead, which is the same thing ACE does to Doom, done on purpose. */
extern char hook_think[];       /* int think(int i): move thing i, once a tic       */
extern char hook_damage[];      /* int damage(int d): how much a hit really costs   */
extern char hook_wall_color[];  /* int wall_color(int cell, int side): a colour     */
extern char hook_frame_end[];   /* void frame_end(void): after present, every frame */
int think_default(int i);
int damage_default(int d);
int wall_color_default(int c, int side);
void frame_end_default(void);
void hook_set(char *site, void *fn);   /* point a hook at fn */
int hook_get(char *site);              /* where a hook points */

#endif
