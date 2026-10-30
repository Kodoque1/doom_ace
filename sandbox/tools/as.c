/* as.c - the D32 assembler.
 *
 *   as [-o out.o] [-s out.sym] file.s ...
 *
 * Output is relocatable (see vm_load): every reference to a label is an offset from
 * the start of the image, fixed up by the loader. The symbol file lists every label
 * as "name offset" in hex, which is how the kit finds the game's addresses.
 *
 * Syntax, one statement per line, ';' starts a comment:
 *   label:
 *   ldi r0, 42          lui r0, 0x1234       mov r1, r0
 *   add r0, r1, r2      (sub mul div mod and or xor shl shr seq sne slt sle)
 *   addi r0, r1, -4     ldw r0, [r6+8]       stb r0, [r1]
 *   jmp L   bz r0, L   bnz r0, L   call f   callr r0   ret   push r0   pop r0
 *   sys 4   jmpr r0   halt
 *   .word 1, label   .byte 1, 'a'   .ascii "x"   .asciz "x"   .zero 16   .align 4
 *   .entry label   .set name, value
 * Registers: r0..r7, sp (= r7), fp (= r6). Numbers: 12, -3, 0x1f, 'c'.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "d32.h"

#define MAXSYM 8192
#define MAXREL 16384
static struct { char name[64]; int value; int defined; int abs; } sym[MAXSYM];  /* abs: from .set */
static int nsym;
static uint8_t img[65536];
static int pc, pass, err;
static struct { uint16_t at; uint8_t type; } rel[MAXREL];
static int nrel;
static const char *curfile;
static int curline;
static char entry_name[64];

static void die(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d: ", curfile, curline);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
	err = 1;
}

static int lookup(const char *n, int create)
{
	for (int i = 0; i < nsym; i++)
		if (!strcmp(sym[i].name, n))
			return i;
	if (!create)
		return -1;
	if (nsym == MAXSYM) {
		die("too many symbols");
		return 0;
	}
	snprintf(sym[nsym].name, sizeof sym[nsym].name, "%s", n);
	return nsym++;
}

/* --- a tiny expression parser over one line --- */
static const char *p;
static void ws(void) { while (*p == ' ' || *p == '\t') p++; }

static int ident(char *out)
{
	ws();
	int n = 0;
	if (!(isalpha((unsigned char)*p) || *p == '_' || *p == '.' || *p == '$'))
		return 0;
	while (isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '$')
		if (n < 63)
			out[n++] = *p++;
		else
			p++;
	out[n] = 0;
	return 1;
}

/* value of an expression; *reloc is set when it is "label + constant" */
static int expr(int *reloc)
{
	int v = 0, sign = 1, nrel_terms = 0;
	*reloc = 0;
	for (;;) {
		ws();
		int t;
		char name[64];
		if (*p == '-') { sign = -sign; p++; continue; }
		if (*p == '+') { p++; continue; }
		if (*p == '\'') {
			p++;
			if (*p == '\\') { p++; t = *p == 'n' ? '\n' : *p == '0' ? 0 : *p; } else t = (unsigned char)*p;
			p++;
			if (*p == '\'') p++;
		} else if (isdigit((unsigned char)*p)) {
			t = (int)strtol(p, (char **)&p, 0);
		} else if (ident(name)) {
			int i = lookup(name, pass == 2 ? 0 : 1);
			if (pass == 2 && (i < 0 || !sym[i].defined)) {
				die("undefined symbol '%s'", name);
				t = 0;
			} else {
				t = i >= 0 ? sym[i].value : 0;
			}
			if (i < 0 || !sym[i].abs) {
				if (sign > 0) nrel_terms++; else nrel_terms--;
			}
		} else {
			die("bad expression near '%s'", p);
			return 0;
		}
		v += sign * t;
		sign = 1;
		ws();
		if (*p == '+' || *p == '-')
			continue;
		break;
	}
	*reloc = nrel_terms == 1;
	return v;
}

