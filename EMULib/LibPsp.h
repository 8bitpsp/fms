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

#define SND_BUFSIZE     512    /* Size of a wave buffer      */

/** InitAudio() **********************************************/
/** Initialize sound. Returns rate (Hz) on success, else 0. **/
/** Rate=0 to skip initialization (will be silent).         **/
/*************************************************************/
unsigned int InitAudio(unsigned int Rate,unsigned int Latency);

/** StopSound() **********************************************/
/** Temporarily suspend sound.                              **/
/*************************************************************/
void StopSound(void);

/** StopSound() **********************************************/
/** Reset sound chips.                                      **/
/*************************************************************/
void ResetSound(void);

/** ResumeSound() ********************************************/
/** Resume sound after StopSound().                         **/
/*************************************************************/
void ResumeSound(void);

/** TrashAudio() *********************************************/
/** Free resources allocated by InitAudio().                **/
/*************************************************************/
void TrashAudio(void);

#if defined(FMSX) && defined(ALTSOUND)
void WritePSG(int R,int V);
void WriteSNG(int R,int V);
void Write2212(int R,int V);
void WriteOPLL(int R,int V);
void WriteAUDIO(int R,int V);
int  ReadAUDIO(int R);
int  ReadPSG(int R);
#endif

int FileExistsArchived(const char *path);

#endif /* LIBPSP_H */
