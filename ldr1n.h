/*
 * matryoshka/ldr1n.h
 *
 * Reentrant second stage - the continuation-based replacement for ACE_LDR1.
 *
 * Stock ACE_LDR1 is one-shot by construction: it frees the stdin buffer,
 * lumpcache, lumpinfo and mainzone, and closes every WAD descriptor, then jumps
 * into the engine. Run it a second time and the parent's world is already gone.
 *
 * LDR1N inverts that. The service table below is the whole authority the loader
 * has, and it deliberately contains no free() and no close(). The loader
 * literally cannot destroy the parent's state; it can only read, allocate from
 * the depth's own memory, and hand off. Non-destructiveness is structural here,
 * not a convention someone has to remember.
 */
#ifndef MATRYOSHKA_LDR1N_H
#define MATRYOSHKA_LDR1N_H

#include <stdint.h>

typedef struct {
	int32_t  (*read)(void *ctx, int32_t fd, void *dst, uint32_t n);
	void    *(*alloc)(void *ctx, uint32_t n);
	void     (*enter)(void *ctx, void *code, uint32_t code_base,
	                  uint32_t data_base, uint32_t ace_base);
} mtx_ldr1n_ops_t;

/* Doom's WAD lumpinfo entry: char name[8], then the file descriptor at 8. */
#define MTX_LDR1N_LUMPINFO_SIZE 20u
#define MTX_LDR1N_FD_OFFSET      8u

typedef struct {
	int32_t  fd;		/* descriptor holding the code lump */
	uint32_t lump_size;	/* size of that lump */
	uint32_t offset;	/* byte offset of the lump */
} mtx_ldr1n_src_t;

typedef struct {
	void    *code;		/* depth-owned: [bss_size][code ...] */
	uint32_t code_size;
	int32_t *fdlist;	/* depth-owned descriptor list */
	uint32_t fdlist_len;	/* entries, excluding the terminator */
} mtx_ldr1n_out_t;

/*
 * The descriptor list the engine walks when it switches. Mirrors ACE_LDR1's
 * walk exactly: step lumpinfo, take the fd field, drop values equal to the
 * previous one (which is seeded to -1, so leading -1 entries are dropped too),
 * terminate with -1. Returns entries written, excluding the terminator, or 0 if
 * it does not fit.
 */
uint32_t mtx_ldr1n_fdlist(const uint8_t *lumpinfo, uint32_t numlumps,
                          int32_t *out, uint32_t cap);

/*
 * Load a depth's code without touching anything the parent owns: read the .bss
 * size, allocate from the depth's own memory, read the rest of the code. Layout
 * matches what the engine expects: [0..3] = bss size, [4..] = code.
 * Returns 0, or a negative step code.
 */
int mtx_ldr1n_load(const mtx_ldr1n_ops_t *ops, void *ctx,
                   const mtx_ldr1n_src_t *src,
                   void **out_mem, uint32_t *out_size);

/* fdlist + load, into a caller-owned result. */
int mtx_ldr1n_prepare(const mtx_ldr1n_ops_t *ops, void *ctx,
                      const mtx_ldr1n_src_t *src,
                      const uint8_t *lumpinfo, uint32_t numlumps,
                      int32_t *fdlist_scratch, uint32_t fdlist_cap,
                      mtx_ldr1n_out_t *out);

/* Final hand-off, in ldr1N.S: ESI=code, EDI=data, ECX=entry, jmp *%ecx. */
void mtx_ldr1n_enter(void *entry, uint32_t code_base, uint32_t data_base);

#endif /* MATRYOSHKA_LDR1N_H */
