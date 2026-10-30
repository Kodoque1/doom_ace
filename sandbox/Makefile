# The Doomception sandbox. Everything is built by one C compiler (the host's) into
# build/: the machine (run), the assembler (as), the kit's own C compiler (cc).
CC      ?= cc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter
B       := build

TOOLS := $(B)/as $(B)/run $(B)/cc $(B)/check

all: $(TOOLS)

$(B):
	mkdir -p $(B)

$(B)/as: tools/as.c tools/vm.c tools/vm.h tools/d32.h | $(B)
	$(CC) $(CFLAGS) -o $@ tools/as.c tools/vm.c

$(B)/run: tools/run.c tools/vm.c tools/vm.h tools/d32.h | $(B)
	$(CC) $(CFLAGS) -o $@ tools/run.c tools/vm.c

$(B)/check: tools/check.c tools/vm.c tools/vm.h tools/d32.h | $(B)
	$(CC) $(CFLAGS) -o $@ tools/check.c tools/vm.c

$(B)/cc: tools/cc.c | $(B)
	$(CC) $(CFLAGS) -o $@ tools/cc.c

# TinyDoom and its levels, on the disk the machine boots from. game.inc exports the
# game's addresses to the level scripts.
D       := $(B)/disk
LEVEL_OBJS := $(patsubst game/levels/%.c,$(D)/%.o,$(wildcard game/levels/*.c))

game: $(D)/game.o $(LEVEL_OBJS)

$(D):
	mkdir -p $(D)

$(D)/game.o $(B)/game.sym: game/tinydoom.c game/game.h $(TOOLS) | $(D)
	$(B)/cc -o $(B)/game.s game/tinydoom.c
	$(B)/as -o $(D)/game.o -s $(B)/game.sym $(B)/game.s

$(B)/game.inc: $(B)/game.sym
	awk '$$1 == "__data_start" { $$1 = "GAME_DATA_START" } \
	     $$1 == "__data_end"   { $$1 = "GAME_DATA_END" } \
	     $$1 !~ /^(__|\.L)/    { print ".set " $$1 ", 0x1000 + 0x" $$2 }' $< > $@

$(D)/%.o: game/levels/%.c game/script.h game/game.h $(B)/game.inc $(TOOLS) | $(D)
	$(B)/cc -o $(B)/$*.s $<
	$(B)/as -o $@ $(B)/game.inc $(B)/$*.s

# watch a level alone: make play L=1
L ?= 1
play: game
	$(B)/run -d $(D) -A $(L) -f 200 -p $(B)/level$(L).ppm

# level 0: your scheduler (SCHED=spoilers/sched.c builds the reference instead)
SCHED ?= you/sched.c
$(D)/level0.o: $(SCHED) kit/kit.c kit/kit.h game/script.h game/game.h $(B)/game.inc $(TOOLS) FORCE | $(D)
	$(B)/cc -o $(B)/level0.s $(SCHED) kit/kit.c
	$(B)/as -o $@ $(B)/game.inc $(B)/level0.s

# watch the copies take turns: make watch N=2 LEVELS=1,2
N      ?= 2
LEVELS ?= 1,2
watch: game $(D)/level0.o
	$(B)/run -d $(D) -A 0,$(N),0,$(LEVELS) -f 12 -a

# the milestones: make check, or make check M=3
M ?= 0
check: game $(D)/level0.o
	$(B)/check -d $(D) -s $(B)/game.sym -m $(M)

FORCE:

# the toolchain's own tests
test: $(TOOLS)
	$(B)/as -o $(B)/hello.o tests/hello.s
	$(B)/run -e -d $(B) -g hello.o; test $$? -eq 42
	$(B)/cc -o $(B)/t1.s tests/t1.c
	$(B)/as -o $(B)/t1.o $(B)/t1.s
	$(B)/run -e -d $(B) -g t1.o

# the checker's own test: each broken scheduler must stop at its milestone
selftest: $(TOOLS) game
	@for t in you/sched.c:0 spoilers/broken/switch_only.c:1 spoilers/broken/no_patches.c:2 \
	          spoilers/broken/heap_tail.c:3 spoilers/broken/patches_once.c:4 \
	          spoilers/broken/pinned_end.c:4 spoilers/broken/no_ring.c:5 spoilers/sched.c:6; do \
		f=$${t%:*}; want=$${t#*:}; \
		$(MAKE) -s $(D)/level0.o SCHED=$$f || exit 1; \
		got=$$($(B)/check -d $(D) -s $(B)/game.sym | sed -n 's/^Reached: M\{0,1\}//p'); \
		if [ "$$got" = "$$want" ]; then echo "ok   $$f reaches $$want"; \
		else echo "FAIL $$f reaches $$got, not $$want"; exit 1; fi; \
	done

clean:
	rm -rf $(B)

.PHONY: all game play watch check test selftest clean FORCE
