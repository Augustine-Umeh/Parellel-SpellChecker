CC=gcc
CFLAGS=-Wall -g -std=c11 -lpthread

all: checker

checker: parallelSpellChecker.c
	$(CC) -o checker parallelSpellChecker.c $(CFLAGS)

clean:
	rm -f checker *.out processed_*.txt 
	rm -rf checker.dSYM