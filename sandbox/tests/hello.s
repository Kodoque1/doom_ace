; hello: print a string, compute 6*7, call a function, exit with r0 = 42
.entry start
start:
	ldi r1, msg
loop:
	ldb r0, [r1]
	bz r0, done
	sys 6
	addi r1, r1, 1
	jmp loop
done:
	ldi r0, 6
	push r0
	call times7
	addi sp, sp, 4
	sys 0
times7:              ; r0 = [sp+4] * 7
	ldw r0, [sp+4]
	ldi r1, 7
	mul r0, r0, r1
	ret
msg:
	.asciz "hello, D32\n"
