/* MS-DOS SHELL - Main I/O Functions
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
 *    $Header: sh5.c 1.9 90/05/11 18:45:40 MS_user Exp $
 *
 *    $Log:	sh5.c $
 * Revision 1.9  90/05/11  18:45:40  MS_user
 * Fix problem when at end of buffer on re-load from file
 *
 * Revision 1.8  90/04/25  10:58:41  MS_user
 * Fix re-reading re-assigned buffers correctly.
 *
 * Revision 1.7  90/04/25  09:20:08  MS_user
 * Fix lseek problem and TAG problem on here documents
 *
 * Revision 1.6  90/04/09  17:04:50  MS_user
 * g_tempname must check for slash or backslash
 *
 * Revision 1.5  90/03/21  14:03:47  MS_user
 * Add new gravechar procedure for handling here documents
 *
 * Revision 1.4  90/03/14  19:31:28  MS_user
 * Add buffered output for here document processing.
 * Fix here document processing so it works correctly.
 * Add missing IOTHERE (<<-) processing for here documents.
 *
 * Revision 1.3  90/03/06  16:49:58  MS_user
 * Add disable history option
 *
 * Revision 1.2  90/03/05  13:51:45  MS_user
 * Add functionality to readc to support dot command via run function
 * Add $HOME as a temporary file directory
 *
 * Revision 1.1  90/01/25  13:41:50  MS_user
 * Initial revision
 *
 */

#include <sys/types.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <unistd.h>
#include "sh.h"

/*
 * shell IO
 */

static IO_Buf		sharedbuf = {AFID_NOBUF};
static IO_Buf		mainbuf = {AFID_NOBUF};
static unsigned int	bufid = AFID_ID;	/* buffer id counter */
					/* list of hear docs while parsing */
static Here_D	*inhere = (Here_D *)NULL;
					/* list of active here documents */
static Here_D	*acthere = (Here_D *)NULL;

static int	dol1_char (IO_State *);
static void	readhere (char **, char *, int, int);
static int	herechar (IO_State *);

int		Getc (ec)
register int	ec;
{
    register int	c;

    if (e.linep > e.eline)
    {
	while (((c = readc ()) != NL) && c)
	    ;

	print_error ("sh: input line too long\n");
	gflg++;
	return c;
    }

    c = readc();
    if ((ec != '\'') && (ec != '`') && (e.iop->task != XGRAVE))
    {
	if (c == '\\')
	{
	    if (((c = readc ()) == NL) && (ec != '\"'))
		return Getc (ec);

	    c |= QUOTE;
	}
    }

    return c;
}

void	unget (c)
int	c;
{
    if (e.iop >= e.iobase)
	e.iop->peekc = c;
}

int	eofc ()
{
    return (e.iop < e.iobase) || ((e.iop->peekc == 0) && (e.iop->prev == 0));
}

/* Read the next character */

int	readc ()
{
    register int	c;
    char		s_dflag = e.iop->dflag;

/* The dflag is transfered from the higher level to the lower level at end
 * of input at the higher level.  This is part of the implementation of
 * $* and $@ processing.
 */

    for (; e.iop >= e.iobase; e.iop--)
    {

/* Set up the current dflag */

	e.iop->dflag = s_dflag;

/* If there is an unget character, use it */

	if ((c = e.iop->peekc) != '\0')
	{
	    e.iop->peekc = 0;
	    return c;
	}

/* Some special processing for multi-line commands */

	else
	{
	    if (e.iop->prev != 0)
	    {

/* Get the next character from the IO function */

		if ((c = (*e.iop->iofn)(e.iop)) != '\0')
		{

/* End of current level, but continue at this level as another read
 * function has been put on the stack
 */

		    if (c == -1)
		    {
			e.iop++;
			continue;
		    }

/* If we are at the bottom - echo the character */

		    if ((e.iop == iostack) && (FL_TEST ('v')))
			S_putc ((char)c);

/* Return the current character */

		    return (e.iop->prev = (char)c);
		}

		else if (e.iop->task == XIO && e.iop->prev != NL)
		{
		    e.iop->prev = 0;

		    if ((e.iop == iostack) && (FL_TEST ('v')))
			S_putc (NL);

		    return NL;
		}

		else
		    s_dflag = e.iop->dflag;
	    }

	    if (e.iop->task == XIO)
	    {
		if (multiline)
		    return e.iop->prev = 0;

		if (talking && (e.iop == iostack + 1) && !e.eof_p)
		    put_prompt (ps1->value);
	    }
	}
    }

/* End of file detected.  If more data on stack and the special EOF
 * processing is not enabled - return 0
 */

    if ((e.iop >= iostack) && !e.eof_p)
	return 0;

    leave();
    /* NOTREACHED */
}

