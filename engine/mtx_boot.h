// kgsws' ACE Engine / matryoshka
////
// Engine-side entry points for the nested-instance host. Both are declared with
// the same call convention as the engine functions that call them
// (regparm(2)/no_caller_saved_registers), so calling them cannot break Doom's
// assumption that eax/ecx/edx survive.
#include "mtx_sched.h"
#include "mtx_raw.h"		// mtx_raw_t, for mtx_engine_raw_pool()

// Called from the top of Z_Init, BEFORE the zone consumes the rest of RAM.
// Idempotent: a nested depth reaches Z_Init too, and the arena belongs to the
// machine, so a second call is a no-op rather than a second claim.
void mtx_engine_reserve(void) MTX_FRAME_TICK_ABI;

// Build the CALLING depth's own zone in its arena slot and prove Doom's allocator
// can work in it. Returns the zone, or 0. This is the nested half of Z_Init - the
// root takes DPMI RAM, a child takes its own slot - and it is also the call the
// loaded guest makes through its service table, so both paths are the same one.
void *mtx_engine_zone_init(void);

// The engine's raw (non-zone) allocation entry points. The zone was half of what
// a depth needs; these are the other half. alloc decides by the current depth,
// free/realloc decide by the pointer's provenance - see the block comment in
// mtx_boot.c for why those are two different questions.
void *mtx_engine_alloc(uint32_t size);
void *mtx_engine_realloc(void *ptr, uint32_t size);
void  mtx_engine_free(void *ptr);

// Read the parameter block a nested instance was entered with (%ebp at the entry,
// stored by asm.S) and act on it: report what the instance got, and point its
// rendering at its own framebuffer. Called at the top of Z_Init, before the
// root/child branch, because that block is what decides the branch. A no-op when
// this copy of the engine IS the host.
void mtx_engine_apply_params(void);
void mtx_esp_probe(const uint8_t *tag);	/* TEMPORARY: stack trace through a boot */

// The stack the continued boot must run on: an instance keeps the stack it was
// entered on, the root gets Doom's own. Called from the engine's entry (asm.S),
// which is why it cannot be a macro over `doom_data_segment` any more.
uint32_t mtx_engine_stack(void);

// ---- diegetic rendering -------------------------------------------------------
//
// One wall texture is repointed at the other instance's framebuffer, so the nested
// Doom appears as a surface in the world instead of an overlay on top of it. The
// number is a TEXTURE index (position in Doom's TEXTURE1+TEXTURE2 list). 189 is
// PIPE4, and the NUMBER comes from the renderer itself - counted, per drawn column -
// rather than from a WAD analysis: the first attempt used what
// verify/which_texture.py called MAP01's most used texture (409, ZZWOLF11) and
// produced not one line of the effect, because the walls actually in view are
// PIPE4/BRONZE2/TEKGREN/SUPPORT. Measured in the view the demo opens on: 189=3024
// columns, 46=2835, 372=378, 374=231, 375=231, 281=21 - so 189 is the largest
// surface on screen, which is what makes the effect legible rather than a keyhole.
#define MTX_SCREEN_TEX 189u

// The frame to show on those walls: for the ROOT, the nested instance's; for a
// DEPTH (whose own copy of the renderer runs the same code), its parent's. A depth
// cannot reach the host's screens[] array, so its pointer comes from its parameter
// block.
uint8_t *mtx_world_screen(void);

// WHICH TEXTURE IS A SCREEN, in the level this depth is on.
//
// The planted surface is a per-MAP choice (verify/mkwadlevels.py plants it and
// records its name in the level's own MTXLEV0d marker), so the number cannot be
// a compile-time constant any more: the ring's levels are different maps, and a
// texture index that only exists in one of them paints nothing in the others -
// silently. Resolved at level load, in the depth's own copy of the engine.
//
// The variable is read per textured column by the renderer, which is why it is a
// variable and not an accessor; MTX_SCREEN_TEX above stays the measured default.
extern uint32_t mtx_screen_tex;
void mtx_level_marker_read(const uint8_t *map_name);
void mtx_level_report(void);

// OPTION B: the running depth appends ITS OWN level WAD to its lump table, from
// inside the level-load path, just before the marker is read and the map lump is
// looked up - so the depth's map AND its marker come from a file the depth
// opened itself. Every level WAD carries the exploit lumps byte-identical, so a
// depth that levels up has loaded the exploit again.
//
// `map_name` is the map this level load is about (Doom's `map_lump.name`), and
// it is the TEST, not decoration: the add happens only when that map is not
// already in the table, so it is a no-op when the command line carried the level
// WADs and the mechanism when it did not. A no-op at the root (whose WAD came
// from the command line) and once per depth image. See mtx_boot.c.
void mtx_level_wad_add(const uint8_t *map_name);

// The seg loop RECORDS the screen rectangle of a screen-textured wall; the frame
// hook PAINTS it from the other instance's framebuffer. Two halves so that every
// bound lives in one place instead of inside Doom's clipping arithmetic.
void mtx_screen_seg_note(uint32_t x, uint32_t yl, uint32_t yh, int32_t texcol);
void mtx_screen_paint(void);

// Which depth this copy of the engine is running as: a nested instance's own
// (from its parameter block), or the scheduler's when this is the host. The
// engine's init branches on this, so it must be the same answer the allocator
// gets - hence one function, used by both.
uint8_t mtx_engine_depth(void);

// The raw pool the calling code allocates from (see mtx_raw.h), for diagnostics
// that need the pool's own account of what it did.
mtx_raw_t *mtx_engine_raw_pool(void);

// TEMPORARY diagnostic: report an absurd allocation request, with its caller.
void mtx_alloc_probe(uint32_t size, uint32_t caller, uint32_t self);

// TEMPORARY diagnostic: the stack an error screen was reached from. Called from
// the I_Error hook in asm.S, which pushes its arguments - so this is a plain C
// function and must not carry MTX_FRAME_TICK_ABI.
void mtx_error_probe(uint32_t esp, uint32_t ret, uint32_t fmt, uint32_t value);

// TEMPORARY diagnostic: the first index the texture loop processes, per depth.
void MTX_FRAME_TICK_ABI mtx_tex_probe(uint32_t first, uint32_t count);

// The HOST's frame service, called BY a depth through the pointer in its parameter
// block to hand the frame back to the root (see mtx_params.h). Plain C on purpose:
// it runs in the host's code with the host's state, and it has to be callable
// through a function pointer the instance holds.
void mtx_host_take_frame(void);

// TEMPORARY diagnostic: what the texture count was derived from, per depth.
void MTX_FRAME_TICK_ABI mtx_texcount_probe(uint32_t t1, uint32_t t2, uint32_t idx1,
                                           uint32_t idx2);

// TEMPORARY diagnostic: report a lump-cache call whose index cannot be valid.
// Same call convention as the ACE hook functions that call it.
void MTX_FRAME_TICK_ABI mtx_lump_probe(uint32_t site, uint32_t idx, uint32_t value,
                                       uint32_t aux);

// Called once per frame from the per-frame hook targets.
void mtx_engine_frame(void) MTX_FRAME_TICK_ABI;
