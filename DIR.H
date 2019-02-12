/*
 * @(#) dir.h 1.4 87/11/06   Public Domain.
 *
 *  A public domain implementation of BSD directory routines for
 *  MS-DOS.  Written by Michael Rendell ({uunet,utai}michael@garfield),
 *  August 1987
 *
 *  Enhanced and ported to OS/2 by Kai Uwe Rommel; added scandir() prototype
 *  December 1989, February 1990
 *  Change of MAXPATHLEN for HPFS, October 1990
 */


#define MAXNAMLEN  256
#define MAXPATHLEN 256

#define A_RONLY    0x01
#define A_HIDDEN   0x02
#define A_SYSTEM   0x04
#define A_LABEL    0x08
#define A_DIR      0x10
#define A_ARCHIVE  0x20


struct direct
{
  ino_t    d_ino;                   /* a bit of a farce */
  int      d_reclen;                /* more farce */
  int      d_namlen;                /* length of d_name */
  char     d_name[MAXNAMLEN + 1];   /* null terminated */
  /* nonstandard fields */
  long     d_size;                  /* size in bytes */
  unsigned d_mode;                  /* DOS or OS/2 file attributes */
  unsigned d_time;
  unsigned d_date;
};

/* The fields d_size and d_mode are extensions by me (Kai Uwe Rommel).
 * The find_first and find_next calls deliver this data without any extra cost.
 * If this data is needed, these fields save a lot of extra calls to stat() 
 * (each stat() again performs a find_first call !).
 */

struct _dircontents
{
  char *_d_entry;
  long _d_size;
  unsigned _d_mode, _d_time, _d_date;
  struct _dircontents *_d_next;
};

typedef struct _dirdesc
{
  int  dd_id;                   /* uniquely identify each open directory */
  long dd_loc;                  /* where we are in directory entry is this */
  struct _dircontents *dd_contents;   /* pointer to contents of dir */
  struct _dircontents *dd_cp;         /* pointer to current position */
}
DIR;


extern int attributes;

extern DIR *opendir(char *);
extern struct direct *readdir(DIR *);
extern void seekdir(DIR *, long);
extern long telldir(DIR *);
extern void closedir(DIR *);
#define rewinddir(dirp) seekdir(dirp, 0L)

extern int scandir(char *, struct direct ***,
                   int (*)(struct direct *),
                   int (*)(struct direct *, struct direct *));

extern int getfmode(char *);
extern int setfmode(char *, unsigned);

