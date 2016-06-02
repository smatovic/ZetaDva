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
#include <stdlib.h>     /* for qsort */

#include "bitboard.h"   /* for population count, pop_count */
#include "eval.h"       /* for evalmove and eval */
#include "movegen.h"    /* for move generator thingies */
#include "types.h"      /* custom types, board defs, data structures, macros */
#include "zetadva.h"    /* for global vars */

/* forward declaration */
Score negamax(Bitboard *board, bool stm, Score alpha, Score beta, s32 depth);

Score perft(Bitboard *board, bool stm, s32 depth)
{
  bool kic = false;
  Score boardscore = (Score)board[QBBSCORE];
  int i = 0;
  int movecounter = 0;
  Cr cr = board[QBBPMVD];
  Move lastmove = board[QBBLAST];
  Move moves[MAXMOVES];

  kic = kingincheck(board, stm);
  /* leaf node, count */
  if (depth == 0)
  {
    NODECOUNT++;
    return 0;
  }
  movecounter = genmoves_general(board, moves, movecounter, stm, false);
  /* checkmate */
  if (movecounter == 0 && kic)
  {
    return 0;
  }
  /* stalemate */
  if (movecounter == 0 && !kic) 
  {
    return 0;
  }
  /* iterate through moves */
  for (i=0;i<movecounter;i++)
  {
    domove(board, moves[i]);
    perft(board, !stm, depth-1);
    undomove(board, moves[i], lastmove, cr, boardscore);
  }
  return 0;
}
Score qsearch(Bitboard *board, bool stm, Score alpha, Score beta, s32 depth)
{
  bool kic = false;
  Score score;
  Score boardscore = (Score)board[QBBSCORE];
  int i = 0;
  int movecounter = 0;
  Cr cr = board[QBBPMVD];
  Move lastmove = board[QBBLAST];
  Move moves[MAXMOVES];

  NODECOUNT++;

  kic = kingincheck(board, stm);
/*
  score = eval(board);
  score = (stm)? -score : score;
*/
  /* get static, incremental board score */
  score = (stm)? -boardscore : boardscore;

  /* stand pat */
  if( !kic && score >= beta )
      return beta;
  if( !kic && alpha < score )
      alpha = score;

  /* when king in check, all evasion moves */
  if (kic)
    movecounter = genmoves(board, moves, movecounter, stm, false);
  else
  {
    movecounter = genmoves_enpassant(board, moves, movecounter, stm);
    movecounter = genmoves_captures(board, moves, movecounter, stm);
  }
  /* sort moves */
  qsort(moves, movecounter, sizeof(Move), cmp_move_desc);
  /* checkmate */
  if (movecounter==0&&kic)
    return -INF+PLY;
  /* quiet leaf node, return  evaluation board score */
  if (movecounter==0)
  {
    score = eval(board);
    score = (stm)? -score : score;  /* consider negated scores for black */
    return score;
  }
  /* iterate through moves */
  for (i=0;i<movecounter;i++)
  {
    domove(board, moves[i]);
    score = -qsearch(board, !stm, -beta, -alpha, depth-1);
    undomove(board, moves[i], lastmove, cr, score);

    if(score>=beta)
      return score;

    if(score>alpha)
      alpha=score;
  }
  return alpha;
}
Score negamax(Bitboard *board, bool stm, Score alpha, Score beta, s32 depth)
{
  bool kic = false;
  Score score = 0;
  Score boardscore = (Score)board[QBBSCORE];
  int i = 0;
  int movecounter = 0;
  int legalmovecounter = 0;
  Cr cr = board[QBBPMVD];
  Move lastmove = board[QBBLAST];
  Move moves[MAXMOVES];

  kic = kingincheck(board, stm);

  /* search extension, checks and pawn promo */
  if(depth==0&&kic)
    depth++;

  if (depth == 0)
    return qsearch(board, stm, alpha, beta, depth-1);

  NODECOUNT++;

  /* generate pawn promo moves first */  
  movecounter = genmoves_promo(board, moves, 0, stm);
  legalmovecounter+= movecounter;
  /* iterate through moves */
  for (i=0;i<movecounter;i++)
  {
    domove(board, moves[i]);
    score = -negamax(board, !stm, -beta, -alpha, depth-1);
    undomove(board, moves[i], lastmove, cr, boardscore);

    if(score>=beta)
      return score;

    if(score>alpha)
      alpha=score;
  }
  /* generate capturing moves next */  
  movecounter = genmoves_captures(board, moves, 0, stm);
  legalmovecounter+= movecounter;
  /* sort moves */
  qsort(moves, movecounter, sizeof(Move), cmp_move_desc);
  /* iterate through moves */
  for (i=0;i<movecounter;i++)
  {
    domove(board, moves[i]);
    score = -negamax(board, !stm, -beta, -alpha, depth-1);
    undomove(board, moves[i], lastmove, cr, boardscore);

    if(score>=beta)
      return score;

    if(score>alpha)
      alpha=score;
  }
  /* generate pawn en passant moves next */  
  movecounter = genmoves_enpassant(board, moves, 0, stm);
  legalmovecounter+= movecounter;
  /* iterate through moves */
  for (i=0;i<movecounter;i++)
  {
    domove(board, moves[i]);
    score = -negamax(board, !stm, -beta, -alpha, depth-1);
    undomove(board, moves[i], lastmove, cr, boardscore);

    if(score>=beta)
      return score;

    if(score>alpha)
      alpha=score;
  }
  /* generate castle moves next */  
  movecounter = genmoves_castles(board, moves, 0, stm);
  legalmovecounter+= movecounter;
  /* iterate through moves, caputres */
  for (i=0;i<movecounter;i++)
  {
    domove(board, moves[i]);
    score = -negamax(board, !stm, -beta, -alpha, depth-1);
    undomove(board, moves[i], lastmove, cr, boardscore);

    if(score>=beta)
      return score;

    if(score>alpha)
      alpha=score;
  }
  /* generate quiet moves last */  
  movecounter = genmoves_noncaptures(board, moves, 0, stm);
  legalmovecounter+= movecounter;
  /* sort moves */
  qsort(moves, movecounter, sizeof(Move), cmp_move_desc);
  /* iterate through moves, caputres */
  for (i=0;i<movecounter;i++)
  {
    domove(board, moves[i]);
    score = -negamax(board, !stm, -beta, -alpha, depth-1);
    undomove(board, moves[i], lastmove, cr, boardscore);

    if(score>=beta)
      return score;

    if(score>alpha)
      alpha=score;
  }
  /* checkmate */
  if (legalmovecounter == 0 && kic)
    return -INF+PLY;
  /* stalemate */
  if (legalmovecounter == 0 && !kic) 
    return STALEMATESCORE;

  return alpha;
}
Move rootsearch(Bitboard *board, bool stm, s32 depth)
{
  bool kic = false;
  Score score;
  Score boardscore = (Score)board[QBBSCORE];
  Score alpha = -INF;
  Score beta  =  INF;
  int i = 0;
  int movecounter = 0;
  Cr cr = board[QBBPMVD];
  Move bestmove = MOVENONE;
  Move lastmove = board[QBBLAST];
  Move moves[MAXMOVES];

  kic = kingincheck(board, stm);
  movecounter = genmoves(board, moves, movecounter, stm, false);
  /* sort moves */
  qsort(moves, movecounter, sizeof(Move), cmp_move_desc);

  /* print checkmate and stalemate result */
  if (movecounter==0&&kic)
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
  if (movecounter==0&&!kic) 
  {
    printf("result 1/2-1/2 { stalemate }");
    return MOVENONE;
  }
  /* first move, full window */
  domove(board, moves[0]);
  score = -negamax(board, !stm, -beta, -alpha, depth-1);
  alpha=score;
  bestmove = moves[0];
  undomove(board, moves[0], lastmove, cr, boardscore);
  /* iterate through moves */
  for (i=1;i<movecounter;i++)
  {
    domove(board, moves[i]);
    /* null window, pvs */
    score = -negamax(board, !stm, -alpha-1, -alpha, depth-1);
    if (score>alpha)
      score = -negamax(board, !stm, -beta, -alpha, depth-1);
    undomove(board, moves[i], lastmove, cr, boardscore);

    if(score>alpha)
    {
      alpha=score;
      bestmove = moves[i];
    }
  }
  return bestmove;
}

