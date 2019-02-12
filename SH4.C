/* MS-DOS SHELL - 'word' Interpretator
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
 *    $Header: sh4.c 1.7 90/06/21 11:11:51 MS_user Exp $
 *
 *    $Log:	sh4.c $
 * Revision 1.7  90/06/21  11:11:51  MS_user
 * Ensure Areanum is set correctly for memory areas
 *
 * Revision 1.6  90/04/25  22:35:26  MS_user
 * Make anys a global function
 *
 * Revision 1.5  90/03/27  20:33:41  MS_user
 * Clear extended file name on interrupt
 *
 * Revision 1.4  90/03/16  21:27:33  MS_user
 * Stop grave changing NL to SP for here documents
 *
 * Revision 1.3  90/03/16  11:50:41  MS_user
 * Correct Bug which prevents $$, $#, $!, $? and $- working.
 *
 * Revision 1.2  90/03/14  19:30:34  MS_user
 * Change subgetc for here document processing.  In particular `list`
 * processing.  I hope the detection method for this is right!
 *
 * Revision 1.1  90/01/25  13:41:38  MS_user
 * Initial revision
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dir.h>
#include <ctype.h>
#include "sh.h"

#define INCL_NOPM
#define INCL_DOS
#include <os2.h>

/*
 * ${}, `command`, blank interpretation, quoting and file name expansion
 */

#define	NSTART		16		/* default number of words to	*/
					/* allow for initially		*/
static Word_B		*C_EList;	/* For expand functions		*/
static Word_B		*New_Elist;
static char		*spcl  = "[?*";
static char		*spcl1 = "\"'";

static void		globname (char *, char *);
static bool		expand (char *, Word_B **, int);
static char		dollar (bool);
static bool		grave (bool);
static Word_B		*Expand_globs (char *, Word_B *);
static bool		anyspcl (Word_B *);
static char		*blank (int);
static char		*generate (char *, char *, char *, char *);
static char		*unquote (char *);
static Word_B		*newword (int);
static char		*anys_p (char *, char *);
static void		Glob_MDrives (char *, char *);
static char		*Check_Multi_Drive (char *);

/*
 * Expand all words to their full potential
 */

char		**eval(ap, f)
register char	**ap;
{
    Word_B	*wb = (Word_B *)NULL;
    char	**wp = (char **)NULL;
    char	**wf = (char **)NULL;
    jmp_buf	ev;

    if (newenv (setjmp (errpt = ev)) == FALSE)
    {
	while ((*ap != (char *)NULL) && isassign (*ap))
	    expand (*(ap++), &wb, f & ~DOGLOB);

	if (FL_TEST ('k'))
	{
	    for (wf = ap; *wf != (char *)NULL; wf++)
	    {
		if (isassign (*wf))
		    expand (*wf, &wb, f & ~DOGLOB);
	    }
	}

/* Now expand the words */

	for (wb = addword ((char *)NULL, wb); *ap; ap++)
	{
	    if (!FL_TEST ('k') || !isassign(*ap))
		expand (*ap, &wb, f & ~DOKEY);
	}

/* Get the word list */

	wp = getwords (wb = addword ((char *)NULL, wb));
	quitenv ();
    }

    else
	gflg = 1;

    return gflg ? (char **)NULL : wp;
}

/*
 * Make the exported environment from the exported names in the dictionary.
 * Keyword assignments will already have been done.  Convert to MSDOS
 * format if flag set and m enabled
 */

char	**makenv ()
{
    register Word_B	*wb = (Word_B *)NULL;
    register Var_List	*vp;
    char		*cp, *sp;
    int			len = 0;

    for (vp = vlist; vp != (Var_List *)NULL; vp = vp->next)
    {
	if (vp->status & EXPORT)
	{
	    if ((len += (strlen (vp->name) + 1)) >= 0x7f00)
		return (char **)NULL;

	    wb = addword (vp->name, wb);

/* If MSDOS mode, we need to copy the variable, convert / to \ and put
 * the copy in the environment list instead
 */

	    if (FL_TEST ('m') && (vp->status & C_MSDOS))
	    {
		cp = strsave (wb->w_words[wb->w_nword - 1], areanum);
		wb->w_words[wb->w_nword - 1] = cp;
		Convert_Slashes (cp);
	    }
	}
    }

    return getwords (wb = addword ((char *)NULL, wb));
}

