/* MS-DOS SHELL - Function Processing
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
 *    $Header: sh10.c 1.3 90/05/31 09:51:06 MS_user Exp $
 *
 *    $Log:	sh10.c $
 * Revision 1.3  90/05/31  09:51:06  MS_user
 * Add some signal lockouts to prevent corruption
 *
 * Revision 1.2  90/04/25  22:34:04  MS_user
 * Fix case in TELIF where then and else parts are not defined
 *
 * Revision 1.1  90/01/25  13:40:54  MS_user
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
#include "sh.h"

/* Function declarations */

static void	Print_Command (C_Op *);
static void	Print_IO (IO_Actions *);
static void	Print_Case (C_Op *);
static void	Print_IString (char *, int);
static void	Set_Free_ExTree (C_Op *, void (*)(char *));
static void	Set_Free_Command (C_Op *, void (*)(char *));
static void	Set_Free_Case (C_Op *, void (*)(char *));
static void	Set_ExTree (char *);
static void	Free_ExTree (char *);

static int	Print_indent;			/* Current indent level	*/

/*
 * print the execute tree - used for displaying functions
 */

void		Print_ExTree (t)
register C_Op	*t;
{
    char		**wp;

    if (t == (C_Op *)NULL)
	return;

/* Check for start of print */

    if (t->type == TFUNC)
    {
	Print_indent = 0;
	v1_puts (*t->words);
	v1a_puts (" ()");
	Print_ExTree (t->left);
	return;
    }

/* Otherwise, process the tree and print it */

    switch (t->type)
    {
	case TPAREN:			/* ()			*/
	case TCOM:			/* A command process	*/
	    Print_Command (t);
	    return;

	case TPIPE:			/* Pipe processing		*/
	    Print_ExTree (t->left);
	    Print_IString ("|\n", 0);
	    Print_ExTree (t->right);
	    return;

	case TLIST:			/* Entries in a for statement	*/
	    Print_ExTree (t->left);
	    Print_ExTree (t->right);
	    return;

	case TOR:			/* || and &&			*/
	case TAND:
	    Print_ExTree (t->left);

	    if (t->right != (C_Op *)NULL)
	    {
		Print_IString ((t->type == TAND) ? "&&\n" : "||\n", 0);
		Print_ExTree (t->right);
	    }

	    return;

	case TFOR:			/* First part of a for statement*/
	    Print_IString ("for ", 0);
	    v1_puts (t->str);

	    if ((wp = t->words) != (char **)NULL)
	    {
		v1_puts (" in");

		while (*wp != (char *)NULL)
		{
		    v1_putc (SP);
		    v1_puts (*wp++);
		}
	    }

	    v1_putc (NL);
	    Print_IString ("do\n", 1);
	    Print_ExTree (t->left);
	    Print_IString ("done\n", -1);
	    return;

	case TWHILE:			/* WHILE and UNTIL functions	*/
	case TUNTIL:
	    Print_IString ((t->type == TWHILE) ? "while " : "until ", 1);
	    Print_ExTree (t->left);
	    Print_IString ("do\n", 0);
	    Print_ExTree (t->right);
	    Print_IString ("done\n", -1);
	    return;

	case TIF:			/* IF and ELSE IF functions	*/
	case TELIF:
	    if (t->type == TIF)
		Print_IString ("if\n", 1);

	    else
		Print_IString ("elif\n", 1);

	    Print_ExTree (t->left);

	    if (t->right != (C_Op *)NULL)
	    {
		Print_indent -= 1;
		Print_IString ("then\n", 1);
		Print_ExTree (t->right->left);

		if (t->right->right != (C_Op *)NULL)
		{
		    Print_indent -= 1;

		    if (t->right->right->type != TELIF)
			Print_IString ("else\n", 1);

		    Print_ExTree (t->right->right);
		}
	    }

	    if (t->type == TIF)
		Print_IString ("fi\n", -1);

	    return;

	case TCASE:			/* CASE function		*/
	    Print_IString ("case ", 1);
	    v1_puts (t->str);
	    v1a_puts (" do");
	    Print_Case (t->left);
	    Print_IString (" esac\n", -1);
	    return;

	case TBRACE:			/* {} statement			*/
	    Print_IString ("{\n", 1);
	    if (t->left != (C_Op *)NULL)
		Print_ExTree (t->left);

	    Print_IString ("}\n", -1);
	    return;
    }
}

/*
 * Print a command line
 */

