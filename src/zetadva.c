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
#include <stdlib.h>     /* for malloc free */
#include <string.h>     /* for string compare */ 
#include <getopt.h>     /* for getopt_long */

#include "book.h"       /* for polyglot book access */
#include "bitboard.h"   /* for population count, pop_count */
#include "eval.h"       /* for evalmove and eval */
#include "movegen.h"    /* for move generator thingies */
#include "search.h"     /* for search algorithms */
#include "timer.h"      /* for time measurement */
#include "types.h"      /* custom types, board defs, data structures, macros */

/* global variables */
FILE *LogFile = NULL;        /* logfile for debug */
char *Line;                   /* for fgetting the input on stdin */
char *Command;                /* for pasring the xboard command */
char *Fen;                    /* for storing the fen chess baord string */
/* counters */
u64 NODECOUNT       = 0;
u64 MOVECOUNT       = 0;
u64 COUNTERS1       = 0;
u64 COUNTERS2       = 0;
/* xboard flags */
bool xboard_mode    = false;  /* chess GUI sets to true */
bool xboard_force   = false;  /* if true aplly only moves, do not think */
bool xboard_post    = false;  /* post search thinking output */
bool xboard_san     = false;  /* use san move notation instead of can */
bool xboard_time    = false;  /* use xboards time command for time management */
bool xboard_debug   = false;  /* print debug information */
s32 xboardmb        = 64;     /* mega bytes for hash table */
/* timers */
double start        = 0;
double end          = 0;
double elapsed      = 0;
bool TIMEOUT        = false;  /* global value for time control*/
/* time control in milli-seconds */
s32 timemode    = 0;  /* 0 = single move, 1 = conventional clock, 2 = ics clock */
s32 MovesLeft   = 1;  /* moves left unit nex time increase */
s32 MaxMoves    = 1;  /* moves to play in time frame */
double TimeInc  = 0;  /* time increase */
double TimeLeft = 5*1000;  /* overall time on clock, 5s default */
double MaxTime  = 5*1000;  /* max time per move */
/* game state */
bool STM            = WHITE; /* site to move */
s32 SD              = MAXPLY;/* max search depth*/
s32 GAMEPLY         = 0;     /* total ply, considering depth via fen string */
s32 PLY             = 0;     /* engine specifix ply counter */
Move *MoveHistory;           /* last game moves indexed by ply */
Hash *HashHistory;           /* last game hashes indexed by ply */
Score *ScoreHistory;          /* last board scores indexed by ply */
Cr *CRHistory;             /* last board castle rights indexed by ply */
Move *Killers;               /* killer move heuristic */
Move *Counters;              /* counter move heuristic */
/* Quad Bitboard */
/* based on http://chessprogramming.wikispaces.com/Quad-Bitboards */
/* by Gerd Isenberg */
Bitboard BOARD[8];
/* quad bitboard array index definition
  0   pieces white
  1   piece type first bit
  2   piece type second bit
  3   piece type third bit
  5   piece moved flags, for castle rights
  6   64 bit board Zobrist hash
  7   lastmove + ep target + halfmove clock + move score
*/
const Bitboard LRANK[2] =
{
  BBRANK7,
  BBRANK2
};
/* forward declarations */
static void print_help(void);
static void print_version(void);
static void selftest(void);
static bool setboard(Bitboard *board, char *fenstring);
static void createfen(char *fenstring, Bitboard *board, bool stm, s32 gameply);
static void move2can(Move move, char *movec);
static Move can2move(char *usermove, Bitboard *board, bool stm);
void printboard(Bitboard *board);
void printbitboard(Bitboard board);
/* transposition hash table */
struct TTE *TT = NULL;
u64 ttbits = 0;

/* release memory, files and tables */
static bool release_inits(void)
{
  /* close log file */
  if (LogFile)
    fclose (LogFile);
  /* release memory */
  if (Line) 
    free(Line);
  if (Command) 
    free(Command);
  if (Fen) 
    free(Fen);
  if (MoveHistory) 
    free(MoveHistory);
  if (HashHistory) 
    free(HashHistory);
  if (ScoreHistory) 
    free(ScoreHistory);
  if (CRHistory) 
    free(CRHistory);
  if (TT) 
    free(TT);
  if (Counters) 
    free(Counters);
  if (Killers) 
    free(Killers);

  bookclose();

  return true;
}
/* compute zobrist hash from position */
Hash computehash(Bitboard *board, bool stm)
{
  Piece piece;
  Bitboard bbWork;
  Square sq;
  u64 hash = HASHNONE;
  u8 side;

  /* for each color */
  for (side=WHITE;side<=BLACK;side++)
  {
    bbWork = (side==BLACK)?board[QBBBLACK]:(board[QBBBLACK]^(board[QBBP1]|board[QBBP2]|board[QBBP3]));
    /* for each piece */
    while(bbWork)
    {
      sq    = popfirst1(&bbWork);
      piece = GETPIECE(board,sq);
      piece = GETPTYPE(piece)-1;
      hash ^= Random64[side*6*64+piece*64+sq];
    }
  }
  /* castle rights */
  if (((~board[QBBPMVD])&SMCRWHITEK)==SMCRWHITEK)
      hash ^= RandomCastle[0];
  if (((~board[QBBPMVD])&SMCRWHITEQ)==SMCRWHITEQ)
      hash ^= RandomCastle[1];
  if (((~board[QBBPMVD])&SMCRBLACKK)==SMCRBLACKK)
      hash ^= RandomCastle[2];
  if (((~board[QBBPMVD])&SMCRBLACKQ)==SMCRBLACKQ)
      hash ^= RandomCastle[3];

  /* en passant */
  sq  = GETSQEP(board[QBBLAST]); 
  if (sq)
    hash ^= RandomEnPassant[GETFILE(sq)]; 

  /* side to move */
  if (!stm)
      hash^=RandomTurn[0];

  return hash;
}
/* initialize transposition and furter tables */
static void initTT(void) 
{
  u64 mem = (xboardmb*1024*1024)/(sizeof(struct TTE));

  ttbits = 0;
  while ( mem >>= 1)   /* get msb */
    ttbits++;
  mem = 1ULL<<ttbits;   /* get number of tt entries */
  ttbits=mem;
  if (TT)
    free(TT);
  TT = (struct TTE*)calloc(mem,sizeof(struct TTE));
  if (!TT)
    fprintf(stdout,"Error (hash table memory allocation, %u mb, failed): memory", xboardmb);
  if (Killers)
    free(Killers);
  Killers = (Move*)calloc(MAXPLY*2,sizeof(Killers));
  if (!Killers)
    fprintf(stdout,"Error (Killers table memory allocation failed)");
  if (Counters)
    free(Counters);
  Counters = (Move*)calloc(64*64,sizeof(Killers));
  if (!Counters)
    fprintf(stdout,"Error (Counters table memory allocation failed)");
}
/* save entry to hash transposition table */
void save_to_tt(Hash hash, Move move, Score score, u8 flag, s32 depth)
{
  struct TTE *tete;

  /* exit when timeout */
  if (TIMEOUT)
    return;

  /* bucket one, always replace */
  tete = &TT[(hash&(ttbits-1))^0];
  tete->hash      = hash;
  tete->bestmove  = move;
  tete->score     = score;
  tete->flag      = flag;
  tete->depth     = depth;

  /* bucket two, depth based */
  tete = &TT[(hash&(ttbits-1))^1];
  if (depth>tete->depth)
  {
    tete->hash      = hash;
    tete->bestmove  = move;
    tete->score     = score;
    tete->flag      = flag;
    tete->depth     = depth;
  }
}
/* load entry from via zobrist hash from transposition table */
struct TTE *load_from_tt(Hash hash)
{
  struct TTE *tete;

  tete = &TT[(hash&(ttbits-1))^1];
  if (tete->hash==hash)
    return tete;

  tete = &TT[(hash&(ttbits-1))^0];
  if (tete->hash==hash)
    return tete;

