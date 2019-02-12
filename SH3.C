/* MS-DOS SHELL - Parse Tree Executor
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
 *    $Header: sh3.c 1.22 90/06/21 11:10:47 MS_user Exp $
 *
 *    $Log:	sh3.c $
 * Revision 1.22  90/06/21  11:10:47  MS_user
 * Ensure Areanum is set correctly for memory areas
 *
 * Revision 1.21  90/06/08  14:53:58  MS_user
 * Finally, we've fixed Gen_Full_Path_Name
 *
 * Revision 1.20  90/05/31  17:44:11  MS_user
 * Ensure Swap file is saved at level 0
 *
 * Revision 1.19  90/05/31  09:49:12  MS_user
 * Implement partial write when swapping to disk
 * Add some signal lockouts to prevent corruption
 * Fix a bug in Gen_Full_Path for c:program
 *
 * Revision 1.18  90/05/15  21:09:39  MS_user
 * Change Restore_Dir parameter to take directory name
 *
 * Revision 1.17  90/05/11  18:47:40  MS_user
 * Get switchchar for command.com
 *
 * Revision 1.16  90/05/09  18:03:08  MS_user
 * Fix bug in Gen_Full_Path with programs in root directory
 *
 * Revision 1.15  90/04/30  19:50:11  MS_user
 * Stop search path if second character of name is colon
 *
 * Revision 1.14  90/04/26  17:27:52  MS_user
 * Fix problem with Picnix Utilities - full path name of executable required
 *
 * Revision 1.13  90/04/25  22:34:39  MS_user
 * Fix case in TELIF where then and else parts are not defined
 * Fix rsh check for execution path declared
 *
 * Revision 1.12  90/04/11  19:49:35  MS_user
 * Another problem with new command line processing
 *
 * Revision 1.11  90/04/11  12:55:49  MS_user
 * Change command line to look exactly like COMMAND.COM
 *
 * Revision 1.10  90/03/27  20:33:10  MS_user
 * Clear extended file name on interrupt
 *
 * Revision 1.9  90/03/27  20:09:21  MS_user
 * Clear_Extended_File required in SH1 for interrupt clean up
 *
 * Revision 1.8  90/03/26  20:57:14  MS_user
 * Change I/O restore so that "exec >filename" works
 *
 * Revision 1.7  90/03/22  13:47:24  MS_user
 * MSDOS does not handle /dev/ files after find_first correctly
 *
 * Revision 1.6  90/03/14  16:44:49  MS_user
 * Add quoting of arguments with white space on command line
 * Add IOTHERE processing to iosetup
 *
 * Revision 1.5  90/03/06  15:10:28  MS_user
 * Get doeval, dodot and runtrap working correctly in run so that a sub-shell
 * is not created and environment variables can be changed.
 *
 * Revision 1.4  90/03/05  13:50:04  MS_user
 * Add XMS driver
 * Fix bug with extended line file not being deleted
 * Get run to support eval and dot functionality correctly
 * Get trap processing to work
 * Add COMMAND.COM support for .bat files
 *
 * Revision 1.3  90/02/22  16:38:20  MS_user
 * Add XMS support
 *
 * Revision 1.2  90/02/14  04:47:06  MS_user
 * Clean up Interrupt 23 and 0 processing, change EMS version error message
 *
 * Revision 1.1  90/01/25  13:41:24  MS_user
 * Initial revision
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <process.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>
#include <dir.h>
#include <ctype.h>

#include "sh.h"

#define INCL_NOPM
#define INCL_DOS
#include <os2.h>

/* static Function and string declarations */

static int	forkexec (C_Op *, int, int, int, char **);
static bool	iosetup (IO_Actions *, int, int);
static C_Op	**find1case (C_Op *, char *);
static C_Op	*findcase (C_Op *, char *);
static void	echo (char **);
static int	rexecve (char *, char **, char **, bool);
static int	Execute_program (char *, char **, char **, bool);
static int	S_spawnve (char *, char **, char **);
static int	build_command_line (char *, char **, char **);
static int	setstatus (int);
static bool	Check_for_bat_file (char *);
static size_t	white_space_len (char *, bool *);
static char	*Gen_Full_Path_Name (char *);

static int      spawnmode = P_WAIT;

static char	*AE2big = "arg/env list too big";
			/* Extended Command line processing file name	*/
static char		*Extend_file = (char *)NULL;

/*
 * execute tree recursively
 */

