/* d32.h - the D32 machine, shared by every tool in the kit.
 *
 * 64 KiB of flat, byte-addressed memory. Eight 32-bit registers r0..r7; r7 is the
 * stack pointer (sp) and r6 is the frame pointer (fp) by convention. No flags, no
 * privileged mode, no memory protection, no interrupts.
 *
 * Every instruction is one 32-bit little-endian word:
 *
 *     bits  0..7   op
 *     bits  8..11  a      (register)
 *     bits 12..15  b      (register)
 *     bits 16..31  imm    (16 bits; signed for arithmetic and offsets, an address for jumps)
 *
 * Three-register ops take their third register c from the low 4 bits of imm.
 */
#ifndef D32_H
#define D32_H
#include <stdint.h>

#define D32_MEM   65536u
#define D32_NREG  8
#define D32_SP    7
#define D32_FP    6

enum {
	OP_HALT = 0,
	OP_LDI,  /* a = sext(imm)                          */
	OP_LUI,  /* a = imm << 16 | (a & 0xffff)           */
	OP_MOV,  /* a = b                                  */
	OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
	OP_AND, OP_OR, OP_XOR, OP_SHL, OP_SHR,   /* SHR is arithmetic              */
	OP_SEQ, OP_SNE, OP_SLT, OP_SLE,          /* a = (b op c) ? 1 : 0, signed   */
	OP_ADDI, /* a = b + sext(imm)                      */
	OP_LDW,  /* a = mem32[b + sext(imm)]               */
	OP_LDB,  /* a = mem8[b + sext(imm)], zero-extended */
	OP_STW,  /* mem32[b + sext(imm)] = a               */
	OP_STB,  /* mem8[b + sext(imm)] = a & 0xff         */
	OP_JMP,  /* pc = imm                               */
	OP_BZ,   /* if a == 0: pc = imm                    */
	OP_BNZ,  /* if a != 0: pc = imm                    */
	OP_CALL, /* sp -= 4; mem32[sp] = pc; pc = imm      */
	OP_CALLR,/* sp -= 4; mem32[sp] = pc; pc = a        */
	OP_RET,  /* pc = mem32[sp]; sp += 4                */
	OP_PUSH, /* sp -= 4; mem32[sp] = a                 */
	OP_POP,  /* a = mem32[sp]; sp += 4                 */
	OP_SYS,  /* operating-system call number imm       */
	OP_JMPR, /* pc = a                                 */
	OP_COUNT
};

/* Operating-system calls. Arguments in r0, r1, r2; the result in r0. */
enum {
	SYS_EXIT = 0,   /* stop the machine                                        */
	SYS_LOAD,       /* r0 = name, r1 = base: load a relocatable file there;    */
	                /*      r0 = its size, or -1                               */
	SYS_ALLOC,      /* r0 = bytes: r0 = address of a fresh block, or 0         */
	SYS_KEY,        /* r0 = the keys held this frame (bit 0: y)                */
	SYS_PRESENT,    /* r0 = a 64x40 framebuffer: show it; this ends a frame    */
	SYS_TICKS,      /* r0 = cycles / 1024                                      */
	SYS_PUTC,       /* r0 = a character for the console                        */
	SYS_COPY,       /* checker hint: r0 = copy id, r1 = stack low, r2 = high   */
	SYS_ARG,        /* r0 = i: r0 = boot argument i (0 if none)                */
	SYS_COUNT
};

#define FB_W 64
#define FB_H 40

static inline uint32_t d32_enc(int op, int a, int b, int imm)
{
	return (uint32_t)(op & 0xff) | (uint32_t)(a & 15) << 8 | (uint32_t)(b & 15) << 12 |
	       (uint32_t)(imm & 0xffff) << 16;
}

extern const char *d32_opname[OP_COUNT];
#endif
