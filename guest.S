/*
 * matryoshka/guest.S
 *
 * The payload a nested depth loads from the WAD and executes.
 *
 * Why it exists: ldr1N can read a code image into depth-owned memory and enter
 * it, but "it was entered" is not evidence on its own. This guest is small,
 * position-independent, and reports two things from inside the loaded image:
 *
 *   - a magic dword ('MTXG'), so the host can see that code fetched from a WAD
 *     lump, placed at an arbitrary depth-owned address and jumped to, really ran;
 *   - its own entry address, computed by the guest ITSELF (call/pop), so the host
 *     can see that the image executed at an address it was not linked for.
 *
 * Between the two reports it yields, which is the interesting part: control
 * leaves the loaded image, the parent runs a frame, and the image resumes exactly
 * where it stopped. A nested instance is a coroutine, not a one-shot call.
 *
 * Entered as an ordinary cdecl function - `void mtx_guest(void *svc)` - because
 * the matryoshka depth entry is dispatched through C (mtx_run_entry), which is
 * also why the bases travel in a descriptor rather than in ESI/EDI (see ldr0N.S).
 *
 * Service table, owned by the host, arrives in ECX (the ldr0N hand-off is a jump,
 * not a call, so there is no argument frame on the stack):
 *   [0] uint32_t *report     where the guest leaves its evidence
 *   [4] void    (*yield)()   parent runs a frame; RETURNS here afterwards
 *   [8] void    (*finish)()  end this depth (never returns)
 *
 * Position independence is not a luxury: the same image is loaded at a different
 * address for every depth, so it must contain no absolute addresses. Note it also
 * cannot keep anything in registers across the yield - a context switch is a real
 * switch, so it parks the service pointer on its own stack and reloads from there.
 *
 * Image header (see ldr1n.h): [0] .bss size, [4] version, then the payload. The
 * entry is therefore payload + 8, the same arithmetic engine/asm.S does.
 */

	.section .text
	.globl mtx_guest
	.type  mtx_guest, @function

	/* Same image header the engine's own code lump carries, so both load alike. */
	.4byte	0			/* this guest needs no .bss */
	.4byte	0x4D545847		/* 'MTXG' - what the loader expects here */

mtx_guest:
	push	%ecx			/* park the service pointer on our own
					   stack: the yield below clobbers every
					   caller-saved register */

	mov	%ecx, %eax
	mov	(%eax), %edx		/* edx = report slot */
	movl	$0x4D545847, (%edx)	/* report[0]: the loaded image ran */

	call	*4(%eax)		/* yield: parent runs, we come back here */

	mov	(%esp), %eax		/* svc back from our own stack */
	mov	(%eax), %edx
	call	9f
9:	pop	%ecx
	subl	$(9b - mtx_guest), %ecx	/* self-computed entry address */
	mov	%ecx, 4(%edx)		/* report[1]: where this image is running */

	mov	(%esp), %eax
	jmp	*8(%eax)		/* finish: depth ends, control goes home */
	.size	mtx_guest, .-mtx_guest

	.section .note.GNU-stack,"",@progbits
