GCC ?= clang

CFLAGS := -Wall -Werror -I../..
CFLAGS += -ggdb3 -O0
# CFLAGS += -O2
CFLAGS += -fPIC -DLIBDEBUG=1
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS := -L/usr/local/lib
LDFLAGS += -fprofile-arcs -ftest-coverage -lgcov

SRC := $(wildcard *.c)
SRC += $(wildcard tst/*.c)
SRC += $(wildcard tst/frames/*.c)
SRC += $(wildcard ../hex-dump/*.c)
SRC += $(wildcard ../strnstr/*.c)

TESTEXECUTABLE := a.out

all: $(TESTEXECUTABLE)

a.out: force
	$(GCC) $(CFLAGS) $(CCINCLUDES) $(SRC) -o $@ $(LDFLAGS)

demo: test

test: $(TESTEXECUTABLE)
	./$(TESTEXECUTABLE)

clean:
	rm -f $(TESTEXECUTABLE)
	rm -f *.gcda *.gcno

force:

