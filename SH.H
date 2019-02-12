/* MS-DOS SHELL - Header File
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
 *    $Header: sh.h 1.20 90/05/31 09:47:41 MS_user Exp $
 *
 *    $Log:	sh.h $
 * Revision 1.20  90/05/31  09:47:41  MS_user
 * Implement partial write when swapping to disk
 *
 * Revision 1.19  90/05/15  21:08:41  MS_user
 * Release 1.6.2
 *
 * Revision 1.18  90/05/09  20:36:22  MS_user
 * Patchlvel for 1.6.1
 *
 * Revision 1.17  90/05/01  19:58:18  MS_user
 * Change Patchlevel
 *
 * Revision 1.16  90/04/25  22:33:03  MS_user
 * The function anys is now global
 *
 * Revision 1.15  90/04/25  10:58:05  MS_user
 * Add offset in buffer to IO_Args to handle TEXT mode
 *
 * Revision 1.14  90/04/25  09:21:35  MS_user
 * Change version message processing
 *
 * Revision 1.13  90/04/03  18:07:24  MS_user
 * Patchlevel
 *
 * Revision 1.12  90/03/27  20:25:29  MS_user
 * Clear_Extended_File required by sh1 for Interrupt cleanup
 *
 * Revision 1.11  90/03/26  20:58:24  MS_user
 * Change I/O restore so that "exec >filename" works
 *
 * Revision 1.10  90/03/26  04:31:32  MS_user
 * Remove original Interrupt 24 save address
 *
 * Revision 1.9  90/03/16  21:25:55  MS_user
 * Create the MAYBE boolean for stopping the removal of NL by grave.
 *
 * Revision 1.8  90/03/14  19:29:42  MS_user
 * Add some new functions and Buffered Output typedef
 * Add IOTHERE for <<- redirection
 *
 * Revision 1.7  90/03/09  16:10:07  MS_user
 * Add build file name function
 *
 * Revision 1.6  90/03/07  09:43:00  MS_user
 * Change PATCHLEVEL
 *
 * Revision 1.5  90/03/06  15:09:09  MS_user
 * Add some new procedures
 *
 * Revision 1.4  90/02/22  16:40:49  MS_user
 * Add XMS Support
 *
 * Revision 1.3  90/02/16  16:58:48  MS_user
 * Set up 1.5 release
 *
 * Revision 1.2  90/02/14  04:45:33  MS_user
 * Add Interrupt 24 processing and clean up interrupt 0 and 23 processing
 *
 * Revision 1.1  90/01/29  17:46:51  MS_user
 * Initial revision
 *
 *
 */

#define PATCHLEVEL	6
#define	LINE_MAX	1024	/* Command line length			*/
#define HISTORY_MAX	100	/* History array length			*/
				/* Space for full file name		*/
#define FFNAME_MAX	(PATH_MAX + NAME_MAX + 4)
#define CMD_LINE_MAX	2048	/* Max command line length		*/
#define SSAVE_IO_SIZE	4	/* Save IO array malloc increment	*/

#define	NPUSH		8	/* limit to input nesting		*/

#define	NOFILE		20	/* Number of open files			*/
#define	NUFILE		10	/* Number of user-accessible files	*/
#define	FDBASE		10	/* First file usable by Shell		*/

#define NL		'\n'
#define SP		' '
#define	NOT		'^'

#define BATCHEXT        ".cmd"

/* Here we introduce a new boolean value - MAYBE.  This is required for
 * a special case of the grave function
 */

#define MAYBE		(bool)(-1)

				/* Open in create mode			*/
#define O_CMASK		(O_WRONLY | O_CREAT | O_TRUNC | O_TEXT)
				/* Open in create mode for a pipe	*/
#define O_PMASK		(O_RDWR | O_CREAT | O_TRUNC | O_TEXT)
				/* Open in create mode for swap file	*/
#define O_SMASK		(O_RDWR | O_CREAT | O_TRUNC | O_BINARY)
#define O_SaMASK	(O_RDWR | O_BINARY)
#define O_RMASK		(O_RDONLY | O_NOINHERIT | O_TEXT)

/*
 * shell components
 */

#define	QUOTE		0200
#define	CMASK		0377
#define	QMASK		(CMASK & ~QUOTE)

#define	NOBLOCK		((C_Op *)NULL)
#define	NOWORD		((char *)NULL)
#define	NOWORDS		((char **)NULL)
#define	NOPIPE		(-1)

/*
 * Description of a command or an operation on commands.
 * Might eventually use a union.
 */

typedef struct op {
    int			type;		/* operation type, see below	*/
    char		**words;	/* arguments to a command	*/
    struct ioword	**ioact;	/* IO actions (eg, < > >>)	*/
    struct op		*left;
    struct op		*right;
    char		*str;		/* identifier for case and for	*/
} C_Op;

#define	TCOM		1	/* command				*/
#define	TPAREN		2	/* (c-list)				*/
#define	TPIPE		3	/* a | b				*/
#define	TLIST		4	/* a [&;] b				*/
#define	TOR		5	/* ||					*/
#define	TAND		6	/* &&					*/
#define	TFOR		7	/* FOR					*/
#define	TDO		8	/* DO					*/
#define	TCASE		9	/* CASE					*/
#define	TIF		10	/* IF					*/
#define	TWHILE		11	/* WHILE				*/
#define	TUNTIL		12	/* UNTIL				*/
#define	TELIF		13	/* ELSE IF				*/
#define	TPAT		14	/* pattern in case			*/
#define	TBRACE		15	/* {c-list}				*/
#define	TASYNC		16	/* c &					*/
#define	TFUNC		17	/* c () {c-list}			*/

/* Built in Command list */

struct	builtin {
    char	*command;
    int		(*fn)(C_Op *);
};

/*
 * actions determining the environment of a process
 */

#define	FEXEC		0x0001	/* execute without forking		*/

extern unsigned int	SW_intr;	/* interrupt pending		*/

/*
 * flags to control evaluation of words
 */

#define	DOSUB		0x01	/* interpret $, `, and quotes		*/
#define	DOBLANK		0x02	/* perform blank interpretation		*/
#define	DOGLOB		0x04	/* interpret [?*			*/
#define	DOKEY		0x08	/* move words with `=' to 2nd arg. list */
#define	DOTRIM		0x10	/* trim resulting string		*/

#define	DOALL		(DOSUB | DOBLANK | DOGLOB | DOKEY | DOTRIM)

extern char		**dolv;		/* $<numeric> values		*/
extern int		dolc;		/* $<numeric> count		*/
extern int		fn_area_number;	/* Next function area number	*/
extern int		exstat;
extern char		gflg;
extern bool		talking;	/* interactive (talking-type	*/
extern int		execflg;
extern int		multiline;	/* \n changed to ;		*/
extern int		*failpt;
extern int		*errpt;
extern int		inparse;	/* In parser flag		*/
extern int		Current_Event;	/* Current history event	*/

/*
 * Buffered Output function
 */

#define BIO_LENGTH	20		/* BIO size			*/

typedef struct	Out_Buffer {
    int		ob_fid;			/* File handler			*/
    char	*ob_start;		/* Start of buffer		*/
    char	*ob_cur;		/* Current position		*/
} Out_Buf;

/*
 * Break/Continue (in for and while), Return and Exit handler
 */

typedef struct brkcon {
    jmp_buf		brkpt;
    struct brkcon	*nextlev;
} Break_C;
				/* Values returned by longjmp		*/
#define BC_LOAD		0	/* Load condition			*/
#define BC_BREAK	1	/* Break condition			*/
#define BC_CONTINUE	2	/* Continue condition			*/

extern Break_C	*Break_List;	/* Break list for FOR/WHILE		*/
extern Break_C	*Return_List;	/* Return list for RETURN		*/
extern Break_C	*SShell_List;	/* SubShell list for EXIT		*/
extern bool	level0;		/* Level zero (read profile)		*/
extern bool	r_flag;		/* Read only shell			*/
extern bool	History_Enabled;

