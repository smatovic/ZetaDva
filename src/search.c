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
#include "movegen.h"    /* for move generator thingies */
#include "types.h"      /* custom types, board defs, data structures, macros */
#include "zetadva.h"    /* for global vars */

Score perft(Bitboard *board, bool stm, u32 depth)
{
  bool kic = false;
  int i = 0;
  int movecounter = 0;
  Cr cr = board[QBBPMVD];
  Move lastmove = board[QBBLAST];
  Move moves[MAXMOVES];

  kic = kingincheck(board, stm);

  if (depth == SD)
  {
    NODECOUNT++;
    return 0;
  }

  movecounter = genmoves_general(board, moves, movecounter, stm, false);

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
    perft(board, !stm, depth+1);
    undomove (board, moves[i], lastmove, cr);
  }
  return 0;
}
Score qsearch(Bitboard *board, bool stm, Score alpha, Score beta, u32 depth)
{
  bool kic = false;
  int i = 0;
  int movecounter = 0;
  Score score = 0;
  Cr cr = board[QBBPMVD];
  Move lastmove = board[QBBLAST];
  Move moves[MAXMOVES];

  kic = kingincheck(board, stm);

  movecounter = genmoves_general(board, moves, movecounter, stm, true);
  MOVECOUNT+= movecounter;

  /* iterate through moves */
  /* iterate through moves */
  for (i=0;i<movecounter;i++)
  {
    domove (board, moves[i]);
    score = -qsearch(board, !stm, -beta, -alpha, depth+1);
    undomove (board, moves[i], lastmove, cr);

    if(score>=alpha)
      alpha=score;
  }
  return score;
}

Score negamax(Bitboard *board, bool stm, Score alpha, Score beta, u32 depth)
{
  bool kic = false;
  int i = 0;
  int movecounter = 0;
  Score score = 0;
  Cr cr = board[QBBPMVD];
  Move lastmove = board[QBBLAST];
  Move moves[MAXMOVES];

  NODECOUNT++;

  kic = kingincheck(board, stm);

  movecounter = genmoves_general(board, moves, movecounter, stm, false);
  MOVECOUNT+= movecounter;

  if (movecounter == 0 && kic)
    return -MATESCORE+PLY;
  if (movecounter == 0 && !kic) 
    return STALEMATESCORE;

  if (depth == SD)
  {
    score = eval(board);
    score = (stm)? -score:score;  /* negate blacks score */
    return score;
  }

  /* iterate through moves */
  for (i=0;i<movecounter;i++)
  {
    domove (board, moves[i]);
    score = -negamax(board, !stm, -beta, -alpha, depth+1);
    undomove (board, moves[i], lastmove, cr);

    if(score>=beta)
      return score;

    if(score>=alpha)
      alpha=score;
  }
  return score;
}
Move search(Bitboard *board, bool stm)
{
  bool kic = false;
  int i = 0;
  int movecounter = 0;
  Score score = 0;
  Score alpha = -INF;
  Score beta  =  INF;
  Cr cr = board[QBBPMVD];
  Move bestmove = MOVENONE;
  Move lastmove = board[QBBLAST];
  Move moves[MAXMOVES];

  kic = kingincheck(board, stm);

  movecounter = genmoves_general(board, moves, movecounter, stm, false);

  MOVECOUNT+= movecounter;

  if (movecounter == 0 && kic)
  {
    if (stm)
    {
      printf("result 1-0 { checkmate }");
    }
    else if (!stm)
    {
      printf("result 0-1 { checkmate }");
    }
    
    return MOVENONE;
  }
  if (movecounter == 0 && !kic) 
  {
    printf("result 1/2-1/2 { stalemate }");
    return 0;
  }

  /* iterate through moves */
  for (i=0;i<movecounter;i++)
  {
    domove (board, moves[i]);
    score = -negamax(board, !stm, alpha, beta, 1);
    undomove (board, moves[i], lastmove, cr);

    if(score>=alpha)
    {
      alpha=score;
      bestmove = moves[i];
    }
  }
  return bestmove;
}
