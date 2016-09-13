#	Makefile for zeta dva

CC=gcc
#CC=/usr/bin/i686-w64-mingw32-gcc
#CC=/usr/bin/x86_64-w64-mingw32-gcc
CFLAGS=-Wall -Wextra -std=c99 -O3 --static

TARGETS=zetadva

zetadva: 
	$(CC) $(CFLAGS) -o zetadva0303.bin bitboard.c book.c eval.c movegen.c search.c timer.c zetadva.c

clean:
	rm -f ${TARGETS}