char		*evalstr(cp, f)
register char	*cp;
int		f;
{
    Word_B	*wb = (Word_B *)NULL;

    if (expand (cp, &wb, f))
    {
	if ((wb == (Word_B *)NULL) || (wb->w_nword == 0) ||
	    ((cp = wb->w_words[0]) == (char *)NULL))
	    cp = null;

	DELETE (wb);
    }

    else
	cp = (char *)NULL;

    return cp;
}

/* Expand special characters and variables */

static bool		expand (cp, wbp, f)
register char		*cp;			/* String to process	*/
register Word_B		**wbp;			/* Word block		*/
int			f;			/* Expand mode		*/
{
    jmp_buf	ev;

    gflg = 0;

    if (cp == (char *)NULL)
	return FALSE;

/* If there are no special characters and no separators, nothing to do,
 * just save the word
 */

    if (!anys (spcl2, cp) && !anys (ifs->value, cp) &&
	((f & DOGLOB) == 0 || !anys (spcl, cp)))
    {
	cp = strsave (cp, areanum);

	if (f & DOTRIM)
	    unquote (cp);

	*wbp = addword (cp, *wbp);
	return TRUE;
    }

/* Set up to read the word back in */

    if (newenv (setjmp (errpt = ev)) == FALSE)
    {
	PUSHIO (aword, cp, strchar);
	e.iobase = e.iop;

	while ((cp = blank (f)) && gflg == 0)
	{
	    e.linep = cp;
	    cp = strsave (cp, areanum);

/* Global expansion disabled ? */

	    if (((f & DOGLOB) == 0) || FL_TEST ('f'))
	    {
		if (f & DOTRIM)
		    unquote (cp);

		*wbp = addword (cp, *wbp);
	    }

	    else
		*wbp = Expand_globs (cp, *wbp);
	}

	quitenv ();
    }

    else
	gflg = 1;

    return (gflg == 0) ? TRUE : FALSE;
}

/*
 * Blank interpretation and quoting
 */

static char	*blank(f)
{
    register int	c, c1;
    register char	*sp = e.linep;
    int			scanequals = (f & DOKEY) ? TRUE : FALSE;
    bool		foundequals = FALSE;

loop:
    switch (c = subgetc ('"', foundequals))
    {
	case 0:
	    if (sp == e.linep)
		return (char *)NULL;

	    *e.linep++ = 0;
	    return sp;

	default:
	    if ((f & DOBLANK) && any ((char)c, ifs->value))
		goto loop;

	    break;

	case '"':
	case '\'':
	    scanequals = FALSE;
	    if (INSUB())
		break;

	    for (c1 = c; (c = subgetc ((char)c1, TRUE)) != c1;)
	    {
		if (c == 0)
		    break;

		if ((c == '\'') || !any ((char)c, "$`\""))
		    c |= QUOTE;

		*e.linep++ = (char)c;
	    }

	    c = 0;
    }

    unget(c);

    if (!isalpha (c))
	scanequals = FALSE;

    while (1)
    {
	if (((c = subgetc ('"', foundequals)) == 0) ||
	    (f & DOBLANK) && any ((char)c, ifs->value) ||
	    !INSUB() && any ((char)c, spcl1))
	{
	    scanequals = FALSE;
	    unget (c);

	    if (any ((char)c, spcl1))
		goto loop;

	    break;
	}

	if (scanequals)
	{
	    if (c == '=')
	    {
		foundequals = TRUE;
		scanequals  = FALSE;
	    }

	    else if (!isalnum (c))
		scanequals = FALSE;
	}

	*e.linep++ = (char)c;
    }

    *e.linep++ = 0;
    return sp;
}

/*
 * Get characters, substituting for ` and $
 */

int		subgetc (ec, quoted)
register char	ec;
bool		quoted;
{
    register char	c;

    while (1)
    {
	c = (char)Getc (ec);

	if (!INSUB() && ec != '\'')
	{

/* Found a ` - execute the command */

	    if (c == '`')
	    {

/* If both ec (end character) is zero and quoted flag is FALSE, this is execute
 * command request is in a here document, so we have to collect the rest of
 * the command from input.  Otherwise, the command is in e.iop->argp->aword.
 *
 * We also need to set quoted so that NL are not processed when reading
 * the output from the command.
 */
		if (!ec && !quoted)
		{
		    e.linep = e.cline;
		    if (collect (c, c) != 0)
			return 0;

		    e.iop->argp->aword = e.cline + 1;
		    quoted = MAYBE;
		}

		if (grave (quoted) == 0)
		    return 0;

/* Re-read the character from the Grave function */

		e.iop->task = XGRAVE;
	    }

/* $ - check for environment variable subsitution */

	    else if (c == '$' && (c = dollar (quoted)) == 0)
		e.iop->task = XDOLL;

/* No special processing required - return the character */

	    else
		return c;
	}

	else
	    return c;
    }
}

