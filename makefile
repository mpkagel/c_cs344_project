# makefile for smallsh.c

CC = gcc
CFLAGS = -Wall -std=c99

smallsh: smallsh.o CommandLine.o
	$(CC) $(CFLAGS) -o $@ $^

smallsh.o: smallsh.c CommandLine.h
	$(CC) $(CFLAGS) -c -o $@ $(@:.o=.c) 

CommandLine.o: CommandLine.c CommandLine.h 
	$(CC) $(CFLAGS) -c -o $@ $(@:.o=.c)

clean: 
	-rm *.o
	-rm smallsh
