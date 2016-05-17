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

#include "pst.h"        /* piece square tables, wood count, table control */
#include "types.h"      /* custom types, board defs, data structures, macros */

/* 
  simple evaluation, based on proposal by Tomasz Michniewski
  https://chessprogramming.wikispaces.com/Simplified+evaluation+function
*/

Score evalmove(PieceType piece, Square sq, bool stm)
{
  Score score = 0;

  score+= (stm)? EvalPieceValues[piece] :   EvalPieceValues[piece];
  score+= (stm)? EvalTable[piece*64+sq] :   EvalTable[piece*64+FLOP(sq)];
  score+= (stm)? EvalControl[sq]        :   EvalControl[FLOP(sq)];

  return score;
}
