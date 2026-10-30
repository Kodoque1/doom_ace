/* Level 8: the third ring level. Its whole east wall is screen-wall, which in the ring
 * shows the next copy's latest frame. */
#include "../script.h"

char level_map[] =
	"8888888888888889"
	"8..............9"
	"8..............9"
	"8..............9"
	"8...22.....22..9"
	"8..............9"
	"8..............9"
	"8..............9"
	"8..............9"
	"8..............9"
	"8..............9"
	"8...22.....22..9"
	"8..............9"
	"8..............9"
	"8..............9"
	"8888888888888889";

void script_main(void)
{
	map = level_map;
	spawn(7, 7, 1);
	spawn(10, 3, 1);
}
