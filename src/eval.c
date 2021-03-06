/*
  Name:         Zeta Dva
  Description:  Amateur level chess engine
  Author:       Srdja Matovic <s.matovic@app26.de>
  Created at:   2011-01-15
  Updated at:   2017
  License:      GPL >= v2

  Copyright (C) 2011-2017 Srdja Matovic

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
  sq = (GETCOLOR(piece))? sq:FLIPFLOP(sq);
  score+= EvalTable[GETPTYPE(piece)*64+sq];
  /* sqaure control */
  score+= EvalControl[sq];

  return score;
}
/* evaluate board position, no checkmates or stalemates */
Score eval(Bitboard *board)
{
  Score score = 0;
  s32 side;
  Square i;
  Square sq;
  PieceType piecetype;
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
      sq = popfirst1(&bbWork);
      piecetype = GETPTYPE(GETPIECE(board,sq));

      /* piece bonus */
      score+= (side)?-10 : 10;
      /* wodd count */
      score+= (side)?-EvalPieceValues[piecetype]:EvalPieceValues[piecetype];
      /* piece square tables */
      score+= (side)?-EvalTable[piecetype*64+sq]:EvalTable[piecetype*64+FLIPFLOP(sq)];
      /* square control table */
      score+= (side)?-EvalControl[sq]:EvalControl[FLIPFLOP(sq)];

      /* simple pawn structure white */
      /* blocked */
      score-=(piecetype==PAWN&&side==WHITE&&GETRANK(sq)<RANK_8&&(bbBoth[BLACK]&SETMASKBB(sq+8)))?15:0;
        /* chain */
      score+=(piecetype==PAWN&&side==WHITE&&GETFILE(sq)<FILE_H&&(bbPawns&bbBoth[WHITE]&SETMASKBB(sq-7)))?10:0;
      score+=(piecetype==PAWN&&side==WHITE&&GETFILE(sq)>FILE_A&&(bbPawns&bbBoth[WHITE]&SETMASKBB(sq-9)))?10:0;
      /* column */
      for(i=sq-8;i>7&&piecetype==PAWN&&side==WHITE;i-=8)
        score-=(bbPawns&bbBoth[WHITE]&SETMASKBB(i))?30:0;

      /* simple pawn structure black */
      /* blocked */
      score+=(piecetype==PAWN&&side==BLACK&&GETRANK(sq)>RANK_1&&(bbBoth[WHITE]&SETMASKBB(sq-8)))?15:0;
        /* chain */
      score-=(piecetype==PAWN&&side==BLACK&&GETFILE(sq)>FILE_A&&(bbPawns&bbBoth[BLACK]&SETMASKBB(sq+7)))?10:0;
      score-=(piecetype==PAWN&&side==BLACK&&GETFILE(sq)<FILE_H&&(bbPawns&bbBoth[BLACK]&SETMASKBB(sq+9)))?10:0;
      /* column */
      for(i=sq+8;i<56&&piecetype==PAWN&&side==BLACK;i+=8)
        score+=(bbPawns&bbBoth[BLACK]&SETMASKBB(i))?30:0;

    }
    /* duble bishop */
    score+= (popcount(bbBoth[side]&(~board[QBBP1]&~board[QBBP2]&board[QBBP3]))==2)?(side)?-25:25:0;
    
  }
  return score;
}

