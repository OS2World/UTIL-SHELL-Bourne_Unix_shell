/* MS-DOS SHELL - Unix File I/O Emulation
 *
 * MS-DOS SHELL - Copyright (c) 1990 Data Logic Limited
 *
 * This code is subject to the following copyright restrictions:
 *
 * 1.  Redistribution and use in source and binary forms are permitted
 *     provided that the above copyright notice is duplicated in the
 *     source form and the copyright notice in file sh6.c is displayed
 *     on entry to the program.
 *
 * 2.  The sources (or parts thereof) or objects generated from the sources
 *     (or parts of sources) cannot be sold under any circumstances.
 *
 *    $Header: sh8.c 1.11 90/06/21 11:12:13 MS_user Exp $
 *
 *    $Log:	sh8.c $
 * Revision 1.11  90/06/21  11:12:13  MS_user
 * Ensure Areanum is set correctly for memory areas
 *
 * Revision 1.10  90/05/31  11:31:53  MS_user
 * Correct misplaced signal restore
 *
 * Revision 1.9  90/05/31  09:50:41  MS_user
 * Add some signal lockouts to prevent corruption
 *
 * Revision 1.8  90/03/26  20:58:11  MS_user
 * Change I/O restore so that "exec >filename" works
 *
 * Revision 1.7  90/03/22  13:48:03  MS_user
 * MSDOS does not handle /dev/ files after find_first correctly
 *
 * Revision 1.6  90/03/14  19:32:42  MS_user
 * Change buffered output to be re-entrant
 *
 * Revision 1.5  90/03/14  16:46:21  MS_user
 * New Open_buffer parameter and Adds_buffer function
 *
 * Revision 1.4  90/03/13  21:20:50  MS_user
 * Add Buffered Output functions
 *
 * Revision 1.3  90/03/06  15:14:03  MS_user
 * Change script detection to look for a character less than 0x08
 *
 * Revision 1.2  90/03/05  13:54:08  MS_user
 * Fix bug in S_dup
 * Change the way we detect shell scripts
 * Add support for alternate command interpreters a la V.4
 *
 * Revision 1.1  90/01/29  17:46:37  MS_user
 * Initial revision
 *
 *
 */

#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include "sh.h"

#define F_START		4

static char	*nopipe = "can't create pipe - try again\n";

/* List of open files to allow us to simulate the Unix open and unlink
 * operation for temporary files
 */

typedef struct flist {
    struct flist	*fl_next;	/* Next link			*/
    char		*fl_name;	/* File name			*/
    bool		fl_close;	/* Delete on close flag		*/
    int			fl_size;	/* Size of fl_fd array		*/
    int			fl_count;	/* Number of entries in array	*/
    int			fl_mode;	/* File open mode		*/
    int			*fl_fd;		/* File ID array (for dup)	*/
} s_flist;

static s_flist		*list_start = (s_flist *)NULL;
static s_flist		*find_entry (int);

/*
 * Open a file and add it to the Open file list.  Errors are the same as
 * for a normal open.
 */

int	S_open (d_flag, name, mode)
bool	d_flag;
char	*name;
int	mode;
{
    va_list		ap;
    int			pmask;
    s_flist		*fp = (struct s_flist *)NULL;
    int			*f_list = (int *)NULL;
    char		*f_name = (char *)NULL;
    void		(*save_signal)(int);

/* Check the permission mask if it exists */

    va_start (ap, mode);
    pmask =  va_arg (ap, int);
    va_end (ap);

/* Grap some space.  If it fails, free space and return an error */

    if (((fp = (s_flist *) space (sizeof (s_flist))) == (s_flist *)NULL) ||
	((f_list = (int *) space (sizeof (int) * F_START)) == (int *)NULL) ||
	((f_name = strsave (name, 0)) == null))
    {
	if (f_list == (int *)NULL)
	    DELETE (f_list);

	if (fp == (s_flist *)NULL)
	    DELETE (fp);

	errno = ENOMEM;
	return -1;
    }

/* Disable signals */

    save_signal = signal (SIGINT, SIG_IGN);

/* Set up the structure.  Change two Unix device names to the DOS
 * equivalents and disable create
 */

    if (strnicmp (name, "/dev/", 5) == 0)
    {
	if (stricmp (&name[5], "tty") == 0)
	    strcpy (&name[5], "con");

	else if (stricmp (&name[5], "null") == 0)
	    strcpy (&name[5], "nul");

	mode &= ~(O_CREAT | O_TRUNC);
    }

    fp->fl_name  = strcpy (f_name, name);
    fp->fl_close = d_flag;
    fp->fl_size  = F_START;
    fp->fl_count = 1;
    fp->fl_fd    = f_list;
    fp->fl_mode  = mode;

/* Open the file */

    if ((fp->fl_fd[0] = open (name, mode, pmask)) < 0)
    {
	pmask = errno;
	DELETE (f_name);
	DELETE (f_list);
	DELETE (fp);
	errno = pmask;
	pmask = -1;
    }

/* Make sure everything is in area 0 */

    else
    {
	setarea ((char *)fp, 0);
	setarea ((char *)f_list, 0);

/* List into the list */

	fp->fl_next   = list_start;
	list_start = fp;

/* Return the file descriptor */

	pmask = fp->fl_fd[0];
    }

/* Restore signals */

    signal (SIGINT, save_signal);

    return pmask;
}