/* Add an Input channel to the input stack */

void		pushio (argp, fn)
IO_Args		*argp;
int		(*fn)(IO_State *);
{
    if (++e.iop >= &iostack[NPUSH])
    {
	e.iop--;
	print_error ("sh: Shell input nested too deeply\n");
	gflg++;
	return;
    }

    e.iop->iofn = fn;

    if (argp->afid != AFID_NOBUF)
	e.iop->argp = argp;

    else
    {
	e.iop->argp  = ioargstack + (e.iop - iostack);
	*e.iop->argp = *argp;
	e.iop->argp->afbuf = e.iop == &iostack[0] ? &mainbuf : &sharedbuf;

	if ((isatty (e.iop->argp->afile) == 0) &&
	    ((e.iop == &iostack[0]) ||
	     (lseek (e.iop->argp->afile, 0L, SEEK_CUR) != -1L)))
	{
	    if (++bufid == AFID_NOBUF)
		bufid = AFID_ID;

	    e.iop->argp->afid  = bufid;
	}
    }

    e.iop->prev  = ~NL;
    e.iop->peekc = 0;
    e.iop->xchar = 0;
    e.iop->nlcount = 0;

    if ((fn == filechar) || (fn == linechar))
	e.iop->task = XIO;

    else if ((fn == gravechar) || (fn == qgravechar) || (fn == sgravechar))
	e.iop->task = XGRAVE;

    else
	e.iop->task = XOTHER;
}

/*
 * Input generating functions
 */

/*
 * Produce the characters of a string, then a newline, then EOF.
 */
int			nlchar (iop)
register IO_State	*iop;
{
    register int	c;

    if (iop->argp->aword == (char *)NULL)
	return 0;

    if ((c = *iop->argp->aword++) == 0)
    {
	iop->argp->aword = (char *)NULL;
	return NL;
    }

    return c;
}

/*
 * Given a list of words, produce the characters
 * in them, with a space after each word.
 */

int			wdchar (iop)
register IO_State	*iop;
{
    register char	c;
    register char	**wl;

    if ((wl = iop->argp->awordlist) == (char **)NULL)
	return 0;

    if (*wl != (char *)NULL)
    {
	if ((c = *(*wl)++) != 0)
	    return (c & 0177);

	iop->argp->awordlist++;
	return SP;
    }

    iop->argp->awordlist = (char **)NULL;
    return NL;
}

/*
 * Return the characters of a list of words, producing a space between them.
 */

int			dol_char (iop)
IO_State		*iop;
{
    register char	*wp;
    char		cflag;

    if ((wp = *(iop->argp->awordlist)++) != (char *)NULL)
    {
	if (*iop->argp->awordlist == (char *)NULL)
	    iop->dflag |= DSA_END;

	cflag = iop->dflag;
	PUSHIO (aword, wp, dol1_char);
	e.iop->dflag = cflag;
	return -1;
    }

    return 0;
}

/* Return next character from the word with a space at the end */

static int		dol1_char (iop)
IO_State		*iop;
{
    register int c;

    if ((iop->dflag & DSA_MODE) == DSA_AMP)
    {
	if (!(iop->dflag & DSA_START))
	    iop->dflag |= DSA_START;

/* Has the previous word ended */

	else if (iop->dflag & DSA_START1)
	{
	    iop->dflag &= ~DSA_START1;
	    return '"';
	}
    }

    if (iop->argp->aword == (char *)NULL)
	return 0;

    if ((c = *iop->argp->aword) == '\0')
    {
	if ((iop->dflag & DSA_MODE) != DSA_AMP)
	{
	    iop->argp->aword = (char *)NULL;
	    return (iop->dflag & DSA_END) ? 0 : SP;
	}

	if (!(iop->dflag & DSA_END1))
	{
	    iop->dflag |= DSA_END1;
	    return '"';
	}

	iop->argp->aword = (char *)NULL;
	iop->dflag &= ~DSA_END1;
	iop->dflag |= DSA_START1;
	return (iop->dflag & DSA_END) ? 0 : SP;
    }

    iop->argp->aword++;
    if ((iop->dflag != DSA_NULL) && any ((char)c, ifs->value))
	c |= QUOTE;

    return c;
}