int		execute (t, pin, pout, act)
register C_Op	*t;
int		pin;
int		pout;
int		act;
{
    register C_Op	*t1;
    int			i, localpipe;
    char		*cp, **wp;
    char		**Local_Tword;
    Var_List		*vp;
    Break_C		bc;
    Break_C		*S_RList;		/* Save link pointers	*/
    Break_C		*S_BList;
    Break_C		*S_SList;
    int			Local_depth;		/* Save local values	*/
    int			Local_areanum;
    int			rv = 0;
    int                 readp, writep;

/* End of tree ? */

    if (t == (C_Op *)NULL)
	return 0;

/* Save original and Increment execute function recursive level */

    Local_depth = Execute_stack_depth++;

/* Save original and increment area number */

    Local_areanum = areanum++;

/* Save the exit points from SubShells, functions and for/whiles */

    S_RList = Return_List;
    S_BList = Break_List;
    S_SList = SShell_List;

/* Expand any arguments */

    wp = (Local_Tword = t->words) != (char **)NULL
	 ? eval (Local_Tword, (t->type == TCOM) ? DOALL : DOALL & ~DOKEY)
	 : (char **)NULL;

/* Switch on tree node type */

    switch (t->type)
    {
	case TFUNC:			/* name () { list; }	*/
	    Save_Function (t, FALSE);
	    break;

/* In the case of a () command string, we need to save and restore the
 * current environment, directory and traps (can't think of anything else).
 * For any other, we just restore the current directory.  Also, we don't
 * want changes in the Variable list header saved for SubShells, because
 * we are effectively back at execute depth zero.
 */
	case TPAREN:			/* ()			*/
	    if ((rv = Create_NG_VL ()) == -1)
		break;

	    if (setjmp (bc.brkpt) == 0)
	    {
		Return_List = (Break_C *)NULL;
		Break_List  = (Break_C *)NULL;
		bc.nextlev  = SShell_List;
		SShell_List = &bc;
		/* rv = forkexec (t, pin, pout, act, wp); wrong! */
		rv = execute (t->left, pin, pout, act);
	    }

/* Restore the original environment */

	    Return_List	= S_RList;
	    Break_List	= S_BList;
	    SShell_List	= S_SList;
	    Restore_Environment (rv, Local_depth);
	    break;

/* After a normal command, we need to restore the original directory.  Note
 * that a cd will have updated the variable $~, so no problem
 */

	case TCOM:			/* A command process	*/
	    rv = forkexec (t, pin, pout, act, wp);
	    Restore_Dir (C_dir->value);
	    break;

	case TPIPE:			/* Pipe processing		*/

/* Create pipe, execute command, reset pipe, execute the other side, close
 * the pipe and fini
 */
	 /* old DOS version:
            if ((rv = openpipe ()) < 0)
		break;
	    localpipe = remap (rv);
	    execute (t->left, pin, localpipe, 0);
	    lseek (localpipe, 0L, SEEK_SET);
	    rv = execute (t->right, localpipe, pout, 0);
	    closepipe (localpipe);
         */

            if (DosMakePipe((PHFILE) &readp, (PHFILE) &writep, 0))
                break;
	    readp = remap (readp);
	    writep = remap (writep);
            DosSetFHandState(readp, OPEN_FLAGS_NOINHERIT);
	    DosSetFHandState(writep, OPEN_FLAGS_NOINHERIT);

            if ( spawnmode == P_WAIT )
            {
              spawnmode = P_NOWAITO;
	      execute (t->left, pin, writep, 0);
              close(writep);
              spawnmode = P_NOWAIT;
	      rv = execute (t->right, readp, pout, 0);
              close(readp);
              spawnmode = P_WAIT;
              cwait(&rv, rv, WAIT_GRANDCHILD);
            }
            else
            {
              rv = spawnmode;
              spawnmode = P_NOWAITO;
	      execute (t->left, pin, writep, 0);
              close(writep);
              spawnmode = rv;
	      rv = execute (t->right, readp, pout, 0);
              close(readp);
            }

	    break;

	case TLIST:			/* Entries in a for statement	*/
	    execute (t->left, pin, pout, 0);
	    rv = execute (t->right, pin, pout, 0);
	    break;

	case TASYNC:
	 /* rv = -1;
	    S_puts ("sh: Async commands not supported\n");
	    setstatus (rv);
         */
            spawnmode = P_NOWAIT;
	    rv = execute (t->left, pin, pout, 0);
            spawnmode = P_WAIT;
            S_puts("[");
            S_puts(putn(rv));
            S_puts("]\n");
            setval (lookup ("!", TRUE), putn (rv));
	    break;

	case TOR:			/* || and &&			*/
	case TAND:
	    rv = execute (t->left, pin, pout, 0);

	    if (((t1 = t->right) != (C_Op *)NULL) &&
		((rv == 0) == (t->type == TAND)))
		rv = execute (t1, pin, pout, 0);

	    break;

	case TFOR:			/* First part of a for statement*/

/* for x do...done - use the parameter values.  Need to know how many as
 * it is not a NULL terminated array
 */

	    if (wp == (char **)NULL)
	    {
		wp = dolv + 1;

		if ((i = dolc) < 0)
		    i = 0;
	    }

/* for x in y do...done - find the start of the variables and use them all */

	    else
	    {
		i = -1;
		while (*wp++ != (char *)NULL)
		    ;
	    }

/* Create the loop variable. */

	    vp = lookup (t->str, TRUE);

/* Set up a long jump return point before executing the for function so that
 * the continue statement is executed, ie we reprocessor the for condition.
 */

	    while (rv = setjmp (bc.brkpt))
	    {

/* Restore the current stack level and clear out any I/O */

		Restore_Environment (0, Local_depth + 1);
		Return_List = S_RList;
		SShell_List = S_SList;

/* If this is a break - clear the variable and terminate the while loop and
 * switch statement
 */

		if (rv == BC_BREAK)
		    break;
	    }

	    if (rv == BC_BREAK)
		break;

/* Process the next entry - Add to the break/continue chain */

	    bc.nextlev = Break_List;
	    Break_List = &bc;

/* Execute the command tree */

	    for (t1 = t->left; i-- && *wp != NULL;)
	    {
		setval (vp, *wp++);
		rv = execute (t1, pin, pout, 0);
	    }

/* Remove this tree from the break list */

	    Break_List = S_BList;
	    break;

/* While and Until function.  Similar to the For function.  Set up a
 * long jump return point before executing the while function so that
 * the continue statement is executed OK.
 */

	case TWHILE:			/* WHILE and UNTIL functions	*/
	case TUNTIL:
	    while (rv = setjmp (bc.brkpt))
	    {

/* Restore the current stack level and clear out any I/O */

		Restore_Environment (0, Local_depth + 1);
		Return_List = S_RList;
		SShell_List = S_SList;

/* If this is a break, terminate the while and switch statements */

		if (rv == BC_BREAK)
		    break;
	    }

	    if (rv == BC_BREAK)
		break;

/* Set up links */

	    bc.nextlev = Break_List;
	    Break_List = &bc;
	    t1 = t->left;

	    while ((execute(t1, pin, pout, 0) == 0) == (t->type == TWHILE))
		rv = execute (t->right, pin, pout, 0);

	    Break_List = S_BList;
	    break;

	case TIF:			/* IF and ELSE IF functions	*/
	case TELIF:
	    if (t->right != (C_Op *)NULL)
		rv = !execute (t->left, pin, pout, 0)
	   		 ? execute (t->right->left, pin, pout, 0)
			 : execute (t->right->right, pin, pout, 0);
	    break;

	case TCASE:			/* CASE function		*/
	    if ((cp = evalstr (t->str, DOSUB | DOTRIM)) == (char *)NULL)
		cp = null;

	    if ((t1 = findcase (t->left, cp)) != (C_Op *)NULL)
		rv = execute (t1, pin, pout, 0);

	    break;

	case TBRACE:			/* {} statement			*/
	    if ((rv >= 0) && ((t1 = t->left) != (C_Op *)NULL))
		rv = execute (t1, pin, pout, 0);

	    break;
    }

/* Processing Completed - Restore environment */

    t->words		= Local_Tword;
    Execute_stack_depth = Local_depth;

/* Remove unwanted malloced space */

    freehere (areanum);
    freearea (areanum);

    areanum		= Local_areanum;

/* Check for traps */

    if ((i = trapset) != 0)
    {
	trapset = 0;
	runtrap (i);
    }

/* Check for interrupts */

    if (Interactive () && SW_intr)
    {
	closeall ();
	fail ();
    }

    return rv;
}

