/* MS-DOS SHELL - Internal Command Processing
 *
 * MS-DOS SHELL - Copyright (c) 1990 Data Logic Limited and Charles Forsyth
 *
 * This code is based on (in part) the shell program written by Charles
 * Forsyth and is subject to the following copyright restrictions.  The
 * code for the test (dotest) command was based on code written by
 * Erik Baalbergen.  The following copyright conditions apply:
 *
 * 1.  Redistribution and use in source and binary forms are permitted
 *     provided that the above copyright notice is duplicated in the
 *     source form and the copyright notice in file sh6.c is displayed
 *     on entry to the program.
 *
 * 2.  The sources (or parts thereof) or objects generated from the sources
 *     (or parts of sources) cannot be sold under any circumstances.
 *
 *    $Header: sh7.c 1.17 90/05/31 09:50:05 MS_user Exp $
 *
 *    $Log:	sh7.c $
 * Revision 1.17  90/05/31  09:50:05  MS_user
 * Implement partial write when swapping to disk
 *
 * Revision 1.16  90/04/30  19:50:44  MS_user
 * Stop search path if second character of name is colon
 *
 * Revision 1.15  90/04/25  22:35:53  MS_user
 * Fix bug in doread to stop multi-line reads
 *
 * Revision 1.14  90/04/25  09:21:11  MS_user
 * Change version message processing
 *
 * Revision 1.13  90/04/03  17:59:43  MS_user
 * type didnot check for functions before searching PATH
 *
 * Revision 1.12  90/03/27  20:33:58  MS_user
 * Clear extended file name on interrupt
 *
 * Revision 1.11  90/03/26  20:57:38  MS_user
 * Change I/O restore so that "exec >filename" works
 *
 * Revision 1.10  90/03/14  19:32:05  MS_user
 * Change buffered output to be re-entrant and add it to getopt
 *
 * Revision 1.9  90/03/14  16:45:52  MS_user
 * New Open_buffer parameter
 *
 * Revision 1.8  90/03/13  21:19:50  MS_user
 * Use the new Buffered Output routines in doecho
 *
 * Revision 1.7  90/03/12  20:43:52  MS_user
 * Change bell test to check initialisation file
 *
 * Revision 1.6  90/03/12  17:09:38  MS_user
 * Add a missing cast
 *
 * Revision 1.5  90/03/09  16:06:41  MS_user
 * Add SH_BELL processing
 *
 * Revision 1.4  90/03/06  16:50:10  MS_user
 * Add disable history option
 *
 * Revision 1.3  90/03/05  13:52:49  MS_user
 * Changes to eval and dot functionality
 * Fix bug in escape processing in doecho
 * Add some array size checks
 *
 * Revision 1.2  90/01/30  14:43:34  MS_user
 * Add missing author note
 *
 * Revision 1.1  90/01/29  17:46:25  MS_user
 * Initial revision
 *
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
#include <stdarg.h>
#include "sh.h"

#define INCL_NOPM
#define INCL_DOS
#include <os2.h>

#define	SECS		60L
#define	MINS		3600L
#define IS_OCTAL(a)	(((a) >= '0') && ((a) <= '7'))

/* Definitions for test */

#define END_OF_INPUT	0
#define FILE_READABLE	1
#define FILE_WRITABLE	2
#define FILE_REGULAR	3
#define _FILE_DIRECTORY	4
#define FILE_NONZERO	5
#define FILE_TERMINAL	6
#define STRING_ZERO	7
#define STRING_NONZERO	8
#define STRING_EQUAL	9
#define STRING_NOTEQUAL	10
#define NUMBER_EQUAL	11
#define NUMBER_NOTEQUAL	12
#define NUMBER_EQ_GREAT	13
#define NUMBER_GREATER	14
#define NUMBER_EQ_LESS	15
#define NUMBER_LESS	16
#define UNARY_NOT	17
#define BINARY_AND	18
#define BINARY_OR	19
#define LPAREN		20
#define RPAREN		21
#define OPERAND		22
#define FILE_EXECUTABLE	23
#define FILE_USER	24
#define FILE_GROUP	25
#define FILE_TEXT	26
#define FILE_BLOCK	27
#define FILE_CHARACTER	28
#define FILE_FIFO	29

#define UNARY_OP	1
#define BINARY_OP	2
#define B_UNARY_OP	3
#define B_BINARY_OP	4
#define PAREN		5

