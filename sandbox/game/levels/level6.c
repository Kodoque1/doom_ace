/* Level 6: the first ring level. The east wall is all screen-wall: in the ring it
 * shows the next copy's frame. */
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
