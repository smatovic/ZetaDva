/*
  Name:         Zeta Dva
  Description:  Amateur level chess engine
  Author:       Srdja Matovic <s.matovic@app26.de>
  Created at:   2011-01-15
  Updated at:   2016-09
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
#include <math.h>       /* for pow */

#include "bitboard.h"   /* for population count, pop_count */
#include "book.h"       /* for polyglot book access */
#include "eval.h"       /* for evalmove and eval */
#include "movegen.h"    /* for move generator thingies */
#include "timer.h"      /* for time measurement */
#include "types.h"      /* custom types, board defs, data structures, macros */
#include "zetadva.h"    /* for global vars */

/* forward declaration */
Score negamax(Bitboard *board,
              bool stm, 
              Score alpha, 
              Score beta, 
              s32 depth, 
              s32 ply, 
              bool prune);

/* perft, just node counting */
Score perft(Bitboard *board, bool stm, s32 depth)
{
  bool kic = false;
  Score boardscore = (Score)board[QBBSCORE];
  s32 i = 0;
  s32 movecounter = 0;
  Cr cr = board[QBBPMVD];
  Move lastmove = board[QBBLAST];
  Hash hash = board[QBBHASH];
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
    undomove(board, moves[i], lastmove, cr, boardscore, hash);
  }
  return 0;
}
/* quiscence search */
Score qsearch(Bitboard *board, 
              bool stm, 
              Score alpha, 
              Score beta, 
              s32 depth, 
              s32 ply)
{
  bool kic = false;
  Score score;
  Score boardscore = (Score)board[QBBSCORE];
  s32 i = 0;
  s32 movecounter = 0;
  Cr cr = board[QBBPMVD];
  Move lastmove = board[QBBLAST];
  Hash hash = board[QBBHASH];
  Move moves[MAXMOVES];

  /* time out? */
  end = get_time();
  elapsed = end-start;
  if (elapsed>=MaxTime-TIMESPARE)
  {
    TIMEOUT=true;
    return 0;
  }
  /* check internal ply limit */
  if (ply>=MAXPLY)
  {
    TIMEOUT=true;
    return 0;
  }

  NODECOUNT++;

  kic = kingincheck(board, stm);

  /* get full eval score */
  score = (stm)? -eval(board): eval(board);

  /* stand pat */
  if(!kic&&score>=beta)
      return score;
  if(!kic&&score>alpha)
      alpha = score;

  movecounter = genmoves_promo(board, moves, movecounter, stm);
  movecounter = genmoves_captures(board, moves, movecounter, stm);
  if(GETSQEP(lastmove))
    movecounter = genmoves_enpassant(board, moves, movecounter, stm);

  /* quiet leaf node, return evaluation board score */
  if (movecounter==0)
    return score;

  /* sort moves */
  qsort(moves, movecounter, sizeof(Move), cmp_move_desc);

  /* iterate through moves */
  for (i=0;i<movecounter;i++)
  {
    domove(board, moves[i]);
    score = -qsearch(board, !stm, -beta, -alpha, depth-1, ply+1);
    undomove(board, moves[i], lastmove, cr, boardscore, hash);

    if(score>=beta)
      return score;

    if(score>alpha)
      alpha=score;
  }
  return alpha;
}
/* internal iterative deepening */
Move iid(Bitboard *board, bool stm, Score alpha, Score beta, s32 depth, s32 ply)
{
  Score score = alpha;
  Score boardscore = (Score)board[QBBSCORE];
  s32 i = 0;
  s32 movecounter = 0;
  Cr cr = board[QBBPMVD];
  Move lastmove = board[QBBLAST];
  Move bestmove = MOVENONE;
  Hash hash = board[QBBHASH];
  Move moves[MAXMOVES];


  movecounter = genmoves(board, moves, 0, stm, false, 0);

  if (movecounter<=1)
      return MOVENONE;

  /* iterate through moves */
  for (i=0;i<movecounter; i++)
  {

    domove(board, moves[i]);
    score = -negamax(board, !stm, -beta, -alpha, depth-1, ply+1, false);
    undomove(board, moves[i], lastmove, cr, boardscore, hash);

    if (score>=beta)
      return moves[i];

    if (score>alpha)
    {
      alpha = score;
      bestmove = moves[i];
    }
  }
  return bestmove;
}
/* negamax, minimax with alpha-beta pruning and further extensions */
Score negamax(Bitboard *board,
              bool stm, 
              Score alpha, 
              Score beta, 
              s32 depth, 
              s32 ply, 
              bool prune)
{
  bool kic = false;
  bool ext = false;
  bool childkic;
  u8 type = FAILLOW;
  Score score = 0;
  Score boardscore = (Score)board[QBBSCORE];
  s32 hmc = (s32)GETHMC(board[QBBLAST]);
  s32 i = 0;
  s32 rdepth;
  s32 movecounter = 0;
  s32 movesplayed = 0;
  s32 legalmovecounter = 0;
  Cr cr = board[QBBPMVD];
  Move lastmove = board[QBBLAST];
  Move bestmove = MOVENONE;
  Move ttmove = MOVENONE;
  Hash hash = board[QBBHASH];
  struct TTE *tt = NULL;
  Move moves[MAXMOVES];

  kic = kingincheck(board, stm);

  /* time out? */  
  end = get_time();
  elapsed = end-start;
  if (elapsed>=MaxTime-TIMESPARE)
  {
    TIMEOUT=true;
    return 0;
  }
  /* check internal ply limit */
  if (ply>=MAXPLY)
  {
    TIMEOUT=true;
    return 0;
  }

  HashHistory[PLY+ply] = hash;

  /* check for 75 move rule */
  if (hmc>150)
    return DRAWSCORE;

  /* check for repetition */
  for (i=PLY+ply-2;i>=0&&i>=PLY+ply-hmc;i-=2)
    if (HashHistory[i]==hash) 
      return DRAWSCORE;

 	/* mate distance pruning
  alpha = (ISMATE(alpha))?MAX((-INF+ply), alpha):alpha;
  beta  = (ISMATE(beta))?MIN(-(-INF+ply), beta):beta;
  if (alpha >= beta)
    return alpha;
  */

  /* search extension, checks and pawn promo */
  if( kic
     ||(GETPTYPE(GETPFROM(lastmove))==PAWN&&GETPTYPE(GETPTO(lastmove))==QUEEN))
  {
    depth++;
    ext = true;
  }
  
  /* call quiescence search */
  if (depth<=0)
    return qsearch(board, stm, alpha, beta, depth, ply);

  /* razoring 
  if (!kic&&!ext&&depth==2)
  {
    score = (stm)? -boardscore : boardscore;
    if (score+EvalPieceValues[QUEEN]<alpha)
    {
      score = qsearch(board, stm, alpha, beta, 0, ply);
      if (score<=alpha)
        return score;
    }
  }
  */

  NODECOUNT++;

  /* null move pruning, Bruce Moreland style */
  rdepth = depth-2;
  if (prune
      &&!kic
      &&!ext
      &&JUSTMOVE(lastmove)!=NULLMOVE
      )
  {
    donullmove(board);
    score = -negamax(board, !stm, -beta, -beta+1, rdepth-1, ply+1, false);
    undonullmove(board, lastmove, hash);
    if (score>=beta)
      return score;
  }

  /* load transposition table */
  tt = load_from_tt(hash);

  /* check transposition table score bounds */
  if (tt
      &&tt->hash==hash
      &&(s32)tt->depth>depth
      &&!ISINF(tt->score)
      &&!ISMATE(tt->score)) 
  {
    if ((tt->flag==EXACTSCORE||tt->flag==FAILHIGH)&&!ISMATE(alpha))
      alpha = MAX(alpha, tt->score);
    if (alpha >= beta) return alpha;
  }

  /* get tt move */
  if (tt
      &&tt->hash==hash
      &&tt->flag>FAILLOW
      &&JUSTMOVE(tt->bestmove)!=MOVENONE) 
  {
    ttmove = ((Move)tt->bestmove)|(lastmove&SMHMC);
  }

  /* internal iterative deepening, get a bestmove anyway */
  if (JUSTMOVE(ttmove)==MOVENONE&&depth>5)
  {
      ttmove = iid(board, stm, -INF, INF, depth/5, ply);
      ttmove = (ttmove&CMHMC)|(lastmove&SMHMC);
      TTHITS--;
  }

  /* check tt move first */
  if (JUSTMOVE(ttmove)!=MOVENONE
      &&GETPFROM(ttmove)==GETPIECE(board,(GETSQFROM(ttmove)))
      &&GETPCPT(ttmove)==GETPIECE(board,(GETSQCPT(ttmove)))
    )
  {
    TTHITS++;
    domove(board, ttmove);
    if (isvalid(board))
    {
      score = -negamax(board, !stm, -beta, -alpha, depth-1, ply+1, prune);

      if(score>=beta)
      {
        if (GETPCPT(ttmove)==PNONE&&prune)
        {
          Counters[GETSQFROM(lastmove)*64+GETSQTO(lastmove)] = JUSTMOVE(ttmove);
          Killers[ply] = JUSTMOVE(ttmove);
        }
        if (prune)
          save_to_tt(hash, (TTMove)(ttmove&SMTTMOVE), score, FAILHIGH, depth);
        undomove(board, ttmove, lastmove, cr, boardscore, hash);
        return score;
      }
      if(score>alpha)
      {
        alpha=score;
        bestmove = ttmove;
        type = EXACTSCORE;
      }

      movesplayed++;
    }
    undomove(board, ttmove, lastmove, cr, boardscore, hash);
  }

  /* generate capturing moves and pawn promotion */
  movecounter = genmoves_promo(board, moves, 0, stm);
  movecounter = genmoves_captures(board, moves, movecounter, stm);
  if(GETSQEP(lastmove))
    movecounter = genmoves_enpassant(board, moves, movecounter, stm);
  legalmovecounter+= movecounter;

  /* sort moves */
  qsort(moves, movecounter, sizeof(Move), cmp_move_desc);

  /* iterate through moves */
  for (i=0;i<movecounter;i++)
  {
    if (ttmove&&
        JUSTMOVE(ttmove)==JUSTMOVE(moves[i]))
      continue;

    domove(board, moves[i]);

    score = -negamax(board, !stm, -beta, -alpha, depth-1, ply+1, prune);

    undomove(board, moves[i], lastmove, cr, boardscore, hash);

    if(score>=beta)
    {
      if (prune)
        save_to_tt(hash, (TTMove)(moves[i]&SMTTMOVE), score, FAILHIGH, depth);
      return score;
    }

    if(score>alpha)
    {
      alpha=score;
      bestmove = moves[i];
      type = EXACTSCORE;
    }
    movesplayed++;
  }

  /* generate quiet moves */  
  movecounter = genmoves_noncaptures(board, moves, 0, stm, ply);
  if (cr&SMCRALL)
    movecounter = genmoves_castles(board, moves, movecounter, stm);
  legalmovecounter+= movecounter;

  /* sort moves */
  qsort(moves, movecounter, sizeof(Move), cmp_move_desc);

  /* iterate through moves, noncaputres */
  for (i=0;i<movecounter;i++)
  {
    if (ttmove&&
        JUSTMOVE(ttmove)==JUSTMOVE(moves[i]))
      continue;

    domove(board, moves[i]);

    childkic = kingincheck(board,!stm);

    /* futility pruning */
    score = (stm)? -boardscore : boardscore;
    if (depth==1
        &&!kic
        &&!ext
        &&movesplayed>0
        &&!childkic
        &&!(GETPTYPE(GETPFROM(moves[i]))==PAWN&&GETPTYPE(GETPTO(moves[i]))==QUEEN)
        &&score+EvalPieceValues[BISHOP]<alpha
       )
    {
      undomove(board, moves[i], lastmove, cr, boardscore, hash);
      continue;
    }

    /* late move reductions */
    rdepth = depth;
    if (!kic
        &&!ext
        &&movesplayed>0
        &&!childkic
        &&popcount(board[QBBBLACK])>=2
        &&popcount(board[QBBBLACK]^(board[QBBP1]|board[QBBP2]|board[QBBP3]))>=2
       )
    {
      rdepth = depth-1;
    }

    score = -negamax(board, !stm, -beta, -alpha, rdepth-1, ply+1, prune);

    /* late move reductions, research */
    if (rdepth!=depth&&
        score>alpha)
    {
      score = -negamax(board, !stm, -beta, -alpha, depth-1, ply+1, prune);
    }

    undomove(board, moves[i], lastmove, cr, boardscore, hash);

    if(score>=beta)
    {
      if (prune)
      {
        Counters[GETSQFROM(lastmove)*64+GETSQTO(lastmove)] = JUSTMOVE(moves[i]);
        Killers[ply] = JUSTMOVE(moves[i]);
        save_to_tt(hash, (TTMove)(moves[i]&SMTTMOVE), score, FAILHIGH, depth);
      }
      return score;
    }

    if(score>alpha)
    {
      alpha=score;
      bestmove = moves[i];
      type = EXACTSCORE;
    }
    movesplayed++;
  }
  /* checkmate */
  if (kic&&legalmovecounter==0)
    return -INF+ply;
  /* stalemate */
  if (!kic&&legalmovecounter==0) 
    return STALEMATESCORE;

  if (type>FAILLOW&&prune)
    save_to_tt(hash, (TTMove)(bestmove&SMTTMOVE), alpha, type, depth);
  return alpha;
}
Move rootsearch(Bitboard *board, bool stm, s32 depth)
{
  bool kic = false;
  Score score;
  Score alpha;
  Score beta;
  Score boardscore = (Score)board[QBBSCORE];
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
  struct TTE *tt = NULL;
  Move moves[MAXMOVES];
  Move pvmoves[MAXMOVES];

  TIMEOUT   = false;
  NODECOUNT = 0;
  MOVECOUNT = 0;
  TTHITS    = 0;
  COUNTERS1 = 0;
  COUNTERS2 = 0;

  start = get_time(); /* start timer */

  HashHistory[PLY] = hash;

  kic = kingincheck(board, stm);
  movecounter = genmoves(board, moves, movecounter, stm, false, PLY);

  /* checkmate and stalemate */
  if (movecounter==0&&kic)
    return MOVENONE;
  if (movecounter==0&&!kic) 
    return MOVENONE;
  /* check for bookmove */
  rootmove = bookmove(board, stm);
  for (i=0;i<movecounter;i++)
  {
    if (rootmove!=MOVENONE&&
        JUSTMOVE(rootmove)==JUSTMOVE(moves[i]))
      return moves[i];
  }
  /* check transposition table */
  tt = load_from_tt(hash);
  if (tt&&
      tt->hash==hash&&
      tt->flag>FAILLOW) 
  {
    for(i=0;i<movecounter;i++)
    {
      if (JUSTMOVE((Move)tt->bestmove)==JUSTMOVE(moves[i]))
      {
        moves[i] = SETSCORE(moves[i], (Move)(INF));
      }
    }
  }

  /* sort moves */
  qsort(moves, movecounter, sizeof(Move), cmp_move_desc);

  /* get a rootmove anyway*/
  rootmove = moves[0];

  /* gui output */
  if (!xboard_mode)
      fprintf(stdout, "ply score time nodes pv\n");
  /* iterative deepening framework */
  do {

    alpha = -INF;
    beta  =  INF;

    /* iterate through moves */
    for (i=0;i<movecounter;i++)
    {
      domove(board, moves[i]);

      score = -negamax(board, !stm, -beta, -alpha, idf-1, 1, true);

      undomove(board, moves[i], lastmove, cr, boardscore, hash);

      if(score>alpha)
      {
        alpha=score;
        bestmove = moves[i];
        moves[i] = SETSCORE(moves[i],(Move)score);
      }
      else
        moves[i] = SETSCORE(moves[i],(Move)(score-i));

      if (TIMEOUT)
        break;
    }

    end = get_time(); /* stop timer */
    elapsed = end-start;

    if (!TIMEOUT)
    {
      rootmove = bestmove;
      save_to_tt(hash, (TTMove)(rootmove&SMTTMOVE), alpha, EXACTSCORE, idf);
      /* sort moves */
      qsort(moves, movecounter, sizeof(Move), cmp_move_desc);
    }

    /* gui output */
    if (!TIMEOUT&&(xboard_post||!xboard_mode))
    {
      pvcount = collect_pv_from_hash(board, hash, pvmoves, idf-1);
      /* xboard mate scores */
      xboard_score = (s32)alpha;
      xboard_score = (alpha<=-MATESCORE)?-100000-(INF+alpha):xboard_score;
      xboard_score = (alpha>=MATESCORE)?100000-(-INF+alpha):xboard_score;
      fprintf(stdout, "%d %d %d %" PRIu64 " ", idf, xboard_score, (s32)(elapsed/10), NODECOUNT);
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile, "%d %d %d %" PRIu64 " ", idf, xboard_score, (s32)(elapsed/10), NODECOUNT);
      }
      for (i=0;i<pvcount;i++)
      {
        printmovecan(pvmoves[i]);
        fprintf(stdout, " ");
        if (LogFile)
          fprintf(LogFile, " ");
      }
      fprintf(stdout, "\n");
      if (LogFile)
        fprintf(LogFile, "\n");
    }
  } while (++idf<=depth&&elapsed*2<MaxTime&&!TIMEOUT&&idf<=MAXPLY);

  if ((!xboard_mode)||xboard_debug)
  {
    fprintf(stdout,"#%" PRIu64 " searched nodes in %lf seconds, with %" PRIu64 " tthits, ebf: %lf, nps: %" PRIu64 " \n", NODECOUNT, elapsed/1000, TTHITS, (double)pow(NODECOUNT, (double)1/idf), (u64)(NODECOUNT/(elapsed/1000)));
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile,"#%" PRIu64 " searched nodes in %lf seconds, with %" PRIu64 " tthits, ebf: %lf, nps: %" PRIu64 " \n", NODECOUNT, elapsed/1000, TTHITS, (double)pow(NODECOUNT, (double)1/idf), (u64)(NODECOUNT/(elapsed/1000)));
    }
  }

  return rootmove;
}

