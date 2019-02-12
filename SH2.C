/* MS-DOS SHELL - Parser
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
 *    $Header: sh2.c 1.5 90/04/25 09:18:38 MS_user Exp $
 *
 *    $Log:	sh2.c $
 * Revision 1.5  90/04/25  09:18:38  MS_user
 * Fix for ... do to not require terminating colon
 *
 * Revision 1.4  90/03/14  19:30:06  MS_user
 * Make collect a global for here document processing.
 * Add IOTHERE support to detect <<- redirection
 *
 * Revision 1.3  90/03/06  16:49:42  MS_user
 * Add disable history option
 *
 * Revision 1.2  90/03/05  13:49:41  MS_user
 * Change talking checks
 *
 * Revision 1.1  90/01/25  13:41:12  MS_user
 * Initial revision
 *
 */

#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "sh.h"

/*
 * shell: syntax (C version)
 */

typedef union {
    char	*cp;
    char	**wp;
    int		i;
    C_Op	*o;
} YYSTYPE;

#define	WORD	256
#define	LOGAND	257
#define	LOGOR	258
#define	BREAK	259
#define	IF	260
#define	THEN	261
#define	ELSE	262
#define	ELIF	263
#define	FI	264
#define	CASE	265
#define	ESAC	266
#define	FOR	267
#define	WHILE	268
#define	UNTIL	269
#define	DO	270
#define	DONE	271
#define	IN	272
#define	YYERRCODE 300

/* flags to yylex */

#define	CONTIN	01	/* skip new lines to complete command */

static bool		startl;
static int		peeksym;
static bool		Allow_funcs;
static int		iounit = IODEFAULT;
static C_Op		*tp;
static YYSTYPE		yylval;
static char		*syntax_err = "sh: syntax error\n";

static C_Op		*pipeline (int);
static C_Op		*andor (void);
static C_Op		*c_list (bool);
static bool		synio (int);
static void		musthave (int, int);
static C_Op		*simple (void);
static C_Op		*nested (int, int);
static C_Op		*command (int);
static C_Op		*dogroup (int);
static C_Op		*thenpart (void);
static C_Op		*elsepart (void);
static C_Op		*caselist (void);
static C_Op		*casepart (void);
static char		**pattern (void);
static char		**wordlist (void);
static C_Op		*list (C_Op *, C_Op *);
static C_Op		*block (int, C_Op *, C_Op *, char **);
static int		rlookup (char *);
static C_Op		*namelist (C_Op *);
static char		**copyw (void);
static void		word (char *);
static IO_Actions	**copyio (void);
static IO_Actions	*io (int, int, char *);
static void		yyerror (char *);
static int		yylex (int);
static int		dual (int);
static void		diag (int);
static char		*tree (unsigned int);

C_Op	*yyparse ()
{
    C_Op	*outtree;

    startl  = TRUE;
    peeksym = 0;
    yynerrs = 0;
    outtree = c_list (TRUE);
    musthave (NL, 0);

    return (yynerrs != 0) ? (C_Op *)NULL : outtree;
}

static C_Op	*pipeline (cf)
int		cf;
{
    register C_Op	*t, *p;
    register int	c;

    if ((t = command (cf)) != (C_Op *)NULL)
    {
	Allow_funcs = FALSE;
	while ((c = yylex (0)) == '|')
	{
	    if ((p = command (CONTIN)) == (C_Op *)NULL)
		yyerror (syntax_err);

/* shell statement */

	    if ((t->type != TPAREN) && (t->type != TCOM))
		t = block (TPAREN, t, NOBLOCK, NOWORDS);

	    t = block (TPIPE, t, p, NOWORDS);
	}

	peeksym = c;
    }

    return t;
}

static C_Op	*andor ()
{
    register C_Op	*t, *p;
    register int	c;

    if ((t = pipeline (0)) != (C_Op *)NULL)
    {
	Allow_funcs = FALSE;
	while (((c = yylex (0)) == LOGAND) || (c == LOGOR))
	{
	    if ((p = pipeline (CONTIN)) == (C_Op *)NULL)
		yyerror (syntax_err);

	    t = block ((c == LOGAND) ? TAND : TOR, t, p, NOWORDS);
	}

	peeksym = c;
    }

    return t;
}

