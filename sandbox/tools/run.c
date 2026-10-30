/* run.c - boot a program on the D32 machine and watch it.
 *
 *   run [-d disk] [-g game.o] [-f frames] [-c max_cycles] [-a] [-e] [-p out.ppm]
 *       [-y frame]... [-A args] [-P sym] [-v copy]
 *
 * -a prints each presented frame as text; -p writes the last one as a PPM image;
 * -y N presses the y key during the Nth presented frame; -A a,b,c sets the boot
 * arguments (SYS_ARG); -e exits with the program's r0 (for tests); -P game.sym
 * prints where the cycles went, by function; -v N shows only copy N's frames (copies
 * are told apart by their stacks, from SYS_COPY; 0 is the boot copy).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"

/* 16 colours, Doom-ish: index 0 is black. */
static const uint8_t pal[16][3] = {
	{0, 0, 0}, {40, 28, 20}, {90, 60, 40}, {140, 100, 70}, {224, 85, 60}, {127, 210, 62},
	{108, 156, 240}, {241, 178, 60}, {234, 220, 203}, {255, 255, 255}, {60, 40, 30},
	{30, 60, 20}, {25, 35, 70}, {120, 30, 20}, {200, 200, 200}, {255, 210, 122}
};
static const char *shade = " .:-=+*#%@$&OX8M";

static int frames, max_frames = 1, ascii, exit_r0, ypress[64], nyp;
static const char *ppm;
static uint16_t last_fb;
static int nstk, stk_lo[16], stk_hi[16], view = -1;

/* which copy is running: the one whose stack holds sp (0: the boot copy) */
static int copy_of(Vm *vm)
{
	for (int i = 0; i < nstk; i++)
		if ((int)(vm->r[D32_SP] & 0xffff) >= stk_lo[i] && (int)(vm->r[D32_SP] & 0xffff) <= stk_hi[i]) return i + 1;
	return 0;
}
static int32_t args[8];
static uint64_t prof[65536];
static const char *profsym;

static void dump_ascii(Vm *vm, uint16_t fb)
{
	for (int y = 0; y < FB_H; y++) {
		for (int x = 0; x < FB_W; x++)
			putchar(shade[vm->mem[(uint16_t)(fb + y * FB_W + x)] & 15]);
		putchar('\n');
	}
}

static void on_sys(Vm *vm, int n, void *user)
{
	(void)user;
	if (n == SYS_COPY && nstk < 16) {
		stk_lo[nstk] = (int)(vm->r[1] & 0xffff);
		stk_hi[nstk] = (int)(vm->r[2] & 0xffff);
		nstk++;
	}
	if (n == SYS_PRESENT) {
		int c = copy_of(vm);
		frames++;
		if (view < 0 || c == view) last_fb = (uint16_t)vm->r[0];
		if (ascii && (view < 0 || c == view)) { printf("--- frame %d, copy %d ---\n", frames, c); dump_ascii(vm, (uint16_t)vm->r[0]); }
		vm->keys = 0;
		for (int i = 0; i < nyp; i++)
			if (ypress[i] == frames + 1) vm->keys = 1;
		if (frames >= max_frames) vm->halted = 1;
	}
}

static void write_ppm(Vm *vm, uint16_t fb, const char *path)
{
	FILE *f = fopen(path, "wb");
	if (!f) { perror(path); return; }
	fprintf(f, "P6\n%d %d\n255\n", FB_W * 4, FB_H * 4);
	for (int y = 0; y < FB_H * 4; y++)
		for (int x = 0; x < FB_W * 4; x++)
			fwrite(pal[vm->mem[(uint16_t)(fb + (y / 4) * FB_W + x / 4)] & 15], 1, 3, f);
	fclose(f);
}

/* cycles by function: labels from the symbol file, offsets from VM_GAME_BASE;
   labels starting with '.' or "hook_" do not start a function */
