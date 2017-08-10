counter: counter.c
	gcc -c -std=c99 counter.c
	gcc -o multisum counter.o
clean:
	rm counter.o counter
