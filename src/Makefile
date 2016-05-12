#	Makefile for zetadva

CC=gcc
#CC=/usr/bin/i686-w64-mingw32-gcc
#CC=/usr/bin/x86_64-w64-mingw32-gcc
CFLAGS=-Wall -Wextra -pedantic -std=c89 -O2

TARGETS=zetadva

zetadva: 
	$(CC) $(CFLAGS) -o zetadva bitboard.c book.c eval.c timer.c zetadva.c

clean:
	rm -f ${TARGETS}

