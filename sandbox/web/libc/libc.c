/* libc.c - the C library for the browser build: memory, strings, formatted I/O, and
 * files that JavaScript keeps. It implements exactly what cc, as, check and the machine
 * call and nothing else, which is small enough that there was no reason to depend on a
 * full WebAssembly C library. */
#include <stdarg.h>
#include <stdint.h>
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "ctype.h"

/* what JavaScript provides (web/kit.js) */
#define IMPORT(n) __attribute__((import_module("kit"), import_name(n)))
IMPORT("open")  int js_open(const char *path, int len, int write);   /* a handle, or -1 */
IMPORT("size")  int js_size(int h);
IMPORT("read")  int js_read(int h, int pos, void *dst, int n);
IMPORT("write") void js_write(int h, const void *src, int n);        /* h 1, 2: stdout, stderr */
IMPORT("close") void js_close(int h);
IMPORT("exit")  _Noreturn void js_exit(int code);
IMPORT("argc")  int js_argc(void);
IMPORT("arg")   int js_arg(int i, char *dst, int max);

/* --- memory: a bump allocator with a size header, enough for short-lived tools --- */
extern unsigned char __heap_base;
static uintptr_t brk_ = 0;
void *malloc(size_t n)
{
	if (!brk_) brk_ = (uintptr_t)&__heap_base;
	uintptr_t p = (brk_ + 15) & ~(uintptr_t)15;
	uintptr_t end = p + 16 + n;
	size_t have = __builtin_wasm_memory_size(0) * 65536;
	if (end > have && __builtin_wasm_memory_grow(0, (end - have + 65535) / 65536) == (size_t)-1) return 0;
	*(size_t *)p = n;
	brk_ = end;
	return (void *)(p + 16);
}
void *calloc(size_t n, size_t size) { void *p = malloc(n * size); if (p) memset(p, 0, n * size); return p; }
void *realloc(void *p, size_t n)
{
	if (!p) return malloc(n);
	size_t old = *(size_t *)((char *)p - 16);
	void *q = malloc(n);
	if (q) memcpy(q, p, old < n ? old : n);
	return q;
}
void free(void *p) { (void)p; }
_Noreturn void exit(int code) { fflush(stdout); js_exit(code); }

/* --- strings --- */
void *memcpy(void *d, const void *s, size_t n) { unsigned char *a = d; const unsigned char *b = s; while (n--) *a++ = *b++; return d; }
void *memmove(void *d, const void *s, size_t n)
{
	unsigned char *a = d; const unsigned char *b = s;
	if (a < b) while (n--) *a++ = *b++; else while (n--) a[n] = b[n];
	return d;
}
void *memset(void *d, int c, size_t n) { unsigned char *a = d; while (n--) *a++ = (unsigned char)c; return d; }
int memcmp(const void *x, const void *y, size_t n)
{
	const unsigned char *a = x, *b = y;
	for (; n; n--, a++, b++) if (*a != *b) return *a - *b;
	return 0;
}
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b) { while (*a && *a == *b) a++, b++; return (unsigned char)*a - (unsigned char)*b; }
int strncmp(const char *a, const char *b, size_t n)
{
	for (; n; n--, a++, b++) { if (*a != *b) return (unsigned char)*a - (unsigned char)*b; if (!*a) return 0; }
	return 0;
}
char *strchr(const char *s, int c) { for (;; s++) { if (*s == (char)c) return (char *)s; if (!*s) return 0; } }
char *strrchr(const char *s, int c) { const char *r = 0; for (;; s++) { if (*s == (char)c) r = s; if (!*s) return (char *)r; } }
char *strstr(const char *h, const char *n)
{
	size_t k = strlen(n);
	for (; *h; h++) if (!strncmp(h, n, k)) return (char *)h;
	return k ? 0 : (char *)h;
}
size_t strcspn(const char *s, const char *r) { size_t n = 0; while (s[n] && !strchr(r, s[n])) n++; return n; }

static int digit(int c) { return isdigit(c) ? c - '0' : isalpha(c) ? (c | 32) - 'a' + 10 : 99; }
unsigned long long strtoull(const char *s, char **end, int base)
{
	while (isspace(*s)) s++;
	int neg = 0;
	if (*s == '-' || *s == '+') neg = *s++ == '-';
	if ((base == 0 || base == 16) && s[0] == '0' && (s[1] | 32) == 'x' && digit(s[2]) < 16) { s += 2; base = 16; }
	else if (base == 0) base = s[0] == '0' ? 8 : 10;
	unsigned long long v = 0;
	while (digit(*s) < base) v = v * (unsigned)base + (unsigned)digit(*s++);
	if (end) *end = (char *)s;
	return neg ? -v : v;
}
unsigned long strtoul(const char *s, char **end, int base) { return (unsigned long)strtoull(s, end, base); }
long strtol(const char *s, char **end, int base) { return (long)strtoull(s, end, base); }
int atoi(const char *s) { return (int)strtol(s, 0, 10); }

