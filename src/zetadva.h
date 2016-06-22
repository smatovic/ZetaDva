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
extern u64 COUNTERS1;
extern u64 COUNTERS2;

/* timers */
extern double start;
extern double end;
extern double elapsed;
bool TIMEOUT;  /* global value for time control*/
/* game state */
extern bool STM;
extern s32 SD;
extern s32 GAMEPLY;
extern s32 PLY;
extern Move *MoveHistory;
extern Hash *HashHistory;
extern Move *Killers;
extern Move *Counters;
extern const Bitboard LRANK[2];
extern double MaxTime;

extern bool xboard_post;
extern bool xboard_mode;
extern bool xboard_debug;
extern bool epd_mode;

bool squareunderattack(Bitboard *board, bool stm, Square sq);
bool kingincheck(Bitboard *board, bool stm);
int cmp_move_desc(const void *ap, const void *bp);
void domove(Bitboard *board, Move move);
void undomove(Bitboard *board, Move move, Move lastmove, Cr cr, Score score, Hash hash);
void domovequick (Bitboard *board, Move move);
void undomovequick (Bitboard *board, Move move);
void donullmove(Bitboard *board);
void undonullmove(Bitboard *board, Move lastmove, Hash hash);
void printboard(Bitboard *board);
void printbitboard(Bitboard board);
void printmove(Move move);
void printmovecan(Move move);
Hash computehash(Bitboard *board, bool stm);
void save_to_tt(Hash hash, Move move, Score score, u8 flag, s32 ply, s32 depth);
struct TTE *load_from_tt(Hash hash);
s32 collect_pv_from_hash(Bitboard *board, Hash hash, Move *moves, s32 ply);
void save_killer(Move move, Score score, s32 ply);
bool isvalid(Bitboard *board);

#endif /* ZETA_H_INCLUDED */

