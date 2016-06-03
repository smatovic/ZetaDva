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

#include "types.h"      /* custom types, board defs, data structures, macros */

/* population count, Donald Knuth SWAR style */
/* as described on CWP */
/* http://chessprogramming.wikispaces.com/Population+Count#SWAR-Popcount */
u64 popcount(u64 x) 
{
  x =  x                        - ((x >> 1)  & 0x5555555555555555);
  x = (x & 0x3333333333333333)  + ((x >> 2)  & 0x3333333333333333);
  x = (x                        +  (x >> 4)) & 0x0f0f0f0f0f0f0f0f;
  x = (x * 0x0101010101010101) >> 56;
  return x;
}
/*  pre condition: x != 0; */
u64 first1(u64 x)
{
  return popcount((x&-x)-1);
}
/*  pre condition: x != 0; */
u64 popfirst1(u64 *a)
{
  u64 b = *a;
  *a &= (*a-1);  /* clear lsb  */
  return popcount((b&-b)-1); /* return pop count of isolated lsb */
}
/* bit twiddling
  bb_work=bb_temp&-bb_temp;  // get lsb 
  bb_temp&=bb_temp-1;       // clear lsb
*/

