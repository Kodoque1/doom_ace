/*
 * matryoshka/ldr0N.S
 *
 * Reentrant first-stage loader - the non-destructive replacement for ACE_LDR0.
 *
 * Why it exists: the stock ACE_LDR0 recovers its own base with fixed
 * stack-offset arithmetic (`sub $0x34615, %esi` from the startup ESP). That is
 * only ever true once, at the original process start, which is what makes LDR0
 * impossible to enter a second time - the first of the nested-instance
 * blockers. LDR0N takes the bases as explicit parameters instead, so it can be
 * switched into at any depth, on any stack.
 *
 * Entry contract (set up by the matryoshka host before the switch):
 *   %esi    = code base   (this loader's segment base)
 *   %edi    = data base   (engine data segment base)
 *   %ebx    = next-stage pointer (the depth's own continuation)
 *   4(%esp) = const char * banner for this depth
 *
 * It prints the banner and hands off to *%ebx. It touches no zone state, frees
 * nothing and closes no descriptors - all of that is the host's business, done
 * per depth, so a parent's state survives a child.
 *
 * Size budget: like ACE_LDR0, this is embedded as a PNAMES fake patch, so keep
 * it under 256 bytes (loader/wadgen.py enforces the same limit).
 */

#define _printf	0x3FE40

	.section .text
	.globl _start

_start:
	push	%ebx			/* save next-stage pointer */
	push	%esi			/* save code base */
	push	%edi			/* save data base */

	/* printf(banner) */
	mov	16(%esp), %eax		/* 4(%esp) before the 3 pushes above */
	push	%eax
	lea	_printf(%esi), %ebp
	call	*%ebp
	add	$4, %esp

	pop	%edi
	pop	%esi
	pop	%ebx

	jmp	*%ebx			/* depth's continuation, never returns */

	.section .note.GNU-stack,"",@progbits
