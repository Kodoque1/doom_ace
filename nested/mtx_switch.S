/*
 * matryoshka/mtx_switch.S
 *
 * The one piece that must be assembly: the context switch primitive and the
 * bootstrap trampoline for a fresh depth.
 *
 *   void mtx_switch_ctx(uint32_t **save_esp, uint32_t *load_esp)
 *
 * Saves the callee-saved registers on the current stack, publishes the current
 * ESP through *save_esp, loads load_esp as the new stack, restores the
 * callee-saved registers from it and returns. On a resume, "returns" lands back
 * inside the caller of the mtx_switch_ctx that switched away, i.e. the switch is
 * symmetric and needs no per-site bookkeeping.
 *
 * A brand-new depth's stack is pre-seeded by the scheduler (mtx_core.c) so that
 * the final `ret` here lands on mtx_trampoline with (%esp) = the entry argument.
 *
 * Why assembly and not setjmp: this has to work inside ACE, where the C runtime
 * does not exist and the game loop (D_DoomLoop) never returns. There is no
 * libc and no unwinding; ESP/EIP are ours to move.
 */

	.section .text
	.globl mtx_switch_ctx
	.globl mtx_trampoline
	.type  mtx_switch_ctx, @function
	.type  mtx_trampoline, @function

mtx_switch_ctx:
	mov	4(%esp), %eax		/* eax = save_esp (uint32_t **) */
	mov	8(%esp), %edx		/* edx = load_esp (new ESP value) */
	push	%ebp
	push	%ebx
	push	%esi
	push	%edi
	mov	%esp, (%eax)		/* *save_esp = esp */
	mov	%edx, %esp		/* switch stacks */
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret				/* resumes the loaded depth */
	.size	mtx_switch_ctx, .-mtx_switch_ctx

/*
 * Entered by the `ret` above on a fresh depth's seeded stack, with
 * (%esp) = arg. mtx_run_entry never returns; the trailing calls are a safety
 * net only.
 */
mtx_trampoline:
	mov	(%esp), %eax
	push	%eax
	call	mtx_run_entry		/* C: dispatch the depth's entry(arg) */
	call	mtx_finish		/* should be unreachable */
	hlt
	.size	mtx_trampoline, .-mtx_trampoline

	.section .note.GNU-stack,"",@progbits