/*
 * Save Standard Input/Output/Error structure
 */

typedef struct save_io {
    int		depth;			/* Execute recursive depth	*/
    int		fp[STDERR_FILENO + 1];	/* File handlers		*/
} Save_IO;

extern Save_IO	*SSave_IO;		/* Save IO array		*/
extern int	NSave_IO_E;		/* Number of entries		*/
extern int	MSave_IO_E;		/* Max Number of entries	*/

/*
 * Function tree processing
 */

typedef struct fun_op {
    struct fun_op	*next;		/* Link				*/
    C_Op		*tree;		/* The tree itself		*/
} Fun_Ops;

extern Fun_Ops	*fun_list;		/* List header			*/

/*
 * redirection
 */

typedef struct ioword {
    short	io_unit;	/* unit affected			*/
    short	io_flag;	/* action (below)			*/
    char	*io_name;	/* file name				*/
} IO_Actions;

#define	IOREAD		0x01	/* <					*/
#define	IOHERE		0x02	/* << (here file)			*/
#define	IOWRITE		0x04	/* >					*/
#define	IOCAT		0x08	/* >>					*/
#define	IOXHERE		0x10	/* ${}, ` in <<				*/
#define	IODUP		0x20	/* >&digit				*/
#define	IOCLOSE		0x40	/* >&-					*/
#define	IOTHERE		0x80	/* <<- (here file			*/

#define	IODEFAULT	(-1)	/* token for default IO unit		*/

/*
 * parsing & execution environment
 */

typedef struct env {
    char	*cline;			/* Current line buffer		*/
    char	*linep;			/* Current pointer in line	*/
    char	*eline;			/* End of line pointer		*/
    struct io	*iobase;
    struct io	*iop;
    int		*errpt;
    bool	eof_p;			/* EOF processing enabled	*/
    int		iofd;
    struct env	*oenv;			/* Previous environment		*/
} Environ;

extern Environ		e;

/*
 * here documents
 */

typedef struct here {
    char		*h_tag;
    int			h_dosub;
    IO_Actions		*h_iop;
    struct here		*h_next;
} Here_D;

/*
 * flags:
 *
 * -a: Set all environment variables to exported
 * -e: Quit on error
 * -f: Disable file name expansion
 * -k: Look for name=value everywhere on command line
 * -n: No execution
 * -t: exit after reading and executing one command
 * -u: Abort if environment variable is not set
 * -v: Echo as read
 * -x: Trace
 */

#define FL_TEST(x)	(flags & (1L << ((x) - 'a')))
#define FL_SET(x)	flags |= (1L << ((x) - 'a'))
#define FL_CLEAR(x)	flags &= (~(1L << ((x) - 'a')))

extern long	flags;
extern char	*null;		/* null value for variable		*/
extern long	ourtrap;	/* Signal processing required		*/
extern int	trapset;	/* trap pending				*/
extern int	yynerrs;	/* yacc					*/
extern int	Execute_stack_depth;	/* execute function recursion	*/
					/* depth			*/

/*
 * Variable list
 */

typedef struct var {
    char		*value;		/* Value			*/
    char		*name;		/* Name				*/
    struct var		*next;		/* Link				*/
    char		status;		/* Type, see below		*/
} Var_List;

#define	COPYV		1	/* flag to setval, suggesting copy	*/
#define	RONLY		0x01	/* variable is read-only		*/
#define	EXPORT		0x02	/* variable is to be exported		*/
#define	GETCELL		0x04	/* name & value space was got with getcell */
#define PONLY		0x08	/* PATH Value - no unset		*/
#define C_MSDOS		0x10	/* Convert to MSDOS format		*/

