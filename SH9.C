/* MS-DOS SHELL - History Processing
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
 *    $Header: sh9.c 1.12 90/05/31 10:39:26 MS_user Exp $
 *
 *    $Log:	sh9.c $
 * Revision 1.12  90/05/31  10:39:26  MS_user
 * Initialise the input buffer in case of interrupts
 *
 * Revision 1.11  90/03/27  20:22:07  MS_user
 * Fix problem with paging down history file - the last item was incorrect
 *
 * Revision 1.10  90/03/26  04:10:53  MS_user
 * Scan_History uses the Match length and not the string length for matching
 *
 * Revision 1.9  90/03/21  14:05:26  MS_user
 * History search sometimes includes the optionals in the search string.
 *
 * Revision 1.8  90/03/14  13:23:48  MS_user
 * Change names of configuration fields to reflect function
 *
 * Revision 1.7  90/03/13  18:36:07  MS_user
 * Add initialisation file processing
 *
 * Revision 1.6  90/03/09  16:07:40  MS_user
 * Add SH_ALT_KEYS processing
 * Fix bottom line processing so that cursor doesn't disappear
 * Fix EGA detection so we get the correct screen size
 *
 * Revision 1.5  90/03/06  16:50:57  MS_user
 * Add disable history option
 *
 * Revision 1.4  90/03/06  15:14:40  MS_user
 * Complete changes for file name completion
 * Add find Max Lines function
 *
 * Revision 1.3  90/03/05  13:54:28  MS_user
 * Fix get previous command request
 * Add filename completion
 * Add Max Columns from BIOS
 * Add cursor position check function
 * Add !! option
 * Change erase to end of line processing to remove ANSI.SYS dependency
 *
 * Revision 1.2  90/02/19  15:42:39  MS_user
 * Remove dependency on ANSI.SYS
 *
 * Revision 1.1  90/01/26  17:25:19  MS_user
 * Initial revision
 *
 *
 */

#include <sys/types.h>
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <setjmp.h>
#include <limits.h>
#include <unistd.h>
#include <dir.h>
#include "sh.h"

#define INCL_NOPM
#define INCL_VIO
#include <os2.h>

/* Keyboard functions */

#define KF_LENGTH		(sizeof (KF_List) / sizeof (KF_List[0]))
#define KF_SCANBACKWARD	0x00		/* Scan backwards in history	*/
#define KF_SCANFOREWARD	0x01		/* Scan forewards in history	*/
#define KF_PREVIOUS	0x02		/* Previous command		*/
#define KF_NEXT		0x03		/* Next command			*/
#define KF_LEFT		0x04		/* Left one character		*/
#define KF_RIGHT	0x05		/* Right one character		*/
#define KF_WORDRIGHT	0x06		/* Right one word		*/
#define KF_WORDLEFT	0x07		/* Left one word		*/
#define KF_START	0x08		/* Move to start of line	*/
#define KF_CLEAR	0x09		/* Clear input line		*/
#define KF_FLUSH	0x0a		/* Flush to end of line		*/
#define KF_END		0x0b		/* End of line			*/
#define KF_INSERT	0x0c		/* Insert mode switch		*/
#define KF_DELETERIGHT	0x0d		/* Delete right character	*/
#define KF_DELETELEFT	0x0e		/* Delete left character	*/
#define KF_COMPLETE	0x0f		/* Complete file name		*/
#define KF_DIRECTORY	0x10		/* Complete directory function	*/
#define KF_JOBS         0x11            /* Jobs list */
#define KF_END_FKEYS	0x12		/* End of function keys		*/
#define KF_RINGBELL	0x12		/* Ring bell			*/
#define KF_HALFHEIGTH	0x13		/* Half height cursor		*/


/* Function Declarations */

#ifndef NO_HISTORY
static bool	alpha_numeric (int);
static bool	function (int);
static bool	Process_History (int);
static bool	Scan_History (void);
static void	Redisplay_Line (void);
static void	Process_Stdin (void);
static void	Page_History (int);
static bool	UpDate_CLine (char *);
static bool	Re_start (char *);
static void	memrcpy (char *, char *, int);
static void	set_cursor_position (int);
static void	gen_cursor_position (void);
static void	erase_to_end_of_line (void);
static void	set_cursor_shape (bool);
static bool	Complete_file (char *, bool);
static void	Init_Input (bool);
#endif
static void	read_cursor_position (void);
static void	Get_Screen_Params (void);

