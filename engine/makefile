program = code
OBJ = asm.o main.o utils.o bugfix.o vesa.o wadfile.o config.o controls.o ldr_texture.o ldr_flat.o ldr_sprite.o textpars.o filebuf.o think.o generic.o sound.o mobj.o player.o decorate.o action.o dehacked.o animate.o inventory.o weapon.o map.o hitscan.o special.o sight.o draw_c.o draw_a.o polyobj.o render.o terrain.o font.o extra3d.o stbar.o menu.o saveload.o cheat.o rng.o wipe.o demo.o mover.o net.o doom_vars.o mtx_boot.o mtx_ldr.o
CC = gcc -m32 -march=i386
OPT = -O2 -g0
BOTH = -fno-pie
CFLAGS = ${BOTH} ${OPT} -mgeneral-regs-only -fdata-sections -ffunction-sections -fomit-frame-pointer -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-stack-protector -fno-reorder-blocks-and-partition -fcf-protection=none -Wno-address-of-packed-member -U_FORTIFY_SOURCE

# matryoshka nested-instance host, in nested/ in this repository.
# Override with `make MTX_DIR=/path/to/nested` if it lives elsewhere.
MTX_DIR ?= ../nested
MTX_OBJ = ${MTX_DIR}/mtx_core.o ${MTX_DIR}/mtx_switch.o ${MTX_DIR}/mtx_host.o ${MTX_DIR}/dos_state.o ${MTX_DIR}/dos_alloc.o ${MTX_DIR}/dos_wire.o ${MTX_DIR}/ldr1n.o ${MTX_DIR}/ldr0N.o ${MTX_DIR}/ldr1N.o ${MTX_DIR}/mtx_sched.o ${MTX_DIR}/mtx_zone.o ${MTX_DIR}/mtx_hooks.o ${MTX_DIR}/mtx_raw.o
CFLAGS += -I${MTX_DIR} -DMTX_DOS_NO_STATIC_POOL
OBJ += ${MTX_OBJ}

${MTX_DIR}/%.o: ${MTX_DIR}/%.c
	${CC} ${CFLAGS} -c -o $@ $<
${MTX_DIR}/%.o: ${MTX_DIR}/%.S
	${CC} ${CFLAGS} -c -o $@ $<

build: ${program}.lmp

# Header dependencies. Without these, editing a header leaves stale objects behind:
# the nested objects compile `mtx_hs` field offsets out of mtx_host.h, so a header
# change that is not rebuilt silently mismatches mtx_boot.o against mtx_ldr.o.
# Measured symptom of exactly that: "ace image: load FAILED (-1)" - the image
# slots read back at the wrong offset - with a perfectly good WAD and a clean boot.
# Blunt on purpose: every object depends on every header. This tree is small, and a
# silent miscompile costs far more than a rebuild.
#
# NOTE: keep this AFTER `build:`, or make takes the first target of this rule
# (asm.o) as the default goal and a bare `make -C engine` stops building the lump.
HDRS = $(wildcard *.h) $(wildcard ${MTX_DIR}/*.h)
${OBJ}: ${HDRS}

clean:
	rm -f *.o ${program} ${program}.rel ${program}.bin ${program}.lmp ${MTX_OBJ}

${program}.lmp: ${OBJ}
	${CC} ${OBJ} ${LIBS} ${BOTH} -o ${program} -nostdlib -Wl,--gc-sections -Wl,--no-ld-generated-unwind-info -Wl,--no-dynamic-linker -Wl,--omagic -Wl,-T linker.ld -Wl,-Map=code.map
	objcopy -O binary --only-section=.text --only-section=.hooks --only-section=.rodata --only-section=.data ${program} ${program}.bin
	objcopy -O binary --only-section=.rel.dyn ${program} ${program}.rel
	./lumpgen.py ${program}.bin ${program}.rel ${program}.lmp

