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
#include "book.h"       /* for polyglot book access */
#include "eval.h"       /* for evalmove and eval */
#include "movegen.h"    /* for move generator thingies */
#include "timer.h"      /* for time measurement */
#include "types.h"      /* custom types, board defs, data structures, macros */
#include "zetadva.h"    /* for global vars */

/* forward declaration */
Score negamax(Bitboard *board, bool stm, Score alpha, Score beta, s32 depth, s32 ply, bool prune);

Score perft(Bitboard *board, bool stm, s32 depth)
{
  bool kic = false;
  Score boardscore = (Score)board[QBBSCORE];
  s32 i = 0;
  s32 movecounter = 0;
  Cr cr = board[QBBPMVD];
  Move lastmove = board[QBBLAST];
  Move moves[MAXMOVES];
  Hash hash = board[QBBHASH];

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
    undomove(board, moves[i], lastmove, cr, boardscore, hash);
  }
  return 0;
}
Score qsearch(Bitboard *board, bool stm, Score alpha, Score beta, s32 depth, s32 ply)
{
  bool kic = false;
  Score score;
  Score boardscore = (Score)board[QBBSCORE];
  s32 i = 0;
  s32 movecounter = 0;
  s32 movecounter_caps = 0;
  Cr cr = board[QBBPMVD];
  Move lastmove = board[QBBLAST];
  Move moves[MAXMOVES];
  Move moves_caps[MAXMOVES];
  Hash hash = board[QBBHASH];

  NODECOUNT++;

  kic = kingincheck(board, stm);
  /* get static, incremental board score */
  score = (stm)? -boardscore : boardscore;
  /* get full eval score 
  score = (stm)? -eval(board): eval(board);
  */
  /* stand pat */
  if(!kic&&score>=beta)
      return beta;
  if(!kic&&alpha<score)
      alpha = score;

  /* when king in check, all evasion moves */
  if (kic)
  {
    movecounter = genmoves_promo(board, moves, movecounter, stm);
    if(GETSQEP(lastmove))
      movecounter_caps = genmoves_enpassant(board, moves_caps, movecounter_caps, stm);
    if ((board[QBBPMVD]&SMCRALL))
      movecounter = genmoves_castles(board, moves, movecounter, stm);
    movecounter = genmoves_noncaptures(board, moves, movecounter, stm);
    movecounter_caps = genmoves_captures(board, moves_caps, movecounter_caps, stm);
    /* checkmate */
    if (kic&&movecounter==0&&movecounter_caps==0)
      return -INF+ply;
  }
  else
    movecounter_caps = genmoves_captures(board, moves_caps, movecounter_caps, stm);
  /* quiet leaf node, return  evaluation board score */
  if (!kic&&movecounter_caps==0)
    return (stm)? -eval(board): eval(board);
  /* sort moves */
  qsort(moves_caps, movecounter_caps, sizeof(Move), cmp_move_desc);
  /* iterate through moves */
  for (i=0;i<movecounter_caps;i++)
  {
    domove(board, moves_caps[i]);
    score = -qsearch(board, !stm, -beta, -alpha, depth-1, ply+1);
    undomove(board, moves_caps[i], lastmove, cr, boardscore, hash);

    if(score>=beta)
      return score;
    if(score>alpha)
      alpha=score;
  }
  return alpha;
}
Score negamax(Bitboard *board, bool stm, Score alpha, Score beta, s32 depth, s32 ply, bool prune)
{
  bool kic = false;
  bool ext = false;
  u8 type = FAILLOW;
  Score score = 0;
  s32 hmc = (s32)GETHMC(board[QBBLAST]);
  Score boardscore = (Score)board[QBBSCORE];
  s32 i = 0;
  s32 reduction = 0;
  s32 movecounter = 0;
  s32 legalmovecounter = 0;
  Cr cr = board[QBBPMVD];
  Move lastmove = board[QBBLAST];
  Move bestmove = MOVENONE;
  Move ttmove = MOVENONE;
  Hash hash = board[QBBHASH];
  Move moves[MAXMOVES];
  struct TTE *tt = NULL;

  kic = kingincheck(board, stm);

  /* time out? */  
  end = get_time();
  elapsed = end-start;
  if (elapsed>=MaxTime)
  {
    TIMEOUT=true;
    return 0;
  }
  HashHistory[PLY+ply] = hash;
  /* check for fifty move rule */
  if (hmc>=50)
    return DRAWSCORE;
  /* check for repetition */
  for (i=PLY+ply-2;i>=PLY+ply-hmc;i-=2)
  {
    if (HashHistory[i]==hash) 
      return DRAWSCORE;
  }
  /* search extension, checks and pawn promo */
  if(kic||(GETPTYPE(GETPFROM(lastmove))==PAWN&&GETPTYPE(GETPTO(lastmove))==QUEEN))
  {
    depth++;
    ext = true;
  }
  /* call quiescence search */
  if (depth <= 0)
    return qsearch(board, stm, alpha, beta, depth-1, ply+1);
  NODECOUNT++;
  /* null move pruning
  reduction = depth>6? 3:2;
  if (prune&&!kic&&!ext&&JUSTMOVE(lastmove)!=MOVENONE&&depth>2)
  {
    donullmove(board);
    score = -negamax(board, !stm, -beta, -alpha, depth-1-reduction, ply+1, false);
    undonullmove(board, lastmove, hash);
    if (score>=beta&&score)
      score = -negamax(board, !stm, -beta, -alpha, depth-1, ply+1, false);
    if (score>=beta&&score)
      return score;
  }
*/
  /* check transposition table */
  tt = load_from_tt(hash);
  if (tt&&tt->hash==hash) 
  {
    /* update and check bounds 
    if ((tt->depth+tt->ply)>(PLY+ply))
    {
      if (tt->flag==EXACTSCORE||tt->flag==FAILHIGH)
      {
        COUNTERS1++;
        alpha = MAX(alpha, tt->score);
      }
      if (tt->flag==EXACTSCORE||tt->flag==FAILLOW) 
      {
        COUNTERS2++;
        beta  = MIN(beta, tt->score);
      }
      if (alpha >= beta) return alpha;
    }
*/
    if (tt->flag>FAILLOW&&JUSTMOVE(tt->bestmove)!=MOVENONE) 
      ttmove = tt->bestmove;
  }
  /* generate pawn promo moves first */  
  movecounter = genmoves_promo(board, moves, 0, stm);
  legalmovecounter+= movecounter;
  /* iterate through moves */
  for (i=0;i<movecounter;i++)
  {
    domove(board, moves[i]);
    score = -negamax(board, !stm, -beta, -alpha, depth-1, ply+1, prune);
    undomove(board, moves[i], lastmove, cr, boardscore, hash);

    if(score>=beta)
    {
      save_to_tt(hash, moves[i], score, FAILHIGH, ply, PLY);
      return score;
    }
    if(score>alpha)
    {
      alpha=score;
      bestmove = moves[i];
      type = EXACTSCORE;
    }
  }
  /* generate capturing moves next */  
  movecounter = genmoves_captures(board, moves, 0, stm);
  legalmovecounter+= movecounter;
  for(i=0;i<movecounter;i++)
  {
    if (JUSTMOVE(ttmove)==JUSTMOVE(moves[i]))
    {
      moves[i] = SETSCORE(moves[i], (Move)(INF-10));
    }
  }
  /* sort moves */
  qsort(moves, movecounter, sizeof(Move), cmp_move_desc);
  /* iterate through moves */
  for (i=0;i<movecounter;i++)
  {
    domove(board, moves[i]);
    score = -negamax(board, !stm, -beta, -alpha, depth-1, ply+1, prune);
    undomove(board, moves[i], lastmove, cr, boardscore, hash);

    if(score>=beta)
    {
      save_to_tt(hash, moves[i], score, FAILHIGH, ply, PLY);
      return score;
    }
    if(score>alpha)
    {
      alpha=score;
      bestmove = moves[i];
      type = EXACTSCORE;
    }
  }
  /* generate pawn en passant moves next */  
  movecounter = genmoves_enpassant(board, moves, 0, stm);
  legalmovecounter+= movecounter;
  /* iterate through moves */
  for (i=0;i<movecounter;i++)
  {
    domove(board, moves[i]);
    score = -negamax(board, !stm, -beta, -alpha, depth-1, ply+1, prune);
    undomove(board, moves[i], lastmove, cr, boardscore, hash);

    if(score>=beta)
    {
      save_to_tt(hash, moves[i], score, FAILHIGH, ply, PLY);
      return score;
    }
    if(score>alpha)
    {
      alpha=score;
      bestmove = moves[i];
      type = EXACTSCORE;
    }
  }
  /* generate castle moves next */  
  movecounter = genmoves_castles(board, moves, 0, stm);
  legalmovecounter+= movecounter;
  /* iterate through moves */
  for (i=0;i<movecounter;i++)
  {
    domove(board, moves[i]);
    score = -negamax(board, !stm, -beta, -alpha, depth-1, ply+1, prune);
    undomove(board, moves[i], lastmove, cr, boardscore, hash);

    if(score>=beta)
    {
      save_to_tt(hash, moves[i], score, FAILHIGH, ply, PLY);
      return score;
    }
    if(score>alpha)
    {
      alpha=score;
      bestmove = moves[i];
      type = EXACTSCORE;
    }
  }
  /* generate quiet moves last */  
  movecounter = genmoves_noncaptures(board, moves, 0, stm);
  legalmovecounter+= movecounter;
  for(i=0;i<movecounter;i++)
  {
    if (JUSTMOVE(ttmove)==JUSTMOVE(moves[i]))
    {
      moves[i] = SETSCORE(moves[i], (Move)(INF-10));
    }
  }
  /* sort moves */
  qsort(moves, movecounter, sizeof(Move), cmp_move_desc);
  /* iterate through moves, caputres */
  reduction = 0;
  /* late move reductions 
  if (!kic&&!ext&&legalmovecounter>0)
    reduction = (depth>3&&legalmovecounter>3&&ply>3)?2:1;
*/
  for (i=0;i<movecounter;i++)
  {
    domove(board, moves[i]);
    score = -negamax(board, !stm, -beta, -alpha, depth-1-reduction, ply+1, prune);
    undomove(board, moves[i], lastmove, cr, boardscore, hash);

    if(score>=beta)
    {
      save_to_tt(hash, moves[i], score, FAILHIGH, ply, PLY);
      return score;
    }
    if(score>alpha)
    {
      alpha=score;
      bestmove = moves[i];
      type = EXACTSCORE;
    }
  }
  /* checkmate */
  if (legalmovecounter==0&&kic)
    return -INF+ply;
  /* stalemate */
  if (legalmovecounter==0&&!kic) 
    return STALEMATESCORE;

  save_to_tt(hash, bestmove, alpha, type, ply, PLY);
  return alpha;
}
Move rootsearch(Bitboard *board, bool stm, s32 depth)
{
  bool kic = false;
  Score score;
  Score boardscore = (Score)board[QBBSCORE];
  Score alpha;
  Score beta;
  s32 xboard_score;
  s32 i = 0;
  s32 pvcount = 0;
  s32 idf = 1;
  s32 movecounter = 0;
  Hash hash = board[QBBHASH];
  Cr cr = board[QBBPMVD];
  Move rootmove = MOVENONE;
  Move bestmove = MOVENONE;
  Move lastmove = board[QBBLAST];
  Move moves[MAXMOVES];
  Move pvmoves[MAXMOVES];
  struct TTE *tt = NULL;

  TIMEOUT   = false;
  NODECOUNT = 0;
  MOVECOUNT = 0;
  COUNTERS1       = 0;
  COUNTERS2       = 0;

  start = get_time(); /* start timer */

  HashHistory[PLY] = hash;

  kic = kingincheck(board, stm);
  movecounter = genmoves(board, moves, movecounter, stm, false);

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
  /* check for bookmove */
  rootmove = bookmove(board, stm);
  for (i=0;i<movecounter;i++)
  {
    if (rootmove!=MOVENONE&&JUSTMOVE(rootmove)==JUSTMOVE(moves[i]))
      return moves[i];
  }
  /* check transposition table */
  tt = load_from_tt(hash);
  if (tt&&tt->hash==hash&&tt->flag>FAILLOW) 
  {
    for(i=0;i<movecounter;i++)
    {
      if (JUSTMOVE(tt->bestmove)==JUSTMOVE(moves[i]))
        moves[i] = SETSCORE(moves[i], (Move)(INF-10));
    }
  }
  /* sort moves */
  qsort(moves, movecounter, sizeof(Move), cmp_move_desc);

  /* gui output */
  if (!xboard_mode&&!epd_mode)
      fprintf(stdout, "ply score time nodes pv\n");
  /* iterative deepening framework */
  do {
    if (TIMEOUT)
      break;

    alpha = -INF;
    beta  =  INF;

    /* first move, full window */
    domove(board, moves[0]);
    score = -negamax(board, !stm, -beta, -alpha, idf-1, 1, true);
    alpha=score;
    bestmove = moves[0];
    undomove(board, moves[0], lastmove, cr, boardscore, hash);
    moves[0] = SETSCORE(moves[0],(Move)score);
    /* iterate through moves */
    for (i=1;i<movecounter;i++)
    {
      if (TIMEOUT)
        break;
      domove(board, moves[i]);
      /* null window */
      score = -negamax(board, !stm, -alpha -1, -alpha, idf-1, 1, false);
      if (score>alpha)
        score = -negamax(board, !stm, -beta, -alpha, idf-1, 1, false);
      undomove(board, moves[i], lastmove, cr, boardscore, hash);

      if(score>alpha)
      {
        alpha=score;
        bestmove = moves[i];
        moves[i] = SETSCORE(moves[i],(Move)score);
      }
      else
        moves[i] = SETSCORE(moves[i],(Move)(score-i));
    }
    end = get_time(); /* stop timer */
    elapsed = end-start;
    /* sort moves */
    qsort(moves, movecounter, sizeof(Move), cmp_move_desc);

    if (!TIMEOUT)
    {
      rootmove = bestmove;
      save_to_tt(hash, rootmove, alpha, EXACTSCORE, idf, PLY);
    }
    /* gui output */
    if (!TIMEOUT&&(xboard_post||!xboard_mode)&&!epd_mode)
    {
      pvcount = collect_pv_from_hash(board, hash, pvmoves);
      /* xboard mate scores */
      xboard_score = (s32)alpha;
      xboard_score = (alpha<=-MATESCORE)?-100000-(INF+alpha):xboard_score;
      xboard_score = (alpha>=MATESCORE)?100000-(-INF+alpha):xboard_score;
      fprintf(stdout, "%d %d %d %llu ", idf, xboard_score, (s32)(elapsed/10), NODECOUNT);
      for (i=0;i<pvcount;i++)
      {
        printmovecan(pvmoves[i]);
        fprintf(stdout, " ");
      }
      fprintf(stdout, "\n");
    }
  } while (++idf<=depth&&elapsed*2<MaxTime);

printf("#COUNTERS1:%llu\n", COUNTERS1);
printf("#COUNTERS2:%llu\n", COUNTERS2);

  return rootmove;
}