static int	s_cursor;		/* Start cursor position	*/
static int	Max_Cols  = 80;		/* Max columns			*/
static int	Max_Lines = 25;		/* Max Lines			*/
#ifndef NO_HISTORY
static bool	insert_mode = TRUE;
static char	*c_buffer_pos;		/* Position in command line	*/
static char	*end_buffer;		/* End of command line		*/
static int	m_line = 0;		/* Max write line number	*/
static int	c_history = -1;		/* Current entry		*/
static int	l_history = 0;		/* End of history array		*/
static int	M_length = -1;		/* Match length			*/
static int	Max_Length = 0;		/* Max line length		*/
static char	l_buffer[LINE_MAX + 1];
static char	*No_prehistory   = "history: No previous commands";
static char	*No_MatchHistory = "history: No history match found";
static char	*No_posthistory  = "history: No more commands";
static char	*History_2long   = "history: History line too long";
static char	*H_TooLongI = "History file line too long - ignored (%d)\n";

/* Function Key table */

static struct Key_Fun_List {
    char	*kf_name;
    char	akey;
    char	fkey;
    char	fcode;
} KF_List[] = {
    { "ScanBackward",	0,	'I',	KF_SCANBACKWARD },
    { "ScanForeward",	0,	'Q',	KF_SCANFOREWARD },
    { "Previous",	0,	'H',	KF_PREVIOUS },
    { "Next",		0,	'P',	KF_NEXT },
    { "Left",		0,	'K',	KF_LEFT },
    { "Right",		0,	'M',	KF_RIGHT },
    { "WordRight",	0,	't',	KF_WORDRIGHT },
    { "WordLeft",	0,	's',	KF_WORDLEFT },
    { "Start",		0,	'G',	KF_START },
    { "Clear",		0x1b,	0,	KF_CLEAR },
    { "Flush",		0,	'u',	KF_FLUSH },
    { "End",		0,	'O',	KF_END },
    { "Insert",		0,	'R',	KF_INSERT },
    { "DeleteRight",	0,	'S',	KF_DELETERIGHT },
    { "DeleteLeft",	0x08,	0,	KF_DELETELEFT },
    { "Complete",	0x09,	0,	KF_COMPLETE },
    { "Directory",	0,	0x0f,	KF_DIRECTORY },
    { "Jobs",		0,	0x94,	KF_JOBS },

/* End of function keys - flags */

    { "Bell",		1,	0,	KF_RINGBELL },
    { "HalfHeight",	1,	0,	KF_HALFHEIGTH }
};

/* Arrary of history Items */

static struct	cmd_history {
    int		number;
    char	*command;
} cmd_history[HISTORY_MAX];

/* Processing standard input */

int			Get_stdin (ap)
register IO_Args	*ap;
{
    int		coff = (int)ap->afpos;
    char	rv;

/* Is there anything in the input buffer.  If not, add the previous line to
 * the history buffer and get the next line
 */

    if (!coff)
	Process_Stdin ();			/* No - get input	*/

/* Get the next character */

    if ((rv = l_buffer[coff]) == NL)
    {
	l_buffer[coff] = 0;
	ap->afpos = 0L;
    }

/* Check for end of file */

    else if (rv == 0x1a)
    {
	l_buffer[coff] = 0;
	ap->afpos = 0L;
	rv = 0;
    }

    else
	ap->afpos++;

    return rv;
}

/* Input processing function */

static void	Process_Stdin ()
{
    char	a_key, f_key;
    int		i;

/* Set to last history item */

    c_history = l_history;
    memset (l_buffer, 0, LINE_MAX + 1);

/* Process the input */

    while (TRUE)
    {
	Init_Input (TRUE);			/* Initialise		*/

	while (((a_key = (char)getch ()) != 0x1a) && (a_key != NL) &&
		(a_key != '\r'))
	{

/* If function key, get the fkey value */

            if ( a_key == 0xE0 )
              a_key = 0;

	    if (!a_key)
		f_key = (char)getch ();

/* Look up the keystroke to see if it is one of our functions */

	    for (i = 0; (i < KF_END_FKEYS); ++i)
	    {
		if (KF_List[i].akey != a_key)
		    continue;

		if ((a_key != 0) || (KF_List[i].fkey == f_key))
		    break;
	    }

/* If this is a function key and is not ours, ignore it */

	    if ((i == KF_END_FKEYS) && (!a_key))
		continue;

	    if (((i == KF_END_FKEYS) ? alpha_numeric (a_key)
				  : function (KF_List[i].fcode)))
		Redisplay_Line ();

/* Reposition the cursor */

	    gen_cursor_position ();
	}

/* Terminate the line */

	*end_buffer = 0;
	v1_putc (NL);
	s_cursor = -1;

/* Line input - check for history */

	if ((*l_buffer == '!') && Process_History (0))
	{
	    v1a_puts (l_buffer);
	    break;
	}

	else if (*l_buffer != '!')
	    break;
    }

    set_cursor_shape (FALSE);
    *end_buffer = (char)((a_key == '\r') ? NL : a_key);
}

/* Handler Alpha_numeric characters */

static bool	alpha_numeric (c)
int		c;
{
    bool	redisplay = FALSE;

/* Normal character processing */

    if ((c_buffer_pos - l_buffer) == LINE_MAX)
    {
	v1_putc (0x07);			/* Ring bell			*/
	return FALSE;
    }

    else if (!insert_mode)
    {
	if (c_buffer_pos == end_buffer)
	    ++end_buffer;

	else if (iscntrl (*c_buffer_pos) || iscntrl (c))
	    redisplay = TRUE;

	*(c_buffer_pos++) = (char)c;

	if (redisplay || (c == '\t'))
	    return TRUE;

	if (iscntrl (c))
	{
	    v1_putc ('^');
	    c += '@';
	}

	v1_putc ((char)c);
	return FALSE;
    }

    else if ((end_buffer - l_buffer) == LINE_MAX)
    {
	v1_putc (0x07);			/* Ring bell - line full	*/
	return FALSE;
    }

    else
    {
	if (c_buffer_pos != end_buffer)
	    memrcpy (end_buffer + 1, end_buffer, end_buffer - c_buffer_pos + 1);

	++end_buffer;
	*(c_buffer_pos++) = (char)c;
	return TRUE;
    }
}

/* Process function keys */

static bool	function (fn)
int		fn;
{
    bool	fn_search = FALSE;

    switch (fn)
    {
	case KF_SCANBACKWARD:		/* Scan backwards in history	*/
	case KF_SCANFOREWARD:		/* Scan forewards in history	*/
	    *end_buffer = 0;

	    if (M_length == -1)
		M_length = strlen (l_buffer);

	    Page_History ((fn == KF_SCANBACKWARD) ? -1 : 1);
	    return TRUE;

	case KF_PREVIOUS:		/* Previous command		*/
	    *end_buffer = 0;
	    Process_History (-1);
	    return TRUE;

	case KF_NEXT:			/* Next command line		*/
	    Process_History (1);
	    return TRUE;

	case KF_LEFT:			/* Cursor left			*/
	    if (c_buffer_pos != l_buffer)
		--c_buffer_pos;

	    else
		v1_putc (0x07);

	    return FALSE;

	case KF_RIGHT:			/* Cursor right			*/
	    if (c_buffer_pos != end_buffer)
		++c_buffer_pos;

	    else
		v1_putc (0x07);

	    return FALSE;

	case KF_WORDLEFT:		/* Cursor left a word		*/
	    if (c_buffer_pos != l_buffer)
	    {
		--c_buffer_pos;		/* Reposition on previous char	*/

		while (isspace (*c_buffer_pos) && (c_buffer_pos != l_buffer))
		    --c_buffer_pos;

		while (!isspace (*c_buffer_pos) && (c_buffer_pos != l_buffer))
		    --c_buffer_pos;

		if (c_buffer_pos != l_buffer)
		    ++c_buffer_pos;
	    }

	    else
		v1_putc (0x07);

	    return FALSE;

	case KF_WORDRIGHT:		/* Cursor right a word		*/
	    if (c_buffer_pos != end_buffer)
	    {

/* Skip to the end of the current word */

		while (!isspace (*c_buffer_pos) && (c_buffer_pos != end_buffer))
		    ++c_buffer_pos;

/* Skip over the white space */

		while (isspace (*c_buffer_pos) && (c_buffer_pos != end_buffer))
		    ++c_buffer_pos;
	    }

	    else
		v1_putc (0x07);

	    return FALSE;

	case KF_START:			/* Cursor home			*/
	    c_buffer_pos = l_buffer;
	    return FALSE;

	case KF_CLEAR:			/* Erase buffer			*/
	    c_buffer_pos = l_buffer;

	case KF_FLUSH:			/* Flush to end			*/
	    memset (c_buffer_pos, ' ', end_buffer - c_buffer_pos);
	    end_buffer = c_buffer_pos;
	    return TRUE;

	case KF_END:			/* Cursor end of command	*/
	    if (*l_buffer == '!')
	    {
		*end_buffer = 0;
		Process_History (2);
		return TRUE;
	    }

	    c_buffer_pos = end_buffer;
	    return FALSE;

	case KF_INSERT:			/* Switch insert mode		*/
	    insert_mode = (insert_mode) ? FALSE : TRUE;
	    set_cursor_shape (insert_mode);
	    return FALSE;

	case KF_DELETERIGHT:		/* Delete right character	*/
	    if (c_buffer_pos == end_buffer)
		return FALSE;

	    memcpy (c_buffer_pos, c_buffer_pos + 1, end_buffer - c_buffer_pos);

	    if (end_buffer == l_buffer)
	    {
		v1_putc (0x07);
		return TRUE;
	    }

	    if (--end_buffer < c_buffer_pos)
		--c_buffer_pos;

   	    return TRUE;

        case KF_JOBS:
	    v1_putc (NL);
            dojobs(NULL);
	    put_prompt (last_prompt);
	    read_cursor_position ();
            return TRUE;

	case KF_DIRECTORY:		/* File name directory		*/
	    fn_search = TRUE;

	case KF_COMPLETE:		/* File name completion		*/
	{
	    char	*fn_start = c_buffer_pos;

	    *end_buffer = 0;

	    if (isspace (*fn_start))
		--fn_start;

	    if (isspace (*fn_start) || (fn_start < l_buffer))
		break;

	    return Complete_file (fn_start, fn_search);
	}

	case KF_DELETELEFT:		/* Delete left character	*/
	    if (c_buffer_pos == l_buffer)
	    {
		v1_putc (0x07);		/* Ring bell			*/
		return FALSE;
	    }

/* Decrement current position */

	    --c_buffer_pos;
	    memcpy (c_buffer_pos, c_buffer_pos + 1, end_buffer - c_buffer_pos);
	    --end_buffer;
	    return TRUE;
    }
}

