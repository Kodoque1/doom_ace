/* Level 6: the first ring level. Its whole east wall is screen-wall, which in the ring
 * shows the next copy's latest frame. */
#include "../script.h"

char level_map[] =
	"3333333333333339"
	"3..............9"
	"3..............9"
	"3..............9"
	"3...55.....55..9"
	"3..............9"
	"3..............9"
	"3..............9"
	"3..............9"
	"3..............9"
	"3..............9"
	"3...55.....55..9"
	"3..............9"
	"3..............9"
	"3..............9"
	"3333333333333339";

void script_main(void)
{
	map = level_map;
	spawn(7, 7, 1);
	spawn(10, 3, 1);
}
