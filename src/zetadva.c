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
#include "timer.h"      /* for time measurement */
#include "types.h"      /* custom types, board defs, data structures, macros */

/* global variables */
FILE 	*Log_File = NULL;       /* logfile for debug */
char *Line;                   /* for fgetting the input on stdin */
char *Command;                /* for pasring the xboard command */
char *Fen;                    /* for storing the fen chess baord string */
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
/* release memory, io buffer, history and hash table */
bool release_inits (void)
{
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
/* initialize engine memory, io buffer, history and hash table */
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
/* set internal chess board presentation to fen string */
bool setboard (Bitboard *board, char *fenstring)
{
  char tempchar;
  char *position; /* piece types and position  row 8, sq 56 to, row 1, sq 7 */
  char *cstm;     /* site to move */
  char *castle;   /* castle rights */
  char *cep;      /* en passant target square */
  char *tempstring;
  char fencharstring[24] = {" PNKBRQ pnkbrq/12345678"}; /* mapping */
  int i;
  int j;
  int side;
  u64 hmc;        /* half move clock */
  u64 fendepth;   /* game depth */
  File file;
  Rank rank;
  Piece piece;
  Square sq;
  Cr cr = BBEMPTY;
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
  if (cstm == NULL) 
  {
    printf ("Error (memory allocation failed): char castle[%d]", 1024);
    return false;
  }
  cep  = malloc (1024 * sizeof (char));
  if (cstm == NULL) 
  {
    printf ("Error (memory allocation failed): char cep[%d]", 1024);
    return false;
  }

  /* get data from fen string */
	sscanf (fenstring, "%s %s %s %s %llu %llu", 
          position, cstm, castle, cep, &hmc, &fendepth);

  /* empty the board */
  board[QBBWHITE] = 0x0ULL;
  board[QBBP1]    = 0x0ULL;
  board[QBBP2]    = 0x0ULL;
  board[QBBP3]    = 0x0ULL;
  board[QBBHASH]  = 0x0ULL;
  board[QBBLAST]  = 0x0ULL;

  /* parse position from fen string */
  file = FILE_A;
  rank = RANK_8;
  i=  0;
  while (!(rank <= RANK_1 && file >= FILE_NONE))
  {
    tempchar = position[i++];
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
        /* empty squares*/
        else if (j >= 15)
        {
            file+=j-14;
        }
        else
        {
            sq        = MAKESQ (rank, file);
            side      = (j > 6)? BLACK : WHITE;
            piece     = side? j-7 : j;
            piece<<=1;
            piece    |= side;
            board[0] |= piece&0x1;
            board[1] |= ((piece>>1)&0x1)<<sq;
            board[2] |= ((piece>>2)&0x1)<<sq;
            board[3] |= ((piece>>3)&0x1)<<sq;
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
        cr |= SMCRWHITEQ;
      /* white kingside */
      if (tempchar == 'K')
        cr |= SMCRWHITEK;
      /* black queenside */
      if (tempchar == 'q')
        cr |= SMCRBLACKQ;
      /* black kingside */
      if (tempchar == 'k')
        cr |= SMCRBLACKK;
      i++;
      tempchar = castle[i];
    }
  }
  /* store castle rights into lastmove */
  lastmove = SETHMC (lastmove, hmc);
  /* set en passant target square */
  tempchar = cep[0];
  if (tempchar != '-')
  {
    rank  = (Rank)cep[1] -49;
    file  = (File)cep[0] -97;
    sq    = MAKESQ (rank,file);
    lastmove = SETSQEP (lastmove, sq);
  }
  PLY = 0;
  GAMEPLY = fendepth*2+STM;

  /* TODO: compute  hash
  board[QBBHASH] = compute_hash(BOARD);
  Hash_History[PLY] = compute_hash(BOARD);
  */

  /* TODO: validity check for two opposing kings present on board */

  /* store lastmove+ in board */
  board[QBBLAST] = lastmove;

  return true;
}
/* create fen string from board state */
void createfen (char *fenstring, Bitboard *board, int gameply)
{

}
/* run internal selftest */
void self_test (void) 
{
  return;
}
/* engine options and usage */
void print_help (void)
{
  printf ("Zeta Dva, version %s\n",VERSION);
  printf ("Yet another amateur level chess engine.\n");
  printf ("Copyright (C) 2011-2016 Srdja Matovic\n");
  printf ("This is free software, licensed under GPL >= v2\n");
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
  printf ("e.g.\n");
  printf ("new            // init new game from start position\n");
  printf ("level 40 4 0   // set time control to 40 moves in 4 minutes\n");
  printf ("go             // let engine play site to move\n");
  printf ("usermove d7d5  // let engine apply usermove and start thinking\n");
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
  printf ("Non-Xboard commands:\n");
  printf ("perft n        // perform a performance test to depth n\n");
  printf ("selftest       // run an internal selftest\n");
  printf ("help           // print usage hints\n");
  printf ("\n");
}
/* version info */
void print_version (void)
{
  printf ("Zeta Dva, version %s\n",VERSION);
  printf ("Copyright (C) 2011-2016 Srdja Matovic\n");
  printf ("This is free software, licensed under GPL >= v2\n");
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
  setbuf(stdout, NULL);
  setbuf(stdin, NULL);

  /* getopt loop */
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
        break;
      case 3:
        self_test ();
        break;
    }
  }

  /* init memory and tables */
  if (!inits())
    exit (EXIT_FAILURE);

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
  printf ("Zeta Dva, version %s\n",VERSION);
  printf ("Yet another amateur level chess engine.\n");
  printf ("Copyright (C) 2011-2016 Srdja Matovic\n");
  printf ("This is free software, licensed under GPL >= v2\n");

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
      printf("feature done=0\n");  
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
        printf("Error (unsupported protocoll version,  < v2): protover\n");
        printf("tellusererror (unsupported protocoll version, < v2): protover\n");
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
        printf("Error (in setting start postition): new\n");        
      }
      PLY = 0;
      xboard_force  = false;
			continue;
		}
    /* set board to position in FEN */
		if (!strcmp (Command, "setboard"))
    {
      sscanf (Line, "setboard %1023[0-9a-zA-Z /-]", Fen);
      if(!setboard (BOARD, Fen))
      {
        printf("Error (in setting chess psotition via fen string): setboard\n");        
      }
      PLY =0;
      continue;
		}
    if (!strcmp(Command, "go"))
    {
      /* zeta supports only CECP >= v2 */
      if (xboard_mode && xboard_protover<2)
      {
        printf("Error (unsupported protocoll version, < v2): go\n");
        printf("tellusererror (unsupported protocoll version. < v2): go\n");
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
        printf("Error (unsupported protocoll version, < v2): usermove\n");
        printf("tellusererror (unsupported protocoll version, <v2): usermove\n");
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
    /* xboard Commands to ignore */
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
      continue;
    }
    /* do an internal self test */
    if (!xboard_mode && !strcmp (Command, "selftest"))
    {
      continue;
    }
    /* print help */
    if (!xboard_mode && !strcmp (Command, "help"))
    {
      print_help();
      continue;
    }
    /* not supported xboard commands...tell user */
		if (!strcmp (Command, "edit"))
    {
      printf("Error (unsupported command): %s\n",Command);
      printf("tellusererror (unsupported command): %s\n",Command);
      printf("tellusererror engine supports only CECP (Xboard) version >=2\n");
      continue;
    }
		if (!strcmp (Command, "undo"))
    {
      printf("Error (unsupported command): %s\n",Command);
      printf("tellusererror (unsupported command): %s\n",Command);
      continue;
    }
		if (!strcmp (Command, "remove"))
    {
      printf("Error (unsupported command): %s\n",Command);
      printf("tellusererror (unsupported command): %s\n",Command);
      continue;
    }
		if (!strcmp (Command, "remove"))
    {
      printf("Error (unsupported command): %s\n",Command);
      printf("tellusererror (unsupported command): %s\n",Command);
      continue;
    }
		if (!strcmp (Command, "analyze"))
    {
      printf("Error (unsupported command): %s\n",Command);
      printf("tellusererror (unsupported command): %s\n",Command);
      continue;
    }
		if (!strcmp (Command, "pause"))
    {
      printf("Error (unsupported command): %s\n",Command);
      printf("tellusererror (unsupported command): %s\n",Command);
      continue;
    }
		if (!strcmp (Command, "resume"))
    {
      printf("Error (unsupported command): %s\n",Command);
      printf("tellusererror (unsupported command): %s\n",Command);
      continue;
    }
    /* unknown command...*/
    printf("Error (unsupported command): %s\n",Command);
  }
  /* close log file */
  if (Log_File != NULL)
    fclose (Log_File);

  /* free allocated memory */
  release_inits();

  exit (EXIT_SUCCESS);
}