/*
 * Prepare to generate the string returned by ${} substitution.
 */

static char	dollar (quoted)
bool		quoted;
{
    IO_State		*oiop;
    char		*dolp, otask;
    register char	*s, c, *cp;
    Var_List		*vp;
    bool		colon_f = FALSE;
    char		*dol_special = "$ ";

    c = (char)readc ();
    s = e.linep;

/* Bracketed or not ? */

    if (c != '{')
    {

/* Get the string, while it is a alpha character */

	*e.linep++ = c;

	if (isalpha (c))
	{
	    while (((c = (char)readc ()) != 0) && isalnum (c))
	    {
		if (e.linep < e.eline)
			*e.linep++ = c;
	    }

	    unget(c);
	}

	c = 0;
    }

/* Bracketed - special case */

    else
    {
	oiop = e.iop;
	otask = e.iop->task;
	e.iop->task = XOTHER;

	while (((c = (char)subgetc ('"', FALSE)) != 0) &&
	       (c != '}') && (c != NL))
	{
	    if (e.linep < e.eline)
		*e.linep++ = c;
	}

	if (oiop == e.iop)
	    e.iop->task = otask;

/* Check terminate correctly */

	if (c != '}')
	{
	    print_error ("sh: unclosed ${\n");
	    gflg++;
	    return c;
	}

/* Check for zero length string */

	if (s == e.linep)
	{
	    print_error ("sh: bad substitution\n");
	    gflg++;
	    return c;
	}
    }

/* Check line length */

    if (e.linep >= e.eline)
    {
	print_error ("sh: string in ${} too long\n");
	gflg++;
	e.linep -= 10;
    }

    *e.linep = 0;

/* Scan for =-+? in string */

    if (*s)
    {
	for (cp = s + 1; *cp; cp++)
	{

/* Check for end character other than null (=-+?) */

	    if (any (*cp, "=-+?"))
	    {
		c = *cp;

/* Check for case of :[=-+?].  If found - set flag */

		if (*(cp - 1) == ':')
		{
		    colon_f = TRUE;
		    *(cp - 1) = 0;
		}

		*(cp++) = 0;
		break;
	    }
	}
    }

/* Check for * and @ processing */

    if (s[1] == 0 && (*s == '*' || *s == '@'))
    {
	if (dolc > 1)
	{
	    e.linep = s;
	    PUSHIO (awordlist, dolv + 1, dol_char);
	    e.iop->dflag = (char)(!quoted ? DSA_NULL
					  : ((*s == '*') ? DSA_STAR : DSA_AMP));
	    return 0;
	}

/* trap the nasty ${=} */

	else
	{
	    s[0] = '1';
	    s[1] = 0;
	}
    }

/* Find the current value
 *
 * $~xxx variables are used by the Shell internally and cannot be accessed
 * by the user.
 */

    if (*s == '~')
	dolp = null;

    else if (!*s || !(isalnum (*s) || any (*s, "#-?$!")))
    {
	dol_special[1] = *s;
	dolp = dol_special;
    }

    else if ((dolp = (vp = lookup (s, FALSE))->value) == null)
    {
	switch (c)
	{
	    case '=':
		if (isdigit (*s))
		{
		    print_error ("sh: cannot use ${...=...} with $n\n");
		    gflg++;
		    break;
		}

		setval ((vp = lookup (s, TRUE)), cp);
		dolp = vp->value;
		break;

	    case '-':
		dolp = strsave (cp, areanum);
		break;

	    case '?':
		if (*cp == 0)
		    cp = "parameter null or not set";

		print_error ("%s: %s\n", s, cp);

		gflg++;
		break;
	}
    }

    else if (c == '+')
	dolp = strsave (cp, areanum);

/* Check for unset values */

    if (FL_TEST ('u') && dolp == null)
    {
	print_error ("sh: unset variable %s\n", s);
	gflg++;
    }

    e.linep = s;
    PUSHIO (aword, dolp, quoted ? qstrchar : strchar);
    return 0;
}

/*
 * Run the command in `...` and read its output.
 */