/* Set cursor shape */

static void	set_cursor_shape (mode)
bool		mode;
{
  VIOCURSORINFO vioci;

  vioci.yStart = mode ? (KF_List[KF_HALFHEIGTH].akey ? -50 : 0) : -90;
  vioci.cEnd = -100;
  vioci.cx = 0;
  vioci.attr = 0;

  VioSetCurType(&vioci, 0);
}
#endif

/* Read Cursor position */

static void	read_cursor_position ()
{
    USHORT usRow, usColumn;

    VioGetCurPos(&usRow, &usColumn, 0);
    s_cursor = (usRow * Max_Cols) + usColumn;
}

/* Re-position the cursor */

#ifndef NO_HISTORY
static void	set_cursor_position (new)
int		new;
{
    int diff;
    USHORT usRow, usColumn;

    usRow    = (unsigned char)(new / Max_Cols);
    usColumn = (unsigned char)(new % Max_Cols);

/* Are we at the bottom of the page? */

    if (usRow >= (unsigned char)Max_Lines)
    {
	diff = usRow + 1 - Max_Lines;
	usRow = (unsigned char)(Max_Lines - 1);
	s_cursor -= Max_Cols * diff;
    }

    VioSetCurPos(usRow, usColumn, 0);
}

/* Erase to end of line (avoid need for STUPID ansi.sys memory eater!) */

static void	erase_to_end_of_line ()
{
    USHORT usRow, usColumn;

    VioGetCurPos(&usRow, &usColumn, 0);
    VioWrtNChar(" ", Max_Cols - usColumn, usRow, usColumn, 0);
}

/* Generate the new cursor position */

static void	gen_cursor_position ()
{
    char	*cp = l_buffer - 1;
    int		off = s_cursor;

/* Search to current position */

    while (++cp != c_buffer_pos)
    {
	if (*cp == '\t')
	    while ((++off) % 8);

	else if (iscntrl (*cp))
	    off += 2;

	else
	    ++off;
    }

/* Position the cursor */

    set_cursor_position (off);
}

/* Redisplay the current line */

