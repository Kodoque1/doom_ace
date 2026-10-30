/* tinydoom.c - TinyDoom: a tiny raycaster, one copy of which is the whole program.
 *
 * Boot: main() takes the level number from boot argument 0, allocates a
 * framebuffer and a heap, then plays. Every frame: update, draw, present, then the
 * frame_end hook. Level scripts (levels/levelN.c, loaded at level start) set the
 * map, the input and the things, and may patch the hooks.
 */
#include "game.h"

asm(".entry _start");
asm("_start:");
asm("\tcall main");
asm("\tsys 0");

/* shared pointers */
char *screen;
char *heap;
char *script;
char *map;
char *wall_tex;
int script_size;

/* the level's state */
int level, gametic, health, deaths, px, py, pa, start_x, start_y, start_a;
int nthings, turning, turn_dir, rng;
char visited[MAP_W * MAP_W];

/* --- maths --- */
int isin(int a)
{
	char *t = "\x00\x06\x0d\x13\x19\x1f\x25\x2c\x32\x38\x3e\x44\x4a\x50\x56\x5c\x62\x67\x6d\x73\x78\x7e\x83\x88\x8e\x93\x98\x9d\xa2\xa7\xab\xb0\xb4\xb9\xbd\xc1\xc5\xc9\xcd\xd0\xd4\xd7\xdb\xde\xe1\xe4\xe7\xe9\xec\xee\xf0\xf2\xf4\xf6\xf7\xf9\xfa\xfb\xfc\xfd\xfe\xfe\xff\xff\xff";
	a = a & 255;
	int i = a & 63;
	if (a < 64) return t[i];
	if (a < 128) return t[64 - i];
	if (a < 192) return -t[i];
	return -t[64 - i];
}

int icos(int a) { return isin(a + 64); }

int cell(int cx, int cy)
{
	if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_W) return 1;
	int c = map[cy * MAP_W + cx];
	if (c == '.') return 0;
	return c - '0';
}

/* --- hooks --- */
void hook_set(char *site, void *fn) { *(int *)site = 26 + ((int)fn << 16); }   /* 26: call */
int hook_get(char *site) { return (*(int *)site >> 16) & 0xffff; }

void frame_end_default(void) { }
int damage_default(int d) { return d; }

int wall_color_default(int c, int side)
{
	char *lit = "\x00\x03\x09\x04\x06\x05\x0f\x08\x0e\x0e";
	char *dark = "\x00\x02\x0e\x0d\x0c\x0b\x07\x03\x02\x0a";
	return side ? dark[c] : lit[c];
}

/* move towards (x, y) by step, sliding along walls */
void walk(int *x, int *y, int dx, int dy)
{
	if (!cell((*x + dx) >> 8, *y >> 8)) *x = *x + dx;
	if (!cell(*x >> 8, (*y + dy) >> 8)) *y = *y + dy;
}

int iabs(int v) { return v < 0 ? -v : v; }

int think_default(int i)
{
	struct Thing *t = &THINGS[i];
	int dx = px - t->x, dy = py - t->y;
	if (iabs(dx) + iabs(dy) < 5 * CELL) {
		/* chase */
		int sx = dx > 0 ? 5 : dx < 0 ? -5 : 0, sy = dy > 0 ? 5 : dy < 0 ? -5 : 0;
		walk(&t->x, &t->y, sx, sy);
	} else {
		/* wander */
		int d = (gametic / 32 + i * 3) & 3;
		walk(&t->x, &t->y, d == 0 ? 6 : d == 2 ? -6 : 0, d == 1 ? 6 : d == 3 ? -6 : 0);
	}
	if (iabs(dx) < 160 && iabs(dy) < 160 && (gametic & 7) == 0)
		return 1;     /* a hit */
	return 0;
}