static void	Print_Command (t)
register C_Op	*t;
{
    char	*cp;
    IO_Actions	**iopp;
    char	**wp = t->words;
    char	**owp = wp;

    if (t->type == TCOM)
    {
	while ((cp = *wp++) != (char *)NULL)
	    ;

	cp = *wp;

/* strip all initial assignments not correct wrt PATH=yyy command  etc */

	if ((cp == (char *)NULL) && (t->ioact == (IO_Actions **)NULL))
	{
	    Print_IString (null, 0);

	    while (*owp != (char *)NULL)
		v1a_puts (*(owp++));

	    return;
	}
    }

/* Parenthesis ? */

    if (t->type == TPAREN)
    {
	Print_IString ("(\n", 1);
	Print_ExTree (t->left);
	Print_IString (")", -1);
    }

    else
    {
	Print_IString (null, 0);

	while (*owp != (char *)NULL)
	{
	    v1_puts (*owp++);

	    if (*owp != (char *)NULL)
		v1_putc (SP);
	}
    }

/* Set up anyother IO required */

    if ((iopp = t->ioact) != (IO_Actions **)NULL)
    {
	while (*iopp != (IO_Actions *)NULL)
	    Print_IO (*iopp++);
    }

    v1_putc (NL);
}

/*
 * Print the IO re-direction
 */

static void		Print_IO (iop)
register IO_Actions	*iop;
{
    int		unit = iop->io_unit;
    static char	*cunit = " x";

    if (unit == IODEFAULT)	/* take default */
	unit = (iop->io_flag & (IOREAD | IOHERE)) ? STDIN_FILENO
						  : STDOUT_FILENO;

/* Output unit number */

    cunit[1] = (char)(unit + '0');
    v1_puts (cunit);

    switch (iop->io_flag)
    {
	case IOHERE:
	case IOHERE | IOXHERE:
	    v1_putc ('<');

	case IOREAD:
	    v1_putc ('<');
	    break;

	case IOWRITE | IOCAT:
	    v1_putc ('>');

	case IOWRITE:
	    v1_putc ('>');
	    break;

	case IODUP:
	    v1_puts (">&");
	    v1_putc (*iop->io_name);
	    return;
    }

    v1_puts (iop->io_name);
}

/*
 * Print out the contents of a case statement
 */

static void	Print_Case (t)
C_Op		*t;
{
    register C_Op	*t1;
    register char	**wp;

    if (t == (C_Op *)NULL)
	return;

/* type - TLIST - go down the left tree first and then processes this level */

    if (t->type == TLIST)
    {
	Print_Case (t->left);
	t1 = t->right;
    }

    else
	t1 = t;

/* Output the conditions */

    Print_IString (null, 0);

    for (wp = t1->words; *wp != (char *)NULL;)
    {
	v1_puts (*(wp++));

	if (*wp != (char *)NULL)
	    v1_puts (" | ");
    }

    v1a_puts (" )");
    Print_indent += 1;

/* Output the commands */

    Print_ExTree (t1->left);
    Print_IString (";;\n", -1);
}

/*
 * Print an indented string
 */

static void	Print_IString (cp, indent)
char		*cp;
int		indent;
{
    int		i;

    if (indent < 0)
	Print_indent += indent;

    for (i = 0; i < (Print_indent / 2); i++)
	v1_putc ('\t');

    if (Print_indent % 2)
	v1_puts ("    ");

    v1_puts (cp);

    if (indent > 0)
	Print_indent += indent;
}

/*
 * Look up a function in the save tree
 */

Fun_Ops		*Fun_Search (name)
char		*name;
{
    Fun_Ops	*fp;

    for (fp = fun_list; fp != (Fun_Ops *)NULL; fp = fp->next)
    {
	if (strcmp (*(fp->tree->words), name) == 0)
	    return fp;
    }

    return (Fun_Ops *)NULL;
}

/*
 * Save or delete a function tree
 */

void	Save_Function (t, delete_only)
C_Op	*t;
bool	delete_only;			/* True - delete		*/
{
    char		*name = *t->words;
    register Fun_Ops	*fp = Fun_Search (name);
    Fun_Ops		*p_fp = (Fun_Ops *)NULL;
    void		(*save_signal)(int);

/* Find the entry */

    for (fp = fun_list; (fp != (Fun_Ops *)NULL) &&
			(strcmp (*(fp->tree->words), name) != 0);
			p_fp = fp, fp = fp->next);

/* Disable signals */

    save_signal = signal (SIGINT, SIG_IGN);

/* If it already exists, free the tree and delete the entry */

    if (fp != (Fun_Ops *)NULL)
    {
	Set_Free_ExTree (fp->tree, Free_ExTree);

	if (p_fp == (Fun_Ops *)NULL)
	    fun_list = fp->next;

	else
	    p_fp->next = fp->next;

	DELETE (fp);
    }

/* Restore signals */

    signal (SIGINT, save_signal);

/* If delete only - exit */

    if (delete_only)
	return;

/* Create new entry */

    if ((fp = (Fun_Ops *)space (sizeof (Fun_Ops))) == (Fun_Ops *)NULL)
	return;

/* Disable signals */

    save_signal = signal (SIGINT, SIG_IGN);

/* Set up the tree */

    setarea ((char *)fp, 0);
    Set_Free_ExTree (t, Set_ExTree);

    fp->tree = t;
    fp->next = fun_list;
    fun_list = fp;

/* Restore signals */

    signal (SIGINT, save_signal);
}

/*
 * Set ExTree areas to zero function
 */

static void	Set_ExTree (s)
char		*s;
{
    setarea (s, 0);
}

/*
 * Free the ExTree function
 */

static void	Free_ExTree (s)
char		*s;
{
    DELETE (s);
}

/*
 * Set/Free function tree area by recursively processing of tree
 */

static void	Set_Free_ExTree (t, func)
C_Op		*t;
void		(*func)(char *);
{
    char		**wp;

    if (t == (C_Op *)NULL)
	return;

/* Check for start of print */

    if (t->type == TFUNC)
    {
	(*func)(*t->words);
	(*func)((char *)t->words);
	Set_Free_ExTree (t->left, func);
    }

/* Otherwise, process the tree and print it */

    switch (t->type)
    {
	case TPAREN:			/* ()			*/
	case TCOM:			/* A command process	*/
	    Set_Free_Command (t, func);
	    break;

	case TPIPE:			/* Pipe processing		*/
	case TLIST:			/* Entries in a for statement	*/
	case TOR:			/* || and &&			*/
	case TAND:
	case TWHILE:			/* WHILE and UNTIL functions	*/
	case TUNTIL:
	    Set_Free_ExTree (t->left, func);
	    Set_Free_ExTree (t->right, func);
	    break;

	case TFOR:			/* First part of a for statement*/
	    (*func)(t->str);

	    if ((wp = t->words) != (char **)NULL)
	    {
		while (*wp != (char *)NULL)
		    (*func) (*wp++);

		(*func)((char *)t->words);
	    }

	    Set_Free_ExTree (t->left, func);
	    break;

	case TIF:			/* IF and ELSE IF functions	*/
	case TELIF:
	    if (t->right != (C_Op *)NULL)
	    {
		Set_Free_ExTree (t->right->left, func);
		Set_Free_ExTree (t->right->right, func);
		(*func)((char *)t->right);
	    }

	case TBRACE:			/* {} statement			*/
	    Set_Free_ExTree (t->left, func);
	    break;

	case TCASE:			/* CASE function		*/
	    (*func)(t->str);
	    Set_Free_Case (t->left, func);
	    break;
    }

    (*func)((char *)t);
}

/*
 * Set/Free a command line
 */

static void	Set_Free_Command (t, func)
C_Op		*t;
void		(*func)(char *);
{
    IO_Actions	**iopp;
    char	**wp = t->words;

/* Parenthesis ? */

    if (t->type == TPAREN)
	Set_Free_ExTree (t->left, func);

    else
    {
	while (*wp != (char *)NULL)
	    (*func)(*wp++);

	(*func) ((char *)t->words);
    }

/* Process up any IO required */

    if ((iopp = t->ioact) != (IO_Actions **)NULL)
    {
	while (*iopp != (IO_Actions *)NULL)
	{
	    (*func)((char *)(*iopp)->io_name);
	    (*func)((char *)*iopp);
	    iopp++;
	}

	(*func)((char *)t->ioact);
    }
}

/*
 * Set/Free the contents of a case statement
 */

static void	Set_Free_Case (t, func)
C_Op		*t;
void		(*func)(char *);
{
    register C_Op	*t1;
    register char	**wp;

    if (t == (C_Op *)NULL)
	return;

/* type - TLIST - go down the left tree first and then processes this level */

    if (t->type == TLIST)
    {
	Set_Free_Case (t->left, func);
	t1 = t->right;
    }

    else
	t1 = t;

/* Set/Free the conditions */

    for (wp = t1->words; *wp != (char *)NULL;)
	(*func)(*(wp++));

    (*func)((char *)t1->words);

    Set_Free_ExTree (t1->left, func);
}
