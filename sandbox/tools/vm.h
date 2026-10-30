/* vm.h - the D32 emulator as a library: the runner and the checker drive it. */
#ifndef VM_H
#define VM_H
#include <stdint.h>
#include "d32.h"

typedef struct Vm Vm;

/* Called on every operating-system call the emulator does not handle itself
   (SYS_PRESENT, SYS_KEY, SYS_COPY), after the default handling. */
typedef void (*VmSysHook)(Vm *vm, int n, void *user);
/* Called on every memory write, when set (the checker uses it to measure). */
typedef void (*VmWriteHook)(Vm *vm, uint16_t addr, int bytes, void *user);

struct Vm {
	uint8_t     mem[D32_MEM];
	uint32_t    r[D32_NREG];
	uint16_t    pc;
	uint64_t    cycles;
	int         halted;      /* 0 running, 1 SYS_EXIT or HALT, 2 a fault */
	char        fault[96];
	/* the operating system's own state: outside the machine's memory */
	uint16_t    heap;        /* SYS_ALLOC bumps this upwards             */
	uint16_t    heap_limit;
	uint32_t    keys;        /* what SYS_KEY returns                     */
	int32_t     args[8];     /* what SYS_ARG returns                     */
	char        disk[256];   /* directory that SYS_LOAD reads files from */
	VmSysHook   on_sys;
	VmWriteHook on_write;
	void       *user;
};

/* Memory map the operating system sets up. */
#define VM_GAME_BASE   0x1000u   /* the game is loaded here at boot       */
#define VM_HEAP_START  0x5800u   /* SYS_ALLOC hands out memory from here  */
#define VM_HEAP_LIMIT  0xF000u   /* ...up to here; above is the boot stack */
#define VM_BOOT_SP     0xFFF0u

void     vm_init(Vm *vm, const char *disk);
int      vm_load(Vm *vm, const char *name, uint16_t base, uint16_t *entry);  /* size or -1 */
int      vm_boot(Vm *vm, const char *game);   /* load the game and point pc at it */
int      vm_step(Vm *vm);                     /* one instruction; 0 once halted    */
uint32_t vm_rd32(Vm *vm, uint16_t a);
void     vm_wr32(Vm *vm, uint16_t a, uint32_t v);
#endif
