GCC ?= clang

CFLAGS := -Wall -Werror -DMODULE_TEST_MODE=1
CFLAGS += -O2 -fPIC
CFLAGS += -I../../
CFLAGS += -I${HOME}/local/include
LDFLAGS := -L/usr/local/lib

SRC := $(wildcard *.c)
OBJS := $(SRC:%.c=%.o)

%.o:%.c
	@$(GCC) $(CFLAGS) $(CCINCLUDES) -c $< -o $@

all: a.out

a.out: $(OBJS)
	$(GCC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(OBJS) a.out tmp_*
