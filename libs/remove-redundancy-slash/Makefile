GCC ?= clang

CFLAGS := -Wall -Werror -O2 -DLIBDEBUG=1
CFLAGS += -O2 -fPIC -I../..

SRC := $(wildcard *.c)

a.out:
	gcc $(CFLAGS) $(SRC) -o $@
	./a.out

clean: force
	rm a.out

force:
