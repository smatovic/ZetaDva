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

#include "timer.h"      /* for time measurement */
#include "types.h"      /* custom types, board defs, data structures, macros */

/* Global variables  */
FILE 	*log_file;
bool log_flag = false;

/* Quad Bitboard */
/* based on http://chessprogramming.wikispaces.com/Quad-Bitboards */
/* by Gerd Isenberg */
Bitboard board[6];
/* quad bitboard array index definition
  0   pieces white
  1   piece type first bit
  2   piece type second bit
  3   piece type third bit
  4   64 bit board Zobrist hash
  5   lastmove + ep target + halfmove clock + castle rights + move score
*/

void self_test(void) 
{
  return;
}
void print_help(void)
{
  printf("Zeta Dva, yet another amateur level chess engine.\n");
  printf("\n");
  printf("\n");
  printf("Options:\n");
  printf(" -l, --log          Write output/debug to file zetadva.log\n");
  printf(" -v, --version      Print Zeta Dva version info.\n");
  printf(" -h, --help         Print Zeta Dva program usage help.\n");
  printf(" -s, --selftest     Run an internal test, usefull after compile.\n");
  printf("\n");
  printf("To play against the engine use an UCI protocol capable chess GUI");
  printf("like Arena, Winboard or Xboard with Polyglot adapter\n");
  printf("\n");
  printf("Alternatively you can use UCI commmand directly on commman line,\n"); 
  printf("e.g.\n");
  printf("\n");
  printf("\n");
  printf("\n");
  printf("License: GPL >= v2\n");
  printf("\n");
}
void print_version(void)
{
  printf("Zeta Dva, version %s\n",VERSION);
  printf("Copyright (C) 2011-2016 Srdja Matovic\n");
  printf("This is free software, licensed under GPL >= v2\n");
}
/* set internal chess board presentation to fen string */
void set_board(char *fen)
{
  printf("%s\n",fen);
}
/* Zeta Dva, amateur level chess engine  */
int main(int argc, char* argv[]) {

  char line[1024];
  char fen[1024];
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
  s32 cores           = 1;
  s32 ply             = 4;

  /* getopt loop */
  while ((c = getopt_long_only(argc, argv, "",
               long_options, &option_index)) != -1) {
    switch (option_index) 
    {
      case 0:
        print_help();
        return 0;
        break;
      case 1:
        print_version();
        return 0;
        break;
      case 2:
        log_flag=true;
        break;
      case 3:
        self_test();
        break;
    }
  }
  /* open log file */
  if (log_flag)
  {
    log_file = fopen("zetadva.log", "ab+");
    if (log_file==NULL)
      log_flag=false;
    else
    {
      /* print binary call to log */
      char timestring[256];
      get_time_string(timestring);
      fprintf(log_file, "%s", timestring);
      for (c=0;c<argc;c++)
      {
        fprintf(log_file, "%s ",argv[c]);
      }
      fprintf(log_file, "\n");
    }
  }
  /* close log file */
  if(log_flag)
    fclose(log_file);

  return 0;
}

