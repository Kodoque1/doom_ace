/* Level 3: patches two hooks: its own wall colours, and double damage. */
#include "../script.h"

char level_map[] =
	"4444444444444444"
	"4..............4"
	"4..1111..1111..4"
	"4..1........1..4"
	"4..1........1..4"
	"4.....3333.....4"
	"4.....3..3.....4"
	"4.....3..3.....9"
	"4.....3..3.....9"
	"4.....3333.....9"
	"4..1........1..4"
	"4..1........1..4"
	"4..1111..1111..4"
	"4..............4"
	"4..............4"
	"4444444444444444";

int night_colors(int c, int side)
{
	char *lit = "\x00\x0c\x0e\x0d\x0c\x0b\x02\x0a\x0c\x0a";
	return side ? 0 : lit[c];
}

int double_damage(int d) { return damage_default(d) * 2; }

void script_main(void)
{
	map = level_map;
	hook_set(hook_wall_color, night_colors);
	hook_set(hook_damage, double_damage);
	spawn(8, 3, 1);
	spawn(8, 12, 1);
	spawn(13, 8, 1);
}