extern Var_List	*vlist;		/* dictionary				*/
extern Var_List **S_UL;		/* Start address update location	*/
extern Var_List	*path;		/* search path for commands		*/
extern Var_List	*ps1;		/* Prompt 1				*/
extern Var_List	*ps2;		/* Prompt 2				*/
extern Var_List	*ifs;		/* Interfield separators		*/
extern Var_List	*C_dir;		/* Current directory			*/
extern char	*last_prompt;	/* Last prompt output			*/
extern char	*home;		/* Home string				*/
extern char	*shell;		/* Shell string				*/
extern char	*hsymbol;	/* Hash string				*/
extern char	*msymbol;	/* Minus string				*/
extern char	*Program_Name;	/* Program name from argv[0]		*/
extern char	*spcl2;
extern char	*history_file;

/*
 * SubShell Save Structure
 */

typedef struct subshell {
    int		depth;			/* Sub_Shell Depth		*/
    Var_List	*header;		/* Header start			*/
} S_SubShell;

extern S_SubShell	*SubShells;	/* Save Vars array		*/
extern int		NSubShells;	/* Number of entries		*/
extern int		MSubShells;	/* Max Number of entries	*/

/* io buffer */

typedef struct iobuf {
    unsigned int	id;		/* buffer id			*/
    char		buf[512];	/* buffer			*/
    char		*bufp;		/* pointer into buffer		*/
    char		*ebufp;		/* pointer to end of buffer	*/
} IO_Buf;

/* possible arguments to an IO function */

typedef struct ioarg {
    char		*aword;
    char		**awordlist;
    int			afile;		/* file descriptor		*/
    unsigned int	afid;		/* buffer id			*/
    long		afpos;		/* file position		*/
    int			afoff;		/* Offset in buffer		*/
    IO_Buf		*afbuf;		/* buffer for this file		*/
} IO_Args;

#define AFID_NOBUF	(~0)
#define AFID_ID		0

extern IO_Args	ioargstack[NPUSH];	/* IO argument stack		*/

/* an input generator's state */

typedef struct io {
    int			(*iofn)(struct io *);
    IO_Args		*argp;
    int			peekc;
    char		prev;		/* previous character read by readc() */
    char		nlcount;	/* for `'s			*/
    char		xchar;		/* for `'s			*/
    char		task;		/* reason for pushed IO		*/
    char		dflag;		/* Special processing flag	*/
} IO_State;

#define DSA_NULL	0x00		/* No special processing req	*/
#define DSA_STAR	0x01		/* Special processing for "$*"	*/
#define DSA_AMP		0x02		/* Special processing for "$@"	*/
#define DSA_MODE	0x03		/* Mode flag			*/
#define DSA_END		0x04		/* Last word processing		*/
#define DSA_START	0x08		/* First word processed?	*/
#define DSA_START1	0x10		/* Subsequent word processed	*/
#define DSA_END1	0x20		/* End processing for word	*/

extern IO_State		iostack[NPUSH];	/* IO Stack			*/

#define	XOTHER		0	/* none of the below			*/
#define	XDOLL		1	/* expanding ${}			*/
#define	XGRAVE		2	/* expanding `'s			*/
#define	XIO		3	/* file IO				*/

/* in substitution */

#define	INSUB()			(e.iop->task == XGRAVE || e.iop->task == XDOLL)

/*
 * IO control
 */

extern IO_Args		temparg;	/* temporary for PUSHIO */
#define	PUSHIO(what, arg, gen)		((temparg.what = (arg)),	\
					  pushio (&temparg, (gen)))

#define	RUN(what, arg, gen, loop)	((temparg.what = (arg)),	\
					  run (&temparg, (gen), loop))

typedef struct wdblock {
    short	w_bsize;
    short	w_nword;
    char	*w_words[1];
} Word_B;

extern Word_B	*wdlist;
extern Word_B	*iolist;

/*
 * storage allocation
 */

extern int		areanum;	/* current allocation area */

#define	NEW(type)	(type *)getcell (sizeof (type))
#define	DELETE(obj)	freecell ((char *)obj)

/* Functions */

