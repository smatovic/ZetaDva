#	Makefile for zeta dva

CC=gcc
#CC=/usr/bin/i686-w64-mingw32-gcc
#CC=/usr/bin/x86_64-w64-mingw32-gcc
CFLAGS=-Wall -Wextra -pedantic -std=c89 -O2 -Wno-long-long -Wno-format -Wno-unused-function

TARGETS=zetadva

zetadva: 
	$(CC) $(CFLAGS) -o zetadva bitboard.c book.c eval.c search.c timer.c zetadva.c

clean:
	rm -f ${TARGETS}