static bool	grave (quoted)
bool		quoted;
{
    char		*cp, *sp;
    int			localpipe, rv;
    jmp_buf		ev, rt;
    C_Op		*outtree;
    Break_C		bc;
    int			(*iof)(IO_State *);

/* Save area */

    long		s_flags = flags;
    Word_B		*s_wdlist = wdlist;
    Word_B		*s_iolist = iolist;
    Break_C		*S_RList = Return_List;	/* Save loval links	*/
    Break_C		*S_BList = Break_List;
    Break_C		*S_SList = SShell_List;
    int			*s_fail = failpt;
    int			s_execflg = execflg;
    int			Local_depth;

/* Check there is an ending grave */

    if ((cp = strchr (e.iop->argp->aword, '`')) == (char *)NULL)
    {
	print_error ("sh: no closing `\n");
	return FALSE;
    }

/* Create the pipe to read the output from the command string */

    if ((localpipe = openpipe ()) < 0)
	return FALSE;

/* Terminate string and initialise save area */

    *cp = 0;

/* Create a new environment */

    S_dup2 (localpipe, 1);

    FL_CLEAR ('e');
    FL_CLEAR ('v');
    FL_CLEAR ('n');

    sp = strsave (e.iop->argp->aword, areanum++);
    unquote (sp);

/* Set up new environment */

    Local_depth = Execute_stack_depth++;
    rv = Create_NG_VL ();

    if ((rv != -1) && (newenv (setjmp (errpt = ev)) == FALSE))
    {
	Return_List = (Break_C *)NULL;
	Break_List  = (Break_C *)NULL;
	wdlist	    = (Word_B *)NULL;
	wdlist	    = (Word_B *)NULL;
	iolist	    = (Word_B *)NULL;

	PUSHIO (aword, sp, nlchar);
	e.cline = space (LINE_MAX);
	e.eline = e.cline + LINE_MAX - 5;
	e.linep = e.cline;
	e.iobase = e.iop;

/* Clear interrupt, error, multiline, parse and execute flags.  */

	SW_intr = 0;
	yynerrs = 0;
	multiline = 0;
	inparse = 0;
	execflg = 1;

/* Parse the line and execute it */

	if ((setjmp (failpt = rt) == 0) &&
	    ((outtree = yyparse ()) != (C_Op *)NULL))
	{
	    if (setjmp (bc.brkpt) == 0)
	    {
		bc.nextlev = SShell_List;
		SShell_List = &bc;
		execute (outtree, NOPIPE, NOPIPE, 0);
	    }
	}

/* Clean up any files around we nolonger need */

	Clear_Extended_File ();
	quitenv ();
    }

/* Fail - close pipe and delete it */

    else
    {
	S_Delete (localpipe);
	S_close (localpipe, TRUE);
    }

/* Restore environment */

    Restore_Environment (0, Local_depth);

/* Free old space */

    freehere (areanum);
    freearea (areanum--);	/* free old space */

/* Ok - completed processing - restore environment and read the pipe */

    execflg	= s_execflg;
    flags	= s_flags;
    wdlist	= s_wdlist;
    iolist	= s_iolist;
    failpt	= s_fail;
    Return_List = S_RList;
    Break_List	= S_BList;
    SShell_List = S_SList;

/* Move pipe to start so we can read it */

    *(cp++) = '`';
    lseek (localpipe, 0L, SEEK_SET);
    e.iop->argp->aword = cp;
    iof = (!quoted) ? gravechar
		    : ((quoted == MAYBE) ? sgravechar : qgravechar);
    PUSHIO (afile, remap (localpipe), iof);
    return TRUE;
}

/*
 * Remove Quotes from a string
 */

static char	*unquote (as)
register char	*as;
{
    register char	*s;

    if ((s = as) != (char *)NULL)
    {
	while (*s)
	    *(s++) &= ~QUOTE;
    }

    return as;
}

/*
 * Expand *, [] and ?
 */

