/*
  Name:         Zeta Dva
  Description:  Amateur level chess engine
  Author:       Srdja Matovic <s.matovic@app26.de>
  Created at:   2011-01-15
  Updated at:   2016-07-13
  License:      GPL >= v2

  Copyright (C) 2011-2016 Srdja Matovic

  Zeta Dva is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  Zeta Dva is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

#ifndef ZETA_H_INCLUDED
#define ZETA_H_INCLUDED

/* global variables */
extern FILE 	*LogFile;
/* counters */
extern u64 NODECOUNT;
extern u64 MOVECOUNT;
/* timers */
extern double start;
extern double end;
extern double elapsed;
/* game state */
extern bool STM;
extern s32 SD;
extern u32 GAMEPLY;
extern u32 PLY;
extern Move *Move_History;
extern Hash *Hash_History;

bool squareunderattack (Bitboard *board, bool stm, Square sq);
bool kingincheck(Bitboard *board, bool stm);
int cmp_move_desc(const void *ap, const void *bp);
void domove (Bitboard *board, Move move);
void undomove (Bitboard *board, Move move, Move lastmove, Cr cr);
void domovequick (Bitboard *board, Move move);
void undomovequick (Bitboard *board, Move move);

#endif /* ZETA_H_INCLUDED */