void spawn(int x, int y, int kind)
{
	if (nthings == MAX_THINGS) return;
	struct Thing *t = &THINGS[nthings];
	t->x = x * CELL + 128;
	t->y = y * CELL + 128;
	t->hp = 10;
	t->kind = kind;
	nthings++;
}

/* --- the level --- */
void level_start(int n)
{
	char name[12];
	char *s = "level";
	int i = 0;
	while (*s) name[i++] = *s++;
	if (n >= 10) name[i++] = '0' + n / 10;
	name[i++] = '0' + n % 10;
	name[i++] = '.';
	name[i++] = 'o';
	name[i] = 0;

	level = n;
	gametic = 0;
	health = 100;
	deaths = 0;
	nthings = 0;
	turning = 0;
	turn_dir = 1;
	rng = n;
	map = 0;
	for (i = 0; i < MAP_W * MAP_W; i++) visited[i] = 0;
	for (i = 0; i < HEAP_SIZE; i++) heap[i] = 0;
	start_x = 2 * CELL + 128;
	start_y = 2 * CELL + 128;
	start_a = 0;

	/* the level script goes where the allocator is now, then the allocator moves past it */
	script = (char *)__sys(SYS_ALLOC, 0);
	script_size = __sys(SYS_LOAD, name, script);
	if (script_size < 0) {
		s = "no level file\n";
		while (*s) __sys(SYS_PUTC, *s++);
		__sys(SYS_EXIT);
	}
	__sys(SYS_ALLOC, script_size);
	void (*entry)(void) = (void *)script;    /* a script's entry is its first byte */
	entry();
	if (!map) {
		s = "level has no map\n";
		while (*s) __sys(SYS_PUTC, *s++);
		__sys(SYS_EXIT);
	}
	px = start_x;
	py = start_y;
	pa = start_a;
}

int random(void)
{
	rng = rng * 1103515245 + 12345;
	return (rng >> 16) & 32767;
}

/* the player walks by itself: straight on until a wall is near, then turn */
void update(void)
{
	if (turning) {
		pa = (pa + 4 * turn_dir) & 255;
		turning--;
	} else if (cell((px + (icos(pa) * 3 >> 2)) >> 8, (py + (isin(pa) * 3 >> 2)) >> 8)) {
		turning = 6 + (random() & 15);
		turn_dir = random() & 1 ? 1 : -1;
	} else {
		walk(&px, &py, icos(pa) * 16 >> 8, isin(pa) * 16 >> 8);
	}
	visited[(py >> 8) * MAP_W + (px >> 8)] = 1;
	for (int i = 0; i < nthings; i++) {
		if (__hook(think, i))
			health = health - __hook(damage, 1);
	}
	if (health <= 0) {
		deaths++;
		health = 100;
		px = start_x;
		py = start_y;
		pa = start_a;
	}
}

/* cast one ray through the grid, a cell at a time; returns the perpendicular
   distance and sets the cell hit, the side, and where along the wall (0-255) */
int cast(int a, int *hit, int *side, int *u)
{
	int dx = icos(a), dy = isin(a);
	int mx = px >> 8, my = py >> 8, c = 0, t = 0;
	char *m = map;
	int stepx = dx < 0 ? -1 : 1, stepy = dy < 0 ? -1 : 1;
	int adx = iabs(dx), ady = iabs(dy);
	/* t is the distance along the ray, in 1/256 of a cell */
	int deltax = adx ? 65536 / adx : 1000000, deltay = ady ? 65536 / ady : 1000000;
	int sidex = adx ? (dx < 0 ? (px & 255) : 256 - (px & 255)) * 256 / adx : 1000000;
	int sidey = ady ? (dy < 0 ? (py & 255) : 256 - (py & 255)) * 256 / ady : 1000000;
	for (int i = 0; i < 40; i++) {
		if (sidex < sidey) { t = sidex; sidex = sidex + deltax; mx = mx + stepx; *side = 0; }
		else { t = sidey; sidey = sidey + deltay; my = my + stepy; *side = 1; }
		if ((mx | my) & ~15) { c = 1; break; }
		c = m[my * MAP_W + mx];
		if (c != '.') { c = c - '0'; break; }
		c = 0;
	}
	*hit = c;
	if (*side == 0) *u = (py + dy * t / 256) & 255;
	else *u = (px + dx * t / 256) & 255;
	int d = t * icos(a - pa) / 256;
	return d < 16 ? 16 : d;
}

