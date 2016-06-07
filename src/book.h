/*
  Polyglot book probing code, implemented as described by Michel Van den Bergh
  http://hardy.uhasselt.be/Toga/book_format.html
*/

#ifndef BOOK_H_INCLUDED
#define BOOK_H_INCLUDED

#include "types.h"      /* custom types, board defs, data structures, macros */

extern u64 Random64[781];
extern u64 *RandomPiece;
extern u64 *RandomCastle;
extern u64 *RandomEnPassant;
extern u64 *RandomTurn;

Hash computebookhash(Bitboard *board, bool stm);

#endif /* BOOK_H_INCLUDED */

