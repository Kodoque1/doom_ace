/*
 * matryoshka/mtx_host.c
 *
 * Definition of the host's control state. Non-static on purpose: dos_wire.c
 * needs its address and size to exclude it from the engine's snapshot region.
 * A single object is what makes that exclusion exact - the linker may place
 * other .bss data before or after mtx_hs, but never inside it.
 */
#include "mtx_host.h"

mtx_host_state_t mtx_hs;