  return NULL;
}
void save_killer(Move move, Score score, s32 ply)
{
  if(score>(Score)GETSCORE(Killers[ply*2+0]))
    Killers[ply*2+0] = SETSCORE(move, (Move)score);
  else
    Killers[ply*2+1] = SETSCORE(move, (Move)score);
}
/* innitialize memory, files and tables */
static bool inits(void)
{
  /* memory allocation */
  Line         = (char *)calloc(1024       , sizeof (char));
  Command      = (char *)calloc(1024       , sizeof (char));
  Fen          = (char *)calloc(1024       , sizeof (char));
  MoveHistory = (Move *)calloc(MAXGAMEPLY , sizeof (Move));
  HashHistory = (Hash *)calloc(MAXGAMEPLY , sizeof (Hash));
  ScoreHistory = (Score *)calloc(MAXGAMEPLY , sizeof (Score));
  CRHistory = (Cr *)calloc(MAXGAMEPLY , sizeof (Cr));

  if (!Line) 
  {
    fprintf(stdout,"Error (memory allocation failed): char Line[%d]", 1024);
    return false;
  }
  if (!Command) 
  {
    fprintf(stdout,"Error (memory allocation failed): char Command[%d]", 1024);
    return false;
  }
  if (!Fen) 
  {
    fprintf(stdout,"Error (memory allocation failed): char Fen[%d]", 1024);
    return false;
  }
  if (!MoveHistory) 
  {
    fprintf(stdout,"Error (memory allocation failed): u64 MoveHistory[%d]",
             MAXGAMEPLY);
    return false;
  }
  if (!HashHistory) 
  {
    fprintf(stdout,"Error (memory allocation failed): u64 HashHistory[%d]",
            MAXGAMEPLY);
    return false;
  }
  if (!ScoreHistory) 
  {
    fprintf(stdout,"Error (memory allocation failed): Score ScoreHistory[%d]",
            MAXGAMEPLY);
    return false;
  }
  if (!CRHistory) 
  {
    fprintf(stdout,"Error (memory allocation failed): Cr CrHistory[%d]",
            MAXGAMEPLY);
    return false;
  }

  bookopen();

/* init pawn attack tables
  Square sq =0;
  Bitboard attacksw[64];
  Bitboard attacksb[64];

  for (sq=0;sq<64;sq++)
  {
    attacksw[sq] = BBEMPTY;
    attacksb[sq] = BBEMPTY;
  }
  for (sq=0;sq<64;sq++)
  {
    if (GETFILE(sq)>FILE_A&&GETRANK(sq)<RANK_8)
      attacksw[sq] |= SETMASKBB(sq)<<7;
    if (GETFILE(sq)<FILE_H&&GETRANK(sq)<RANK_8)
      attacksw[sq] |= SETMASKBB(sq)<<9;
  }
  for (sq=0;sq<64;sq++)
  {
    if (GETFILE(sq)<FILE_H&&GETRANK(sq)>RANK_1)
      attacksb[sq] |= SETMASKBB(sq)>>7;
    if (GETFILE(sq)>FILE_A&&GETRANK(sq)>RANK_1)
      attacksb[sq] |= SETMASKBB(sq)>>9;
  }

  for (sq=0;sq<64;sq++)
  {
    printf("0x%llx,", attacksb[sq]);
  }
*/
  return true;
}
int cmp_move_desc(const void *ap, const void *bp)
{
    const Move *a = ap;
    const Move *b = bp;

    return (Score)GETSCORE(*b) - (Score)GETSCORE(*a);
}
/* apply null-move on board */
void donullmove(Bitboard *board)
{
  /* consider color */
  board[QBBHASH] ^=RandomTurn[0];
  /* clear en passant */
  if (GETSQEP(board[QBBLAST]))
    board[QBBHASH] ^= RandomEnPassant[GETFILE(GETSQEP(board[QBBLAST]))]; 

  board[QBBLAST] = MOVENONE|(CMMOVE&board[QBBLAST]);
}
/* restore board again after nullmove */
void undonullmove(Bitboard *board, Move lastmove, Hash hash)
{
  board[QBBHASH] = hash;
  board[QBBLAST] = lastmove;
}
/* apply move on board, quick during move generation */
void domovequick(Bitboard *board, Move move)
{
  Square sqfrom   = GETSQFROM(move);
  Square sqto     = GETSQTO(move);
  Square sqcpt    = GETSQCPT(move);
  Piece pto       = GETPTO(move);
  Bitboard bbTemp = BBEMPTY;

  /* check for edges */
  if (move==MOVENONE)
    return;

  /* unset square from, square capture and square to */
  bbTemp = CLRMASKBB(sqfrom)&CLRMASKBB(sqcpt)&CLRMASKBB(sqto);
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;

  /* set piece to */
  board[QBBBLACK] |= (pto&0x1)<<sqto;
  board[QBBP1]    |= ((pto>>1)&0x1)<<sqto;
  board[QBBP2]    |= ((pto>>2)&0x1)<<sqto;
  board[QBBP3]    |= ((pto>>3)&0x1)<<sqto;
}
/* restore board again, quick during move generation */
void undomovequick(Bitboard *board, Move move)
{
  Square sqfrom   = GETSQFROM(move);
  Square sqto     = GETSQTO(move);
  Square sqcpt    = GETSQCPT(move);
  Piece pfrom     = GETPFROM(move);
  Piece pcpt      = GETPCPT(move);
  Bitboard bbTemp = BBEMPTY;

  /* check for edges */
  if (move==MOVENONE)
    return;

  /* unset square capture, square to */
  bbTemp = CLRMASKBB(sqcpt)&CLRMASKBB(sqto);
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;

  /* restore piece capture */
  board[QBBBLACK] |= (pcpt&0x1)<<sqcpt;
  board[QBBP1]    |= ((pcpt>>1)&0x1)<<sqcpt;
  board[QBBP2]    |= ((pcpt>>2)&0x1)<<sqcpt;
  board[QBBP3]    |= ((pcpt>>3)&0x1)<<sqcpt;

  /* restore piece from */
  board[QBBBLACK] |= (pfrom&0x1)<<sqfrom;
  board[QBBP1]    |= ((pfrom>>1)&0x1)<<sqfrom;
  board[QBBP2]    |= ((pfrom>>2)&0x1)<<sqfrom;
  board[QBBP3]    |= ((pfrom>>3)&0x1)<<sqfrom;
}
/* apply move on board */
void domove(Bitboard *board, Move move)
{
  Score boardscore;
  Score score     = 0;
  Square sqfrom   = GETSQFROM(move);
  Square sqto     = GETSQTO(move);
  Square sqcpt    = GETSQCPT(move);
  Piece pfrom     = GETPFROM(move);
  Piece pto       = GETPTO(move);
  Piece pcpt      = GETPCPT(move);
  Bitboard bbTemp = BBEMPTY;
  Piece pcastle   = PNONE;
  u64 hmc         = GETHMC(board[QBBLAST]);

  /* check for edges */
  if (move==MOVENONE)
    return;

  /* increase half move clock */
  hmc++;

  /* do hash increment , clear old */
  /* castle rights */
  if(((~board[QBBPMVD])&SMCRWHITEK)==SMCRWHITEK)
    board[QBBHASH] ^= RandomCastle[0];
  if(((~board[QBBPMVD])&SMCRWHITEQ)==SMCRWHITEQ)
    board[QBBHASH] ^= RandomCastle[1];
  if(((~board[QBBPMVD])&SMCRBLACKK)==SMCRBLACKK)
    board[QBBHASH] ^= RandomCastle[2];
  if(((~board[QBBPMVD])&SMCRBLACKQ)==SMCRBLACKQ)
    board[QBBHASH] ^= RandomCastle[3];
  /* file en passant */
  if (GETSQEP(board[QBBLAST]))
    board[QBBHASH] ^= RandomEnPassant[GETFILE(GETSQEP(board[QBBLAST]))]; 
  if (GETCOLOR(pfrom)==WHITE)
      board[QBBHASH] ^=RandomTurn[0];

  /* unset square from, square capture and square to */
  bbTemp = CLRMASKBB(sqfrom)&CLRMASKBB(sqcpt)&CLRMASKBB(sqto);
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;

  /* set piece to */
  board[QBBBLACK] |= (pto&0x1)<<sqto;
  board[QBBP1]    |= ((pto>>1)&0x1)<<sqto;
  board[QBBP2]    |= ((pto>>2)&0x1)<<sqto;
  board[QBBP3]    |= ((pto>>3)&0x1)<<sqto;

  /* set piece moved flag, for castle rights */
  board[QBBPMVD]  |= SETMASKBB(sqfrom);
  board[QBBPMVD]  |= SETMASKBB(sqto);
  board[QBBPMVD]  |= SETMASKBB(sqcpt);

  /* handle castle rook, queenside */
  pcastle = (move&MOVEISCRQ)?MAKEPIECE(ROOK,GETCOLOR(pfrom)):PNONE;
  /* unset castle rook from */
  bbTemp  = (move&MOVEISCRQ)?CLRMASKBB(sqfrom-4):BBFULL;
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;
  /* set castle rook to */
  board[QBBBLACK] |= (pcastle&0x1)<<(sqto+1);
  board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqto+1);
  board[QBBP2]    |= ((pcastle>>2)&0x1)<<(sqto+1);
  board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqto+1);
  /* set piece moved flag, for castle rights */
  board[QBBPMVD]  |= (pcastle)?SETMASKBB(sqfrom-4):BBEMPTY;
  /* reset halfmoveclok */
  hmc = (pcastle)?0:hmc;  /* castle move */
  /* do score increment */
  score-= (pcastle==PNONE)?0:evalmove(pcastle, sqfrom-4);
  score+= (pcastle==PNONE)?0:evalmove(pcastle, sqto+1);
  /* do hash increment, clear old rook */
  board[QBBHASH] ^= (pcastle)?Random64[GETCOLOR(pfrom)*6*64+(ROOK-1)*64+sqfrom-4]:BBEMPTY;
  /* do hash increment, set new rook */
  board[QBBHASH] ^= (pcastle)?Random64[GETCOLOR(pfrom)*6*64+(ROOK-1)*64+sqto+1]:BBEMPTY;

  /* handle castle rook, kingside */
  pcastle = (move&MOVEISCRK)?MAKEPIECE(ROOK,GETCOLOR(pfrom)):PNONE;
  /* unset castle rook from */
  bbTemp  = (move&MOVEISCRK)?CLRMASKBB(sqfrom+3):BBFULL;
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;
  /* set castle rook to */
  board[QBBBLACK] |= (pcastle&0x1)<<(sqto-1);
  board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqto-1);
  board[QBBP2]    |= ((pcastle>>2)&0x1)<<(sqto-1);
  board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqto-1);
  /* set piece moved flag, for castle rights */
  board[QBBPMVD]  |= (pcastle)?SETMASKBB(sqfrom+3):BBEMPTY;
  /* reset halfmoveclok */
  hmc = (pcastle)?0:hmc;  /* castle move */
  /* do score increment */
  score-= (pcastle==PNONE)?0:evalmove(pcastle, sqfrom+3);
  score+= (pcastle==PNONE)?0:evalmove(pcastle, sqto-1);
  /* do hash increment, clear old rook */
  board[QBBHASH] ^= (pcastle)?Random64[GETCOLOR(pfrom)*6*64+(ROOK-1)*64+sqfrom+3]:BBEMPTY;
  /* do hash increment, set new rook */
  board[QBBHASH] ^= (pcastle)?Random64[GETCOLOR(pfrom)*6*64+(ROOK-1)*64+sqto-1]:BBEMPTY;

  /* handle halfmove clock */
  hmc = (GETPTYPE(pfrom)==PAWN)?0:hmc;   /* pawn move */
  hmc = (GETPTYPE(pcpt)!=PNONE)?0:hmc;  /* capture move */


  /* do score increment */
  score-= evalmove(pfrom, sqfrom);
  score+= (pcpt==PNONE)?0:evalmove(pcpt, sqcpt);
  score+= evalmove(pto, sqto);
  score= (GETCOLOR(pfrom))?-score:score;
  boardscore = (Score)board[QBBSCORE];
  boardscore+= score;
  board[QBBSCORE] = (u64)boardscore;

  /* do hash increment, clear piece from */
  board[QBBHASH] ^= Random64[GETCOLOR(pfrom)*6*64+(GETPTYPE(pfrom)-1)*64+sqfrom];
  /* do hash increment, set piece to */
  board[QBBHASH] ^= Random64[GETCOLOR(pfrom)*6*64+(GETPTYPE(pto)-1)*64+sqto];
  /* do hash increment, clear piece capture */
  board[QBBHASH] ^= (pcpt)?Random64[GETCOLOR(pcpt)*6*64+(GETPTYPE(pcpt)-1)*64+sqcpt]:BBEMPTY;

  /* do hash increment , set new */
  /* castle rights */
  if(((~board[QBBPMVD])&SMCRWHITEK)==SMCRWHITEK)
    board[QBBHASH] ^= RandomCastle[0];
  if(((~board[QBBPMVD])&SMCRWHITEQ)==SMCRWHITEQ)
    board[QBBHASH] ^= RandomCastle[1];
  if(((~board[QBBPMVD])&SMCRBLACKK)==SMCRBLACKK)
    board[QBBHASH] ^= RandomCastle[2];
  if(((~board[QBBPMVD])&SMCRBLACKQ)==SMCRBLACKQ)
    board[QBBHASH] ^= RandomCastle[3];
  /* file en passant */
  if (GETSQEP(move))
    board[QBBHASH] ^= RandomEnPassant[GETFILE(GETSQEP(move))]; 
  if (!GETCOLOR(pfrom)==WHITE)
      board[QBBHASH] ^=RandomTurn[0];

  /* store hmc  */  
  move = SETHMC(move, hmc);
  /* store lastmove in board */
  board[QBBLAST] = move;
}
/* restore board again */
void undomove(Bitboard *board, Move move, Move lastmove, Cr cr, Score score, Hash hash)
{
  Square sqfrom   = GETSQFROM(move);
  Square sqto     = GETSQTO(move);
  Square sqcpt    = GETSQCPT(move);
  Piece pfrom     = GETPFROM(move);
  Piece pcpt      = GETPCPT(move);
  Bitboard bbTemp = BBEMPTY;
  Piece pcastle   = PNONE;

  /* check for edges */
  if (move==MOVENONE)
    return;

  /* restore lastmove with hmc, cr and score */
  board[QBBLAST] = lastmove;
  /* restore castle rights. via piece moved flags */
  board[QBBPMVD] = cr;
  /* restore board score */
  board[QBBSCORE] = (u64)score;
  /* restore hash */
  board[QBBHASH] = hash;

  /* unset square capture, square to */
  bbTemp = CLRMASKBB(sqcpt)&CLRMASKBB(sqto);
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;

  /* restore piece capture */
  board[QBBBLACK] |= (pcpt&0x1)<<sqcpt;
  board[QBBP1]    |= ((pcpt>>1)&0x1)<<sqcpt;
  board[QBBP2]    |= ((pcpt>>2)&0x1)<<sqcpt;
  board[QBBP3]    |= ((pcpt>>3)&0x1)<<sqcpt;

  /* restore piece from */
  board[QBBBLACK] |= (pfrom&0x1)<<sqfrom;
  board[QBBP1]    |= ((pfrom>>1)&0x1)<<sqfrom;
  board[QBBP2]    |= ((pfrom>>2)&0x1)<<sqfrom;
  board[QBBP3]    |= ((pfrom>>3)&0x1)<<sqfrom;

  /* handle castle rook, queenside */
  pcastle = (move&MOVEISCRQ)?MAKEPIECE(ROOK,GETCOLOR(pfrom)):PNONE;
  /* unset castle rook to */
  bbTemp  = (move&MOVEISCRQ)?CLRMASKBB(sqto+1):BBFULL;
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;
  /* restore castle rook from */
  board[QBBBLACK] |= (pcastle&0x1)<<(sqfrom-4);
  board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqfrom-4);
  board[QBBP2]    |= ((pcastle>>2)&0x1)<<(sqfrom-4);
  board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqfrom-4);
  /* handle castle rook, kingside */
  pcastle = (move&MOVEISCRK)?MAKEPIECE(ROOK,GETCOLOR(pfrom)):PNONE;
  /* restore castle rook from */
  bbTemp  = (move&MOVEISCRK)?CLRMASKBB(sqto-1):BBFULL;
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;
  /* set castle rook to */
  board[QBBBLACK] |= (pcastle&0x1)<<(sqfrom+3);
  board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqfrom+3);
  board[QBBP2]    |= ((pcastle>>2)&0x1)<<(sqfrom+3);
  board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqfrom+3);
}
/* collect principal variaton from hash table for xboard output */
s32 collect_pv_from_hash(Bitboard *board, Hash hash, Move *moves, s32 ply)
{
  s32 i = 0;
  s32 count = 0;
  s32 repcount = 0;
  struct TTE *tt = NULL;
  Cr cr[MAXMOVES];
  Score scores[MAXMOVES];
  Hash hashes[MAXMOVES];
  Hash lastmoves[MAXMOVES];

  tt = load_from_tt(hash);
  while (tt&&tt->hash==hash&&JUSTMOVE(tt->bestmove)!=MOVENONE&&i<MAXMOVES&&i<=ply)
  {
    hashes[i] = hash;
    scores[i] = board[QBBSCORE];
    cr[i] = board[QBBPMVD];
    lastmoves[i] = board[QBBLAST];

    moves[i++] = tt->bestmove;
    domove(board, tt->bestmove);
    hash = board[QBBHASH];
    tt = load_from_tt(hash);
    /* check for repetition loop */
    count = i-1;
    for (count=i;count>=0;count--)
    {
      if (hash==hashes[count])
        repcount++;
    }      
    if (repcount>2)
      break;
  }

  count = i;
  while(i-->0)
  {
    undomove(board, moves[i], lastmoves[i], cr[i], scores[i], hashes[i]);
  }
  return count;
}
/* is square attacked by an enemy piece, via superpiece approach */
bool squareunderattack(Bitboard *board, bool stm, Square sq) 
{
  Bitboard bbWork;
  Bitboard bbMoves;
  Bitboard bbBlockers;
  Bitboard bbBoth[2];

  bbBlockers    = board[QBBP1]|board[QBBP2]|board[QBBP3];
  bbBoth[WHITE] = board[QBBBLACK]^bbBlockers;
  bbBoth[BLACK] = board[QBBBLACK];

  /* rooks and queens */
  bbMoves = rook_attacks(bbBlockers, sq);
  bbWork =    (bbBoth[stm]&(board[QBBP1]&~board[QBBP2]&board[QBBP3])) 
            | (bbBoth[stm]&(~board[QBBP1]&board[QBBP2]&board[QBBP3]));
  if (bbMoves&bbWork)
  {
    return true;
  }
  bbMoves = bishop_attacks(bbBlockers, sq);
  /* bishops and queens */
  bbWork =  (bbBoth[stm]&(~board[QBBP1]&~board[QBBP2]&board[QBBP3])) 
          | (bbBoth[stm]&(~board[QBBP1]&board[QBBP2]&board[QBBP3]));
  if (bbMoves&bbWork)
  {
    return true;
  }
  /* knights */
  bbWork = bbBoth[stm]&(~board[QBBP1]&board[QBBP2]&~board[QBBP3]);
  bbMoves = AttackTables[128+sq] ;
  if (bbMoves&bbWork) 
  {
    return true;
  }
  /* pawns */
  bbWork = bbBoth[stm]&(board[QBBP1]&~board[QBBP2]&~board[QBBP3]);
  bbMoves = AttackTables[!stm*64+sq];
  if (bbMoves&bbWork)
  {
    return true;
  }
  /* king */
  bbWork = bbBoth[stm]&(board[QBBP1]&board[QBBP2]&~board[QBBP3]);
  bbMoves = AttackTables[192+sq];
  if (bbMoves&bbWork)
  {
    return true;
  } 

  return false;
}
/* is king attacked by an enemy piece */
bool kingincheck(Bitboard *board, bool stm) 
{
  Square sqking;
  Bitboard bbKing;

  /* get colored pieces */
  bbKing  = (stm)? board[QBBBLACK] : 
            (board[QBBBLACK]^(board[QBBP1]|board[QBBP2]|board[QBBP3]));

  /* get colored king */
  bbKing &= board[QBBP1]&board[QBBP2]&~board[QBBP3];
  /* get king square */
  sqking  = first1(bbKing);

  return squareunderattack(board, !stm, sqking);
}
/* check for two opposite kings */
bool isvalid(Bitboard *board)
{
  if ( (popcount(board[QBBBLACK]&(board[QBBP1]&board[QBBP2]&~board[QBBP3]))==1) 
        && (popcount( (board[QBBBLACK]^(board[QBBP1]|board[QBBP2]|board[QBBP3]))
                      &(board[QBBP1]&board[QBBP2]&~board[QBBP3]))==1)
     )
  {
    return true;
  }

  return false;
}
/* print bitboard */
void printbitboard(Bitboard board)
{
  s32 rank;
  s32 file;
  Square sq;

  fprintf(stdout,"###ABCDEFGH###\n");
  for(rank=RANK_8;rank>=RANK_1;rank--) {
    fprintf(stdout,"#%i ",rank+1);
    for(file=FILE_A;file<FILE_NONE;file++) {
      sq = MAKESQ(file, rank);
      if (board&SETMASKBB(sq)) 
        fprintf(stdout,"x");
      else 
        fprintf(stdout,"-");
    }
    fprintf(stdout,"\n");
  }
  fprintf(stdout,"###ABCDEFGH###\n");

  fflush(stdout);
}
void printmove(Move move)
{
  fprintf(stdout,"#sqfrom:%llu\n",GETSQFROM(move));
  fprintf(stdout,"#sqto:%llu\n",GETSQTO(move));
  fprintf(stdout,"#sqcpt:%llu\n",GETSQCPT(move));
  fprintf(stdout,"#pfrom:%llu\n",GETPFROM(move));
  fprintf(stdout,"#pto:%llu\n",GETPTO(move));
  fprintf(stdout,"#pcpt:%llu\n",GETPCPT(move));
  fprintf(stdout,"#sqep:%llu\n",GETSQEP(move));
  fprintf(stdout,"#hmc:%u\n",(u32)GETHMC(move));
  fprintf(stdout,"#score:%i\n",(Score)GETSCORE(move));
}
/* move in algebraic notation, eg. e2e4, to internal packed move  */
static Move can2move(char *usermove, Bitboard *board, bool stm) 
{
  File file;
  Rank rank;
  Square sqfrom;
  Square sqto;
  Square sqcpt;
  Piece pto;
  Piece pfrom;
  Piece pcpt;
  Move move;
  char promopiece;
  Square sqep = 0;

  file    = (int)usermove[0] -97;
  rank    = (int)usermove[1] -49;
  sqfrom  = MAKESQ(file, rank);
  file    = (int)usermove[2] -97;
  rank    = (int)usermove[3] -49;
  sqto    = MAKESQ(file, rank);

  pfrom = GETPIECE(board, sqfrom);
  pto = pfrom;
  sqcpt = sqto;
  pcpt = GETPIECE(board, sqcpt);

  /* en passant move , set square capture */
  sqcpt = ((pfrom>>1)==PAWN&&(stm==WHITE)&&GETRANK(sqfrom)==RANK_5  
            &&sqto-sqfrom!=8&&pcpt==PNONE)?sqto-8:sqcpt;

  sqcpt = ((pfrom>>1)==PAWN&&(stm==BLACK)&&GETRANK(sqfrom)==RANK_4  
            &&sqfrom-sqto!=8 &&pcpt==PNONE)?sqto+8:sqcpt;

  pcpt = GETPIECE(board, sqcpt);

  /* pawn double square move, set en passant target square */
  if ((pfrom>>1)==PAWN&&GETRRANK(sqfrom,stm)==1&&GETRRANK(sqto,stm)==3)
    sqep = sqto;

  /* pawn promo piece */
  promopiece = usermove[4];
  if (promopiece == 'q' || promopiece == 'Q' )
      pto = MAKEPIECE(QUEEN,stm);
  else if (promopiece == 'n' || promopiece == 'N' )
      pto = MAKEPIECE(KNIGHT,stm);
  else if (promopiece == 'b' || promopiece == 'B' )
      pto = MAKEPIECE(BISHOP,stm);
  else if (promopiece == 'r' || promopiece == 'R' )
      pto = MAKEPIECE(ROOK,stm);

  /* pack move, considering hmc, cr and score */
  move = MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto , pcpt, sqep,
                  GETHMC(board[QBBLAST]), (u64)0);

  /* set castle move flag */
  if ((pfrom>>1)==KING&&!stm&&sqfrom==4&&sqto==2)
    move |= MOVEISCRQ;
  if ((pfrom>>1)==KING&&!stm&&sqfrom==4&&sqto==6)
    move |= MOVEISCRK;
  if ((pfrom>>1)==KING&&stm&&sqfrom==60&&sqto==58)
    move |= MOVEISCRQ;
  if ((pfrom>>1)==KING&&stm&&sqfrom==60&&sqto==62)
    move |= MOVEISCRK;
 
  return move;
}
/* packed move to move in coordinate algebraic notation,
  e.g. 
  e2e4 
  e1c1 => when king, indicates castle queenside  
  e7e8q => indicates pawn promotion to queen
*/
static void move2can(Move move, char * movec) 
{
  char rankc[8] = "12345678";
  char filec[8] = "abcdefgh";
  Square from   = GETSQFROM(move);
  Square to     = GETSQTO(move);
  Piece pfrom   = GETPFROM(move);
  Piece pto     = GETPTO(move);

  movec[0] = filec[GETFILE(from)];
  movec[1] = rankc[GETRANK(from)];
  movec[2] = filec[GETFILE(to)];
  movec[3] = rankc[GETRANK(to)];
  movec[4] = '\0';

  /* pawn promo */
  if ( (pfrom>>1) == PAWN && (pto>>1) != PAWN)
  {
    if ( (pto>>1) == QUEEN)
      movec[4] = 'q';
    if ( (pto>>1) == ROOK)
      movec[4] = 'r';
    if ( (pto>>1) == BISHOP)
      movec[4] = 'b';
    if ( (pto>>1) == KNIGHT)
      movec[4] = 'n';
    movec[5] = '\0';
  }
}
void printmovecan(Move move)
{
  char movec[4];
  move2can(move, movec);
  fprintf(stdout, "%s",movec);
}
/* print quadbitbooard */
void printboard(Bitboard *board)
{
  s32 rank;
  s32 file;
  Square sq;
  Piece piece;
  char wpchars[] = "-PNKBRQ";
  char bpchars[] = "-pnkbrq";
  char fenstring[1024];

  fprintf(stdout,"###ABCDEFGH###\n");
  for (rank = RANK_8; rank >= RANK_1; rank--) 
  {
    fprintf(stdout,"#%i ",rank+1);
    for (file = FILE_A; file < FILE_NONE; file++)
    {
      sq = MAKESQ(file, rank);
      piece = GETPIECE(board, sq);
      if (piece != PNONE && (piece&BLACK))
        fprintf(stdout,"%c", bpchars[piece>>1]);
      else if (piece != PNONE)
        fprintf(stdout,"%c", wpchars[piece>>1]);
      else 
        fprintf(stdout,"-");
    }
    fprintf(stdout,"\n");
  }
  fprintf(stdout,"###ABCDEFGH###\n");

  createfen (fenstring, BOARD, STM, GAMEPLY);
  fprintf(stdout,"#fen: %s\n",fenstring);
  fprintf(stdout,"# incremental score: %d\n",(Score)BOARD[QBBSCORE]);
  fprintf(stdout,"# eval score: %d\n",eval(BOARD));

  if (LogFile)
  {
    fprinttime(LogFile);
    fprintf(LogFile, "#fen: %s\n",fenstring);
    fprinttime(LogFile);
    fprintf(LogFile, "###ABCDEFGH###\n");
    for (rank = RANK_8; rank >= RANK_1; rank--) 
    {
    fprinttime(LogFile);
      fprintf(LogFile, "#%i ",rank+1);
      for (file = FILE_A; file < FILE_NONE; file++)
      {
        sq = MAKESQ(file, rank);
        piece = GETPIECE(board, sq);
        if (piece != PNONE && (piece&BLACK))
          fprintf(LogFile, "%c", bpchars[piece>>1]);
        else if (piece != PNONE)
          fprintf(LogFile, "%c", wpchars[piece>>1]);
        else 
          fprintf(LogFile, "-");
      }
      fprintf(LogFile, "\n");
    }
    fprinttime(LogFile);
    fprintf(LogFile, "###ABCDEFGH###\n");
    fprinttime(LogFile);
    fprintf(LogFile, "# incremental score: %d\n",(Score)BOARD[QBBSCORE]);
    fprintf(LogFile,"# eval score: %d\n",eval(BOARD));

    fflush (LogFile);
  }
  fflush (stdout);
}
/* create fen string from board state */
static void createfen(char *fenstring, Bitboard *board, bool stm, s32 gameply)
{
  s32 rank;
  s32 file;
  Square sq;
  Piece piece;
  char wpchars[] = " PNKBRQ";
  char bpchars[] = " pnkbrq";
  char rankc[8] = "12345678";
  char filec[8] = "abcdefgh";
  char *stringptr = fenstring;
  s32 spaces = 0;

  /* add pieces from board to string */
  for (rank = RANK_8; rank >= RANK_1; rank--)
  {
    spaces=0;
    for (file = FILE_A; file < FILE_NONE; file++)
    {
      sq = MAKESQ(file, rank);
      piece = GETPIECE(board, sq);
      /* handle empty squares */
      if (spaces > 0 && piece != PNONE)
      {
        stringptr+=sprintf(stringptr, "%d", spaces);
        spaces=0;
      }
      /* handle pieces, black and white */
      if (piece != PNONE && (piece&BLACK))
        stringptr+=sprintf(stringptr, "%c", bpchars[piece>>1]);
      else if (piece != PNONE)
        stringptr+=sprintf(stringptr, "%c", wpchars[piece>>1]);
      else
        spaces++;
    }
    /* handle empty squares */
    if (spaces > 0)
    {
      stringptr+=sprintf(stringptr, "%d", spaces);
      spaces=0;
    }
    /* handle rows delimeter */
    if (rank <= RANK_8 && rank > RANK_1)
      stringptr+=sprintf(stringptr, "/");
  }

  stringptr+=sprintf(stringptr, " ");

  /* add site to move */
  if (stm&BLACK)
  {
    stringptr+=sprintf(stringptr, "b");
  }
  else
  {
    stringptr+=sprintf(stringptr, "w");
  }

  stringptr+=sprintf(stringptr, " ");

  /* add castle rights */
  if (((~board[QBBPMVD])&SMCRALL)==BBEMPTY)
    stringptr+=sprintf(stringptr, "-");
  else
  {
    /* white kingside */
    if (((~board[QBBPMVD])&SMCRWHITEK)==SMCRWHITEK)
      stringptr+=sprintf(stringptr, "K");
    /* white queenside */
    if (((~board[QBBPMVD])&SMCRWHITEQ)==SMCRWHITEQ)
      stringptr+=sprintf(stringptr, "Q");
    /* black kingside */
    if (((~board[QBBPMVD])&SMCRBLACKK)==SMCRBLACKK)
      stringptr+=sprintf(stringptr, "k");
    /* black queenside */
    if (((~board[QBBPMVD])&SMCRBLACKQ)==SMCRBLACKQ)
      stringptr+=sprintf(stringptr, "q");
  }

  stringptr+=sprintf(stringptr," ");

  /* add en passant target square */
  sq = GETSQEP(board[QBBLAST]);
  if (sq > 0)
  {
    if (stm)
      sq-=8;
    if (!stm)
      sq+=8;
    stringptr+=sprintf(stringptr, "%c", filec[GETFILE(sq)]);
    stringptr+=sprintf(stringptr, "%c", rankc[GETRANK(sq)]);
  }
  else
    stringptr+=sprintf(stringptr, "-");

  stringptr+=sprintf(stringptr," ");

  /* add halpfmove clock  */
  stringptr+=sprintf(stringptr, "%llu",GETHMC(board[QBBLAST]));
  stringptr+=sprintf(stringptr, " ");

  stringptr+=sprintf(stringptr, "%d", ((gameply+PLY)/2));
}
/* set internal chess board presentation to fen string */
static bool setboard(Bitboard *board, char *fenstring)
{
  char tempchar;
  char *position; /* piece types and position, row_8, file_a, to row_1, file_h*/
  char *cstm;     /* site to move */
  char *castle;   /* castle rights */
  char *cep;      /* en passant target square */
  char fencharstring[24] = {" PNKBRQ pnkbrq/12345678"}; /* mapping */
  Score score = 0;
  File file;
  Rank rank;
  Piece piece;
  Square sq;
  u64 i;
  u64 j;
  u64 hmc = 0;        /* half move clock */
  u64 fendepth = 1;   /* game depth */
  Move lastmove = MOVENONE;
  Bitboard bbCr = BBEMPTY;

  /* memory, fen string ist max 1023 char in size */
  position  = malloc (1024 * sizeof (char));
  if (!position) 
  {
    fprintf(stdout,"Error (memory allocation failed): char position[%d]", 1024);
  }
  cstm  = malloc (1024 * sizeof (char));
  if (!cstm) 
  {
    fprintf(stdout,"Error (memory allocation failed): char cstm[%d]", 1024);
  }
  castle  = malloc (1024 * sizeof (char));
  if (!castle) 
  {
    fprintf(stdout,"Error (memory allocation failed): char castle[%d]", 1024);
  }
  cep  = malloc (1024 * sizeof (char));
  if (!cep) 
  {
    fprintf(stdout,"Error (memory allocation failed): char cep[%d]", 1024);
  }
  if (!position||!cstm||!castle||!cep)
  {
    /* release memory */
    if (position) 
      free(position);
    if (cstm) 
      free(cstm);
    if (castle) 
      free(castle);
    if (cep) 
      free(cep);
    return false;
  }

  /* get data from fen string */
	sscanf (fenstring, "%s %s %s %s %llu %llu", 
          position, cstm, castle, cep, &hmc, &fendepth);

  /* empty the board */
  board[QBBBLACK] = 0x0ULL;
  board[QBBP1]    = 0x0ULL;
  board[QBBP2]    = 0x0ULL;
  board[QBBP3]    = 0x0ULL;
  board[QBBPMVD]  = BBFULL;
  board[QBBHASH]  = 0x0ULL;
  board[QBBSCORE] = 0x0ULL;
  board[QBBLAST]  = 0x0ULL;

  /* parse piece types and position from fen string */
  file = FILE_A;
  rank = RANK_8;
  i=  0;
  while (!(rank <= RANK_1 && file >= FILE_NONE))
  {
    tempchar = position[i++];
    /* iterate through all characters */
    for (j = 0; j <= 23; j++) 
    {
  		if (tempchar == fencharstring[j])
      {
        /* delimeter / */
        if (j == 14)
        {
            rank--;
            file = FILE_A;
        }
        /* empty squares */
        else if (j >= 15)
        {
            file+=j-14;
        }
        else if (j >= 7)
        {
            sq               = MAKESQ(file, rank);
            piece            = j-7;
            piece            = MAKEPIECE(piece,(u64)BLACK);
            board[QBBBLACK] |= (piece&0x1)<<sq;
            board[QBBP1]    |= ((piece>>1)&0x1)<<sq;
            board[QBBP2]    |= ((piece>>2)&0x1)<<sq;
            board[QBBP3]    |= ((piece>>3)&0x1)<<sq;
            file++;
        }
        else if (j <= 6)
        {
            sq               = MAKESQ(file, rank);
            piece            = j;
            piece            = MAKEPIECE(piece,(u64)WHITE);
            board[QBBBLACK] |= (piece&0x1)<<sq;
            board[QBBP1]    |= ((piece>>1)&0x1)<<sq;
            board[QBBP2]    |= ((piece>>2)&0x1)<<sq;
            board[QBBP3]    |= ((piece>>3)&0x1)<<sq;
            file++;
        }
        break;                
      } 
    }
  }
  /* site to move */
  STM = WHITE;
  if (cstm[0] == 'b' || cstm[0] == 'B')
  {
    STM = BLACK;
  }
  /* castle rights */
  tempchar = castle[0];
  if (tempchar != '-')
  {
    i = 0;
    while (tempchar != '\0')
    {
      /* white queenside */
      if (tempchar == 'Q')
        bbCr |= SMCRWHITEQ;
      /* white kingside */
      if (tempchar == 'K')
        bbCr |= SMCRWHITEK;
      /* black queenside */
      if (tempchar == 'q')
        bbCr |= SMCRBLACKQ;
      /* black kingside */
      if (tempchar == 'k')
        bbCr |= SMCRBLACKK;
      i++;
      tempchar = castle[i];
    }
  }
  /* store castle rights via piece moved flags in board */
  board[QBBPMVD]  ^= bbCr;
  /* store halfmovecounter into lastmove */
  lastmove = SETHMC(lastmove, hmc);

  /* set en passant target square */
  tempchar = cep[0];
  file  = 0;
  rank  = 0;
  if (tempchar != '-' && tempchar != '\0' && tempchar != '\n')
  {
    file  = cep[0] - 97;
    rank  = cep[1] - 49;
  }
  sq    = MAKESQ(file, rank);
  lastmove = SETSQEP(lastmove, sq);

  /* ply starts at zero */
  PLY = 0;
  /* game ply can be more */
  GAMEPLY = fendepth*2+STM;

  /* compute zobrist hash */
  board[QBBHASH] = computehash(BOARD, STM);
  HashHistory[PLY] = board[QBBHASH];

  /* store lastmove+ in board */
  board[QBBLAST] = lastmove;
  /* store lastmove+ in history */
  MoveHistory[PLY] = lastmove;

  /* release memory */
  if (position) 
    free(position);
  if (cstm) 
    free(cstm);
  if (castle) 
    free(castle);
  if (cep) 
    free(cep);

  /* static eval for inceremental eval scores during do/undo */
  score = evalstatic(board);
  /* store incremental score in board */
  board[QBBSCORE] = (u64)score;
  /* store incremental score in history */
  ScoreHistory[PLY] = score;

  /* board valid check */
  if (!isvalid(board))
  {
    fprintf(stdout,"Error (given fen position is illegal): setboard\n");        
    if (LogFile)
    {
      fprinttime(LogFile);
      fprintf(LogFile,"Error (given fen position is illegal): setboard\n");        
    }
    return false;
  }

  return true;
}
/* run internal selftest */
static void selftest(void) 
{
  Score scorea = 0;
  Score scoreb = 0;
  u64 done;
  u64 passed = 0;
  const u64 todo = 41;
  Move move;
  Hash hash;
  Hash computedhash;
  Hash incrementalhash;

  char movesc1[6][5] =
  {
    "e2e4",
    "d7d5",
    "e4e5", 
    "f7f5",
    "e1e2", 
    "e8f7"
  };
  char movesc2[7][5] =
  {
    "a2a4",
    "b7b5",
    "h2h4",
    "b5b4",
    "c2c4",
    "b4c3",
    "a1a3"
  };
  Hash hashes[9] =
  { 
    0x463b96181691fc9c,
    0x823c9b50fd114196,
    0x0756b94461c50fb0,
    0x662fafb965db29d4,
    0x22a48b5a8e47ff78,
    0x652a607ca3f242c1,
    0x00fdd303c946bdd9,
    0x3c8123ea7b067637,
    0x5c3f9b829b279560
  };
  char fenpositions[23][256]  =
  {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -"
  };
  u32 depths[] =
  {
    1,2,3,4,
    1,2,3,
    1,2,3,4,5,
    1,2,3,4,
    1,2,3,4,
    1,2,3
  };
  u64 nodecounts[] =
  {
    20,400,8902,197281,
    48,2039,97862,
    14,191,2812,43238,674624,
    6,264,9467,422333,
    44,1486,62379,2103487,
    46,2079,89890,3894594
  };

  for (done=0;done<23;done++)
  {
    NODECOUNT = 0;
    MOVECOUNT = 0;

    SD = depths[done];
    
    fprintf(stdout,"# doing perft depth: %d for position\n", SD);  
    if (LogFile)
    {
      fprinttime(LogFile);
      fprintf(LogFile,"# doing perft depth: %d for position\n", SD);  
    }
    if (!setboard(BOARD,  fenpositions[done]))
    {
      fprintf(stdout,"# Error (in setting fen position): setboard\n");        
      if (LogFile)
      {
        fprinttime(LogFile);
        fprintf(LogFile,"# Error (in setting fen position): setboard\n");        
      }
      continue;
    }
    else
      printboard(BOARD);

    /* store score */
    scorea = (Score)BOARD[QBBSCORE];
    /* time measurement */
    start = get_time();
    /* perfomance test, just leaf nodecount to given depth */
    perft(BOARD, STM, SD);
    /* store score */
    scoreb = (Score)BOARD[QBBSCORE];
    /* time measurement */
    end = get_time();   
    elapsed = end-start;
    elapsed /= 1000;

    if(NODECOUNT==nodecounts[done]&&scorea==scoreb)
      passed++;

    if(NODECOUNT==nodecounts[done])
    {
      fprintf(stdout,"# Nodecount Correct, %llu nodes in %lf seconds with \
%llu nps.\n", NODECOUNT, elapsed, (u64)(NODECOUNT/elapsed));
      if (LogFile)
      {
        fprinttime(LogFile);
        fprintf(LogFile,"# Nodecount Correct, %llu nodes in %lf seconds with \
%llu nps.\n", NODECOUNT, elapsed, (u64)(NODECOUNT/elapsed));
      }
    }
    else
    {
      fprintf(stdout,"# Nodecount NOT Correct, %llu computed nodes != %llu \
nodes for depth %d.\n", NODECOUNT, nodecounts[done], SD);
      if (LogFile)
      {
        fprinttime(LogFile);
        fprintf(LogFile,"# Nodecount NOT Correct, %llu computed nodes != %llu \
nodes for depth %d.\n", NODECOUNT, nodecounts[done], SD);
      }
    }
    if(scorea!=scoreb)
    {
      fprintf(stdout,"# IncrementaL evaluation  scores NOT Correct, \
               %d != %d .\n", scorea, scoreb);
      if (LogFile)
      {
        fprinttime(LogFile);
        fprintf(LogFile,"# Nodecount Correct, %llu nodes in %lf seconds \
               with %llu nps.\n", NODECOUNT, elapsed, (u64)(NODECOUNT/elapsed));
        fprintf(LogFile,"# IncrementaL evaluation  scores NOT Correct, \
                %d != %d .\n", scorea, scoreb);
      }
    }
  }
  /* test for book hashes */
  setboard(BOARD,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  fprintf(stdout,"#\n");  
  fprintf(stdout,"# doing zobrist hash checks\n");  
  if (LogFile)
  {
    fprinttime(LogFile);
    fprintf(LogFile,"#\n");  
    fprintf(LogFile,"# doing zobrist hash checks\n");  
  }
  done = 0;
  do
  {
    printboard(BOARD);
    hash = computebookhash(BOARD,STM);
    if(hash!=hashes[done])
    {
      fprintf(stdout,"# Book hash NOT Correct, 0x%016llx != 0x%016llx\n", hash, hashes[done]);
      if (LogFile)
      {
        fprinttime(LogFile);
       fprintf(LogFile,"# Book hash NOT Correct, 0x%016llx != 0x%016llx\n", hash, hashes[done]);
      }
    }
    else
    {
      passed++;
      fprintf(stdout,"# Book hash Correct, 0x%016llx == 0x%016llx\n", hash, hashes[done]);
      if (LogFile)
      {
        fprinttime(LogFile);
       fprintf(LogFile,"# Book hash Correct, 0x%016llx == 0x%016llx\n", hash, hashes[done]);
      }
    }
    incrementalhash = BOARD[QBBHASH];
    computedhash = computehash(BOARD,STM);
    if(incrementalhash!=computedhash)
    {
      fprintf(stdout,"# incremental hash NOT Correct, 0x%016llx != 0x%016llx\n", incrementalhash, computedhash);
      if (LogFile)
      {
        fprinttime(LogFile);
       fprintf(LogFile,"# incremental hash NOT Correct, 0x%016llx != 0x%016llx\n", incrementalhash, computedhash);
      }
    }
    else
    {
      passed++;
      fprintf(stdout,"# incremental hash Correct, 0x%016llx == 0x%016llx\n", incrementalhash, computedhash);
      if (LogFile)
      {
        fprinttime(LogFile);
       fprintf(LogFile,"# incremental hash Correct, 0x%016llx == 0x%016llx\n", incrementalhash, computedhash);
      }
    }
    move = can2move(movesc1[done], BOARD, STM);
    domove(BOARD, move);
    STM = !STM;
  }
  while (done++<6);

  /* test for book hashes */
  setboard(BOARD,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  for (done=0;done<5;done++)
  {
    move = can2move(movesc2[done], BOARD, STM);
    domove(BOARD, move);
    STM = !STM;
  }
  printboard(BOARD);
  hash = computebookhash(BOARD,STM);
  if(hash!=0x3c8123ea7b067637)
  {
    fprintf(stdout,"# Book hash NOT Correct, 0x%016llx != 0x3c8123ea7b067637\n", hash);
    if (LogFile)
    {
      fprinttime(LogFile);
     fprintf(LogFile,"# Book hash NOT Correct, 0x%016llx != 0x3c8123ea7b067637\n", hash);
    }
  }
  else
  {
    passed++;
    fprintf(stdout,"# Book hash Correct, 0x%016llx == 0x3c8123ea7b067637\n", hash);
    if (LogFile)
    {
      fprinttime(LogFile);
     fprintf(LogFile,"# Book hash Correct, 0x%016llx == 0x3c8123ea7b067637\n", hash);
    }
  }
  incrementalhash = BOARD[QBBHASH];
  computedhash = computehash(BOARD,STM);
  if(incrementalhash!=computedhash)
  {
    fprintf(stdout,"# incremental hash NOT Correct, 0x%016llx != 0x%016llx\n", incrementalhash, computedhash);
    if (LogFile)
    {
      fprinttime(LogFile);
     fprintf(LogFile,"# incremental hash NOT Correct, 0x%016llx != 0x%016llx\n", incrementalhash, computedhash);
    }
  }
  else
  {
    passed++;
    fprintf(stdout,"# incremental hash Correct, 0x%016llx == 0x%016llx\n", incrementalhash, computedhash);
    if (LogFile)
    {
      fprinttime(LogFile);
     fprintf(LogFile,"# incremental hash Correct, 0x%016llx == 0x%016llx\n", incrementalhash, computedhash);
    }
  }  for (done=5;done<7;done++)
  {
    move = can2move(movesc2[done], BOARD, STM);
    domove(BOARD, move);
    STM = !STM;
  }
  printboard(BOARD);
  hash = computebookhash(BOARD,STM);
  if(hash!=0x5c3f9b829b279560)
  {
    fprintf(stdout,"# Book hash NOT Correct, 0x%016llx != 0x5c3f9b829b279560\n", hash);
    if (LogFile)
    {
      fprinttime(LogFile);
     fprintf(LogFile,"# Book hash NOT Correct, 0x%016llx != 0x5c3f9b829b279560\n", hash);
    }
  }
  else
  {
    passed++;
    fprintf(stdout,"# Book hash Correct, 0x%016llx == 0x5c3f9b829b279560\n", hash);
    if (LogFile)
    {
      fprinttime(LogFile);
     fprintf(LogFile,"# Book hash Correct, 0x%016llx == 0x5c3f9b829b279560\n", hash);
    }
  }  
  incrementalhash = BOARD[QBBHASH];
  computedhash = computehash(BOARD,STM);
  if(incrementalhash!=computedhash)
  {
    fprintf(stdout,"# incremental hash NOT Correct, 0x%016llx != 0x%016llx\n", incrementalhash, computedhash);
    if (LogFile)
    {
      fprinttime(LogFile);
     fprintf(LogFile,"# incremental hash NOT Correct, 0x%016llx != 0x%016llx\n", incrementalhash, computedhash);
    }
  }
  else
  {
    passed++;
    fprintf(stdout,"# incremental hash Correct, 0x%016llx == 0x%016llx\n", incrementalhash, computedhash);
    if (LogFile)
    {
      fprinttime(LogFile);
     fprintf(LogFile,"# incremental hash Correct, 0x%016llx == 0x%016llx\n", incrementalhash, computedhash);
    }
  }

  fprintf(stdout,"#\n###############################\n");
  fprintf(stdout,"### passed %llu from %llu tests ###\n", passed, todo);
  fprintf(stdout,"###############################\n");
}
/* print engine info to console */
static void print_version(void)
{
  fprintf(stdout,"Zeta Dva version: %s\n",VERSION);
  fprintf(stdout,"Yet another amateur level chess engine.\n");
  fprintf(stdout,"Copyright (C) 2011-2016 Srdja Matovic, Montenegro\n");
  fprintf(stdout,"This is free software, licensed under GPL >= v2\n");
}
/* engine options and usage */
static void print_help(void)
{
  fprintf(stdout,"Zeta Dva, yet another amateur level chess engine.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Options:\n");
  fprintf(stdout," -l, --log          Write output/debug to file zetadva.log\n");
  fprintf(stdout," -v, --version      Print Zeta Dva version info.\n");
  fprintf(stdout," -h, --help         Print Zeta Dva program usage help.\n");
  fprintf(stdout," -s, --selftest     Run an internal test, usefull after compile.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"To play against the engine use an CECP v2 protocol capable chess GUI\n");
  fprintf(stdout,"like Arena, Cutechess, Winboard or Xboard.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Alternatively you can use Xboard commmands directly on commmand Line,\n"); 
  fprintf(stdout,"e.g.:\n");
  fprintf(stdout,"new            // init new game from start position\n");
  fprintf(stdout,"level 40 4 0   // set time control to 40 moves in 4 minutes\n");
  fprintf(stdout,"go             // let engine play site to move\n");
  fprintf(stdout,"usermove d7d5  // let engine apply usermove in coordinate algebraic\n");
  fprintf(stdout,"               // notation and optionally start thinking\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Not supported Xboard commands:\n");
  fprintf(stdout,"analyze        // enter analyze mode\n");
  fprintf(stdout,"?              // move now\n");
  fprintf(stdout,"draw           // handle draw offers\n");
  fprintf(stdout,"hard/easy      // turn on/off pondering\n");
  fprintf(stdout,"hint           // give user a hint move\n");
  fprintf(stdout,"bk             // book Lines\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Non-Xboard commands:\n");
  fprintf(stdout,"perft          // perform a performance test, depth set by sd command\n");
  fprintf(stdout,"selftest       // run an internal test\n");
  fprintf(stdout,"help           // print usage info\n");
  fprintf(stdout,"log            // turn log on\n");
  fprintf(stdout,"\n");
}
/* Zeta Dva, amateur level chess engine  */
int main(int argc, char* argv[])
{
  /* xboard states */
  s32 xboard_protover = 0;      /* Zeta works with protocoll version >= v2 */
  /* for get opt */
  s32 c;
  static struct option long_options[] = 
  {
    {"help", 0, 0, 'h'},
    {"version", 0, 0, 'v'},
    {"selftest", 0, 0, 's'},
    {"log", 0, 0, 'l'},
    {NULL, 0, NULL, 0}
  };
  s32 option_index = 0;

  /* no buffers */
  setbuf (stdout, NULL);
  setbuf (stdin, NULL);

  /* init memory, files and tables */
  if (!inits ())
  {
    release_inits ();
    exit (EXIT_FAILURE);
  }
  /* init transposition hash table with 64 mb*/
  initTT();
  /* init starting position */
  setboard(BOARD,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  /* turn log on */
  for (c=1;c<argc;c++)
  {
    if (!strcmp(argv[c], "-l") || !strcmp(argv[c],"--log"))
    {
      /* open/create log file */
      LogFile = fopen("zetadva.log", "a");
      if (!LogFile ) 
      {
        fprintf(stdout,"Error (opening logfile zetadva.log): --log");
      }
    }
  }
  /* getopt loop, parsing for help, version and logging */
  while ((c = getopt_long_only (argc, argv, "",
               long_options, &option_index)) != -1) {
    switch (option_index) 
    {
      case 0:
        print_help ();
        exit (EXIT_SUCCESS);
        break;
      case 1:
        print_version ();
        exit (EXIT_SUCCESS);
        break;
      case 2:
        selftest ();
        exit (EXIT_SUCCESS);
        break;
      case 3:
        break;
    }
  }
  /* open log file */
  if (LogFile)
  {
    /* no buffers */
    setbuf(LogFile, NULL);
    /* print binary call to log */
    fprinttime(LogFile);
    for (c=0;c<argc;c++)
    {
      fprintf(LogFile, "%s ",argv[c]);
    }
    fprintf(LogFile, "\n");
  }

  /* print engine info to console */
  fprintf(stdout,"Zeta Dva %s\n",VERSION);
  fprintf(stdout,"Yet another amateur level chess engine.\n");
  fprintf(stdout,"Copyright (C) 2011-2016 Srdja Matovic, Montenegro\n");
  fprintf(stdout,"This is free software, licensed under GPL >= v2\n");

  /* xboard command loop */
  for (;;)
  {
    /* just to be sure, flush the output...*/
    fflush (stdout);
    if (LogFile)
      fflush (LogFile);
    /* get Line */
    if (!fgets (Line, 1023, stdin)) {}
    /* ignore empty Lines */
    if (Line[0] == '\n')
      continue;
    /* print io to log file */
    if (LogFile)
    {
      fprinttime(LogFile);
      fprintf(LogFile, "%s\n",Line);
    }
    /* get command */
    sscanf (Line, "%s", Command);
    /* xboard commands */
    /* set xboard mode */
    if (!strcmp(Command, "xboard"))
    {
      fprintf(stdout,"feature done=0\n");  
      xboard_mode = true;
      continue;
    }
    /* set epd mode */
    if (!strcmp(Command, "epd"))
    {
      fprintf(stdout,"\n");
      xboard_mode = false;
      continue;
    }
    /* get xboard protocoll version */
    if (!strcmp(Command, "protover")) 
    {
      sscanf (Line, "protover %d", &xboard_protover);
      /* zeta supports only CECP >= v2 */
      if (xboard_mode && xboard_protover<2)
      {
        fprintf(stdout,"Error (unsupported protocoll version,  < v2): protover\n");
        fprintf(stdout,"tellusererror (unsupported protocoll version, < v2): protover\n");
        if (LogFile)
        {
          fprinttime(LogFile);
          fprintf(LogFile,"Error (unsupported protocoll version,  < v2): protover\n");
        }
      }
      else
      {
        /* send feature list to xboard */
        fprintf(stdout,"feature myname=\"Zeta Dva %s\"\n",VERSION);
        fprintf(stdout,"feature ping=0\n");
        fprintf(stdout,"feature setboard=1\n");
        fprintf(stdout,"feature playother=0\n");
        fprintf(stdout,"feature san=0\n");
        /* check feature san accepted  */
        if (!fgets (Line, 1023, stdin)) {}
        /* get command */
        sscanf (Line, "%s", Command);
        if (strstr(Command, "rejected"))
          xboard_san = true;
        fprintf(stdout,"feature usermove=1\n");
        /* check feature usermove accepted  */
        if (!fgets (Line, 1023, stdin)) {}
        /* get command */
        sscanf (Line, "%s", Command);
        if (strstr(Command, "rejected"))
        {
          fprintf(stdout,"Error (unsupported feature usermove): rejected\n");
          fprintf(stdout,"tellusererror (unsupported feature usermove): rejected\n");
          if (LogFile)
          {
            fprinttime(LogFile);
            fprintf(LogFile,"Error (unsupported feature usermove): rejected\n");
          }
          release_inits();
          exit(EXIT_FAILURE);
        }
        fprintf(stdout,"feature time=1\n");
        /* check feature time accepted  */
        if (!fgets (Line, 1023, stdin)) {}
        /* get command */
        sscanf (Line, "%s", Command);
        if (strstr(Command, "accepted"))
          xboard_time = true;
        fprintf(stdout,"feature draw=0\n");
        fprintf(stdout,"feature sigint=0\n");
        fprintf(stdout,"feature reuse=1\n");
        fprintf(stdout,"feature analyze=0\n");
        fprintf(stdout,"feature variants=\"normal\"\n");
        fprintf(stdout,"feature colors=0\n");
        fprintf(stdout,"feature ics=0\n");
        fprintf(stdout,"feature name=0\n");
        fprintf(stdout,"feature pause=0\n");
        fprintf(stdout,"feature nps=0\n");
        fprintf(stdout,"feature debug=1\n");
        /* check feature debug accepted  */
        if (!fgets (Line, 1023, stdin)) {}
        /* get command */
        sscanf (Line, "%s", Command);
        if (strstr(Command, "accepted"))
          xboard_debug = true;
        fprintf(stdout,"feature memory=1\n");
        fprintf(stdout,"feature smp=0\n");
        fprintf(stdout,"feature san=0\n");
        fprintf(stdout,"feature exclude=0\n");
        fprintf(stdout,"feature done=1\n");
      }
      continue;
    }
    if (!strcmp(Command, "accepted")) 
      continue;
    if (!strcmp(Command, "rejected")) 
      continue;
    /* initialize new game */
		if (!strcmp(Command, "new"))
    {
      if (!setboard 
          (BOARD, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"))
      {
        fprintf(stdout,"Error (in setting start postition): new\n");        
        if (LogFile)
        {
          fprinttime(LogFile);
          fprintf(LogFile,"Error (in setting start postition): new\n");        
        }
      }
      initTT();
      if (!xboard_mode)
        printboard(BOARD);
      xboard_force  = false;
			continue;
		}
    /* set board to position in FEN */
		if (!strcmp(Command, "setboard"))
    {
      sscanf (Line, "setboard %1023[0-9a-zA-Z /-]", Fen);
      if(*Fen != '\n' && *Fen != '\0'  && !setboard (BOARD, Fen))
      {
        fprintf(stdout,"Error (in setting chess psotition via fen string): setboard\n");        
        if (LogFile)
        {
          fprinttime(LogFile);
          fprintf(LogFile,"Error (in setting chess psotition via fen string): setboard\n");        
        }
      }
      initTT();
      if (!xboard_mode)
        printboard(BOARD);
      continue;
		}
    if (!strcmp(Command, "go"))
    {
      /* zeta supports only CECP >= v2 */
      if (xboard_mode && xboard_protover<2)
      {
        fprintf(stdout,"Error (unsupported protocoll version, < v2): go\n");
        fprintf(stdout,"tellusererror (unsupported protocoll version. < v2): go\n");
        if (LogFile)
        {
          fprinttime(LogFile);
          fprintf(LogFile,"Error (unsupported protocoll version, < v2): go\n");
        }
      }
      else 
      {
        bool kic = false;
        s32 movecounter = 0;
        Move move;
        Move moves[MAXMOVES];
        xboard_force = false;
        NODECOUNT = 0;
        MOVECOUNT = 0;
        start = get_time();

        HashHistory[PLY] = BOARD[QBBHASH];

        kic = kingincheck(BOARD, STM);
        movecounter = genmoves(BOARD, moves, movecounter, STM, false, 0);
        /* print checkmate and stalemate result */
        if (kic&&movecounter==0)
        {
          if (STM)
          {
            printf("result 1-0 { checkmate }\n");
          }
          else if (!STM)
          {
            printf("result 0-1 { checkmate }\n");
          }
        }
        else if (!kic&&movecounter==0) 
        {
            printf("result 1/2-1/2 { stalemate }\n");
        }
        else 
        {
          /* start thinking */
          move = rootsearch(BOARD,STM, SD);

          end = get_time();   
          elapsed = end-start;
          elapsed /= 1000;
          domove(BOARD, move);
          fprintf(stdout,"move ");
          printmovecan(move);
          fprintf(stdout,"\n");
          if ((!xboard_mode)||xboard_debug)
          {
            printboard(BOARD);
            fprintf(stdout,"#%llu searched nodes in %lf seconds, nps: %llu \n", NODECOUNT, elapsed, (u64)(NODECOUNT/elapsed));
          }
          PLY++;
          STM = !STM;
          HashHistory[PLY] = BOARD[QBBHASH];
          MoveHistory[PLY] = move;
          ScoreHistory[PLY] = BOARD[QBBSCORE];
          CRHistory[PLY] = BOARD[QBBPMVD];
        }
      }
      continue;
    }
    /* set xboard force mode, no thinking just apply moves */
		if (!strcmp(Command, "force"))
    {
      xboard_force = true;
      continue;
    }
    /* set time control */
		if (!strcmp(Command, "level"))
    {
      s32 sec   = 0;
      s32 min   = 0;
      TimeLeft  = 0;
      TimeInc   = 0;
      MovesLeft = 0;
      MaxMoves  = 0;

      if(sscanf(Line, "level %d %d %lf",
               &MaxMoves, &min, &TimeInc)!=3 &&
         sscanf(Line, "level %d %d:%d %lf",
               &MaxMoves, &min, &sec, &TimeInc)!=4)
           continue;

      if (MaxMoves==0)
        timemode = 2; /* ics clocks */
      else
        timemode = 1; /* conventional clocks */
      /* set moves left to 40 in sudden death or ics time control */
      if (timemode==2)
        MovesLeft = 40;
      MovesLeft = MaxMoves;
      TimeLeft  = 60*min + sec;
      TimeLeft *= 1000;
      TimeInc  *= 1000;
      /* set max time per move */
      if (timemode==2)
      {
        MovesLeft = MaxMoves;
        MaxTime   = TimeInc+TimeLeft/MaxMoves;
      }
      if (timemode==1)
      {
        MovesLeft = (MaxMoves-(((PLY+1)/2)%MaxMoves))+1;
        MaxTime   = TimeLeft/MovesLeft;
      }
      continue;
    }
    /* set time control to n seconds per move */
		if (!strcmp(Command, "st"))
    {
      sscanf(Line, "st %lf", &TimeLeft);
      TimeLeft *= 1000; 
      timemode  = 0;
      TimeInc   = 0;
      MovesLeft = MaxMoves = 1; /* jsut one move*/
      MaxTime   = TimeLeft/MaxMoves; /* set max time per move */
      continue;
    }
    /* time left on clock */
		if (!strcmp(Command, "time"))
    {
      if (xboard_time)
      {
        sscanf(Line, "time %lf", &TimeLeft);
        TimeLeft *= 10;  /* centi-seconds to milliseconds */
        /* set max time per move */
        if (timemode==2)
        {
          MovesLeft = MaxMoves;
          MaxTime   = TimeInc+TimeLeft/MaxMoves;
        }
        if (timemode==1)
        {
          MovesLeft = (MaxMoves-(((PLY+1)/2)%MaxMoves))+1;
          MaxTime   = TimeLeft/MovesLeft;
        }
      }
      continue;
    }
    /* opp time left, ignore */
		if (!strcmp(Command, "otim"))
      continue;
    /* memory for hash size  */
		if (!strcmp(Command, "memory"))
    {
      sscanf(Line, "memory %d", &xboardmb);
      initTT();
      continue;
    }
    if (!strcmp(Command, "usermove"))
    {
      bool kic = false;
      s32 movecounter = 0;
      char movec[6];
      Move move;
      Move moves[MAXMOVES];
      /* zeta supports only CECP >= v2 */
      if (xboard_mode && xboard_protover<2)
      {
        fprintf(stdout,"Error (unsupported protocoll version, < v2): usermove\n");
        fprintf(stdout,"tellusererror (unsupported protocoll version, <v2): usermove\n");
        if (LogFile)
        {
          fprinttime(LogFile);
          fprintf(LogFile,"Error (unsupported protocoll version, < v2): usermove\n");
        }
      }
      /* apply given move */
      sscanf (Line, "usermove %s", movec);
      move = can2move(movec, BOARD,STM);
      domove(BOARD, move);
      PLY++;
      STM = !STM;
      HashHistory[PLY] = BOARD[QBBHASH];
      MoveHistory[PLY] = move;
      ScoreHistory[PLY] = BOARD[QBBSCORE];
      CRHistory[PLY] = BOARD[QBBPMVD];
      if (!xboard_mode||xboard_debug)
          printboard(BOARD);
      /* start thinking */
      if (!xboard_force)
      {
        kic = kingincheck(BOARD, STM);
        movecounter = genmoves(BOARD, moves, movecounter, STM, false, 0);
        /* print checkmate and stalemate result */
        if (kic&&movecounter==0)
        {
          if (STM)
          {
            printf("result 1-0 { checkmate }\n");
          }
          else if (!STM)
          {
            printf("result 0-1 { checkmate }\n");
          }
        }
        else if (!kic&&movecounter==0) 
        {
          printf("result 1/2-1/2 { stalemate }\n");
        }
        else 
        {
          /* start thinking */
          move = rootsearch(BOARD,STM, SD);

          end = get_time();   
          elapsed = end-start;
          elapsed /= 1000;
          domove(BOARD, move);
          fprintf(stdout,"move ");
          printmovecan(move);
          fprintf(stdout,"\n");
          if (!xboard_mode||xboard_debug)
          {
            printboard(BOARD);
            fprintf(stdout,"#%llu searched nodes in %lf seconds, nps: %llu \n", NODECOUNT, elapsed, (u64)(NODECOUNT/elapsed));
          }
          PLY++;
          STM = !STM;
          HashHistory[PLY] = BOARD[QBBHASH];
          MoveHistory[PLY] = move;
          ScoreHistory[PLY] = BOARD[QBBSCORE];
          CRHistory[PLY] = BOARD[QBBPMVD];
        }
      }
      continue;
    }
    /* back up one ply */
		if (!strcmp(Command, "undo"))
    {
      if (PLY>0)
      {
        undomove(BOARD, MoveHistory[PLY], MoveHistory[PLY-1], CRHistory[PLY], ScoreHistory[PLY], HashHistory[PLY]);
        PLY--;
        STM = !STM;
      }
      continue;
    }
    /* back up two plies */
		if (!strcmp(Command, "remove"))
    {
      if (PLY>=2)
      {
        undomove(BOARD, MoveHistory[PLY], MoveHistory[PLY-1], CRHistory[PLY], ScoreHistory[PLY], HashHistory[PLY]);
        PLY--;
        STM = !STM;
        undomove(BOARD, MoveHistory[PLY], MoveHistory[PLY-1], CRHistory[PLY], ScoreHistory[PLY], HashHistory[PLY]);
        PLY--;
        STM = !STM;
      }
      continue;
    }
    /* exit program */
		if (!strcmp(Command, "quit"))
    {
      break;
    }
    /* set search depth */
    if (!strcmp(Command, "sd"))
    {
      sscanf (Line, "sd %d", &SD);
      if (SD>=MAXPLY)
        SD = MAXPLY;
      continue;
    }
    /* turn on thinking output */
		if (!strcmp(Command, "post"))
    {
      xboard_post = true;
      continue;
    }
    /* turn off thinking output */
		if (!strcmp(Command, "nopost"))
    {
      xboard_post = false;
      continue;
    }
    /* xboard commands to ignore */
		if (!strcmp(Command, "random"))
    {
      continue;
    }
		if (!strcmp(Command, "white"))
    {
      continue;
    }
		if (!strcmp(Command, "black"))
    {
      continue;
    }
		if (!strcmp(Command, "draw"))
    {
      continue;
    }
		if (!strcmp(Command, "ping"))
    {
      continue;
    }
		if (!strcmp(Command, "result"))
    {
      continue;
    }
		if (!strcmp(Command, "hint"))
    {
      continue;
    }
		if (!strcmp(Command, "bk"))
    {
      continue;
    }
		if (!strcmp(Command, "hard"))
    {
      continue;
    }
		if (!strcmp(Command, "easy"))
    {
      continue;
    }
		if (!strcmp(Command, "name"))
    {
      continue;
    }
		if (!strcmp(Command, "rating"))
    {
      continue;
    }
		if (!strcmp(Command, "ics"))
    {
      continue;
    }
		if (!strcmp(Command, "computer"))
    {
      continue;
    }
    /* non xboard commands */
    /* do an node count to depth defined via sd  */
    if (!xboard_mode && !strcmp(Command, "perft"))
    {
      NODECOUNT = 0;
      MOVECOUNT = 0;

      fprintf(stdout,"### doing perft depth %d: ###\n", SD);  

      start = get_time();

      perft(BOARD, STM, SD);

      end = get_time();   
      elapsed = end-start;
      elapsed /= 1000;

      fprintf(stdout,"nodecount:%llu, seconds: %lf, nps: %llu \n", 
              NODECOUNT, elapsed, (u64)(NODECOUNT/elapsed));

      fflush(stdout);
  
      continue;
    }
    /* do an internal self test */
    if (!xboard_mode && !strcmp(Command, "selftest"))
    {
      selftest();
      continue;
    }
    /* print help */
    if (!xboard_mode && !strcmp(Command, "help"))
    {
      print_help();
      continue;
    }
    /* toggle log flag */
    if (!xboard_mode && !strcmp(Command, "log"))
    {
      /* open/create log file */
      if (!LogFile ) 
      {
        LogFile = fopen("zetadva.log", "a");
        if (!LogFile ) 
        {
          fprintf(stdout,"Error (opening logfile zetadva.log): log");
        }
      }
      continue;
    }
    /* not supported xboard commands...tell user */
		if (!strcmp(Command, "edit"))
    {
      fprintf(stdout,"Error (unsupported command): %s\n",Command);
      fprintf(stdout,"tellusererror (unsupported command): %s\n",Command);
      fprintf(stdout,"tellusererror engine supports only CECP (Xboard) version >=2\n");
      if (LogFile)
      {
        fprinttime(LogFile);
        fprintf(LogFile,"Error (unsupported command): %s\n",Command);
      }
      continue;
    }
		if (
        !strcmp(Command, "analyze")||
        !strcmp(Command, "pause")||
        !strcmp(Command, "resume")
        )
    {
      fprintf(stdout,"Error (unsupported command): %s\n",Command);
      fprintf(stdout,"tellusererror (unsupported command): %s\n",Command);
      if (LogFile)
      {
        fprinttime(LogFile);
        fprintf(LogFile,"Error (unsupported command): %s\n",Command);
      }
      continue;
    }
    /* unknown command...*/
    fprintf(stdout,"Error (unsupported command): %s\n",Command);
    if (LogFile)
    {
      fprinttime(LogFile);
      fprintf(LogFile,"Error (unsupported command): %s\n",Command);
    }
  }
  /* release memory, files and tables */
  release_inits();
  exit(EXIT_SUCCESS);
}

