# Simple Makefile for the defrag project

CC = gcc
CFLAGS = -std=c11 -O0 -g -Wall -Wextra

all: defrag

defrag: defrag.c
	$(CC) $(CFLAGS) -o defrag defrag.c

clean:
	rm -f defrag disk_defrag