static struct test_op {
    char	*op_text;
    short 	op_num;
    short 	op_type;
} test_ops[] = {
    {"-r",	FILE_READABLE,		UNARY_OP},
    {"-w",	FILE_WRITABLE,		UNARY_OP},
    {"-x",	FILE_EXECUTABLE,	UNARY_OP},
    {"-f",	FILE_REGULAR,		UNARY_OP},
    {"-d",	_FILE_DIRECTORY,	UNARY_OP},
    {"-s",	FILE_NONZERO,		UNARY_OP},
    {"-t",	FILE_TERMINAL,		UNARY_OP},
    {"-z",	STRING_ZERO,		UNARY_OP},
    {"-n",	STRING_NONZERO,		UNARY_OP},
    {"=",	STRING_EQUAL,		BINARY_OP},
    {"!=",	STRING_NOTEQUAL,	BINARY_OP},
    {"-eq",	NUMBER_EQUAL,		BINARY_OP},
    {"-ne",	NUMBER_NOTEQUAL,	BINARY_OP},
    {"-ge",	NUMBER_EQ_GREAT,	BINARY_OP},
    {"-gt",	NUMBER_GREATER,		BINARY_OP},
    {"-le",	NUMBER_EQ_LESS,		BINARY_OP},
    {"-lt",	NUMBER_LESS,		BINARY_OP},
    {"!",	UNARY_NOT,		B_UNARY_OP},
    {"-a",	BINARY_AND,		B_BINARY_OP},
    {"-o",	BINARY_OR,		B_BINARY_OP},
    {"(",	LPAREN,			PAREN},
    {")",	RPAREN,			PAREN},
#ifdef S_IFCHR
    {"-c",	FILE_CHARACTER,		UNARY_OP},
#endif
#ifdef S_IFBLK
    {"-b",	FILE_BLOCK,		UNARY_OP},
#endif
#ifdef S_ISUID
    {"-u",	FILE_USER,		UNARY_OP},
#endif
#ifdef S_ISGID
    {"-g",	FILE_GROUP,		UNARY_OP},
#endif
#ifdef S_ISVTX
    {"-k",	FILE_TEXT,		UNARY_OP},
#endif
#ifdef S_IFIFO
    {"-p",	FILE_FIFO,		UNARY_OP},
#endif
    {(char *)NULL,	NULL,		NULL}
};

static int		expr (int);
static int		bexpr (int);
static int		primary (int);
static int		lex (char *);
static long		num (char *);
static void		syntax (void);
static int		dolabel (C_Op *);
static int		dochdir (C_Op *);
static int		dodrive (C_Op *);
static int		doshift (C_Op *);
static int		doumask (C_Op *);
static int		dodot (C_Op *);
static int		doecho (C_Op *);
static int		dogetopt (C_Op *);
static int		dopwd (C_Op *);
static int		dounset (C_Op *);
static int		dotype (C_Op *);
static int		dotest (C_Op *);
static int		dover (C_Op *);
static int		doread (C_Op *);
static int		doeval (C_Op *);
static int		dotrap (C_Op *);
static int		getsig (char *);
static int		dobreak (C_Op *);
static int		docontinue (C_Op *);
static int		brkcontin (char *, int);
static int		doexit (C_Op *);
static int		doexec (C_Op *);
static int		doreturn (C_Op *);
static int		doexport (C_Op *);
static int		domsdos (C_Op *);
static int		doreadonly (C_Op *);
static int		doset (C_Op *);
static int		dohistory (C_Op *);
extern int              dojobs(C_Op *);

static void		setsig (int, int (*)());
static int		rdexp (char **, int, char *);

static char		**test_alist;
static struct test_op	*test_op;
static jmp_buf		test_jmp;

/*
 * built-in commands: doX
 */

static int	dolabel (t)
C_Op		*t;
{
    return 0;
}

/*
 * Getopt - split arguments.  getopts pattern args
 */

static int	dogetopt (t)
register C_Op	*t;
{
    int			argc;
    char		**argv = t->words;
    int			c;
    Out_Buf		*bp;
    char		*c_s = "-c ";

/* Count arguments */

    optind = 1;				/* Reset the optind flag	*/
    opterr = 1;				/* Reset the error flag		*/

    for (argc = 0; t->words[argc] != (char *)NULL; argc++);

    if (argc < 2)
    {
	S_puts ("usage: getopt legal-args $*\n");
	return 2;
    }

/* Get some memory for the buffer */

    if ((bp = Open_buffer (1, FALSE)) == (Out_Buf *)NULL)
    {
	print_error ("getopt: %s\n", strerror (ENOMEM));
	return 1;
    }

    argc -= 2;
    argv += 2;

/* Scan each argument */

    while ((c = getopt (argc, argv, t->words[1])) != EOF)
    {
	if (c == '?')
	    return 2;

	*(c_s + 1) = (char)c;
	Adds_buffer (c_s, bp);

/* Check for addition parameter */

	if (*(strchr (t->words[1], c) + 1) == ':')
	{
	    Adds_buffer (optarg, bp);
	    Add_buffer (SP, bp);
	}
    }

/* Output the separator */

    Adds_buffer ("-- ", bp);
    argv += optind;

/* Output the arguments */

    while (optind++ < argc)
    {
	Adds_buffer (*argv++, bp);
	Add_buffer ((char)((optind == argc) ? NL : SP), bp);
    }

    Close_buffer (bp);
    return 0;
}

/*
 * Echo the parameters
 */

static int	doecho (t)
register C_Op	*t;
{
    int		n = 1;			/* Argument number		*/
    int		no_eol = 0;		/* No EOL			*/
    char	*ip;			/* Input pointer		*/
    int		c_val;			/* Current character		*/
    char	c;
    bool	end_s;
    Out_Buf	*bp;

/* Get some memory for the buffer */

    if ((bp = Open_buffer (1, FALSE)) == (Out_Buf *)NULL)
    {
	print_error ("echo: %s\n", strerror (ENOMEM));
	return 1;
    }

/* Process the arguments */

    while ((ip = t->words[n++]) != (char *)NULL)
    {

/* Check for -n switch */

	if ((n == 2) && (strcmp (ip, "-n") == 0))
	{
	    no_eol++;
	    continue;
	}

/* Process the string */

	end_s = FALSE;

	do
	{

/* Any special character processing ? */

	    if ((c = *(ip++)) == '\\')
	    {
		if ((c_val = Process_Escape (&ip)) == -1)
		{
		    no_eol = 1;
		    continue;
		}

		c = (char)c_val;
	    }

/* End of string - check to see if a space if required */

	    else if (c == 0)
	    {
		end_s = TRUE;

		if (t->words[n] != (char *)NULL)
		    c = SP;

		else
		    continue;
	    }

/* Output the character */

	    Add_buffer (c, bp);

	} while (!end_s);
    }

/* Is EOL required ? */

    if (!no_eol)
	Add_buffer (NL, bp);

/* Flush buffer */

    Close_buffer (bp);
    return 0;
}