static void profile(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f) { perror(path); return; }
	static char names[4096][64];
	static int addr[4096];
	int n = 0;
	unsigned a;
	while (n < 4096 && fscanf(f, "%63s %x", names[n], &a) == 2)
		if (names[n][0] != '.' && strncmp(names[n], "hook_", 5)) addr[n++] = (int)(a + VM_GAME_BASE);
	fclose(f);
	uint64_t total = 0, by[4096] = { 0 }, other = 0;
	for (int pc = 0; pc < 65536; pc++) {
		if (!prof[pc]) continue;
		total += prof[pc];
		int best = -1;
		for (int i = 0; i < n; i++)
			if (addr[i] <= pc && (best < 0 || addr[i] > addr[best])) best = i;
		if (best < 0 || pc >= (int)VM_HEAP_START) other += prof[pc]; else by[best] += prof[pc];
	}
	for (;;) {
		int best = -1;
		for (int i = 0; i < n; i++) if (by[i] && (best < 0 || by[i] > by[best])) best = i;
		if (best < 0) break;
		fprintf(stderr, "%10llu %5.1f%%  %s\n", (unsigned long long)by[best], 100.0 * by[best] / total, names[best]);
		by[best] = 0;
	}
	if (other) fprintf(stderr, "%10llu %5.1f%%  (outside the game)\n", (unsigned long long)other, 100.0 * other / total);
}

int main(int argc, char **argv)
{
	const char *disk = ".", *game = "game.o";
	uint64_t max_cycles = 200000000ull;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d") && i + 1 < argc) disk = argv[++i];
		else if (!strcmp(argv[i], "-g") && i + 1 < argc) game = argv[++i];
		else if (!strcmp(argv[i], "-f") && i + 1 < argc) max_frames = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-c") && i + 1 < argc) max_cycles = strtoull(argv[++i], 0, 0);
		else if (!strcmp(argv[i], "-a")) ascii = 1;
		else if (!strcmp(argv[i], "-e")) exit_r0 = 1;
		else if (!strcmp(argv[i], "-v") && i + 1 < argc) view = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-P") && i + 1 < argc) profsym = argv[++i];
		else if (!strcmp(argv[i], "-A") && i + 1 < argc) {
			char *s = argv[++i];
			for (int k = 0; k < 8 && *s; k++) { args[k] = (int)strtol(s, &s, 0); if (*s == ',') s++; }
		}
		else if (!strcmp(argv[i], "-p") && i + 1 < argc) ppm = argv[++i];
		else if (!strcmp(argv[i], "-y") && i + 1 < argc && nyp < 64) ypress[nyp++] = atoi(argv[++i]);
		else { fprintf(stderr, "run: unknown option %s\n", argv[i]); return 2; }
	}
	static Vm vm;
	vm_init(&vm, disk);
	vm.on_sys = on_sys;
	memcpy(vm.args, args, sizeof args);
	if (vm_boot(&vm, game) < 0) { fprintf(stderr, "run: cannot load %s/%s\n", disk, game); return 1; }
	if (!profsym) {
		while (vm_step(&vm) && vm.cycles < max_cycles)
			;
	} else {
		for (;;) {
			uint16_t at = vm.pc;
			uint64_t c = vm.cycles;
			int go = vm_step(&vm);
			prof[at] += vm.cycles - c;
			if (!go || vm.cycles >= max_cycles) break;
		}
		profile(profsym);
	}
	if (vm.halted == 2) fprintf(stderr, "run: fault: %s\n", vm.fault);
	if (ppm && frames) write_ppm(&vm, last_fb, ppm);
	fprintf(stderr, "run: %d frames, %llu cycles, r0=%d, pc=%04x\n", frames, (unsigned long long)vm.cycles,
	        (int)vm.r[0], vm.pc);
	if (vm.halted == 2) return 2;
	return exit_r0 ? (int)(vm.r[0] & 255) : 0;
}
