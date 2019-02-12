/* ISFAT.C
 *
 * Autor:    Kai Uwe Rommel
 * Datum:    Sun 28-Oct-1990
 *
 * Compiler: MS C ab 6.00
 * System:   OS/2 ab 1.2
 */

#define LABEL    "isfat.c"
#define VERSION  "1.0"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
 
#define INCL_NOPM
#include <os2.h>


int IsFileSystemFAT(char *dir)
{
  USHORT nDrive;
  ULONG lMap;
  BYTE bData[64], bName[3];
  USHORT cbData;

  if ( _osmode == DOS_MODE )
    return TRUE;
  else
  {
    /* We separate FAT and HPFS file systems here.
     * Filenames read from a FAT system are converted to lower case
     * while the case of filenames read from a HPFS (and other future
     * file systems, like Unix-compatibles) is preserved.
     */

    if ( isalpha(dir[0]) && (dir[1] == ':') )
      nDrive = toupper(dir[0]) - '@';
    else
      DosQCurDisk(&nDrive, &lMap);

    bName[0] = (char) (nDrive + '@');
    bName[1] = ':';
    bName[2] = 0;

    cbData = sizeof(bData);

    if ( !DosQFSAttach(bName, 0U, 1U, bData, &cbData, 0L) )
      return !strcmp(bData + (*(USHORT *) (bData + 2) + 7), "FAT");
    else
      return FALSE;

    /* End of this ugly code */
  }
}



/* Ende ISFAT.C */
