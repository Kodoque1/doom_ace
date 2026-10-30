/* Level 7: the second ring level. Its whole east wall is screen-wall, which in the ring
 * shows the next copy's latest frame. */
#include "../script.h"

char level_map[] =
	"6666666666666669"
	"6..............9"
	"6..............9"
	"6..............9"
	"6...44.....44..9"
	"6..............9"
	"6..............9"
	"6..............9"
	"6..............9"
	"6..............9"
	"6..............9"
	"6...44.....44..9"
	"6..............9"
	"6..............9"
	"6..............9"
	"6666666666666669";

void script_main(void)
{
	map = level_map;
	spawn(7, 7, 1);
	spawn(10, 3, 1);
}