static C_Op	*c_list (allow)
bool		allow;
{
    register C_Op	*t, *p;
    register int	c;

/* Functions are only allowed at the start of a line */

    Allow_funcs = allow;

    if ((t = andor ()) != (C_Op *)NULL)
    {
	Allow_funcs = FALSE;

	if ((peeksym = yylex (0)) == '&')
	    t = block (TASYNC, t, NOBLOCK, NOWORDS);

	while ((c = yylex(0)) == ';' || c == '&' || multiline && c == NL)
	{
	    if ((p = andor ()) == (C_Op *)NULL)
		return t;

	    if ((peeksym = yylex (0)) == '&')
		p = block (TASYNC, p, NOBLOCK, NOWORDS);

	    t = list (t, p);
	}
	peeksym = c;
    }

    return t;
}


static bool	synio (cf)
int		cf;
{
    register IO_Actions	*iop;
    register int	i;
    register int	c;

    if (((c = yylex (cf)) != '<') && (c != '>'))
    {
	peeksym = c;
	return FALSE;
    }

    i = yylval.i;
    musthave (WORD, 0);
    iop = io (iounit, i, yylval.cp);
    iounit = IODEFAULT;

    if (i & IOHERE)
	markhere (yylval.cp, iop);

    return TRUE;
}

static void	musthave (c, cf)
int		c, cf;
{
    if ((peeksym = yylex (cf)) != c)
	yyerror (syntax_err);

    peeksym = 0;
}

static C_Op	*simple ()
{
    register C_Op	*t = (C_Op *)NULL;

    while (1)
    {
	switch (peeksym = yylex (0))
	{
	    case '<':
	    case '>':
		synio (0);
		break;

	    case WORD:
		if (t == (C_Op *)NULL)
		    (t = (C_Op *)tree (sizeof (C_Op)))->type = TCOM;

		peeksym = 0;
		word (yylval.cp);
		break;

/* Check for function - name () { word; } */

	    case '(':
		if ((t != (C_Op *)NULL) && (Allow_funcs == TRUE) &&
		    (wdlist != (Word_B *)NULL) && (wdlist->w_nword == 1))
		{
		    Word_B	*save;

		    peeksym = 0;
		    musthave (')', 0);
		    musthave ('{', 0);
		    save = wdlist;
		    wdlist = (Word_B *)NULL;
		    t->type = TFUNC;
		    t->left = nested (TBRACE, '}');
		    wdlist = save;
		    Allow_funcs = FALSE;
		    musthave (NL, 0);
		    peeksym = NL;
		}

	    default:
		return t;
	}
    }
}

static C_Op	*nested (type, mark)
int		type, mark;
{
    register C_Op	*t;

    multiline++;
    t = c_list (FALSE);
    musthave (mark, 0);
    multiline--;
    return block (type, t, NOBLOCK, NOWORDS);
}

static C_Op	*command (cf)
int		cf;
{
    register C_Op	*t;
    Word_B		*iosave = iolist;
    register int	c;

    iolist = (Word_B *)NULL;

    if (multiline)
	cf |= CONTIN;

    while (synio (cf))
	cf = 0;

    switch (c = yylex (cf))
    {
	default:
	    peeksym = c;

	    if ((t = simple ()) == (C_Op *)NULL)
	    {
		if (iolist == (Word_B *)NULL)
		    return (C_Op *)NULL;

		(t = (C_Op *)tree (sizeof (C_Op)))->type = TCOM;
	    }

	    break;

	case '(':
	    t = nested (TPAREN, ')');
	    break;

	case '{':
	    t = nested (TBRACE, '}');
	    break;

	case FOR:
	    (t = (C_Op *)tree (sizeof (C_Op)))->type = TFOR;
	    musthave (WORD, 0);
	    startl = TRUE;
	    t->str = yylval.cp;
	    multiline++;
	    t->words = wordlist ();

/* CHeck for "for word in word...; do" versus "for word do" */

	    c = yylex (0);

	    if ((t->words == (char **)NULL) && (c != NL))
		peeksym = c;

	    else if ((t->words != (char **)NULL) && (c != NL) && (c != ';'))
		yyerror (syntax_err);

	    t->left = dogroup (0);
	    multiline--;
	    break;

	case WHILE:
	case UNTIL:
	    multiline++;
	    t = (C_Op *)tree (sizeof (C_Op));
	    t->type = (c == WHILE) ? TWHILE : TUNTIL;
	    t->left = c_list (FALSE);
	    t->right = dogroup (1);
	    t->words = (char **)NULL;
	    multiline--;
	    break;

	case CASE:
	    (t = (C_Op *)tree (sizeof (C_Op)))->type = TCASE;
	    musthave (WORD, 0);
	    t->str = yylval.cp;
	    startl = TRUE;
	    multiline++;
	    musthave (IN, CONTIN);
	    startl = TRUE;
	    t->left = caselist();
	    musthave (ESAC, 0);
	    multiline--;
	    break;

	case IF:
	    multiline++;
	    (t = (C_Op *)tree (sizeof (C_Op)))->type = TIF;
	    t->left = c_list (FALSE);
	    t->right = thenpart ();
	    musthave (FI, 0);
	    multiline--;
	    break;
    }

    while (synio (0))
	;

    t = namelist (t);
    iolist = iosave;
    return t;
}

static C_Op	*dogroup (onlydone)
int		onlydone;
{
    register int	c;
    register C_Op	*list;

    if (((c = yylex (CONTIN)) == DONE) && onlydone)
	return (C_Op *)NULL;

    if (c != DO)
	yyerror (syntax_err);

    list = c_list (FALSE);
    musthave (DONE, 0);
    return list;
}

static C_Op	*thenpart ()
{
    register int	c;
    register C_Op	*t;

    if ((c = yylex (0)) != THEN)
    {
	peeksym = c;
	return (C_Op *)NULL;
    }

    (t = (C_Op *)tree (sizeof (C_Op)))->type = 0;

    if ((t->left = c_list (FALSE)) == (C_Op *)NULL)
	yyerror (syntax_err);

    t->right = elsepart ();
    return t;
}

static C_Op	*elsepart ()
{
    register int	c;
    register C_Op	*t;

    switch (c = yylex (0))
    {
	case ELSE:
	    if ((t = c_list (FALSE)) == (C_Op *)NULL)
		yyerror (syntax_err);

	    return t;

	case ELIF:
	    (t = (C_Op *)tree (sizeof (C_Op)))->type = TELIF;
	    t->left = c_list (FALSE);
	    t->right = thenpart ();
	    return t;

	default:
	    peeksym = c;
	    return (C_Op *)NULL;
    }
}

static C_Op	*caselist()
{
    register C_Op	*t = (C_Op *)NULL;

    while ((peeksym = yylex (CONTIN)) != ESAC)
	t = list (t, casepart ());

    return t;
}

static C_Op	*casepart ()
{
    register C_Op	*t = (C_Op *)tree (sizeof (C_Op));

    t->type = TPAT;
    t->words = pattern ();
    musthave (')', 0);
    t->left = c_list (FALSE);

    if ((peeksym = yylex (CONTIN)) != ESAC)
	musthave (BREAK, CONTIN);

    return t;
}

static char	**pattern()
{
    register int	c, cf;

    cf = CONTIN;

    do
    {
	musthave (WORD, cf);
	word (yylval.cp);
	cf = 0;
    } while ((c = yylex(0)) == '|');

    peeksym = c;
    word (NOWORD);
    return copyw();
}

static char	**wordlist()
{
    register int	c;

    if ((c = yylex(0)) != IN)
    {
	peeksym = c;
	return (char **)NULL;
    }

    startl = FALSE;
    while ((c = yylex (0)) == WORD)
	word (yylval.cp);

    word (NOWORD);
    peeksym = c;

    return copyw();
}

/*
 * supporting functions
 */

static C_Op	*list (t1, t2)
register C_Op	*t1, *t2;
{
    if (t1 == (C_Op *)NULL)
	return t2;

    if (t2 == (C_Op *)NULL)
	return t1;

    return block (TLIST, t1, t2, NOWORDS);
}

static C_Op	*block (type, t1, t2, wp)
C_Op		*t1, *t2;
char		**wp;
{
    register C_Op *t = (C_Op *)tree (sizeof (C_Op));

    t->type = type;
    t->left = t1;
    t->right = t2;
    t->words = wp;
    return t;
}

static struct res {
    char	*r_name;
    int		r_val;
} restab[] = {
    {	"for",		FOR},		{"case",	CASE},
	{"esac",	ESAC},		{"while",	WHILE},
	{"do",		DO},		{"done",	DONE},
	{"if",		IF},		{"in",		IN},
	{"then",	THEN},		{"else",	ELSE},
	{"elif",	ELIF},		{"until",	UNTIL},
	{"fi",		FI},

	{";;",		BREAK},		{"||",		LOGOR},
	{"&&",		LOGAND},	{"{",		'{'},
	{"}",		'}'},

	{(char *)NULL,	0}
};

static int	rlookup (n)
register char	*n;
{
    register struct res		*rp = restab;

    while ((rp->r_name != (char *)NULL) && strcmp (rp->r_name, n))
	rp++;

    return rp->r_val;
}

static C_Op	*namelist(t)
register C_Op	*t;
{
    if (iolist)
    {
	iolist = addword ((char *)NULL, iolist);
	t->ioact = copyio ();
    }

    else
	t->ioact = (IO_Actions **)NULL;

    if ((t->type != TCOM) && (t->type != TFUNC))
    {
	if ((t->type != TPAREN) && (t->ioact != (IO_Actions **)NULL))
	{
	    t = block (TPAREN, t, NOBLOCK, NOWORDS);
	    t->ioact = t->left->ioact;
	    t->left->ioact = (IO_Actions **)NULL;
	}
    }

    else
    {
	word (NOWORD);
	t->words = copyw();
    }

    return t;
}

static char	**copyw ()
{
    register char **wd = getwords (wdlist);

    wdlist = (Word_B *)NULL;
    return wd;
}

static void	word (cp)
char		*cp;
{
    wdlist = addword (cp, wdlist);
}

static IO_Actions	**copyio ()
{
    IO_Actions	**iop = (IO_Actions **)getwords (iolist);

    iolist = (Word_B *)NULL;
    return iop;
}

static IO_Actions	*io (u, f, cp)
int			f, u;
char			*cp;
{
    register IO_Actions *iop = (IO_Actions *)tree (sizeof (IO_Actions));

    iop->io_unit = u;
    iop->io_flag = f;
    iop->io_name = cp;
    iolist = addword ((char *)iop, iolist);
    return iop;
}

static void	yyerror (s)
char		*s;
{
    yynerrs++;

    if (Interactive ())
    {
	multiline = 0;

	while ((eofc () == 0) && (yylex (0) != NL))
	    ;
    }

    print_error (s);
    fail ();
}