/*
 * Produce the characters from a single word (string).
 */

int		strchar (iop)
IO_State	*iop;
{
    register int	c;

    return ((iop->argp->aword == (char *)NULL) ||
	    ((c = *(iop->argp->aword++)) == 0)) ? 0 : c;
}

/*
 * Produce quoted characters from a single word (string).
 */

int		qstrchar (iop)
IO_State	*iop;
{
    register int	c;

    return ((iop->argp->aword == (char *)NULL) ||
	    ((c = *(iop->argp->aword++)) == 0)) ? 0 : (c | QUOTE);
}

/*
 * Return the characters from a file.
 */

int		filechar (iop)
IO_State	*iop;
{
    register IO_Args	*ap = iop->argp;
    register int	i;
    char		c;
    IO_Buf		*bp = ap->afbuf;

    if (ap->afid != AFID_NOBUF)
    {

/* When we reread a buffer, we need to check to see if we have reached the
 * end.  If we have, we need to read the next buffer.  Hence, this loop for
 * the second read
 */

	while (((i = (ap->afid != bp->id)) || (bp->bufp == bp->ebufp)))
	{

/* Are we re-reading a corrupted buffer? */

	    if (i)
		lseek (ap->afile, ap->afpos, SEEK_SET);

/* No, filling so set offset to zero */

	    else
		ap->afoff = 0;

/* Save the start of the next buffer */

	    ap->afpos = lseek (ap->afile, 0L, SEEK_CUR);

/* Read in the next buffer */

	    if ((i = read (ap->afile, bp->buf, sizeof (bp->buf))) <= 0)
	    {
		if (ap->afile > STDERR_FILENO)
		    S_close (ap->afile, TRUE);

		return 0;
	    }

/* Set up buffer id, start and end */

	    bp->id    = ap->afid;
	    bp->bufp  = bp->buf + ap->afoff;
	    bp->ebufp = bp->buf + i;
	}

/* Return the next character from the buffer */

	++(ap->afoff);
	return *bp->bufp++ & 0177;
    }

/* If this is the terminal, there is special input processing */

#ifndef NO_HISTORY
    else if ((ap->afile == 0) && isatty (ap->afile))
        return Get_stdin (ap);
#endif

    if ((i = read (ap->afile, &c, sizeof(c))) == sizeof (c))
	return (int)c & 0177;

    if (ap->afile > STDERR_FILENO)
	S_close (ap->afile, TRUE);

    return 0;
}

/*
 * Return the characters from a here temp file.
 */

static int		herechar (iop)
register IO_State	*iop;
{
    char			c;

    if (read (iop->argp->afile, &c, sizeof(c)) != sizeof(c))
    {
	S_close (iop->argp->afile, TRUE);
	c = 0;
    }

    return c;
}

/*
 * Return the characters produced by a process (`...`).
 * De-quote them if required.  Use in here documents.
 */

int		sgravechar (iop)
IO_State	*iop;
{
    return  qgravechar (iop) & ~QUOTE;
}

/*
 * Return the characters produced by a process (`...`).
 * De-quote them if required, and converting NL to space.
 */

int		gravechar (iop)
IO_State	*iop;
{
    register int c;

    if ((c = qgravechar (iop) & ~QUOTE) == NL)
	c = SP;

    return c;
}

/*
 * Process input from a `...` string
 */

int		qgravechar (iop)
IO_State	*iop;
{
    register int	c;

    if (iop->xchar)
    {
	if (iop->nlcount)
	{
	    iop->nlcount--;
	    return (NL | QUOTE);
	}

	c = iop->xchar;
	iop->xchar = 0;
    }

    else if ((c = filechar (iop)) == NL)
    {
	iop->nlcount = 1;

	while ((c = filechar (iop)) == NL)
	    iop->nlcount++;

	iop->xchar = (char)c;

	if (c == 0)
	    return c;

	iop->nlcount--;
	c = NL;
    }

    return (c != 0) ? (c | QUOTE) : 0;
}

/*
 * Return a single command (usually the first line) from a file.
 */