/*
 * Process_Escape - Convert an escaped character to a binary value.
 *
 * Returns the binary value and updates the string pointer.
 */

int	Process_Escape (cp)
char	**cp;					/* Pointer to character */
{
    int		c_val = **cp;			/* Current character    */

    if (c_val)
        (*cp)++;

/* Process escaped characters */

    switch (c_val)
    {
        case 'b':			/* Backspace                    */
            return 0x08;

        case 'f':			/* Form Feed                    */
            return 0x0c;

        case 'v':			/* Vertical Tab                 */
            return 0x0b;

        case 'n':			/* New Line                     */
            return 0x0a;

        case 'r':			/* Carriage return              */
            return 0x0d;

        case 't':			/* Forward tab                  */
	    return 0x09;

        case '\\':			/* Backslash                    */
	    return '\\';

        case 'c':			/* no eol			*/
	    return -1;
    }

/* Check for an octal string */

    if (IS_OCTAL (c_val))
    {
	c_val -= '0';

	while ((IS_OCTAL (**cp)))
	    c_val = (c_val * 8) + *((*cp)++) - '0';

	return c_val;
    }

    return c_val;
}

/*
 * Display the current version
 */

static int	dover (t)
C_Op		*t;
{
    Print_Version (1);
    return 0;
}

/*
 * Output the current path: pwd
 */

static int	dopwd (t)
register C_Op	*t;
{
    v1a_puts (C_dir->value);
    return 0;
}

/*
 * Unset a variable: unset <flag..> <variable name...>
 */

static int	dounset (t)
register C_Op	*t;
{
    register int	n = 1;

    while (t->words[n] != (char *)NULL)
        unset (t->words[n++], FALSE);

    return 0;
}

/* Delete a variable or function.  If all is set, system variables can be
 * deleted.  This is used to delete the trap functions
 */

void		unset (cp, all)
register char	*cp;
bool		all;
{
    register Var_List		*vp;
    register Var_List		*pvp;

/* Unset a flag */

    if (*cp == '-')
    {
	while (*(++cp) != 0)
	{
	    if (islower (*cp))
		FL_CLEAR (*cp);
	}

	setdash ();
	return;
    }

/* Ok - unset a variable and not a local value */

    if (!all && !(isalpha (*cp)))
	return;

/* Check in list */

    pvp = (Var_List *)NULL;

    for (vp = vlist; (vp != (Var_List *)NULL) && !eqname (vp->name, cp);
	 vp = vp->next)
	pvp = vp;

/* If not found, delete the function if it exists */

    if (vp == (Var_List *)NULL)
    {
	Fun_Ops 	*fp;

	if ((fp = Fun_Search (cp)) != (Fun_Ops *)NULL)
	    Save_Function (fp->tree, TRUE);

	return;
    }

/* Error if read-only */

    if (vp->status & (RONLY | PONLY))
    {
	if ((cp = strchr (vp->name, '=')) != (char *)NULL)
	    *cp = 0;

	S_puts (vp->name);

	if (cp != (char *)NULL)
	    *cp = '=';

	S_puts ((vp->status & PONLY) ? ": cannot unset\n" : " is read-only\n");
	return;
    }

/* Delete it */

    if (vp->status & GETCELL)
	DELETE (vp->name);

    if (pvp == (Var_List *)NULL)
	vlist = vp->next;

    else
	pvp->next = vp->next;

    DELETE (vp);
}

/*
 * Execute a test: test <arguments>
 */

static int	dotest (t)
register C_Op	*t;
{
    int		st = 0;

    if (*(test_alist = &t->words[1]) == (char *)NULL)
	return 1;

/* If [ <arguments> ] form, check for end ] and remove it */

    if (strcmp (t->words[0], "[") == 0)
    {
	while (t->words[++st] != (char *)NULL)
	    ;

	if (strcmp (t->words[--st], "]") != 0)
	{
	    print_error ("test: missing ']'\n");
	    return 1;
	}

	else
	    t->words[st] = (char *)NULL;
    }

/* Set abort address */

    if (setjmp (test_jmp))
	return 1;

    st = !expr (lex (*test_alist));

    if (*(++test_alist) != (char *)NULL)
	syntax ();

    return (st);
}

static int	expr (n)
int		n;
{
    int		res;

    if (n == END_OF_INPUT)
	syntax ();

    res = bexpr (n);

    if (lex (*(++test_alist)) == BINARY_OR)
	return expr (lex (*(++test_alist))) || res;

    test_alist--;
    return res;
}

static int	bexpr (n)
int		n;
{
    int res;

    if (n == END_OF_INPUT)
	syntax ();

    res = primary (n);
    if (lex (*(++test_alist)) == BINARY_AND)
	return bexpr (lex (*(++test_alist))) && res;

    test_alist--;
    return res;
}