/*
 * Scan the File list for the appropriate entry for the specified ID
 */

static s_flist		*find_entry (fid)
int			fid;
{
    s_flist	*fp = list_start;
    int		i;

    while (fp != (s_flist *)NULL)
    {
	for (i = 0; i < fp->fl_count; i++)
	{
	    if (fp->fl_fd[i] == fid)
		return fp;
	}

	fp = fp->fl_next;
    }

    return (s_flist *)NULL;
}

/* Close the file
 *
 * We need a version of close that does everything but close for dup2 as
 * new file id is closed.  If c_flag is TRUE, close the file as well.
 */

int	S_close (fid, c_flag)
int	fid;
bool	c_flag;
{
    s_flist	*fp = find_entry (fid);
    s_flist	*last = (s_flist *)NULL;
    s_flist	*fp1 = list_start;
    int		i, serrno;
    bool	release = TRUE;
    bool	delete = FALSE;
    char	*fname;
    void	(*save_signal)(int);

/* Disable signals */

    save_signal = signal (SIGINT, SIG_IGN);

/* Find the entry for this ID */

    if (fp != (s_flist *)NULL)
    {
	for (i = 0; i < fp->fl_count; i++)
	{
	    if (fp->fl_fd[i] == fid)
		fp->fl_fd[i] = -1;

	    if (fp->fl_fd[i] != -1)
		release = FALSE;
	}

/* Are all the Fids closed ? */

	if (release)
	{
	    fname = fp->fl_name;
	    delete = fp->fl_close;
	    DELETE (fp->fl_fd);

/* Scan the list and remove the entry */

	    while (fp1 != (s_flist *)NULL)
	    {
		if (fp1 != fp)
		{
		    last = fp1;
		    fp1 = fp1->fl_next;
		    continue;
		}

		if (last == (s_flist *)NULL)
		    list_start = fp->fl_next;

		else
		    last->fl_next = fp->fl_next;

		break;
	    }

/* OK - delete the area */

	    DELETE (fp);
	}
    }

/* Close the file anyway */

    if (c_flag)
    {
	i = close (fid);
	serrno = errno;
    }

/* Delete the file ? */

    if (delete)
    {
	unlink (fname);
	DELETE (fname);
    }

/* Restore signals */

    signal (SIGINT, save_signal);

/* Restore results and error code */

    errno = serrno;
    return i;
}

/*
 * Duplicate file handler.  Add the new handler to the ID array for this
 * file.
 */

int	S_dup (old_fid)
int	old_fid;
{
    int		new_fid;

    if ((new_fid = dup (old_fid)) >= 0)
	S_Remap (old_fid, new_fid);

    return new_fid;
}

/*
 * Add the ID to the ID array for this file
 */

int	S_Remap (old_fid, new_fid)
int	old_fid, new_fid;
{
    s_flist	*fp = find_entry (old_fid);
    int		*flist;
    int		i;

    if (fp == (s_flist *)NULL)
	return new_fid;

/* Is there an empty slot ? */

    for (i = 0; i < fp->fl_count; i++)
    {
	if (fp->fl_fd[i] == -1)
	    return (fp->fl_fd[i] = new_fid);
    }

/* Is there any room at the end ? No - grap somemore space and effect a
 * re-alloc.  What to do if the re-alloc fails - should really get here.
 * Safty check only??
 */

    if (fp->fl_count == fp->fl_size)
    {
	if ((flist = (int *) space ((fp->fl_size + F_START) * sizeof (int)))
	    == (int *)NULL)
	    return new_fid;

	setarea ((char *)flist, 0);
	memcpy ((char *)flist, (char *)fp->fl_fd, sizeof (int) * fp->fl_size);
	DELETE (fp->fl_fd);

	fp->fl_fd   = flist;
	fp->fl_size += F_START;
    }

    return (fp->fl_fd[fp->fl_count++] = new_fid);
}

/*
 * Set Delete on Close flag
 */

