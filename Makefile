# matryoshka scaffold
#
#   make run        build + run the host proof (32-bit freestanding)
#   make doscheck   compile every source with the ACE engine's exact flags
#   make ldr0n      assemble the reentrant first-stage loader skeleton
#   make clean

CC = gcc

# host proof: mtx_core/mtx_switch/dos_state/dos_alloc/ldr1n.
# dos_wire.c is NOT here - it references engine symbols (doom_data_segment,
# mainzone, screen_buffer, _bss_start) and only builds inside ACE.
CORE = mtx_core.c mtx_switch.S dos_state.c dos_alloc.c ldr1n.c mtx_sched.c

# host proof: freestanding, no libc, raw syscalls.
# -mgeneral-regs-only matches the engine (no x87/SSE), and is required for the
# no_caller_saved_registers attribute on mtx_frame_tick in mtx_sched.h.
HOSTFLAGS = -m32 -nostdlib -fno-pie -ffreestanding -fno-builtin \
            -fno-stack-protector -O2 -Wall -mgeneral-regs-only

# exact flags from doom_ace/engine/makefile
ENGFLAGS = -m32 -march=i386 -fno-pie -O2 -g0 -mgeneral-regs-only \
           -fdata-sections -ffunction-sections -fomit-frame-pointer \
           -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables \
           -fno-stack-protector -fno-reorder-blocks-and-partition \
           -fcf-protection=none -Wno-address-of-packed-member -U_FORTIFY_SOURCE

all: mtx_test

mtx_test: test_mtx.c $(CORE) context.h dos_state.h dos_alloc.h ldr1n.h
	$(CC) $(HOSTFLAGS) -no-pie -o $@ test_mtx.c $(CORE)

run: mtx_test
	./mtx_test

# In ACE there is no static snapshot pool: the host passes zone memory, so the
# 7.5 MiB static pool is compiled out.
DOSSRC = mtx_core.c mtx_switch.S dos_state.c dos_alloc.c dos_wire.c ldr1n.c ldr1N.S mtx_sched.c

doscheck:
	@for f in $(DOSSRC); do \
	  o=$$(echo $$f | sed 's/\..*$$/.o/'); \
	  echo "  CC (engine flags) $$f"; \
	  $(CC) $(ENGFLAGS) -DMTX_DOS_NO_STATIC_POOL -c -o /tmp/mtx_dos_$$o $$f || exit 1; \
	done
	@echo "DOS-target compile OK: $(DOSSRC)"

ldr0n: ldr0N.S
	$(CC) -m32 -nostdlib -c -o ldr0N.o ldr0N.S
	$(CC) -m32 -nostdlib ldr0N.o -o ldr0N -Wl,--section-start=.text=0x00034615
	objcopy -O binary --only-section=.text ldr0N ldr0N.bin
	@stat -c 'ldr0N.bin %s bytes  (ACE_LDR0 patch limit: 256)' ldr0N.bin

clean:
	rm -f *.o mtx_test ldr0N ldr0N.bin

.PHONY: all run doscheck ldr0n clean