static int reg(void)
{
	char n[64];
	if (!ident(n)) { die("register expected"); return 0; }
	if (!strcmp(n, "sp")) return D32_SP;
	if (!strcmp(n, "fp")) return D32_FP;
	if (n[0] == 'r' && n[1] >= '0' && n[1] <= '7' && !n[2]) return n[1] - '0';
	die("bad register '%s'", n);
	return 0;
}

static void comma(void) { ws(); if (*p == ',') p++; else die("',' expected"); }

static void emit8(int v) { if (pc < 65536) img[pc] = (uint8_t)v; pc++; }
static void emit32(uint32_t v) { for (int i = 0; i < 4; i++) emit8((int)(v >> (8 * i))); }

static void addrel(int type)
{
	if (pass != 2) return;
	if (nrel == MAXREL) { die("too many relocations"); return; }
	rel[nrel].at = (uint16_t)pc;
	rel[nrel].type = (uint8_t)type;
	nrel++;
}

static void insn(int op, int a, int b, int imm, int reloc)
{
	if (reloc) addrel(1);
	emit32(d32_enc(op, a, b, imm));
}

/* "[rB + expr]", "[rB - expr]", "[rB]" */
static void memop(int *b, int *imm, int *reloc)
{
	ws();
	if (*p != '[') { die("'[' expected"); return; }
	p++;
	*b = reg();
	ws();
	*imm = 0;
	*reloc = 0;
	if (*p == '+' || *p == '-')
		*imm = expr(reloc);
	ws();
	if (*p == ']') p++; else die("']' expected");
}

static int opcode(const char *m)
{
	for (int i = 0; i < OP_COUNT; i++)
		if (!strcmp(m, d32_opname[i]))
			return i;
	return -1;
}

static void string_bytes(int zero)
{
	ws();
	if (*p != '"') { die("string expected"); return; }
	p++;
	while (*p && *p != '"') {
		int c = (unsigned char)*p++;
		if (c == '\\') {
			c = (unsigned char)*p++;
			c = c == 'n' ? '\n' : c == 't' ? '\t' : c == '0' ? 0 : c;
		}
		emit8(c);
	}
	if (*p == '"') p++;
	if (zero) emit8(0);
}

static void line(char *s)
{
	char *c = s;
	int inq = 0;
	for (; *c; c++) {
		if (*c == '"') inq = !inq;
		if (*c == '\'' && c[1] && c[2] == '\'') { c += 2; continue; }
		if (*c == ';' && !inq) { *c = 0; break; }
	}
	p = s;
	char m[64];
	for (;;) {
		const char *save = p;
		if (!ident(m)) { ws(); if (*p) die("syntax error near '%s'", p); return; }
		ws();
		if (*p == ':') {
			p++;
			int i = lookup(m, 1);
			if (pass == 1 && sym[i].defined) die("'%s' defined twice", m);
			sym[i].value = pc;
			sym[i].defined = 1;
			continue;
		}
		p = save;
		ident(m);
		break;
	}
	int a = 0, b = 0, cc = 0, v = 0, rl = 0;
	if (!strcmp(m, ".word")) {
		do { v = expr(&rl); if (rl) addrel(2); emit32((uint32_t)v); ws(); } while (*p == ',' && p++);
	} else if (!strcmp(m, ".byte")) {
		do { ws(); if (*p == '"') string_bytes(0); else emit8(expr(&rl)); ws(); } while (*p == ',' && p++);
	} else if (!strcmp(m, ".ascii")) {
		string_bytes(0);
	} else if (!strcmp(m, ".asciz")) {
		string_bytes(1);
	} else if (!strcmp(m, ".zero")) {
		v = expr(&rl);
		while (v-- > 0) emit8(0);
	} else if (!strcmp(m, ".align")) {
		v = expr(&rl);
		while (v > 0 && pc % v) emit8(0);
	} else if (!strcmp(m, ".set")) {      /* .set name, constant */
		char n[64];
		ident(n);
		comma();
		v = expr(&rl);
		int i = lookup(n, 1);
		sym[i].value = v;
		sym[i].defined = 1;
		sym[i].abs = 1;
	} else if (!strcmp(m, ".entry")) {
		ident(entry_name);
	} else {
		int op = opcode(m);
		if (op < 0) { die("unknown instruction '%s'", m); return; }
		switch (op) {
		case OP_HALT: case OP_RET: insn(op, 0, 0, 0, 0); break;
		case OP_LDI: case OP_LUI: a = reg(); comma(); v = expr(&rl); insn(op, a, 0, v, rl); break;
		case OP_MOV: a = reg(); comma(); b = reg(); insn(op, a, b, 0, 0); break;
		case OP_ADDI: a = reg(); comma(); b = reg(); comma(); v = expr(&rl); insn(op, a, b, v, rl); break;
		case OP_LDW: case OP_LDB: case OP_STW: case OP_STB:
			a = reg(); comma(); memop(&b, &v, &rl); insn(op, a, b, v, rl); break;
		case OP_JMP: case OP_CALL: case OP_SYS: v = expr(&rl); insn(op, 0, 0, v, rl); break;
		case OP_BZ: case OP_BNZ: a = reg(); comma(); v = expr(&rl); insn(op, a, 0, v, rl); break;
		case OP_CALLR: case OP_PUSH: case OP_POP: case OP_JMPR: a = reg(); insn(op, a, 0, 0, 0); break;
		default: /* three registers */
			a = reg(); comma(); b = reg(); comma(); cc = reg(); insn(op, a, b, cc, 0); break;
		}
	}
	ws();
	if (*p) die("unexpected '%s'", p);
}