static int	primary (n)
int		n;
{
    register char	*opnd1, *opnd2;
    struct stat		s;
    int			res;

    if (n == END_OF_INPUT)
	syntax ();

    if (n == UNARY_NOT)
	return !expr (lex (*(++test_alist)));

    if (n == LPAREN)
    {
	res = expr (lex (*(++test_alist)));

	if (lex (*(++test_alist)) != RPAREN)
	    syntax ();

	return res;
    }

    if (n == OPERAND)
    {
	opnd1 = *test_alist;
	(void) lex (*(++test_alist));

	if ((test_op != (C_Op *)NULL) && test_op->op_type == BINARY_OP)
	{
	    struct test_op *op = test_op;

	    if ((opnd2 = *(++test_alist)) == (char *)NULL)
		syntax ();

	    switch (op->op_num)
	    {
		case STRING_EQUAL:
		    return strcmp (opnd1, opnd2) == 0;

		case STRING_NOTEQUAL:
		    return strcmp (opnd1, opnd2) != 0;

		case NUMBER_EQUAL:
		    return num (opnd1) == num (opnd2);

		case NUMBER_NOTEQUAL:
		    return num (opnd1) != num (opnd2);

		case NUMBER_EQ_GREAT:
		    return num (opnd1) >= num (opnd2);

		case NUMBER_GREATER:
		    return num (opnd1) > num (opnd2);

		case NUMBER_EQ_LESS:
		    return num (opnd1) <= num (opnd2);

		case NUMBER_LESS:
		    return num (opnd1) < num (opnd2);
	    }
	}

	test_alist--;
	return strlen (opnd1) > 0;
    }

/* unary expression */

    if (test_op->op_type != UNARY_OP || *++test_alist == 0)
	syntax ();

    switch (n)
    {
	case STRING_ZERO:
	    return strlen (*test_alist) == 0;

	case STRING_NONZERO:
	    return strlen (*test_alist) != 0;

	case FILE_READABLE:
	    return access (*test_alist, R_OK) == 0;

	case FILE_WRITABLE:
	    return access (*test_alist, W_OK) == 0;

	case FILE_EXECUTABLE:
	    return access (*test_alist, X_OK) == 0;

	case FILE_REGULAR:
	    return stat (*test_alist, &s) == 0 && S_ISREG(s.st_mode);

	case _FILE_DIRECTORY:
	    return stat (*test_alist, &s) == 0 && S_ISDIR(s.st_mode);

	case FILE_NONZERO:
	    return stat (*test_alist, &s) == 0 && (s.st_size > 0L);

	case FILE_TERMINAL:
	    return isatty ((int)num (*test_alist));

#ifdef S_ISUID
	case FILE_USER:
	    return stat (*test_alist, &s) == 0 && (s.st_mode & S_ISUID);
#endif

#ifdef S_ISGID
	case FILE_GROUP:
	    return stat (*test_alist, &s) == 0 && (s.st_mode & S_ISGID);
#endif

#ifdef S_ISVTX
	case FILE_TEXT:
	    return stat (*test_alist, &s) == 0 && (s.st_mode & S_ISVTX);
#endif

#ifdef S_IFBLK
	case FILE_BLOCK:
	    return stat (*test_alist, &s) == 0 && S_ISBLK(s.st_mode);
#endif

#ifdef S_IFCHR
	case FILE_CHARACTER:
	    return stat (*test_alist, &s) == 0 && S_ISCHR(s.st_mode);
#endif

#ifdef S_IFIFO
	case FILE_FIFO:
	    return stat (*test_alist, &s) == 0 && S_ISFIFO(s.st_mode);
#endif
    }
}

static int	lex (s)
register char	*s;
{
    register struct test_op	*op = test_ops;

    if (s == (char *)NULL)
	return END_OF_INPUT;

    while (op->op_text)
    {
	if (strcmp (s, op->op_text) == 0)
	{
	    test_op = op;
	    return op->op_num;
	}

	op++;
    }

    test_op = (struct test_op *)NULL;
    return OPERAND;
}

/*
 * Get a long numeric value
 */

static long	num (s)
register char	*s;
{
    char	*ep;
    long	l = strtol (s, &ep, 10);

    if (!*s || *ep)
	syntax ();

    return l;
}

/*
 * test syntax error - abort
 */

static void	syntax ()
{
    print_error ("test: syntax error\n");
    longjmp (test_jmp, 1);
}

/*
 * Select a new drive: x:
 *
 * Select the drive, get the current directory and check that we have
 * actually selected the drive
 */

static int	dodrive (t)
register C_Op	*t;
{
    unsigned int	cdrive;
    unsigned int	ndrive = tolower (**t->words) - 'a' + 1;
    ULONG l_map;

    DosSelectDisk(ndrive);
    Getcwd ();
    DosQCurDisk((PUSHORT) &cdrive, &l_map);
    return (ndrive == cdrive) ? 0 : 1;
}

/*
 * Select a new directory: cd
 */