/*
 * Restore the original directory
 */

void	Restore_Dir (path)
char	*path;
{
    unsigned int	dummy;

    DosSelectDisk(tolower(*path) - 'a' + 1);

    if (chdir (&path[2]) != 0)
    {
	S_puts ("Warning: current directory reset to /\n");
	chdir ("/");
	Getcwd ();
    }
}

/*
 * Ok - execute the program, resetting any I/O required
 */

static int	forkexec (t, pin, pout, act, wp)
register C_Op	*t;
int		pin;
int		pout;
int		act;
char		**wp;
{
    int		rv = -1;
    int		(*shcom)(C_Op *) = (int (*)())NULL;
    char	*cp;
    IO_Actions	**iopp;
    int		resetsig = 0;
    void	(*sig_int)();
    char	**owp = wp;
    bool	spawn = FALSE;
    Fun_Ops	*fop;
    char        *cmdcom = NULL;

    if (t->type == TCOM)
    {
	while ((cp = *wp++) != (char *)NULL)
	    ;

	cp = *wp;

/* strip all initial assignments not correct wrt PATH=yyy command  etc */

	if (FL_TEST ('x'))
	    echo (cp != (char *)NULL ? wp : owp);

/* Is it only an assignement? */

	if ((cp == (char *)NULL) && (t->ioact == (IO_Actions **)NULL))
	{
	    while (((cp = *owp++) != (char *)NULL) && assign (cp, COPYV))
		;

	    return setstatus (0);
	}

/* Check for built in commands */

	else
          if (cp != (char *)NULL)
	    if ( (shcom = inbuilt (cp)) == NULL )
              cmdcom = cmd_internal(cp);
    }

/* Unix fork simulation? */

    t->words = wp;
    if (shcom == NULL && (act & FEXEC) == 0)
    {
	spawn = TRUE;

	if (Interactive ())
	{
#ifdef SIGQUIT
	    signal (SIGQUIT, SIG_IGN);
#endif
	    sig_int = signal (SIGINT, SIG_IGN);
	    resetsig = 1;
	}
    }

/* Set any variables */

    while (((cp = *owp++) != (char *)NULL) && assign (cp, COPYV))
    {
	if (shcom == NULL)
	    s_vstatus (lookup (cp, TRUE), EXPORT);
    }

/* We cannot close the pipe, because once the exec/spawn has taken place
 * the processing of the pipe is not yet complete.
 */

    if (pin != NOPIPE)
    {
	S_dup2 (pin, STDIN_FILENO);
	lseek (STDIN_FILENO, 0L, SEEK_SET);
    }

    if (pout != NOPIPE)
    {
	S_dup2 (pout, STDOUT_FILENO);
	lseek (STDOUT_FILENO, 0L, SEEK_END);
    }

/* Set up any other IO required */

    if ((iopp = t->ioact) != (IO_Actions **)NULL)
    {
	while (*iopp != (IO_Actions *)NULL)
	{
	    if (iosetup (*iopp++, pin, pout))
		return rv;
	}
    }

    if (shcom)
        return restore_std (setstatus ((*shcom)(t)), TRUE);

/* All fids above 10 are autoclosed in the exec file because we have used
 * the O_NOINHERIT flag.  Note I patched open.obj to pass this flag to the
 * open function.
 */

    if (resetsig)
    {
#ifdef SIGQUIT
	signal (SIGQUIT, SIG_IGN);
#endif
	signal (SIGINT, sig_int);
    }

    if (t->type == TPAREN)
	return restore_std (execute (t->left, NOPIPE, NOPIPE, FEXEC), TRUE);

/* Are we just changing the I/O re-direction for the shell ? */

    if (wp[0] == NULL)
    {
	if (spawn)
	    restore_std (0, TRUE);

	return 0;
    }

/* No - Check for a function the program.  At this point, we need to put
 * in some processing for return.
 */

    if ((fop = Fun_Search (wp[0])) != (Fun_Ops *)NULL)
    {
	char			**s_dolv = dolv;
	int			s_dolc   = dolc;
	Break_C			*s_RList = Return_List;
	Break_C			*s_BList = Break_List;
	Break_C			*s_SList = SShell_List;
	int			LS_depth = Execute_stack_depth;
	Break_C			bc;

/* Set up $0..$n for the function */

	dolv = wp;
	for (dolc = 0; dolv[dolc] != (char *)NULL; ++dolc);
	setval (lookup ("#", TRUE), putn (dolc));

	if (setjmp (bc.brkpt) == 0)
	{
	    Break_List  = (Break_C *)NULL;
	    bc.nextlev  = Return_List;
	    Return_List = &bc;
	    rv = execute (fop->tree->left, NOPIPE, NOPIPE, FEXEC);
	}

/* A return has been executed - Unlike, while and for, we just need to
 * restore the local execute stack level and the return will restore
 * the correct I/O.
 */

	else
	    rv = getn (lookup ("?", FALSE)->value);

/* Restore the old $0, and previous return address */

	Break_List  = s_BList;
	Return_List = s_RList;
	SShell_List = s_SList;
	dolv	    = s_dolv;
	dolc	    = s_dolc;
	Restore_Environment (rv, LS_depth);
	setval (lookup ("#", TRUE), putn (dolc));
	return rv;
    }

/* Check for another drive or directory in the restricted shell */

    if (anys (":/\\", wp[0]) && check_rsh (wp[0]))
	return restore_std (-1, TRUE);

/* Ok - execute the program */

    if (cmdcom)
      return restore_std (rexecve (NULL, wp, makenv (), spawn), TRUE);
    else
      return restore_std (rexecve (wp[0], wp, makenv (), spawn), TRUE);
}

/*
 * Restore Local Environment
 */

void	Restore_Environment (retval, stack)
int	retval;
int	stack;
{
    Execute_stack_depth = stack;
    Delete_G_VL ();
    Restore_Dir (C_dir->value);
    restore_std (setstatus (retval), TRUE);
}

/*
 * Set up I/O redirection.  0< 1> are ignored as required within pipelines.
 */

static bool		iosetup (iop, pipein, pipeout)
register IO_Actions	*iop;
int			pipein;
int			pipeout;
{
    register int	u;
    char		*cp, *msg;

    if (iop->io_unit == IODEFAULT)	/* take default */
	iop->io_unit = (iop->io_flag & (IOREAD | IOHERE)) ? STDIN_FILENO
							  : STDOUT_FILENO;

/* Check for pipes */

    if ((pipein != NOPIPE) && (iop->io_unit == STDIN_FILENO))
	return FALSE;

    if ((pipeout != NOPIPE) && (iop->io_unit == STDOUT_FILENO))
	return FALSE;

    msg = (iop->io_flag & (IOREAD | IOHERE)) ? "open" : "create";

    if ((iop->io_flag & IOHERE) == 0)
    {
	if ((cp = evalstr (iop->io_name, DOSUB | DOTRIM)) == (char *)NULL)
	    return TRUE;
    }

    if (iop->io_flag & IODUP)
    {
	if ((cp[1]) || !isdigit (*cp) && *cp != '-')
	{
	    print_error ("%s: illegal >& argument\n", cp);
	    return TRUE;
	}

	if (*cp == '-')
	    iop->io_flag = IOCLOSE;

	iop->io_flag &= ~(IOREAD | IOWRITE);
    }

/*
 * When writing to /dev/???, we have to cheat because MSDOS appears to
 * have a problem with /dev/ files after find_first/find_next.
 */

    if (((iop->io_flag & ~(IOXHERE | IOTHERE)) == IOWRITE) &&
	(strnicmp (cp, "/dev/", 5) == 0))
	iop->io_flag |= IOCAT;

/* Open the file in the appropriate mode */

    switch (iop->io_flag & ~(IOXHERE | IOTHERE))
    {
	case IOREAD:				/* <			*/
	    u = S_open (FALSE, cp, O_RDONLY);
	    break;

	case IOHERE:				/* <<			*/
	    u = herein (iop->io_name, iop->io_flag & IOXHERE);
	    cp = "here file";
	    break;

	case IOWRITE | IOCAT:			/* >>			*/
	    if (check_rsh (cp))
		return TRUE;

	    if ((u = S_open (FALSE, cp, O_WRONLY | O_TEXT)) >= 0)
	    {
		lseek (u, 0L, SEEK_END);
		break;
	    }

	case IOWRITE:				/* >			*/
	    if (check_rsh (cp))
		return TRUE;

	    u = S_open (FALSE, cp, O_CMASK, 0666);
	    break;

	case IODUP:				/* >&			*/
	    if (check_rsh (cp))
		return TRUE;

	    u = S_dup2 (*cp - '0', iop->io_unit);
	    break;

	case IOCLOSE:				/* >-			*/
	    if ((iop->io_unit >= STDIN_FILENO) &&
		(iop->io_unit <= STDERR_FILENO))
		S_dup2 (-1, iop->io_unit);

	    S_close (iop->io_unit, TRUE);
	    return FALSE;
    }

    if (u < 0)
    {
	print_warn ("%s: cannot %s\n", cp, msg);
	return TRUE;
    }

    else if (u != iop->io_unit)
    {
	S_dup2 (u, iop->io_unit);
	S_close (u, TRUE);
    }

    return FALSE;
}

/*
 * -x flag - echo command to be executed
 */

static void	echo (wp)
register char	**wp;
{
    register int	i;

    S_putc ('+');

    for (i = 0; wp[i] != (char *)NULL; i++)
    {
	S_putc (SP);
	S_puts (wp[i]);
    }

    S_putc (NL);
}

static C_Op	**find1case (t, w)
C_Op		*t;
char		*w;
{
    register C_Op	*t1;
    C_Op		**tp;
    register char	**wp, *cp;

    if (t == (C_Op *)NULL)
	return (C_Op **)NULL;

    if (t->type == TLIST)
    {
	if ((tp = find1case (t->left, w)) != (C_Op *)NULL)
	    return tp;

	t1 = t->right;	/* TPAT */
    }

    else
	t1 = t;

    for (wp = t1->words; *wp != (char *)NULL;)
    {
	if ((cp = evalstr (*(wp++), DOSUB)) && gmatch (w, cp, FALSE))
	    return &t1->left;
    }

    return (C_Op **)NULL;
}

static C_Op	*findcase (t, w)
C_Op		*t;
char		*w;
{
    register C_Op **tp;

    return ((tp = find1case (t, w)) != (C_Op **)NULL) ? *tp : (C_Op *)NULL;
}

/*
 * Set up the status on exit from a command
 */

static int	setstatus (s)
register int	s;
{
    exstat = s;
    setval (lookup ("?", TRUE), putn (s));
    return s;
}

/*
 * PATH-searching interface to execve.
 */

static int	rexecve (c, v, envp, d_flag)
char		*c;
char		**v;
char		**envp;
bool		d_flag;
{
    register char	*sp;
    int			res;			/* Result		*/
    char		*em;			/* Exit error message	*/
    bool		eloop;			/* Re-try flag		*/
    char		**new_argv;
    int			batfile;		/* .bat file flag	*/
    int			argc = 0;		/* Original # of argcs	*/
    char		*params;		/* Script parameters	*/
    int			nargc = 0;		/* # script args	*/
    char		*p_name;		/* Program name		*/
    int			i;

/* Count the number of arguments to the program in case of shell script or
 * bat file
 */

    while (v[argc++] != (char *)NULL);

    ++argc;				/* Including the null		*/

/* If the environment is null - It is too big - error */

    if (envp == (char **)NULL)
	em = AE2big;

    else if ((p_name = getcell (FFNAME_MAX)) == (char *)NULL)
	em = strerror (ENOMEM);

    else if ( c == NULL )
    {
/* cmd.exe interal command */

      if ((new_argv = (char **) getcell (sizeof(char *) * (argc + 2)))
 	      == NULL)
  	em = strerror (ENOMEM);
      else
      {
        memcpy (&new_argv[2], &v[0], sizeof(char *) * argc);
        new_argv[0] = lookup ("COMSPEC", FALSE)->value;
        new_argv[1] = "/c";
        res = rexecve (new_argv[0], new_argv, envp, d_flag);
        DELETE (new_argv);
	return res;
      }
    }

    else
    {
/* Start off on the search path for the executable file */

	sp = (any ('/', c) || (*(c + 1) == ':')) ? null : path->value;

	do
	{
	    sp = path_append (sp, c, p_name);

	    if ((res = Execute_program (p_name, v, envp, d_flag)) != -1)
		return res;

	    eloop = TRUE;

	    switch (errno)
	    {

/* No entry for the file - if the file exists, execute it as a shell
 * script
 */
		case ENOENT:
		    if ((res = O_for_execute (p_name, &params, &nargc)) >= 0)
		    {
			batfile = 1;
			S_close (res, TRUE);
		    }

		    else if (!Check_for_bat_file (p_name))
		    {
			em = "not found";
			eloop = FALSE;
			break;
		    }

		    else
		    {
			Convert_Slashes (p_name);
			batfile = 0;
			nargc = 0;
		    }

/* Ok - either a shell script or a bat file (batfile = 0) */

		    nargc = (nargc < 2) ? 0 : nargc - 1;
		    if ((new_argv = (char **)getcell (sizeof (char *) *
		    					(argc + nargc + 2)))
			      == (char **)NULL)
		    {
			em = strerror (ENOMEM);
			break;
		    }

		    memcpy (&new_argv[2 + nargc], &v[0], sizeof(char *) * argc);

/* If BAT file, use command.com else use sh */

		    if (!batfile)
		    {
			new_argv[0] = lookup ("COMSPEC", FALSE)->value;
			new_argv[1] = "/c";
		    }

/* Stick in the pre-fix arguments */

		    else if (nargc)
		    {
			i = 1;
			em = params;

			while (*em)
			{
			    while (isspace (*em))
				*(em++) = 0;

			    if (*em)
				new_argv[i++] = em;

			    while (!isspace (*em) && *em)
				++em;
			}
		    }

		    else if (params != null)
			new_argv[1] = params;

		    else
			new_argv[1] = lookup (shell, FALSE)->value;

		    new_argv[2 + nargc] = p_name;

		    res = rexecve (new_argv[batfile], &new_argv[batfile],
				    envp, d_flag);
/* Release allocated space */

		    DELETE (new_argv);

		    if (params != null)
			DELETE (params);

		    if (res != -1)
			return res;
/* No - shell */

		    em = "no Shell";
		    eloop = FALSE;
		    break;

		case ENOEXEC:
		    em = "program corrupt";
		    break;

		case ENOMEM:
		    em = "program too big";
		    break;

		case E2BIG:
		    em = AE2big;
		    break;

		default:
		    em = "cannot execute";
		    eloop = FALSE;
		    break;
	    }
	} while ((sp != (char *)NULL) && !eloop);
    }

    print_warn ("%s: %s\n", c, em);

    if (!d_flag)
	exit (-1);

    return -1;
}

/* Check to see if this is a bat file */

static bool	Check_for_bat_file (name)
char		*name;
{
    char	*local_path;
    char	*cp;
    bool	res;

    if ((local_path = getcell (strlen (name) + 5)) == (char *)NULL)
	return FALSE;

/* Work on a copy of the path */

    if ((cp = strrchr (strcpy (local_path, name), '/')) == (char *)NULL)
	cp = local_path;

    else
	++cp;

    if ((cp = strrchr (cp, '.')) == (char *)NULL)
	strcat (local_path, BATCHEXT);

    else if (stricmp (cp, BATCHEXT) != 0)
    {
	DELETE (local_path);
	return FALSE;
    }

    res = (access (local_path, X_OK) == 0) ? TRUE : FALSE;
    DELETE (local_path);
    return res;
}

/*
 * Run the command produced by generator `f' applied to stream `arg'.
 */

