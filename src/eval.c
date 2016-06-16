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

#include "bitboard.h"   /* for population count, pop_count */
#include "pst.h"        /* piece square tables, wood count, table control */
#include "types.h"      /* custom types, board defs, data structures, macros */

/* 
  simple evaluation, based on proposal by Tomasz Michniewski
  https://chessprogramming.wikispaces.com/Simplified+evaluation+function
*/

Score evalmove(Piece piece, Square sq)
{
  Score score = 0;

  /* wood count */
  score+= EvalPieceValues[GETPTYPE(piece)];
  /* piece square tables */
  sq = (GETCOLOR(piece))? FLIP(sq) : FLIP(FLOP(sq));
  score+= EvalTable[GETPTYPE(piece)*64+sq];
  /* sqaure control */
  score+= EvalControl[sq];

  return score;
}
/* evaluate board position with static values no checkmates or stalemates */
Score evalstatic(Bitboard *board)
{
  u32 side;
  Score score = 0;
  Square sq;
  PieceType piecetype;
  Bitboard bbWork;
  Bitboard bbBoth[2];

  bbBoth[WHITE] = board[QBBBLACK]^(board[QBBP1]|board[QBBP2]|board[QBBP3]);
  bbBoth[BLACK] = board[QBBBLACK];

  /* for each side */
  for(side=WHITE;side<=BLACK;side++) 
  {
    bbWork = bbBoth[side];

    while (bbWork) 
    {
      sq      = popfirst1(&bbWork);
      piecetype  = GETPTYPE(GETPIECE(board,sq));

      /* wodd count */
      score+= (side)? -EvalPieceValues[piecetype]:EvalPieceValues[piecetype];
      /* piece square tables */
      score+= (side)? -EvalTable[piecetype*64+FLIP(sq)]:EvalTable[piecetype*64+FLIP(FLOP(sq))];
      /* square control table */
      score+= (side)? -EvalControl[FLIP(sq)]:EvalControl[FLIP(FLOP(sq))];
    }
  }
  return score;
}
/* evaluate board position, no checkmates or stalemates */
Score eval(Bitboard *board)
{
  Score score = 0;
  s32 side;
  s32 i;
  Square sq;
  Piece piece;
  Bitboard bbWork;
  Bitboard bbPawns;
  Bitboard bbBoth[2];

  bbPawns = board[QBBP1]&~board[QBBP2]&~board[QBBP3];
  bbBoth[WHITE] = board[QBBBLACK]^(board[QBBP1]|board[QBBP2]|board[QBBP3]);
  bbBoth[BLACK] = board[QBBBLACK];

  /* for each side */
  for(side=WHITE;side<=BLACK;side++) 
  {
    bbWork = bbBoth[side];

    while (bbWork) 
    {
      sq     = popfirst1(&bbWork);
      piece  = GETPIECE(board,sq);

      /* piece bonus */
      score+= (side)?-10 : 10;
      /* wodd count */
      score+= (side)?-EvalPieceValues[GETPTYPE(piece)]:EvalPieceValues[GETPTYPE(piece)];
      /* piece square tables */
      score+= (side)?-EvalTable[GETPTYPE(piece)*64+FLIP(sq)]:EvalTable[GETPTYPE(piece)*64+FLIP(FLOP(sq))];
      /* square control table */
      score+= (side)?-EvalControl[FLIP(sq)]:EvalControl[FLIP(FLOP(sq))];
      /* simple pawn structure white */
      /* blocked */
      score-=(GETPTYPE(piece)==PAWN&&!side&&GETRANK(sq)<RANK_8&&((bbPawns|board[!side])&SETMASKBB(sq+8)))?15:0;
      /* column */
      for(i=sq-8;i>7&&!side;i-=8)
        score-=(bbPawns&bbBoth[side]&SETMASKBB(i))?15:0;
        /* chain */
      score+=(GETPTYPE(piece)==PAWN&&!side&&(GETFILE(sq)<FILE_H&&(bbPawns&board[side]&SETMASKBB(sq-7))))?10:0;
      score+=(GETPTYPE(piece)==PAWN&&!side&&(GETFILE(sq)>FILE_A&&(bbPawns&board[side]&SETMASKBB(sq-9))))?10:0;
      /* simple pawn structure black */
      /* blocked */
      score+=(GETPTYPE(piece)==PAWN&&side&&GETRANK(sq)>RANK_1&&((bbPawns|board[!side])&SETMASKBB(sq-8)))?15:0;
        /* chain */
      score-=(GETPTYPE(piece)==PAWN&&side&&(GETFILE(sq)>FILE_A&&(bbPawns&board[side]&SETMASKBB(sq+7))))?10:0;
      score-=(GETPTYPE(piece)==PAWN&&side&&(GETFILE(sq)<FILE_H&&(bbPawns&board[side]&SETMASKBB(sq+9))))?10:0;
      /* column */
      for(i=sq+8;i<56&&side;i+=8)
        score+=(bbPawns&bbBoth[side]&SETMASKBB(i))?15:0;
    }
    /* duble bishop */
    score+= (popcount(bbBoth[side]&(~board[QBBP1]&~board[QBBP2]&board[QBBP3]))==2)?(side)?-25:25:0;

  }
  return score;
}

