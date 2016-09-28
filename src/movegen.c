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

#include "bitboard.h"   /* for population count, pop_count */
#include "eval.h"       /* for evalmove and eval */
#include "types.h"      /* custom types, board defs, data structures, macros */
#include "zetadva.h"    /* for global vars */

/* move generation is done via generalized KoggeStone bitboard approach */
/* based on work by Steffan Westcott */
/* http://chessprogramming.wikispaces.com/Kogge-Stone+Algorithm */

/* move generator constants */
/* pawn pushes, simple and duble */
const Bitboard AttackTablesPawnPushes[2*64] = 
{
  /* white pawn pushes */
  0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000,0x1010000,0x2020000,0x4040000,0x8080000,0x10100000,0x20200000,0x40400000,0x80800000,0x1000000,0x2000000,0x4000000,0x8000000,0x10000000,0x20000000,0x40000000,0x80000000,0x100000000,0x200000000,0x400000000,0x800000000,0x1000000000,0x2000000000,0x4000000000,0x8000000000,0x10000000000,0x20000000000,0x40000000000,0x80000000000,0x100000000000,0x200000000000,0x400000000000,0x800000000000,0x1000000000000,0x2000000000000,0x4000000000000,0x8000000000000,0x10000000000000,0x20000000000000,0x40000000000000,0x80000000000000,0x100000000000000,0x200000000000000,0x400000000000000,0x800000000000000,0x1000000000000000,0x2000000000000000,0x4000000000000000,0x8000000000000000,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
  /* black pawn pushes */
  0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x1,0x2,0x4,0x8,0x10,0x20,0x40,0x80,0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000,0x10000,0x20000,0x40000,0x80000,0x100000,0x200000,0x400000,0x800000,0x1000000,0x2000000,0x4000000,0x8000000,0x10000000,0x20000000,0x40000000,0x80000000,0x100000000,0x200000000,0x400000000,0x800000000,0x1000000000,0x2000000000,0x4000000000,0x8000000000,0x10100000000,0x20200000000,0x40400000000,0x80800000000,0x101000000000,0x202000000000,0x404000000000,0x808000000000,0x1000000000000,0x2000000000000,0x4000000000000,0x8000000000000,0x10000000000000,0x20000000000000,0x40000000000000,0x80000000000000
};
/* piece attack tables */
const Bitboard AttackTables[7*64] = 
{
  /* white pawn */
  0x200,0x500,0xa00,0x1400,0x2800,0x5000,0xa000,0x4000,0x20000,0x50000,0xa0000,0x140000,0x280000,0x500000,0xa00000,0x400000,0x2000000,0x5000000,0xa000000,0x14000000,0x28000000,0x50000000,0xa0000000,0x40000000,0x200000000,0x500000000,0xa00000000,0x1400000000,0x2800000000,0x5000000000,0xa000000000,0x4000000000,0x20000000000,0x50000000000,0xa0000000000,0x140000000000,0x280000000000,0x500000000000,0xa00000000000,0x400000000000,0x2000000000000,0x5000000000000,0xa000000000000,0x14000000000000,0x28000000000000,0x50000000000000,0xa0000000000000,0x40000000000000,0x200000000000000,0x500000000000000,0xa00000000000000,0x1400000000000000,0x2800000000000000,0x5000000000000000,0xa000000000000000,0x4000000000000000,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
  /* black pawn */
  0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x2,0x5,0xa,0x14,0x28,0x50,0xa0,0x40,0x200,0x500,0xa00,0x1400,0x2800,0x5000,0xa000,0x4000,0x20000,0x50000,0xa0000,0x140000,0x280000,0x500000,0xa00000,0x400000,0x2000000,0x5000000,0xa000000,0x14000000,0x28000000,0x50000000,0xa0000000,0x40000000,0x200000000,0x500000000,0xa00000000,0x1400000000,0x2800000000,0x5000000000,0xa000000000,0x4000000000,0x20000000000,0x50000000000,0xa0000000000,0x140000000000,0x280000000000,0x500000000000,0xa00000000000,0x400000000000,0x2000000000000,0x5000000000000,0xa000000000000,0x14000000000000,0x28000000000000,0x50000000000000,0xa0000000000000,0x40000000000000,
  /* knight */
  0x20400,0x50800,0xa1100,0x142200,0x284400,0x508800,0xa01000,0x402000,0x2040004,0x5080008,0xa110011,0x14220022,0x28440044,0x50880088,0xa0100010,0x40200020,0x204000402,0x508000805,0xa1100110a,0x1422002214,0x2844004428,0x5088008850,0xa0100010a0,0x4020002040,0x20400040200,0x50800080500,0xa1100110a00,0x142200221400,0x284400442800,0x508800885000,0xa0100010a000,0x402000204000,0x2040004020000,0x5080008050000,0xa1100110a0000,0x14220022140000,0x28440044280000,0x50880088500000,0xa0100010a00000,0x40200020400000,0x204000402000000,0x508000805000000,0xa1100110a000000,0x1422002214000000,0x2844004428000000,0x5088008850000000,0xa0100010a0000000,0x4020002040000000,0x400040200000000,0x800080500000000,0x1100110a00000000,0x2200221400000000,0x4400442800000000,0x8800885000000000,0x100010a000000000,0x2000204000000000,0x4020000000000,0x8050000000000,0x110a0000000000,0x22140000000000,0x44280000000000,0x88500000000000,0x10a00000000000,0x20400000000000,
  /* king */
  0x302,0x705,0xe0a,0x1c14,0x3828,0x7050,0xe0a0,0xc040,0x30203,0x70507,0xe0a0e,0x1c141c,0x382838,0x705070,0xe0a0e0,0xc040c0,0x3020300,0x7050700,0xe0a0e00,0x1c141c00,0x38283800,0x70507000,0xe0a0e000,0xc040c000,0x302030000,0x705070000,0xe0a0e0000,0x1c141c0000,0x3828380000,0x7050700000,0xe0a0e00000,0xc040c00000,0x30203000000,0x70507000000,0xe0a0e000000,0x1c141c000000,0x382838000000,0x705070000000,0xe0a0e0000000,0xc040c0000000,0x3020300000000,0x7050700000000,0xe0a0e00000000,0x1c141c00000000,0x38283800000000,0x70507000000000,0xe0a0e000000000,0xc040c000000000,0x302030000000000,0x705070000000000,0xe0a0e0000000000,0x1c141c0000000000,0x3828380000000000,0x7050700000000000,0xe0a0e00000000000,0xc040c00000000000,0x203000000000000,0x507000000000000,0xa0e000000000000,0x141c000000000000,0x2838000000000000,0x5070000000000000,0xa0e0000000000000,0x40c0000000000000,
  /* bishop */
  0x8040201008040200,0x80402010080500,0x804020110a00,0x8041221400,0x182442800,0x10204885000,0x102040810a000,0x102040810204000,0x4020100804020002,0x8040201008050005,0x804020110a000a,0x804122140014,0x18244280028,0x1020488500050,0x102040810a000a0,0x204081020400040,0x2010080402000204,0x4020100805000508,0x804020110a000a11,0x80412214001422,0x1824428002844,0x102048850005088,0x2040810a000a010,0x408102040004020,0x1008040200020408,0x2010080500050810,0x4020110a000a1120,0x8041221400142241,0x182442800284482,0x204885000508804,0x40810a000a01008,0x810204000402010,0x804020002040810,0x1008050005081020,0x20110a000a112040,0x4122140014224180,0x8244280028448201,0x488500050880402,0x810a000a0100804,0x1020400040201008,0x402000204081020,0x805000508102040,0x110a000a11204080,0x2214001422418000,0x4428002844820100,0x8850005088040201,0x10a000a010080402,0x2040004020100804,0x200020408102040,0x500050810204080,0xa000a1120408000,0x1400142241800000,0x2800284482010000,0x5000508804020100,0xa000a01008040201,0x4000402010080402,0x2040810204080,0x5081020408000,0xa112040800000,0x14224180000000,0x28448201000000,0x50880402010000,0xa0100804020100,0x40201008040201,
  /* rook */
  0x1010101010101fe,0x2020202020202fd,0x4040404040404fb,0x8080808080808f7,0x10101010101010ef,0x20202020202020df,0x40404040404040bf,0x808080808080807f,0x10101010101fe01,0x20202020202fd02,0x40404040404fb04,0x80808080808f708,0x101010101010ef10,0x202020202020df20,0x404040404040bf40,0x8080808080807f80,0x101010101fe0101,0x202020202fd0202,0x404040404fb0404,0x808080808f70808,0x1010101010ef1010,0x2020202020df2020,0x4040404040bf4040,0x80808080807f8080,0x1010101fe010101,0x2020202fd020202,0x4040404fb040404,0x8080808f7080808,0x10101010ef101010,0x20202020df202020,0x40404040bf404040,0x808080807f808080,0x10101fe01010101,0x20202fd02020202,0x40404fb04040404,0x80808f708080808,0x101010ef10101010,0x202020df20202020,0x404040bf40404040,0x8080807f80808080,0x101fe0101010101,0x202fd0202020202,0x404fb0404040404,0x808f70808080808,0x1010ef1010101010,0x2020df2020202020,0x4040bf4040404040,0x80807f8080808080,0x1fe010101010101,0x2fd020202020202,0x4fb040404040404,0x8f7080808080808,0x10ef101010101010,0x20df202020202020,0x40bf404040404040,0x807f808080808080,0xfe01010101010101,0xfd02020202020202,0xfb04040404040404,0xf708080808080808,0xef10101010101010,0xdf20202020202020,0xbf40404040404040,0x7f80808080808080,
  /* queen */
  0x81412111090503fe,0x2824222120a07fd,0x404844424150efb,0x8080888492a1cf7,0x10101011925438ef,0x2020212224a870df,0x404142444850e0bf,0x8182848890a0c07f,0x412111090503fe03,0x824222120a07fd07,0x4844424150efb0e,0x80888492a1cf71c,0x101011925438ef38,0x20212224a870df70,0x4142444850e0bfe0,0x82848890a0c07fc0,0x2111090503fe0305,0x4222120a07fd070a,0x844424150efb0e15,0x888492a1cf71c2a,0x1011925438ef3854,0x212224a870df70a8,0x42444850e0bfe050,0x848890a0c07fc0a0,0x11090503fe030509,0x22120a07fd070a12,0x4424150efb0e1524,0x88492a1cf71c2a49,0x11925438ef385492,0x2224a870df70a824,0x444850e0bfe05048,0x8890a0c07fc0a090,0x90503fe03050911,0x120a07fd070a1222,0x24150efb0e152444,0x492a1cf71c2a4988,0x925438ef38549211,0x24a870df70a82422,0x4850e0bfe0504844,0x90a0c07fc0a09088,0x503fe0305091121,0xa07fd070a122242,0x150efb0e15244484,0x2a1cf71c2a498808,0x5438ef3854921110,0xa870df70a8242221,0x50e0bfe050484442,0xa0c07fc0a0908884,0x3fe030509112141,0x7fd070a12224282,0xefb0e1524448404,0x1cf71c2a49880808,0x38ef385492111010,0x70df70a824222120,0xe0bfe05048444241,0xc07fc0a090888482,0xfe03050911214181,0xfd070a1222428202,0xfb0e152444840404,0xf71c2a4988080808,0xef38549211101010,0xdf70a82422212020,0xbfe0504844424140,0x7fc0a09088848281,
};
/* kogge stone shifts */
const u64 shifts4[4] =
{
   9, 1, 7, 8              /* all four, non-knight, directions */
};
/* wraps for kogge stone shifts */
const Bitboard wraps[8] =
{
  /* wraps shift left */
  0xfefefefefefefe00, /* <<9 */ 
  0xfefefefefefefefe, /* <<1 */
  0x7f7f7f7f7f7f7f00, /* <<7 */
  0xffffffffffffff00, /* <<8 */

  /* wraps shift right */
  0x007f7f7f7f7f7f7f, /* >>9 */
  0x7f7f7f7f7f7f7f7f, /* >>1 */
  0x00fefefefefefefe, /* >>7 */
  0x00ffffffffffffff  /* >>8 */
};
/* promotion pawns only */
int genmoves_promo(Bitboard *board, Move *moves, int movecounter, bool stm) 
{
  bool kic = false;
  Score score;
  Piece pfrom;
  Piece pto;
  Piece pcpt;
  Square sqfrom;
  Square sqto;
  Square sqcpt;
  Move move;
  Move lastmove;
  Bitboard bbTemp    = BBEMPTY;
  Bitboard bbWork     = BBEMPTY;
  Bitboard bbMoves    = BBEMPTY;
  Bitboard bbBlockers = BBEMPTY;
  Bitboard bbBoth[2];

  lastmove      =  board[QBBLAST];

  bbBlockers    = board[QBBP1]|board[QBBP2]|board[QBBP3];
  bbBoth[WHITE] = board[QBBBLACK]^bbBlockers;
  bbBoth[BLACK] = board[QBBBLACK];
  /* get pawns */
  bbWork        = bbBoth[stm]&LRANK[stm]&(board[QBBP1]&~board[QBBP2]&~board[QBBP3]);

  /* for each pawn of site to move */
  while (bbWork)
  {
    sqfrom  = popfirst1(&bbWork);
    pfrom   = GETPIECE(board, sqfrom);

    bbTemp  = BBEMPTY;
    bbMoves = BBEMPTY;

    /* pawn attacks via attack tables  */
    bbTemp  = AttackTables[stm*64+sqfrom]&bbBoth[!stm];
    /* white pawn push */
    bbTemp |= (!stm&&((~bbBlockers)&SETMASKBB(sqfrom+8)))?SETMASKBB(sqfrom+8):bbTemp;
    /* black pawn push */
    bbTemp |= (stm&&((~bbBlockers)&SETMASKBB(sqfrom-8)))?SETMASKBB(sqfrom-8):bbTemp;

    bbMoves = bbTemp;

    /* extract moves */
    while (bbMoves)
    {
      sqto  = popfirst1(&bbMoves);
      sqcpt = sqto;
      pcpt  = GETPIECE(board, sqcpt);

      /* queen promo only */
      pto = MAKEPIECE(QUEEN, GETCOLOR(pfrom));
      /* get score, non captures via static values, capture via MVV-LVA */
      score = (pcpt==PNONE)? (evalmove (pto, sqto)-evalmove(pfrom, sqfrom)):(EvalPieceValues[GETPTYPE(pcpt)]*16-EvalPieceValues[GETPTYPE(pto)]);
      /* pack move into 64 bits, considering castle rights and halfmovecounter and score */
      move = MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt, 0, (u64)GETHMC(lastmove), (u64)score);

      /* legal moves only */
      domovequick(board, move);
      /* king in check? */
      kic = kingincheck(board, stm);
      if (!kic)
      {
        moves[movecounter] = move;
        movecounter++;
      }
      undomovequick(board, move);
      if (movecounter>=MAXMOVES)
        return movecounter;

      /* queen promo only
      pto = (!kic)?MAKEPIECE(KNIGHT, GETCOLOR(pfrom)):PNONE;
      score = (pcpt==PNONE)? (evalmove (pto, sqto)-evalmove(pfrom, sqfrom)):(EvalPieceValues[GETPTYPE(pcpt)]*16-EvalPieceValues[GETPTYPE(pto)]);
      move = (pto==PNONE)?MOVENONE:MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt, 0, (u64)GETHMC(lastmove), (u64)score);
      moves[movecounter] = move;
      movecounter+=(pto==PNONE)?0:1;
      if (movecounter>=MAXMOVES)
        return movecounter;

      pto = (!kic)?MAKEPIECE(BISHOP, GETCOLOR(pfrom)):PNONE;
      score = (pcpt==PNONE)? (evalmove (pto, sqto)-evalmove(pfrom, sqfrom)):(EvalPieceValues[GETPTYPE(pcpt)]*16-EvalPieceValues[GETPTYPE(pto)]);
      move = (pto==PNONE)?MOVENONE:MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt, 0, (u64)GETHMC(lastmove), (u64)score);
      moves[movecounter] = move;
      movecounter+=(pto==PNONE)?0:1;
      if (movecounter>=MAXMOVES)
        return movecounter;

      pto = (!kic)?MAKEPIECE(ROOK, GETCOLOR(pfrom)):PNONE;
      score = (pcpt==PNONE)? (evalmove (pto, sqto)-evalmove(pfrom, sqfrom)):(EvalPieceValues[GETPTYPE(pcpt)]*16-EvalPieceValues[GETPTYPE(pto)]);
      move = (pto==PNONE)?MOVENONE:MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt, 0, (u64)GETHMC(lastmove), (u64)score);
      moves[movecounter] = move;
      movecounter+=(pto==PNONE)?0:1;
     */
    }
  }
  return movecounter;
}
/* castle moves only */
int genmoves_castles(Bitboard *board, Move *moves, int movecounter, bool stm) 
{
  Score score;
  Piece pfrom;
  Square sqfrom;
  Move move;
  Move lastmove;
  Bitboard bbBlockers = BBEMPTY;
  Bitboard bbTempA    = BBEMPTY;
  Bitboard bbTempB    = BBEMPTY;
  Bitboard bbTempC    = BBEMPTY;
  Bitboard bbBoth[2];

  if (!(board[QBBPMVD]&SMCRALL))
    return movecounter;

  lastmove      = board[QBBLAST];

  bbBlockers    = board[QBBP1]|board[QBBP2]|board[QBBP3];
  bbBoth[WHITE] = board[QBBBLACK]^bbBlockers;
  bbBoth[BLACK] = board[QBBBLACK];

  /* gen castle moves */
  /* get king square */
  sqfrom  = first1(bbBoth[stm]&(board[QBBP1]&board[QBBP2]&~board[QBBP3]));
  pfrom   = GETPIECE(board, sqfrom);
  /* get castle rights queenside */
  bbTempA = (stm)?(((~board[QBBPMVD])&SMCRBLACKQ)==SMCRBLACKQ)?true:false:(((~board[QBBPMVD])&SMCRWHITEQ)==SMCRWHITEQ)?true:false;
  /* rook present */
  bbTempA = (GETPIECE(board, sqfrom-4)==MAKEPIECE(ROOK,stm))?bbTempA:false;
  /* check for empty squares */
  bbTempB = ((bbBlockers&SETMASKBB(sqfrom-1))|(bbBlockers&SETMASKBB(sqfrom-2))|(bbBlockers&SETMASKBB(sqfrom-3)));
  /* check for king and empty squares in check */
  bbTempC =  (squareunderattack(board,!stm,sqfrom)|squareunderattack(board,!stm,sqfrom-1)|squareunderattack(board,!stm,sqfrom-2));
  /* set castle move score */
  score   = INF-100;
  /* make move */
  move    = (bbTempA&&!bbTempB&&!bbTempC)?MAKEMOVE(sqfrom, (sqfrom-2), (sqfrom-2), pfrom, pfrom, PNONE, 0, (u64)GETHMC(lastmove), (u64)score):MOVENONE;
  move   |= (bbTempA&&!bbTempB&&!bbTempC)?MOVEISCRQ:BBEMPTY;
  /* store move */
  moves[movecounter] = move;
  movecounter+=(bbTempA&&!bbTempB&&!bbTempC)?1:0;
  if (movecounter>=MAXMOVES)
    return movecounter;

  /* get castle rights kingside */
  bbTempA = (stm)?(((~board[QBBPMVD])&SMCRBLACKK)==SMCRBLACKK)?true:false:(((~board[QBBPMVD])&SMCRWHITEK)==SMCRWHITEK)?true:false;
  /* rook present */
  bbTempA = (GETPIECE(board, sqfrom+3)==MAKEPIECE(ROOK,stm))?bbTempA:false;
  /* check for empty squares */
  bbTempB = ((bbBlockers&SETMASKBB(sqfrom+1))|(bbBlockers&SETMASKBB(sqfrom+2)));
  /* check for king and empty squares in check */
  bbTempC =  (squareunderattack(board,!stm,sqfrom)|squareunderattack(board,!stm,sqfrom+1)|squareunderattack(board,!stm,sqfrom+2));
  /* set castle move score */
  score   = INF-90;
  /* make move */
  move    = (bbTempA&&!bbTempB&&!bbTempC)?MAKEMOVE(sqfrom, (sqfrom+2), (sqfrom+2), pfrom, pfrom, PNONE, 0, (u64)GETHMC(lastmove), (u64)score):MOVENONE;
  move   |= (bbTempA&&!bbTempB&&!bbTempC)?MOVEISCRK:BBEMPTY;
  /* store move */
  moves[movecounter] = move;
  movecounter+=(bbTempA&&!bbTempB&&!bbTempC)?1:0;

  return movecounter;
}
/* en passant moves only */
int genmoves_enpassant(Bitboard *board, Move *moves, int movecounter, bool stm) 
{
  bool kic = false;
  Score score;
  Piece pfrom;
  Piece pto;
  Piece pcpt;
  Square sqfrom;
  Square sqto;
  Square sqcpt;
  Square sqep = 0; 
  Move move;
  Move lastmove;
  Bitboard bbTempA;
  Bitboard bbWork;
  Bitboard bbBlockers;
  Bitboard bbBoth[2];

  if(!GETSQEP(board[QBBLAST]))
    return movecounter;

  lastmove      = board[QBBLAST];
  bbBlockers    = board[QBBP1]|board[QBBP2]|board[QBBP3];
  bbBoth[WHITE] = bbBlockers^board[QBBBLACK];
  bbBoth[BLACK] = board[QBBBLACK];
  bbWork        = bbBoth[stm];

  /* gen en passant moves */
  sqep    = GETSQEP(board[QBBLAST]); 
  bbWork  = bbBoth[stm]&(board[QBBP1]&~board[QBBP2]&~board[QBBP3]);
  bbWork &= (stm)? 0xFF000000 : 0xFF00000000;
  bbTempA = (sqep)? bbWork&(SETMASKBB(sqep+1)|SETMASKBB(sqep-1)):BBEMPTY;
  score   = EvalPieceValues[PAWN]*16-EvalPieceValues[PAWN];

  /* check for first en passant pawn */
  sqfrom  = (bbTempA)?popfirst1(&bbTempA):0x0;
  pfrom   = GETPIECE(board, sqfrom);
  pto     = pfrom; 
  sqcpt   = sqep;
  sqto    = (stm)? sqep-8:sqep+8;
  pcpt    = GETPIECE(board, sqcpt);
  /* pack move into 64 bits, considering castle rights and halfmovecounter and score */
  move    = (sqfrom)?MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt, 0, (u64)GETHMC(lastmove), (u64)score):MOVENONE;
  /* legal moves only */
  domovequick(board, move);
  kic = kingincheck(board, stm);
  undomovequick(board, move);
  moves[movecounter] = move;
  movecounter+=(sqfrom&&!kic)?1:0;
  if (movecounter>=MAXMOVES)
    return movecounter;

  /* check for second en passant pawn */
  sqfrom  = (bbTempA)?popfirst1(&bbTempA):0x0;
  sqcpt   = sqep;
  sqto    = (stm)? sqep-8:sqep+8;
  /* pack move into 64 bits, considering castle rights and halfmovecounter and score */
  move    = (sqfrom)?MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt, 0, (u64)GETHMC(lastmove), (u64)score):MOVENONE;
  /* legal moves only */
  domovequick(board, move);
  kic = kingincheck(board, stm);
  undomovequick(board, move);
  moves[movecounter] = move;
  movecounter+=(sqfrom&&!kic)?1:0;

  return movecounter;
}
/* captures only */
int genmoves_captures(Bitboard *board, Move *moves, int movecounter, bool stm) 
{
  bool kic = false;
  Score score;
  s32 i;
  Piece pfrom;
  Piece pto;
  Piece pcpt;
  Square sqfrom;
  Square sqto;
  Square sqcpt;
  Move move;
  Move lastmove;
  Bitboard bbTemp;
  Bitboard bbWork;
  Bitboard bbMoves;
  Bitboard bbBlockers;
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbBoth[2];
  u64 shift;

  lastmove = board[QBBLAST];

  bbBlockers    = board[QBBP1]|board[QBBP2]|board[QBBP3];
  bbBoth[WHITE] = board[QBBBLACK]^bbBlockers;
  bbBoth[BLACK] = board[QBBBLACK];
  bbWork        = bbBoth[stm];

  /* for each piece of site to move */
  while (bbWork)
  {
    sqfrom  = popfirst1(&bbWork);
    pfrom   = GETPIECE(board, sqfrom);

    /* no pawn promo */
    if(GETPTYPE(pfrom)==PAWN&&GETRRANK(sqfrom,stm)==RANK_7)
      continue;

    bbTemp  = BBEMPTY;
    bbMoves = BBEMPTY;
  
    /* directions left shifting */
    for (i=0;i<4;i++)
    {
      shift   = shifts4[i];
      bbPro   = ~bbBlockers;
      bbGen   = SETMASKBB(sqfrom);
      bbWrap  = wraps[i];
    
      bbPro  &= bbWrap;
      /* do kogge stone */
      bbGen  |= bbPro &   (bbGen << shift);
      bbPro  &=           (bbPro << shift);
      bbGen  |= bbPro &   (bbGen << 2*shift);
      bbPro  &=           (bbPro << 2*shift);
      bbGen  |= bbPro &   (bbGen << 4*shift);
      /* shift one further */
      bbGen   = bbWrap &  (bbGen << shift);
      bbTemp |= bbGen;
    }
    /* directions right shifting */
    for (i=0;i<4;i++)
    {
      shift   = shifts4[i];
      bbPro   = ~bbBlockers;
      bbGen   = SETMASKBB(sqfrom);
      bbWrap  = wraps[4+i];

      bbPro  &= bbWrap;
      /* do kogge stone */
      bbGen  |= bbPro &   (bbGen >> shift);
      bbPro  &=           (bbPro >> shift);
      bbGen  |= bbPro &   (bbGen >> 2*shift);
      bbPro  &=           (bbPro >> 2*shift);
      bbGen  |= bbPro &   (bbGen >> 4*shift);
      /* shift one further */
      bbGen   = bbWrap &  (bbGen >> shift);
      bbTemp |= bbGen;
    }
    /* consider knights */
    bbTemp    = (GETPTYPE(pfrom)==KNIGHT)?BBFULL:bbTemp;
    /* verify captures */
    i         = (GETPTYPE(pfrom)==PAWN)?stm:GETPTYPE(pfrom);
    bbPro     = AttackTables[i*64+sqfrom];
    bbMoves   = bbPro&bbTemp&bbBoth[!stm];

    /* extract moves */
    while (bbMoves)
    {
      sqto    = popfirst1(&bbMoves);
      sqcpt   = sqto;
      pto     = pfrom;
      pcpt    = GETPIECE(board, sqcpt);

      /* get score, non captures via static values, capture via MVV-LVA */
      score = (EvalPieceValues[GETPTYPE(pcpt)]*16-EvalPieceValues[GETPTYPE(pto)]);
      /* pack move into 64 bits, considering castle rights and halfmovecounter and score */
      move = MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt, 0, (u64)GETHMC(lastmove), (u64)score);

      /* legal moves only */
      domovequick(board, move);
      /* king in check? */
      kic = kingincheck(board, stm);
      if (!kic)
      {
        moves[movecounter] = move;
        movecounter++;
      }
      undomovequick(board, move);
      if (movecounter>=MAXMOVES)
        return movecounter;
    }
  }
  return movecounter;
}
/* quiet moves only */
int genmoves_noncaptures(Bitboard *board, Move *moves, int movecounter, bool stm, s32 ply) 
{
  bool kic = false;
  Score score;
  s32 i;
  Piece pfrom;
  Piece pto;
  Square sqfrom;
  Square sqto;
  Square sqep; 
  Move move;
  Move lastmove;
  Bitboard bbTemp;
  Bitboard bbWork;
  Bitboard bbMoves;
  Bitboard bbBlockers;
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbBoth[2];
  u64 shift;

  lastmove = board[QBBLAST];

  bbBlockers    = board[QBBP1]|board[QBBP2]|board[QBBP3];
  bbBoth[WHITE] = board[QBBBLACK]^bbBlockers;
  bbBoth[BLACK] = board[QBBBLACK];
  bbWork        = bbBoth[stm];

  /* for each piece of site to move */
  while (bbWork)
  {
    sqfrom   = popfirst1 (&bbWork);
    pfrom    = GETPIECE(board, sqfrom);

    /* no pawn promo */
    if(GETPTYPE(pfrom)==PAWN&&GETRRANK(sqfrom,stm)==RANK_7)
      continue;

    bbTemp  = BBEMPTY;
    bbMoves = BBEMPTY;
  
    /* directions left shifting */
    for (i=0;i<4;i++)
    {
      shift   = shifts4[i];
      bbPro   = ~bbBlockers;
      bbGen   = SETMASKBB(sqfrom);
      bbWrap  = wraps[i];
    
      bbPro  &= bbWrap;
      /* do kogge stone */
      bbGen  |= bbPro &   (bbGen << shift);
      bbPro  &=           (bbPro << shift);
      bbGen  |= bbPro &   (bbGen << 2*shift);
      bbPro  &=           (bbPro << 2*shift);
      bbGen  |= bbPro &   (bbGen << 4*shift);
      /* shift one further */
      bbGen   = bbWrap &  (bbGen << shift);
      bbTemp |= bbGen;
    }
    /* directions right shifting */
    for (i=0;i<4;i++)
    {
      shift   = shifts4[i];
      bbPro   = ~bbBlockers;
      bbGen   = SETMASKBB(sqfrom);
      bbWrap  = wraps[4+i];

      bbPro  &= bbWrap;
      /* do kogge stone */
      bbGen  |= bbPro &   (bbGen >> shift);
      bbPro  &=           (bbPro >> shift);
      bbGen  |= bbPro &   (bbGen >> 2*shift);
      bbPro  &=           (bbPro >> 2*shift);
      bbGen  |= bbPro &   (bbGen >> 4*shift);
      /* shift one further */
      bbGen   = bbWrap &  (bbGen >> shift);
      bbTemp |= bbGen;
    }
    /* consider knights */
    bbTemp    = (GETPTYPE(pfrom)==KNIGHT)?BBFULL:bbTemp;
    /* verify non captures */    
    bbPro     = (GETPTYPE(pfrom)==PAWN)?(AttackTablesPawnPushes[stm*64+sqfrom]):
                                         AttackTables[GETPTYPE(pfrom)*64+sqfrom];
    bbMoves   = (bbPro&bbTemp&~bbBlockers);

    /* extract moves */
    while (bbMoves)
    {
      sqto    = popfirst1(&bbMoves);
      pto     = pfrom;

      /* set en passant target square */
      sqep    = (GETPTYPE(pfrom)==PAWN&&GETRRANK(sqto,stm)-GETRRANK(sqfrom,stm)==2)?(stm)?sqto:sqto:0x0; 

      /* get score, non captures via static values, capture via MVV-LVA */
      score = (evalmove (pto, sqto)-evalmove(pfrom, sqfrom));
      /* pack move into 64 bits, considering castle rights and halfmovecounter and score */
      move = MAKEMOVE(sqfrom, sqto, sqto, pfrom, pto, PNONE, sqep, (Move)GETHMC(lastmove), (Move)score);

      /* set killers and counters score */
      if (JUSTMOVE(move)==JUSTMOVE(Killers[ply*2+0]))
      {
        score = INF-10;
      }
      else if (JUSTMOVE(move)==JUSTMOVE(Killers[ply*2+1]))
      {
        score = INF-20;
      }
      else if (JUSTMOVE(move)==JUSTMOVE(Counters[GETSQFROM(lastmove)*64+GETSQTO(lastmove)]))
      {
        score = INF-30;
      }
      move = SETSCORE(move,(Move)score);

      /* legal moves only */
      domovequick(board, move);
      /* king in check? */
      kic = kingincheck(board, stm);
      if (!kic)
      {
        moves[movecounter] = move;
        movecounter++;
      }
      undomovequick(board, move);
      if (movecounter>=MAXMOVES)
        return movecounter;
    }
  }
  return movecounter;
}
/* generate all pieces via generalized KoggeStone bitboard approach */
/* based on work by Steffan Westcott */
/* http://chessprogramming.wikispaces.com/Kogge-Stone+Algorithm */
int genmoves_general(Bitboard *board, Move *moves, int movecounter, bool stm, bool qs) 
{
  bool kic = false;
  Score score;
  s32 i;
  Piece pfrom;
  Piece pto;
  Piece pcpt;
  Square sqfrom;
  Square sqto;
  Square sqcpt;
  Square sqep; 
  Move move;
  Move lastmove;
  Bitboard bbTemp;
  Bitboard bbWork;
  Bitboard bbMoves;
  Bitboard bbBlockers;
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbBoth[2];
  u64 shift;

  lastmove = board[QBBLAST];

  bbBlockers    = board[QBBP1]|board[QBBP2]|board[QBBP3];
  bbBoth[WHITE] = board[QBBBLACK]^bbBlockers;
  bbBoth[BLACK] = board[QBBBLACK];
  bbWork        = bbBoth[stm];

  /* for each piece of site to move */
  while (bbWork)
  {
    sqfrom    = popfirst1(&bbWork);
    pfrom     = GETPIECE(board, sqfrom);
    bbTemp    = BBEMPTY;
    bbMoves   = BBEMPTY;
  
    /* directions left shifting */
    for (i=0;i<4;i++)
    {
      shift   = shifts4[i];
      bbPro   = ~bbBlockers;
      bbGen   = SETMASKBB(sqfrom);
      bbWrap  = wraps[i];
    
      bbPro  &= bbWrap;
      /* do kogge stone */
      bbGen  |= bbPro &   (bbGen << shift);
      bbPro  &=           (bbPro << shift);
      bbGen  |= bbPro &   (bbGen << 2*shift);
      bbPro  &=           (bbPro << 2*shift);
      bbGen  |= bbPro &   (bbGen << 4*shift);
      /* shift one further */
      bbGen   = bbWrap &  (bbGen << shift);
      bbTemp |= bbGen;
    }
    /* directions right shifting */
    for (i=0;i<4;i++)
    {
      shift   = shifts4[i];
      bbPro   = ~bbBlockers;
      bbGen   = SETMASKBB(sqfrom);
      bbWrap  = wraps[4+i];

      bbPro  &= bbWrap;
      /* do kogge stone */
      bbGen  |= bbPro &   (bbGen >> shift);
      bbPro  &=           (bbPro >> shift);
      bbGen  |= bbPro &   (bbGen >> 2*shift);
      bbPro  &=           (bbPro >> 2*shift);
      bbGen  |= bbPro &   (bbGen >> 4*shift);
      /* shift one further */
      bbGen   = bbWrap &  (bbGen >> shift);
      bbTemp |= bbGen;
    }
    /* consider knights */
    bbTemp    = (GETPTYPE(pfrom)==KNIGHT)?BBFULL:bbTemp;
    /* verify captures */
    i         = (GETPTYPE(pfrom)==PAWN)?stm:GETPTYPE(pfrom);
    bbPro     = AttackTables[i*64+sqfrom];
    bbMoves   = bbPro&bbTemp&bbBoth[!stm];
    /* verify non captures */    
    bbPro     = (GETPTYPE(pfrom)==PAWN)?(AttackTablesPawnPushes[stm*64+sqfrom]):bbPro;
    bbMoves  |= (qs)?BBEMPTY:(bbPro&bbTemp&~bbBlockers);

    /* extract moves */
    while (bbMoves)
    {
      sqto    = popfirst1(&bbMoves);
      sqcpt   = sqto;
      pcpt    = GETPIECE(board, sqcpt);

      /* set en passant target square */
      sqep      = (GETPTYPE(pfrom)==PAWN&&GETRRANK(sqto,stm)-GETRRANK(sqfrom,stm)==2)?(stm)?sqto:sqto:0x0; 

      /* handle pawn promo: knight */
      pto = (GETPTYPE(pfrom)==PAWN&&GETRRANK(sqto,stm)==RANK_8)?MAKEPIECE(KNIGHT, GETCOLOR(pfrom)):pfrom;
      /* get score, non captures via static values, capture via MVV-LVA */
      score = (pcpt==PNONE)? (evalmove (pto, sqto)-evalmove(pfrom, sqfrom)):(EvalPieceValues[GETPTYPE(pcpt)]*16-EvalPieceValues[GETPTYPE(pto)]);
      /* pack move into 64 bits, considering castle rights and halfmovecounter and score */
      move = MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt, sqep, (u64)GETHMC(lastmove), (u64)score);

      /* legal moves only */
      domovequick(board, move);
      /* king in check? */
      kic = kingincheck(board, stm);
      if (!kic)
      {
        moves[movecounter] = move;
        movecounter++;
      }
      undomovequick(board, move);
      if (movecounter>=MAXMOVES)
        return movecounter;

      /* TODO: in non-perft do queen promo only? */
      /* handle pawn promo: bishop */
      pto = (!kic&&GETPTYPE(pfrom)==PAWN&&GETRRANK(sqto,stm)==RANK_8)?MAKEPIECE(BISHOP, GETCOLOR(pfrom)):PNONE;
      /* get score, non captures via static values, capture via MVV-LVA */
      score = (pcpt==PNONE)? (evalmove (pto, sqto)-evalmove(pfrom, sqfrom)):(EvalPieceValues[GETPTYPE(pcpt)]*16-EvalPieceValues[GETPTYPE(pto)]);
      /* pack move into 64 bits, considering castle rights and halfmovecounter and score */
      move = (pto==PNONE)?MOVENONE:MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt, 0, (u64)GETHMC(lastmove), (u64)score);
      moves[movecounter] = move;
      movecounter+=(pto==PNONE)?0:1;
      if (movecounter>=MAXMOVES)
        return movecounter;

      /* handle pawn promo: rook */
      pto = (!kic&&GETPTYPE(pfrom)==PAWN&&GETRRANK(sqto,stm)==RANK_8)?MAKEPIECE(ROOK, GETCOLOR(pfrom)):PNONE;
      /* get score, non captures via static values, capture via MVV-LVA */
      score = (pcpt==PNONE)? (evalmove (pto, sqto)-evalmove(pfrom, sqfrom)):(EvalPieceValues[GETPTYPE(pcpt)]*16-EvalPieceValues[GETPTYPE(pto)]);
      /* pack move into 64 bits, considering castle rights and halfmovecounter and score */
      move = (pto==PNONE)?MOVENONE:MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt, 0, (u64)GETHMC(lastmove), (u64)score);
      moves[movecounter] = move;
      movecounter+=(pto==PNONE)?0:1;
      if (movecounter>=MAXMOVES)
        return movecounter;

      /* handle pawn promo: queen */
      pto = (!kic&&GETPTYPE(pfrom)==PAWN&&GETRRANK(sqto,stm)==RANK_8)?MAKEPIECE(QUEEN, GETCOLOR(pfrom)):PNONE;
      /* get score, non captures via static values, capture via MVV-LVA */
      score = (pcpt==PNONE)? (evalmove (pto, sqto)-evalmove(pfrom, sqfrom)):(EvalPieceValues[GETPTYPE(pcpt)]*16-EvalPieceValues[GETPTYPE(pto)]);
      /* pack move into 64 bits, considering castle rights and halfmovecounter and score */
      move = (pto==PNONE)?MOVENONE:MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt, 0, (u64)GETHMC(lastmove), (u64)score);
      moves[movecounter] = move;
      movecounter+=(pto==PNONE)?0:1;
      if (movecounter>=MAXMOVES)
        return movecounter;
    }
  }

  /* gen en passant moves */
  sqep    = GETSQEP(board[QBBLAST]); 
  bbPro   = bbBoth[stm]&(board[QBBP1]&~board[QBBP2]&~board[QBBP3]);
  bbPro   &= (stm)? 0xFF000000 : 0xFF00000000;
  bbTemp  = (sqep)?bbPro&(SETMASKBB(sqep+1)|SETMASKBB(sqep-1)):BBEMPTY;
  score   = EvalPieceValues[PAWN]*16-EvalPieceValues[PAWN];

  /* check for first en passant pawn */
  sqfrom  = (bbTemp)?popfirst1(&bbTemp):0x0;
  pfrom   = GETPIECE(board, sqfrom);
  pto     = pfrom; 
  sqcpt   = sqep;
  pcpt    = GETPIECE(board, sqcpt);
  sqto    = (stm)? sqep-8:sqep+8;
  /* pack move into 64 bits, considering castle rights and halfmovecounter and score */
  move    = (sqfrom)?MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt, 0, (u64)GETHMC(lastmove), (u64)score):MOVENONE;
  /* legal moves only */
  domovequick(board, move);
  kic = kingincheck(board, stm);
  undomovequick(board, move);
  moves[movecounter] = move;
  movecounter+=(sqfrom&&!kic)?1:0;
  if (movecounter>=MAXMOVES)
    return movecounter;

  /* check for second en passant pawn */
  sqfrom  = (bbTemp)?popfirst1(&bbTemp):0x0;
  sqcpt   = sqep;
  sqto    = (stm)? sqep-8:sqep+8;
  /* pack move into 64 bits, considering castle rights and halfmovecounter and score */
  move    = (sqfrom)?MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt, 0, (u64)GETHMC(lastmove), (u64)score):MOVENONE;
  /* legal moves only */
  domovequick(board, move);
  kic = kingincheck(board, stm);
  undomovequick(board, move);
  moves[movecounter] = move;
  movecounter+=(sqfrom&&!kic)?1:0;
  if (movecounter>=MAXMOVES)
    return movecounter;

  /* gen castle moves */
  /* get king square */
  sqfrom  = first1(bbBoth[stm]&(board[QBBP1]&board[QBBP2]&~board[QBBP3]));
  pfrom   = GETPIECE(board, sqfrom);
  /* get castle rights queenside */
  bbTemp  = (stm)?(((~board[QBBPMVD])&SMCRBLACKQ)==SMCRBLACKQ)?true:false:(((~board[QBBPMVD])&SMCRWHITEQ)==SMCRWHITEQ)?true:false;
  /* rook present */
  bbTemp  = (GETPIECE(board, sqfrom-4)==MAKEPIECE(ROOK,stm))?bbTemp:false;
  /* check for empty squares */
  bbPro   = ((bbBlockers&SETMASKBB(sqfrom-1))|(bbBlockers&SETMASKBB(sqfrom-2))|(bbBlockers&SETMASKBB(sqfrom-3)));
  /* check for king and empty squares in check */
  bbGen  =  (squareunderattack(board,!stm,sqfrom)|squareunderattack(board,!stm,sqfrom-1)|squareunderattack(board,!stm,sqfrom-2));
  /* set castle move score */
  score   = INF-100;
  /* make move */
  move    = (bbTemp&&!bbPro&&!bbGen)?MAKEMOVE(sqfrom, (sqfrom-2), (sqfrom-2), pfrom, pfrom, PNONE, 0, (u64)GETHMC(lastmove), (u64)score):MOVENONE;
  move   |= (bbTemp&&!bbPro&&!bbGen)?MOVEISCRQ:BBEMPTY;
  /* store move */
  moves[movecounter] = move;
  movecounter+=(bbTemp&&!bbPro&&!bbGen)?1:0;
  if (movecounter>=MAXMOVES)
    return movecounter;

  /* get castle rights kingside */
  bbTemp  = (stm)?(((~board[QBBPMVD])&SMCRBLACKK)==SMCRBLACKK)?true:false:(((~board[QBBPMVD])&SMCRWHITEK)==SMCRWHITEK)?true:false;
  /* rook present */
  bbTemp  = (GETPIECE(board, sqfrom+3)==MAKEPIECE(ROOK,stm))?bbTemp:false;
  /* check for empty squares */
  bbPro   = ((bbBlockers&SETMASKBB(sqfrom+1))|(bbBlockers&SETMASKBB(sqfrom+2)));
  /* check for king and empty squares in check */
  bbGen  =  (squareunderattack(board,!stm,sqfrom)|squareunderattack(board,!stm,sqfrom+1)|squareunderattack(board,!stm,sqfrom+2));
  /* set castle move score */
  score   = INF-90;
  /* make move */
  move    = (bbTemp&&!bbPro&&!bbGen)?MAKEMOVE(sqfrom, (sqfrom+2), (sqfrom+2), pfrom, pfrom, PNONE, 0, (u64)GETHMC(lastmove), (u64)score):MOVENONE;
  move   |= (bbTemp&&!bbPro&&!bbGen)?MOVEISCRK:BBEMPTY;
  /* store move */
  moves[movecounter] = move;
  movecounter+=(bbTemp&&!bbPro&&!bbGen)?1:0;

  return movecounter;
}
/* wrapper for move genration */
int genmoves(Bitboard *board, Move *moves, int movecounter, bool stm, bool qs, s32 ply)
{

/*
  return genmoves_general(board, moves, movecounter, stm, qs);
*/

  movecounter = genmoves_promo(board, moves, movecounter, stm);

  if(GETSQEP(board[QBBLAST]))
    movecounter = genmoves_enpassant(board, moves, movecounter, stm);

  if (!qs&&(board[QBBPMVD]&SMCRALL))
    movecounter = genmoves_castles(board, moves, movecounter, stm);

  movecounter = genmoves_captures(board, moves, movecounter, stm);

  if (!qs)
    movecounter = genmoves_noncaptures(board, moves, movecounter, stm, ply);

  return movecounter;
}
/* generate rook moves via koggestone shifts */
Bitboard ks_attacks_ls1(Bitboard bbBlockers, Square sq)
{
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

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

  return bbMoves;
}
Bitboard ks_attacks_ls8(Bitboard bbBlockers, Square sq)
{
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

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
  
  return bbMoves;
}
Bitboard ks_attacks_rs1(Bitboard bbBlockers, Square sq)
{
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

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

  return bbMoves;
}
Bitboard ks_attacks_rs8(Bitboard bbBlockers, Square sq)
{
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

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

  return bbMoves;
}
Bitboard rook_attacks(Bitboard bbBlockers, Square sq)
{
  return ks_attacks_ls1(bbBlockers, sq) |
         ks_attacks_ls8(bbBlockers, sq) |
         ks_attacks_rs1(bbBlockers, sq) |
         ks_attacks_rs8(bbBlockers, sq);
}
/* generate bishop moves via koggestone shifts */
Bitboard ks_attacks_ls9(Bitboard bbBlockers, Square sq)
{
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

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

  return bbMoves;
}
Bitboard ks_attacks_ls7(Bitboard bbBlockers, Square sq)
{
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

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

  return bbMoves;
}
Bitboard ks_attacks_rs9(Bitboard bbBlockers, Square sq)
{
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

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

  return bbMoves;
}
Bitboard ks_attacks_rs7(Bitboard bbBlockers, Square sq)
{
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

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

  return bbMoves;
}

Bitboard bishop_attacks(Bitboard bbBlockers, Square sq)
{
  return ks_attacks_ls7(bbBlockers, sq) |
         ks_attacks_ls9(bbBlockers, sq) |
         ks_attacks_rs7(bbBlockers, sq) |
         ks_attacks_rs9(bbBlockers, sq);
}

