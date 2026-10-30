# The Doomception sandbox. Everything is built by one C compiler (the host's) into
# build/: the machine (run), the assembler (as), the kit's own C compiler (cc).
CC      ?= cc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter
B       := build

TOOLS := $(B)/as $(B)/run $(B)/cc

all: $(TOOLS)

$(B):
	mkdir -p $(B)

$(B)/as: tools/as.c tools/vm.c tools/vm.h tools/d32.h | $(B)
	$(CC) $(CFLAGS) -o $@ tools/as.c tools/vm.c

$(B)/run: tools/run.c tools/vm.c tools/vm.h tools/d32.h | $(B)
	$(CC) $(CFLAGS) -o $@ tools/run.c tools/vm.c

$(B)/cc: tools/cc.c | $(B)
	$(CC) $(CFLAGS) -o $@ tools/cc.c

# the toolchain's own tests
test: $(TOOLS)
	$(B)/as -o $(B)/hello.o tests/hello.s
	$(B)/run -e -d $(B) -g hello.o; test $$? -eq 42
	$(B)/cc -o $(B)/t1.s tests/t1.c
	$(B)/as -o $(B)/t1.o $(B)/t1.s
	$(B)/run -e -d $(B) -g t1.o

clean:
	rm -rf $(B)

.PHONY: all test clean
