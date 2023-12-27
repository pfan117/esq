GCC ?= clang

CFLAGS := -Wall -Werror
CFLAGS += -O2 -fPIC -I../../
LDFLAGS := -L/usr/local/lib

SRC := $(wildcard *.c) ../utf8-to-unicode/utf8-to-unicode.c
TESTSRC := $(wildcard tst/*.c)
OBJS := $(SRC:%.c=%.o)
TESTOBJS := $(TESTSRC:%.c=%.o)
TESTEXECUTABLE := a.out

%.o:%.c
	@$(GCC) $(CFLAGS) $(CCINCLUDES) -c $< -o $@

all: $(TESTEXECUTABLE)

a.out: $(TESTOBJS) $(OBJS)
	$(GCC) $(CFLAGS) $^ -o $@

test: $(TESTEXECUTABLE)
	./$(TESTEXECUTABLE)

clean:
	rm -f $(TESTOBJS) $(OBJS) $(TESTEXECUTABLE)
