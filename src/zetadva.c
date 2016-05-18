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

#include "bitboard.h"   /* for population count, pop_count */
#include "eval.h"       /* for evalmove and eval */
#include "movegen.h"    /* for move generator thingies */
#include "search.h"     /* for search algorithms */
#include "timer.h"      /* for time measurement */
#include "types.h"      /* custom types, board defs, data structures, macros */

/* global variables */
FILE 	*Log_File = NULL;       /* logfile for debug */
char *Line;                   /* for fgetting the input on stdin */
char *Command;                /* for pasring the xboard command */
char *Fen;                    /* for storing the fen chess baord string */
/* counters */
u64 NODECOUNT       = 0;
u64 MOVECOUNT       = 0;
/* timers */
double start        = 0;
double end          = 0;
double elapsed      = 0;
/* game state */
bool STM            = WHITE;  /* site to move */
u32 SD              = MAXPLY; /* max search depth*/
u32 GAMEPLY         = 0;      /* total ply, considering depth via fen string */
u32 PLY             = 0;      /* engine specifix ply counter */
Move *Move_History;           /* last game moves indexed by ply */
Hash *Hash_History;           /* last game hashes indexed by ply */
/* Quad Bitboard */
/* based on http://chessprogramming.wikispaces.com/Quad-Bitboards */
/* by Gerd Isenberg */
Bitboard BOARD[6];
/* quad bitboard array index definition
  0   pieces white
  1   piece type first bit
  2   piece type second bit
  3   piece type third bit
  4   64 bit board Zobrist hash
  5   lastmove + ep target + halfmove clock + castle rights + move score
*/
/* forward declarations */
void print_help (void);
void print_version (void);
void self_test (void);
bool setboard (Bitboard *board, char *fenstring);
void createfen (char *fenstring, Bitboard *board, bool stm, int gameply);
void move2alg (Move move, char * movec);
Move alg2move (char *usermove, Bitboard *board, bool stm);
void print_move(Move move);
void print_bitboard(Bitboard board);
void print_board(Bitboard *board);