void draw(void)
{
	int x, y, zbuf[FB_W];
	for (x = 0; x < FB_W; x++) {
		int c, side, u;
		int d = cast(pa + ((x - 32) * 40) / 64, &c, &side, &u);
		zbuf[x] = d;
		int h = 15360 / d;
		int top = 20 - h / 2, bot = 20 + h / 2;
		int col = c == 9 ? 0 : __hook(wall_color, c, side);
		int y0 = top < 0 ? 0 : top, y1 = bot > FB_H ? FB_H : bot;
		char *p = screen + x;
		for (y = 0; y < y0; y++) { *p = 10; p += FB_W; }
		if (c != 9) {
			for (; y < y1; y++) { *p = col; p += FB_W; }
		} else if (wall_tex) {
			/* a screen-wall: another framebuffer, stretched over the wall */
			char *t = wall_tex + (u >> 2);
			for (; y < y1; y++) { *p = t[((y - top) * FB_H / h) * FB_W]; p += FB_W; }
		} else {
			for (; y < y1; y++) { *p = ((x * 7 + y * 13 + gametic) & 4) ? 14 : 10; p += FB_W; }   /* no signal */
		}
		for (; y < FB_H; y++) { *p = 1; p += FB_W; }
	}
	/* the things: red squares, hidden behind nearer walls */
	int ca = icos(pa), sa = isin(pa);
	for (int i = 0; i < nthings; i++) {
		int dx = THINGS[i].x - px, dy = THINGS[i].y - py;
		int fwd = (dx * ca + dy * sa) >> 8, side = (dy * ca - dx * sa) >> 8;
		if (fwd < 48) continue;
		int sx = 32 + side * 60 / fwd, s = 7680 / fwd;
		int y0 = 20 - s / 4, y1 = 20 + s / 2;
		if (y0 < 0) y0 = 0;
		if (y1 > FB_H - 1) y1 = FB_H - 1;
		for (x = sx - s / 4; x <= sx + s / 4; x++) {
			if (x < 0 || x >= FB_W || zbuf[x] < fwd) continue;
			for (y = y0; y < y1; y++) screen[y * FB_W + x] = y < 20 ? 4 : 13;
		}
	}
	/* the automap, top left: visited cells, walls next to them, things, the player */
	for (y = 0; y < MAP_W; y++)
		for (x = 0; x < MAP_W; x++)
			if (visited[y * MAP_W + x]) screen[y * FB_W + x] = 0;
	for (int i = 0; i < nthings; i++) {
		int tx = THINGS[i].x >> 8, ty = THINGS[i].y >> 8;
		if (visited[ty * MAP_W + tx]) screen[ty * FB_W + tx] = 4;
	}
	screen[(py >> 8) * FB_W + (px >> 8)] = 7;
	/* health along the bottom row */
	for (x = 0; x < FB_W; x++)
		screen[(FB_H - 1) * FB_W + x] = x * 100 < health * FB_W ? 5 : 13;
}

void game_loop(void)
{
	for (;;) {
		update();
		draw();
		__sys(SYS_PRESENT, screen);
		gametic++;
		__hook(frame_end);
	}
}

void copy_main(int n, char *fb, char *hp)
{
	screen = fb;
	heap = hp;
	level_start(n);
	game_loop();
}

int main(void)
{
	copy_main(__sys(SYS_ARG, 0), (char *)__sys(SYS_ALLOC, FB_SIZE), (char *)__sys(SYS_ALLOC, HEAP_SIZE));
	return 0;
}
