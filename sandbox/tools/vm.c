/* vm.c - the D32 emulator. It is deterministic, so the same memory, registers and
 * keys always give the same run, cycle for cycle; the checker's solo runs and the
 * browser page's rewind both depend on that. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"

const char *d32_opname[OP_COUNT] = {
	"halt", "ldi", "lui", "mov", "add", "sub", "mul", "div", "mod", "and", "or", "xor",
	"shl", "shr", "seq", "sne", "slt", "sle", "addi", "ldw", "ldb", "stw", "stb", "jmp",
	"bz", "bnz", "call", "callr", "ret", "push", "pop", "sys", "jmpr"
};

/* What each instruction costs in cycles, which is the unit the scores are counted in. */
static const uint8_t cost[OP_COUNT] = {
	1, 1, 1, 1, 1, 1, 3, 8, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 1,
	1, 1, 2, 2, 2, 2, 2, 20, 1
};

uint32_t vm_rd32(Vm *vm, uint16_t a)
{
	return (uint32_t)vm->mem[a] | (uint32_t)vm->mem[(uint16_t)(a + 1)] << 8 |
	       (uint32_t)vm->mem[(uint16_t)(a + 2)] << 16 | (uint32_t)vm->mem[(uint16_t)(a + 3)] << 24;
}

void vm_wr32(Vm *vm, uint16_t a, uint32_t v)
{
	for (int i = 0; i < 4; i++)
		vm->mem[(uint16_t)(a + i)] = (uint8_t)(v >> (8 * i));
}

static void wr32(Vm *vm, uint16_t a, uint32_t v)
{
	vm_wr32(vm, a, v);
	if (vm->on_write)
		vm->on_write(vm, a, 4, vm->user);
}

static void wr8(Vm *vm, uint16_t a, uint8_t v)
{
	vm->mem[a] = v;
	if (vm->on_write)
		vm->on_write(vm, a, 1, vm->user);
}

void vm_init(Vm *vm, const char *disk)
{
	memset(vm, 0, sizeof *vm);
	vm->heap = VM_HEAP_START;
	vm->heap_limit = VM_HEAP_LIMIT;
	snprintf(vm->disk, sizeof vm->disk, "%s", disk ? disk : ".");
}

/* A relocatable file: "D32O", size, relocation count, entry offset, then the image,
   then (offset, type) pairs. Type 1 adds the base to an instruction's imm field,
   type 2 to a 32-bit word. */
int vm_load(Vm *vm, const char *name, uint16_t base, uint16_t *entry)
{
	char path[512];
	snprintf(path, sizeof path, "%s/%s", vm->disk, name);
	FILE *f = fopen(path, "rb");
	if (!f)
		return -1;
	uint8_t h[12];
	if (fread(h, 1, 12, f) != 12 || memcmp(h, "D32O", 4)) {
		fclose(f);
		return -1;
	}
	unsigned size = h[4] | h[5] << 8, nrel = h[6] | h[7] << 8, ent = h[8] | h[9] << 8;
	if ((unsigned)base + size > D32_MEM) {
		fclose(f);
		return -1;
	}
	if (fread(vm->mem + base, 1, size, f) != size) {
		fclose(f);
		return -1;
	}
	for (unsigned i = 0; i < nrel; i++) {
		uint8_t r[3];
		if (fread(r, 1, 3, f) != 3)
			break;
		uint16_t at = (uint16_t)(base + (r[0] | r[1] << 8));
		if (r[2] == 1) {
			uint16_t v = (uint16_t)(vm->mem[(uint16_t)(at + 2)] | vm->mem[(uint16_t)(at + 3)] << 8);
			v = (uint16_t)(v + base);
			vm->mem[(uint16_t)(at + 2)] = (uint8_t)v;
			vm->mem[(uint16_t)(at + 3)] = (uint8_t)(v >> 8);
		} else if (r[2] == 2) {
			vm_wr32(vm, at, vm_rd32(vm, at) + base);
		}
	}
	fclose(f);
	if (entry)
		*entry = (uint16_t)(base + ent);
	return (int)size;
}

int vm_boot(Vm *vm, const char *game)
{
	uint16_t entry;
	if (vm_load(vm, game, VM_GAME_BASE, &entry) < 0)
		return -1;
	vm->pc = entry;
	vm->r[D32_SP] = VM_BOOT_SP;
	return 0;
}

