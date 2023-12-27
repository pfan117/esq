all: a.out force
	./a.out

force:

a.out: main.c ../ipv4-addr.c ../integer-10000000.c ../hash-string.c
	gcc -I../../.. $^

clean:
	rm a.out