void	S_Delete (fid)
int	fid;
{
    s_flist	*fp = find_entry (fid);

    if (fp != (s_flist *)NULL)
	fp->fl_close = TRUE;
}

/*
 * Duplicate file handler onto specific handler
 */

int	S_dup2 (old_fid, new_fid)
int	old_fid;
int	new_fid;
{
    int		res = -1;
    int		i;
    Save_IO	*sp;

/* If duping onto stdin, stdout or stderr, Search the Save IO stack for an
 * entry matching us
 */

    if ((new_fid >= STDIN_FILENO) && (new_fid <= STDERR_FILENO))
    {
	for (sp = SSave_IO, i = 0; (i < NSave_IO_E) &&
				   (SSave_IO[i].depth < Execute_stack_depth);
	     i++);

/* If depth is greater the Execute_stack_depth - we should panic as this
 * should not happen.  However, for the moment, I'll ignore it
 */

/* If there an entry for this depth ? */

	if (i == NSave_IO_E)
	{

/* Do we need more space? */

	    if (NSave_IO_E == MSave_IO_E)
	    {
		sp = (Save_IO *)space ((MSave_IO_E + SSAVE_IO_SIZE) * sizeof (Save_IO));

/* Check for error */

		if (sp == (Save_IO *)NULL)
		{
		    errno = ENOMEM;
		    return -1;
		}

/* Save original data */

		if (MSave_IO_E != 0)
		{
		    memcpy (sp, SSave_IO, sizeof (Save_IO) * MSave_IO_E);
		    DELETE (SSave_IO);
		}

		setarea ((char *)sp, 1);
		SSave_IO = sp;
		MSave_IO_E += SSAVE_IO_SIZE;
	    }

/* Initialise the new entry */

	    sp = &SSave_IO[NSave_IO_E++];
	    sp->depth             = Execute_stack_depth;
	    sp->fp[STDIN_FILENO]  = -1;
	    sp->fp[STDOUT_FILENO] = -1;
	    sp->fp[STDERR_FILENO] = -1;
	}

	if (sp->fp[new_fid] == -1)
	    sp->fp[new_fid] = remap (new_fid);
    }

/* OK - Dup the descriptor */

    if ((old_fid != -1) && ((res = dup2 (old_fid, new_fid)) >= 0))
    {
	S_close (new_fid, FALSE);
	res = S_Remap (old_fid, new_fid);
    }

    return res;
}

/*
 * Restore the Stdin, Stdout and Stderr to original values.  If change is
 * FALSE, just remove entries from stack.  A special case for exec.
 */

int	restore_std (rv, change)
int	rv;
bool	change;
{
    int		j, i;
    Save_IO	*sp;

/* Start at the top and remove any entries above the current execute stack
 * depth
 */

    for (j = NSave_IO_E; j > 0; j--)
    {
       sp = &SSave_IO[j - 1];

       if (sp->depth < Execute_stack_depth)
	   break;

/* Reduce number of entries */

	--NSave_IO_E;

/* If special case (changed at this level) - continue */

	if (!change && (sp->depth == Execute_stack_depth))
	    continue;

/* Close and restore any files */

	for (i = STDIN_FILENO; i <= STDERR_FILENO; i++)
	{
	    if (sp->fp[i] != -1)
	    {
		S_close (i, TRUE);
		dup2 (sp->fp[i], i);
		S_close (sp->fp[i], TRUE);
	    }
	}
    }

    return rv;
}

/*
 * Create a Pipe
 */

int		openpipe ()
{
    register int	i;

    if ((i = S_open (TRUE, g_tempname (), O_PMASK, 0600)) < 0)
	print_error (nopipe);

    return i;
}

/*
 * Close a pipe
 */

void		closepipe (pv)
register int	pv;
{
    if (pv != -1)
	S_close (pv, TRUE);
}

/*
 * Write a character to STDERR
 */

void	S_putc (c)
int	c;
{
    write (STDERR_FILENO, (char *)&c, 1);
}

/*
 * Write a string to STDERR
 */

void	S_puts (s)
char	*s;
{
    write (STDERR_FILENO, s, strlen (s));
}

/*
 * Check for restricted shell
 */

bool	check_rsh (s)
char	*s;
{
    if (r_flag)
    {
	print_error ("%s: restricted\n", s);
	return TRUE;
    }

    return FALSE;
}

/*
 * Check to see if a file is a shell script.  If it is, return the file
 * handler for the file
 */

int	O_for_execute (path, params, nargs)
char	*path;
char	**params;
int	*nargs;
{
    int		i = -1;
    char	*local_path;

/* Work on a copy of the path */

    if ((local_path = getcell (strlen (path) + 4)) == (char *)NULL)
	return -1;

/* Try the file name and then with a .sh appended */

    if ((i = Check_Script (strcpy (local_path, path), params, nargs)) < 0)
      if ((i = Check_Script (strcat (local_path, ".sh"), params, nargs)) == 0)
        strcpy(path, local_path);

    DELETE (local_path);
    return i;
}

