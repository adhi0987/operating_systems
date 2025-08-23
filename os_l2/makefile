all:
	gcc -Wall -o parent parent.c
	gcc -Wall -o child child.c
	gcc -Wall -o dummy dummy.c
run: all
	./parent 10
clean:
	clear
	-rm -f parent child dummy childpid.txt dummycpid.txt