int		run (argp, f, f_loop)
IO_Args		*argp;
int		(*f)(IO_State *);
bool		f_loop;
{
    Word_B		*swdlist = wdlist;
    Word_B		*siolist = iolist;
    jmp_buf		ev, rt;
    int			*ofail = failpt;
    int			rv = -1;
    Break_C		*S_RList = Return_List;	/* Save loval links	*/
    Break_C		*S_BList = Break_List;
    int			LS_depth = Execute_stack_depth;
    int			sjr;
    C_Op		*outtree;
    int			s_execflg = execflg;

/* Create a new save area */

    areanum++;

/* Execute the command */

    if (newenv (setjmp (errpt = ev)) == FALSE)
    {
	Return_List = (Break_C *)NULL;
	Break_List  = (Break_C *)NULL;
	wdlist      = (Word_B *)NULL;
	iolist      = (Word_B *)NULL;

	pushio (argp, f);
	e.iobase  = e.iop;
	e.eof_p   = (bool)!f_loop;	/* Set EOF processing		*/
	/*+*/SW_intr   = 0;
	multiline = 0;
	inparse   = 0;
	execflg   = (!f_loop) ? 1 : execflg;

/* Read Input (if f_loop is not set, we are processing a . file command)
 * either for one line or until end of file.
 */
	do
	{
	    yynerrs = 0;

	    if (((sjr = setjmp (failpt = rt)) == 0) &&
		((outtree = yyparse ()) != (C_Op *)NULL))
		rv = execute (outtree, NOPIPE, NOPIPE, 0);

/* Fail or no loop - zap any files if necessary */

	    else if (sjr || f_loop)
	    {
		Clear_Extended_File ();
		break;
	    }

	} while (!f_loop);

	quitenv ();
    }

/* Restore the environment */

    Return_List = S_RList;
    Break_List = S_BList;
    execflg = s_execflg;
    wdlist = swdlist;
    iolist = siolist;
    failpt = ofail;

    Restore_Environment (rv, LS_depth);

    freearea (areanum--);
    return rv;
}

/* Exec or spawn the program ? */

static int	Execute_program (path, parms, envp, d_flag)
char		*path;
char		**parms;
char		**envp;
bool		d_flag;
{
    return setstatus ((!d_flag) ? execve (path, parms, envp)
				: S_spawnve (path, parms, envp));
}

/* Set up to spawn a process */

static int	S_spawnve (path, parms, envp)
char		*path;
char		**parms;
char		**envp;
{
    char		*ep, *ep1, path_line[255];
    int			res, serrno;

/* Check to see if the file exists */

    strcpy (path_line, path);

    if ((ep = strrchr (path_line, '/')) == (char *)NULL)
	ep = path_line;

/* If no dot in name - check for .exe and .com files */

    if ((ep1 = strchr (ep, '.')) == (char *)NULL)
    {
	ep1 = ep + strlen (ep);
	strcpy (ep1, ".exe");

	if ((res = access (path_line, F_OK)) != 0)
	{
	    strcpy (ep1, ".com");
	    res = access (path_line, F_OK);
	}

	if (res != 0)
	    return -1;
    }

    else if ((stricmp (ep1, ".exe") != 0) && (stricmp (ep1, ".com") != 0))
    {
	if (access (path_line, F_OK) == 0)
	    errno = ENOEXEC;

	return -1;
    }

    else if (access (path_line, F_OK) != 0)
	return -1;

/* Process the command line. */

    return build_command_line (path_line, parms, envp);
}

/* Set up command line.  If the EXTENDED_LINE variable is set, we create
 * a temporary file, write the argument list (one entry per line) to the
 * this file and set the command line to @<filename>.  If NOSWAPPING, we
 * execute the program because I have to modify the argument line
 */

