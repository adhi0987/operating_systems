all: block coordinator

block: block.c
	gcc -Wall -o block block.c

coordinator: coordinator.c
	gcc -Wall -o coordinator coordinator.c

run: all
	./coordinator

clean:
	-rm -f block coordinator