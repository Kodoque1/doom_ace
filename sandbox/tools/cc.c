/* cc.c - the kit's C compiler, which turns a subset of C into D32 assembly.
 *
 *   cc [-I dir] [-o out.s] file.c ...      (the files form one translation unit)
 *
 * Supported: int, char (unsigned, 8 bits), void, pointers, arrays, struct, enum,
 * typedef, function pointers; globals with initialisers; if, while, do, for, break,
 * continue, return; the C operators except the comma outside for(); casts, sizeof;
 * #include "...", #define (with parameters), #undef, #ifdef, #ifndef, #else, #endif.
 * Not supported: floats, varargs, switch, goto, struct values (assign or pass by
 * pointer), unsigned arithmetic (all arithmetic is signed 32-bit).
 *
 * Builtins:
 *   asm("...")            emit assembly lines, inside a function or at top level
 *   __sys(n, a, b, c)     operating-system call n with r0..r2 = a, b, c; gives r0
 *   __hook(name, args)    call name_default(args) from a call site labelled hook_name,
 *                         so the site can be found and patched
 *
 * Each statement is preceded by a comment "; @file:line", so a listing can map code
 * back to the C.
 *
 * The code it generates is a simple stack machine: r0 is the accumulator,
 * temporaries go on the stack and r6 is the frame pointer. Arguments are pushed right
 * to left and popped by the caller, parameter i is at [fp + 8 + 4i], and the result
 * comes back in r0, so nothing but sp and fp survives a call. Globals are emitted
 * after the code, between the labels __data_start and __data_end, which is how the
 * kit finds the game's globals.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- errors and memory ---------- */
static const char *err_file;
static int err_line;
static void die(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d: error: ", err_file ? err_file : "cc", err_line);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
	exit(1);
}
static void *zalloc(size_t n)
{
	void *p = calloc(1, n);
	if (!p) die("out of memory");
	return p;
}
static char *dupn(const char *s, int n)
{
	char *d = zalloc((size_t)n + 1);
	memcpy(d, s, (size_t)n);
	return d;
}

/* ---------- tokens ---------- */
enum { TK_IDENT, TK_NUM, TK_STR, TK_PUNCT, TK_EOF };
typedef struct Tok {
	int kind;
	char *s;        /* text (identifier, punctuator) or bytes (string) */
	int len;        /* string length in bytes                          */
	int val;        /* number                                          */
	int bol;        /* first token on its line                         */
	int space;      /* preceded by white space                         */
	const char *file;
	int line;
} Tok;

typedef struct { Tok *v; int n, cap; } TokVec;
static void tv_push(TokVec *tv, Tok t)
{
	if (tv->n == tv->cap) {
		tv->cap = tv->cap ? tv->cap * 2 : 256;
		tv->v = realloc(tv->v, sizeof(Tok) * (size_t)tv->cap);
		if (!tv->v) die("out of memory");
	}
	tv->v[tv->n++] = t;
}

static const char *puncts[] = {
	"<<=", ">>=", "...", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||",
	"+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", NULL
};

static int escape(const char **pp)
{
	const char *p = *pp;
	int c = (unsigned char)*p++;
	if (c == '\\') {
		c = (unsigned char)*p++;
		switch (c) {
		case 'n': c = '\n'; break;
		case 't': c = '\t'; break;
		case 'r': c = '\r'; break;
		case '0': c = 0; break;
		case 'x': c = (int)strtol(p, (char **)&p, 16); break;
		default: break;
		}
	}
	*pp = p;
	return c;
}

static char *read_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *s = zalloc((size_t)n + 2);
	if (fread(s, 1, (size_t)n, f) != (size_t)n) die("cannot read %s", path);
	fclose(f);
	s[n] = '\n';
	return s;
}

static void tokenize(const char *file, const char *src, TokVec *out)
{
	const char *p = src;
	int line = 1, bol = 1, space = 0;
	err_file = file;
	while (*p) {
		if (*p == '\n') { line++; bol = 1; space = 1; p++; continue; }
		if (isspace((unsigned char)*p)) { space = 1; p++; continue; }
		if (p[0] == '\\' && p[1] == '\n') { p += 2; line++; space = 1; continue; }
		if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; continue; }
		if (p[0] == '/' && p[1] == '*') {
			p += 2;
			while (*p && !(p[0] == '*' && p[1] == '/')) { if (*p == '\n') line++; p++; }
			if (*p) p += 2;
			space = 1;
			continue;
		}
		Tok t = { 0 };
		t.space = space;
		space = 0;
		t.file = file;
		t.line = line;
		t.bol = bol;
		bol = 0;
		err_line = line;
		if (isdigit((unsigned char)*p)) {
			char *e;
			t.kind = TK_NUM;
			t.val = (int)strtoul(p, &e, 0);
			while (*e == 'u' || *e == 'U' || *e == 'l' || *e == 'L') e++;
			t.s = dupn(p, (int)(e - p));
			p = e;
		} else if (isalpha((unsigned char)*p) || *p == '_') {
			const char *s = p;
			while (isalnum((unsigned char)*p) || *p == '_') p++;
			t.kind = TK_IDENT;
			t.s = dupn(s, (int)(p - s));
		} else if (*p == '\'') {
			p++;
			t.kind = TK_NUM;
			t.val = escape(&p);
			if (*p != '\'') die("bad character literal");
			p++;
			t.s = "'c'";
		} else if (*p == '"') {
			char buf[4096];
			int n = 0;
			p++;
			while (*p && *p != '"') {
				if (*p == '\n') die("newline in string");
				if (n >= (int)sizeof buf - 1) die("string too long");
				buf[n++] = (char)escape(&p);
			}
			if (*p != '"') die("unterminated string");
			p++;
			t.kind = TK_STR;
			t.s = dupn(buf, n);
			t.len = n;
		} else {
			int k;
			for (k = 0; puncts[k]; k++)
				if (!strncmp(p, puncts[k], strlen(puncts[k])))
					break;
			int n = puncts[k] ? (int)strlen(puncts[k]) : 1;
			t.kind = TK_PUNCT;
			t.s = dupn(p, n);
			p += n;
		}
		tv_push(out, t);
	}
}

/* ---------- the preprocessor ---------- */
typedef struct Macro {
	char *name;
	int funclike, nparams;
	char *params[16];
	TokVec body;
	int busy;
	struct Macro *next;
} Macro;
static Macro *macros;
static const char *incdirs[16];
static int nincdirs;

static Macro *find_macro(const char *name)
{
	for (Macro *m = macros; m; m = m->next)
		if (!strcmp(m->name, name))
			return m;
	return NULL;
}

static int is_p(Tok *t, const char *s) { return t->kind == TK_PUNCT && !strcmp(t->s, s); }

static void expand(TokVec *in, int *i, TokVec *out);

/* expand a list of tokens completely */
static void expand_all(TokVec *in, TokVec *out)
{
	int i = 0;
	while (i < in->n)
		expand(in, &i, out);
}

/* copy in->v[*i] to out, expanding it if it is a macro */
static void expand(TokVec *in, int *i, TokVec *out)
{
	Tok t = in->v[*i];
	if (t.kind == TK_IDENT && !strcmp(t.s, "__LINE__")) {
		t.kind = TK_NUM;
		t.val = t.line;
		tv_push(out, t);
		(*i)++;
		return;
	}
	Macro *m = t.kind == TK_IDENT ? find_macro(t.s) : NULL;
	if (!m || m->busy || (m->funclike && !(*i + 1 < in->n && is_p(&in->v[*i + 1], "(")))) {
		tv_push(out, t);
		(*i)++;
		return;
	}
	(*i)++;
	TokVec rep = { 0 };
	if (!m->funclike) {
		for (int k = 0; k < m->body.n; k++) tv_push(&rep, m->body.v[k]);
	} else {
		TokVec args[16];
		memset(args, 0, sizeof args);
		int na = 0, depth = 0;
		(*i)++;                                  /* "(" */
		if (*i < in->n && is_p(&in->v[*i], ")")) {
			(*i)++;
		} else {
			na = 1;
			for (;;) {
				if (*i >= in->n) die("unterminated call of macro %s", m->name);
				Tok a = in->v[(*i)++];
				if (depth == 0 && is_p(&a, ")")) break;
				if (depth == 0 && is_p(&a, ",")) { if (na == 16) die("too many arguments"); na++; continue; }
				if (is_p(&a, "(")) depth++;
				if (is_p(&a, ")")) depth--;
				tv_push(&args[na - 1], a);
			}
		}
		if (na != m->nparams) die("macro %s takes %d arguments, got %d", m->name, m->nparams, na);
		TokVec xargs[16];
		memset(xargs, 0, sizeof xargs);
		for (int k = 0; k < na; k++) expand_all(&args[k], &xargs[k]);
		for (int k = 0; k < m->body.n; k++) {
			Tok b = m->body.v[k];
			int pi = -1;
			if (b.kind == TK_IDENT)
				for (int q = 0; q < m->nparams; q++)
					if (!strcmp(b.s, m->params[q])) pi = q;
			if (pi < 0) tv_push(&rep, b);
			else for (int q = 0; q < xargs[pi].n; q++) tv_push(&rep, xargs[pi].v[q]);
		}
	}
	for (int k = 0; k < rep.n; k++) { rep.v[k].file = t.file; rep.v[k].line = t.line; rep.v[k].bol = 0; }
	m->busy = 1;
	expand_all(&rep, out);
	m->busy = 0;
}

static void preprocess(const char *path, TokVec *out, int depth);

static int line_end(TokVec *raw, int i) { return i >= raw->n || raw->v[i].bol; }

static void preprocess_tokens(TokVec *raw, const char *path, TokVec *out, int depth)
{
	int skip[64], nskip = 0, skipping = 0;
	int i = 0;
	while (i < raw->n) {
		Tok *t = &raw->v[i];
		err_file = t->file;
		err_line = t->line;
		if (t->bol && is_p(t, "#")) {
			i++;
			Tok *d = &raw->v[i];
			const char *dn = d->kind == TK_IDENT ? d->s : "";
			int start = i;
			if (!strcmp(dn, "ifdef") || !strcmp(dn, "ifndef")) {
				int def = find_macro(raw->v[i + 1].s) != NULL;
				if (nskip == 64) die("#if nested too deep");
				skip[nskip++] = skipping;
				if (!skipping) skipping = dn[2] == 'd' ? !def : def;
			} else if (!strcmp(dn, "else")) {
				if (!nskip) die("#else without #if");
				if (!skip[nskip - 1]) skipping = !skipping;
			} else if (!strcmp(dn, "endif")) {
				if (!nskip) die("#endif without #if");
				skipping = skip[--nskip];
			} else if (skipping) {
				/* ignore */
			} else if (!strcmp(dn, "define")) {
				Tok *n = &raw->v[i + 1];
				Macro *m = zalloc(sizeof *m);
				m->name = n->s;
				i += 2;
				/* "#define F(x) ..." takes parameters; "#define V (x)" does not */
				if (i < raw->n && !line_end(raw, i) && is_p(&raw->v[i], "(") && !raw->v[i].space) {
					m->funclike = 1;
					i++;
					while (!is_p(&raw->v[i], ")")) {
						if (raw->v[i].kind == TK_IDENT) m->params[m->nparams++] = raw->v[i].s;
						i++;
					}
					i++;
				}
				while (!line_end(raw, i)) tv_push(&m->body, raw->v[i++]);
				m->next = macros;
				macros = m;
				continue;
			} else if (!strcmp(dn, "undef")) {
				for (Macro **pm = &macros; *pm; pm = &(*pm)->next)
					if (!strcmp((*pm)->name, raw->v[i + 1].s)) { *pm = (*pm)->next; break; }
			} else if (!strcmp(dn, "include")) {
				Tok *f = &raw->v[i + 1];
				if (f->kind != TK_STR) die("#include \"file\" expected");
				char dir[512], full[1024];
				snprintf(dir, sizeof dir, "%s", path);
				char *slash = strrchr(dir, '/');
				if (slash) slash[1] = 0; else dir[0] = 0;
				snprintf(full, sizeof full, "%s%s", dir, f->s);
				FILE *probe = fopen(full, "r");
				for (int k = 0; !probe && k < nincdirs; k++) {
					snprintf(full, sizeof full, "%s/%s", incdirs[k], f->s);
					probe = fopen(full, "r");
				}
				if (!probe) die("cannot find include file %s", f->s);
				fclose(probe);
				if (depth > 32) die("#include nested too deep");
				preprocess(dupn(full, (int)strlen(full)), out, depth + 1);
			} else {
				die("unknown directive #%s", dn);
			}
			i = start + 1;
			while (!line_end(raw, i)) i++;
			continue;
		}
		if (skipping) { i++; continue; }
		expand(raw, &i, out);
	}
	if (nskip) die("#if without #endif");
}

static void preprocess(const char *path, TokVec *out, int depth)
{
	char *src = read_file(path);
	if (!src) die("cannot open %s", path);
	TokVec raw = { 0 };
	tokenize(path, src, &raw);
	preprocess_tokens(&raw, path, out, depth);
}

/* ---------- types ---------- */
enum { TY_VOID, TY_CHAR, TY_INT, TY_PTR, TY_ARRAY, TY_STRUCT, TY_FUNC };
typedef struct Member { char *name; struct Type *ty; int offset; struct Member *next; } Member;
typedef struct Type {
	int kind, size, align, len;
	struct Type *base;     /* pointer, array: the element; function: the result */
	Member *members;
	int defined;
	int nparams;
} Type;

static Type ty_void_ = { TY_VOID, 1, 1, 0, 0, 0, 0, 0 };
static Type ty_char_ = { TY_CHAR, 1, 1, 0, 0, 0, 0, 0 };
static Type ty_int_ = { TY_INT, 4, 4, 0, 0, 0, 0, 0 };
static Type *ty_void = &ty_void_, *ty_char = &ty_char_, *ty_int = &ty_int_;

static Type *ptr_to(Type *b) { Type *t = zalloc(sizeof *t); t->kind = TY_PTR; t->size = t->align = 4; t->base = b; return t; }
static Type *array_of(Type *b, int len)
{
	Type *t = zalloc(sizeof *t);
	t->kind = TY_ARRAY;
	t->base = b;
	t->len = len;
	t->size = b->size * len;
	t->align = b->align;
	return t;
}
static Type *func_of(Type *ret, int nparams) { Type *t = zalloc(sizeof *t); t->kind = TY_FUNC; t->base = ret; t->size = t->align = 4; t->nparams = nparams; return t; }
static int is_int(Type *t) { return t->kind == TY_INT || t->kind == TY_CHAR; }
static int is_scalar(Type *t) { return is_int(t) || t->kind == TY_PTR; }

/* ---------- symbols ---------- */
typedef struct Var {
	char *name;
	Type *ty;
	int local, offset;     /* locals: offset from fp */
	int is_enum, value;    /* enum constants */
	int is_typedef;
	int defined;           /* globals: storage emitted */
	char *label;           /* static locals: the global label, when not the name */
	int depth;
	struct Var *next;
} Var;
static Var *globals, *locals;
static int scope_depth;
typedef struct Tag { char *name; Type *ty; struct Tag *next; } Tag;
static Tag *tags;

static Var *find_var(const char *name)
{
	for (Var *v = locals; v; v = v->next) if (!strcmp(v->name, name)) return v;
	for (Var *v = globals; v; v = v->next) if (!strcmp(v->name, name)) return v;
	return NULL;
}

/* ---------- the syntax tree ---------- */
enum {
	N_NUM, N_STR, N_VAR, N_ADDR, N_DEREF, N_CALL, N_SYS, N_HOOK, N_ASSIGN, N_OPASSIGN,
	N_PREINC, N_POSTINC, N_BIN, N_AND, N_OR, N_COND, N_NEG, N_NOT, N_BNOT, N_CAST, N_COMMA,
	/* statements */
	N_IF, N_WHILE, N_DO, N_FOR, N_RETURN, N_BLOCK, N_EXPR, N_BREAK, N_CONTINUE, N_ASM
};
typedef struct Node {
	int kind;
	Type *ty;
	int op;                 /* N_BIN, N_OPASSIGN: the operator character or code */
	int val;                /* N_NUM; N_PREINC/POSTINC: the step                 */
	char *name;             /* N_STR label, N_CALL/N_HOOK name, N_ASM text       */
	Var *var;
	struct Node *l, *r, *c, *init, *inc, *body, *els;
	struct Node *args[16];
	int nargs;
	struct Node *next;      /* in a block */
	int line;
	const char *file;
} Node;

/* binary operator codes beyond single characters */
enum { OP_SHL = 256, OP_SHR, OP_LE, OP_GE, OP_EQ, OP_NE };

