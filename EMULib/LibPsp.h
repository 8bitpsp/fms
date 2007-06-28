/** EMULib Emulation Library *********************************/
/**                                                         **/
/**                        LibPsp.c                         **/
/**                                                         **/
/** This file contains PSP-dependent definitions and        **/
/** declarations for the emulation library.                 **/
/**                                                         **/
/** Copyright (C) Akop Karapetyan 2007                      **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#ifndef LIBPSP_H
#define LIBPSP_H

#include "video.h"

#define FILE_NOT_FOUND 0
#define FILE_PLAIN     1
#define FILE_ZIP       2

int CaptureVramBuffer(const char *path, const char *filename);
int SaveScreenshot(const char *path, const char *filename, const PspImage *image);
int FileExistsArchived(const char *path);

#endif /* LIBPSP_H */
