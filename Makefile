CC = gcc
FLAGS = -Wall -g

PROGS = minls minget

all: $(PROGS) clean

minls: minls.c minls.h min.o
	$(CC) $(FLAGS) -c minls.c
	$(CC) $(FLAGS) -o minls minls.o min.o
	rm minls.o

minget: minget.c minget.h min.o
	$(CC) $(FLAGS) -c minget.c
	$(CC) $(FLAGS) -o minget minget.o min.o

min.o: min.h min.c
	$(CC) $(FLAGS) -c min.c

clean:
	rm -f *.o *~ TAGS