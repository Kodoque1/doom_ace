/*
 * matryoshka/dos_state.h
 *
 * The DOS side of save_state/load_state: what actually has to move when two
 * engine instances swap. Engine-agnostic on purpose - regions, patched
 * locations and per-depth pointer variables are all registered by the wiring
 * layer, so this file builds both into ACE and into the host test.
 *
 * Three things are per-depth:
 *
 *   1. shared global regions  - Doom's data segment and the engine's own
 *      writable .data/.bss. Copied out on switch-away, copied in on switch-in.
 *   2. patched locations      - every address ACE hooked. Saved/restored as raw
 *      bytes so depth N's hook set does not bleed into depth N+1.
 *   3. pinned pointer globals - `mainzone` and `screen_buffer` are ordinary Doom
 *      variables, but each depth needs its OWN heap and its OWN framebuffer.
 *      They are restored from the snapshot like everything else and then
 *      overwritten with the depth's own values.
 *
 * A depth's snapshot is seeded by cloning the parent's LIVE state the first time
 * the depth is entered, so a child starts life as a copy of its parent and then
 * diverges - which is the behaviour a nested instance wants.
 */
#ifndef MATRYOSHKA_DOS_STATE_H
#define MATRYOSHKA_DOS_STATE_H

#include <stdint.h>
#include "context.h"

#define MTX_DOS_MAX_REGIONS 6
#define MTX_DOS_MAX_HOOKS   512
#define MTX_DOS_MAX_PINS    4

/* pinned pointer globals we know about */
enum {
	MTX_PIN_MAINZONE = 0,	/* void ** / memzone_t ** - the depth's zone heap */
	MTX_PIN_SCREEN   = 1,	/* uint8_t ** - ACE's own screen_buffer */
	MTX_PIN_DOOMSCREEN = 2,	/* uint8_t ** - Doom's OWN screen pointer
	                           (`screens[0]`), which the renderer writes through.
	                           ACE copies it once at init, so pinning only ACE's
	                           copy leaves Doom drawing into the root's frame. */
	MTX_PIN_COUNT
};

/* One-time wire-up. snapshot_mem is max_depth * per_depth_bytes of caller-owned
   memory (zone memory in DOS); pass NULL/0 for the built-in static pool (host). */
void mtx_dos_init(uint8_t max_depth, void *snapshot_mem, uint32_t per_depth_bytes);

/* Register a shared writable region (e.g. the Doom data segment). */
int  mtx_dos_add_region(void *base, uint32_t size);

/* Register a location ACE patched, so it can be isolated per depth. */
int  mtx_dos_add_hook(void *addr, uint32_t len);

/* Register where a pinned pointer global lives, and hand a depth its value. */
int  mtx_dos_set_pin(void *var_addr, int pin);
void mtx_dos_set_value(uint8_t depth, int pin, void *value);

/* Per-depth footprint actually required, so the caller can size its pool. */
uint32_t mtx_dos_snapshot_bytes(void);

/* How many regions and patched locations the wiring registered. For the boot log and
   for tests that need to know the isolation is actually populated rather than merely
   implemented - an empty registry isolates nothing and looks identical from outside. */
uint32_t mtx_dos_region_count(void);
uint32_t mtx_dos_hook_count(void);

/* The callbacks to pass into mtx_init(). */
void mtx_dos_save_state(mtx_context_t *outgoing);
void mtx_dos_load_state(mtx_context_t *incoming);

#endif /* MATRYOSHKA_DOS_STATE_H */
