/* MS-DOS SHELL - Main program, memory and variable management
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
 *    $Header: sh1.c 1.16 90/05/31 09:48:06 MS_user Exp $
 *
 *    $Log:	sh1.c $
 * Revision 1.16  90/05/31  09:48:06  MS_user
 * Implement partial write when swapping to disk
 * Add some signal lockouts to prevent corruption
 *
 * Revision 1.15  90/05/15  21:08:59  MS_user
 * Restore original directory on exit
 *
 * Revision 1.14  90/04/25  22:33:28  MS_user
 * Fix rsh check for PATH
 *
 * Revision 1.13  90/04/25  09:18:12  MS_user
 * Change version message processing
 *
 * Revision 1.12  90/04/04  11:32:12  MS_user
 * Change MAILPATH to use a semi-colon and not a colon for DOS
 *
 * Revision 1.11  90/04/03  17:58:35  MS_user
 * Stop shell exit from lowest level CLI
 *
 * Revision 1.10  90/03/27  20:24:49  MS_user
 * Fix problem with Interrupts not restoring std??? and clearing extended file
 *
 * Revision 1.9  90/03/26  20:56:13  MS_user
 * Change I/O restore so that "exec >filename" works
 *
 * Revision 1.8  90/03/26  04:30:14  MS_user
 * Remove original Interrupt 24 save address
 *
 * Revision 1.7  90/03/12  20:16:22  MS_user
 * Save program name for Initialisation file processing
 *
 * Revision 1.6  90/03/09  16:05:33  MS_user
 * Add build file name function and change the profile check to use it
 *
 * Revision 1.5  90/03/06  16:49:14  MS_user
 * Add disable history option
 *
 * Revision 1.4  90/03/06  15:09:27  MS_user
 * Add Unix PATH variable conversion
 *
 * Revision 1.3  90/03/05  13:47:45  MS_user
 * Get /etc/profile and profile order rigth
 * Use $HOME/profile and not profile
 * Check cursor position before outputing prompt
 * Move some of processing in main to sub-routines
 *
 * Revision 1.2  90/02/14  04:46:20  MS_user
 * Add Interrupt 24 processing
 *
 * Revision 1.1  90/01/25  13:40:39  MS_user
 * Initial revision
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <dos.h>
#include <time.h>
#include "sh.h"

#define INCL_NOPM
#define INCL_DOSPROCESS
#define INCL_DOSERRORS
#include <os2.h>

/*
 * Structure of Malloced space to allow release of space nolonger required
 * without having to know about it.
 */

typedef struct region {
    struct region	*next;
    int			area;
} s_region;

static struct region	*areastart = (s_region *)NULL;

/*
 * default shell, search rules
 */

static char	*shellname = "c:/bin/sh";
static char	*search    = ";c:/bin;c:/usr/bin";
static char	*ymail     = "You have mail\n";
static char	*Path	   = "PATH";
				/* Entry directory			*/
static char	*Start_directory = (char *)NULL;
#ifdef SIGQUIT
static void	(*qflag)(int) = SIG_IGN;
#endif

/* Functions */

static char	*cclass (char *, int, bool);
static char	*copy_to_equals (char *, char *);
static void	nameval (Var_List *, char *, char *, bool);
static bool	Initialise (char *);
static void	onecommand (void);
static void	Check_Mail (void);
static void	Pre_Process_Argv (char **);
static void	Load_G_VL (void);
static void	Convert_Backslashes (char *);
static void	Load_profiles (void);
static void	U2D_Path (void);
static void     dowaits(int);

unsigned int SW_intr;

/*
 * The main program starts here
 */

void		main (argc, argv)
int		argc;
register char	**argv;
{
    register int	f;
    int			cflag = 0;
    int			sc;
    char		*name = *argv;
    char		**ap;
    int			(*iof)(IO_State *) = filechar;
					/* Load up various parts of the	*/
					/* system			*/
    bool		l_rflag = Initialise (*argv);

/* Preprocess options to convert two character options of the form /x to
 * -x.  Some programs!!
 */

    Pre_Process_Argv (argv);

/* Save the start directory for when we exit */

    Start_directory = getcwd ((char *)NULL, PATH_MAX + 4);

/* Process the options */

    while ((sc = getopt (argc, argv, "abc:defghijklmnopqrtsuvwxyz0")) != EOF)
    {
	switch (sc)
	{
	    case '0':				/* Level 0 flag for DOS	*/
		level0 = TRUE;
		break;

	    case 'r':				/* Restricted		*/
		l_rflag = TRUE;
		break;

	    case 'c':				/* Command on line	*/
		ps1->status &= ~EXPORT;
		ps2->status &= ~EXPORT;
		setval (ps1, null);
		setval (ps2, null);
		cflag = 1;

		PUSHIO (aword, optarg, iof = nlchar);
		break;

	    case 'q':				/* No quit ints		*/
#ifdef SIGQUIT
		qflag = SIG_DFL;
#endif
		break;

	    case 's':				/* standard input	*/
		break;

	    case 't':				/* One command		*/
		ps1->status &= ~EXPORT;
		setval (ps1, null);
		iof = linechar;
		break;

	    case 'i':				/* Set interactive	*/
		talking = TRUE;

	    default:
		if (islower (sc))
		    FL_SET (sc);
	}
    }

    argv += optind;
    argc -= optind;

/* Execute one off command - disable prompts */

    if ((iof == filechar) && (argc > 0))
    {
	setval (ps1, null);
	setval (ps2, null);
	ps1->status &= ~EXPORT;
	ps2->status &= ~EXPORT;

	f = 0;

/* Open the file if necessary */

	if (strcmp ((name = *argv), "-") != 0)
	{
	    if ((f = O_for_execute (name, (char **)NULL, (int *)NULL)) < 0)
	    {
		print_error ("%s: cannot open\n", name);
		exit (1);
	    }
	}

	PUSHIO (afile, remap (f), filechar); 	/* Load into I/O stack	*/
    }

/* Set up the $- variable */

    setdash ();

/* Load terminal I/O structure if necessary and load the history file */

    if (e.iop < iostack)
    {
	PUSHIO (afile, 0, iof);

	if (isatty (0) && isatty (1) && !cflag)
	{
	    Print_Version (2);

	    talking = TRUE;
#ifndef NO_HISTORY
	    History_Enabled = TRUE;
	    Load_History ();
	    Configure_Keys ();
#endif
	}
    }

#ifdef SIGQUIT
    signal (SIGQUIT, qflag);
#endif

/* Read profile ? */

    if (((name != (char *)NULL) && (*name == '-')) || level0)
	Load_profiles ();

/* Set up signals */

    if (talking)
	signal (SIGTERM, sig);

    if (signal (SIGINT, SIG_IGN) != SIG_IGN)
	signal (SIGINT, onintr);

/* Load any parameters */

    dolv = argv;
    dolc = argc;
    dolv[0] = name;

    if (dolc > 1)
    {
	for (ap = ++argv; --argc > 0;)
	{
	    if (assign (*ap = *argv++, !COPYV))
		dolc--;					/* keyword */

	    else
		ap++;
	}
    }

    setval (lookup ("#", TRUE), putn ((--dolc < 0) ? (dolc = 0) : dolc));

/* Execute the command loop */

    while (1)
    {
	if (talking && e.iop <= iostack)
	{
	    In_Col_Zero ();
	    Check_Mail ();
	    put_prompt (ps1->value);
	    r_flag = l_rflag;
	    closeall ();		/* Clean up any open shell files */
	}

	onecommand ();

        dowaits(0);  /* wait for ending async. processes */
    }
}

/*
 * Set up the value of $-
 */

void	setdash ()
{
    register char	*cp, c;
    char		m['z' - 'a' + 1];

    for (cp = m, c = 'a'; c <= 'z'; ++c)
    {
	if (FL_TEST (c))
	    *(cp++) = c;
    }

    *cp = 0;
    setval (lookup ("-", TRUE), m);
}

/* Execute a command */

static void	onecommand ()
{
    register int	i;
    jmp_buf		m1;
    C_Op		*outtree = (C_Op *)NULL;

/* Exit any previous environments */

    while (e.oenv)
	quitenv ();

/* initialise space */

    areanum = 1;
    freehere (areanum);
    freearea (areanum);
    wdlist = (Word_B *)NULL;
    iolist = (Word_B *)NULL;
    e.errpt = (int *)NULL;
    e.cline = space (LINE_MAX);
    e.eline = e.cline + LINE_MAX - 5;
    e.linep = e.cline;
    yynerrs = 0;
    multiline = 0;
    inparse = 1;
    SW_intr = 0;
    execflg = 0;

/* Get the line and process it */

    if (setjmp (failpt = m1) || ((outtree = yyparse ()) == (C_Op *)NULL) ||
	SW_intr)
    {

/* Failed - If parse failed - save command line as history */

#ifndef NO_HISTORY
	if ((outtree == (C_Op *)NULL) && Interactive ())
	    Add_History (FALSE);
#endif

/* If interrupt occured, remove current Input stream */

	if (SW_intr && (e.iop > e.iobase))
	    e.iop--;

/* Quit all environments */

	while (e.oenv)
	    quitenv ();

	scraphere ();

	if (!talking && SW_intr)
	    leave ();

/* Exit */

	inparse = 0;
	SW_intr = 0;
	return;
    }

/* Ok - reset some variables and then execute the command tree */

    inparse = 0;
    Break_List = (Break_C *)NULL;
    Return_List = (Break_C *)NULL;
    SShell_List = (Break_C *)NULL;
    SW_intr = 0;
    execflg = 0;

/* Set execute function recursive level and the SubShell count to zero */

    Execute_stack_depth = 0;

/* Set up Redirection IO (Saved) array and SubShell Environment information */

    NSave_IO_E = 0;		/* Number of entries		*/
    MSave_IO_E = 0;		/* Max Number of entries	*/
    NSubShells = 0;		/* Number of entries		*/
    MSubShells = 0;		/* Max Number of entries	*/

/* Save the environment information */

#ifndef NO_HISTORY
    if (Interactive ())
	Add_History (FALSE);
#endif

/* Ok - if we wail, we need to clean up the stacks */

    if ((setjmp (failpt = m1) == 0) && !FL_TEST ('n'))
	execute (outtree, NOPIPE, NOPIPE, 0);

/* Make sure the I/O and environment are back at level 0 and then clear them */

    Execute_stack_depth = 0;
    Clear_Extended_File ();

    if (NSubShells != 0)
	Delete_G_VL ();

    if (NSave_IO_E)
	restore_std (0, TRUE);

    if (MSubShells)
	DELETE (SubShells);

    if (MSave_IO_E)
	DELETE (SSave_IO);

/* Check for interrupts */

    if (!talking && SW_intr)
    {
	execflg = 0;
	leave ();
    }

/* Run any traps that are required */

    if ((i = trapset) != 0)
    {
	trapset = 0;
	runtrap (i);
    }
}


/*
 * look for any ending childs
 */

static void dowaits(int mode)
{
  RESULTCODES rc;
  PID pid;
  char buffer[32];
  USHORT res;
  USHORT wmode = DCWW_NOWAIT;

  for (;;)
  {
    res = DosCwait(DCWA_PROCESS, wmode, &rc, &pid, 0);

    if ( mode && (res == ERROR_CHILD_NOT_COMPLETE) )
    {
      S_puts("(Waiting for childs)\n");
      wmode = DCWW_WAIT;
      continue;
    }
    else
      if ( res )
        break;

    sprintf(buffer, "[%u] ", pid);
    S_puts(buffer);

    switch (rc.codeTerminate)
    {
    case TC_EXIT:
      sprintf(buffer, "Done (%u)\n", rc.codeResult);
      S_puts(buffer);
      break;
    case TC_HARDERROR:
      S_puts("Terminated\n");
      break;
    case TC_TRAP:
      sprintf(buffer, "Trap %d\n", rc.codeResult);
      S_puts(buffer);
      break;
    case TC_KILLPROCESS:
      S_puts("Killed\n");
      break;
    }
  }
}


/*
 * Terminate current environment with an error
 */

void	fail ()
{
    longjmp (failpt, 1);

    /* NOTREACHED */
}

/*
 * Exit the shell
 */

void	leave ()
{
    if (execflg)
	fail ();

#if 0
    if (Orig_I24_V == (void (far *)())NULL)
    {
	S_puts ("sh: ignoring attempt to leave lowest level shell\n");
	fail ();
    }
#endif

/* Clean up */

    scraphere ();
    freehere (1);

/* Trap zero on exit */

    runtrap (0);

/* wait for running bg processes */

    dowaits(1);

/* Dump history on exit */

#ifndef NO_HISTORY
    if (talking && isatty(0))
	Dump_History ();
#endif

    closeall ();

/* If this is a command only - restore the directory because DOS doesn't
 * and the user might expect it
 */

    if (Start_directory != (char *)NULL)
	Restore_Dir (Start_directory);

/* Exit - hurray */

    exit (exstat);

/* NOTREACHED */
}

/*
 * Output warning message
 */

void	print_warn (fmt)
char	*fmt;
{
    va_list	ap;
    char	x[100];

    va_start (ap, fmt);
    vsprintf (x, fmt, ap);
    S_puts (x);
    exstat = -1;

/* If leave on error - exit */

    if (FL_TEST ('e'))
	leave ();

    va_end (ap);
}

/*
 * Output error message
 */

void	print_error (fmt)
char	*fmt;
{
    va_list	ap;
    char	x[100];

/* Error message processing */

    va_start (ap, fmt);
    vsprintf (x, fmt, ap);
    S_puts (x);
    exstat = -1;

    if (FL_TEST ('e'))
	leave ();

    va_end (ap);

/* Error processing */

    if (FL_TEST ('n'))
	return;

/* If not interactive - exit */

    if (!talking)
	leave ();

    if (e.errpt)
	longjmp (e.errpt, 1);

/* closeall (); Removed - caused problems.  There may be problems
 * remaining with files left open?
 */

    e.iop = e.iobase = iostack;
}

/*
 * Create or delete a new environment.  If f is set, delete the environment
 */

bool	newenv (f)
int	f;
{
    register Environ	*ep;

/* Delete environment? */

    if (f)
    {
	quitenv ();
	return TRUE;
    }

/* Create a new environment */

    if ((ep = (Environ *) space (sizeof (Environ))) == (Environ *)NULL)
    {
	while (e.oenv)
	    quitenv ();

	fail ();
    }

    *ep = e;
    e.eof_p = FALSE;			/* Disable EOF processing	*/
    e.oenv  = ep;
    e.errpt = errpt;
    return FALSE;
}

/*
 * Exit the current environment successfully
 */

void	quitenv ()
{
    register Environ	*ep;
    register int	fd;

/* Restore old environment, delete the space and close any files opened in
 * this environment
 */

    if ((ep = e.oenv) != (Environ *)NULL)
    {
	fd = e.iofd;
	e = *ep;

	DELETE (ep);

	while (--fd >= e.iofd)
	    S_close (fd, TRUE);
    }
}

/*
 * Is character c in s?
 */

bool		any (c, s)
register char	c;
register char	*s;
{
    while (*s)
    {
	if (*(s++) == c)
	    return TRUE;
    }

    return FALSE;
}

/*
 * Convert binary to ascii
 */

char		*putn (n)
register int	n;
{
    static char		nt[10];

    sprintf (nt, "%u", n);
    return nt;
}

/*
 * SIGINT interrupt processing
 */

void	onintr (signo)
int	signo;
{

/* Restore signal processing and set SIGINT detected flag */

    signal (SIGINT, onintr);
    SW_intr = 1;

/* Are we talking to the user?  Yes - check in parser */

    if (talking)
    {
	if (inparse)
	    S_putc (NL);

/* Abandon processing */

	fail ();
    }

/* No - exit */

    else
    {
	execflg = 0;
	leave ();
    }
}

/*
 * Grap some space and check for an error
 */

char	*space (n)
int	n;
{
    register char *cp;

    if ((cp = getcell (n)) == (char *)NULL)
	print_error ("sh: out of string space\n");

    return cp;
}

/*
 * Save a string in a given area
 */

char		*strsave (s, a)
register char	*s;
{
    register char	*cp;

    if ((cp = space (strlen (s) + 1)) != (char *)NULL)
    {
	setarea ((char *)cp, a);
	return strcpy (cp, s);
    }

    return null;
}

/*
 * trap handling - Save signal number and restore signal processing
 */

void		sig (i)
register int	i;
{
    if (i == SIGINT)		/* Need this because swapper sets it	*/
	SW_intr = 0;

    trapset = i;
    signal (i, sig);
}

/*
 * Execute a trap command
 */

void	runtrap (i)
int	i;
{
    char	*trapstr;
    char	tval[10];

    sprintf (tval, "~%d", i);

    if (((trapstr = lookup (tval, FALSE)->value)) == null)
	return;

/* If signal zero, save a copy of the trap value and then delete the trap */

    if (i == 0)
    {
	trapstr = strsave (trapstr, areanum);
	unset (tval, TRUE);
    }

    RUN (aword, trapstr, nlchar, TRUE);
}

/*
 * Find the given name in the dictionary and return its value.  If the name was
 * not previously there, enter it now and return a null value.
 */

Var_List	*lookup (n, cflag)
register char	*n;
bool		cflag;
{
    register Var_List	*vp;
    register char	*cp;
    register int	c;
    static Var_List	dummy;

/* Set up the dummy variable */

    dummy.name = n;
    dummy.status = RONLY;
    dummy.value = null;

/* If digit string - use the dummy to return the value */

    if (isdigit (*n))
    {
	for (c = 0; isdigit (*n) && (c < 1000); n++)
	    c = c * 10 + *n - '0';

	dummy.value = (c <= dolc) ? dolv[c] : null;
	return &dummy;
    }

/* Look up in list */

    for (vp = vlist; vp != (Var_List *)NULL; vp = vp->next)
    {
	if (eqname (vp->name, n))
	    return vp;
    }

/* If we don't want to create it, return a dummy */

    if (!cflag)
	return &dummy;

/* Create a new variable */

    cp = findeq (n);

    if (((vp = (Var_List *)space (sizeof (Var_List))) == (Var_List *)NULL)
	|| (vp->name = space ((int)(cp - n) + 2)) == (char *)NULL)
    {
	dummy.name = null;
	return &dummy;
    }

/* Set area for space to zero - no auto-delete */

    setarea ((char *)vp, 0);
    setarea ((char *)vp->name, 0);

/* Just the name upto the equals sign, no more */

    copy_to_equals (vp->name, n);

/* Link into list */

    vp->value = null;
    vp->next = vlist;
    vp->status = GETCELL;
    vlist = vp;
    return vp;
}

/*
 * give variable at `vp' the value `val'.
 */

void		setval (vp, val)
Var_List	*vp;
char		*val;
{
    nameval (vp, val, (char *)NULL, FALSE);
}

/*
 * Copy and check that it terminates in an equals sign
 */

static char	*copy_to_equals (d, s)
char		*d, *s;
{
    int		n = (int) (findeq (s) - s);

    strncpy (d, s, n);
    *(d += n) = '=';
    *(++d) = 0;
    return d;
}

/*
 * Set up new value for name
 *
 * If name is not NULL, it must be a prefix of the space `val', and end with
 * `='.  This is all so that exporting values is reasonably painless.
 */

static void		nameval (vp, val, name, disable)
register Var_List	*vp;
char			*val;
char			*name;
bool			disable;
{
    register char	*xp;
    int			fl = 0;

/* Check if variable is read only */

    if (vp->status & RONLY)
    {
	char	c = *(xp = findeq (vp->name));

	*xp = 0;
	S_puts (xp);
	*xp = c;
	print_error (" is read-only\n");
	return;
    }

/* Check for $PATH reset in restricted shell */

    if (!disable && (strncmp (vp->name, "PATH=", 5) == 0) && check_rsh (Path))
	return;

/* Get space for string ? */

    if (name == (char *)NULL)
    {
	if ((xp = space (strlen (vp->name) + strlen (val) + 2)) == (char *)NULL)
	    return;

/* make string:  name=value */

	setarea ((char *)xp, 0);
	name = xp;

	xp = copy_to_equals (xp, vp->name);
	strcpy (xp, val);
	val = xp;
	fl = GETCELL;
    }

    if (vp->status & GETCELL)
	DELETE (vp->name);	/* form new string `name=value' */

    vp->name = name;
    vp->value = val;
    vp->status |= fl;

    if (FL_TEST ('a'))
	s_vstatus (vp, EXPORT);

/* Convert UNIX to DOS for PATH variable */

    if (vp == path)
	U2D_Path ();
}

/*
 * Set the status of an environment variable
 */

void		s_vstatus (vp, flag)
Var_List	*vp;
int		flag;			/* Addition status flags	*/
{
    if (isalpha (*vp->name))		/* not an internal symbol ($# etc) */
	vp->status |= flag;
}

/*
 * Check for assignment X=Y
 */

bool		isassign (s)
register char	*s;
{
    if (!isalpha (*s))
	return FALSE;

    for (; *s != '='; s++)
    {
	if (!*s || !isalnum (*s))
	    return FALSE;
    }

    return TRUE;
}

/*
 * Execute an assignment.  If a valid assignment, load it into the variable
 * list.
 */

bool		assign (s, cf)
register char	*s;
int		cf;
{
    register char	*cp;
    Var_List		*vp;

    if (!isalpha (*s))
	return FALSE;

    for (cp = s; *cp != '='; cp++)
    {
	if (!*cp || !isalnum (*cp))
	    return FALSE;
    }

    nameval ((vp = lookup (s, TRUE)), ++cp, (cf == COPYV ? (char *)NULL : s),
	     FALSE);

    if (cf != COPYV)
	vp->status &= ~GETCELL;

    return TRUE;
}

/*
 * Compare two environment strings
 */

bool			eqname(n1, n2)
register char		*n1, *n2;
{
    for (; *n1 != '=' && *n1 != 0; n1++)
    {
	if (*n2++ != *n1)
	    return FALSE;
    }

    return (!*n2 || (*n2 == '=')) ? TRUE : FALSE;
}

/*
 * Find the equals sign in a string
 */

char		*findeq (cp)
register char	*cp;
{
    while (*cp && (*cp != '='))
	cp++;

    return cp;
}

/*
 * Duplicate the Variable List for a Subshell
 *
 * Create a new Var_list environment for a Sub Shell
 */

int	Create_NG_VL ()
{
    int			i;
    S_SubShell		*sp;
    Var_List		*vp, *vp1;

    for (sp = SubShells, i = 0; (i < NSubShells) &&
			       (SubShells[i].depth < Execute_stack_depth);
	 i++);

/* If depth is greater or equal to the Execute_stack_depth - we should panic
 * as this should not happen.  However, for the moment, I'll ignore it
 */

    if (NSubShells == MSubShells)
    {
	sp = (S_SubShell *)space ((MSubShells + SSAVE_IO_SIZE) * sizeof (S_SubShell));

/* Check for error */

	if (sp == (S_SubShell *)NULL)
	{
	    errno = ENOMEM;
	    return -1;
	}

/* Save original data */

	if (MSubShells != 0)
	{
	    memcpy (sp, SubShells, sizeof (S_SubShell) * MSubShells);
	    DELETE (SubShells);
	}

	setarea ((char *)sp, 0);
	SubShells = sp;
	MSubShells += SSAVE_IO_SIZE;
    }

/* Save the depth and the old vlist value */

    sp = &SubShells[NSubShells++];
    sp->depth  = Execute_stack_depth;
    sp->header = vlist;
    vlist = (Var_List *)NULL;

/* Duplicate the old Variable list */

    for (vp = sp->header; vp != (Var_List *)NULL; vp = vp->next)
    {
	nameval ((vp1 = lookup (vp->name, TRUE)), findeq (vp->name) + 1,
		 (char *)NULL, TRUE);

	vp1->status |= (vp->status & (C_MSDOS | PONLY | EXPORT | RONLY));
    }

/* Reset global values */

    Load_G_VL ();
    return 0;
}

/*
 * Delete a SubShell environment and restore the original
 */

void	Delete_G_VL ()
{
    int		j;
    S_SubShell	*sp;
    Var_List	*vp;

    for (j = NSubShells; j > 0; j--)
    {
       sp = &SubShells[j - 1];

       if (sp->depth < Execute_stack_depth)
	   break;

/* Reduce number of entries */

	--NSubShells;

/* Release the space */

	for (vp = vlist; vp != (Var_List *)NULL; vp = vp->next)
	{
	    if (vp->status & GETCELL)
		DELETE (vp->name);

	    DELETE (vp);
	}

/* Restore vlist information */

	vlist = sp->header;
	Load_G_VL ();
    }
}

/*
 * Load GLobal Var List values
 */

static void	Load_G_VL ()
{
    path  = lookup (Path, TRUE);
    ifs   = lookup ("IFS", TRUE);
    ps1   = lookup ("PS1", TRUE);
    ps2   = lookup ("PS2", TRUE);
    C_dir = lookup ("~", TRUE);
    Restore_Dir (C_dir->value);
}

/*
 * Match a pattern as in sh(1).
 */

bool		gmatch (s, p, IgnoreCase)
register char	*s, *p;
bool		IgnoreCase;
{
    register int	sc, pc;

    if ((s == (char *)NULL) || (p == (char *)NULL))
	return FALSE;

    while ((pc = *(p++) & CMASK) != '\0')
    {
	sc = *(s++) & QMASK;

	switch (pc)
	{
	    case '[':			/* Class expression		*/
		if ((p = cclass (p, sc, IgnoreCase)) == (char *)NULL)
		    return FALSE;

		break;

	    case '?':			/* Match any character		*/
		if (sc == 0)
		    return FALSE;

		break;

	    case '*':			/* Match as many as possible	*/
		s--;
		do
		{
		    if (!*p || gmatch (s, p, IgnoreCase))
			return TRUE;

		} while (*(s++));

		return FALSE;

	    default:
		if (IgnoreCase)
		{
		    sc = tolower (sc);
		    pc = tolower ((pc & ~QUOTE));
		}

		if (sc != (pc & ~QUOTE))
		    return FALSE;
	}
    }

    return (*s == 0) ? TRUE : FALSE;
}

/*
 * Process a class expression - []
 */

static char	*cclass (p, sub, IgnoreCase)
register char	*p;
register int	sub;
bool		IgnoreCase;
{
    register int	c, d, not, found;

/* Exclusive or inclusive class */

    if ((not = *p == NOT) != 0)
	p++;

    found = not;

    do
    {
	if (!*p)
	    return (char *)NULL;

/* Get the next character in class, converting to lower case if necessary */

	c = IgnoreCase ? tolower ((*p & CMASK)) : (*p & CMASK);

/* If this is a range, get the end of range character */

	if ((*(p + 1) == '-') && (*(p + 2) != ']'))
	{
	    d = IgnoreCase ? tolower ((*(p + 2) & CMASK)) : (*(p + 2) & CMASK);
	    p++;
	}

	else
	    d = c;

/* Is the current character in the class? */

	if ((c <= sub) && (sub <= d))
	    found = !not;

    } while (*(++p) != ']');

    return found ? p + 1 : (char *)NULL;
}

/*
 * Get a string in a malloced area
 */

char		*getcell(nbytes)
unsigned int	nbytes;
{
    s_region		*np;
    void		(*save_signal)(int);

    if (nbytes == 0)
	abort ();	/* silly and defeats the algorithm */

/* Grab some space */

    if ((np = (s_region *)calloc (nbytes + sizeof (s_region), 1)) == (s_region *)NULL)
        return (char *)NULL;

/* Disable signals */

    save_signal = signal (SIGINT, SIG_IGN);

/* Link into chain */

    np->next = areastart;
    np->area = areanum;
    areastart = np;

/* Restore signals */

    signal (SIGINT, save_signal);

    return ((char *)np) + sizeof (s_region);
}

/*
 * Free a string in a malloced area
 */

void	freecell (s)
char	*s;
{
    register s_region	*cp = areastart;
    s_region		*lp = (s_region *)NULL;
    s_region		*sp = (s_region *)(s - sizeof (s_region));
    void		(*save_signal)(int);

/* Disable signals */

    save_signal = signal (SIGINT, SIG_IGN);

/* Find the string in the chain */

    if (s != (char *)NULL)
    {
	while (cp != (s_region *)NULL)
	{
	    if (cp != sp)
	    {
		lp = cp;
		cp = cp->next;
		continue;
	    }

/* First in chain ? */

	    else if (lp == (s_region *)NULL)
		areastart = cp->next;

/* Delete the current entry and relink */

	    else
		lp->next = cp->next;

	    free (cp);
	    break;
	}
    }

/* Restore signals */

    signal (SIGINT, save_signal);
}

/*
 * Autodelete space nolonger required.  Ie. Free all the strings in a malloced
 * area
 */

void		freearea (a)
register int	a;
{
    register s_region	*cp = areastart;
    s_region		*lp = (s_region *)NULL;
    void		(*save_signal)(int);

/* Disable signals */

    save_signal = signal (SIGINT, SIG_IGN);

    while (cp != (s_region *)NULL)
    {

/* Is the area number less than that specified - yes, continue */

	if (cp->area < a)
	{
	    lp = cp;
	    cp = cp->next;
	}

/* OK - delete the area.  Is it the first in chain ?  Yes, delete, relink
 * and update start location
 */

	else if (lp == (s_region *)NULL)
	{
	    lp = cp;
	    cp = cp->next;
	    areastart = cp;

	    free (lp);
	    lp = (char *)NULL;
	}

/* Not first, delete the current entry and relink */

	else
	{
	    lp->next = cp->next;
	    free (cp);
	    cp = lp->next;
	}
    }

/* Restore signals */

    signal (SIGINT, save_signal);
}

/*
 * Set the area number for a malloced string.  This allows autodeletion of
 * space that is nolonger required.
 */

void	setarea (cp,a)
char	*cp;
int	a;
{
    s_region	*sp = (s_region *)(cp - sizeof (s_region));

    if (cp != (char *)NULL)
	sp->area = a;
}

/*
 * Get the area number for a malloced string
 */

int	getarea (cp)
char	*cp;
{
    s_region	*sp = (s_region *)(cp - sizeof (s_region));

    return sp->area;
}

/* Output one of the Prompt.  We save the prompt for the history part of
 * the program
 */

void	put_prompt (s)
char	*s;
{
    time_t ti;
    struct tm *tl;
    int	i;
    char buf[PATH_MAX + 4];

    last_prompt = s;		/* Save the Last prompt output		*/

    time(&ti);
    tl = localtime(&ti);

    while (*s)
    {

/* If a format character, process it */

	if (*s == '%')
	{
	    s++;
	    *s = tolower(*s);

	    if (*s == '%')
		v1_putc ('%');

	    else
	    {
		*buf = 0;

		switch (*(s++))
		{
		    case 'e':		    /* Current event number */
			if (History_Enabled)
			    sprintf (buf, "%d", Current_Event + 1);

			break;

		    case 't':		    /* time	    */
			sprintf (buf,"%.2d:%.2d", tl -> tm_hour, tl -> tm_min);
			break;

		    case 'd':		    /* date	    */
			sprintf (buf, "%.3s %.2d-%.2d-%.2d",
				 &"SunMonTueWedThuFriSat"[tl -> tm_wday * 3],
				 tl -> tm_mday, tl -> tm_mon, tl -> tm_year % 100);
			break;

		    case 'p':		    /* directory    */
		    case 'n':		    /* default drive */
			strcpy (buf, C_dir->value);

			if (*(s - 1) == 'n')
			    buf[1] = 0;

			break;

		    case 'v':		    /* version	    */
			sprintf (buf, "%.2d.%.2d", _osmajor, _osminor);
			break;
		}

/* Output the string */

		v1_puts (buf);
	    }
	}

/* Escaped character ? */

	else if (*s == '\\')
	{
	    ++s;
	    if ((i = Process_Escape (&s)) == -1)
		i = 0;

	    v1_putc ((char)i);
	}

	else
	    v1_putc (*(s++));
    }
}