static TokVec toks;
static int pos;
static Tok *tk(void) { return &toks.v[pos]; }
static int at(const char *s) { Tok *t = tk(); return (t->kind == TK_PUNCT || t->kind == TK_IDENT) && !strcmp(t->s, s); }
static int accept(const char *s) { if (at(s)) { pos++; return 1; } return 0; }
static void expect(const char *s)
{
	err_file = tk()->file;
	err_line = tk()->line;
	if (!accept(s)) die("'%s' expected before '%s'", s, tk()->kind == TK_EOF ? "end of file" : tk()->s);
}
static char *ident_tok(void)
{
	err_file = tk()->file;
	err_line = tk()->line;
	if (tk()->kind != TK_IDENT) die("identifier expected before '%s'", tk()->s);
	return toks.v[pos++].s;
}

static Node *mk(int kind) { Node *n = zalloc(sizeof *n); n->kind = kind; n->line = tk()->line; n->file = tk()->file; return n; }
static Node *num(int v) { Node *n = mk(N_NUM); n->val = v; n->ty = ty_int; return n; }

/* decay arrays and functions to pointers */
static Type *value_type(Type *t)
{
	if (t->kind == TY_ARRAY) return ptr_to(t->base);
	if (t->kind == TY_FUNC) return ptr_to(t);
	return t;
}

/* ---------- declarations ---------- */
static int is_typename(void)
{
	Tok *t = tk();
	if (t->kind != TK_IDENT) return 0;
	static const char *kw[] = { "int", "char", "void", "struct", "enum", "unsigned", "signed",
	                            "const", "volatile", "static", "extern", "typedef", "long", "short", NULL };
	for (int i = 0; kw[i]; i++) if (!strcmp(t->s, kw[i])) return 1;
	Var *v = find_var(t->s);
	return v && v->is_typedef;
}

static Node *assign_expr(void);
static Node *expr(void);
static Node *cond_expr(void);
static Type *declarator(Type *ty, char **name);
static int const_eval(Node *n, int *ok);
int is_typename_at(int i);

static int const_expr(void)
{
	Node *n = cond_expr();
	int ok = 1, v = const_eval(n, &ok);
	if (!ok) die("constant expression expected");
	return v;
}

typedef struct { int is_static, is_extern, is_typedef; } Attr;

static Type *struct_decl(void)
{
	char *tag = NULL;
	if (tk()->kind == TK_IDENT) tag = ident_tok();
	Type *ty = NULL;
	if (tag)
		for (Tag *t = tags; t; t = t->next)
			if (!strcmp(t->name, tag)) ty = t->ty;
	if (!ty || (at("{") && ty->defined)) {
		ty = zalloc(sizeof *ty);
		ty->kind = TY_STRUCT;
		ty->align = 1;
		if (tag) { Tag *t = zalloc(sizeof *t); t->name = tag; t->ty = ty; t->next = tags; tags = t; }
	}
	if (!accept("{")) return ty;
	Member head = { 0 }, *cur = &head;
	int off = 0;
	while (!accept("}")) {
		Attr a = { 0 };
		Type *base = NULL;
		extern Type *declspec(Attr *);
		base = declspec(&a);
		do {
			char *name = NULL;
			Type *mt = declarator(base, &name);
			Member *m = zalloc(sizeof *m);
			m->name = name;
			m->ty = mt;
			off = (off + mt->align - 1) / mt->align * mt->align;
			m->offset = off;
			off += mt->size;
			if (mt->align > ty->align) ty->align = mt->align;
			cur = cur->next = m;
		} while (accept(","));
		expect(";");
	}
	ty->members = head.next;
	ty->size = (off + ty->align - 1) / ty->align * ty->align;
	ty->defined = 1;
	return ty;
}

static void enum_decl(void)
{
	if (tk()->kind == TK_IDENT) pos++;
	if (!accept("{")) return;
	int v = 0;
	while (!accept("}")) {
		char *name = ident_tok();
		if (accept("=")) v = const_expr();
		Var *e = zalloc(sizeof *e);
		e->name = name;
		e->is_enum = 1;
		e->value = v++;
		e->ty = ty_int;
		e->depth = scope_depth;
		if (scope_depth) { e->next = locals; locals = e; } else { e->next = globals; globals = e; }
		if (!accept(",")) { expect("}"); break; }
	}
}

Type *declspec(Attr *a)
{
	Type *ty = NULL;
	for (;;) {
		Tok *t = tk();
		err_file = t->file;
		err_line = t->line;
		if (accept("static")) { if (a) a->is_static = 1; continue; }
		if (accept("extern")) { if (a) a->is_extern = 1; continue; }
		if (accept("typedef")) { if (!a) die("typedef not allowed here"); a->is_typedef = 1; continue; }
		if (accept("const") || accept("volatile") || accept("unsigned") || accept("signed") ||
		    accept("long") || accept("short")) {
			if (!ty) ty = ty_int;
			continue;
		}
		if (accept("int")) { ty = ty_int; continue; }
		if (accept("char")) { ty = ty_char; continue; }
		if (accept("void")) { ty = ty_void; continue; }
		if (accept("struct")) { ty = struct_decl(); continue; }
		if (accept("enum")) { enum_decl(); ty = ty_int; continue; }
		if (!ty && t->kind == TK_IDENT) {
			Var *v = find_var(t->s);
			if (v && v->is_typedef) { pos++; ty = v->ty; continue; }
		}
		break;
	}
	if (!ty) die("type expected before '%s'", tk()->s);
	return ty;
}

static Type *type_suffix(Type *ty)
{
	if (accept("(")) {
		int n = 0;
		if (at("void") && toks.v[pos + 1].kind == TK_PUNCT && !strcmp(toks.v[pos + 1].s, ")")) pos++;
		while (!accept(")")) {
			if (n) expect(",");
			if (accept("...")) die("varargs are not supported");
			Type *b = declspec(NULL);
			char *nm = NULL;
			declarator(b, &nm);
			n++;
		}
		return func_of(ty, n);
	}
	if (accept("[")) {
		int len = -1;
		if (!at("]")) len = const_expr();
		expect("]");
		ty = type_suffix(ty);
		return array_of(ty, len < 0 ? 0 : len);
	}
	return ty;
}

/* the parameter names of the function declarator just parsed, for definitions */
static char *pnames[16];
static Type *ptypes[16];
static int npnames;

static Type *declarator(Type *ty, char **name)
{
	while (accept("*")) { while (accept("const") || accept("volatile")) {} ty = ptr_to(ty); }
	if (at("(") && !(toks.v[pos + 1].kind == TK_PUNCT && !strcmp(toks.v[pos + 1].s, ")")) &&
	    !(toks.v[pos + 1].kind == TK_IDENT && is_typename_at(pos + 1))) {
		/* nested declarator: int (*f)(int) */
		pos++;
		int start = pos;
		Type dummy = { 0 };
		char *ignore = NULL;
		declarator(&dummy, &ignore);
		expect(")");
		ty = type_suffix(ty);
		int end = pos;
		pos = start;
		Type *r = declarator(ty, name);
		expect(")");
		pos = end;
		return r;
	}
	if (tk()->kind == TK_IDENT) *name = ident_tok();
	if (at("(")) {
		/* remember parameter names for a possible definition */
		int save = pos;
		pos++;
		npnames = 0;
		if (at("void") && toks.v[pos + 1].kind == TK_PUNCT && !strcmp(toks.v[pos + 1].s, ")")) pos++;
		while (!accept(")")) {
			if (npnames) expect(",");
			Type *b = declspec(NULL);
			char *nm = NULL;
			Type *pt = declarator(b, &nm);
			if (npnames == 16) die("too many parameters");
			ptypes[npnames] = value_type(pt);
			pnames[npnames++] = nm;
		}
		pos = save;
	}
	return type_suffix(ty);
}

/* is the identifier at token index i a type name? (for "(" disambiguation) */
int is_typename_at(int i)
{
	int save = pos;
	pos = i;
	int r = is_typename();
	pos = save;
	return r;
}

/* ---------- expressions ---------- */
static Type *member_type(Type *st, const char *name, int *off)
{
	if (st->kind != TY_STRUCT) die("member access on a non-struct");
	if (!st->defined) die("incomplete struct");
	for (Member *m = st->members; m; m = m->next)
		if (m->name && !strcmp(m->name, name)) { *off = m->offset; return m->ty; }
	die("no member named %s", name);
	return NULL;
}

static Node *new_bin(int op, Node *l, Node *r);

static Node *new_deref(Node *p)
{
	Type *t = value_type(p->ty);
	if (t->kind != TY_PTR) die("dereference of a non-pointer");
	Node *n = mk(N_DEREF);
	n->l = p;
	n->ty = t->base;
	return n;
}

static Node *new_member(Node *s, const char *name)
{
	int off = 0;
	Type *mt = member_type(s->ty, name, &off);
	/* *(mt *)((char *)&s + off) */
	Node *a = mk(N_ADDR);
	a->l = s;
	a->ty = ptr_to(s->ty);
	Node *c = mk(N_CAST);
	c->l = a;
	c->ty = ptr_to(ty_char);
	Node *p = mk(N_CAST);
	p->l = off ? new_bin('+', c, num(off)) : c;
	p->ty = ptr_to(mt);
	Node *n = mk(N_DEREF);
	n->l = p;
	n->ty = mt;
	return n;
}

static Node *primary(void)
{
	Tok *t = tk();
	err_file = t->file;
	err_line = t->line;
	if (accept("(")) {
		Node *n = expr();
		expect(")");
		return n;
	}
	if (t->kind == TK_NUM) { pos++; return num(t->val); }
	if (t->kind == TK_STR) {
		/* adjacent strings concatenate */
		char buf[8192];
		int n = 0;
		while (tk()->kind == TK_STR) {
			if (n + tk()->len >= (int)sizeof buf) die("string too long");
			memcpy(buf + n, tk()->s, (size_t)tk()->len);
			n += tk()->len;
			pos++;
		}
		extern char *new_string(const char *s, int n);
		Node *s = mk(N_STR);
		s->name = new_string(buf, n);
		s->ty = array_of(ty_char, n + 1);
		return s;
	}
	if (t->kind != TK_IDENT) die("expression expected before '%s'", t->s);
	char *name = ident_tok();
	if (!strcmp(name, "sizeof")) {
		Type *ty;
		if (at("(") && is_typename_at(pos + 1)) {
			pos++;
			ty = declspec(NULL);
			char *nm = NULL;
			ty = declarator(ty, &nm);
			expect(")");
		} else {
			extern Node *unary(void);
			ty = unary()->ty;
		}
		return num(ty->size);
	}
	if (!strcmp(name, "__sys") || !strcmp(name, "__hook")) {
		Node *n = mk(name[2] == 's' ? N_SYS : N_HOOK);
		expect("(");
		if (n->kind == N_SYS) n->val = const_expr();
		else n->name = ident_tok();
		while (accept(",")) {
			if (n->nargs == 16) die("too many arguments");
			n->args[n->nargs++] = assign_expr();
		}
		expect(")");
		if (n->kind == N_SYS && n->nargs > 3) die("__sys takes at most three arguments");
		n->ty = ty_int;
		return n;
	}
	Var *v = find_var(name);
	if (at("(") && !v) {
		/* call of an undeclared function: assume int f() */
		Var *g = zalloc(sizeof *g);
		g->name = name;
		g->ty = func_of(ty_int, -1);
		g->next = globals;
		globals = g;
		v = g;
	}
	if (!v) die("undeclared identifier %s", name);
	if (v->is_enum) return num(v->value);
	Node *n = mk(N_VAR);
	n->var = v;
	n->ty = v->ty;
	return n;
}

static Node *postfix(void)
{
	Node *n = primary();
	for (;;) {
		err_line = tk()->line;
		if (accept("(")) {
			Node *c = mk(N_CALL);
			Type *ft = n->ty;
			if (ft->kind == TY_PTR && ft->base->kind == TY_FUNC) ft = ft->base;
			if (ft->kind != TY_FUNC) die("call of a non-function");
			c->l = n;
			while (!accept(")")) {
				if (c->nargs) expect(",");
				if (c->nargs == 16) die("too many arguments");
				c->args[c->nargs++] = assign_expr();
			}
			if (ft->nparams >= 0 && c->nargs != ft->nparams) die("wrong number of arguments: %d for %d", c->nargs, ft->nparams);
			c->ty = ft->base;
			n = c;
			continue;
		}
		if (accept("[")) {
			Node *i = expr();
			expect("]");
			n = new_deref(new_bin('+', n, i));
			continue;
		}
		if (accept(".")) { n = new_member(n, ident_tok()); continue; }
		if (accept("->")) { n = new_member(new_deref(n), ident_tok()); continue; }
		if (at("++") || at("--")) {
			Node *p = mk(N_POSTINC);
			int dir = at("++") ? 1 : -1;
			pos++;
			p->l = n;
			p->ty = value_type(n->ty);
			p->val = dir * (p->ty->kind == TY_PTR ? p->ty->base->size : 1);
			n = p;
			continue;
		}
		return n;
	}
}

static Node *cast_expr(void);

Node *unary(void)
{
	err_line = tk()->line;
	if (accept("+")) return cast_expr();
	if (accept("-")) { Node *n = mk(N_NEG); n->l = cast_expr(); n->ty = ty_int; return n; }
	if (accept("!")) { Node *n = mk(N_NOT); n->l = cast_expr(); n->ty = ty_int; return n; }
	if (accept("~")) { Node *n = mk(N_BNOT); n->l = cast_expr(); n->ty = ty_int; return n; }
	if (accept("*")) return new_deref(cast_expr());
	if (accept("&")) {
		Node *l = cast_expr();
		Node *n = mk(N_ADDR);
		n->l = l;
		n->ty = ptr_to(l->ty);
		return n;
	}
	if (at("++") || at("--")) {
		int dir = at("++") ? 1 : -1;
		pos++;
		Node *n = mk(N_PREINC);
		n->l = unary();
		n->ty = value_type(n->l->ty);
		n->val = dir * (n->ty->kind == TY_PTR ? n->ty->base->size : 1);
		return n;
	}
	return postfix();
}

static Node *cast_expr(void)
{
	if (at("(") && is_typename_at(pos + 1)) {
		pos++;
		Type *ty = declspec(NULL);
		char *nm = NULL;
		ty = declarator(ty, &nm);
		expect(")");
		Node *n = mk(N_CAST);
		n->l = cast_expr();
		n->ty = ty;
		return n;
	}
	return unary();
}

static Node *new_bin(int op, Node *l, Node *r)
{
	Type *lt = value_type(l->ty), *rt = value_type(r->ty);
	Node *n = mk(N_BIN);
	n->op = op;
	if (op == '+' && is_int(lt) && rt->kind == TY_PTR) { Node *t = l; l = r; r = t; Type *tt = lt; lt = rt; rt = tt; }
	n->l = l;
	n->r = r;
	if ((op == '+' || op == '-') && lt->kind == TY_PTR && is_int(rt)) n->ty = lt;
	else if (op == '-' && lt->kind == TY_PTR && rt->kind == TY_PTR) n->ty = ty_int;
	else n->ty = ty_int;
	return n;
}

static int prec(int *op)
{
	static const struct { const char *s; int op, prec; } t[] = {
		{ "*", '*', 10 }, { "/", '/', 10 }, { "%", '%', 10 }, { "+", '+', 9 }, { "-", '-', 9 },
		{ "<<", OP_SHL, 8 }, { ">>", OP_SHR, 8 }, { "<", '<', 7 }, { ">", '>', 7 }, { "<=", OP_LE, 7 },
		{ ">=", OP_GE, 7 }, { "==", OP_EQ, 6 }, { "!=", OP_NE, 6 }, { "&", '&', 5 }, { "^", '^', 4 },
		{ "|", '|', 3 }, { "&&", 'A', 2 }, { "||", 'O', 1 }, { NULL, 0, 0 }
	};
	Tok *k = tk();
	if (k->kind != TK_PUNCT) return -1;
	for (int i = 0; t[i].s; i++)
		if (!strcmp(k->s, t[i].s)) { *op = t[i].op; return t[i].prec; }
	return -1;
}

static Node *binary(int minp)
{
	Node *l = cast_expr();
	for (;;) {
		int op = 0, p = prec(&op);
		if (p < minp) return l;
		pos++;
		Node *r = binary(p + 1);
		if (op == 'A' || op == 'O') {
			Node *n = mk(op == 'A' ? N_AND : N_OR);
			n->l = l;
			n->r = r;
			n->ty = ty_int;
			l = n;
		} else {
			l = new_bin(op, l, r);
		}
	}
}

