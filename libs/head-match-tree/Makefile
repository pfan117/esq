GCC ?= clang

CFLAGS := -Wall -Werror -I../..
CFLAGS += -ggdb3 -O0
# CFLAGS += -O2
CFLAGS += -fPIC -DLIBDEBUG=1
CFLAGS += -fprofile-arcs -ftest-coverage

LDFLAGS := -L/usr/local/lib
LDFLAGS += -fprofile-arcs -ftest-coverage -lgcov

SRC := $(wildcard *.c)
TESTSRC := $(wildcard tst/*.c)
OBJS := $(SRC:%.c=%.o)
TESTOBJS := $(TESTSRC:%.c=%.o)
TESTEXECUTABLE := a.out

%.o:%.c
	@$(GCC) $(CFLAGS) $(CCINCLUDES) -c $< -o $@ $(LDFLAGS)

all: $(TESTEXECUTABLE)

a.out: $(TESTOBJS) $(OBJS)
	$(GCC) $(CFLAGS) $^ -o $@

test: $(TESTEXECUTABLE)
	./$(TESTEXECUTABLE)

clean:
	rm -f $(TESTOBJS) $(OBJS) $(TESTEXECUTABLE)
	rm -f *.gcno *.gcda
	rm -f tst/*.gcno tst/*.gcda
