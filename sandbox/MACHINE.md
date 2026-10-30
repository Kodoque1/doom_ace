# The D32 machine

The whole computer the sandbox runs on. `tools/d32.h` is the reference; `tools/vm.c`
is the emulator.

## State

- **Memory:** 64 KiB, flat, byte-addressed, little-endian. Addresses wrap at 16 bits.
  No protection: any code can write anywhere.
- **Registers:** `r0`–`r7`, 32 bits each. By convention `r7` is the stack pointer
  (`sp`, grows down) and `r6` the frame pointer (`fp`). There is also `pc`.
- **Nothing else:** no flags, no privileged mode, no interrupts. A program runs until
  it calls the operating system or halts.

## Instructions

One 32-bit word each: `op` in bits 0–7, register `a` in bits 8–11, register `b` in
bits 12–15, a 16-bit `imm` in bits 16–31. Three-register forms take `c` from the low
4 bits of `imm`. `sext` is sign extension.

| Instruction | Effect | Cycles |
|---|---|---|
| `ldi a, imm` | `a = sext(imm)` | 1 |
| `lui a, imm` | `a = imm << 16 \| (a & 0xffff)` | 1 |
| `mov a, b` | `a = b` | 1 |
| `add sub and or xor shl shr a, b, c` | `a = b op c`; `shr` is arithmetic | 1 |
| `mul a, b, c` | `a = b * c` | 3 |
| `div mod a, b, c` | signed; dividing by 0 gives 0 | 8 |
| `seq sne slt sle a, b, c` | `a = (b op c) ? 1 : 0`, signed | 1 |
| `addi a, b, imm` | `a = b + sext(imm)` | 1 |
| `ldw ldb a, [b + imm]` | load a word, or a byte zero-extended | 2 |
| `stw stb a, [b + imm]` | store a word, or the low byte | 2 |
| `jmp imm` · `jmpr a` | `pc = imm` · `pc = a` | 1 |
| `bz bnz a, imm` | branch if `a` is zero · not zero | 1 |
| `call imm` · `callr a` | push the return address, jump | 2 |
| `ret` · `push a` · `pop a` | through `sp`, 4 bytes at a time | 2 |
| `sys imm` | operating-system call | 20 |
| `halt` | stop | 1 |

## The operating system

`sys n` is handled by the emulator, outside the 64 KiB: the OS keeps its own state
(the allocator's top, the keys) where no program can reach it, and keeps nothing on
a program's behalf. Arguments go in `r0`–`r2`, the result comes back in `r0`.

| n | Call | |
|---|---|---|
| 0 | `exit` | stop the machine |
| 1 | `load(name, base)` | load a relocatable file from the disk at `base`; its size, or -1 |
| 2 | `alloc(bytes)` | a fresh block from a bump allocator, or 0 |
| 3 | `key()` | the keys held this frame (bit 0: y) |
| 4 | `present(fb)` | show a 64×40 framebuffer, one byte per pixel, 16 colours; ends a frame |
| 5 | `ticks()` | cycles / 1024 |
| 6 | `putc(c)` | write a character to the console |
| 7 | `copy(id, lo, hi)` | a hint for the checker: copy `id` runs on the stack `lo`–`hi` |
| 8 | `arg(i)` | boot argument `i` |

## Memory map at boot

| Address | |
|---|---|
| `0x0000`–`0x0fff` | free |
| `0x1000`– | the game, loaded from `game.o`; `pc` starts at its entry |
| `0x5800`–`0xefff` | what `alloc` hands out |
| `0xf000`–`0xffff` | the boot stack; `sp` starts at `0xfff0` |

## Files

A relocatable file (`.o`) is `"D32O"`, the image size, the relocation count and the
entry offset (16 bits each, then 2 bytes of padding), the image, then 3 bytes per
relocation: a 16-bit offset and a type. Type 1 adds the load address to an
instruction's `imm`, type 2 to a 32-bit word. That is how the same level script can
run at any address.
