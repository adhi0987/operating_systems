# Compiler and flags
CC = gcc
CFLAGS = -Wall

# Targets
all: cook waiter customer

# Rule to compile cook.c
cook: cook.c
	$(CC) $(CFLAGS) -o cook cook.c

# Rule to compile waiter.c
waiter: waiter.c
	$(CC) $(CFLAGS) -o waiter waiter.c

# Rule to compile customer.c
customer: customer.c
	$(CC) $(CFLAGS) -o customer customer.c

# Rule to generate customers.txt using gencustomers.c
db: gencustomers
	./gencustomers > customers.txt

# Rule to compile gencustomers.c
gencustomers: gencustomers.c
	$(CC) $(CFLAGS) -o gencustomers gencustomers.c

# Rule to clean up compiled binaries
clean:
	-rm -f cook waiter customer gencustomers

# Phony targets (not actual files)
.PHONY: all db clean