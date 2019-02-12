/*
 *  MODULE NAME:     expand.c  					  Revision: 1.0
 *
 *  AUTHOR:	     Ian Stewartson
 *
 *  LOCATION:	     Data Logic,
 *		     Greenford,
 *		     Middlesex,
 *		     England.
 *
#include <logo.h>
 *  MODULE DEFINITION:	This function expandes the command line parameters
 *			in a UNIX like manner.	Wild character *?[] are
 *			allowed in file names. @filename causes command lines
 *			to be read from filename.  Strings between " or ' are
 *			not expanded.  All entries in the array are malloced.
 *
 *			This function replaces the standard MS-DOS command
 *			line processing function (_setargv in stdargv.obj).
 *
 *  CALLING SEQUENCE:	The following calling sequences are used:
 *
 *			void	_setargv ();
 *
 *  ERROR MESSAGES:	Out of memory
 *
 *  INCLUDE FILES:
 */

#include <sys/types.h>			/* MS-DOS type definitions          */
#include <sys/stat.h>			/* File status definitions	    */
#include <stdio.h>			/* Standard I/O delarations         */
#include <stdlib.h>			/* Standard library functions       */
#include <errno.h>			/* Error number declarations        */
#include <ctype.h>			/* Character type declarations      */
#include <string.h>			/* String library functions         */
#include <limits.h>			/* String library functions         */
#include <fcntl.h>			/* File Control Declarations        */
#include <io.h>				/* Input/Output Declarations        */
#include <changes.h>
#include <dir.h>			/* Direction I/O functions	    */

#define INCL_NOPM
#define INCL_DOS
#include <os2.h>

/*
 *  DATA DEFINITIONS:
 */

#define MAX_LINE	256		/* Max line length		*/
#define S_ENTRY		sizeof (char *)

/*
 *  DATA DECLARATIONS:
 */
#ifdef MSDOS

extern void	_setargv (void);
static void	exp_line (char *);		/* Expand file		*/
static int	ex_pfield (char	*, char *);	/* Expand field		*/
static void	ex_pfile (char *);
static char	*ex_gspace (int, char *);	/* Get space		*/
static void	ex_add_arg (char *);	/* Add argument			*/
static char	*ex_skip_sp (char *);	/* Skip spaces			*/
static char	*ex_tounix (char *);	/* Convert name to Unix format	*/
static int	ex_find (char*, int);	/* Split file name		*/
static void	ex_fatal (int, char *, char *);	/* Fatal error processing*/
static char	*ex_environment (char *);	/* Process environment	*/
static char	*_ex_multi_drive (char *);	/* Check for multidrive	*/
static char	*ex_nomem = "%s: %s\n";

extern char far	*_pgmptr; 		/* Program name			*/
extern char	**__argv; 		/* Current argument address	*/
extern int	__argc; 		/* Current argument count	*/

extern unsigned _aenvseg;
extern unsigned _acmdln;

/*
 *  MODULE ABSTRACT: _setargv
 *
 *  UNIX like command line expansion
 */

void	_setargv ()
{
					/* Set up pointer to command line */
    char far		*argvp = MAKEP(_aenvseg, _acmdln);
    char far		*s; 		/* Temporary string pointer    	*/
#ifndef M_I86LM
    char		buf[MAX_LINE];	/* Temporary space		*/
    char		*cp;
#endif

/* Command line can be null or 0x0d terminated - convert to null */

    s = argvp;

    while (*s && (*s != 0x0d))
	++s;

    if (*s == 0x0d)
	*s = 0;

/* Set up global parameters and expand */

    __argc = 0;

/* Get the program name */

    if (_osmajor <= 2)
	s = "unknown";

/* In the case of DOS 3+, we look in the environment space */

    else
	for ( s = argvp; *(s - 1); s-- );

    _pgmptr = s;

#ifndef M_I86LM
    cp = buf;
    while (*(cp++) = *(s++));

    ex_add_arg (ex_tounix (buf));	/* Add the program name		*/

    s  = argvp;
    cp = buf;
    while (*(cp++) = *(s++));

    exp_line (buf);
#else
    ex_add_arg (ex_tounix (s));		/* Add the program name		*/
    exp_line (argvp);
#endif

    ex_add_arg ((char *)NULL);
    --__argc;
}

/*
 * Expand a line
 */

static void	exp_line (argvp)
char		*argvp;			/* Line to expand    		*/
{
    char	*spos;			/* End of string pointer	*/
    char	*cpos;			/* Start of string pointer	*/
    char	*fn;			/* Extracted file name string	*/

/* Search for next separator */

    spos = argvp;

    while (*(cpos = ex_skip_sp (spos)))
    {

/* Extract string argument */

	if ((*cpos == '"') || (*cpos == '\''))
	{
	    spos = cpos + 1;

	    do
	    {
		if ((spos = strchr (spos, *cpos)) != NULL)
		{
		    spos++;
		    if (spos[-2] != '\\')
			break;
		}

		else
		    spos = &spos[strlen (cpos)];

	    }
	    while (*spos);

	    fn	= ex_gspace (spos - cpos - 2, cpos + 1);
	}

/* Extract normal argument */

	else
	{
	    spos = cpos;
	    while (!isspace(*spos) && *spos)
		spos++;

	    fn = ex_gspace (spos - cpos, cpos);
	}

/* Process argument */

	if (*cpos != '"')
	    fn = ex_environment (fn);

	switch (*cpos)
	{
	    case '@':		/* Expand file	    			*/
		ex_pfile (fn);
		break;

	    case '"':		/* Expand string	    		*/
	    case '\'':
		ex_add_arg (fn);
		break;

	    default:		/* Expand field	    			*/
		if (!ex_find (fn, 0))
		    ex_add_arg (fn);
	}

	free (fn);
    }
}

/* Expand a field if it has metacharacters in it */

static int	ex_pfield (prefix, postfix)
char		*prefix;		/* Prefix field	    		*/
char		*postfix;		/* Postfix field	    	*/
{
    int 	 	count;		/* File path length		*/
    int 		f_count = 0;	/* Number of files generated    */
    int			slash_flag = 0;	/* slash required		*/
    char		fn[PATH_MAX + NAME_MAX + 2];/* Search file name */
    char		*name;		/* Match string			*/
    char		*p, *p1;
    DIR			*dp;
    struct direct	*c_de;
    unsigned int	c_drive;	/* Current drive		*/
    unsigned int	m_drive;	/* Max drive			*/
    unsigned int	s_drive;	/* Selected drive		*/
    unsigned int	x_drive, y_drive;	/* Dummies		*/
    char		*multi;		/* Multi-drive flag		*/
    char		t_drive[2];
    ULONG               l_map;
    int                 cnt;

/* Convert file name to lower case */

    strlwr (prefix);

/* Search all drives ? */

    if ((multi = _ex_multi_drive (prefix)) != (char *)NULL)
    {
        DosQCurDisk((PUSHORT) &c_drive, &l_map);
	t_drive[1] = 0;

        for ( cnt = 1; cnt <= 26; cnt++, l_map >>= 1 )
          if ( l_map & 1L )
	  {
	      t_drive[0] = (char)(cnt + 'a' - 1);
              *multi = 0;

	      if (pnmatch (t_drive, prefix, 0))
	      {
                *multi = ':';
	  	fn[0] = t_drive[0];
	  	strcpy (fn + 1, multi);
	  	f_count += ex_pfield (fn, postfix);
	      }
              else
                *multi = ':';
	  }

	return f_count;
    }

/* Get the path length */

    p = strrchr (prefix, '/');
    p1 = strchr (prefix, ':');

    if ((p1 == (char *)NULL) || (p1 < p))
    {
	if (p == (char *)NULL)
	{
	    count = 0;
	    name = prefix;
	}

	else
	{
	    count = p - prefix;
	    name = p + 1;
	}
    }

    else if ((p == (char *)NULL) || (p < p1))
    {
	count = p1 - prefix;
	name = p1 + 1;
    }

/* Set up file name for search */

    if (((count == 2) && (strncmp (prefix + 1, ":/", 2) == 0)) ||
	((count == 0) && (*prefix == '/')))
    {
	strncpy (fn, prefix, ++count);
	fn[count] = 0;
	strcat (fn, ".");
    }

    else
    {
	if ((count == 1) && (*(prefix + 1) == ':'))
	    count++;

	strncpy (fn, prefix, count);
	fn[count] = 0;

	if (((count == 2) && (*(prefix + 1) == ':')) || (count == 0))
	    strcat (fn, ".");

	else
	    slash_flag = 1;
    }

/* Search for file names */

    if ((dp = opendir (fn)) == (DIR *)NULL)
	return 0;

/* Are there any matches */

    while ((c_de = readdir (dp)) != (struct direct *)NULL)
    {
	if ((*c_de->d_name == '.') && (*name != '.'))
	    continue;

/* Check for match */

	if (pnmatch (c_de->d_name, name, 0))
	{
	    fn[count] = 0;

	    if (slash_flag)
		strcat (fn, "/");

	    strcat (fn, c_de->d_name);

/* If the postfix is not null, this must be a directory */

	    if (postfix != (char *)NULL)
	    {
		struct stat		statb;

		if (stat (fn, &statb) < 0 ||
		    (statb.st_mode & S_IFMT) != S_IFDIR)
		    continue;

		strcat (fn, "/");
		strcat (fn, postfix);
	    }

	    f_count += ex_find (fn, 1);
	}
    }

    closedir (dp);
    return f_count;
}

/* Expand file name */

static void	ex_pfile (file)
char		*file;		/* Expand file name	    		*/
{
    FILE    	*fp;		/* File descriptor	    		*/
    char	*p;		/* Pointer				*/
    int		c_maxlen = MAX_LINE;
    char	*line;		/* Line buffer		    		*/

/* Grab some memory for the line */

    if ((line = malloc (c_maxlen)) == (char *)NULL)
	ex_fatal (ENOMEM, ex_nomem, (char *)NULL);

/* If file open fails, expand as a field */

    if ((fp = fopen (file + 1, "rt")) == NULL)
    {
	if (!ex_find (file, 0))
	    ex_add_arg (file);

	return;
    }

/* For each line in the file, remove EOF characters and add argument */

    while (fgets (line, c_maxlen, fp) != (char *)NULL)
    {
	while ((p = strchr (line, '\n')) == (char *)NULL)
	{
	    if ((p = strchr (line, 0x1a)) != (char *)NULL)
		break;

	    if ((line = realloc (line, c_maxlen + MAX_LINE)) == (char *)NULL)
		ex_fatal (ENOMEM, ex_nomem, (char *)NULL);

	    if (fgets (&line[c_maxlen - 1], MAX_LINE, fp) == (char *)NULL)
		break;

	    c_maxlen += MAX_LINE - 1;
	}

	if (p != (char *)NULL)
	    *p = 0;

	ex_add_arg (line);
    }

    if (ferror(fp))
	ex_fatal (errno, "%s: %s (%s)\n", file + 1);

    free (line);
    fclose (fp);
}

/* Get space for name */

static char	*ex_gspace (l, in_s)
int		l;			/* String length                */
char		*in_s;                  /* String address		*/
{
    char	*out_s;			/* Malloced space address       */

    if ((out_s = malloc (l + 1)) == (char *)NULL)
	ex_fatal (ENOMEM, ex_nomem, (char *)NULL);

/* Copy string for specified length */

    strncpy (out_s, in_s, l);
    out_s[l] = 0;

    return (out_s);
}

/* Append an argument to the string */

static void	ex_add_arg (fn)
char		*fn;			/* Argument to add		*/
{
    if (__argc == 0)
	__argv = (char **)malloc (50 * S_ENTRY);

    else if ((__argc % 50) == 0)
	__argv = (char **)realloc (__argv, (__argc + 50) * S_ENTRY);

    if (__argv == (char **)NULL)
	ex_fatal (ENOMEM, ex_nomem, (char *)NULL);

    __argv[__argc++] = (fn == (char *)NULL) ? fn : ex_gspace (strlen (fn), fn);
}

/*  Skip over spaces */

static char	*ex_skip_sp (a)
char		*a;			/* String start address		*/
{
    while (isspace(*a))
        a++;

    return (a);
}

/* Convert name to Unix format */

static char	*ex_tounix (a)
char		*a;
{
    char	*sp = a;

    while ((a = strchr (a, '\\')) != (char *)NULL)
	*(a++) = '/';

    return strlwr (sp);
}

/* Find the location of meta-characters.  If no meta, add the argument and
 * return NULL.  If meta characters, return position of end of directory
 * name.  If not multiple directories, return -1
 */

static int	ex_find (file, must_exist)
char		*file;
int		must_exist;		/* FIle must exist flag		*/
{
    char	*p;
    int		i;
    static char	ex_meta[] = "?*[]\\";	/* Metacharacters		*/

    if ((p = strpbrk (file, ex_meta)) == (char *)NULL)
    {
	if (must_exist && (access (file, 0) < 0))
	    return 0;

	ex_add_arg (file);
	return 1;
    }

    else if ((p = strchr (p, '/')) != (char *)NULL)
	*(p++) = 0;

    i = ex_pfield (file, p);

    if (p != (char *)NULL)
       *(--p) = '/';

    return i;
}

/* Fatal errors */

static void	ex_fatal (ecode, format, para)
int		ecode;
char		*format;
char		*para;
{
    fprintf (stderr, format, "stdargv", strerror (ecode), para);
    exit (1);
}

/* Process Environment - note that field is a malloc'ed field */

static char	*ex_environment (field)
char		*field;
{
    char	*sp, *cp, *np, *ep;
    char	save;
    int		b_flag;

    sp = field;

/* Replace any $ strings */

    while ((sp = strchr (sp, '$')) != (char *)NULL)
    {
	if (*(cp = ++sp) == '{')
	{
	    b_flag = 1;
	    ++cp;

	    while (*cp && (*cp != '}'))
		cp++;
	}

	else
	{
	    b_flag;

	    while (isalnum(*cp))
		cp++;
	}

/* Grab the environment variable */

	if (cp == sp)
	    continue;

	save = *cp;
	*cp = 0;
	ep = getenv (sp + b_flag);
	*cp = save;

	if (ep != (char *)NULL)
	{
	    np = ex_gspace (strlen(field) - (cp - sp) + strlen (ep) - 1, field);
	    strcpy (&np[sp - field - 1], ep);
	    ex_tounix (&np[sp - field - 1]);
	    free (field);
	    strcpy ((sp = &np[strlen(np)]), cp + b_flag);
	    field = np;
	}
    }

    return field;
}

/* Check for multi_drive prefix */

static char	*_ex_multi_drive (prefix)
char		*prefix;
{
    if (strlen (prefix) < 2)
	return (char *)NULL;

    if (((*prefix == '*') || (*prefix == '?')) && (prefix[1] == ':'))
	return prefix + 1;

    if (*prefix != '[')
	return (char *)NULL;

    while (*prefix && (*prefix != ']'))
    {
	if ((*prefix == '\\') && (*(prefix + 1)))
	    ++prefix;

	++prefix;
    }

    return (*prefix && (*(prefix + 1) == ':')) ? prefix + 1 : (char *)NULL;
}
#endif