static void	Redisplay_Line ()
{
    char	*control = "^x";
    char	*cp = l_buffer;
    int		off = s_cursor;

/* Reposition to start of line */

    set_cursor_position (s_cursor);

/* Output the line */

    while (cp != end_buffer)
    {
	if (*cp == '\t')
	{
	    do
	    {
		v1_putc (SP);
	    } while ((++off) % 8);
	}

	else if (iscntrl (*cp))
	{
	    control[1] = *cp + '@';
	    v1_puts (control);
	    off += 2;
	}

	else
	{
	    ++off;
	    v1_putc (*cp);
	}

	++cp;
    }

    if ((m_line = ((s_cursor + Max_Length) / Max_Cols) + 1) >= Max_Lines)
	m_line = Max_Lines - 1;

    erase_to_end_of_line ();		/* clear to end of line	*/
    Max_Length = end_buffer - l_buffer;
}

/* Process history command
 *
 * -1: Previous command
 *  1: Next command
 *  0: Current command
 *  2: Current command with no options processing
 */

static bool	Process_History (direction)
int		direction;
{
    char	*optionals = null;

    c_buffer_pos = l_buffer;
    end_buffer = l_buffer;
    c_history += (direction == 2) ? 0 : direction;

    switch (direction)
    {
	case -1:			/* Move up one line		*/
	    if (c_history < 0)
	    {
		c_history = -1;
		return Re_start (No_prehistory);
	    }

	    break;

	case 1:				/* Move to next history line	*/
	    if (c_history >= l_history)
	    {
		c_history = l_history;
		return Re_start (No_posthistory);
	    }

	    break;

	case 0:				/* Check out l_buffer		*/
	    optionals = l_buffer;	/* Are there any additions to	*/
					/* the history line		*/

/* Find the end of the first part */

	    while (!isspace (*optionals) && *optionals)
	    {
		if (*optionals == '!')
		{

/* Terminate at !! */

		    if (*(optionals + 1) == '!')
		    {
			optionals += 2;
			break;
		    }

/* Terminate at a numeric value */

		    else if (isdigit (*(optionals + 1)) ||
			     (*(optionals + 1) == '-'))
		    {
			optionals += 2;
			while (isdigit (*optionals))
			    ++optionals;

			break;
		    }
		}

		++optionals;
	    }

/* Copy selected item into line buffer */

	case 2:
	    M_length = (optionals == null) ? strlen (l_buffer) - 1
					   : optionals - l_buffer - 1;

	    if (!Scan_History ())
		return FALSE;

	    break;
    }

    return UpDate_CLine (optionals);
}

/* Ok c_history points to the new line.  Move optionals after history
 * and the copy in history and add a space
 */

static bool	UpDate_CLine (optionals)
char		*optionals;
{
    int		opt_len;

    end_buffer = &l_buffer[strlen (cmd_history[c_history].command)];

    if ((end_buffer - l_buffer + (opt_len = strlen (optionals)) + 1) >= LINE_MAX)
	return Re_start (History_2long);

    if (end_buffer > optionals)
	memrcpy (end_buffer + opt_len, optionals + opt_len, opt_len + 1);

    else
	strcpy (end_buffer, optionals);

    strncpy (l_buffer, cmd_history[c_history].command, (end_buffer - l_buffer));
    end_buffer = &l_buffer[strlen (l_buffer)];
    c_buffer_pos = end_buffer;
    return TRUE;
}

/* Scan the line buffer for a history match */

static bool	Scan_History ()
{
    char	*cp = l_buffer + 1;
    char	*ep;
    int		i = (int)strtol (cp, &ep, 10);

/* Get the previous command ? (single ! or double !!) */

    if ((M_length == 0) || (*cp == '!'))
    {
	if (c_history >= l_history)
	    c_history = l_history - 1;

	if (c_history < 0)
	    return Re_start (No_prehistory);

	return TRUE;
    }

/* Request for special history number item.  Check History file empty */

    if (l_history == 0)
	return Re_start (No_MatchHistory);

/* Check for number */

    if ((*l_buffer == '!') && (ep > cp) && M_length)
    {
	M_length = -1;

	for (c_history = l_history - 1;
	    (cmd_history[c_history].number != i) && (c_history >= 0);
	    --c_history);
    }

/* No - scan for a match */

    else
    {
	for (c_history = l_history - 1;
	    (strncmp (cp, cmd_history[c_history].command, M_length) != 0)
	     && (c_history >= 0);
	    --c_history);
    }

/* Anything found ? */

    if (c_history == -1)
    {
	c_history = l_history - 1;
	return Re_start (No_MatchHistory);
    }

    return TRUE;
}

/* Scan back or forward from current history */

static void	Page_History (direction)
int		direction;
{
    c_buffer_pos = l_buffer;
    end_buffer = l_buffer;

    if (l_history == 0)
    {
	Re_start (No_MatchHistory);
	return;
    }

/* scan for a match */

    while (((c_history += direction) >= 0) && (c_history != l_history) &&
	   (strncmp (l_buffer, cmd_history[c_history].command, M_length) != 0));

/* Anything found ? */

    if ((c_history < 0) || (c_history >= l_history))
    {
	c_history = l_history - 1;
	Re_start (No_MatchHistory);
    }

    else
	UpDate_CLine (null);
}

/* Load history file */

void	Load_History ()
{
    FILE		*fp;
    char		*cp;
    int			i = 0;
    Var_List		*lset;

/* Initialise history array */

    memset (cmd_history, 0, sizeof (struct cmd_history) * HISTORY_MAX);
    c_history = -1;			/* Current entry		*/
    l_history = 0;			/* End of history array		*/

    if ((lset = lookup (history_file, TRUE))->value == null)
    {
	setval (lset, (cp = Build_H_Filename ("history.sh")));
	DELETE (cp);
    }

    if (!History_Enabled || ((fp = fopen (lset->value, "rt")) == (FILE *)NULL))
	return;

/* Read in file */

    while (fgets (l_buffer, LINE_MAX, fp) != (char *)NULL)
    {
	++i;

	if ((cp = strchr (l_buffer, NL)) == (char *)NULL)
	    print_warn (H_TooLongI, i);

	else
	{
	    *cp = 0;
	    Add_History (TRUE);
	}
    }

    fclose (fp);
}

/* Add entry to history file */

void	Add_History (past)
bool	past;				/* Past history?	*/
{
    int			i;

    if ((!History_Enabled) || (strlen (l_buffer) == 0))
	return;

/* If adding past history, decrement all numbers previous */

    if ((past) && l_history)
    {
	for (i = 0; i < l_history; i++)
	    --(cmd_history[i].number);
    }

/* If the array is full, remove the last item */

    if (l_history == HISTORY_MAX)
    {
	if (cmd_history[0].command != null)
	    DELETE (cmd_history[0].command);

	--l_history;
	memcpy (&cmd_history[0], &cmd_history[1],
		sizeof (struct cmd_history) * (HISTORY_MAX - 1));
    }

/* If there are any items in the array */

    c_history = l_history;
    Current_Event = (l_history) ? cmd_history[l_history - 1].number + 1 : 0;
    cmd_history[l_history].number = Current_Event;

/* Save the string */

    cmd_history[l_history++].command = strsave (l_buffer, 0);
}

/* Print history */

void	Display_History ()
{
    int			i;
    struct cmd_history	*cp = cmd_history;

    if (!l_history)
	return;

    for (i = 0; i < l_history; ++cp, ++i)
    {
	v1printf ("%5d: ", cp->number);
	v1a_puts (cp->command);
    }
}

/* Dump history to file */

void	Dump_History ()
{
    int			i;
    struct cmd_history	*cp = cmd_history;
    FILE		*fp;

    if (!History_Enabled ||
	((fp = fopen (lookup (history_file, FALSE)->value, "wt")) ==
	 (FILE *)NULL))
	return;

    for (i = 0; i < l_history; ++cp, ++i)
    {
	fputs (cp->command, fp);
	fputc (NL, fp);
    }

    fclose (fp);
}

/* Clear out history */

void	Clear_History ()
{
    int			i;
    struct cmd_history	*cp = cmd_history;

    for (i = 0; i < l_history; ++cp, ++i)
    {
	if (cp->command != null)
	    DELETE (cp->command);
    }

    memset (cmd_history, 0, sizeof (struct cmd_history) * HISTORY_MAX);

    c_history = -1;			/* Current entry		*/
    l_history = 0;			/* End of history array		*/
    Current_Event = 0;
}

/* Output warning message and prompt */

static bool	Re_start (cp)
char		*cp;
{
    if (cp != (char *)NULL)
    {
	if (strlen (l_buffer) && (s_cursor != -1))
	    S_putc (NL);

	print_warn (cp);
	erase_to_end_of_line ();
	v1_putc (NL);
    }

    put_prompt (last_prompt);

/* Re-initialise */

    Init_Input (insert_mode);
    return FALSE;
}

/* Copy backwards */

static void	memrcpy (sp1, sp, cnt)
char		*sp1;
char		*sp;
int		cnt;
{
    while (cnt--)
	*(sp1--) =  *(sp--);
}

/* Complete file name */

static bool	Complete_file (fn_start, fn_search)
char		*fn_start;
bool		fn_search;
{
    char		*fn_end, *cp, *fn_mstart, fn_es, *fn_dir;
    int			fn_len, pre_len, i;
    DIR			*dn;
    char		d_name [NAME_MAX + 1];
    struct direct	*d_ce;
    int			found_cnt = 0;
    int			max_per_line;
    static char		*ms_drive = "a:/";

    while (!isspace (*fn_start) && (fn_start != l_buffer))
	--fn_start;

    if (isspace (*fn_start))
	++fn_start;

    fn_end = fn_start;

    while (!isspace (*fn_end) && (fn_end != end_buffer))
	++fn_end;

/* Get the directory name */

    if (fn_end != end_buffer)
    {
	fn_es = *fn_end;
	*fn_end = 0;
    }

/* Find the directory name */

    if ((cp = strrchr (fn_start, '/')) != (char *)NULL)
    {
	fn_mstart = cp + 1;
	fn_dir = fn_start;
    }

/* No directory flag - Drive specifier? */

    else if (*(fn_start + 1) == ':')
    {
	*(fn_dir = ms_drive) = *fn_start;
	*(fn_dir + 2) = '.';
	fn_mstart = fn_start + 2;
    }

/* No drive specifier */

    else
    {
	fn_dir = ".";
	fn_mstart = fn_start;
    }

/* Set up some values - length and end */

    fn_len = fn_end - fn_mstart;

    if (fn_end != end_buffer)
	*fn_end = fn_es;

/* Get the match length which must be nonzero unless we are doing a display
 * of the directory
 */

    if (!fn_len && !fn_search)
    {
	v1_putc (0x07);
	return FALSE;
    }

/* Reset the / to a zero to terminate the directory name */

    if (cp != (char *)NULL)
	*cp = 0;

/* Check for some special cases - root */

    if ((i = strlen (fn_dir)) == 0)
	fn_dir = "/";

    else if ((i == 2) && (*(fn_dir + 1) == ':'))
    {
	*(fn_dir = ms_drive) = *fn_start;
	*(fn_dir + 2) = '/';
    }

/* Get the prefix length and open the directory */

    pre_len = fn_mstart - l_buffer;
    dn = opendir (fn_dir);

    if (cp != (char *)NULL)
	*cp = '/';

    if (dn == (DIR *)NULL)
    {
	v1_putc (0x07);
	return FALSE;
    }

/* Initialise the save buffer for a search or a match.  In the case of a
 * search, we alway want to output NAME_MAX characters.  In the case of a
 * match we want to know if we found it.
 */

    d_name[NAME_MAX] = 0;
    *d_name = 0;
    max_per_line = (Max_Cols / (((NAME_MAX / 8) + 1) * 8));

/* Scan the directory */

    while ((d_ce = readdir (dn)) != (struct direct *)NULL)
    {
	if (strnicmp (d_ce->d_name, fn_mstart, fn_len) == 0)
	{

/* Are we displaying the directory or just searching */

	    if (fn_search)
	    {
		v1_putc ((char)((found_cnt % max_per_line == 0) ? NL : '\t'));
		memset (d_name, ' ', NAME_MAX);
		memcpy (d_name, d_ce->d_name, strlen (d_ce->d_name));
		v1_puts (d_name);
	    }

/* Just search - check for first entry match */

	    else if (!*d_name)
		strcpy (d_name, d_ce->d_name);

	    else
	    {
		for (i = fn_len; d_name[i] == d_ce->d_name[i] ; i++);
		d_name[i] = 0;
	    }

/* Increment counter */

	    ++found_cnt;
	}
    }

    closedir (dn);

/* If we are searching and we found something - redraw */

    if (fn_search && found_cnt)
    {
	v1_putc (NL);
	put_prompt (last_prompt);
	read_cursor_position ();
	return TRUE;
    }

/* Did I find anything? - no exit */

    if (!*d_name)
    {
	v1_putc (0x07);
	return FALSE;
    }

/* Check that the line is not too long and if there is an end bit, we can
 * save a copy of it.
 */

    cp = null;
    fn_len = strlen (fn_end);

    if (((fn_len + strlen (d_name) + pre_len) >= LINE_MAX) ||
	((fn_len != 0) && ((cp = strdup (fn_end)) == (char *)NULL)))
    {
	v1_putc (0x07);
	return FALSE;
    }

/* Append the new end of line bits */

    strcpy (fn_mstart, d_name);
    strcat (fn_mstart, cp);

    if (cp != null)
	free (cp);

    end_buffer = &l_buffer[strlen (l_buffer)];
    c_buffer_pos = end_buffer;

/* Beep if more than one */

    if (found_cnt > 1)
	v1_putc (0x07);

    return TRUE;
}

