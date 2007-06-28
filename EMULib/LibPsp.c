/** EMULib Emulation Library *********************************/
/**                                                         **/
/**                        LibPsp.c                         **/
/**                                                         **/
/** This file contains PSP-dependent implementation parts   **/
/** parts of the emulation library.                         **/
/**                                                         **/
/** Copyright (C) Akop Karapetyan 2007                      **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

#include <string.h>
#include <malloc.h>
#include <stdio.h>

#include "fileio.h"
#include "image.h"

#include "LibPsp.h"

#ifdef MINIZIP
#include "unzip.h"
#endif

int CaptureVramBuffer(const char *path, const char *filename)
{
  PspImage* vram = pspVideoGetVramBufferCopy();
  if (!vram) return 0;

  int exit_code = SaveScreenshot(path, filename, vram);
  pspImageDestroy(vram);

  return exit_code;
}

int SaveScreenshot(const char *path, const char *filename, const PspImage *image)
{
  char *full_path;

  /* Allocate enough space for the file path */
  if (!(full_path = (char*)malloc(sizeof(char) * (strlen(path) + strlen(filename) + 10))))
    return 0;

  /* Loop until first free screenshot slot is found */
  int i = 0;
  do
  {
    sprintf(full_path, "%s%s-%02i.png", path, filename, i);
  } while (pspFileIoCheckIfExists(full_path) && ++i < 100);

  /* Save the screenshot */
  return pspImageSavePng(full_path, image);
}

int FileExistsArchived(const char *path)
{
  /* Try opening file */
  FILE *file = fopen(path, "r");

#ifdef MINIZIP
  unzFile ZF=NULL;

  if (!file)
  {
    /* Name is not an actual file: maybe it's a file inside a ZIP file? */
    char *ZipName;
    const char *ArchivedName;
    const char *Pos;

    /* Check if Name is in <ZIPFILE>/<ArchiveFile> notation */
    if (!(Pos = strrchr(path, '/')))
      return FILE_NOT_FOUND;

    /* Get the <ZIPFILE> portion */
    ZipName = (char*)malloc(Pos - path + 1);
    strncpy(ZipName, path, Pos - path);
    ZipName[Pos - path] = '\0';

    /* Assume it's a ZIP file and try opening it */
    ZF = unzOpen(ZipName);
    free(ZipName);

    /* Not a ZIP file */
    if (!ZF) 
      return FILE_NOT_FOUND;

    /* Get the <ArchiveFile> portion of Name */
    ArchivedName = Pos + 1;

    /* Locate the requested file in the ZIP archive */
    int err = unzLocateFile(ZF, ArchivedName, 0);

    /* Done */
    unzClose(ZF);
    return (err == UNZ_OK) ? FILE_ZIP : FILE_NOT_FOUND;
  }
#endif
  if (!file) 
    return FILE_NOT_FOUND;

  fclose(file);
  return FILE_PLAIN;
}