static int	dochdir (t)
register C_Op	*t;
{
    char		*p;		/* Original new directory	*/
    char		*nd;		/* New directory		*/
    register char	*cp;		/* In CDPATH Pointer		*/
    char		*directory;
    int			first = 0;
    unsigned int	dummy;
    unsigned int	cdrive;
    ULONG l_map;

/* If restricted shell - illegal */

    if (check_rsh ("cd"))
	return 1;

/* Use default ? */

    if (((p = t->words[1]) == (char *)NULL) &&
	((p = lookup (home, FALSE)->value) == null))
    {
	print_error ("cd: no home directory\n");
	return 1;
    }

    if ((directory = getcell (FFNAME_MAX)) == (char *)NULL)
    {
	print_error ("cd: %s\n", strerror (ENOMEM));
	return 1;
    }

/* Save the current drive */

    DosQCurDisk((PUSHORT) &cdrive, &l_map);

/* Scan for the directory.  If there is not a / or : at start, use the
 * CDPATH variable
 */

    cp = ((*p == '/') || (*(p + 1) == ':')) ? null
					    : lookup ("CDPATH", FALSE)->value;

    do
    {
	cp = path_append (cp, p, directory);

/* Check for new disk drive */

	nd = directory;

	if (*(nd + 1) == ':')
	{
	    DosSelectDisk(tolower (*nd) - 'a' + 1);
	    nd += 2;
	}

/* Was the change successful? */

	if ((!*nd) || (chdir (nd) == 0))
	{

/* OK - reset the current directory (in the shell) and display the new
 * path if appropriate
 */

	    Getcwd ();

	    if (first || (strchr (p, '/') != (char *)NULL))
		dopwd (t);

	    return 0;
	}

	first = 1;

    } while (cp != (char *)NULL);

/* Restore our original drive and restore directory info */

    DosSelectDisk(cdrive);
    Getcwd ();

    print_error ("%s: bad directory\n", p);
    return 1;
}

/*
 * Extract the next path from a string and build a new path from the
 * extracted path and a file name
 */
char		*path_append (path_s, file_s, output_s)
register char	*path_s;		/* Path string			*/
register char	*file_s;		/* File name string		*/
char		*output_s;		/* Output path			*/
{
    register char	*s = output_s;
    int			fsize = 0;

    while (*path_s && (*path_s != ';') && (fsize++ < FFNAME_MAX))
	*s++ = *path_s++;

    if ((output_s != s) && (*(s - 1) != '/') && (fsize++ < FFNAME_MAX))
	*s++ = '/';

    *s = '\0';

    if (file_s != (char *)NULL)
	strncpy (s, file_s, FFNAME_MAX - fsize);

    output_s[FFNAME_MAX - 1] = 0;

    return (*path_s ? ++path_s : (char *)NULL);
}

/*
 * Execute a shift command: shift <n>
 */

static int	doshift (t)
register C_Op	*t;
{
    register int	n;

    n = (t->words[1] != (char *)NULL) ? getn (t->words[1]) : 1;

    if (dolc < n)
    {
	print_error ("sh: nothing to shift\n");
	return 1;
    }

    dolv[n] = dolv[0];
    dolv += n;
    dolc -= n;
    setval (lookup ("#", TRUE), putn (dolc));
    return 0;
}

/*
 * Execute a umask command: umask <n>
 */

static int	doumask (t)
register C_Op	*t;
{
    register int	i;
    register char	*cp;

    if ((cp = t->words[1]) == (char *)NULL)
    {
	i = umask (0);
	umask (i);
	v1printf ("%o\n", i);
    }

    else
    {
	i = 0;
	while (IS_OCTAL (*cp))
	    i = i * 8 + (*(cp++) - '0');

	umask (i);
    }

    return 0;
}

/*
 * Execute an exec command: exec <arguments>
 */

static int	doexec (t)
register C_Op	*t;
{
    register int	i;
    jmp_buf		ex;
    int			*ofail;
    IO_Actions		**ios = t->ioact;

    for (i = 0; (t->words[i] = t->words[i + 1]) != (char *)NULL; i++)
	;

/* Left the I/O as it is */

    if (i == 0)
	return restore_std (0, FALSE);

    execflg = 1;
    ofail = failpt;

/* Set execute function recursive level to zero */

    Execute_stack_depth = 0;
    t->ioact = (IO_Actions **)NULL;

    if (setjmp (failpt = ex) == 0)
	execute (t, NOPIPE, NOPIPE, FEXEC);

/* Clear the extended file if an interrupt happened */

    Clear_Extended_File ();
    t->ioact = ios;
    failpt = ofail;
    execflg = 0;
    return 1;
}

/*
 * Execute a script in the current shell
 */

static int	dodot (t)
C_Op		*t;
{
    register int	i;
    register char	*sp;
    char		*cp;
    char		*l_path;

    if ((cp = t->words[1]) == (char *)NULL)
	return 0;

/* Get some space */

    if ((l_path = getcell (FFNAME_MAX)) == (char *)NULL)
    {
	print_error (".: %s\n", strerror (ENOMEM));
	return 1;
    }

/* Save the current drive */

    sp = (any ('/', cp) || (*(cp + 1) == ':')) ? null : path->value;

    do
    {
	sp = path_append (sp, cp, l_path);

	if ((i = O_for_execute (l_path, (char **)NULL, (int *)NULL)) >= 0)
	    return RUN (afile, remap (i), filechar, FALSE);

    } while (sp != (char *)NULL);

    print_error ("%s: not found\n", cp);
    return 1;
}

/*
 * Read from standard input into a variable list
 */

