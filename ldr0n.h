/*
 * matryoshka/ldr0n.h
 *
 * Contract of the reentrant first stage.
 *
 * The stock ACE_LDR0 recovers its own base with fixed stack arithmetic
 * (`sub $0x34615, %esi` off the startup ESP), which is only true once, at the
 * original process start - that is what makes LDR0 impossible to enter a second
 * time. `ldr0n_entry` takes its bases as data instead, so it can run at any
 * depth on any stack.
 *
 * The bases travel in a descriptor rather than in ESI/EDI because a matryoshka
 * depth entry is dispatched through C (`mtx_run_entry`), which has already
 * clobbered the argument registers by the time the entry is called. One pointer
 * argument is the only thing that survives that path intact.
 */
#ifndef MATRYOSHKA_LDR0N_H
#define MATRYOSHKA_LDR0N_H

#include <stdint.h>

typedef struct {
	const char *banner;	/* printed on entry, via Doom's printf */
	uint32_t    code_base;	/* Doom code segment (where printf lives)    */
	uint32_t    data_base;	/* Doom data segment                          */
	void       *next_stage;	/* jumped to, with ESI/EDI/EBX/ECX/EBP set; no return */
	uint32_t    arg;	/* passed to next_stage in ECX                */
	uint32_t    params;	/* passed to next_stage in EBP                */
} mtx_ldr0n_desc_t;

/* Prints the banner, then jmp *next_stage with ESI = code_base, EDI = data_base,
   EBX = next_stage, ECX = arg, EBP = params. Touches no zone state, frees
   nothing, closes nothing. The argument goes in a register rather than on the
   stack because this is a jump, not a call: there is no return frame for a
   cdecl argument to sit below.

   EBP carries the nested-instance parameter block (see mtx_params.h). It is the
   one register the next stage's own entry does not consume, and the engine's
   `_start` stores it into a global right after clearing `.bss` - which is why a
   block handed over here survives to be read, while anything written into that
   `.bss` beforehand would not. Pass 0 for a stage that does not want one.

   Note the register contract: setting ESI/EDI/EBX/ECX/EBP for the next stage IS
   the hand-off, so this function does not restore them for its caller - which is
   fine only because it never returns to it. Its caller's frame is abandoned. */
void mtx_ldr0n_entry(const mtx_ldr0n_desc_t *desc);

#endif /* MATRYOSHKA_LDR0N_H */
