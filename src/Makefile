CC = gcc

CFLAGS = -Wall -g -I./common/

HEADER = common/shared.h

all: director supplier worker

director: director.c $(HEADER)
	$(CC) $(CFLAGS) director.c -o director

supplier: supplier.c $(HEADER)
	$(CC) $(CFLAGS) supplier.c -o supplier

worker: worker.c $(HEADER)
	$(CC) $(CFLAGS) worker.c -o worker

clean:
	rm -f director supplier worker magazyn.dat fabryka_raport.txt