static int	yylex (cf)
int		cf;
{
    register int	c, c1;
    bool		atstart;

    if ((c = peeksym) > 0)
    {
	peeksym = 0;

	if (c == NL)
	    startl = TRUE;

	return c;
    }

    e.linep = e.cline;
    atstart = startl;
    startl = FALSE;
    yylval.i = 0;

loop:
    while ((c = Getc (0)) == SP || c == '\t')
	;

    switch (c)
    {
	default:
	    if (isdigit (c))
	    {
		unget (c1 = Getc(0));

		if ((c1 == '<') || (c1 == '>'))
		{
		    iounit = c - '0';
		    goto loop;
		}

		*e.linep++ = (char)c;
		c = c1;
	    }

	    break;

	case '#':
	    while ((c = Getc(0)) != 0 && (c != NL))
		;

	    unget(c);
	    goto loop;

	case 0:
	    return c;

	case '$':
	    *e.linep++ = (char)c;

	    if ((c = Getc(0)) == '{')
	    {
		if ((c = collect (c, '}')) != '\0')
		    return (c);

		goto pack;
	    }

	    break;

	case '`':
	case '\'':
	case '"':
	    if ((c = collect (c, c)) != '\0')
		return c;

	    goto pack;

	case '|':
	case '&':
	case ';':
	    if ((c1 = dual (c)) != '\0')
	    {
		startl = TRUE;
		return c1;
	    }

	case '(':
	case ')':
	    startl = TRUE;
	    return c;

	case '^':
	    startl = TRUE;
	    return '|';

	case '>':
	case '<':
	    diag (c);
	    return c;

	case NL:
	    gethere ();
	    startl = TRUE;

	    if (multiline || (cf & CONTIN))
	    {
		if (Interactive ())
		{
#ifndef NO_HISTORY
		    Add_History (FALSE);
#endif
		    put_prompt (ps2->value);
		}

		if (cf & CONTIN)
		    goto loop;
	    }

	    return(c);
    }

    unget (c);

pack:
    while (((c = Getc (0)) != 0) && (!any ((char)c, "`$ '\"\t;&<>()|^\n")))
    {
	if (e.linep >= e.eline)
	    print_error ("sh: word too long\n");

	else
	    *e.linep++ = (char)c;
    }

    unget (c);

    if (any ((char)c, spcl2))
	goto loop;

    *e.linep++ = '\0';

    if (atstart && (c = rlookup (e.cline)) != 0)
    {
	startl = TRUE;
	return c;
    }

    yylval.cp = strsave (e.cline, areanum);
    return WORD;
}

/* Read input until we read the specified end character */

int		collect (c, c1)
register int	c, c1;
{
    char *s = "x\n";

    *e.linep++ = (char)c;		/* Save the current character	*/

    while ((c = Getc (c1)) != c1)
    {
	if (c == 0) 			/* End of file - abort		*/
	{
	    unget (c);
	    *s = (char)c1;
	    S_puts ("sh: no closing ");
	    yyerror (s);
	    return YYERRCODE;
	}

	if (Interactive () && (c == NL))
	{
#ifndef NO_HISTORY
	    Add_History (FALSE);
#endif
	    put_prompt (ps2->value);
	}

	*e.linep++ = (char)c;
    }

    *e.linep++ = (char)c;
    return 0;
}

/* Check for &&, || and ;; */

static int	dual (c)
register int	c;
{
    char		s[3];
    register char	*cp = s;

/* Get the next character and set up double string.  Look up in valid
 * operators.  If invalid, unget character
 */

    *cp++ = (char)c;
    *cp++ = (char)Getc (0);
    *cp = 0;

    if ((c = rlookup (s)) == 0)
	unget (*--cp);

    return c;
}

/* Process I/O re-direction */

static void	diag (ec)
register int	ec;
{
    register int	c;

    if (((c = Getc (0)) == '>') || (c == '<'))
    {
	if (c != ec)
	    yyerror (syntax_err);

	yylval.i = (ec == '>') ? IOWRITE | IOCAT : IOHERE;
	c = Getc(0);
    }

    else
	yylval.i = (ec == '>') ? IOWRITE : IOREAD;

/* Check for >&, <& and <<- */

    if ((c == '-') && (yylval.i == IOHERE))
	yylval.i |= IOTHERE;

    else if ((c != '&') || (yylval.i == IOHERE))
	unget (c);

    else
	yylval.i |= IODUP;
}

/* Get a new tree leaf structure */

static char	*tree (size)
unsigned int	size;
{
    register char *t;

    if ((t = getcell (size)) == (char *)NULL)
    {
	S_puts ("sh: command line too complicated\n");
	fail ();
    }

    return t;
}