/*
 * Get the current path in UNIX format and save it in the environment
 * variable $~
 */

void	Getcwd ()
{
    char	ldir[PATH_MAX + 6];

    getcwd (ldir, PATH_MAX + 4);
    ldir[PATH_MAX + 5] = 0;

/* Convert to Unix format */

    Convert_Backslashes (strlwr (ldir));

/* Save in environment */

    setval ((C_dir = lookup ("~", TRUE)), ldir);
}

/*
 * Initialise the shell and Patch up various parts of the system for the
 * shell.  At the moment, we modify the ctype table so that _ is an upper
 * case character.
 */

static bool	Initialise (name)
char		*name;
{
    register char	*s, *s1;
    char		**ap;
    Var_List		*lset, *init;
    bool		l_rflag = FALSE;

/* Patch the ctype table as a cheat */

    (_ctype+1)['_'] |= _UPPER;

/* Get original interrupt 24 address and set up our new interrupt 24
 * address
 */

/* Load the environment into our structures */

    if ((ap = environ) != (char **)NULL)
    {
	while (*ap)
	    assign (*ap++, !COPYV);

	for (ap = environ; *ap;)
	    s_vstatus (lookup (*ap++, TRUE), EXPORT);
    }

/* Change COMSPEC to unix format for execution */

    lset = lookup ("COMSPEC", FALSE);
    Convert_Backslashes (lset->value);
    s_vstatus (lset, C_MSDOS);

/* Zap all files */

    closeall ();
    areanum = 1;

/* Get the current directory */

    Getcwd ();

/* Set up SHELL variable.  First check for a restricted shell.  Check the
 * restricted shell
 */

    Program_Name = name;
    if ((s = strrchr (name, '/')) == (char *)NULL)
	s = name;

    if ((s1 = strchr (s, '.')) != (char *)NULL)
	*s1 = 0;

    if (strcmp (s, "rsh") == 0)
	l_rflag = TRUE;

/* Has the program name got a .exe extension - Yes probably DOS 3+.  So
 * save it as the Shell name
 */

    lset = lookup (shell, TRUE);

    if (s1 != (char *)NULL)
    {
	if ((stricmp (s1 + 1, "exe") == 0) && (lset->value == null))
	    setval (lset, name);

	*s1 = '.';
    }

/* Default if necessary */

    if (lset->value == null)
	setval (lset, shellname);

    Convert_Backslashes (lset->value);
    s_vstatus (lset, EXPORT);

/* Check for restricted shell */

    if ((s = strrchr (lset->value, '/')) == (char *)NULL)
	s = lset->value;

    else
	s++;

    if (*s == 'r')
	l_rflag = TRUE;

/* Set up home directory */

    if ((lset = lookup (home, TRUE))->value == null)
      if ((init = lookup ("INIT", TRUE))->value != null)
	setval (lset, init->value);
      else
	setval (lset, C_dir->value);

    s_vstatus (lset, EXPORT);

/* Set up history file location */

    setval (lookup ("$", TRUE), putn (getpid ()));

    Load_G_VL ();
    path->status |= (EXPORT | PONLY);
    ifs->status  |= (EXPORT | PONLY);
    ps1->status  |= (EXPORT | PONLY);
    ps2->status  |= (EXPORT | PONLY);

    if (path->value == null)
	setval (path, search);

    if (ifs->value == null)
	setval (ifs, " \t\n");

    if (ps1->value == null)
	setval (ps1, "%e$ ");

    if (ps2->value == null)
	setval (ps2, "> ");

    return l_rflag;
}

/*
 * Mail Check processing.  Every $MAILCHECK seconds, we check either $MAIL
 * or $MAILPATH to see if any file has changed its modification time since
 * we last looked.  In $MAILCHECK, the files are separated by semi-colon (;).
 * If the filename contains a %, the string following the % is the message
 * to display if the file has changed.
 */