extern void	main (int, char **);
extern void	setdash (void);
extern void	fail (void);
extern void	leave (void);
extern void	print_warn (char *, ...);
extern void	print_error (char *, ...);
extern bool	newenv (int);
extern void	quitenv (void);
extern char	*putn (int);
extern void	onintr (int);
extern char	*space (int);
extern char	*strsave (char *, int);
extern void	sig (int);
extern void	runtrap (int);
extern Var_List	*lookup (char *, bool);
extern void	setval (Var_List *, char *);
extern void	s_vstatus (Var_List *, int);
extern bool	isassign (char *);
extern bool	assign (char *, int);
extern bool	gmatch (char *, char *, bool);
extern char	*getcell (unsigned int);
extern void	freecell (char *);
extern void	freearea (int);
extern void	setarea (char *, int);
extern int	getarea (char *);
extern C_Op	*yyparse (void);
extern int	execute (C_Op *, int, int, int);
extern int	run (IO_Args *, int (*)(IO_State *), bool);
extern int	Getc (int);
extern void	unget (int);
extern int	eofc (void);
extern int	readc (void);
extern void	pushio (IO_Args *, int (*)(IO_State *));
extern int	nlchar (IO_State *);
extern int	wdchar (IO_State *);
extern int	dol_char (IO_State *);
extern int	strchar (IO_State *);
extern int	qstrchar (IO_State *);
extern int	filechar (IO_State *);
extern int	gravechar (IO_State *);
extern int	qgravechar (IO_State *);
extern int	sgravechar (IO_State *);
extern int	linechar (IO_State *);
extern void	closeall (void);
extern int	remap (int);
extern int	openpipe (void);
extern void	closepipe (int);
extern void	markhere (char *, IO_Actions *);
extern void	gethere (void);
extern int	herein (char *, int);
extern void	scraphere (void);
extern void	freehere (int);

extern char	**eval (char **, int);
extern char	**makenv (void);
extern char	*evalstr (char *, int);
extern int	subgetc (char, bool);
extern Word_B	*addword (char *, Word_B *);
extern char	**getwords (Word_B *);
extern void	put_prompt (char *);
extern bool	eqname (char *, char *);
extern bool	any (char, char *);
extern int	(*inbuilt (char *))();
extern char     *cmd_internal(char *s);
extern char	*path_append (char *, char *, char *);
extern void	unset (char *, bool);
extern int	S_open (bool, char *, int, ...);
extern int	S_close (int, bool);
extern int	S_dup (int);
extern int	S_dup2 (int, int);
extern int	S_Remap (int, int);
extern void	S_Delete (int);
extern void	Getcwd (void);
extern char	*g_tempname (void);
extern void	S_puts (char *);
extern void	S_putc (int);
extern bool	check_rsh (char *);
extern int	O_for_execute (char *, char **, int *);
extern int	SA_spawn (char **);
extern char	*findeq (char *);
extern int	restore_std (int, bool);
extern void	Load_History (void);
extern void	Dump_History (void);
extern void	Display_History (void);
extern void	Clear_History (void);
extern void	v1_puts (char *);
extern void	v1a_puts (char *);
extern void	v1_putc (char);
extern void	v1printf (char *, ...);
extern int	Get_stdin (IO_Args *);
extern int	Process_Escape (char **);
extern void	Add_History (bool);
extern void	Convert_Slashes (char *);
extern void	Print_ExTree (C_Op *);
extern Fun_Ops	*Fun_Search (char *);
extern void	Save_Function (C_Op *, bool);
extern int	getn (char *);
extern int	Create_NG_VL (void);
extern void	Delete_G_VL (void);
extern void	Restore_Dir (char *);
extern void	Restore_Environment (int, int);
extern int	sort_compare (char **, char **);
extern int	Check_Script (char *, char **, int *);
extern bool	Interactive (void);
extern void	In_Col_Zero (void);
extern char	*Build_H_Filename (char *);
extern void	Configure_Keys (void);
extern bool	Ring_Bell (void);
extern void	Close_buffer (Out_Buf *);
extern void	Add_buffer (char, Out_Buf *);
extern void	Adds_buffer (char *, Out_Buf *);
extern Out_Buf	*Open_buffer (int, bool);
extern int	collect (int, int);
extern void	Clear_Extended_File (void);
extern void	Print_Version (int);
extern bool	anys (char *, char *);