static int	doread (t)
C_Op		*t;
{
    register char	*cp;
    register char	**wp;
    register int	nb;
    bool		nl_detected = FALSE;
    char		*buffer;
    char		*ep;

/* Check usage */

    if (t->words[1] == (char *)NULL)
    {
	print_error ("Usage: read name ...\n");
	return 1;
    }

/* Get some memory */

    if ((buffer = getcell (LINE_MAX)) == (char *)NULL)
    {
	print_error ("read: %s\n", strerror (ENOMEM));
	return 1;
    }

    ep = &buffer[LINE_MAX - 2];

/* Save the current drive */

    for (wp = t->words + 1; *wp != (char *)NULL; wp++)
    {

/* Read in until end of line, file or a field separator is detected */

	for (cp = buffer; !nl_detected && (cp < ep); cp++)
	{
	    if (((nb = read (STDIN_FILENO, cp, 1)) != 1) || (*cp == NL) ||
		((wp[1] != (char *)NULL) && any (*cp, ifs->value)))
	    {
		if ((nb != 1) || (*cp == NL))
		    nl_detected = TRUE;

		break;
	    }
	}

	*cp = 0;

	if (nb <= 0)
	    break;

	setval (lookup (*wp, TRUE), buffer);
    }

    return (nb <= 0);
}

/*
 * Evaluate an expression
 */

static int	doeval (t)
register C_Op	*t;
{
    return RUN (awordlist, t->words + 1, wdchar, TRUE);
}

/*
 * Execute a trap
 */

static int	dotrap (t)
register C_Op	*t;
{
    register int	n, i;
    register int	resetsig;
    char		tval[10];
    char		*cp;

    if (t->words[1] == (char *)NULL)
    {

/* Display trap - look up each trap and print those we find */

	for (i = 0; i < NSIG; i++)
	{
	    sprintf (tval, "~%d", i);

	    if ((cp = lookup (tval, FALSE)->value) != null)
	    {
		v1printf ("%u: ", i);
		v1a_puts (cp);
	    }
	}

	return 0;
    }

    resetsig = isdigit (*t->words[1]);		/* Reset signal?	*/

    for (i = resetsig ? 1 : 2; t->words[i] != (char *)NULL; ++i)
    {

/* Generate the variable name */

	sprintf (tval, "~%d", (n = getsig (t->words[i])));

	if (n == -1)
	    return 1;

	unset (tval, TRUE);

/* Re-define signal processing */

	if (!resetsig)
	{
	    if (*t->words[1] != '\0')
	    {
		setval (lookup (tval, TRUE), t->words[1]);
		setsig (n, sig);
	    }

	    else
		setsig (n, SIG_IGN);
	}

/* Clear signal processing */

	else if (talking)
	{
	    if (n == SIGINT)
		setsig (n, onintr);

	    else
#ifdef SIGQUIT
		setsig (n, n == SIGQUIT ? SIG_IGN : SIG_DFL);
#else
		setsig (n, SIG_DFL);
#endif
	}

	else
	    setsig (n, SIG_DFL);
    }

    return 0;
}

/*
 * Get a signal number
 */

static int	getsig (s)
char		*s;
{
    register int	n;

    if (((n = getn (s)) < 0) || (n >= NSIG))
    {
	print_error ("trap: bad signal number\n");
	n = -1;
    }

    return n;
}

/*
 * Set up a signal function
 */

static void	setsig (n, f)
register int	n;
int		(*f)();
{
    if (n == 0)
	return;

    if ((signal (n, SIG_IGN) != SIG_IGN) || (ourtrap & (1L << n)))
    {
	ourtrap |= (1L << n);
	signal (n, f);
    }
}

/* Convert a string to a number */

int	getn (as)
char	*as;
{
    char	*s;
    int		n = (int)strtol (as, &s, 10);

    if (*s)
	print_error ("%s: bad number\n", as);

    return n;
}

/*
 * BREAK and CONTINUE processing
 */

static int	dobreak (t)
C_Op		*t;
{
    return brkcontin (t->words[1], BC_BREAK);
}

static int	docontinue (t)
C_Op		*t;
{
    return brkcontin (t->words[1], BC_CONTINUE);
}

static int	brkcontin (cp, val)
register char	*cp;
int		val;
{
    register Break_C	*Break_Loc;
    register int	nl;

    if ((nl = (cp == (char *)NULL) ? 1 : getn (cp)) <= 0)
	nl = 999;

    do
    {
	if ((Break_Loc = Break_List) == (Break_C *)NULL)
	    break;

	Break_List = Break_Loc->nextlev;

    } while (--nl);

    if (nl)
    {
	print_error ("sh: bad break/continue level\n");
	return 1;
    }

    longjmp (Break_Loc->brkpt, val);

/* NOTREACHED */
}

/*
 * Exit function
 */

static int	doexit (t)
C_Op		*t;
{
    Break_C	*SShell_Loc = SShell_List;

    execflg = 0;

/* Set up error codes */

    if (t->words[1] != (char *)NULL)
    {
	exstat = getn (t->words[1]);
	setval (lookup ("?", TRUE), t->words[1]);
    }

/* Are we in a subshell.  Yes - do a longjmp instead of an exit */

    if (SShell_Loc != (Break_C *)NULL)
    {
	SShell_List = SShell_Loc->nextlev;
	longjmp (SShell_Loc->brkpt, 1);
    }

    leave ();
    return 1;
}

/*
 * Function return - set exit value and return via a long jmp
 */

static int	doreturn (t)
C_Op		*t;
{
    Break_C	*Return_Loc = Return_List;

    if  (t->words[1] != (char *)NULL)
	setval (lookup ("?", TRUE), t->words[1]);

/* If the return address is defined - return to it.  Otherwise, return
 * the value
 */

    if (Return_Loc != (Break_C *)NULL)
    {
	Return_List = Return_Loc->nextlev;
	longjmp (Return_Loc->brkpt, 1);
    }

    return getn (t->words[1]);
}

