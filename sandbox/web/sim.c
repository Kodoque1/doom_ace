/* sim.c - the D32 machine for the browser page: tools/vm.c, wrapped so JavaScript can
 * step it, save and restore it whole (for rewinding), and see what happens.
 *
 * Everything the machine is lives in one struct, `S`, so a checkpoint is one copy of
 * it. Events (a copy announced, a frame presented, a file loaded) are logged with the
 * instruction count they happened at; the page builds its timeline from them.
 */
#include <stdio.h>
#include <string.h>
#include "../tools/vm.h"

#define EXPORT(n) __attribute__((export_name(n)))

enum { EV_COPY = 1, EV_PRESENT = 2, EV_LOAD = 4, EV_EXIT = 8, EV_FAULT = 16 };
typedef struct { uint32_t kind, a, b, c, sp, lo, hi; } Event;   /* lo, hi: the step */

static struct {
	Vm vm;
	uint64_t steps;
} S;

static Event ev[1024];
static int nev;
static uint16_t writes[256];
static int nwrites;
static int32_t args[8];

static void log_ev(uint32_t kind, uint32_t a, uint32_t b, uint32_t c)
{
	if (nev == 1024) return;
	ev[nev].kind = kind;
	ev[nev].a = a;
	ev[nev].b = b;
	ev[nev].c = c;
	ev[nev].sp = S.vm.r[D32_SP] & 0xffff;
	ev[nev].lo = (uint32_t)S.steps;
	ev[nev].hi = (uint32_t)(S.steps >> 32);
	nev++;
}

static void on_sys(Vm *vm, int n, void *user)
{
	(void)user;
	if (n == SYS_COPY) log_ev(EV_COPY, vm->r[0], vm->r[1] & 0xffff, vm->r[2] & 0xffff);
	if (n == SYS_PRESENT) log_ev(EV_PRESENT, vm->r[0] & 0xffff, 0, 0);
	if (n == SYS_LOAD) log_ev(EV_LOAD, vm->r[1] & 0xffff, vm->r[0], 0);
	if (n == SYS_EXIT) log_ev(EV_EXIT, 0, 0, 0);
}

static void on_write(Vm *vm, uint16_t a, int bytes, void *user)
{
	(void)vm;
	(void)bytes;
	(void)user;
	writes[nwrites++ & 255] = a;
}

EXPORT("sim_args") int32_t *sim_args(void) { return args; }

/* boot game.o from the directory "disk", with the boot arguments in sim_args() */
EXPORT("sim_boot") int sim_boot(void)
{
	vm_init(&S.vm, "disk");
	S.steps = 0;
	memcpy(S.vm.args, args, sizeof args);
	if (vm_boot(&S.vm, "game.o") < 0) return -1;
	return 0;
}

/* Run up to n instructions, stopping after any event whose kind is in stop_mask.
   Returns the number of instructions run. Callbacks are set here, not at boot, so a
   restored checkpoint (which holds them too) is always consistent. */
EXPORT("sim_run") uint32_t sim_run(uint32_t n, uint32_t stop_mask)
{
	S.vm.on_sys = on_sys;
	S.vm.on_write = on_write;
	uint32_t i = 0;
	while (i < n && !S.vm.halted) {
		int before = nev;
		vm_step(&S.vm);
		S.steps++;
		i++;
		if (S.vm.halted == 2) log_ev(EV_FAULT, S.vm.pc, 0, 0);
		if (stop_mask && nev > before && (ev[nev - 1].kind & stop_mask)) break;
	}
	return i;
}

EXPORT("sim_state") void *sim_state(void) { return &S; }
EXPORT("sim_state_size") int sim_state_size(void) { return (int)sizeof S; }
EXPORT("sim_mem") uint8_t *sim_mem(void) { return S.vm.mem; }
EXPORT("sim_reg") uint32_t sim_reg(int i) { return S.vm.r[i & 7]; }
EXPORT("sim_pc") int sim_pc(void) { return S.vm.pc; }
EXPORT("sim_halted") int sim_halted(void) { return S.vm.halted; }
EXPORT("sim_fault") const char *sim_fault(void) { return S.vm.fault; }
EXPORT("sim_steps") double sim_steps(void) { return (double)S.steps; }
EXPORT("sim_cycles") double sim_cycles(void) { return (double)S.vm.cycles; }
EXPORT("sim_heap") int sim_heap(void) { return S.vm.heap; }
/* the event log: JavaScript reads it, then clears it */
EXPORT("sim_events") Event *sim_events(void) { return ev; }
EXPORT("sim_nevents") int sim_nevents(void) { return nev; }
EXPORT("sim_clear_events") void sim_clear_events(void) { nev = 0; }
EXPORT("sim_writes") uint16_t *sim_writes(void) { return writes; }
EXPORT("sim_nwrites") int sim_nwrites(void) { return nwrites; }

int main(int argc, char **argv) { (void)argc; (void)argv; return 0; }
