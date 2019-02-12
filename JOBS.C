/* Display running background processes
 * Kai Uwe Rommel
 * Sat 04-Aug-1990
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <setjmp.h>
#include <unistd.h>
#include <dir.h>
#include "sh.h"

#define INCL_NOPM
#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#include <os2.h>


extern USHORT APIENTRY DosQProcStatus(PVOID pBuf, USHORT cbBuf);


struct process
{
  USHORT pid;
  USHORT ppid;
  USHORT threads;
  USHORT children;
  USHORT modhandle;
  USHORT module;
};


struct module
{
  USHORT modhandle;
  USHORT max_dependents;
  USHORT *dependents;
  UCHAR  *modname;
};


struct process **procs = NULL;
struct module  **mods  = NULL;

USHORT max_procs = 0;
USHORT cur_procs = 0;
USHORT max_mods  = 0;
USHORT cur_mods  = 0;


int parse_processes(UCHAR * bBuf)
{
  USHORT sel, offs;
  USHORT type, tpid;
  USHORT count, kount;
  UCHAR buffer[256];
  UCHAR *cptr, *ptr;

  ptr = bBuf;
  sel = SELECTOROF(ptr);

  while ( (type = *(USHORT *) ptr) != 0xFFFFU )
  {
    ptr += 2;
    offs = *(USHORT *) ptr;
    ptr += 2;

    switch ( type )
    {

    case 0: /* process */

      if ( cur_procs >= max_procs )
      {
        max_procs += 50;

	if ( !(procs = realloc(procs, max_procs * sizeof(struct process *))) )
          return 1;
      }

      if ( !(procs[cur_procs] = calloc(1, sizeof(struct process))) )
        return 1;

      procs[cur_procs] -> pid = *(USHORT *) ptr;
      ptr += 2;
      procs[cur_procs] -> ppid = *(USHORT *) ptr;
      ptr += 2;
      ptr += 2;
      procs[cur_procs] -> modhandle = *(USHORT *) ptr;

      procs[cur_procs] -> threads = 0;
      ++cur_procs;

      break;

    case 1: /* thread */

      ptr += 2;
      tpid = *(USHORT *) ptr;

      for ( count = 0; count < cur_procs; count++ )
	if ( procs[count] -> pid == tpid )
	{
	  ++procs[count] -> threads;
	  break;
	}

      break;

    case 2: /* module */

      if ( cur_mods >= max_mods )
      {
        max_mods += 50;

	if ( !(mods = realloc(mods, max_mods * sizeof(struct module *))) )
          return 1;
      }

      if ( !(mods[cur_mods] = calloc(1, sizeof(struct module))) )
        return 1;

      mods[cur_mods] -> modhandle = *(USHORT *) ptr;
      ptr += 2;
      mods[cur_mods] -> max_dependents = *(USHORT *) ptr;
      ptr += 2;
      ptr += 2;
      ptr += 2;

      if ( mods[cur_mods] -> max_dependents )
	  ptr += (mods[cur_mods] -> max_dependents) * 2;

      for ( cptr = buffer; *cptr++ = *ptr++; );

      if ( !(mods[cur_mods] -> modname = strdup(buffer)) )
        return 1;

      ++cur_mods;

      break;

    case 3: /* system semaphore */
      break;

    case 4: /* shared memory */
      break;

    }

    ptr = MAKEP(sel, offs);
  }

  for ( count = 0; count < cur_procs; count++ )
    for ( kount = 0; kount < cur_mods; kount++ )
      if ( procs[count] -> modhandle == mods[kount] -> modhandle )
      {
        procs[count] -> module = kount;
	break;
      }

  for ( count = 0; count < cur_procs; count++ )
    for ( kount = 0; kount < cur_procs; kount++ )
      if ( procs[count] -> pid == procs[kount] -> ppid )
	(procs[count] -> children)++;

  return 0;
}


void proctree(int pid, int indent)
{
  USHORT count;
  UCHAR *mName, pName[256];

  for (count = 0; count < cur_procs; count++)
    if ( procs[count] -> ppid == pid )
    {
      if ( procs[count] -> module )
      {
        mName = mods[procs[count] -> module] -> modname;
        DosGetModName(procs[count] -> modhandle, sizeof(pName), pName);
      }
      else
      {
        mName = "unknown";  /* Zombie process, i.e. result for DosCwait() */
        pName[0] = 0;
      }

      printf("[%d]\t%2d %-8s %*s%s\n", procs[count] -> pid,
        procs[count] -> threads, mName, indent, "", pName);

      proctree(procs[count] -> pid, indent + 2);
    }
}


int compare(struct process **p1, struct process **p2)
{
  return (*p1) -> pid - (*p2) -> pid;
}


void dojobs(C_Op *t)
{
  UCHAR *pBuf;
  USHORT count;

  pBuf = malloc(0x2000);
  DosQProcStatus(pBuf, 0x2000);

  if ( parse_processes(pBuf) )
  {
    printf("Error: Out of memory 2!\n");
    DosExit(EXIT_PROCESS, 1);
  }

  free(pBuf);

  qsort(procs, cur_procs, sizeof(struct process *), compare);
  proctree(getpid(), 0);

  for (count = 0; count < cur_procs; count++)
    free(procs[count]);

  for (count = 0; count < cur_mods; count++)
  {
    free(mods[count] -> modname);
    free(mods[count]);
  }

  free(procs);
  free(mods);

  max_procs = max_mods = cur_procs = cur_mods = 0;
  procs = NULL;
  mods = NULL;
}


#ifdef TEST
void main(void)
{
  dojobs(NULL);
}
#endif