/*
 * MSDOS, EXPORT and READONLY functions
 */

static int	doexport (t)
C_Op		*t;
{
    return rdexp (t->words + 1, EXPORT, "export ");
}

static int	doreadonly (t)
C_Op		 *t;
{
    return rdexp (t->words + 1, RONLY, "readonly ");
}

static int	domsdos (t)
C_Op		*t;
{
    return rdexp (t->words + 1, C_MSDOS, "msdos ");
}

static int	rdexp (wp, key, tstring)
register char	**wp;
int		key;
char		*tstring;
{
    char	*cp;
    bool	valid;

    if (*wp != (char *)NULL)
    {
	for (; *wp != (char *)NULL; wp++)
	{
	    cp = *wp;
	    valid = TRUE;

/* Check for a valid name */

	    if (!isalpha (*(cp++)))
		valid = FALSE;

	    else
	    {
		while (*cp)
		{
		    if (!isalnum (*(cp++)))
		    {
			valid = FALSE;
			break;
		    }
		}
	    }

/* If valid - update, otherwise print a message */

	    if (valid)
		s_vstatus (lookup (*wp, TRUE), key);

	    else
		print_error ("%s: bad identifier\n", *wp);
	}
    }

    else
    {
	register Var_List	*vp;

	for (vp = vlist; vp != (Var_List *) NULL; vp = vp->next)
	{
	    if ((vp->status & key) && isalpha (*vp->name))
	    {
		v1_puts (tstring);
		write (STDOUT_FILENO, vp->name,
		       (int)(findeq (vp->name) - vp->name));
		v1_putc (NL);
	    }
	}
    }

    return 0;
}

/*
 * Sort Compare function for displaying variables
 */

int	sort_compare (s1, s2)
char	**s1;
char	**s2;
{
    return strcmp (*s1, *s2);
}

/*
 * Set function
 */

static int	doset (t)
register C_Op	*t;
{
    register Var_List	*vp;
    register char	*cp;
    register int	n, j;
    Fun_Ops		*fp;
    char		sign;
    char		**list;

/* Display ? */

    if ((cp = t->words[1]) == (char *)NULL)
    {

/* Count the number of entries to print */

	for (n = 0, vp = vlist; vp != (Var_List *)NULL; vp = vp->next)
	{
	    if (isalnum (*vp->name))
		n++;
	}

/* Build a local array of name */

	list = (char **)space (sizeof (char *) * n);

	for (n = 0, vp = vlist; vp != (Var_List *)NULL; vp = vp->next)
	{
	    if (isalnum (*vp->name))
	    {
		if (list == (char **)NULL)
		    v1a_puts (vp->name);

		else
		    list[n++] = vp->name;
	    }
	}

/* Sort them and then print */

	if (list != (char **)NULL)
	{
	    qsort (list, n, sizeof (char *), sort_compare);

	    for (j = 0; j < n; j++)
		v1a_puts (list[j]);

	    DELETE (list);
	}

/* Print the list of functions */

	for (fp = fun_list; fp != (Fun_Ops *)NULL; fp = fp->next)
	    Print_ExTree (fp->tree);

	return 0;
    }

/* Set/Unset a flag ? */

    if (((sign = *cp) == '-') || (*cp == '+'))
    {
	for (n = 0; (t->words[n] = t->words[n + 1]) != (char *)NULL; n++)
	    ;

	for (; *cp; cp++)
	{
	    if (*cp == 'r')
	    {
		print_error ("set: -r bad option\n");
		return 1;
	    }

	    if (*cp == 'e')
	    {
		if (!talking)
		{
		    if (sign == '-')
			FL_SET ('e');

		    else
			FL_CLEAR ('e');
		}
	    }

	    else if (islower (*cp))
	    {
		if (sign == '-')
		    FL_SET (*cp);

		else
		    FL_CLEAR (*cp);
	    }
	}

	setdash ();
    }

/* Set up parameters ? */

    if (t->words[1])
    {
	t->words[0] = dolv[0];

	for (n = 1; t->words[n] != (char *)NULL; n++)
	    setarea ((char *)t->words[n], 0);

	dolc = n-1;
	dolv = t->words;
	setval (lookup ("#", TRUE), putn (dolc));
	setarea ((char *)(dolv - 1), 0);
    }

    return 0;
}

/*
 * History functions - display, initialise, enable, disable
 */

#ifndef NO_HISTORY
static int	dohistory (t)
C_Op		*t;
{
    char	*cp;

    if (!talking)
	return 1;

    if ((cp = t->words[1]) == (char *)NULL)
	Display_History ();

    else if (strcmp (cp, "-i") == 0)
	Clear_History ();

    else if (strcmp (cp, "-d") == 0)
	History_Enabled = FALSE;

    else if (strcmp (cp, "-e") == 0)
	History_Enabled = TRUE;

    return 0;
}
#endif

/*
 * Type function: For each name, indicate how it would be interpreted
 */

static char	*type_ext[] = {
    "", ".exe", ".com", ".sh", BATCHEXT
};

