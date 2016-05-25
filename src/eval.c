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

Score evalmove(PieceType piece, Square sq, bool stm)
{
  Score score = 0;

  score+= (stm)? EvalPieceValues[GETPTYPE(piece)-1]   : EvalPieceValues[GETPTYPE(piece)-1];
  score+= (stm)? EvalTable[(GETPTYPE(piece)-1)*64+sq] : EvalTable[(GETPTYPE(piece)-1)*64+FLOP(sq)];
  score+= (stm)? EvalControl[sq]                      : EvalControl[FLOP(sq)];

  return score;
}
/* evaluate board position, no checkmates or stalemates */
Score eval(Bitboard *board)
{
  u8 side;
  Score score = 0;
  Square sq;
  PieceType piecet;
  Bitboard bbWork;
  Bitboard bbPawns = (board[QBBP1]&~board[QBBP2]&~board[QBBP3]);
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
      piecet  = GETPTYPE(GETPIECE(board,sq));

      /* piece bonus */
      score+= side? -10 : 10;
      /* wodd count */
      score+= side? -EvalPieceValues[piecet-1]    : EvalPieceValues[piecet-1];
      /* piece square tables */
      score+= side? -EvalTable[(piecet-1)*64+sq]  : EvalTable[(piecet-1)*64+FLOP(sq)];
      /* square control table */
      score+= side? -EvalControl[sq]              : EvalControl[FLOP(sq)];

      /* simple pawn structure */
      /* blocked */
      if (piecet == PAWN && !side) {
        /* blocked */
        if ( GETRANK(sq) < RANK_8 && ( (bbPawns | board[!side]) & SETMASKBB(sq+8)))
          score-= 15;
        /* chain */
        if ( GETFILE(sq) < FILE_H  && ( bbPawns & board[side] & SETMASKBB(sq-7))  )
          score+= 10;
        if ( GETFILE(sq) > FILE_A  && ( bbPawns & board[side] & SETMASKBB(sq-9))  )
          score+= 10;
      }
      if (piecet == PAWN && side) {
        /* blocked */
        if ( GETRANK(sq) > RANK_1 && ( (bbPawns | board[!side]) & SETMASKBB(sq-8)))
          score+= 15;
        /* chain */
        if ( GETFILE(sq) > FILE_A  && ( bbPawns & board[side] & SETMASKBB(sq+7))  )
          score-= 10;
        if ( GETFILE(sq) < FILE_H  && ( bbPawns & board[side] & SETMASKBB(sq+9))  )
          score-= 10;
      }
    }
    /* duble bishop */
    score+= (popcount(bbBoth[side]&(~board[QBBP1]&~board[QBBP2]&board[QBBP3]))>=2)?side?-25:25:0;
  }
  return score;
}
