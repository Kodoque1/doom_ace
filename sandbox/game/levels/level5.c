/* Level 5: hostile. Its script writes everywhere a level script may write, every
 * tic: every hook (it chains the frame_end hook to whatever was there, and changes
 * its wall colours there, at the end of a frame), the automap and the random seed in
 * the game's globals, and the free stack below its own stack pointer.
 * Deterministic, so its solo run is still a reference. */
#include "../script.h"

char level_map[] =
	"2222222222222222"
	"2..............2"
	"2..............2"
	"2...111..111...2"
	"2...1......1...2"
	"2...1......1...2"
	"2..............2"
	"2......44......2"
	"2......44......2"
	"2..............2"
	"2...1......1...2"
	"2...1......1...2"
	"2...111..111...2"
	"2..............2"
	"2..............2"
	"2222222222222222";

int prev_end;      /* where frame_end pointed when the level started */

int harsh(int d) { return d * 3; }
int gloomy(int c, int side) { return side ? 1 : 2; }
int loud(int c, int side) { return side ? 13 : 4; }

int churn(int i)
{
	int junk[64];
	for (int k = 0; k < 64; k++) junk[k] = 0x5a5a5a5a ^ (gametic * 64 + k);
	hook_set(hook_damage, harsh);
	hook_set(hook_think, churn);
	visited[(gametic * 7 + i) & 255] = junk[i & 63] & 1;
	rng = rng ^ junk[gametic & 63];
	return think_default(i);
}

void end_of_frame(void)
{
	void (*f)(void) = (void *)prev_end;
	/* a patch made at the end of a frame must still be there at the next one */
	hook_set(hook_wall_color, (gametic / 32) & 1 ? loud : gloomy);
	hook_set(hook_frame_end, end_of_frame);
	f();
}

void script_main(void)
{
	map = level_map;
	prev_end = hook_get(hook_frame_end);
	hook_set(hook_frame_end, end_of_frame);
	hook_set(hook_think, churn);
	spawn(3, 3, 1);
	spawn(12, 3, 1);
	spawn(3, 12, 1);
	spawn(12, 12, 1);
}
