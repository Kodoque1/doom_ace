/* script.h - include this first in every level script. The game calls a script at
 * the first byte of its image, so the first thing in the image has to be this jump to
 * script_main(). */
#ifndef SCRIPT_H
#define SCRIPT_H
#include "game.h"
asm("\tjmp script_main");
void script_main(void);
#endif