static Word_B	*Expand_globs (cp, wb)
char		*cp;
Word_B		*wb;
{
    register int	i = 0;
    register char	*pp;

/* Ignore null strings */

    if (cp == (char *)NULL)
	return wb;

/* Any special characters */

    for (pp = cp; *pp; pp++)
    {
	if (any (*pp, spcl))
	    i++;

	else if (!any (*pp & ~QUOTE, spcl))
	    *pp &= ~QUOTE;
    }

/* No - just add the word to the selected block */

    if (i == 0)
	return addword (unquote (cp), wb);

/* OK - we have to expand the word whilst any words in cl have special
 * characters in them
 */

    for (C_EList = addword (strsave (cp, areanum), (Word_B *)NULL);
	 anyspcl (C_EList); C_EList = New_Elist)
    {

/* Get a new block for this pass of the expansion */

	New_Elist = newword (C_EList->w_nword * 2);

/* For each word, expand it */

	for (i = 0; i < C_EList->w_nword; i++)
	{
	    if ((pp = anys_p (C_EList->w_words[i], spcl)) != (char *)NULL)
		Glob_MDrives (C_EList->w_words[i], pp);

	    else
		New_Elist = addword (strsave (C_EList->w_words[i], areanum),
				     New_Elist);
	}

/* The current list is now the previous list, so delete it */

	for (i = 0; i < C_EList->w_nword; i++)
	    DELETE (C_EList->w_words[i]);

	DELETE (C_EList);
    }

    for (i = 0; i < C_EList->w_nword; i++)
	unquote (C_EList->w_words[i]);

    qsort (C_EList->w_words, C_EList->w_nword, sizeof (char *), sort_compare);

/* Did we find any files matching the specification.  Yes - add them to
 * the block
 */

    if (C_EList->w_nword)
    {
	for (i = 0; i < C_EList->w_nword; i++)
	    wb = addword (C_EList->w_words[i], wb);

	DELETE (C_EList);
	return wb;
    }

/* No - add the original word */

    else
	return addword (unquote (cp), wb);
}

/*
 * Read a directory for matches against the specified name
 */

static void	globname (we, pp)
char		*we;			/* Start			*/
register char	*pp;			/* First special character	*/
{
    register char	*np, *cp;
    char		*name, *gp, *dp;
    DIR			*dn;
    struct direct	*d_ce;
    char		dname[NAME_MAX + 1];
    struct stat		dbuf;

/* Find the previous directory separator */

    for (np = we; np != pp; pp--)
    {
	if (pp[-1] == '/')
	    break;
    }

/* If we don't find it, check for a drive */

    if ((np == pp) && (strlen (we) > 2) && (we[1] == ':'))
	pp += 2;

/* Save copy of directory name */

    for (dp = cp = space ((int)(pp - np) + 3); np < pp;)
	*cp++ = *np++;

    *cp++ = '.';
    *cp = '\0';

/* Save copy of pattern for this directory.  NP is left pointing to the
 * rest of the string for any subdirectories
 */

    for (gp = cp = space (strlen (pp) + 1); *np && *np != '/';)
	*cp++ = *np++;

    *cp = '\0';

/* Open the directory */

    if ((dn = opendir (dp)) == (DIR *)NULL)
    {
	DELETE (dp);
	DELETE (gp);
	return;
    }

/* Scan for matches */

    while ((d_ce = readdir (dn)) != (struct direct *)NULL)
    {
	if ((*(strcpy (dname, d_ce->d_name)) == '.') && (*gp != '.'))
	    continue;

	for (cp = dname; *cp; cp++)
	{
	    if (any (*cp, spcl))
		*cp |= QUOTE;
	}

/* Check for a match */

	if (gmatch (dname, gp, TRUE))
	{

/* If there are no special characters in the new full name, the file must
 * exist
 */

	    name = generate (we, pp, dname, np);

	    if (*np && !anys (np, spcl))
	    {
		if (stat (name, &dbuf))
		{
		    DELETE (name);
		    continue;
		}
	    }

/* Ok save the name */

	    New_Elist = addword (name, New_Elist);
	}
    }

    closedir (dn);
    DELETE (dp);
    DELETE (gp);
}

/*
 * generate a pathname as below.  start..end1 / middle end.  The slashes come
 * for free
 */

static char	*generate (start1, end1, middle, end)
char		*start1;
register char	*end1;
char		*middle, *end;
{
    register char	*op;
    int			clen = (int)(end1 - start1);

    op = space (clen + strlen (middle) + strlen (end) + 2);

    strncpy (op, start1, clen);
    strcat (strcpy (&op[clen], middle), end);
    return op;
}

/*
 * Scan a Word Block for special characters
 */

static bool	anyspcl (wb)
register Word_B	*wb;
{
    register int	i;
    register char	**wd = wb->w_words;

    for (i = 0; i < wb->w_nword; i++)
    {
	if (anys (spcl, *wd++))
	    return TRUE;
    }

    return FALSE;
}

/*
 * Create a new Word Block
 */