/* release memory, files and tables */
bool release_inits (void)
{
  /* close log file */
  if (Log_File != NULL)
    fclose (Log_File);
  /* release memory */
  if (Line != NULL) 
    free(Line);
  if (Command != NULL) 
    free(Command);
  if (Fen != NULL) 
    free(Fen);
  if (Move_History != NULL) 
    free(Move_History);
  if (Hash_History != NULL) 
    free(Hash_History);

  return true;
}
/* innitialize memory, files and tables */
bool inits (void)
{
  /* memory allocation */
  Line         = malloc (1024       * sizeof (char));
  Command      = malloc (1024       * sizeof (char));
  Fen          = malloc (1024       * sizeof (char));
  Move_History = malloc (MAXGAMEPLY * sizeof (Move));
  Hash_History = malloc (MAXGAMEPLY * sizeof (Hash));

  if (Line == NULL) 
  {
    printf ("Error (memory allocation failed): char Line[%d]", 1024);
    return false;
  }
  if (Command == NULL) 
  {
    printf ("Error (memory allocation failed): char Command[%d]", 1024);
    return false;
  }
  if (Fen == NULL) 
  {
    printf ("Error (memory allocation failed): char Fen[%d]", 1024);
    return false;
  }
  if (Move_History == NULL) 
  {
    printf ("Error (memory allocation failed): u64 Move_History[%d]",
             MAXGAMEPLY);
    return false;
  }
  if (Hash_History == NULL) 
  {
    printf ("Error (memory allocation failed): u64 Hash_History[%d]",
            MAXGAMEPLY);
    return false;
  }
  return true;
}
/* apply move on board */
void domove (Bitboard *board, Move move)
{
  Square sqfrom   = GETSQFROM(move);
  Square sqto     = GETSQTO(move);
  Square sqcpt    = GETSQCPT(move);
  Piece pfrom     = GETPFROM(move);
  Piece pto       = GETPTO(move);
  Piece pcpt      = GETPCPT(move);
  Bitboard bbTemp = BBEMPTY;
  Piece pcastle   = PNONE;
  u64 hmc;

  /* increase half move clock */
  hmc = GETHMC(move);
  hmc++;
  /* store lastmove in board */
  board[QBBLAST] = move;

  /* unset square from, square capture and square to */
  bbTemp = CLRMASKBB(sqfrom) & CLRMASKBB(sqcpt) & CLRMASKBB(sqto);
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;

  /* set piece to */
  board[QBBBLACK] |= (pto&0x1)<<sqto;
  board[QBBP1]    |= ((pto>>1)&0x1)<<sqto;
  board[QBBP2]    |= ((pto>>2)&0x1)<<sqto;
  board[QBBP3]    |= ((pto>>3)&0x1)<<sqto;

  /* handle castle rook, queenside */
  pcastle = (GETPTYPE(pfrom)==KING&&sqfrom-2==sqto)?
            MAKEPIECE(ROOK,GETCOLOR(pfrom)) : PNONE;

  board[QBBBLACK] |= (pcastle&0x1)<<(sqto+1);
  board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqto+1);
  board[QBBP2]    |= ((pcastle>>2)&0x1)<<(sqto+1);
  board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqto+1);

  /* handle castle rights, clear all of color */
  bbTemp = (pcastle)? (pfrom&BLACK)? CMCRBLACK : CMCRWHITE : BBFULL;
  board[QBBLAST] &= bbTemp;

  /* reset halfmoveclok */
  hmc = (pcastle)? 0 :hmc;

  /* handle castle rook, kingside */
  pcastle = (GETPTYPE(pfrom) == KING && sqto-2==sqfrom)?
            MAKEPIECE(ROOK,GETCOLOR(pfrom)) : PNONE;

  board[QBBBLACK] |= (pcastle&0x1)<<(sqto-1);
  board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqto-1);
  board[QBBP3]    |= ((pcastle>>2)&0x1)<<(sqto-1);
  board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqto-1);

  /* handle castle rights, clear all of color */
  bbTemp = (pcastle)? (pfrom&BLACK)? CMCRBLACK : CMCRWHITE : BBFULL;
  board[QBBLAST] &= bbTemp;

  /* reset halfmoveclok */
  hmc = (pcastle)? 0 :hmc;

  /* handle castle rights, king moved, clear all*/
  bbTemp = (GETPTYPE(pfrom) == KING && (sqfrom == 4 || sqfrom == 60 ))? 
            (pfrom&BLACK)? CMCRBLACK : CMCRWHITE : BBFULL;
  board[QBBLAST] &= bbTemp;

  /* handle castle rights, rook file a moved, clear queenside */
  bbTemp = ( GETPTYPE(pfrom) == ROOK && (sqfrom == 0 || sqfrom == 56 ))?
            (pfrom&BLACK)? CMCRBLACKQ : CMCRWHITEQ : BBFULL;
  board[QBBLAST] &= bbTemp;

  /* handle castle rights, rook file a captured, clear queenside */
  bbTemp = ( GETPTYPE(pcpt) == ROOK && (sqcpt == 0 || sqcpt == 56 ))?
            (pcpt&BLACK)? CMCRBLACKQ : CMCRWHITEQ : BBFULL;
  board[QBBLAST] &= bbTemp;

  /* handle castle rights, rook file h moved, clear kingside */
  bbTemp = ( GETPTYPE(pfrom) == ROOK && (sqfrom == 7 || sqfrom == 63 ))?
            (pfrom&BLACK)? CMCRBLACKK : CMCRWHITEK : BBFULL;
  board[QBBLAST] &= bbTemp;

  /* handle castle rights, rook file h captured, clear kingside */
  bbTemp = ( GETPTYPE(pcpt) == ROOK && (sqcpt == 7 || sqcpt == 63 ))?
            (pcpt&BLACK)? CMCRBLACKK : CMCRWHITEK : BBFULL;
  board[QBBLAST] &= bbTemp;

  /* handle halfmove clock */
  hmc = (GETPTYPE(pfrom) == PAWN )? 0 :hmc; /* pawn move */
  hmc = (GETPTYPE(pcpt) != PNONE )? 0 :hmc; /* capture move */

  /* store hmc in board */  
  board[QBBLAST] = SETHMC(board[QBBLAST], hmc);
}
/* restore board again */
void undomove (Bitboard *board, Move move, Move lastmove)
{
  Square sqfrom   = GETSQFROM(move);
  Square sqto     = GETSQTO(move);
  Square sqcpt    = GETSQCPT(move);
  Piece pfrom     = GETPFROM(move);
  Piece pcpt      = GETPCPT(move);
  Bitboard bbTemp = BBEMPTY;
  Piece pcastle   = PNONE;

  /* restore lastmove with hmc, cr and score */
  board[QBBLAST] = lastmove;

  /* unset square capture, square to */
  bbTemp = CLRMASKBB(sqcpt) & CLRMASKBB(sqto);
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

  /* restore castle rook, queenside */
  /* restore castle rook, queenside */
  pcastle = (GETPTYPE(pfrom) == KING && sqfrom-sqto == 2)? 
            MAKEPIECE(ROOK,GETCOLOR(pfrom)) : PNONE;

  board[QBBBLACK] |= (pcastle&0x1)<<(sqfrom-4);
  board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqfrom-4);
  board[QBBP2]    |= ((pcastle>>2)&0x1)<<(sqfrom-4);
  board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqfrom-4);

  /* restore castle rook, kingside */
  /* handle castle rook, kingside */
  pcastle = (GETPTYPE(pfrom) == KING && sqto-2==sqfrom)?
            MAKEPIECE(ROOK,GETCOLOR(pfrom)) : PNONE;

  board[QBBBLACK] |= (pcastle&0x1)<<(sqfrom+3);
  board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqfrom+3);
  board[QBBP2]    |= ((pcastle>>2)&0x1)<<(sqfrom+3);
  board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqfrom+3);
}
/* is square attacked by an enemy piece, via superpiece approach */
bool squareunderattack (Bitboard *board, Square sq, bool stm) 
{

  Bitboard bbWrap;
  Bitboard bbGen;
  Bitboard bbPro;
  Bitboard bbWork;
  Bitboard bbMoves;
  Bitboard bbBlockers;
  Bitboard bbBoth[2];

  bbBlockers    = board[QBBP1]|board[QBBP2]|board[QBBP3];
  bbBoth[WHITE] = board[QBBBLACK]^bbBlockers;
  bbBoth[BLACK] = board[QBBBLACK];

  bbMoves = BBEMPTY;
  /* directions left shifting <<1 ROOK */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTAFILE;
  bbPro  &= bbWrap;
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen << 1);
  bbPro  &=           (bbPro << 1);
  bbGen  |= bbPro &   (bbGen << 2*1);
  bbPro  &=           (bbPro << 2*1);
  bbGen  |= bbPro &   (bbGen << 4*1);
  /* shift one further */
  bbGen   = bbWrap &  (bbGen << 1);
  bbMoves|= bbGen;

  /* directions left shifting <<8 ROOK */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen << 8);
  bbPro  &=           (bbPro << 8);
  bbGen  |= bbPro &   (bbGen << 2*8);
  bbPro  &=           (bbPro << 2*8);
  bbGen  |= bbPro &   (bbGen << 4*8);
  /* shift one further */
  bbGen   =           (bbGen << 8);
  bbMoves|= bbGen;

  /* directions right shifting >>1 ROOK */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTHFILE;
  bbPro  &= bbWrap;
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen >> 1);
  bbPro  &=           (bbPro >> 1);
  bbGen  |= bbPro &   (bbGen >> 2*1);
  bbPro  &=           (bbPro >> 2*1);
  bbGen  |= bbPro &   (bbGen >> 4*1);
  /* shift one further */
  bbGen   = bbWrap &  (bbGen >> 1);
  bbMoves|= bbGen;

  /* directions right shifting >>8 ROOK */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen >> 8);
  bbPro  &=           (bbPro >> 8);
  bbGen  |= bbPro &   (bbGen >> 2*8);
  bbPro  &=           (bbPro >> 2*8);
  bbGen  |= bbPro &   (bbGen >> 4*8);
  /* shift one further */
  bbGen   =           (bbGen >> 8);
  bbMoves|= bbGen;

  /* rooks and queens */
  bbWork =    (bbBoth[stm]&(board[QBBP1]&~board[QBBP2]&board[QBBP3])) 
            | (bbBoth[stm]&(~board[QBBP1]&board[QBBP2]&board[QBBP3]));
  if (bbMoves&bbWork)
  {
    return true;
  }

  bbMoves = BBEMPTY;
  /* directions left shifting <<9 BISHOP */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);

  bbWrap  = BBNOTAFILE;
  bbPro  &= bbWrap;
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen << 9);
  bbPro  &=           (bbPro << 9);
  bbGen  |= bbPro &   (bbGen << 2*9);
  bbPro  &=           (bbPro << 2*9);
  bbGen  |= bbPro &   (bbGen << 4*9);
  /* shift one further */
  bbGen   = bbWrap &  (bbGen << 9);
  bbMoves|= bbGen;

  /* directions left shifting <<7 BISHOP */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTHFILE;
  bbPro  &= bbWrap;
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen << 7);
  bbPro  &=           (bbPro << 7);
  bbGen  |= bbPro &   (bbGen << 2*7);
  bbPro  &=           (bbPro << 2*7);
  bbGen  |= bbPro &   (bbGen << 4*7);
  /* shift one further */
  bbGen   = bbWrap &  (bbGen << 7);
  bbMoves|= bbGen;

  /* directions right shifting >>9 ROOK */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTHFILE;
  bbPro  &= bbWrap;
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen >> 9);
  bbPro  &=           (bbPro >> 9);
  bbGen  |= bbPro &   (bbGen >> 2*9);
  bbPro  &=           (bbPro >> 2*9);
  bbGen  |= bbPro &   (bbGen >> 4*9);
  /* shift one further */
  bbGen   = bbWrap &  (bbGen >> 9);
  bbMoves|= bbGen;

  /* directions right shifting <<7 ROOK */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTAFILE;
  bbPro  &= bbWrap;
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen >> 7);
  bbPro  &=           (bbPro >> 7);
  bbGen  |= bbPro &   (bbGen >> 2*7);
  bbPro  &=           (bbPro >> 2*7);
  bbGen  |= bbPro &   (bbGen >> 4*7);
  /* shift one further */
  bbGen   = bbWrap &  (bbGen >> 7);
  bbMoves|= bbGen;

  /* bishops and queens */
  bbWork =  (bbBoth[stm]&(~board[QBBP1]&~board[QBBP2]&board[QBBP3])) 
          | (bbBoth[stm]&(~board[QBBP1]&board[QBBP2]&board[QBBP3]));
  if (bbMoves&bbWork)
  {
    return true;
  }
  /* knights */
  bbWork = bbBoth[stm]&(~board[QBBP1]&board[QBBP2]&~board[QBBP3]);
  bbMoves = AttackTablesTo[stm*7*64+KNIGHT*64+sq] ;
  if (bbMoves&bbWork) 
  {
    return true;
  }
  /* pawns */
  bbWork = bbBoth[stm]&(board[QBBP1]&~board[QBBP2]&~board[QBBP3]);
  bbMoves = AttackTablesTo[stm*7*64+PAWN*64+sq];
  if (bbMoves&bbWork)
  {
    return true;
  }
  /* king */
  bbWork = bbBoth[stm]&(board[QBBP1]&board[QBBP2]&~board[QBBP3]);
  bbMoves = AttackTablesTo[stm*7*64+KING*64+sq];
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
  Bitboard bbKing = board[QBBP1]&board[QBBP2]&~board[QBBP3]; /* get kings */

  /* get colored king */
  bbKing &= (stm)? board[QBBBLACK] : 
            (board[QBBBLACK]^(board[QBBP1]|board[QBBP2]|board[QBBP3]));

  sqking  = first1 (bbKing);

  return squareunderattack (board, sqking, !stm);
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
void print_bitboard (Bitboard board)
{

  int rank;
  int file;
  Square sq;

  printf ("###ABCDEFGH###\n");
  for(rank = RANK_8; rank >= RANK_1; rank--) {
    printf ("#%i ",rank+1);
    for(file = FILE_A; file < FILE_NONE; file++) {
      sq = MAKESQ(file, rank);
      if (board&SETMASKBB(sq)) 
        printf ("x");
      else 
        printf ("-");
    }
    printf ("\n");
  }
  printf ("###ABCDEFGH###\n");

  fflush(stdout);
}
void print_move (Move move)
{
  printf ("sqfrom:%llu\n",GETSQFROM(move));
  printf ("sqto:%llu\n",GETSQTO(move));
  printf ("sqcpt:%llu\n",GETSQCPT(move));
  printf ("pfrom:%llu\n",GETPFROM(move));
  printf ("pto:%llu\n",GETPTO(move));
  printf ("pcpt:%llu\n",GETPCPT(move));
  printf ("sqep:%llu\n",GETSQEP(move));
  printf ("cr:%llx\n",GETCR(move));
  printf ("hmc:%u\n",(u32)GETHMC(move));
  printf ("score:%i\n",(Score)GETSCORE(move));
}
/* move in algebraic notation, eg. e2e4, to internal packed move  */
Move alg2move (char *usermove, Bitboard *board, bool stm) 
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

  /* en passant move */
  sqcpt = ( (pfrom>>1) == PAWN && (stm == WHITE) && GETRANK(sqfrom) == RANK_5  
            && sqto-sqfrom != 8 && (pcpt>>1) == PNONE ) ? sqto-8 : sqcpt;
  sqcpt = ( (pfrom>>1) == PAWN && (stm == BLACK) && GETRANK(sqfrom) == RANK_4  
            && sqfrom-sqto != 8 && (pcpt>>1) == PNONE ) ? sqto+8 : sqcpt;

  pcpt = GETPIECE(board, sqcpt);

  /* pawn double square move, set en passant target square */
  if ( (pfrom>>1) == PAWN && GETRRANK(sqto,stm) - GETRRANK(sqfrom,stm) == 2 )
    sqep = sqto;

  /* pawn promo piece */
  promopiece = usermove[4];
  if (promopiece == 'q' || promopiece == 'Q' )
      pto = MAKEPIECE(QUEEN,(u64)stm);
  else if (promopiece == 'n' || promopiece == 'N' )
      pto = MAKEPIECE(KNIGHT,(u64)stm);
  else if (promopiece == 'b' || promopiece == 'B' )
      pto = MAKEPIECE(BISHOP,(u64)stm);
  else if (promopiece == 'r' || promopiece == 'R' )
      pto = MAKEPIECE(ROOK,(u64)stm);

  /* pack move, considering hmc, cr and score */
  move = MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto , pcpt, sqep,
                    GETHMC(board[QBBLAST]) ,
                    GETCR(board[QBBLAST]),
                    (u64)0);

  return move;
}
/* packed move to move in algebraic notation, eg. e2e4 */
void move2alg (Move move, char * movec) 
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
  movec[4] = ' ';
  movec[5] = '\0';

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
  }
}
/* print quadbitbooard */
void print_board (Bitboard *board)
{

  int rank;
  int file;
  Square sq;
  Piece piece;
  char wpchars[] = "-PNKBRQ";
  char bpchars[] = "-pnkbrq";
  char fenstring[1024];

/*
print_bitboard(board[0]);
print_bitboard(board[1]);
print_bitboard(board[2]);
print_bitboard(board[3]);
print_bitboard(board[4]);
print_bitboard(board[5]);
*/
  printf ("###ABCDEFGH###\n");
  for (rank = RANK_8; rank >= RANK_1; rank--) 
  {
    printf ("#%i ",rank+1);
    for (file = FILE_A; file < FILE_NONE; file++)
    {
      sq = MAKESQ(file, rank);
      piece = GETPIECE(board, sq);
      if (piece != PNONE && (piece&BLACK))
        printf ("%c", bpchars[piece>>1]);
      else if (piece != PNONE)
        printf ("%c", wpchars[piece>>1]);
      else 
        printf ("-");
    }
    printf ("\n");
  }
  printf ("###ABCDEFGH###\n");

  createfen (fenstring, BOARD, STM, GAMEPLY);

  printf ("#fen: %s\n",fenstring);

  fflush (stdout);
}
/* create fen string from board state */
void createfen (char *fenstring, Bitboard *board, bool stm, int gameply)
{

  int rank;
  int file;
  Square sq;
  Piece piece;
  char wpchars[] = " PNKBRQ";
  char bpchars[] = " pnkbrq";
  char rankc[8] = "12345678";
  char filec[8] = "abcdefgh";
  char *stringptr = fenstring;
  int spaces = 0;

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
        stringptr+=sprintf (stringptr, "%d", spaces);
        spaces=0;
      }
      /* handle pieces, black and white */
      if (piece != PNONE && (piece&BLACK))
        stringptr+=sprintf (stringptr, "%c", bpchars[piece>>1]);
      else if (piece != PNONE)
        stringptr+=sprintf (stringptr, "%c", wpchars[piece>>1]);
      else
        spaces++;
    }
    /* handle empty squares */
    if (spaces > 0)
    {
      stringptr+=sprintf (stringptr, "%d", spaces);
      spaces=0;
    }
    /* handle rows delimeter */
    if (rank <= RANK_8 && rank > RANK_1)
      stringptr+=sprintf (stringptr, "/");
  }

  stringptr+=sprintf (stringptr, " ");

  /* add site to move */
  if (stm&BLACK)
  {
    stringptr+=sprintf (stringptr, "b");
  }
  else
  {
    stringptr+=sprintf (stringptr, "w");
  }

  stringptr+=sprintf (stringptr, " ");

  /* add castle rights */
  if (!(board[QBBLAST] & SMCRALL))
    stringptr+=sprintf (stringptr, "-");
  else
  {
    /* white kingside */
    if (board[QBBLAST] & SMCRWHITEK)
      stringptr+=sprintf (stringptr, "K");
    /* white queenside */
    if (board[QBBLAST] & SMCRWHITEQ)
      stringptr+=sprintf (stringptr, "Q");
    /* black kingside */
    if (board[QBBLAST] & SMCRBLACKK)
      stringptr+=sprintf (stringptr, "k");
    /* black queenside */
    if (board[QBBLAST] & SMCRBLACKQ)
      stringptr+=sprintf (stringptr, "q");
  }

  stringptr+=sprintf (stringptr," ");

  /* add en passant target square */
  sq = GETSQEP(board[QBBLAST]);
  if (sq > 0)
  {
    stringptr+=sprintf (stringptr, "%c", filec[GETFILE(sq)]);
    stringptr+=sprintf (stringptr, "%c", rankc[GETRANK(sq)]);
  }
  else
    stringptr+=sprintf (stringptr, "-");

  stringptr+=sprintf (stringptr," ");

  /* add halpfmove clock  */
  stringptr+=sprintf (stringptr, "%d",GETHMC(board[QBBLAST]));
  stringptr+=sprintf (stringptr, " ");

  stringptr+=sprintf (stringptr, "%d", (gameply/2));

}
/* set internal chess board presentation to fen string */
bool setboard (Bitboard *board, char *fenstring)
{
  char tempchar;
  char *position; /* piece types and position, row_8, file_a, to row_1, file_h*/
  char *cstm;     /* site to move */
  char *castle;   /* castle rights */
  char *cep;      /* en passant target square */
  char fencharstring[24] = {" PNKBRQ pnkbrq/12345678"}; /* mapping */
  u64 i;
  u64 j;
  u64 hmc;        /* half move clock */
  u64 fendepth;   /* game depth */
  File file;
  Rank rank;
  Piece piece;
  Square sq;
  Move lastmove = MOVENONE;

  /* memory, fen string ist max 1023 char in size */
  position  = malloc (1024 * sizeof (char));
  if (position == NULL) 
  {
    printf ("Error (memory allocation failed): char position[%d]", 1024);
    return false;
  }
  cstm  = malloc (1024 * sizeof (char));
  if (cstm == NULL) 
  {
    printf ("Error (memory allocation failed): char cstm[%d]", 1024);
    return false;
  }
  castle  = malloc (1024 * sizeof (char));
  if (castle == NULL) 
  {
    printf ("Error (memory allocation failed): char castle[%d]", 1024);
    return false;
  }
  cep  = malloc (1024 * sizeof (char));
  if (cep == NULL) 
  {
    printf ("Error (memory allocation failed): char cep[%d]", 1024);
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
  board[QBBHASH]  = 0x0ULL;
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
        lastmove |= SMCRWHITEQ;
      /* white kingside */
      if (tempchar == 'K')
        lastmove |= SMCRWHITEK;
      /* black queenside */
      if (tempchar == 'q')
        lastmove |= SMCRBLACKQ;
      /* black kingside */
      if (tempchar == 'k')
        lastmove |= SMCRBLACKK;
      i++;
      tempchar = castle[i];
    }
  }
  /* store castle rights into lastmove */
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
  lastmove = SETSQEP (lastmove, sq);

  /* ply starts at zero */
  PLY = 0;
  /* game ply can be more */
  GAMEPLY = fendepth*2+STM;

  /* TODO: compute  hash
  board[QBBHASH] = compute_hash(BOARD);
  Hash_History[PLY] = compute_hash(BOARD);
  */

  /* TODO: validity check for two opposing kings present on board */

  /* store lastmove+ in board */
  board[QBBLAST] = lastmove;

  /* board valid check */
  if (!isvalid(board))
  {
    printf ("Error (given fen position is illegal): setboard\n");        
    return false;
  }

  return true;
}
/* run internal selftest */
void self_test (void) 
{
  u64 perftdepth4  = 197281;

  NODECOUNT = 0;
  MOVECOUNT = 0;
  SD = 4;

  printf ("# doing perft depth: %u for position\n", SD);  

  if (!setboard 
      (BOARD, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"))
  {
    printf ("# Error (in setting start postition): new\n");        
    return;
  }
  else
    print_board(BOARD);
  
  start = get_time();

  perft (BOARD, STM, 0);

  end = get_time();   
  elapsed = end-start;
  elapsed /= 1000;

  if(NODECOUNT==perftdepth4)
    printf ("# Nodecount Correct, %llu nodes in %f seconds with %llu nps.\n", NODECOUNT, elapsed, (u64)(NODECOUNT/elapsed));
  else
    printf ("# Nodecount Not Correct, %llu computed nodes != %llu nodes for depth 4.\n", NODECOUNT, perftdepth4);

  return;
}
/* print engine info to console */
void print_version (void)
{
  printf ("Zeta Dva version: %s\n",VERSION);
  printf ("Yet another amateur level chess engine.\n");
  printf ("Copyright (C) 2011-2016 Srdja Matovic, Montenegro\n");
  printf ("This is free software, licensed under GPL >= v2\n");
}
/* engine options and usage */
void print_help (void)
{
  printf ("Zeta Dva, yet another amateur level chess engine.\n");
  printf ("\n");
  printf ("Options:\n");
  printf (" -l, --log          Write output/debug to file zetadva.log\n");
  printf (" -v, --version      Print Zeta Dva version info.\n");
  printf (" -h, --help         Print Zeta Dva program usage help.\n");
  printf (" -s, --selftest     Run an internal test, usefull after compile.\n");
  printf ("\n");
  printf ("To play against the engine use an CECP v2 protocol capable chess GUI\n");
  printf ("like Arena, Winboard or Xboard.\n");
  printf ("\n");
  printf ("Alternatively you can use Xboard commmands directly on commman Line,\n"); 
  printf ("e.g.:\n");
  printf ("new            // init new game from start position\n");
  printf ("level 40 4 0   // set time control to 40 moves in 4 minutes\n");
  printf ("go             // let engine play site to move\n");
  printf ("usermove d7d5  // let engine apply usermove and start thinking\n");
  printf ("\n");
  printf ("Non-Xboard commands:\n");
  printf ("perft          // perform a performance test to depth set by sd command\n");
  printf ("selftest       // run an internal test\n");
  printf ("help           // print usage hints\n");
  printf ("log            // toggle log flag\n");
  printf ("\n");
  printf ("Not supported Xboard commands:\n");
  printf ("analyze        // enter analyze mode\n");
  printf ("undo/remove    // take back last moves\n");
  printf ("?              // move now\n");
  printf ("draw           // handle draw offers\n");
  printf ("hard/easy      // turn on pondering\n");
  printf ("hint           // give user a hint move\n");
  printf ("bk             // book Lines\n");
  printf ("pause/resume   // pause the engine\n");
  printf ("\n");
}
/* Zeta Dva, amateur level chess engine  */
int main (int argc, char* argv[])
{
  /* xboard states */
  bool xboard_mode    = false;  /* chess GUI sets to true */
  bool xboard_force   = false;  /* if true aplly only moves, do not think */
  bool xboard_post    = false;  /* post search thinking output */
  s32 xboard_protover = 0;      /* Zeta works with protocoll version >= v2 */
  /* for get opt */
  s32 c;
  static struct option long_options[] = 
  {
    {"help", 0, 0, 'h'},
    {"version", 0, 0, 'v'},
    {"log", 0, 0, 'l'},
    {"selftest", 0, 0, 's'},
    {NULL, 0, NULL, 0}
  };
  s32 option_index = 0;

  /* no buffers */
  setbuf (stdout, NULL);
  setbuf (stdin, NULL);

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
        /* open/create log file */
        Log_File = fopen ("zetadva.log", "a");
        if (Log_File == NULL) 
        {
          printf ("Error (opening logfile zetadva.log): --log");
          return false;
        }
        break;
      case 3:
        self_test ();
        exit (EXIT_SUCCESS);
        break;
    }
  }
  /* init memory, files and tables */
  if (!inits ())
  {
    release_inits ();
    exit (EXIT_FAILURE);
  }
  /* open log file */
  if (Log_File != NULL)
  {
    char timestring[256];
    /* no buffers */
    setbuf(Log_File, NULL);
    /* print binary call to log */
    get_time_string (timestring);
    fprintf (Log_File, "%s, ", timestring);
    for (c=0;c<argc;c++)
    {
      fprintf (Log_File, "%s ",argv[c]);
    }
    fprintf (Log_File, "\n");
  }
  /* print engine info to console */
  printf ("Zeta Dva %s\n",VERSION);
  printf ("Yet another amateur level chess engine.\n");
  printf ("Copyright (C) 2011-2016 Srdja Matovic, Montenegro\n");
  printf ("This is free software, licensed under GPL >= v2\n");

  /* init starting position */
  setboard(BOARD,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  /* xboard command loop */
  for (;;)
  {
    /* console mode */
    if (!xboard_mode)
      printf ("> ");
    /* just to be sure, flush the output...*/
    fflush (stdout);
    /* get Line */
    if (!fgets (Line, 1023, stdin)) {}
    /* ignore empty Lines */
    if (Line[0] == '\n')
      continue;
    /* print io to log file */
    if (Log_File != NULL)
    {
      char timestring[256];
      get_time_string (timestring);
      fprintf (Log_File, "%s, ", timestring);
      fprintf (Log_File, "%s\n",Line);
    }
    /* get command */
    sscanf (Line, "%s", Command);
    /* xboard commands */
    /* set xboard mode */
    if (!strcmp (Command, "xboard"))
    {
      printf ("feature done=0\n");  
      xboard_mode = true;
      continue;
    }
    /* get xboard protocoll version */
    if (!strcmp(Command, "protover")) 
    {
      sscanf (Line, "protover %d", &xboard_protover);
      /* zeta supports only CECP >= v2 */
      if (xboard_mode && xboard_protover<2)
      {
        printf ("Error (unsupported protocoll version,  < v2): protover\n");
        printf ("tellusererror (unsupported protocoll version, < v2): protover\n");
      }
      else
      {
        /* send feature list to xboard */
        printf ("feature myname=\"Zeta Dva %s\"\n",VERSION);
        printf ("feature ping=0\n");
        printf ("feature setboard=1\n");
        printf ("feature playother=0\n");
        printf ("feature san=0\n");
        printf ("feature usermove=1\n");
        printf ("feature time=1\n");
        printf ("feature draw=0\n");
        printf ("feature sigint=0\n");
        printf ("feature reuse=1\n");
        printf ("feature analyze=0\n");
        printf ("feature variants=normal\n");
        printf ("feature colors=0\n");
        printf ("feature ics=0\n");
        printf ("feature name=0\n");
        printf ("feature pause=0\n");
        printf ("feature nps=0\n");
        printf ("feature debug=0\n");
        printf ("feature memory=1\n");
        printf ("feature smp=0\n");
        printf ("feature san=0\n");
        printf ("feature debug=0\n");
        printf ("feature exclude=0\n");
        printf ("feature setscore=0\n");
        printf ("feature highlight=0\n");
        printf ("feature setscore=0\n");
        printf ("feature done=1\n");
      }
      continue;
    }
    /* initialize new game */
		if (!strcmp (Command, "new"))
    {
      if (!setboard 
          (BOARD, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"))
      {
        printf ("Error (in setting start postition): new\n");        
      }
      if (!xboard_mode)
        print_board(BOARD);
      xboard_force  = false;
			continue;
		}
    /* set board to position in FEN */
		if (!strcmp (Command, "setboard"))
    {
      sscanf (Line, "setboard %1023[0-9a-zA-Z /-]", Fen);
      if(!setboard (BOARD, Fen))
      {
        printf ("Error (in setting chess psotition via fen string): setboard\n");        
      }
      if (!xboard_mode)
        print_board(BOARD);
      continue;
		}
    if (!strcmp(Command, "go"))
    {
      /* zeta supports only CECP >= v2 */
      if (xboard_mode && xboard_protover<2)
      {
        printf ("Error (unsupported protocoll version, < v2): go\n");
        printf ("tellusererror (unsupported protocoll version. < v2): go\n");
      }
      xboard_force = false;
      PLY++;
      STM = !STM;
      continue;
    }
    /* set xboard force mode, no thinking just apply moves */
		if (!strcmp (Command, "force"))
    {
      xboard_force = true;
      continue;
    }
    /* set time control */
		if (!strcmp (Command, "level"))
    {
      continue;
    }
    /* set time control to n seconds per move */
		if (!strcmp (Command, "st"))
    {
      continue;
    }
    if (!strcmp(Command, "usermove"))
    {
      /* zeta supports only CECP >= v2 */
      if (xboard_mode && xboard_protover<2)
      {
        printf ("Error (unsupported protocoll version, < v2): usermove\n");
        printf ("tellusererror (unsupported protocoll version, <v2): usermove\n");
      }
      PLY++;
      STM = !STM;
      continue;
    }
    /* exit program */
		if (!strcmp (Command, "quit"))
    {
      break;
    }
    /* set search depth */
    if (!strcmp (Command, "sd"))
    {
      sscanf (Line, "sd %u", &SD);
      continue;
    }
    /* turn on thinking output */
		if (!strcmp (Command, "post"))
    {
      xboard_post = true;
      continue;
    }
    /* turn off thinking output */
		if (!strcmp (Command, "nopost"))
    {
      xboard_post = false;
      continue;
    }
    /* xboard commands to ignore */
		if (!strcmp (Command, "white"))
    {
      continue;
    }
		if (!strcmp (Command, "black"))
    {
      continue;
    }
		if (!strcmp (Command, "draw"))
    {
      continue;
    }
		if (!strcmp (Command, "ping"))
    {
      continue;
    }
		if (!strcmp (Command, "result"))
    {
      continue;
    }
		if (!strcmp (Command, "hint"))
    {
      continue;
    }
		if (!strcmp (Command, "bk"))
    {
      continue;
    }
		if (!strcmp (Command, "hard"))
    {
      continue;
    }
		if (!strcmp (Command, "easy"))
    {
      continue;
    }
		if (!strcmp (Command, "name"))
    {
      continue;
    }
		if (!strcmp (Command, "rating"))
    {
      continue;
    }
		if (!strcmp (Command, "ics"))
    {
      continue;
    }
		if (!strcmp (Command, "computer"))
    {
      continue;
    }
    /* non xboard commands */
    /* do an node count to depth defined via sd  */
    if (!xboard_mode && !strcmp (Command, "perft"))
    {

      NODECOUNT = 0;
      MOVECOUNT = 0;

      printf ("#doing perft depth %u:\n", SD);  

      start = get_time();

      perft (BOARD, STM, 0);

      end = get_time();   
      elapsed = end-start;
      elapsed /= 1000;

      printf ("%llu nodes, seconds: %f, nps: %llu \n", NODECOUNT, elapsed, (u64)(NODECOUNT/elapsed));

      continue;
    }
    /* do an internal self test */
    if (!xboard_mode && !strcmp (Command, "selftest"))
    {
      self_test();
      continue;
    }
    /* print help */
    if (!xboard_mode && !strcmp (Command, "help"))
    {
      print_help();
      continue;
    }
    /* toggle log flag */
    if (!xboard_mode && !strcmp (Command, "log"))
    {
      /* close log file */
      if (Log_File != NULL)
        fclose (Log_File);
      /* open/create log file */
      else if (Log_File == NULL) 
      {
        Log_File = fopen ("zetadva.log", "a");
        if (Log_File == NULL) 
        {
          printf ("Error (opening logfile zetadva.log): log");
        }
      }
      continue;
    }
    /* not supported xboard commands...tell user */
		if (!strcmp (Command, "edit"))
    {
      printf ("Error (unsupported command): %s\n",Command);
      printf ("tellusererror (unsupported command): %s\n",Command);
      printf ("tellusererror engine supports only CECP (Xboard) version >=2\n");
      continue;
    }
		if (!strcmp (Command, "undo"))
    {
      printf ("Error (unsupported command): %s\n",Command);
      printf ("tellusererror (unsupported command): %s\n",Command);
      continue;
    }
		if (!strcmp (Command, "remove"))
    {
      printf ("Error (unsupported command): %s\n",Command);
      printf ("tellusererror (unsupported command): %s\n",Command);
      continue;
    }
		if (!strcmp (Command, "remove"))
    {
      printf ("Error (unsupported command): %s\n",Command);
      printf ("tellusererror (unsupported command): %s\n",Command);
      continue;
    }
		if (!strcmp (Command, "analyze"))
    {
      printf ("Error (unsupported command): %s\n",Command);
      printf ("tellusererror (unsupported command): %s\n",Command);
      continue;
    }
		if (!strcmp (Command, "pause"))
    {
      printf ("Error (unsupported command): %s\n",Command);
      printf ("tellusererror (unsupported command): %s\n",Command);
      continue;
    }
		if (!strcmp (Command, "resume"))
    {
      printf ("Error (unsupported command): %s\n",Command);
      printf ("tellusererror (unsupported command): %s\n",Command);
      continue;
    }
    /* unknown command...*/
    printf ("Error (unsupported command): %s\n",Command);
  }

  /* release memory, files and tables */
  release_inits ();

  exit (EXIT_SUCCESS);
}