static void sys(Vm *vm, int n)
{
	uint32_t *r = vm->r;
	switch (n) {
	case SYS_EXIT:
		vm->halted = 1;
		break;
	case SYS_LOAD: {
		char name[64];
		int i;
		for (i = 0; i < 63 && vm->mem[(uint16_t)(r[0] + i)]; i++)
			name[i] = (char)vm->mem[(uint16_t)(r[0] + i)];
		name[i] = 0;
		r[0] = (uint32_t)vm_load(vm, name, (uint16_t)r[1], NULL);
		break;
	}
	case SYS_ALLOC: {
		uint32_t n4 = (r[0] + 3) & ~3u;
		if ((uint32_t)vm->heap + n4 > vm->heap_limit) {
			r[0] = 0;
		} else {
			r[0] = vm->heap;
			vm->heap = (uint16_t)(vm->heap + n4);
		}
		break;
	}
	case SYS_KEY:
		r[0] = vm->keys;
		break;
	case SYS_TICKS:
		r[0] = (uint32_t)(vm->cycles >> 10);
		break;
	case SYS_ARG:
		r[0] = r[0] < 8 ? (uint32_t)vm->args[r[0]] : 0;
		break;
	case SYS_PUTC:
		putchar((int)(r[0] & 0xff));
		break;
	default:
		break;
	}
	if (vm->on_sys)
		vm->on_sys(vm, n, vm->user);
}

int vm_step(Vm *vm)
{
	if (vm->halted)
		return 0;
	uint32_t w = vm_rd32(vm, vm->pc);
	int op = w & 0xff, a = (w >> 8) & 15, b = (w >> 12) & 15;
	int32_t imm = (int16_t)(w >> 16);
	uint16_t uimm = (uint16_t)(w >> 16);
	uint32_t *r = vm->r;
	uint16_t next = (uint16_t)(vm->pc + 4);
	if (op >= OP_COUNT || a >= D32_NREG || b >= D32_NREG) {
		snprintf(vm->fault, sizeof vm->fault, "bad instruction %08x at %04x", w, vm->pc);
		vm->halted = 2;
		return 0;
	}
	vm->cycles += cost[op];
	int c = imm & 15;
	if (c >= D32_NREG)
		c = 0;
	int32_t sb = (int32_t)r[b], sc = (int32_t)r[c];
	switch (op) {
	case OP_HALT: vm->halted = 1; return 0;
	case OP_LDI: r[a] = (uint32_t)imm; break;
	case OP_LUI: r[a] = (uint32_t)uimm << 16 | (r[a] & 0xffff); break;
	case OP_MOV: r[a] = r[b]; break;
	case OP_ADD: r[a] = r[b] + r[c]; break;
	case OP_SUB: r[a] = r[b] - r[c]; break;
	case OP_MUL: r[a] = (uint32_t)(sb * sc); break;
	case OP_DIV: r[a] = sc ? (uint32_t)(sb / sc) : 0; break;
	case OP_MOD: r[a] = sc ? (uint32_t)(sb % sc) : 0; break;
	case OP_AND: r[a] = r[b] & r[c]; break;
	case OP_OR:  r[a] = r[b] | r[c]; break;
	case OP_XOR: r[a] = r[b] ^ r[c]; break;
	case OP_SHL: r[a] = r[b] << (r[c] & 31); break;
	case OP_SHR: r[a] = (uint32_t)(sb >> (r[c] & 31)); break;
	case OP_SEQ: r[a] = r[b] == r[c]; break;
	case OP_SNE: r[a] = r[b] != r[c]; break;
	case OP_SLT: r[a] = sb < sc; break;
	case OP_SLE: r[a] = sb <= sc; break;
	case OP_ADDI: r[a] = r[b] + (uint32_t)imm; break;
	case OP_LDW: r[a] = vm_rd32(vm, (uint16_t)(r[b] + imm)); break;
	case OP_LDB: r[a] = vm->mem[(uint16_t)(r[b] + imm)]; break;
	case OP_STW: wr32(vm, (uint16_t)(r[b] + imm), r[a]); break;
	case OP_STB: wr8(vm, (uint16_t)(r[b] + imm), (uint8_t)r[a]); break;
	case OP_JMP: next = uimm; break;
	case OP_BZ:  if (!r[a]) next = uimm; break;
	case OP_BNZ: if (r[a]) next = uimm; break;
	case OP_CALL:
	case OP_CALLR: {
		uint16_t to = op == OP_CALL ? uimm : (uint16_t)r[a];
		r[D32_SP] = (uint16_t)(r[D32_SP] - 4);
		wr32(vm, (uint16_t)r[D32_SP], next);
		next = to;
		break;
	}
	case OP_RET:
		next = (uint16_t)vm_rd32(vm, (uint16_t)r[D32_SP]);
		r[D32_SP] = (uint16_t)(r[D32_SP] + 4);
		break;
	case OP_PUSH:
		r[D32_SP] = (uint16_t)(r[D32_SP] - 4);
		wr32(vm, (uint16_t)r[D32_SP], r[a]);
		break;
	case OP_POP:
		r[a] = vm_rd32(vm, (uint16_t)r[D32_SP]);
		r[D32_SP] = (uint16_t)(r[D32_SP] + 4);
		break;
	case OP_SYS: vm->pc = next; sys(vm, uimm); return !vm->halted;
	case OP_JMPR: next = (uint16_t)r[a]; break;
	}
	vm->pc = next;
	return 1;
}
