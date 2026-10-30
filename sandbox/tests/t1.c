/* a compiler test: prints results, exits with the number of failures */
#define PUTC(c) __sys(6, c)
#define CHECK(x) check(x, __LINE__)
asm(".entry start");
asm("start:\n\tcall main\n\tsys 0");

int fails;
void putn(int n)
{
	if (n < 0) { PUTC('-'); n = -n; }
	if (n >= 10) putn(n / 10);
	PUTC('0' + n % 10);
}
void puts_(char *s) { while (*s) PUTC(*s++); }
void check(int ok, int line) { if (!ok) { puts_("FAIL line "); putn(line); PUTC('\n'); fails++; } }

struct Pt { int x; char tag; int y; };
typedef struct Pt Pt;
enum { RED, GREEN = 5, BLUE };
int table[5] = { 1, 2, 3 };
char name[] = "doom";
char *msg = "hi";
Pt origin = { 3, 'o', 4 };
int *tp = &table[0] + 2;
int big = 0x12345678;
static int counter;

int add(int a, int b) { return a + b; }
int twice(int (*f)(int, int), int v) { return f(v, v); }
int fact(int n) { return n <= 1 ? 1 : n * fact(n - 1); }
int count(void) { static int k; return ++k; }
int sum(int *p, int n) { int s = 0; for (int i = 0; i < n; i++) s += p[i]; return s; }

int main()
{
	int a = 7, b = -3;
	char c = 250;
	int arr[4];
	Pt p;
	Pt *pp = &p;
	CHECK(a + b == 4);
	CHECK(a * b == -21);
	CHECK(a / b == -2 && a % b == 1);
	CHECK((a & 3) == 3 && (a | 8) == 15 && (a ^ 1) == 6);
	CHECK((1 << 10) == 1024 && (-16 >> 2) == -4);
	CHECK(a > b && b < a && a >= 7 && a <= 7 && a != b && !(a == b));
	CHECK(c == 250);
	c += 10;
	CHECK(c == 4);
	CHECK(big == 305419896 && big >> 16 == 0x1234);
	CHECK(table[0] + table[1] + table[2] + table[3] + table[4] == 6);
	CHECK(*tp == 3);
	CHECK(sizeof(Pt) == 12 && sizeof(table) == 20 && sizeof(name) == 5 && sizeof p.tag == 1);
	CHECK(name[0] == 'd' && name[4] == 0 && msg[1] == 'i');
	CHECK(origin.x == 3 && origin.tag == 'o' && origin.y == 4);
	CHECK(RED == 0 && GREEN == 5 && BLUE == 6);
	p.x = 10; pp->y = 20; pp->tag = 'q';
	CHECK(p.y == 20 && pp->x == 10 && p.tag == 'q');
	for (int i = 0; i < 4; i++) arr[i] = i * i;
	CHECK(sum(arr, 4) == 14);
	int *q = arr;
	q++;
	CHECK(*q == 1 && q[2] == 9 && q - arr == 1 && *(arr + 3) == 9);
	CHECK(add(2, 3) == 5 && twice(add, 21) == 42);
	int (*f)(int, int) = add;
	CHECK(f(1, 1) == 2);
	CHECK(fact(6) == 720);
	CHECK(count() == 1 && count() == 2);
	int n = 0, i = 0;
	while (1) { i++; if (i > 10) break; if (i % 2) continue; n += i; }
	CHECK(n == 30);
	do n--; while (n > 25);
	CHECK(n == 25);
	CHECK((0 || 5) == 1 && (3 && 0) == 0 && (2 && 3) == 1);
	CHECK(-a == -7 && ~0 == -1 && !0 == 1);
	int x = 5;
	CHECK(x++ == 5 && x == 6 && ++x == 7 && x-- == 7 && --x == 5);
	x <<= 2; CHECK(x == 20);
	x -= 30; CHECK(x == -10);
	counter = 3; counter *= counter; CHECK(counter == 9);
	char *s = name;
	CHECK(*s++ == 'd' && *s == 'o');
	CHECK((char)300 == 44);
	puts_("t1 done\n");
	return fails;
}
