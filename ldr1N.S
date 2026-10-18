/*
 * matryoshka/ldr1N.S
 *
 * The final hand-off of the reentrant second stage. ldr1n.c does the reading
 * and bootstrapping; this is only the jump ACE_LDR1 ended with (jmp *%ecx), now
 * taking its bases explicitly instead of re-deriving them from the stack.
 *
 *   void mtx_ldr1n_enter(void *entry, uint32_t code_base, uint32_t data_base)
 *   -> ESI = code base, EDI = data base, ECX = entry, jmp *%ecx
 */
	.section .text
	.globl mtx_ldr1n_enter
	.type  mtx_ldr1n_enter, @function

mtx_ldr1n_enter:
	mov	4(%esp), %ecx		/* entry */
	mov	8(%esp), %esi		/* code base */
	mov	12(%esp), %edi		/* data base */
	jmp	*%ecx			/* never returns */
	.size	mtx_ldr1n_enter, .-mtx_ldr1n_enter

	.section .note.GNU-stack,"",@progbits
