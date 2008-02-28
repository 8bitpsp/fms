/** EMULib Emulation Library *********************************/
/**                                                         **/
/**                        EMULib.h                         **/
/**                                                         **/
/** This file contains platform-independent definitions and **/
/** declarations for the emulation library.                 **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1996-2008                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#ifndef EMULIB_H
#define EMULIB_H

#ifdef PSP
#include "LibPsp.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** sample ***************************************************/
/** Audio samples may be either 8bit or 16bit.              **/
/*************************************************************/
#ifndef SAMPLE_TYPE_DEFINED
#define SAMPLE_TYPE_DEFINED
#ifdef BPS16
typedef signed short sample;
#else
typedef signed char sample;
#endif
#endif

/** GetFreeAudio() *******************************************/
/** Get the amount of free samples in the audio buffer.     **/
/*************************************************************/
unsigned int GetFreeAudio(void);

/** WriteAudio() *********************************************/
/** Write up to a given number of samples to audio buffer.  **/
/** Returns the number of samples written.                  **/
/*************************************************************/
unsigned int WriteAudio(sample *Data,unsigned int Length);

#ifdef __cplusplus
}
#endif
#endif /* EMULIB_H */
