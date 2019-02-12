/* MS-DOS SHELL - Data Declarations
 *
 * MS-DOS SHELL - Copyright (c) 1990 Data Logic Limited and Charles Forsyth
 *
 * This code is based on (in part) the shell program written by Charles
 * Forsyth and is subject to the following copyright restrictions:
 *
 * 1.  Redistribution and use in source and binary forms are permitted
 *     provided that the above copyright notice is duplicated in the
 *     source form and the copyright notice in file sh6.c is displayed
 *     on entry to the program.
 *
 * 2.  The sources (or parts thereof) or objects generated from the sources
 *     (or parts of sources) cannot be sold under any circumstances.
 *
 *    $Header: sh6.c 1.13 90/05/15 21:10:19 MS_user Exp $
 *
 *    $Log:	sh6.c $
 * Revision 1.13  90/05/15  21:10:19  MS_user
 * Release 1.6.2
 *
 * Revision 1.12  90/05/09  20:35:41  MS_user
 * Change to release 1.6.1
 *
 * Revision 1.11  90/04/25  22:38:47  MS_user
 * Add initialisation for new field in IO_Args
 *
 * Revision 1.10  90/04/25  09:20:39  MS_user
 * Change version message processing
 *
 * Revision 1.9  90/04/11  12:57:12  MS_user
 * Update release date
 *
 * Revision 1.8  90/04/06  17:17:46  MS_user
 * RELEASE 1.6!
 *
 * Revision 1.7  90/03/26  20:55:36  MS_user
 * Move to beta test version
 *
 * Revision 1.6  90/03/12  20:34:07  MS_user
 * Add save program name variable for initialisation file
 *
 * Revision 1.5  90/03/06  15:13:48  MS_user
 * Set up for alpha release
 *
 * Revision 1.4  90/03/05  13:52:31  MS_user
 * Change in environment structure
 *
 * Revision 1.3  90/02/22  16:38:56  MS_user
 * Add XMS support
 *
 * Revision 1.2  90/02/16  16:58:22  MS_user
 * Set up 1.5 release
 *
 * Revision 1.1  90/01/25  13:42:04  MS_user
 * Initial revision
 *
 */

#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include "sh.h"

static char	*Copy_Right1 = "\nSH Version 1.6.2 - %s (OS/2 %d.%02d)\n";
static char	*Copy_Right2 = "Copyright (c) Data Logic Ltd and Charles Forsyth 1990\n";
static char	*Copy_Right3 = "Ported to OS/2 by Kai Uwe Rommel 1990\n\n";

char		**dolv;		/* Parameter array			*/
int		dolc;		/* Number of entries in parameter array	*/
int		exstat;		/* Exit status				*/
char		gflg;
int		fn_area_number = -1;	/* Next function area number	*/
bool		talking = FALSE;/* interactive (talking-type wireless)	*/
int		execflg;	/* Exec mode				*/
int		multiline;	/* \n changed to ;			*/
int		Current_Event = 0;	/* Current history event	*/
int		*failpt;	/* Current fail point jump address	*/
int		*errpt;		/* Current error point jump address	*/
				/* Swap mode				*/
Break_C		*Break_List;	/* Break list for FOR/WHILE		*/
Break_C		*Return_List;	/* Return list for RETURN		*/
Break_C		*SShell_List;	/* SubShell list for EXIT		*/
bool		level0 = FALSE;	/* Level Zero flag			*/
bool		r_flag = FALSE;	/* Restricted shell			*/
				/* History processing enabled flag	*/
bool		History_Enabled = FALSE;
Fun_Ops		*fun_list = (Fun_Ops *)NULL;	/* Function list	*/
Save_IO		*SSave_IO;	/* Save IO array			*/
int		NSave_IO_E = 0;	/* Number of entries in Save IO array	*/
int		MSave_IO_E = 0;	/* Max Number of entries in SSave_IO	*/
S_SubShell	*SubShells;	/* Save Vars array			*/
int		NSubShells = 0;	/* Number of entries in SubShells	*/
int		MSubShells = 0;	/* Max Number of entries in SubShells	*/

Word_B		*wdlist;	/* Current Word List			*/
Word_B		*iolist;	/* Current IO List			*/
long		ourtrap = 0L;	/* Signal detected			*/
int		trapset;	/* Trap pending				*/
int		yynerrs;	/* yacc errors detected			*/
int		Execute_stack_depth;	/* execute function recursion	*/
					/* depth			*/
Var_List	*vlist = (Var_List *)NULL;	/* dictionary		*/
Var_List	*path;		/* search path for commands		*/
Var_List	*ps1;		/* Prompt 1				*/
Var_List	*ps2;		/* Prompt 2				*/
Var_List	*C_dir;		/* Current directory			*/
char		*last_prompt;	/* Last prompt output			*/
Var_List	*ifs;		/* Inter-field separator		*/
char		*Program_Name;	/* Program name				*/
char		*home = "HOME";
char		*shell = "SHELL";
char		*history_file = "HISTFILE";
char		*hsymbol = "#";
char		*msymbol = "-";
char		*spcl2 = "$`'\"";

				/* I/O stacks				*/
IO_Args		ioargstack[NPUSH];
IO_State	iostack[NPUSH];

				/* Temporary I/O argument		*/
IO_Args		temparg = {
    (char *)NULL,		/* Word					*/
    (char **)NULL,		/* Word list				*/
    0,				/* File descriptor			*/
    AFID_NOBUF,			/* Buffer id				*/
    0L,				/* File position			*/
    0,				/* Offset in buffer			*/
    (IO_Buf *)NULL		/* Buffer				*/
};

int		areanum;	/* Current allocation area		*/
int		inparse;	/* In parser flag			*/
long		flags = 0L;	/* Command line flags			*/
char		*null = "";

				/* Current environment			*/
Environ	e = {
    (char *)NULL,		/* Current line buffer			*/
    (char *)NULL,		/* Current pointer in line		*/
    (char *)NULL,		/* End of line pointer			*/
    iostack,			/* I/O Stack pointers			*/
    iostack - 1,
    (int *)NULL,
    FALSE,			/* End of file processing		*/
    FDBASE,			/* Base file handler			*/
    (Environ *)NULL		/* Previous Env pointer			*/
};

/* The only bit of code in this module prints the version number */

void	Print_Version (fp)
int	fp;
{
    char	buf[100];

    sprintf (buf, Copy_Right1, __DATE__, _osmajor / 10, _osminor);
    write (fp, buf, strlen (buf));
    write (fp, Copy_Right2, strlen (Copy_Right2));
    write (fp, Copy_Right3, strlen (Copy_Right3));
}
