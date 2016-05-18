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

#include <stdio.h>      /* for print and scan */

#include "bitboard.h"   /* for population count, pop_count */
#include "eval.h"       /* for evalmove and eval */
#include "types.h"      /* custom types, board defs, data structures, macros */
#include "zetadva.h"   /* for global vars */

Score perft (Bitboard *board, bool stm, u32 depth)
{

  Move moves[MAXMOVES];
  Move lastmove = board[QBBLAST];
  Score score = 0;
  int i = 0;
  int movecounter = 0;
  bool kic = false;

  kic = kingincheck (board, stm);

  if (depth == SD)
  {
    NODECOUNT++;
    return 0;
  }

  movecounter = genmoves_general (board, moves, movecounter, stm, false);

  MOVECOUNT+= movecounter;

  if (movecounter == 0 && kic)
  {
    return 0;
  }
  if (movecounter == 0 && !kic) 
  {
    return 0;
  }

  /* iterate through moves */
  for (i=0;i<movecounter;i++)
  {
    domove (board, moves[i]);
    score = -perft(board, !stm, depth+1);
    undomove (board, moves[i], lastmove);
  }
  return 0;
}

