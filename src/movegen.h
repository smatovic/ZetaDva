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

#ifndef MOVEGEN_H_INCLUDED
#define MOVEGEN_H_INCLUDED

extern const Bitboard AttackTablesTo[2*7*64];
extern const Bitboard AttackTablesNK[2*64];
extern const Bitboard AttackTablesByPawns[2*64];

int genmoves_general(Bitboard *board, Move *moves, int movecounter, bool stm, bool qs);
Bitboard rook_attacks(Bitboard bbBlockers, Square sq);
Bitboard bishop_attacks(Bitboard bbBlockers, Square sq);
#endif /* MOVEGEN_H_INCLUDED */

