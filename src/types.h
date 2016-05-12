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

#ifndef TYPES_H_INCLUDED
#define TYPES_H_INCLUDED

/* C89 style typedefs
typedef unsigned char bool;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef unsigned long long u64;
#define false   0
#define true    1
*/
/* C99 headers */
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t         u8;
typedef int16_t         s16;
typedef int32_t         s32;
typedef uint32_t        u32;
typedef uint64_t        u64;
/* custom typedefs */
typedef s16 Score;
typedef u64 Bitboard;
typedef u64 Cr;
typedef u64 Hmc;
typedef u64 Square;
typedef u64 Piece;
typedef u64 PieceType;
typedef u64 Move;
#define VERSION "0301"
/* quad bitboard array index definition
  0   pieces white
  1   piece type first bit
  2   piece type second bit
  3   piece type third bit
  4   64 bit board hash, for future use
  5   lastmove + ep target + halfmove clock + castle rights + move score
*/
/* move encoding 

   0  -  5  square from
   6  - 11  square to
  12  - 17  square capture
  18  - 21  piece from
  22  - 25  piece to
  26  - 29  piece capture
  30  - 35  square en passant target
  36  - 43  halfmove clock for fity move rule, last capture/castle/pawn move
  44  - 47  castle rights
  48  - 63  move score
*/
/* castle right encoding mask in 4 bits
  0 white queenside
  1 white kingside
  2 black queenside
  3 black kingside
*/
/* colors */
#define WHITE   0
#define BLACK   1
/* piece type enumeration */
#define PNONE   0
#define PAWN    1
#define KNIGHT  2
#define KING    3
#define BISHOP  4
#define ROOK    5
#define QUEEN   6
/* set masks */
#define SMEP              0x0000000FC0000000
#define SMHMC             0x00000FF000000000
#define SMCRALL           0x0000F00000000000
#define SMSCORE           0xFFFF000000000000
/* clear masks */
#define CMEP              0xFFFFFFF03FFFFFFF
#define CMHMC             0xFFFFF00FFFFFFFFF
#define CMCRALL           0xFFFF0FFFFFFFFFFF
#define CMSCORE           0x0000FFFFFFFFFFFF
/* castle right set masks big endian */
#define SMCRWHITE         0x0000300000000000
#define SMCRWHITEQ        0x0000100000000000
#define SMCRWHITEK        0x0000200000000000
#define SMCRBLACK         0x0000C00000000000
#define SMCRBLACKQ        0x0000400000000000
#define SMCRBLACKK        0x0000800000000000
/* castle right clear masks big endian */
#define CMCRWHITE         0xFFFFCFFFFFFFFFFF
#define CMCRWHITEQ        0xFFFFEFFFFFFFFFFF
#define CMCRWHITEK        0xFFFFDFFFFFFFFFFF
#define CMCRBLACK         0xFFFF3FFFFFFFFFFF
#define CMCRBLACKQ        0xFFFFBFFFFFFFFFFF
#define CMCRBLACKK        0xFFFF7FFFFFFFFFFF
/* move helpers */
#define GETSQFROM(mv)     ((mv)&0x3F)         /* 6 bit square */
#define GETSQTO(mv)       (((mv)>>6)&0xF)     /* 6 bit square */
#define GETSQCAPTURE(mv)  (((mv)>>12)&0x3F)   /* 6 bit square */
#define GETPFROM(mv)      (((mv)>>18)&0x3F)   /* 4 bit piece type */
#define GETPTO(mv)        (((mv)>>22)&0xF)    /* 4 bit piece type */
#define GETPCPT(mv)       (((mv)>>26)&0xF)    /* 4 bit piece type */
#define GETSQEP(mv)       (((mv)>>30)&0x3F)   /* 6 bit square */
#define GETHMC(mv)        (((mv)>>36)&0xFF)   /* 8 bit halfmove clock */
#define GETCR(mv)         (((mv)>>44)&0xF)    /* 4 bit castle rights */
#define SETCR(mv,cr)      ((mv&CMCRALL)|cr)   /* 4 bit castle rights */
#define GETSCORE(mv)      (((mv)>>48)&0xFFFF) /* signed 16 bit score */
#define SETSCORE(mv,score)(((mv)&CMSCORE)|(((score)&0xFFFF)<<48)) 
/* pack move into 64 bits */
#define MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt, sqep, hmc, cr, score) \
( \
     sqfrom      | (sqto<<6)  | (sqcpt<<12) \
  | (pfrom<<18)  | (pto<<22)  | (pcpt<<26) \
  | (sqep<<30)   | (hmc<<36)  | (cr<<44) | (score<<48) \
)
/* file enumeration */
enum Files
{
  FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H
};
/* rank enumeration */
enum Ranks
{
  RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8
};
/* square enumeration */
enum Squares
{
  SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
  SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
  SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
  SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
  SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
  SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
  SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
  SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8
};

#endif /* TYPES_H_INCLUDED */


