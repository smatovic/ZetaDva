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

#ifndef SEARCH_H_INCLUDED
#define SEARCH_H_INCLUDED

Score perft(Bitboard *board, bool stm, s32 depth);
Move rootsearch(Bitboard *board, bool stm, s32 depth);
Score negamax(Bitboard *board,
              bool stm, 
              Score alpha, 
              Score beta, 
              s32 depth, 
              s32 ply, 
              bool prune);
Score qsearch(Bitboard *board, 
              bool stm, 
              Score alpha, 
              Score beta, 
              s32 depth, 
              s32 ply);

#endif /* SEARCH_H_INCLUDED */