static void run_pass(int n, char **files, int nfiles)
{
	pass = n;
	pc = 0;
	nrel = 0;
	for (int i = 0; i < nfiles; i++) {
		FILE *f = fopen(files[i], "r");
		if (!f) { fprintf(stderr, "as: cannot open %s\n", files[i]); exit(1); }
		curfile = files[i];
		curline = 0;
		char buf[1024];
		while (fgets(buf, sizeof buf, f)) {
			curline++;
			buf[strcspn(buf, "\r\n")] = 0;
			line(buf);
		}
		fclose(f);
	}
}

int main(int argc, char **argv)
{
	const char *out = "a.o", *symout = NULL;
	char *files[64];
	int nfiles = 0;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
		else if (!strcmp(argv[i], "-s") && i + 1 < argc) symout = argv[++i];
		else if (nfiles < 64) files[nfiles++] = argv[i];
	}
	if (!nfiles) { fprintf(stderr, "usage: as [-o out.o] [-s out.sym] file.s ...\n"); return 2; }
	run_pass(1, files, nfiles);
	if (err) return 1;
	run_pass(2, files, nfiles);
	if (err) return 1;
	if (pc > 65535) { fprintf(stderr, "as: image too large (%d bytes)\n", pc); return 1; }
	int entry = 0;
	if (entry_name[0]) {
		int i = lookup(entry_name, 0);
		if (i < 0 || !sym[i].defined) { fprintf(stderr, "as: entry '%s' undefined\n", entry_name); return 1; }
		entry = sym[i].value;
	}
	FILE *f = fopen(out, "wb");
	if (!f) { perror(out); return 1; }
	uint8_t h[12] = { 'D', '3', '2', 'O', (uint8_t)pc, (uint8_t)(pc >> 8), (uint8_t)nrel, (uint8_t)(nrel >> 8),
	                  (uint8_t)entry, (uint8_t)(entry >> 8), 0, 0 };
	fwrite(h, 1, 12, f);
	fwrite(img, 1, (size_t)pc, f);
	for (int i = 0; i < nrel; i++) {
		uint8_t r[3] = { (uint8_t)rel[i].at, (uint8_t)(rel[i].at >> 8), rel[i].type };
		fwrite(r, 1, 3, f);
	}
	fclose(f);
	if (symout) {
		FILE *s = fopen(symout, "w");
		if (!s) { perror(symout); return 1; }
		for (int i = 0; i < nsym; i++)
			if (sym[i].defined && !sym[i].abs)
				fprintf(s, "%s %04x\n", sym[i].name, sym[i].value);
		fclose(s);
	}
	return 0;
}