static Node *cond_expr(void)
{
	Node *c = binary(1);
	if (!accept("?")) return c;
	Node *n = mk(N_COND);
	n->c = c;
	n->l = expr();
	expect(":");
	n->r = cond_expr();
	n->ty = value_type(n->l->ty);
	return n;
}

static Node *assign_expr(void)
{
	Node *l = cond_expr();
	static const struct { const char *s; int op; } ops[] = {
		{ "+=", '+' }, { "-=", '-' }, { "*=", '*' }, { "/=", '/' }, { "%=", '%' }, { "&=", '&' },
		{ "|=", '|' }, { "^=", '^' }, { "<<=", OP_SHL }, { ">>=", OP_SHR }, { NULL, 0 }
	};
	if (accept("=")) {
		Node *n = mk(N_ASSIGN);
		n->l = l;
		n->r = assign_expr();
		n->ty = l->ty;
		if (l->ty->kind == TY_STRUCT || l->ty->kind == TY_ARRAY) die("cannot assign a struct or an array");
		return n;
	}
	for (int i = 0; ops[i].s; i++)
		if (accept(ops[i].s)) {
			Node *n = mk(N_OPASSIGN);
			n->op = ops[i].op;
			n->l = l;
			n->r = assign_expr();
			n->ty = value_type(l->ty);
			return n;
		}
	return l;
}

static Node *expr(void)
{
	Node *n = assign_expr();
	while (accept(",")) {
		Node *c = mk(N_COMMA);
		c->l = n;
		c->r = assign_expr();
		c->ty = c->r->ty;
		n = c;
	}
	return n;
}

static int const_eval(Node *n, int *ok)
{
	switch (n->kind) {
	case N_NUM: return n->val;
	case N_NEG: return -const_eval(n->l, ok);
	case N_NOT: return !const_eval(n->l, ok);
	case N_BNOT: return ~const_eval(n->l, ok);
	case N_CAST: return n->ty->kind == TY_CHAR ? const_eval(n->l, ok) & 255 : const_eval(n->l, ok);
	case N_COND: return const_eval(n->c, ok) ? const_eval(n->l, ok) : const_eval(n->r, ok);
	case N_AND: return const_eval(n->l, ok) && const_eval(n->r, ok);
	case N_OR: return const_eval(n->l, ok) || const_eval(n->r, ok);
	case N_BIN: {
		if (value_type(n->l->ty)->kind == TY_PTR) { *ok = 0; return 0; }
		int a = const_eval(n->l, ok), b = const_eval(n->r, ok);
		switch (n->op) {
		case '+': return a + b;
		case '-': return a - b;
		case '*': return a * b;
		case '/': return b ? a / b : 0;
		case '%': return b ? a % b : 0;
		case '&': return a & b;
		case '|': return a | b;
		case '^': return a ^ b;
		case OP_SHL: return (int)((unsigned)a << (b & 31));
		case OP_SHR: return a >> (b & 31);
		case '<': return a < b;
		case '>': return a > b;
		case OP_LE: return a <= b;
		case OP_GE: return a >= b;
		case OP_EQ: return a == b;
		case OP_NE: return a != b;
		}
	}
	}
	*ok = 0;
	return 0;
}

/* ---------- output ---------- */
static FILE *out;
static char *databuf, *strbuf;
static size_t datalen, datacap, strlen_, strcap;
static void appendf(char **buf, size_t *len, size_t *cap, const char *fmt, va_list ap)
{
	char tmp[8192];
	int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
	if (*len + (size_t)n + 1 > *cap) {
		*cap = (*cap + (size_t)n + 1) * 2;
		*buf = realloc(*buf, *cap);
		if (!*buf) die("out of memory");
	}
	memcpy(*buf + *len, tmp, (size_t)n + 1);
	*len += (size_t)n;
}
static void emit(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vfprintf(out, fmt, ap); fputc('\n', out); va_end(ap); }
static void data(const char *fmt, ...) { va_list ap; va_start(ap, fmt); appendf(&databuf, &datalen, &datacap, fmt, ap); va_end(ap); }
static void strs(const char *fmt, ...) { va_list ap; va_start(ap, fmt); appendf(&strbuf, &strlen_, &strcap, fmt, ap); va_end(ap); }

static int nstr, nlabel;
char *new_string(const char *s, int n)
{
	char name[32];
	snprintf(name, sizeof name, ".Ls%d", nstr++);
	strs("%s:", name);
	for (int i = 0; i <= n; i += 16) {
		strs("\t.byte ");
		for (int k = i; k < i + 16 && k <= n; k++)
			strs("%s%d", k > i ? ", " : "", k < n ? (unsigned char)s[k] : 0);
		strs("\n");
	}
	return dupn(name, (int)strlen(name));
}

/* ---------- code generation ---------- */
static const char *vlabel(Var *v) { return v->label ? v->label : v->name; }
static int ret_label, brk[64], cnt[64], nloop;
static Type *ret_type;   /* the function being generated returns this */
static int discard;   /* the expression being generated is a statement: its value is unused */

static void load_const(int reg, int v)
{
	if (v >= -32768 && v <= 32767) {
		emit("\tldi r%d, %d", reg, v);
	} else {
		emit("\tldi r%d, %d", reg, (int)(short)(v & 0xffff));
		emit("\tlui r%d, %d", reg, (int)((unsigned)v >> 16));
	}
}

static void load_label(int reg, const char *name)
{
	emit("\tldi r%d, %s", reg, name);
	emit("\tlui r%d, 0", reg);   /* addresses are 16-bit unsigned */
}

/* load from the address in r<a> into r<d>, by type */
static void load(Type *t, int d, int a)
{
	if (t->kind == TY_ARRAY || t->kind == TY_STRUCT || t->kind == TY_FUNC) {
		if (d != a) emit("\tmov r%d, r%d", d, a);
		return;
	}
	emit("\t%s r%d, [r%d]", t->kind == TY_CHAR ? "ldb" : "ldw", d, a);
}

static void store(Type *t, int v, int a)
{
	emit("\t%s r%d, [r%d]", t->kind == TY_CHAR ? "stb" : "stw", v, a);
}

static void gen_expr(Node *n);

/* local scalar variables can be addressed directly from fp */
static int is_local_scalar(Node *n) { return n->kind == N_VAR && n->var->local && is_scalar(n->ty); }

static void gen_addr(Node *n)
{
	switch (n->kind) {
	case N_VAR:
		if (n->var->local) emit("\taddi r0, fp, %d", n->var->offset);
		else load_label(0, vlabel(n->var));
		return;
	case N_DEREF:
		gen_expr(n->l);
		return;
	case N_STR:
		load_label(0, n->name);
		return;
	case N_COMMA:
		gen_expr(n->l);
		gen_addr(n->r);
		return;
	}
	die("line %d: not an lvalue", n->line);
}

/* simple operands are computed into any register without the stack */
static int simple(Node *n)
{
	if (n->kind == N_NUM) return 1;
	if (n->kind == N_VAR && !n->var->local && n->ty->kind != TY_STRUCT) return 1;
	if (n->kind == N_VAR && n->var->local) return 1;
	return 0;
}
static void gen_simple(Node *n, int reg)
{
	if (n->kind == N_NUM) { load_const(reg, n->val); return; }
	if (n->var->local) {
		Type *t = n->ty;
		if (t->kind == TY_ARRAY || t->kind == TY_STRUCT) emit("\taddi r%d, fp, %d", reg, n->var->offset);
		else emit("\t%s r%d, [fp%+d]", t->kind == TY_CHAR ? "ldb" : "ldw", reg, n->var->offset);
		return;
	}
	load_label(reg, vlabel(n->var));
	load(n->ty, reg, reg);
}

static int log2i(int v) { int k = 0; while ((1 << k) < v) k++; return (1 << k) == v ? k : -1; }

/* r0 = r0 op r1, with pointer scaling done by the caller */
static void binop(int op)
{
	const char *m = NULL;
	switch (op) {
	case '+': m = "add"; break;
	case '-': m = "sub"; break;
	case '*': m = "mul"; break;
	case '/': m = "div"; break;
	case '%': m = "mod"; break;
	case '&': m = "and"; break;
	case '|': m = "or"; break;
	case '^': m = "xor"; break;
	case OP_SHL: m = "shl"; break;
	case OP_SHR: m = "shr"; break;
	case '<': m = "slt"; break;
	case OP_LE: m = "sle"; break;
	case OP_EQ: m = "seq"; break;
	case OP_NE: m = "sne"; break;
	case '>': emit("\tslt r0, r1, r0"); return;
	case OP_GE: emit("\tsle r0, r1, r0"); return;
	}
	emit("\t%s r0, r0, r1", m);
}

