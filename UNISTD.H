#ifndef _UNISTD_H
#define _UNISTD_H

/*  unistd.h  */

#include <sys/types.h>
#include <proto.h>
#include <changes.h>

/* Definition for NULL pointer */

#ifndef NULL
#  ifdef MSDOS
#    if (defined(M_I86SM) || defined(M_I86MM))
#      define NULL	0
#    else
#      if (defined(M_I86CM) || defined(M_I86LM) || defined(M_I86HM))
#        define NULL	0L
#      endif
#    endif
#  else
#    define NULL	0
#  endif
#endif

/*  for access(2)  */

#define R_OK     	4
#define W_OK     	2
#define X_OK     	1
#define F_OK     	0

/*  for lockf()  */

#define F_ULOCK		0
#define F_LOCK		1
#define F_TLOCK		2
#define F_TEST		3

/*  for lseek(2)  */

#ifndef SEEK_SET
#define SEEK_SET	0
#endif

#ifndef SEEK_CUR
#define SEEK_CUR	1
#endif

#ifndef SEEK_END
#define SEEK_END	2
#endif

/* STDIO definitions */

#define	STDIN_FILENO	0
#define	STDOUT_FILENO	1
#define	STDERR_FILENO	2

/* Standard paths */

#define GF_PATH		"/etc/group"
#define PF_PATH		"/etc/passwd"
#define IF_PATH		"/usr/include"
#define SF_PATH		"/etc/shadow"

/* POSIX definitions */

#define _POSIX_VERSION		198803L
#undef	_POSIX_CHOWN_RESTRICTED
#undef	_POSIX_JOB_CONTROL
#define	_POSIX_NO_TRUNC		1
#define _POSIX_SAVED_IDS	1
#undef	_POSIX_VDISABLE
/* #define _XOPEN_VERSION		*/

/* sysconf values */

#define _SC_ARG_MAX		0
#define _SC_CHILD_MAX		1
#define _SC_CLK_TCK		2
#define _SC_NGROUPS_MAX		3
#define _SC_OPEN_MAX		4
#define _SC_JOB_CONTROL		5
#define _SC_SAVED_IDS		6
#define _SC_VERSION		7
#define _SC_PASS_MAX		8
#define _SC_XOPEN_VERSION	9

/* pathconf values */

#define _PC_LINK_MAX		0
#define _PC_MAX_CANON		1
#define _PC_MAX_INPUT		2
#define _PC_NAME_MAX		3
#define _PC_PATH_MAX		4
#define _PC_PIPE_BUF		5
#define _PC_CHOWN_RESTRICTED	6
#define _PC_NO_TRUNC		7
#define _PC_VDISABLE		8

/* confstring values */

#define _CS_PATH		1

/* Function declarations */

extern size_t _CDECL	confstring	_PROTO ((int, char *, size_t));
extern void _CDECL	abort		_PROTO ((void));
#ifdef MSDOS
extern int _CDECL	chsize		_PROTO ((int, off_t));
#else
extern int _CDECL	chsize		_PROTO ((char *, off_t));
#endif
extern void _CDECL	sync		_PROTO ((void));

/* --- Process creation and execution --- */
extern pid_t _CDECL	fork		_PROTO ((void));
extern pid_t _CDECL	vfork		_PROTO ((void));
extern int _CDECL	execl		_PROTO ((char *, char *, ...));
extern int _CDECL	execle		_PROTO ((char *, char *, ...));
extern int _CDECL	execlp		_PROTO ((char *, char *, ...));
extern int _CDECL	execlpe		_PROTO ((char *, char *, ...));
extern int _CDECL	execv		_PROTO ((char *, char **));
extern int _CDECL	execve		_PROTO ((char *, char **, char **));
extern int _CDECL	execvp		_PROTO ((char *, char **));
extern int _CDECL	execvpe		_PROTO ((char *, char **, char **));

/* --- Process termination --- */
extern void _CDECL	_exit		_PROTO ((int));
extern void _CDECL	exit		_PROTO ((int));

/* --- Timer operations --- */
extern unsigned int _CDECL	alarm	_PROTO ((unsigned int));
extern int _CDECL		pause	_PROTO ((void));
extern unsigned int _CDECL	sleep	_PROTO ((unsigned int));

/* --- Process identification --- */
extern pid_t _CDECL	getpid		_PROTO ((void));
extern pid_t _CDECL	getppid		_PROTO ((void));

/* --- User identification --- */
extern uid_t _CDECL	getuid		_PROTO ((void));
extern uid_t _CDECL	geteuid		_PROTO ((void));
extern gid_t _CDECL	getgid		_PROTO ((void));
extern gid_t _CDECL	getegid		_PROTO ((void));
extern int _CDECL	setuid		_PROTO ((uid_t));
extern int _CDECL	setgid		_PROTO ((gid_t));
extern int _CDECL	getgroups	_PROTO ((int, gid_t *));
extern char * _CDECL	getlogin	_PROTO ((void));
extern char * _CDECL	cuserid		_PROTO ((char *));

/* --- Process groups --- */
extern pid_t _CDECL	getpgrp		_PROTO ((void));
extern pid_t _CDECL	setsid		_PROTO ((void));
extern int _CDECL	setpgid		_PROTO ((pid_t, pid_t));

/* --- Terminal identification --- */
extern char * _CDECL	ctermid	_PROTO ((char *));
extern char * _CDECL	ttyname	_PROTO ((int));
extern int _CDECL	isatty		_PROTO ((int));

/* --- Configurable system variables --- */
extern long _CDECL	sysconf		_PROTO ((int));

/* --- Working directory --- */
extern int _CDECL	chdir		_PROTO ((char *));
extern char * _CDECL	getcwd		_PROTO ((char *, int));

/* --- General file creation --- */
extern int _CDECL	link		_PROTO ((char *, char *));
extern int _CDECL	rename		_PROTO ((const char *, const char *));
extern char * _CDECL	mktemp		_PROTO ((char *));

/* --- File removal --- */
extern int _CDECL	unlink		_PROTO ((const char *));
extern int _CDECL	remove		_PROTO ((const char *));
extern int _CDECL	rmdir		_PROTO ((char *));

/* --- File characteristics --- */
extern int _CDECL	access		_PROTO ((char *, int));
extern int _CDECL	chown		_PROTO ((char *, uid_t, gid_t));
extern long _CDECL	tell 		_PROTO ((int));

/* --- Configurable pathname variables --- */
extern long _CDECL	pathconf	_PROTO ((char *, int));
extern long _CDECL	fpathconf	_PROTO ((int, int));

/* --- Pipes --- */
extern int _CDECL	pipe		_PROTO ((int[2]));

/* --- File descriptor manipulation --- */
extern int _CDECL	dup		_PROTO ((int));
extern int _CDECL	dup2		_PROTO ((int, int));

/* --- File descriptor deassignment --- */
extern int _CDECL	close		_PROTO ((int));

/* --- Input and output --- */
extern int _CDECL	read		_PROTO ((int, char *, unsigned int));
extern int _CDECL	write		_PROTO ((int, char *, unsigned int));

/* --- Control operations on files --- */
extern off_t _CDECL	lseek		_PROTO ((int, off_t, int));
#endif
