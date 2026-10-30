/* Level 4: patches two other ways: the imps run away, and the player cannot be hurt. */
#include "../script.h"

char level_map[] =
	"5555555555555555"
	"5..............5"
	"5.66....66.....5"
	"5.66....66.....5"
	"5..............5"
	"5.....7777.....5"
	"5..............9"
	"5..8........8..9"
	"5..8........8..9"
	"5..............5"
	"5.....7777.....5"
	"5..............5"
	"5.66....66.....5"
	"5.66....66.....5"
	"5..............5"
	"5555555555555555";

int flee(int i)
{
	struct Thing *t = &THINGS[i];
	int dx = t->x - px, dy = t->y - py;
	if (dx > 0) dx = 4; else dx = -4;
	if (dy > 0) dy = 4; else dy = -4;
	if (!cell((t->x + dx) >> 8, t->y >> 8)) t->x = t->x + dx;
	if (!cell(t->x >> 8, (t->y + dy) >> 8)) t->y = t->y + dy;
	return 0;
}

int no_damage(int d) { return 0; }

void script_main(void)
{
	map = level_map;
	hook_set(hook_think, flee);
	hook_set(hook_damage, no_damage);
	spawn(4, 4, 1);
	spawn(11, 4, 1);
	spawn(4, 11, 1);
	spawn(11, 11, 1);
}
