// kgsws' ACE Engine
////
// WAD initialization replacement.
// Fixes bug: https://doomwiki.org/wiki/PWAD_size_limit

void wad_init();

uint64_t wad_name64(const uint8_t *name);

int32_t wad_check_lump(const uint8_t *name);
int32_t wad_get_lump(const uint8_t *name);
uint32_t wad_read_lump(void *dest, int32_t idx, uint32_t limit);
void *wad_cache_lump(int32_t idx, uint32_t *size);
void *wad_cache_optional(const uint8_t *name, uint32_t *size);
void wad_hide_lump(const uint8_t *name);

// Evidence for the hide above: how many lumps the IWAD contributed, and how many
// PWAD copies of a lump directory were hidden. Reported at the first frame.
extern uint32_t wad_iwad_lumps;
extern uint32_t wad_dir_hidden;

void wad_handle_range(uint16_t ident, void (*cb)(lumpinfo_t*));
void wad_handle_lump(const uint8_t *name, void (*cb)(lumpinfo_t*));