static void scale(int reg, int size)
{
	if (size == 1) return;
	int k = log2i(size);
	if (k > 0) { emit("\tldi r5, %d", k); emit("\tshl r%d, r%d, r5", reg, reg); }
	else { load_const(5, size); emit("\tmul r%d, r%d, r5", reg, reg); }
}

static void gen_bin(Node *n)
{
	int ok = 1, v = const_eval(n, &ok);
	if (ok) { load_const(0, v); return; }
	Type *lt = value_type(n->l->ty), *rt = value_type(n->r->ty);
	int elem = (lt->kind == TY_PTR && is_int(rt) && (n->op == '+' || n->op == '-')) ? lt->base->size : 1;
	/* pointer + constant: one addi */
	int rok = 1, rv = const_eval(n->r, &rok);
	if (rok && (n->op == '+' || n->op == '-')) {
		long d = (long)rv * elem * (n->op == '-' ? -1 : 1);
		gen_expr(n->l);
		if (d >= -32768 && d <= 32767) { if (d) emit("\taddi r0, r0, %ld", d); }
		else { load_const(1, (int)d); emit("\tadd r0, r0, r1"); }
		return;
	}
	gen_expr(n->l);
	if (simple(n->r)) {
		gen_simple(n->r, 1);
	} else {
		emit("\tpush r0");
		gen_expr(n->r);
		emit("\tmov r1, r0");
		emit("\tpop r0");
	}
	scale(1, elem);
	binop(n->op);
	if (n->op == '-' && lt->kind == TY_PTR && rt->kind == TY_PTR && lt->base->size > 1) {
		load_const(1, lt->base->size);
		emit("\tdiv r0, r0, r1");
	}
}

static void gen_call(Node *n)
{
	for (int i = n->nargs - 1; i >= 0; i--) {
		gen_expr(n->args[i]);
		emit("\tpush r0");
	}
	if (n->kind == N_HOOK) {
		emit("hook_%s:", n->name);
		emit("\tcall %s_default", n->name);
	} else if (n->l->kind == N_VAR && !n->l->var->local && n->l->var->ty->kind == TY_FUNC) {
		emit("\tcall %s", vlabel(n->l->var));
	} else {
		gen_expr(n->l);
		emit("\tcallr r0");
	}
	if (n->nargs) emit("\taddi sp, sp, %d", 4 * n->nargs);
}

static void gen_cond_jump(Node *c, int target_if_false)
{
	gen_expr(c);
	emit("\tbz r0, .L%d", target_if_false);
}

static void gen_expr(Node *n)
{
	int unused = discard;
	discard = 0;
	err_line = n->line;
	switch (n->kind) {
	case N_NUM: load_const(0, n->val); return;
	case N_STR: load_label(0, n->name); return;
	case N_VAR:
		if (n->var->local) { gen_simple(n, 0); return; }
		if (n->ty->kind == TY_FUNC) { load_label(0, vlabel(n->var)); return; }
		gen_simple(n, 0);
		return;
	case N_ADDR:
		if (n->l->kind == N_VAR && n->l->ty->kind == TY_FUNC) { load_label(0, vlabel(n->l->var)); return; }
		gen_addr(n->l);
		return;
	case N_DEREF:
		gen_expr(n->l);
		load(n->ty, 0, 0);
		return;
	case N_CAST:
		gen_expr(n->l);
		if (n->ty->kind == TY_CHAR && n->l->ty->kind != TY_CHAR) { emit("\tldi r1, 255"); emit("\tand r0, r0, r1"); }
		return;
	case N_CALL:
	case N_HOOK:
		gen_call(n);
		return;
	case N_SYS:
		for (int i = n->nargs - 1; i >= 0; i--) { gen_expr(n->args[i]); emit("\tpush r0"); }
		for (int i = 0; i < n->nargs; i++) emit("\tpop r%d", i);
		emit("\tsys %d", n->val);
		return;
	case N_ASSIGN:
		if (is_local_scalar(n->l)) {
			gen_expr(n->r);
			emit("\t%s r0, [fp%+d]", n->l->ty->kind == TY_CHAR ? "stb" : "stw", n->l->var->offset);
		} else if (n->l->kind == N_VAR && !n->l->var->local) {
			gen_expr(n->r);
			load_label(1, vlabel(n->l->var));
			store(n->l->ty, 0, 1);
		} else if (simple(n->r)) {
			gen_addr(n->l);
			gen_simple(n->r, 1);
			store(n->l->ty, 1, 0);
			if (!unused) emit("\tmov r0, r1");
		} else {
			gen_addr(n->l);
			emit("\tpush r0");
			gen_expr(n->r);
			emit("\tpop r1");
			store(n->l->ty, 0, 1);
		}
		if (n->l->ty->kind == TY_CHAR && !unused) { emit("\tldi r1, 255"); emit("\tand r0, r0, r1"); }
		return;
	case N_OPASSIGN: {
		Type *lt = value_type(n->l->ty);
		int elem = (lt->kind == TY_PTR && (n->op == '+' || n->op == '-')) ? lt->base->size : 1;
		if (is_local_scalar(n->l) && simple(n->r)) {
			const char *ld = n->l->ty->kind == TY_CHAR ? "ldb" : "ldw", *st = n->l->ty->kind == TY_CHAR ? "stb" : "stw";
			emit("\t%s r0, [fp%+d]", ld, n->l->var->offset);
			long k = n->r->kind == N_NUM ? (long)n->r->val * elem * (n->op == '-' ? -1 : 1) : 1L << 20;
			if ((n->op == '+' || n->op == '-') && k >= -32768 && k <= 32767) {
				emit("\taddi r0, r0, %ld", k);
			} else {
				gen_simple(n->r, 1);
				scale(1, elem);
				binop(n->op);
			}
			emit("\t%s r0, [fp%+d]", st, n->l->var->offset);
			if (n->l->ty->kind == TY_CHAR && !unused) { emit("\tldi r1, 255"); emit("\tand r0, r0, r1"); }
			return;
		}
		gen_addr(n->l);
		emit("\tpush r0");
		gen_expr(n->r);
		emit("\tmov r1, r0");
		scale(1, elem);
		emit("\tldw r2, [r7]");          /* the address, still on the stack */
		load(n->l->ty, 0, 2);
		binop(n->op);
		emit("\tpop r1");
		store(n->l->ty, 0, 1);
		if (n->l->ty->kind == TY_CHAR) { emit("\tldi r1, 255"); emit("\tand r0, r0, r1"); }
		return;
	}
	case N_PREINC:
	case N_POSTINC:
		if (is_local_scalar(n->l)) {
			const char *ld = n->l->ty->kind == TY_CHAR ? "ldb" : "ldw", *st = n->l->ty->kind == TY_CHAR ? "stb" : "stw";
			emit("\t%s r0, [fp%+d]", ld, n->l->var->offset);
			emit("\taddi r1, r0, %d", n->val);
			emit("\t%s r1, [fp%+d]", st, n->l->var->offset);
		} else {
			gen_addr(n->l);
			emit("\tmov r2, r0");
			load(n->l->ty, 0, 2);
			emit("\taddi r1, r0, %d", n->val);
			store(n->l->ty, 1, 2);
		}
		if (n->kind == N_PREINC && !unused) {
			emit("\tmov r0, r1");
			if (n->l->ty->kind == TY_CHAR) { emit("\tldi r1, 255"); emit("\tand r0, r0, r1"); }
		}
		return;
	case N_BIN: gen_bin(n); return;
	case N_AND:
	case N_OR: {
		int hit = nlabel++, end = nlabel++;
		const char *br = n->kind == N_AND ? "bz" : "bnz";
		gen_expr(n->l);
		emit("\t%s r0, .L%d", br, hit);
		gen_expr(n->r);
		emit("\t%s r0, .L%d", br, hit);
		emit("\tldi r0, %d", n->kind == N_AND);
		emit("\tjmp .L%d", end);
		emit(".L%d:", hit);
		emit("\tldi r0, %d", n->kind != N_AND);
		emit(".L%d:", end);
		return;
	}
	case N_COND: {
		int els = nlabel++, end = nlabel++;
		gen_cond_jump(n->c, els);
		gen_expr(n->l);
		emit("\tjmp .L%d", end);
		emit(".L%d:", els);
		gen_expr(n->r);
		emit(".L%d:", end);
		return;
	}
	case N_NEG: gen_expr(n->l); emit("\tldi r1, 0"); emit("\tsub r0, r1, r0"); return;
	case N_NOT: gen_expr(n->l); emit("\tldi r1, 0"); emit("\tseq r0, r0, r1"); return;
	case N_BNOT: gen_expr(n->l); emit("\tldi r1, -1"); emit("\txor r0, r0, r1"); return;
	case N_COMMA: gen_expr(n->l); gen_expr(n->r); return;
	}
	die("cannot generate expression kind %d", n->kind);
}