/*
NAME
     opendir, readdir, telldir, seekdir, rewinddir, closedir -
     directory operations

SYNTAX
     #include <sys/types.h>
     #include <sys/dir.h>

     DIR *opendir(filename)
     char *filename;

     struct direct *readdir(dirp)
     DIR *dirp;

     long telldir(dirp)
     DIR *dirp;

     seekdir(dirp, loc)
     DIR *dirp;
     long loc;

     rewinddir(dirp)
     DIR *dirp;

     int closedir(dirp)
     DIR *dirp;

DESCRIPTION
     The opendir library routine opens the directory named by
     filename and associates a directory stream with it.  A
     pointer is returned to identify the directory stream in sub-
     sequent operations.  The pointer NULL is returned if the
     specified filename can not be accessed, or if insufficient
     memory is available to open the directory file.

     The readdir routine returns a pointer to the next directory
     entry.  It returns NULL upon reaching the end of the direc-
     tory or on detecting an invalid seekdir operation.  The
     readdir routine uses the getdirentries system call to read
     directories. Since the readdir routine returns NULL upon
     reaching the end of the directory or on detecting an error,
     an application which wishes to detect the difference must
     set errno to 0 prior to calling readdir.

     The telldir routine returns the current location associated
     with the named directory stream. Values returned by telldir
     are good only for the lifetime of the DIR pointer from which
     they are derived.  If the directory is closed and then reo-
     pened, the telldir value may be invalidated due to
     undetected directory compaction.

     The seekdir routine sets the position of the next readdir
     operation on the directory stream. Only values returned by
     telldir should be used with seekdir.

     The rewinddir routine resets the position of the named
     directory stream to the beginning of the directory.

     The closedir routine closes the named directory stream and
     returns a value of 0 if successful. Otherwise, a value of -1
     is returned and errno is set to indicate the error.  All
     resources associated with this directory stream are
     released.

EXAMPLE
     The following sample code searches a directory for the entry
     name.

     len = strlen(name);

     dirp = opendir(".");

     for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp))

     if (dp->d_namlen == len && !strcmp(dp->d_name, name)) {

               closedir(dirp);

               return FOUND;

          }

     closedir(dirp);

     return NOT_FOUND;


SEE ALSO
     close(2), getdirentries(2), lseek(2), open(2), read(2),
     dir(5)

-----------------------

NAME
     scandir - scan a directory

SYNTAX
     #include <sys/types.h>
     #include <sys/dir.h>

     scandir(dirname, namelist, select, compar)
     char *dirname;
     struct direct *(*namelist[]);
     int (*select)();
     int (*compar)();

     alphasort(d1, d2)
     struct direct **d1, **d2;

DESCRIPTION
     The scandir subroutine reads the directory dirname and
     builds an array of pointers to directory entries using mal-
     loc(3).  It returns the number of entries in the array and a
     pointer to the array through namelist.

     The select parameter is a pointer to a user supplied subrou-
     tine which is called by scandir to select which entries are
     to be included in the array.  The select routine is passed a
     pointer to a directory entry and should return a non-zero
     value if the directory entry is to be included in the array.
     If select is null, then all the directory entries will be
     included.

     The compar parameter is a pointer to a user supplied subrou-
     tine which is passed to qsort(3) to sort the completed
     array.  If this pointer is null, the array is not sorted.
     The alphasort is a routine which can be used for the compar
     parameter to sort the array alphabetically.

     The memory allocated for the array can be deallocated with
     free by freeing each pointer in the array and the array
     itself.  For further information, see malloc(3).

DIAGNOSTICS
     Returns -1 if the directory cannot be opened for reading or
     if malloc(3) cannot allocate enough memory to hold all the
     data structures.

SEE ALSO
     directory(3), malloc(3), qsort(3), dir(5)

----------------

NAME
     dir - format of directories

SYNTAX
     #include <sys/types.h>
     #include <sys/dir.h>

DESCRIPTION
     A directory behaves exactly like an ordinary file, except
     that no user may write into a directory.  The fact that a
     file is a directory is indicated by a bit in the flag word
     of its i-node entry.  For further information, see fs(5).
     The structure of a directory entry as given in the include
     file is:

     A directory consists of some number of blocks of DIRBLKSIZ
     bytes, where DIRBLKSIZ is chosen such that it can be
     transferred to disk in a single atomic operation (for exam-
     ple, 512 bytes on most machines).

     Each DIRBLKSIZ byte block contains some number of directory
     entry structures, which are of variable length.  Each direc-
     tory entry has a struct direct at the front of it, contain-
     ing its inode number, the length of the entry, and the
     length of the name contained in the entry.  These are fol-
     lowed by the name padded to a 4 byte boundary with null
     bytes.  All names are guaranteed null terminated.  The max-
     imum length of a name in a directory is MAXNAMLEN.

     The macro DIRSIZ(dp) gives the amount of space required to
     represent a directory entry.  Free space in a directory is
     represented by entries which have dp->d_reclen > DIRSIZ(dp).
     All DIRBLKSIZ bytes in a directory block are claimed by the
     directory entries.  This usually results in the last entry
     in a directory having a large dp->d_reclen.  When entries
     are deleted from a directory, the space is returned to the
     previous entry in the same directory block by increasing its
     dp->d_reclen.  If the first entry of directory block is
     free, then its dp->d_ino is set to 0.  Entries other than
     the first in a directory do not normally have dp->d_ino set
     to 0.

          #ifdef KERNEL
          #define DIRBLKSIZ DEV_BSIZE
          #else
          #define   DIRBLKSIZ 512
          #endif

          #define MAXNAMLEN 255


     The DIRSIZ macro gives the minimum record length which will
     hold the directory entry.  This requires the amount of space
     in struct direct without the d_name field, plus enough space
     for the name with a terminating null byte (dp->d_namlen+1),
     rounded up to a 4 byte boundary.



          #undef DIRSIZ
          #define DIRSIZ(dp) \
              ((sizeof (struct direct) - (MAXNAMLEN+1)) + \
              (((dp)->d_namlen+1 + 3) &~ 3))

          struct    direct {
               u_long    d_ino;
               short     d_reclen;
               short     d_namlen;
               char d_name[MAXNAMLEN + 1];
               * typically shorter *
          };

          struct _dirdesc {
               int  dd_fd;
               long dd_loc;
               long dd_size;
               char dd_buf[DIRBLKSIZ];
          };


     By convention, the first two entries in each directory are
     for `.' and `..'.  The first is an entry for the directory
     itself.  The second is for the parent directory.  The mean-
     ing of `..' is modified for the root directory of the master
     file system ("/"), where `..' has the same meaning as `.'.

SEE ALSO
     fs(5)
*/
