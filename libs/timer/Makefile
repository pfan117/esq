all: a.out force
	./a.out

a.out: main.c
	gcc -DTIMER_CHECK=1 main.c

clean: force
	rm -rf *.out *.o

force:
