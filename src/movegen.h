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

const Bitboard AttackTablesPawns[4*64];
const Bitboard AttackTablesByPawns[2*64];
const Bitboard AttackTablesNK[2*64];

int genmoves(Bitboard *board, Move *moves, int movecounter, bool stm, bool qs);
int genmoves_general(Bitboard *board, Move *moves, int movecounter, bool stm, bool qs);
int genmoves_piecewise(Bitboard *board, Move *moves, int movecounter, bool stm, bool qs);
int genmoves_captures(Bitboard *board, Move *moves, int movecounter, bool stm);
int genmoves_noncaptures(Bitboard *board, Move *moves, int movecounter, bool stm);
int genmoves_castles(Bitboard *board, Move *moves, int movecounter, bool stm);
int genmoves_promo(Bitboard *board, Move *moves, int movecounter, bool stm);
int genmoves_enpassant(Bitboard *board, Move *moves, int movecounter, bool stm);
Bitboard rook_attacks(Bitboard bbBlockers, Square sq);
Bitboard bishop_attacks(Bitboard bbBlockers, Square sq);
#endif /* MOVEGEN_H_INCLUDED */