int		linechar (iop)
IO_State	*iop;
{
    register int	c;

    if ((c = filechar (iop)) == NL)
    {
	if (!multiline)
	{
	    if (iop->argp->afile > STDERR_FILENO)
		S_close (iop->argp->afile, TRUE);

	    iop->argp->afile = -1;	/* illegal value */
	}
    }

    return c;
}

void	closeall ()
{
    register int	u;

    for (u = NUFILE; u < NOFILE;)
	S_close (u++, TRUE);
}

/*
 * remap fd into Shell's fd space
 */

int		remap (fd)
register int	fd;
{
    register int	i;
    register int	n_io = 0;
    int			map[NOFILE];
    int			o_fd = fd;

    if (fd < e.iofd)
    {
	do
	{
	    map[n_io++] = fd;
	    fd = dup (fd);

	} while ((fd >= 0) && (fd < e.iofd));

	for (i = 0; i < n_io; i++)
	    close (map[i]);

	S_Remap (o_fd, fd);
	S_close (o_fd, TRUE);

	if (fd < 0)
	    print_error ("sh: too many files open\n");
    }

    return fd;
}

/*
 * here documents
 */

void		markhere (s, iop)
register char	*s;
IO_Actions 	*iop;
{
    register Here_D	*h, *lh;

    if ((h = (Here_D *) space(sizeof(Here_D))) == (Here_D *)NULL)
	return;

    if ((h->h_tag = evalstr (s, DOSUB)) == (char *)NULL)
	return;

    h->h_iop     = iop;
    iop->io_name = (char *)NULL;
    h->h_next    = (Here_D *)NULL;

    if (inhere == (Here_D *)NULL)
	inhere = h;

    else
    {
	for (lh = inhere; lh != (Here_D *)NULL; lh = lh->h_next)
	{
	    if (lh->h_next == (Here_D *)NULL)
	    {
		lh->h_next = h;
		break;
	    }
	}
    }

    iop->io_flag |= IOHERE|IOXHERE;

    for (s = h->h_tag; *s; s++)
    {
	if (*s & QUOTE)
	{
	    iop->io_flag &= ~ IOXHERE;
	    *s &= ~ QUOTE;
	}
    }

    h->h_dosub = iop->io_flag & IOXHERE;
}

void	gethere ()
{
    register Here_D	*h, *hp;

/* Scan here files first leaving inhere list in place */

    for (hp = h = inhere; h != (Here_D *)NULL; hp = h, h = h->h_next)
	readhere (&h->h_iop->io_name, h->h_tag, h->h_dosub ? 0 : '\'',
		  h->h_iop->io_flag);

/* Make inhere list active - keep list intact for scraphere */

    if (hp != (Here_D *)NULL)
    {
	hp->h_next = acthere;
	acthere    = inhere;
	inhere     = (Here_D *)NULL;
    }
}

static void	readhere (name, s, ec, ioflag)
char		**name;
register char	*s;
int		ec;
int		ioflag;
{
    int			tf;
    register int	c;
    jmp_buf		ev;
    char		*line;
    char		*next;
    int			stop_len;
    int			c_len;
    Out_Buf		*bp;

/* Create a temporary file and open it */

    *name = strsave (g_tempname (), areanum);

    if ((tf = S_open (FALSE, *name, O_CMASK | O_NOINHERIT, 0600)) < 0)
	return;

    if (newenv (setjmp (errpt = ev)) == TRUE)
	S_Delete (tf);

    else
    {
	pushio (e.iop->argp, e.iop->iofn);
	e.iobase = e.iop;

/* Strip leading tabs? */

	if (ioflag & IOTHERE)
	{
	    while (*s && (*s == '\t'))
		++s;
	}

/* Open the Output buffer */

	line = space ((stop_len = strlen (s) + 2) + 1);
	bp = Open_buffer (tf, TRUE);

/* Read in */

	while (1)
	{
	    next = line;
	    c_len = 0;

	    if (talking && e.iop <= iostack + 1)
	    {
#ifndef NO_HISTORY
		Add_History (FALSE);
#endif
		put_prompt (ps2->value);
	    }

/* Read the here document */

	    while ((c = Getc (ec)) != NL && c)
	    {

/* Strip leading tabs? */

		if ((ioflag & IOTHERE) && (c == '\t') && (next == line))
		    continue;

		if (ec == '\'')
		    c &= ~ QUOTE;

/* If not end of search string, add to search string */

		if ((++c_len) < stop_len)
		    *(next++) = (char)c;

/* If one greater that search string, buffer search string */

		else
		{
		    if (c_len == stop_len)
		    {
			*next = 0;
			Adds_buffer (line, bp);
		    }

/* Buffer the current character */

		    Add_buffer ((char)c, bp);
		}
	    }

/* Check for end of document */

	    *next = 0;
	    if (strcmp (s, line) == 0 || c == 0)
		break;

	    if (c_len < stop_len)
	        Adds_buffer (line, bp);

	    Add_buffer (NL, bp);
	}

	Close_buffer (bp);

	if (c == 0)
	    print_error ("here document `%s' unclosed\n", s);

	quitenv ();
    }

    S_close (tf, TRUE);
}

/*
 * open here temp file.
 * If unquoted here, expand here temp file into second temp file.
 */

int		herein (hname, xdoll)
char		*hname;
int		xdoll;
{
    register int	hf, tf;

/* If the here document is invalid or does not exist, return */

    if (hname == (char *)NULL)
	return -1;

    if ((hf = S_open (FALSE, hname, O_RDONLY)) < 0)
	return -1;

/* If processing for $, ` and ' is required, do it */

    if (xdoll)
    {
	char		c;
	char		*tname = strsave (g_tempname (), areanum);
	Out_Buf		*bp;
	jmp_buf		ev;

	if ((tf = S_open (FALSE, tname, O_CMASK | O_NOINHERIT, 0600)) < 0)
	    return -1;

	if (newenv (setjmp (errpt = ev)) == FALSE)
	{
	    bp = Open_buffer (tf, TRUE);
	    PUSHIO (afile, hf, herechar);
	    e.iobase = e.iop;

	    while ((c = (char)subgetc (0, 0)) != 0)
	    {

/* Determine how many characters to write.  If MSB set, write \x.
 * Otherwise, write x
 */

		if ((c & QUOTE) && !any ((c & ~QUOTE), "$`\\"))
		    Add_buffer ('\\', bp);

		Add_buffer ((char)(c & (~QUOTE)), bp);
	    }

	    quitenv ();
	}

	else
	    S_Delete (tf);

	Close_buffer (bp);
	S_close (tf, TRUE);
	return S_open (TRUE, tname, O_RDONLY);
    }

    else
	return hf;
}

void	scraphere()
{
    register Here_D	*h;

    for (h = inhere; h != (Here_D *)NULL; h = h->h_next)
    {
	if ((h->h_iop != (IO_Actions *)NULL) &&
	    (h->h_iop->io_name != (char *)NULL))
	    unlink (h->h_iop->io_name);
    }

    inhere = (Here_D *)NULL;
}

/* unlink here temp files before a freearea (area) */

void	freehere (area)
int	area;
{
    register Here_D	*h;
    register Here_D	*hl = (Here_D *)NULL;

    for (h = acthere; h != (Here_D *)NULL; hl = h, h = h->h_next)
    {
	if (getarea ((char *)h) >= area)
	{
	    if (h->h_iop->io_name != (char *)NULL)
		unlink (h->h_iop->io_name);

	    if (hl == (Here_D *)NULL)
		acthere = h->h_next;

	    else
		hl->h_next = h->h_next;
	}
    }
}

char	*g_tempname ()
{
    static char	tmpfile[FFNAME_MAX];
    char	*tmpdir;	/* Points to directory prefix of pipe	*/
    static int	temp_count = 0;
    char	*sep = "/";

/* Find out where we should put temporary files */

    if (((tmpdir = lookup ("TMP", FALSE)->value) == null) &&
	((tmpdir = lookup (home, FALSE)->value) == null))
	tmpdir = lookup ("TMPDIR", FALSE)->value;

    if (any (tmpdir[strlen (tmpdir) - 1], "/\\"))
	sep = null;

/* Get a unique temporary file name */

    while (1)
    {
	sprintf (tmpfile, "%s%ssht%.5u.tmp", tmpdir, sep, temp_count++);

	if (access (tmpfile, F_OK) != 0)
	    break;
    }

    return tmpfile;
}

/* Test to see if the current Input device is the console */

bool	Interactive ()
{
    return (talking && (e.iop->task == XIO) && isatty (e.iop->argp->afile))
	   ? TRUE : FALSE;
}