/* Initialise input */

static void	Init_Input (im)
bool		im;
{
    c_buffer_pos = l_buffer;	/* Initialise			*/
    end_buffer = l_buffer;
    insert_mode = im;
    set_cursor_shape (insert_mode);
    M_length = -1;

/* Reset max line length and get the number of columns */

    Max_Length = 0;
    Get_Screen_Params ();

/* Save the cursor position */

    read_cursor_position ();
}

/* Configure Keyboard I/O */

void	Configure_Keys ()
{
    char		*sp;			/* Line pointers	*/
    char		*cp;
    FILE		*fp;
    char		*line;			/* Input line		*/
    char		c;			/* Save character	*/
    int			i, fval, cval;
    int			line_len;

/* Get some memory for the input line and the file name */

    line_len = max (strlen (Program_Name) + 4, 200);
    if ((line = getcell (line_len)) == (char *)NULL)
	return;

    strcpy (line, Program_Name);

/* Find the .exe in the name */

    if ((cp = strrchr (line, '/')) != (char *)NULL)
	++cp;

    else
	cp = line;

    if ((cp = strrchr (cp, '.')) == (char *)NULL)
	cp = &line[strlen (line)];

    strcpy (cp, ".ini");

    if ((fp = fopen (line, "rt")) == (FILE *)NULL)
    {
	DELETE (line);
	return;
    }

    while (fgets (line, line_len - 1, fp) != (char *)NULL)
    {

/* Ignore comment lines */

	if (*line == '#')
	    continue;

/* Remove the EOL */

	if ((cp = strchr (line, '\n')) != (char *)NULL)
	    *cp = 0;

/* Find the keyword */

	cp = line;
	while (!isspace (*cp) && *cp && (*cp != '='))
	    ++cp;

	if (!*cp)
	    continue;

	c = *cp;
	*cp = 0;

/* Look up the keyword name */

	for (i = 0; (i < KF_LENGTH) &&
		    (stricmp (line, KF_List[i].kf_name) != 0); ++i);

/* Ignore no matches */

	if (i == KF_LENGTH)
	    continue;

/* Find the equals */

	*cp = c;
	while (isspace (*cp))
	    ++cp;

	if (*(cp++) != '=')
	    continue;

	while (isspace (*cp))
	    ++cp;

/* Get the value */

	errno = 0;
	cval = 0;

	fval = (int)strtol (cp, &sp, 0);

/* Check for correct terminator */

	if (errno || (fval < 0) ||
	    ((fval != 0) && *sp) ||
	    ((fval == 0) &&
	      (((i < KF_END_FKEYS) && !isspace (*sp)) ||
	       ((i >= KF_END_FKEYS) && *sp))))
	    continue;

	if ((fval == 0) && (i < KF_END_FKEYS))
	{
	    cp = sp;
	    while (isspace (*cp))
		++cp;

	    errno = 0;
	    cval = (int)strtol (cp, &sp, 0);

	    if (errno || (cval == 0) || *sp)
		continue;
	}

/* OK we have a valid value, save it */

	KF_List[i].akey = (char)fval;
	KF_List[i].fkey = (char)cval;
    }

    DELETE (line);
    fclose (fp);
}
#endif

/* Check cursor is in column zero */

void	In_Col_Zero ()
{
    CHAR str[1];
    USHORT cb = sizeof(str);
    USHORT usRow, usColumn;

    Get_Screen_Params ();
    read_cursor_position ();

    VioGetCurPos(&usRow, &usColumn, 0);
    VioReadCharStr(str, &cb, usRow, usColumn, 0);

    if ((s_cursor % Max_Cols) || (str[0] != ' '))
	v1_putc (NL);
}

/* Get screen parameters */

static void	Get_Screen_Params ()
{
    VIOMODEINFO viomi;

    viomi.cb = sizeof(viomi);
    VioGetMode(&viomi, 0);

    Max_Cols  = viomi.col;
    Max_Lines = viomi.row;
}

/* Ring Bell ? */

bool	Ring_Bell ()
{
#ifdef NO_HISTORY
    return TRUE;
#else
    return (bool)(KF_List[KF_RINGBELL].akey ? TRUE : FALSE);
#endif
}