static void gen_stmt(Node *n)
{
	for (; n; n = n->next) {
		err_line = n->line;
		/* where each statement comes from: the simulator maps pc back to the C */
		if (n->kind != N_BLOCK && n->file) emit("; @%s:%d", n->file, n->line);
		switch (n->kind) {
		case N_EXPR: discard = 1; gen_expr(n->l); discard = 0; break;
		case N_BLOCK: gen_stmt(n->body); break;
		case N_ASM: {
			const char *p = n->name;
			while (*p) {
				const char *e = strchr(p, '\n');
				int len = e ? (int)(e - p) : (int)strlen(p);
				fprintf(out, "%.*s\n", len, p);
				p += len + (e != NULL);
			}
			break;
		}
		case N_IF: {
			int els = nlabel++, end = nlabel++;
			gen_cond_jump(n->c, els);
			gen_stmt(n->body);
			if (n->els) emit("\tjmp .L%d", end);
			emit(".L%d:", els);
			if (n->els) { gen_stmt(n->els); emit(".L%d:", end); }
			break;
		}
		case N_WHILE:
		case N_FOR: {
			int top = nlabel++, c = nlabel++, b = nlabel++;
			if (n->init) gen_stmt(n->init);
			emit(".L%d:", top);
			if (n->c) gen_cond_jump(n->c, b);
			if (nloop == 64) die("loops nested too deep");
			brk[nloop] = b; cnt[nloop] = c; nloop++;
			gen_stmt(n->body);
			nloop--;
			emit(".L%d:", c);
			if (n->inc) gen_expr(n->inc);
			emit("\tjmp .L%d", top);
			emit(".L%d:", b);
			break;
		}
		case N_DO: {
			int top = nlabel++, c = nlabel++, b = nlabel++;
			emit(".L%d:", top);
			brk[nloop] = b; cnt[nloop] = c; nloop++;
			gen_stmt(n->body);
			nloop--;
			emit(".L%d:", c);
			gen_expr(n->c);
			emit("\tbnz r0, .L%d", top);
			emit(".L%d:", b);
			break;
		}
		case N_RETURN:
			if (n->l) {
				gen_expr(n->l);
				/* a char function returns a char: what an assignment to one would keep */
				if (ret_type && ret_type->kind == TY_CHAR && n->l->ty->kind != TY_CHAR) { emit("\tldi r1, 255"); emit("\tand r0, r0, r1"); }
			}
			emit("\tjmp .L%d", ret_label);
			break;
		case N_BREAK:
		case N_CONTINUE:
			if (!nloop) die("%s outside a loop", n->kind == N_BREAK ? "break" : "continue");
			emit("\tjmp .L%d", n->kind == N_BREAK ? brk[nloop - 1] : cnt[nloop - 1]);
			break;
		default:
			die("cannot generate statement kind %d", n->kind);
		}
	}
}

/* ---------- statements and declarations ---------- */
static int frame_size, nstatic;
static void global_var(char *name, Type *ty, Attr *a);

static Var *new_local(char *name, Type *ty)
{
	Var *v = zalloc(sizeof *v);
	v->name = name;
	v->ty = ty;
	v->local = 1;
	v->depth = scope_depth;
	int al = ty->align < 4 ? 4 : ty->align;
	frame_size = (frame_size + ty->size + al - 1) / al * al;
	v->offset = -frame_size;
	v->next = locals;
	locals = v;
	return v;
}

static Node *stmt(void);

static Node *local_decl(void)
{
	Attr a = { 0 };
	Type *base = declspec(&a);
	Node head = { 0 }, *cur = &head;
	if (accept(";")) return mk(N_BLOCK);
	do {
		char *name = NULL;
		Type *ty = declarator(base, &name);
		if (!name) die("variable name expected");
		if (a.is_typedef) {
			Var *v = zalloc(sizeof *v);
			v->name = name; v->ty = ty; v->is_typedef = 1; v->depth = scope_depth;
			v->next = locals; locals = v;
			continue;
		}
		if (a.is_static || a.is_extern || ty->kind == TY_FUNC) {
			/* storage outside the frame: a global, visible only in this block */
			Var *alias = zalloc(sizeof *alias);
			if (a.is_static && ty->kind != TY_FUNC) {
				char buf[128];
				snprintf(buf, sizeof buf, "__static%d_%s", nstatic++, name);
				char *label = dupn(buf, (int)strlen(buf));
				global_var(label, ty, &a);
				alias->ty = ty;
				alias->label = label;
			} else {
				Var *old = NULL;
				for (Var *v = globals; v; v = v->next) if (!strcmp(v->name, name)) old = v;
				if (!old) { old = zalloc(sizeof *old); old->name = name; old->ty = ty; old->next = globals; globals = old; }
				alias->ty = old->ty;
			}
			alias->name = name;
			alias->depth = scope_depth;
			alias->next = locals;
			locals = alias;
			continue;
		}
		Var *v = new_local(name, ty);
		if (accept("=")) {
			if (!is_scalar(ty)) die("only scalar locals can have initialisers");
			Node *var = mk(N_VAR);
			var->var = v;
			var->ty = ty;
			Node *as = mk(N_ASSIGN);
			as->l = var;
			as->r = assign_expr();
			as->ty = ty;
			Node *e = mk(N_EXPR);
			e->l = as;
			cur = cur->next = e;
		}
	} while (accept(","));
	expect(";");
	Node *b = mk(N_BLOCK);
	b->body = head.next;
	return b;
}

static Node *block(void)
{
	Node head = { 0 }, *cur = &head;
	Var *save = locals;
	scope_depth++;
	while (!accept("}")) {
		if (tk()->kind == TK_EOF) die("'}' expected at end of file");
		Node *s = is_typename() ? local_decl() : stmt();
		cur = cur->next = s;
	}
	scope_depth--;
	locals = save;
	Node *b = mk(N_BLOCK);
	b->body = head.next;
	return b;
}

static Node *stmt(void)
{
	err_file = tk()->file;
	err_line = tk()->line;
	if (accept("{")) return block();
	if (accept(";")) return mk(N_BLOCK);
	if (accept("if")) {
		Node *n = mk(N_IF);
		expect("(");
		n->c = expr();
		expect(")");
		n->body = stmt();
		if (accept("else")) n->els = stmt();
		return n;
	}
	if (accept("while")) {
		Node *n = mk(N_WHILE);
		expect("(");
		n->c = expr();
		expect(")");
		n->body = stmt();
		return n;
	}
	if (accept("do")) {
		Node *n = mk(N_DO);
		n->body = stmt();
		expect("while");
		expect("(");
		n->c = expr();
		expect(")");
		expect(";");
		return n;
	}
	if (accept("for")) {
		Node *n = mk(N_FOR);
		Var *save = locals;
		scope_depth++;
		expect("(");
		if (is_typename()) n->init = local_decl();
		else if (!accept(";")) { n->init = mk(N_EXPR); n->init->l = expr(); expect(";"); }
		if (!at(";")) n->c = expr();
		expect(";");
		if (!at(")")) n->inc = expr();
		expect(")");
		n->body = stmt();
		scope_depth--;
		locals = save;
		return n;
	}
	if (accept("return")) {
		Node *n = mk(N_RETURN);
		if (!accept(";")) { n->l = expr(); expect(";"); }
		return n;
	}
	if (accept("break")) { expect(";"); return mk(N_BREAK); }
	if (accept("continue")) { expect(";"); return mk(N_CONTINUE); }
	if (accept("asm")) {
		Node *n = mk(N_ASM);
		expect("(");
		char buf[8192];
		int len = 0;
		while (tk()->kind == TK_STR) {
			if (len + tk()->len >= (int)sizeof buf) die("asm text too long");
			memcpy(buf + len, tk()->s, (size_t)tk()->len);
			len += tk()->len;
			pos++;
		}
		n->name = dupn(buf, len);
		expect(")");
		expect(";");
		return n;
	}
	Node *n = mk(N_EXPR);
	n->l = expr();
	expect(";");
	return n;
}

