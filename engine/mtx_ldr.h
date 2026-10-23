// kgsws' ACE Engine / matryoshka
////
// Engine-side entry points for the reentrant loaders (see mtx_ldr.c).
#ifndef MATRYOSHKA_MTX_LDR_H
#define MATRYOSHKA_MTX_LDR_H

#include "mtx_sched.h"		// MTX_FRAME_TICK_ABI

// The version dword the matryoshka guest image carries ('MTXG').
#define MTX_LDR_GUEST_VERSION 0x4D545847u

/*
 * Load the code lump `name` into depth `depth`'s own image slot.
 * `expect_ver` non-zero also checks the image's version dword, which is what
 * tells a real engine image apart from an unrelated lump.
 * On success fills the entry address (payload + 8) and the total allocated size.
 * Returns 0, or a negative step code; nothing outside the depth is touched.
 */
int mtx_ldr_load(const uint8_t *name, uint8_t depth, uint32_t expect_ver,
                 uint32_t *out_entry, uint32_t *out_total);

/*
 * How much arena a depth needs to hold that image: bss + lump + ldr1n's headroom,
 * read from the image's own header. 0 if the lump is missing or unreadable.
 * The boot uses this to size each depth's image slot, so the budget stays honest
 * rather than reserving a number somebody guessed.
 */
uint32_t mtx_ldr_image_bytes(const uint8_t *name);

/*
 * Hand off to an already-loaded image through ldr0N. Never returns.
 *
 * TWO CONVENTIONS, and they are not interchangeable:
 *
 *  mtx_ldr_enter(entry)         ECX = the service table pointer. This is the
 *                               matryoshka guest's own convention: the guest is
 *                               hand-written and expects its services there.
 *
 *  mtx_ldr_enter_engine(entry)  ECX = the entry address ITSELF. This is what ACE's
 *                               own `_start` requires: `sub $0x1008,%ecx` recovers
 *                               the image base from it, and every relocation is
 *                               applied relative to that. Handing the engine image
 *                               the service table instead would make it relocate
 *                               itself around a garbage base.
 *
 * Both hand ESI/EDI the Doom code/data segments, which both conventions need.
 */
void mtx_ldr_enter(uint32_t entry);
void mtx_ldr_enter_engine(uint32_t entry);

#endif // MATRYOSHKA_MTX_LDR_H
