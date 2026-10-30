/*
 * matryoshka/ldr0N.S
 *
 * Reentrant first stage - the non-destructive replacement for ACE_LDR0.
 *
 * Why it exists: stock ACE_LDR0 recovers its own base with fixed stack-offset
 * arithmetic (`sub $0x34615, %esi` from the startup ESP). That is only ever true
 * once, at the original process start, which is what makes LDR0 impossible to
 * enter a second time - the first of the nested-instance blockers.
 *
 * This variant takes its bases as data (see ldr0n.h for why they arrive in a
 * descriptor rather than in ESI/EDI), announces the depth, and hands off. It
 * touches no zone state, frees nothing and closes no descriptors: all of that is
 * the host's business, done per depth, so a parent's state survives a child.
 *
 * Size budget: this stays a small PNAMES-patch-sized artifact, so keep it under
 * 256 bytes (the same limit loader/wadgen.py enforces for ACE_LDR0).
 *
 * Contract: void mtx_ldr0n_entry(const mtx_ldr0n_desc_t *desc)
 *   -> printf(desc->banner)
 *   -> ESI = desc->code_base, EDI = desc->data_base, EBX = desc->next_stage,
 *      ECX = desc->arg, EBP = desc->params
 *   -> jmp *%ebx            (never returns)
 */

#define _printf	0x3FE40

	.section .text
	.globl mtx_ldr0n_entry
	.type  mtx_ldr0n_entry, @function

mtx_ldr0n_entry:
	push	%ebp			/* EBP is scratch below, as in the original
					   LDR0 - but that one never returned, so it
					   could afford to trash the caller's frame
					   pointer. This one keeps it, which costs
					   two instructions and makes it callable. */
	mov	8(%esp), %esi		/* ESI = desc (4 push + 4 return address);
					   it doubles as scratch until the very end */
	mov	4(%esi), %ebp		/* code base, to build the printf address */
	mov	(%esi), %eax		/* banner */
	lea	_printf(%ebp), %ebp
	push	%eax
	call	*%ebp			/* Doom's printf is a C function: it preserves
					   ebx/esi/edi but clobbers eax/ecx/edx */
	add	$4, %esp

	/* Load the hand-off registers only now. Doing it before the call was a real
	   bug: printf destroyed ECX, so the next stage received a garbage argument. */
	mov	4(%esi), %eax		/* code base */
	mov	8(%esi), %edi		/* data base */
	mov	12(%esi), %ebx		/* next stage */
	mov	16(%esi), %ecx		/* argument */

	pop	%ebp			/* caller's frame pointer back */
	/* and then EBP belongs to the next stage: the nested-instance parameter
	   block (see mtx_params.h). The next stage's entry is the only reader, and
	   it is the one register that entry does not consume, so this is where a
	   block survives the .bss clear the entry performs. Loaded from the
	   descriptor while %esi still holds it. */
	mov	20(%esi), %ebp		/* parameter block */

	mov	%eax, %esi		/* ESI = code base (last: it held desc) */
	jmp	*%ebx			/* the depth's continuation, never returns */
	.size	mtx_ldr0n_entry, .-mtx_ldr0n_entry

	.section .note.GNU-stack,"",@progbits