/*
 * Check for shell script
 */

int	Check_Script (path, params, nargs)
char	*path;
char	**params;
int	*nargs;
{
    char	buf[512];		/* Input buffer			*/
    int		fp;			/* File handler			*/
    int		nbytes;			/* Number of bytes read		*/
    char	*bp;			/* Pointers into buffers	*/
    char	*ep;

    if ((fp = S_open (FALSE, path, O_RMASK)) < 0)
	return -1;

/* zero or less bytes - not a script */

    memset (buf, 0, 512);
    nbytes = read (fp, buf, 512);

    for (ep = &buf[nbytes], bp = buf; (bp < ep) && ((unsigned char)*bp >= 0x08); ++bp);

/* If non-ascii file or lenght is less than 1 - not a script */

    if ((bp != ep) || (nbytes < 1))
    {
	S_close (fp, TRUE);
	return -1;
    }

/* Ensure end of buffer detected */

    buf[511] = 0;

/* Initialise the return parameters, if specified */

    if (params != (char **)NULL)
	*params = null;

    if (nargs != (int *)NULL)
	*nargs = 0;

/* We don't care how many bytes were read now, so use it to count the
 * additional arguments
 */

    nbytes = 0;

/* Find the end of the first line */

    if ((bp = strchr (buf, '\n')) != (char *)NULL)
	*bp = 0;

    bp = buf;
    ep = (char *)NULL;

/* Check for script */

    if ((*(bp++) != '#') || (*(bp++) != '!'))
	return fp;

    while (*bp)
    {
	while (isspace (*bp))
	    ++bp;

/* Save the start of the arguments */

	if (*bp)
	{
	    if (ep == (char *)NULL)
		ep = bp;

/* Count the arguments */

	    ++nbytes;
	}

	while (!isspace (*bp) && *bp)
	    ++bp;
    }

/* Set up the return parameters, if appropriate */

    if ((params != (char **)NULL) && (strlen (ep) != 0))
    {
	if ((*params = getcell (strlen (ep) + 1)) == (char *)NULL)
	{
	    *params = null;
	    S_close (fp, TRUE);
	    return -1;
	}

	strcpy (*params, ep);
    }

    if (nargs != (int *)NULL)
	*nargs = nbytes;

    return fp;
}

/*
 * Convert slashes to backslashes for MSDOS
 */

void	Convert_Slashes (sp)
char	*sp;
{
    while (*sp)
    {
	if (*sp == '/')
	    *sp = '\\';

	++sp;
    }
}

/*
 * Some buffered Output functions to speed somethings up.
 */

/* Open the buffer */

Out_Buf	*Open_buffer (fid, f_abort)
int	fid;
bool	f_abort;
{
    Out_Buf	*bp;

    if (((bp = (Out_Buf *)getcell (sizeof (Out_Buf))) == (Out_Buf *)NULL) ||
	((bp->ob_start = getcell (BIO_LENGTH)) == (char *)NULL))
    {
	if (f_abort)
	{
	    print_error ("sh: %s\n", strerror (ENOMEM));
	    fail ();
	}

	return (Out_Buf *)NULL;
    }

/* Ok - save info */

    bp->ob_fid = fid;
    bp->ob_cur = bp->ob_start;
    return bp;
}

/* Add a character to the buffer */

void		Add_buffer (c, bp)
char		c;
Out_Buf		*bp;
{
    *(bp->ob_cur++) = c;

    if (bp->ob_cur == &bp->ob_start[BIO_LENGTH - 1])
    {
	write (bp->ob_fid, bp->ob_start, BIO_LENGTH - 1);
	bp->ob_cur = bp->ob_start;
    }
}

/* Close the buffer */

void		Close_buffer (bp)
Out_Buf		*bp;
{
    int		n;

    if ((n = (int)(bp->ob_cur - bp->ob_start)))
	write (bp->ob_fid, bp->ob_start, n);

    DELETE (bp->ob_start);
    DELETE (bp);
}

/* Output string */

void		Adds_buffer (s, bp)
char		*s;
Out_Buf		*bp;
{
    while (*s)
    {
	*(bp->ob_cur++) = *(s++);

	if (bp->ob_cur == &bp->ob_start[BIO_LENGTH - 1])
	{
	    write (bp->ob_fid, bp->ob_start, BIO_LENGTH - 1);
	    bp->ob_cur = bp->ob_start;
	}
    }
}
