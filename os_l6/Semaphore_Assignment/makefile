# Compiler and flags
CC = gcc
CFLAGS = -Wall -I. # Include current directory for common.h

# Targets
all: cook waiter customer 

# Rule to compile cook.c with common.c
cook: cook.o common.o
	$(CC) $(CFLAGS) -o cook cook.o common.o

# Rule to compile waiter.c with common.c
waiter: waiter.o common.o
	$(CC) $(CFLAGS) -o waiter waiter.o common.o

# Rule to compile customer.c with common.c
customer: customer.o common.o
	$(CC) $(CFLAGS) -o customer customer.o common.o

# Rule to compile object files (except gencustomers.c)
%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to generate customers.txt using gencustomers.c
db: gencustomers
	./gencustomers > customers.txt

# Rule to compile gencustomers.c
gencustomers: gencustomers.c
	$(CC) $(CFLAGS) -o gencustomers gencustomers.c

# Rule to clean up compiled binaries and object files
clean:
	-rm -f cook waiter customer gencustomers *.o customers.txt

# Phony targets (not actual files)
.PHONY: all db clean