/* --- files --- */
struct FILE { int h, pos, write, eof; char buf[512]; int n; };
static FILE fstd[3] = { { -1, 0, 0, 0, {0}, 0 }, { 1, 0, 1, 0, {0}, 0 }, { 2, 0, 1, 0, {0}, 0 } };
FILE *stdin = &fstd[0], *stdout = &fstd[1], *stderr = &fstd[2];

FILE *fopen(const char *path, const char *mode)
{
	int w = mode[0] == 'w' || mode[0] == 'a';
	int h = js_open(path, (int)strlen(path), w);
	if (h < 0) return 0;
	FILE *f = calloc(1, sizeof *f);
	f->h = h;
	f->write = w;
	return f;
}
int fflush(FILE *f) { if (f->write && f->n) { js_write(f->h, f->buf, f->n); f->n = 0; } return 0; }
int fclose(FILE *f) { fflush(f); if (f->h > 2) js_close(f->h); return 0; }
size_t fwrite(const void *p, size_t size, size_t n, FILE *f)
{
	const char *s = p;
	size_t t = size * n;
	for (size_t i = 0; i < t; i++) { if (f->n == (int)sizeof f->buf) fflush(f); f->buf[f->n++] = s[i]; }
	if (f->h <= 2) fflush(f);
	return n;
}
size_t fread(void *p, size_t size, size_t n, FILE *f)
{
	if (!size || f->h < 0) return 0;
	int got = js_read(f->h, f->pos, p, (int)(size * n));
	f->pos += got;
	if (got < (int)(size * n)) f->eof = 1;
	return (size_t)got / size;
}
int fseek(FILE *f, long off, int whence)
{
	f->pos = (int)(whence == SEEK_SET ? off : whence == SEEK_CUR ? f->pos + off : js_size(f->h) + off);
	f->eof = 0;
	return 0;
}
long ftell(FILE *f) { return f->pos; }
static int fgetc_(FILE *f) { unsigned char c; return fread(&c, 1, 1, f) == 1 ? c : EOF; }
char *fgets(char *s, int n, FILE *f)
{
	int i = 0, c = 0;
	while (i < n - 1 && (c = fgetc_(f)) != EOF) { s[i++] = (char)c; if (c == '\n') break; }
	if (!i) return 0;
	s[i] = 0;
	return s;
}
int fputc(int c, FILE *f) { char ch = (char)c; fwrite(&ch, 1, 1, f); return c; }
int fputs(const char *s, FILE *f) { fwrite(s, 1, strlen(s), f); return 0; }
int putchar(int c) { return fputc(c, stdout); }
void perror(const char *s) { fprintf(stderr, "%s: cannot open\n", s); }

