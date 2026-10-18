/*
 * matryoshka/dos_wire.h
 *
 * The ACE-specific wiring, split so it can run in two phases: register what has
 * to move (which yields the snapshot size), then hand out the per-depth values.
 * mtx_dos_boot() needs that split because the pool size is only known after the
 * regions are registered.
 */
#ifndef MATRYOSHKA_DOS_WIRE_H
#define MATRYOSHKA_DOS_WIRE_H

#include <stdint.h>
#include "dos_state.h"

/* Registers regions, patched locations and pin addresses for the CURRENT
   mtx_dos_init(). Returns the per-depth snapshot size in bytes. */
uint32_t mtx_dos_wire_register(void);

/* Fills in each depth's pinned values (its own heap and framebuffer). */
void mtx_dos_wire_values(uint8_t max_depth, void **heaps, void **screens);

/* Convenience: init + register + values in one go. */
void mtx_dos_wire(uint8_t max_depth,
                  void *snapshot_mem, uint32_t snapshot_per_depth,
                  void **heaps, void **screens);

/* Call once per patched address (from utils_install_hooks). */
void mtx_dos_wire_hook(void *addr);

#endif /* MATRYOSHKA_DOS_WIRE_H */