static Word_B	*newword (nw)
register int	nw;
{
    register Word_B	*wb;

    wb = (Word_B *) space (sizeof (Word_B) + nw * sizeof (char *));
    wb->w_bsize = nw;
    wb->w_nword = 0;

    return wb;
}

/*
 * Add a new word to a Word Block or list
 */

Word_B		*addword (wd, wb)
char		*wd;
register Word_B	*wb;
{
    register Word_B	*wb2;
    register int	nw;

    if (wb == (Word_B *)NULL)
	wb = newword (NSTART);

/* Do we require more space ? */

    if ((nw = wb->w_nword) >= wb->w_bsize)
    {
	wb2 = newword (nw * 2);
	memcpy ((char *)wb2->w_words, (char *)wb->w_words, nw*sizeof(char *));
	wb2->w_nword = nw;
	DELETE (wb);
	wb = wb2;
    }

/* Add to the list */

    wb->w_words[wb->w_nword++] = wd;
    return wb;
}

/*
 * Convert a word block structure into a array of strings
 */

char		**getwords(wb)
register Word_B	*wb;
{
    register char	**wd;
    register nb;

/* If the word block is empty or does not exist, return no list */

    if (wb == (Word_B **)NULL)
	return (char *)NULL;

    if (wb->w_nword == 0)
    {
	DELETE (wb);
	return (char *)NULL;
    }

/* Get some space for the array and set it up */

    wd = (char **)space (nb = sizeof (char *) * wb->w_nword);

    memcpy ((char *)wd, (char *)wb->w_words, nb);
    DELETE (wb);	/* perhaps should done by caller */
    return wd;
}

/*
 * Is any character from s1 in s2?  Return a boolean.
 */

bool		anys (s1, s2)
register char	*s1, *s2;
{
    while (*s1)
    {
	if (any (*(s1++), s2))
	    return TRUE;
    }

    return FALSE;
}

/*
 * Is any character from s1 in s2? Yes - return a pointer to that
 * character.
 */

static char	*anys_p (s1, s2)
register char	*s1, *s2;
{
    while (*s1)
    {
	if (any (*(s1++), s2))
	    return --s1;
    }

    return (char *)NULL;
}

/*
 * Expansion - check for multiple drive request
 *
 * If there is a multi-drive expansion (*:, ?: or []:), we have to check
 * out each existing drive and then expand.  So we check for a multi-drive
 * condition and then for each existing drive, we check that pattern
 * against the drive and then expand the rest of the pattern.
 *
 * Otherwise, we just expand the pattern.
 */

static void	Glob_MDrives (pattern, start)
char		*pattern;
char		*start;
{
    unsigned int	c_drive;	/* Current drive		*/
    char		*multi;		/* Multi-drive flag		*/
    static char		*t_drive = "x";
    char		*new_pattern;
    ULONG               l_map;
    int                 cnt;

/* Search all drives ? */

    if ((multi = Check_Multi_Drive (pattern)) != (char *)NULL)
    {
        DosQCurDisk((PUSHORT) &c_drive, &l_map);

	new_pattern = space (strlen (multi) + 2);
	strcpy (new_pattern + 1, multi);

        for ( cnt = 1; cnt <= 26; cnt++, l_map >>= 1 )
          if ( l_map & 1L )
	  {
	      *t_drive = (char)(cnt + 'a' - 1);
              *multi = 0;

	      if (gmatch (t_drive, pattern, TRUE))
	      {
	  	*new_pattern = *t_drive;
                *multi = ':';
	  	globname (new_pattern, strchr(new_pattern,0));
	      }
              else
                *multi = ':';
	  }

/* Restore and delete space */

	DELETE (new_pattern);
    }

/* No drive specifier - just check it out */

    else
	globname (pattern, start);
}

/*
 * Check for multi_drive prefix - *:, ?: or []:
 *
 * Return NULL or the address of the colon character
 */

static char	*Check_Multi_Drive (pattern)
char		*pattern;
{
    if (strlen (pattern) < 3)
	return (char *)NULL;

    if (((*pattern == '*') || (*pattern == '?')) && (pattern[1] == ':'))
	return pattern + 1;

    if (*pattern != '[')
	return (char *)NULL;

    while (*pattern && (*pattern != ']'))
    {
	if ((*pattern == '\\') && (*(pattern + 1)))
	    ++pattern;

	++pattern;
    }

    return (*pattern && (*(pattern + 1) == ':')) ? pattern + 1 : (char *)NULL;
}
