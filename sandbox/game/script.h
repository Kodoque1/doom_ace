/* script.h - include first in every level script. A script's entry is the first
 * byte of its image: this jump, to script_main(). */
#ifndef SCRIPT_H
#define SCRIPT_H
#include "game.h"
asm("\tjmp script_main");
void script_main(void);
#endif