static void	Check_Mail ()
{
    int			delay = atoi (lookup ("MAILCHECK", FALSE)->value);
    Var_List		*mail = lookup ("MAIL", FALSE);
    Var_List		*mailp = lookup ("MAILPATH", FALSE);
    static time_t	last = 0L;
    time_t		current = time ((time_t *)NULL);
    struct stat		st;
    char		*cp, *sp, *ap;

/* Have we waited long enough */

    if ((current - last) < delay)
	return;

/* Yes - Check $MAILPATH.  If it is defined, process it.  Otherwise, use
 * $MAIL
 */

    if (mailp->value != null)
    {

/* Check MAILPATH */

	sp = mailp->value;

/* Look for the next separator */

	while ((cp = strchr (sp, ';')) != (char *)NULL)
	{
	    *cp = 0;

/* % in string ? */

	    if ((ap = strchr (ap, '%')) != (char *)NULL)
		*ap = 0;

/* Check the file name */

	    if ((stat (sp, &st) != -1) && (st.st_mtime > last))
	    {
		if (ap != (char *)NULL)
		{
		    S_puts (ap + 1);
		    S_putc (NL);
		}

		else
		    S_puts (ymail);
	    }

/* Restore the % */

	    if (ap != (char *)NULL)
		*ap = '%';

/* Restore the semi-colon and find the next one */

	    *cp = ';';
	    sp = cp + 1;
	}
    }

/* Just check MAIL */

    else if ((mail->value != null) && (stat (mail->value, &st) != -1) &&
	     (st.st_mtime > last))
	S_puts (ymail);

/* Save the last check time */

    last = current;
}

/*
 * Preprocess Argv to get handle of options in the format /x
 *
 * Some programs invoke the shell using / instead of - to mark the options.
 * We need to convert to -.  Also /c is a special case.  The rest of the
 * command line is the command to execute.  So, we get the command line
 * from the original buffer instead of argv array.
 */

static void	Pre_Process_Argv (argv)
char		**argv;
{
    extern      char far *_pgmptr;
    char	*ocl = _pgmptr;

    ocl = strchr(ocl, 0) + 1;
    ocl = strchr(ocl, 0) + 1;

/* Check for these options */

    while ((*++argv != (char *)NULL) && (strlen (*argv) == 2) &&
	   (**argv == '/'))
    {
       *strlwr (*argv) = '-';

/* Get the original information from the command line */

       if ((*argv)[1] == 'c')
       {
	    while ((*ocl != '/') && (*(ocl + 1) != 'c') && (*ocl) &&
		   (*ocl != '\r'))
		++ocl;

	    if (*ocl != '/')
		continue;

/* Find the start of the string */

	    ocl += 2;

	    while (isspace (*ocl) && (*ocl != '\r'))
		++ocl;

	    if (*ocl == '\r')
		continue;

/* Found the start.  Set up next parameter and ignore the rest */

	    if (*(argv + 1) == (char *)NULL)
		continue;

	    *(argv + 1) = ocl;
	    *(argv + 2) = (char *)NULL;

	    if ((ocl = strchr (ocl, '\r')) != (char *)NULL)
		*ocl = 0;

	    return;
       }
    }
}

/*
 * Convert backslashes to slashes for UNIX
 */

static void	Convert_Backslashes (sp)
char		*sp;
{
    while (*sp)
    {
	if (*sp == '\\')
	    *sp = '/';

	++sp;
    }
}

/* Load profiles onto I/O Stack */

static void	Load_profiles ()
{
    char	*name;
    int		f;
    char        prof[128], *env;

/* Set up home profile */

    name = Build_H_Filename ("profile");

    talking = TRUE;

    if ((f = O_for_execute (name, (char **)NULL, (int *)NULL)) >= 0)
    {
	PUSHIO (afile, remap (f), filechar);
    }

    DELETE (name);

    if ((f = O_for_execute ("/etc/profile", (char **)NULL, (int *)NULL)) >= 0)
    {
	PUSHIO (afile, remap (f), filechar);
    }

    if ( (env = getenv("INIT")) != NULL )
      if ((f = O_for_execute (strcat(strcpy(prof, env), "/profile"),
                              (char **)NULL, (int *)NULL)) >= 0)
      {
          PUSHIO (afile, remap (f), filechar);
      }

    if ( (env = getenv("HOME")) != NULL )
      if ((f = O_for_execute (strcat(strcpy(prof, env), "/profile"),
                              (char **)NULL, (int *)NULL)) >= 0)
      {
          PUSHIO (afile, remap (f), filechar);
      }
}

/*
 * Convert Unix PATH to MSDOS PATH
 */

static void	U2D_Path ()
{
    char	*cp = path->value;
    int		colon = 0;

/* If there is a semi-colon or a backslash, we assume this is a DOS format
 * path
 */

    if ((strchr (cp, ';') != (char *)NULL) ||
	(strchr (cp, '\\') != (char *)NULL))
	return;

/* Count the number of colons */

    while ((cp = strchr (cp, ':')) != (char *)NULL)
    {
	++colon;
	++cp;
    }

/* If there are no colons or there is one colon as the second character, it
 * is probably an MSDOS path
 */

    cp = path->value;
    if ((colon == 0) || ((colon == 1) && (*(cp + 1) == ':')))
	return;

/* Otherwise, convert all colons to semis */

    while ((cp = strchr (cp, ':')) != (char *)NULL)
	*(cp++) = ';';
}

/* Generate a file name from a directory and name.  Return null if an error
 * occurs or some malloced space containing the file name otherwise
 */

char	*Build_H_Filename (name)
char	*name;
{
    char	*dir = lookup (home, FALSE)->value;
    char	*cp;

/* Get some space */

    if ((cp = getcell (strlen (dir) + strlen (name) + 2)) == (char *)NULL)
	return null;

/* Addend the directory and a / if the directory does not end in one */

    strcpy (cp, dir);

    if (cp[strlen (cp) - 1] != '/')
	strcat (cp, "/");

/* Append the file name */

    return strcat (cp, name);
}
