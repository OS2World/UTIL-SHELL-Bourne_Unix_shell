extern int _CDECL	open	(char *, int, ...);
extern int _CDECL	creat	(char *, int);

#define PATH_MAX	256		/* Maximum path length		*/
#define NAME_MAX	64		/* Maximum file name length	*/

extern FILE * _CDECL	popen(char *, char *);
extern int _CDECL	pclose(FILE *);

extern int _CDECL		optind;
extern int _CDECL		opterr;
extern int _CDECL		optind;
extern int _CDECL		optopt;
extern int _CDECL		optvar;
extern char * _CDECL		optarg;
extern int _CDECL		getopt	(int, char **, char *);
extern int _CDECL		pnmatch	(char *, char *, int);

/* Tests for file types */

#define	S_IFBLK		0060000		/* block special		*/

#define S_ISDIR(m)	((((m) & S_IFMT) == S_IFDIR))
#define S_ISCHR(m)	((((m) & S_IFMT) == S_IFCHR))
#define S_ISREG(m)	((((m) & S_IFMT) == S_IFREG))
#define S_ISBLK(m)	((((m) & S_IFMT) == S_IFBLK))
extern int _CDECL	chmod	(char *, int);
extern int _CDECL	umask	(int);

#ifndef _USHORT_T_DEFINED
typedef unsigned short	ushort;
#define _USHORT_T_DEFINED
#endif

#ifndef _MODE_T_DEFINED
typedef unsigned short	mode_t;
#define _MODE_T_DEFINED
#endif

#ifndef _PID_T_DEFINED
typedef int		pid_t;
#define _PID_T_DEFINED
#endif

#ifndef _GID_T_DEFINED
typedef int		gid_t;
#define _GID_T_DEFINED
#endif

#ifndef _UID_T_DEFINED
typedef int		uid_t;
#define _UID_T_DEFINED
#endif

#ifndef _BOOL_T_DEFINED
typedef char		bool;	/* Boolean: 0 = false, 1 = true		*/
#define FALSE	0		/* Boolean 'false'			*/
#define TRUE	1		/* Boolean 'true'			*/
#define _BOOL_T_DEFINED
#endif
