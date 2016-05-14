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

/* Global variables  */
FILE 	*log_file;              /* logfile for debug */
bool log_flag       = false;  /* log flag */
char *line;                   /* for fgetting the input on stdin */
char *command;                /* for pasring the xboard command */
char *fen;                    /* for storing the fen chess baord string */

/* xboard states */
bool xboard_mode    = false;  /* chess GUI sets to true */
bool xboard_force   = false;  /* if true aplly only moves, do not think */
bool xboard_post    = false;  /* post search thinking output */
s32 xboard_protover = 0;      /* Zeta works with protocoll version >= v2 */
/* game state */
bool STM            = WHITE;  /* site to move */
u32 SD              = MAXPLY; /* max search depth*/
u32 GAMEPLY         = 0;      /* total ply, considering depth via fen string */
u32 PLY             = 0;      /* engine specifix ply counter */
Move *MoveHistory;            /* last game moves indexed by ply */
Hash *HashHistory;            /* last game hashes indexed by ply */

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
/* initialize engine, history and hash table */
bool inits(void)
{
  /* memory allocation */
  line        = malloc(1024       * sizeof (char));
  command     = malloc(1024       * sizeof (char));
  fen         = malloc(1024       * sizeof (char));
  MoveHistory = malloc(MAXGAMEPLY * sizeof (Move));
  HashHistory = malloc(MAXGAMEPLY * sizeof (Hash));

  if (line == NULL) 
  {
    printf ("Error (memory allocation failed): char line[%u]", 1024);
    return false;
  }
  if (command == NULL) 
  {
    printf ("Error (memory allocation failed): char command[%u]", 1024);
    return false;
  }
  if (fen == NULL) 
  {
    printf ("Error (memory allocation failed): char fen[%u]", 1024);
    return false;
  }
  if (MoveHistory == NULL) 
  {
    printf ("Error (memory allocation failed): u64 MoveHistory[%u]", MAXGAMEPLY);
    return false;
  }
  if (HashHistory == NULL) 
  {
    printf ("Error (memory allocation failed): u64 HashHistory[%u]", MAXGAMEPLY);
    return false;
  }
  return true;
}
/* set internal chess board presentation to fen string */
bool setboard(char *fenstring) {

  char tempchar;
  char *position;
  char *cstm;
  char *castle;
  char *cep;
  char *tempstring;
  char fencharstring[24] = {" PNKBRQ pnkbrq/12345678"};
  int i;
  int j;
  int side;
  u64 hmc;
  u64 fendepth;
  File file;
  Rank rank;
  Piece piece;
  Square sq;
  Cr cr = BBEMPTY;
  Move lastmove = MOVENONE;

  /* memory, fen ist max 1023 char in size */
  position  = malloc(1024 * sizeof (char));
  if (position == NULL) 
  {
    printf ("Error (memory allocation failed): char position[%u]", 1024);
    return false;
  }
  cstm  = malloc(1024 * sizeof (char));
  if (cstm == NULL) 
  {
    printf ("Error (memory allocation failed): char cstm[%u]", 1024);
    return false;
  }
  castle  = malloc(1024 * sizeof (char));
  if (cstm == NULL) 
  {
    printf ("Error (memory allocation failed): char castle[%u]", 1024);
    return false;
  }
  cep  = malloc(1024 * sizeof (char));
  if (cstm == NULL) 
  {
    printf ("Error (memory allocation failed): char cep[%u]", 1024);
    return false;
  }

  /* get data from fen string */
	sscanf(fenstring, "%s %s %s %s %llu %llu", position, cstm, castle, cep, &hmc, &fendepth);

  /* empty the board */
  BOARD[QBBWHITE] = 0x0ULL;
  BOARD[QBBP1]    = 0x0ULL;
  BOARD[QBBP2]    = 0x0ULL;
  BOARD[QBBP3]    = 0x0ULL;
  BOARD[QBBHASH]  = 0x0ULL;
  BOARD[QBBLAST]  = 0x0ULL;

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
            BOARD[0] |= piece&0x1;
            BOARD[1] |= ((piece>>1)&0x1)<<sq;
            BOARD[2] |= ((piece>>2)&0x1)<<sq;
            BOARD[3] |= ((piece>>3)&0x1)<<sq;
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

  /* TODO: compute set hash
  HashHistory[PLY] = compute_hash(BOARD);
  */

  /* TODO: validity check for two kings present on board */

  return true;
}
/* create fen string from board state */
void createfen(char *fen, Bitboard *board, int gameply)
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
  printf ("Alternatively you can use Xboard commmands directly on commman line,\n"); 
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
  printf ("bk             // book lines\n");
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
        log_flag =true;
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
  if (log_flag)
  {
    log_file = fopen ("zetadva.log", "ab+");
    if (log_file==NULL)
      log_flag=false;
    else
    {
      char timestring[256];
      /* no buffers */
      setbuf(log_file, NULL);
      /* print binary call to log */
      get_time_string (timestring);
      fprintf (log_file, "%s, ", timestring);
      for (c=0;c<argc;c++)
      {
        fprintf (log_file, "%s ",argv[c]);
      }
      fprintf (log_file, "\n");
    }
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
    /* get line */
    if (!fgets (line, 1023, stdin)) {}
    /* ignore empty lines */
    if (line[0] == '\n')
      continue;
    /* print io to log file */
    if (log_flag)
    {
      char timestring[256];
      get_time_string (timestring);
      fprintf (log_file, "%s, ", timestring);
      fprintf (log_file, "%s\n",line);
    }
    /* get command */
    sscanf (line, "%s", command);

    /* xboard commands */
    /* set xboard mode */
    if (!strcmp (command, "xboard"))
    {
      printf("feature done=0\n");  
      xboard_mode = true;
      continue;
    }
    if (!strcmp(command, "protover")) 
    {
      /* get xboard protocoll version */
      sscanf (line, "protover %d", &xboard_protover);
      /* zeta supports only CECP >= v2 */
      if (xboard_mode && xboard_protover<2)
      {
        printf("Error (unsupported xboard protocoll version): < v2\n");
        printf("tellusererror (unsupported xboard protocoll version): < v2\n");
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
		if (!strcmp (command, "new"))
    {
      setboard ("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
      PLY = 0;
      xboard_force  = false;
			continue;
		}
    /* set board to position in FEN */
		if (!strcmp (command, "setboard"))
    {
      sscanf (line, "setboard %1023[0-9a-zA-Z /-]", fen);
      setboard (fen);
      PLY =0;
      continue;
		}
    if (!strcmp(command, "go"))
    {
      /* zeta supports only CECP >= v2 */
      if (xboard_mode && xboard_protover<2)
      {
        printf("Error (unsupported Xboard protocoll version): < v2\n");
        printf("tellusererror (unsupported Xboard protocoll version): < v2\n");
      }
      xboard_force = false;
      PLY++;
      STM = !STM;
      continue;
    }		
    /* set xboard force mode, no thinking just apply moves */
		if (!strcmp (command, "force"))
    {
      xboard_force = true;
      continue;
		}
    /* set time control */
		if (!strcmp (command, "level"))
    {
      continue;
		}
    /* set time control to n seconds per move */
		if (!strcmp (command, "st"))
    {
      continue;
		}
    if (!strcmp(command, "usermove"))
    {
      /* zeta supports only CECP >= v2 */
      if (xboard_mode && xboard_protover<2)
      {
        printf("Error (unsupported Xboard protocoll version): < v2\n");
        printf("tellusererror (unsupported Xboard protocoll version): < v2\n");
      }
      PLY++;
      STM = !STM;
      continue;
    }		
    /* exit program */
		if (!strcmp (command, "quit"))
    {
      break;
    }
    /* set search depth */
    if (!strcmp (command, "sd"))
    {
      sscanf (line, "sd %u", &SD);
      continue;
    }
    /* turn on thinking output */
		if (!strcmp (command, "post"))
    {
      xboard_post = true;
      continue;
		}
    /* turn off thinking output */
		if (!strcmp (command, "nopost"))
    {
      xboard_post = false;
      continue;
		}
    /* xboard commands to ignore */
		if (!strcmp (command, "white"))
    {
      continue;
		}
		if (!strcmp (command, "black"))
    {
      continue;
		}
		if (!strcmp (command, "draw"))
    {
      continue;
		}
		if (!strcmp (command, "ping"))
    {
      continue;
		}
		if (!strcmp (command, "result"))
    {
      continue;
		}
		if (!strcmp (command, "hint"))
    {
      continue;
		}
		if (!strcmp (command, "bk"))
    {
      continue;
		}
		if (!strcmp (command, "hard"))
    {
      continue;
		}
		if (!strcmp (command, "easy"))
    {
      continue;
		}
		if (!strcmp (command, "name"))
    {
      continue;
		}
		if (!strcmp (command, "rating"))
    {
      continue;
		}
		if (!strcmp (command, "ics"))
    {
      continue;
		}
		if (!strcmp (command, "computer"))
    {
      continue;
		}
    /* non xboard commands */
    /* do an node count to depth defined via sd  */
    if (!xboard_mode && !strcmp (command, "perft"))
    {
      continue;
    }
    /* do an internal self test */
    if (!xboard_mode && !strcmp (command, "selftest"))
    {
      continue;
    }
    /* print help */
    if (!xboard_mode && !strcmp (command, "help"))
    {
      print_help();
      continue;
    }
    /* not supported xboard commands...tell user */
		if (!strcmp (command, "edit"))
    {
      printf("Error (unsupported command): %s\n",command);
      printf("tellusererror (unsupported command): %s\n",command);
      printf("tellusererror engine supports only CECP (Xboard) version >=2\n");
      continue;
		}
		if (!strcmp (command, "undo"))
    {
      printf("Error (unsupported command): %s\n",command);
      printf("tellusererror (unsupported command): %s\n",command);
      continue;
		}
		if (!strcmp (command, "remove"))
    {
      printf("Error (unsupported command): %s\n",command);
      printf("tellusererror (unsupported command): %s\n",command);
      continue;
		}
		if (!strcmp (command, "remove"))
    {
      printf("Error (unsupported command): %s\n",command);
      printf("tellusererror (unsupported command): %s\n",command);
      continue;
		}
		if (!strcmp (command, "analyze"))
    {
      printf("Error (unsupported command): %s\n",command);
      printf("tellusererror (unsupported command): %s\n",command);
      continue;
		}
		if (!strcmp (command, "pause"))
    {
      printf("Error (unsupported command): %s\n",command);
      printf("tellusererror (unsupported command): %s\n",command);
      continue;
		}
		if (!strcmp (command, "resume"))
    {
      printf("Error (unsupported command): %s\n",command);
      printf("tellusererror (unsupported command): %s\n",command);
      continue;
		}
    /* unknown command...*/
    printf("Error (unsupported command): %s\n",command);
  }
  /* close log file */
  if (log_flag)
    fclose (log_file);

  exit (EXIT_SUCCESS);
}