int	build_command_line (path, argv, envp)
char	*path;
char	**argv;
char	**envp;
{
    char		**pl = argv;
    char		*fname;
    int			res, fd;
    char		*pname;
    FILE		*fp;
    char		nbuffer[NAME_MAX + 2];
    bool		found;
    char		*ep;
    char		*new_args[3];
    char                cmd_line[CMD_LINE_MAX];
    void 		(*save_signal)(int);

/* Translate process name to MSDOS format */

    if ((argv[0] = Gen_Full_Path_Name (path)) == (char *)NULL)
	return -1;

/* Find the start of the program name */

    pname = ((pname = strrchr (path, '\\')) == (char *)NULL) ? path : pname + 1;

/* Extended command line processing */

    Extend_file == (char *)NULL;		/* Set no file		*/

    if ((*(pl++) != (char *)NULL) &&
	((fname = lookup ("EXTENDED_LINE", FALSE)->value) != null) &&
	((fp = fopen (fname, "rt")) != (FILE *)NULL))
    {

/* Loop through the file look for the current program */

	found = FALSE;

	while (fgets (nbuffer, NAME_MAX + 1, fp) != (char *)NULL)
	{
	    if ((ep = strchr (nbuffer, '\n')) != (char *)NULL)
		*ep = 0;

	    if (stricmp (nbuffer, pname) == 0)
	    {
		found = TRUE;
		break;
	    }
	}

	fclose (fp);

/* Check parameters don't contain a re-direction parameter */

	if (found)
	{
	    char	**pl1 = pl;

	    while (*pl1 != (char *)NULL)
	    {
		if (**(pl1++) == '@')
		{
		    found = FALSE;
		    break;
		}
	    }
	}

/* If we find it - create a temporary file and write the stuff */

	if ((found) &&
	    ((fd = S_open (FALSE, Extend_file = g_tempname (), O_CMASK,
			   0600)) >= 0))
	{
	    Extend_file = strsave (Extend_file, 0);

/* Copy to end of list */

	    while (*pl != (char *)NULL)
	    {
		if (((res = strlen (*pl)) && (write (fd, *pl, res) != res)) ||
		    (write (fd, "\n", 1) != 1))
		{
		    close (fd);
		    Clear_Extended_File ();
		    errno = ENOSPC;
		    return -1;
		}

		++pl;
	    }

/* Completed write OK */

	    close (fd);

/* Set up cmd_line[1] to contain the filename */

	    memset (cmd_line, 0, CMD_LINE_MAX);
	    cmd_line[1] = ' ';
	    cmd_line[2] = '@';
	    strcpy (&cmd_line[3], Extend_file);
	    cmd_line[0] = (char)(strlen (Extend_file) + 2);

/* Correctly terminate cmd_line in no swap mode */

	    cmd_line[cmd_line[0] + 2] = '\r';

/* If the name in the file is in upper case - use \ for separators */

	    if (isupper (*nbuffer))
		Convert_Slashes (&cmd_line[2]);

/* OK we are ready to execute */

	    new_args[0] = *argv;
	    new_args[1] = &cmd_line[1];
	    new_args[2] = (char *)NULL;

            save_signal = signal (SIGINT, SIG_DFL);
	    res = spawnve (spawnmode, path, new_args, envp);
            signal (SIGINT, save_signal);
            return res;
	}
    }

/* Check length of Parameter list */

    res = 0;
    cmd_line[0] = 0;
    cmd_line[1] = '\r';

/* Skip the first parameter and get the length of the rest */

    if (*argv != (char *)NULL)
    {
	*(ep = cmd_line + 1) = 0;

	while (*pl != (char *)NULL)
	{
	    res += white_space_len (*pl, &found);

	    if (res >= CMD_LINE_MAX)
  	    {
  		errno = E2BIG;
  		return -1;
  	    }

	    if (found)
	    	strcat (strcat (strcat (ep, " \""), *(pl++)), "\"");

	    else
		strcat (strcat (ep, " "), *(pl++));
  	}

	cmd_line[res + 1] = '\r';
    }

/* Terminate the line and insert the line length */

    cmd_line[0] = (char)res;

/* Just execute it */

    save_signal = signal (SIGINT, SIG_DFL);
    res = spawnve (spawnmode, path, argv, envp);
    signal (SIGINT, save_signal);
    return res;
}

/* Check string for white space */

static size_t	white_space_len (s, wsf)
char		*s;
bool		*wsf;
{
    char	*os = s;

    *wsf = FALSE;

    while (*s)
    {
        if (isspace (*s))
	    *wsf = TRUE;

	++s;
    }

    return (size_t)(s - os) + (*wsf ? 3 : 1);
}

/* Clear Extended command line file */

void	Clear_Extended_File ()
{
    if (Extend_file != (char *)NULL)
    {
	unlink (Extend_file);
	DELETE (Extend_file);
    }

    Extend_file = (char *)NULL;
}

/* Convert the executable path to the full path name */

static char	*Gen_Full_Path_Name (path)
char		*path;
{
    char		cpath[PATH_MAX + 4];
    char		npath[PATH_MAX + NAME_MAX + 4];
    char		n1path[PATH_MAX + 4];
    char		*p;
    unsigned int	dummy;

    Convert_Slashes (path);
    strupr (path);

/* Get the current path */

    getcwd (cpath, PATH_MAX + 3);
    strcpy (npath, cpath);

/* In current directory ? */

    if ((p = strrchr (path, '\\')) == (char *)NULL)
    {
	 p = path;

/* Check for a:program case */

	 if (*(p + 1) == ':')
	 {
	    p += 2;

/* Switch drives and get the path of the other drive */

	    DosSelectDisk(tolower (*path) - 'a' + 1);
	    getcwd (npath, PATH_MAX + 3);
	    DosSelectDisk(tolower (*cpath) - 'a' + 1);
	 }
    }

/* In root directory */

    else if ((p - path) == 0)
    {
	++p;
	strcpy (npath, "x:\\");
	*npath = *cpath;
    }

    else if (((p - path) == 2) && (*(path + 1) == ':'))
    {
	++p;
	strcpy (npath, "x:\\");
	*npath = *path;
    }

/* Find the directory */

    else
    {
	*(p++) = 0;

/* Change to the directory containing the executable */

	if (*(path + 1) == ':')
	    DosSelectDisk(tolower (*path) - 'a' + 1);

/* Save the current directory on this drive */

	getcwd (n1path, PATH_MAX + 3);

/* Find the directory we want */

	if (chdir (path) < 0)
	    return (char *)NULL;

/* Save its full name */

	getcwd (npath, PATH_MAX + 3);

/* Restore the original */

	chdir (n1path);

/* Restore our original directory */

	DosSelectDisk(tolower (*cpath) - 'a' + 1);

	if (chdir (cpath) < 0)
	    return (char *)NULL;
    }

    if (npath[strlen (npath) - 1] != '\\')
	strcat (npath, "\\");

    strcat (npath, p);
    return strcpy (path, npath);
}