/* ---------- globals and their initialisers ---------- */
static int init_off;    /* bytes emitted so far for the current initialiser */

static void data_zero(int n) { if (n > 0) { data("\t.zero %d\n", n); init_off += n; } }

static const char *addr_eval(Node *e, int *add);
static const char *lvalue_eval(Node *e, int *add)
{
	if (e->kind == N_VAR && !e->var->local) return vlabel(e->var);
	if (e->kind == N_DEREF) return addr_eval(e->l, add);
	return NULL;
}
static const char *addr_eval(Node *e, int *add)
{
	switch (e->kind) {
	case N_STR: return e->name;
	case N_CAST: return addr_eval(e->l, add);
	case N_ADDR: return lvalue_eval(e->l, add);
	case N_VAR:
		if (!e->var->local && (e->ty->kind == TY_FUNC || e->ty->kind == TY_ARRAY)) return vlabel(e->var);
		return NULL;
	case N_BIN: {
		int ok = 1, k = const_eval(e->r, &ok);
		Type *lt = value_type(e->l->ty);
		if (!ok || lt->kind != TY_PTR || (e->op != '+' && e->op != '-')) return NULL;
		*add += k * lt->base->size * (e->op == '-' ? -1 : 1);
		return addr_eval(e->l, add);
	}
	}
	return NULL;
}

static void init_scalar(Type *ty)
{
	int braces = accept("{");
	Node *e = assign_expr();
	if (braces) { accept(","); expect("}"); }
	int ok = 1, v = const_eval(e, &ok);
	if (ok) {
		if (ty->kind == TY_CHAR) data("\t.byte %d\n", v & 255);
		else data("\t.word %d\n", v);
		init_off += ty->size;
		return;
	}
	if (ty->kind == TY_CHAR) die("char initialiser must be constant");
	/* an address: "str", &global, &a[3], function, array, plus a constant */
	int add = 0;
	const char *label = addr_eval(e, &add);
	if (!label) die("initialiser must be constant");
	if (add) data("\t.word %s %c %d\n", label, add < 0 ? '-' : '+', add < 0 ? -add : add);
	else data("\t.word %s\n", label);
	init_off += 4;
}

static void init_value(Type *ty)
{
	if (ty->kind == TY_ARRAY) {
		int start = init_off;
		if (ty->base->kind == TY_CHAR && tk()->kind == TK_STR) {
			char buf[8192];
			int n = 0;
			while (tk()->kind == TK_STR) {     /* adjacent strings concatenate */
				if (n + tk()->len >= (int)sizeof buf) die("string too long");
				memcpy(buf + n, tk()->s, (size_t)tk()->len);
				n += tk()->len;
				pos++;
			}
			if (!ty->len) { ty->len = n + 1; ty->size = ty->len; }
			for (int i = 0; i < ty->len; i += 16) {
				data("\t.byte ");
				for (int k = i; k < i + 16 && k < ty->len; k++) data("%s%d", k > i ? ", " : "", k < n ? (unsigned char)buf[k] : 0);
				data("\n");
			}
			init_off += ty->len;
			return;
		}
		expect("{");
		int n = 0;
		while (!accept("}")) {
			if (ty->len && n >= ty->len) die("too many initialisers");
			init_value(ty->base);
			n++;
			if (!accept(",")) { expect("}"); break; }
		}
		if (!ty->len) { ty->len = n; ty->size = n * ty->base->size; }
		data_zero(start + ty->size - init_off);
		return;
	}
	if (ty->kind == TY_STRUCT) {
		int start = init_off;
		expect("{");
		Member *m = ty->members;
		while (!accept("}")) {
			if (!m) die("too many initialisers");
			data_zero(start + m->offset - init_off);
			init_value(m->ty);
			m = m->next;
			if (!accept(",")) { expect("}"); break; }
		}
		data_zero(start + ty->size - init_off);
		return;
	}
	init_scalar(ty);
}

static void global_var(char *name, Type *ty, Attr *a)
{
	Var *v = NULL;
	for (Var *g = globals; g; g = g->next) if (!strcmp(g->name, name)) v = g;
	if (!v) { v = zalloc(sizeof *v); v->name = name; v->next = globals; globals = v; }
	v->ty = ty;
	if (a->is_extern && !at("=")) return;
	if (v->defined && !at("=")) return;
	if (v->defined) die("%s is defined twice", name);
	v->defined = 1;
	if (ty->align >= 4) data("\t.align 4\n");
	data("%s:\n", name);
	init_off = 0;
	if (accept("=")) init_value(ty);
	else data_zero(ty->size);
	if (ty->size == 0) die("%s has no size", name);
}

static void function(char *name, Type *ty)
{
	Var *g = NULL;
	for (Var *v = globals; v; v = v->next) if (!strcmp(v->name, name)) g = v;
	if (!g) { g = zalloc(sizeof *g); g->name = name; g->next = globals; globals = g; }
	g->ty = ty;
	if (g->defined) die("function %s is defined twice", name);
	g->defined = 1;
	locals = NULL;
	frame_size = 0;
	int np = npnames;
	for (int i = 0; i < np; i++) {
		Var *p = zalloc(sizeof *p);
		p->name = pnames[i] ? pnames[i] : "";
		p->ty = ptypes[i];
		p->local = 1;
		p->offset = 8 + 4 * i;
		p->next = locals;
		locals = p;
	}
	expect("{");
	scope_depth = 1;
	Node *body = block();
	scope_depth = 0;
	ret_label = nlabel++;
	ret_type = ty->base;
	emit("%s:", name);
	emit("\tpush fp");
	emit("\tmov fp, sp");
	if (frame_size) emit("\taddi sp, sp, %d", -frame_size);
	gen_stmt(body);
	emit(".L%d:", ret_label);
	emit("\tmov sp, fp");
	emit("\tpop fp");
	emit("\tret");
	locals = NULL;
}

static void program(void)
{
	while (tk()->kind != TK_EOF) {
		err_file = tk()->file;
		err_line = tk()->line;
		if (at("asm")) {
			Node *n = stmt();
			gen_stmt(n);
			continue;
		}
		Attr a = { 0 };
		Type *base = declspec(&a);
		if (accept(";")) continue;
		for (;;) {
			char *name = NULL;
			Type *ty = declarator(base, &name);
			if (!name) die("name expected");
			if (a.is_typedef) {
				Var *v = zalloc(sizeof *v);
				v->name = name; v->ty = ty; v->is_typedef = 1;
				v->next = globals; globals = v;
			} else if (ty->kind == TY_FUNC) {
				if (at("{")) { function(name, ty); break; }
				Var *g = NULL;
				for (Var *v = globals; v; v = v->next) if (!strcmp(v->name, name)) g = v;
				if (!g) { g = zalloc(sizeof *g); g->name = name; g->next = globals; globals = g; }
				g->ty = ty;
			} else {
				global_var(name, ty, &a);
			}
			if (accept(",")) continue;
			expect(";");
			break;
		}
	}
}

int main(int argc, char **argv)
{
	const char *outpath = NULL;
	const char *files[64];
	int nfiles = 0;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-o") && i + 1 < argc) outpath = argv[++i];
		else if (!strcmp(argv[i], "-I") && i + 1 < argc && nincdirs < 16) incdirs[nincdirs++] = argv[++i];
		else if (nfiles < 64) files[nfiles++] = argv[i];
	}
	if (!nfiles) { fprintf(stderr, "usage: cc [-I dir] [-o out.s] file.c ...\n"); return 2; }
	for (int i = 0; i < nfiles; i++) preprocess(files[i], &toks, 0);
	Tok eof = { 0 };
	eof.kind = TK_EOF;
	eof.s = "";
	tv_push(&toks, eof);
	out = outpath ? fopen(outpath, "w") : stdout;
	if (!out) { perror(outpath); return 1; }
	emit("; generated by the kit's cc");
	program();
	emit("; string literals");
	if (strbuf) fputs(strbuf, out);
	emit("\t.align 4");
	emit("__data_start:");
	if (databuf) fputs(databuf, out);
	emit("\t.align 4");
	emit("__data_end:");
	if (outpath) fclose(out);
	return 0;
}
