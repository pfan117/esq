all: a.out force
	./a.out

a.out: *.c tst/main.c
	gcc -I../.. $^

clean:
	rm -f *.o a.out

force:
