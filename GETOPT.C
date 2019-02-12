/*
 *  MODULE NAME:   getopt.c				Revision 1.0
 *
 *  AUTHOR:        I. Stewartson
 *                 Data Logic Ltd.,
 *                 Queens House,
 *                 Greenhill Way,
 *                 Harrow,
 *                 Middlesex HA1 1YR.
 *                 Telephone: London (01) 863 0383
 *
#include <logo.h>
 *  MODULE DESCRIPTION: This function is based on the UNIX library function.
 *			getopt return the next option letter in argv that
 *			matches a letter in opstring.  optstring is a string
 *			of recognised option letters; if a letter is followed
 *			by a colon, the option is expected to have an argument
 *			that may or may not be separated from it by white
 *			space.  optarg is set to point to the start of the
 *			option argument on return from getopt.
 *
 *			getopt places in optind the argv index of the next
 *			argument to be processed.  Because optind is external,
 *			it is normally initialised to zero automatically before
 *			the first call to getopt.
 *
 *			When all options have been processed (i.e. up to the
 *			first non-option argument), getopt returns EOF.  The
 *			special option -- may be used to delimit the end of
 *			the options; EOF will be returned, and -- will be
 *			skipped.
 *
 *			getopt prints an error message on stderr and returns a
 *			question mark (?) when it encounters an option letter
 *			not included in optstring.  This error message may be
 *			disabled by setting opterr to a non-zero value.
 *
 *  CALLING SEQUENCE:	The following calling sequences are used:
 *
 *			int	getopt(argc, argv, optstring)
 *			int	argc;
 *			char	**argv;
 *			char	*optstring;
 *
 *  ERROR MESSAGES:
 *			%s: illegal option -- %c
 *			%s: option requires an argument -- %c
 *
 *  INCLUDE FILES:
 */

#include <stdio.h>			/* Standard Input/Output	*/
#include <string.h>			/* String function declarations	*/
#include <stdlib.h>			/* Standard library declarations*/

/*
 *  DATA DECLARATIONS:
 */

int		opterr = 0;
int		optind = 1;
int		optopt;
int		optvar = 0;
char		*optarg;

static char	*errmes1 = "%s: illegal option -- %c\n";
static char	*errmes2 = "%s: option requires an argument -- %c\n";

/*
 *  MODULE ABSTRACT:
 *
 *  EXECUTABLE CODE:
 */

int	getopt(argc, argv, optstring)
int	argc;				/* Argument count		*/
char	**argv;				/* Argument string vector	*/
char	*optstring;			/* Valid options		*/
{
    static int	string_off = 1;		/* Current position		*/
    int		cur_option;		/* Current option		*/
    char	*cp;			/* Character pointer		*/

    if (string_off == 1)
    {
	if ((optind >= argc) || (argv[optind][0] != '-') || (!argv[optind][1]))
	    return (EOF);

	else if (!strcmp(argv[optind], "--"))
	{
	    optind++;
	    return (EOF);
	}
    }

/* Get the current character from the current argument vector */

    optopt = cur_option = argv[optind][string_off];

/* Validate it */

    if ((cur_option == ':') || ((cur_option == '*') && optvar) ||
	((cp = strchr(optstring, cur_option)) == (char *)NULL))
    {
	if (opterr)
	    fprintf(stderr, errmes1, cur_option, argv[0]);

	if (!argv[optind][++string_off])
	{
	    optind++;
	    string_off = 1;
	}

	return ('?');
    }

/* Parameters following ? */

    if (*(++cp) == ':')
    {
	if (argv[optind][string_off + 1])
	    optarg = &argv[optind++][string_off + 1];

	else if (++optind >= argc)
	{
	    if (opterr)
		fprintf(stderr, errmes2, cur_option, argv[0]);

	    string_off = 1;
	    return ('?');
	}

	else
	    optarg = argv[optind++];

	string_off = 1;
    }

    else if ((*cp == '*') && optvar)
    {
	if (argv[optind][string_off + 1] != 0)
	    optarg = &argv[optind++][string_off + 1];
	else
	{
	    optarg = "";
	    optind++;
	    string_off = 1;
	}
    }

    else
    {
	if (!argv[optind][++string_off])
	{
	    string_off = 1;
	    optind++;
	}

	optarg = (char *)NULL;
    }

    return (cur_option);
}