/* --- formatted output: %d %i %u %x %X %c %s %p %%, flags - + 0 space, width, precision, l ll z h --- */
typedef struct { char *s; size_t n, at; } Out;
static void put(Out *o, char c) { if (o->at + 1 < o->n) o->s[o->at] = c; o->at++; }
int vsnprintf(char *s, size_t n, const char *fmt, va_list ap)
{
	Out o = { s, n, 0 };
	for (; *fmt; fmt++) {
		if (*fmt != '%') { put(&o, *fmt); continue; }
		fmt++;
		int left = 0, plus = 0, zero = 0, space = 0, width = 0, prec = -1, lng = 0;
		for (;; fmt++) {
			if (*fmt == '-') left = 1; else if (*fmt == '+') plus = 1; else if (*fmt == '0') zero = 1;
			else if (*fmt == ' ') space = 1; else if (*fmt == '#') ; else break;
		}
		if (*fmt == '*') { width = va_arg(ap, int); fmt++; if (width < 0) { left = 1; width = -width; } }
		else while (isdigit(*fmt)) width = width * 10 + *fmt++ - '0';
		if (*fmt == '.') { fmt++; prec = 0; if (*fmt == '*') { prec = va_arg(ap, int); fmt++; } else while (isdigit(*fmt)) prec = prec * 10 + *fmt++ - '0'; }
		while (*fmt == 'l' || *fmt == 'z' || *fmt == 'h') { if (*fmt == 'l' || *fmt == 'z') lng++; fmt++; }
		char tmp[32], *body = tmp;
		int len = 0, neg = 0;
		char c = *fmt;
		if (c == 'd' || c == 'i' || c == 'u' || c == 'x' || c == 'X' || c == 'p') {
			unsigned long long v;
			if (c == 'p') { v = (uintptr_t)va_arg(ap, void *); c = 'x'; }
			else if (lng >= 2) v = va_arg(ap, unsigned long long);
			else if (lng == 1) v = (c == 'd' || c == 'i') ? (unsigned long long)(long long)va_arg(ap, long) : va_arg(ap, unsigned long);
			else v = (c == 'd' || c == 'i') ? (unsigned long long)(long long)va_arg(ap, int) : va_arg(ap, unsigned);
			if ((c == 'd' || c == 'i') && (long long)v < 0) { neg = 1; v = -v; }
			int base = (c == 'x' || c == 'X') ? 16 : 10;
			const char *dg = c == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
			char rev[32];
			int k = 0;
			do { rev[k++] = dg[v % (unsigned)base]; v /= (unsigned)base; } while (v);
			while (k < prec) rev[k++] = '0';
			while (k) tmp[len++] = rev[--k];
		} else if (c == 'c') {
			tmp[len++] = (char)va_arg(ap, int);
		} else if (c == 's') {
			body = va_arg(ap, char *);
			if (!body) body = "(null)";
			len = (int)strlen(body);
			if (prec >= 0 && len > prec) len = prec;
		} else {
			put(&o, c);
			continue;
		}
		char sign = neg ? '-' : plus ? '+' : space ? ' ' : 0;
		if (c == 'u' || c == 'x' || c == 'X' || c == 'c' || c == 's') sign = c == 'u' || c == 'x' || c == 'X' ? 0 : 0;
		int total = len + (sign != 0);
		if (!left && !zero) for (int k = total; k < width; k++) put(&o, ' ');
		if (sign) put(&o, sign);
		if (!left && zero) for (int k = total; k < width; k++) put(&o, '0');
		for (int k = 0; k < len; k++) put(&o, body[k]);
		if (left) for (int k = total; k < width; k++) put(&o, ' ');
	}
	if (o.n) o.s[o.at < o.n ? o.at : o.n - 1] = 0;
	return (int)o.at;
}
int snprintf(char *s, size_t n, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vsnprintf(s, n, fmt, ap); va_end(ap); return r; }
int vfprintf(FILE *f, const char *fmt, va_list ap)
{
	char buf[4096];
	va_list aq;
	va_copy(aq, ap);
	int r = vsnprintf(buf, sizeof buf, fmt, aq);
	va_end(aq);
	if (r >= (int)sizeof buf) {
		char *big = malloc((size_t)r + 1);
		vsnprintf(big, (size_t)r + 1, fmt, ap);
		fwrite(big, 1, (size_t)r, f);
	} else {
		fwrite(buf, 1, (size_t)r, f);
	}
	return r;
}
int fprintf(FILE *f, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vfprintf(f, fmt, ap); va_end(ap); return r; }
int printf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vfprintf(stdout, fmt, ap); va_end(ap); return r; }

/* --- formatted input: whitespace, %Ns, %x, %d; what the checker's symbol file needs --- */
int fscanf(FILE *f, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int got = 0, c = fgetc_(f);
	for (; *fmt; fmt++) {
		if (isspace(*fmt)) { while (c != EOF && isspace(c)) c = fgetc_(f); continue; }
		if (*fmt != '%') { if (c != *fmt) break; c = fgetc_(f); continue; }
		fmt++;
		int width = 0;
		while (isdigit(*fmt)) width = width * 10 + *fmt++ - '0';
		while (c != EOF && isspace(c)) c = fgetc_(f);
		if (c == EOF) { va_end(ap); return got ? got : EOF; }
		if (*fmt == 's') {
			char *d = va_arg(ap, char *);
			int i = 0;
			while (c != EOF && !isspace(c) && (!width || i < width)) { d[i++] = (char)c; c = fgetc_(f); }
			d[i] = 0;
		} else if (*fmt == 'x' || *fmt == 'd') {
			int base = *fmt == 'x' ? 16 : 10, neg = 0, any = 0;
			unsigned v = 0;
			if (c == '-') { neg = 1; c = fgetc_(f); }
			while (c != EOF && digit(c) < base) { v = v * (unsigned)base + (unsigned)digit(c); c = fgetc_(f); any = 1; }
			if (!any) break;
			*va_arg(ap, unsigned *) = neg ? -v : v;
		} else break;
		got++;
	}
	if (c != EOF) f->pos--;      /* push back the look-ahead */
	va_end(ap);
	return got;
}

/* --- the entry point: arguments from JavaScript --- */
int main(int argc, char **argv);
__attribute__((export_name("start"))) void start(void)
{
	int argc = js_argc();
	char **argv = calloc((size_t)argc + 1, sizeof *argv);
	for (int i = 0; i < argc; i++) { argv[i] = malloc(256); js_arg(i, argv[i], 256); }
	exit(main(argc, argv));
}