static int	dotype (t)
register C_Op	*t;
{
    register char	*sp;			/* Path pointers	*/
    char		*cp;
    char		*ep;
    char		*xp;			/* In file name pointers */
    char		*xp1;
    int			n = 1;			/* Argument count	*/
    int			i, fp;
    bool		found;			/* Found flag		*/
    char		*l_path;
    Fun_Ops		*fops;
    char                *mp;

/* Get some memory for the buffer */

    if ((l_path = getcell (FFNAME_MAX + 4)) == (char *)NULL)
    {
	print_error ("type: %s\n", strerror (ENOMEM));
	return 1;
    }

/* Process each parameter */

    while ((cp = t->words[n++]) != (char *)NULL)
    {
	if ( inbuilt (cp) )
        {
	    v1_puts (cp);
	    v1a_puts (" is a shell internal command");
	    continue;
        }

        if ( cmd_internal(cp) )
        {
	    v1_puts (cp);
	    v1a_puts (" is a CMD.EXE internal command");
	    continue;
        }

/* Check for a function */

	if ((fops = Fun_Search (cp)) != (Fun_Ops *)NULL)
	{
	    v1_puts (cp);
	    v1a_puts (" is a function");
	    Print_ExTree (fops->tree);
	    continue;
	}

/* Scan the path for an executable */

	sp = (any ('/', cp) || (*(cp + 1) == ':')) ? null : path->value;
	found = FALSE;

	do
	{
	    sp = path_append (sp, cp, l_path);
	    ep = &l_path[strlen (l_path)];

/* Get start of file name */

	    if ((xp1 = strrchr (l_path, '/')) == (char *)NULL)
		xp1 = l_path;

	    else
		++xp1;

/* Look up all 5 types */

	    for (i = 0; (i < 5) && !found; i++)
	    {
		strcpy (ep, type_ext[i]);

		if (access (l_path, F_OK) == 0)
		{

/* If no extension or .sh extension, check for shell script */

		    if (((xp = strchr (xp1, '.')) == (char *)NULL) ||
			(stricmp (xp, ".sh") == 0))
		    {
			if ((fp = Check_Script (l_path, (char **)NULL,
						(int *)NULL)) < 0)
			    continue;

			S_close (fp, TRUE);
		    }

		    else if ((stricmp (xp, ".exe") != 0) &&
			     (stricmp (xp, ".com") != 0) &&
			     (stricmp (xp, BATCHEXT) != 0))
			continue;

                    strlwr(l_path);

                    for ( mp = l_path; *mp; mp++ )
                      if ( *mp == '\\' )
                        *mp = '/';

		    print_error ("%s is %s\n", cp, l_path);
		    found = TRUE;
		}
	    }
	} while ((sp != (char *)NULL) && !found);

	if (!found)
	    print_error ("%s not found\n", cp);
    }

    return 0;
}

/* Table of internal commands */

static struct	builtin	builtin[] = {
	".",		dodot,
	":",		dolabel,
	"[",		dotest,
	"break",	dobreak,
	"cd",		dochdir,
	"continue",	docontinue,
	"echo",		doecho,
	"eval",		doeval,
	"exec",		doexec,
	"exit",		doexit,
	"export",	doexport,
	"getopt",	dogetopt,
#ifndef NO_HISTORY
	"history",	dohistory,
#endif
        "jobs",         dojobs,
	"msdos",	domsdos,
	"pwd",		dopwd,
	"read",		doread,
	"readonly",	doreadonly,
	"return",	doreturn,
	"set",		doset,
	"shift",	doshift,
	"test",		dotest,
	"trap",		dotrap,
	"type",		dotype,
	"umask",	doumask,
	"unset",	dounset,
	"ver",		dover,
	(char *)NULL,
};

/*
 * Look up a built in command
 */

int		(*inbuilt (s))()
register char	*s;
{
    register struct builtin	*bp;

    if ((strlen (s) == 2) && isalpha (*s) && (*s != '_') && (*(s + 1) == ':'))
	return dodrive;

    for (bp = builtin; bp->command != (char *)NULL; bp++)
    {
	if (stricmp (bp->command, s) == 0)
	    return bp->fn;
    }

    return (int (*)())NULL;
}

/* recognize CMD.EXE internal commands */

char *cmd_tab[] =
{
  "chcp", "cls", "copy",
  "date", "del", "detach", "dir",
  "erase",
  "md", "mkdir", "move",
  "ren", "rename", "rd", "rmdir",
  "start",
  "time",
  NULL
};

#define cmds (sizeof(cmd_tab) / sizeof(char *) - 1)

char *cmd_internal(char *s)
{
    int cnt;

    for ( cnt = 0; cnt < cmds; cnt++ )
    {
	if (stricmp (cmd_tab[cnt], s) == 0)
	    return cmd_tab[cnt];
    }

    return NULL;
}

/* Write to stdout functions - printf, fputs, fputc, and a special */

/*
 * Equivalent of printf without using streams
 */

void	v1printf (fmt)
char	*fmt;
{
    va_list	ap;
    char	x[100];

    va_start (ap, fmt);
    vsprintf (x, fmt, ap);
    v1_puts (x);
    va_end (ap);
}

/*
 * Write string to STDOUT
 */

void		v1_puts (s)
char		*s;
{
    write (STDOUT_FILENO, s, strlen (s));
}

/*
 * Write string to STDOUT with a NL at end
 */

void		v1a_puts (s)
char		*s;
{
    char	c = NL;

    write (STDOUT_FILENO, s, strlen (s));
    write (STDOUT_FILENO, &c, 1);
}

/*
 * Write 1 character to STDOUT
 */

void		v1_putc (c)
char		c;
{
    if ((c != 0x07) || Ring_Bell ())
	write (STDOUT_FILENO, &c, 1);
}
