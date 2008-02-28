/** EMULib Emulation Library *********************************/
/**                                                         **/
/**                          SndPsp.c                       **/
/**                                                         **/
/** This file contains standard sound generation routines   **/
/** for Playstation Portable console                        **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1996-2007                 **/
/**               Akop Karapetyan 2007                      **/
/**               Vincent van Dam 2002                      **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

#include <malloc.h>

#if defined(FMSX) && defined(ALTSOUND)
#include "fmopl.h"
#include "emu2149.h"
#include "emu2212.h"
#include "emu2413.h"
#endif

#include "audio.h"
#include "Sound.h"

void AudioCallback(void* buf, unsigned int *length, void *userdata);

#if defined(FMSX) && defined(ALTSOUND)
#define MSX_CLK 3579545

static OPLL   *opll;
static FM_OPL *fm_opl;
static PSG    *psg;
static SCC    *scc;

int Use2413 = 0;     /* MSX-MUSIC emulation (1=enable)  */
int Use8950 = 0;     /* MSX-AUDIO emulation (1=enable)  */

float FactorPSG  = 3.00;  /* Volume percentage of PSG        */
float FactorSCC  = 3.00;  /* Volume percentage of SCC        */
float Factor2413 = 3.00;  /* Volume percentage of MSX-MUSIC  */
float Factor8950 = 2.25;  /* Volume percentage of MSX-AUDIO  */
#else
static int SndSize     = 0;  /* SndData[] size               */
static sample *SndData = 0;  /* Audio buffers                */
static int RPtr        = 0;  /* Read pointer into Bufs       */
static int WPtr        = 0;  /* Write pointer into Bufs      */
#endif

static int SndRate     = 0;  /* Audio sampling rate          */

/** StopSound() **********************************************/
/** Temporarily suspend sound.                              **/
/*************************************************************/
void StopSound(void) { pspAudioSetChannelCallback(0, 0, 0); }

/** ResumeSound() ********************************************/
/** Resume sound after StopSound().                         **/
/*************************************************************/
void ResumeSound(void) { pspAudioSetChannelCallback(0, AudioCallback, 0); }

/** InitAudio() **********************************************/
/** Initialize sound. Returns rate (Hz) on success, else 0. **/
/** Rate=0 to skip initialization (will be silent).         **/
/*************************************************************/
unsigned int InitAudio(unsigned int Rate,unsigned int Latency)
{
  TrashAudio();

#if defined(FMSX) && defined(ALTSOUND)
  /* MSX-MUSIC emulation */
  if (Use2413)
  {
    OPLL_init(MSX_CLK,Rate);
    opll=OPLL_new();
    OPLL_reset(opll);
    OPLL_reset_patch(opll,0);
  }

  /* MSX-AUDIO emulation */
  if (Use8950)
  {
    fm_opl=OPLCreate(OPL_TYPE_Y8950,MSX_CLK,Rate,256);
    OPLResetChip(fm_opl);
  }

  /* PSG/SCC emulation */
  PSG_init(MSX_CLK,Rate);
  psg=PSG_new();
  PSG_reset(psg);
  PSG_setVolumeMode(psg,2);

  SCC_init(MSX_CLK,Rate);
  scc=SCC_new();
  SCC_reset(scc);
#else
  register int J;

  SndSize = 0;
  SndData = 0;
  RPtr    = 0;
  WPtr    = 0;

  /* Compute sound buffer size */
  SndSize=(Rate*Latency/1000+SND_BUFSIZE-1);

  /* Allocate audio buffers */
  SndData=(sample *)malloc(SndSize*sizeof(sample));
  if(!SndData) { TrashSound();return(0); }

  /* Clear audio buffers */
  for(J=0;J<SndSize;++J) SndData[J]=0;
#endif

  /* Only 44100 supported */
  if(Rate != 44100) return(SndRate = 0);

  /* Done */
  return(SndRate = Rate);
}

/** ResetSound() *********************************************/
/** Reset sound chips.                                      **/
/*************************************************************/
void ResetSound()
{
#if defined(FMSX) && defined(ALTSOUND)
  PSG_reset(psg);
  SCC_reset(scc);
  if (Use2413) OPLL_reset(opll);
  if (Use8950) OPLResetChip(fm_opl);
#endif
}

/** TrashAudio() *********************************************/
/** Free resources allocated by InitAudio().                **/
/*************************************************************/
void TrashAudio(void)
{
  if (!SndRate) return;
  SndRate = 0;

#if defined(FMSX) && defined(ALTSOUND)
  /* clean up MSXMUSIC */
  if (Use2413)
  {
    OPLL_close();
    OPLL_delete(opll);
  }

  /* clean up MSXAUDIO */
  if (Use8950) OPLDestroy(fm_opl);

  /* clean up PSG/SCC */
  PSG_delete(psg);
  SCC_delete(scc);
#else
  /* If buffers were allocated... */
  if(SndData) free(SndData);

  /* Sound trashed */
  SndData = 0;
  SndSize = 0;
  RPtr    = 0;
  WPtr    = 0;
#endif

  /* Shutdown wave audio */
  StopSound();
}

/** AudioCallback() ******************************************/
/** Called by the system to render sound                    **/
/*************************************************************/
void AudioCallback(void* buf, unsigned int *length, void *userdata)
{
  PspMonoSample *OutBuf = (PspMonoSample*)buf;

#if defined(FMSX) && defined(ALTSOUND)
  register int   J,R;
  register INT16 P,O,A,S;

  for(J=0;J<*length;J++)
  {
    P=PSG_calc(psg);
    S=SCC_calc(scc);
    O=Use2413? OPLL_calc(opll): 0;
    A=Use8950? Y8950UpdateOne(fm_opl): 0;
    R=P*FactorPSG+O*Factor2413+A*Factor8950+S*FactorSCC;

    (OutBuf++)->Channel = (R>32767)?32767:(R<-32768)?-32768:R;
  }
#else
  register int J;

  /* Mix and convert waveforms */
  for(J=0;J<*length;J++)
    (OutBuf++)->Channel = SndData[J];

  /* Advance buffer pointer, clearing the buffer */
  for(J=0;J<SND_BUFSIZE;++J) SndData[RPtr++]=0;
  if(RPtr>=SndSize) RPtr=0;
#endif
}

/** GetFreeAudio() *******************************************/
/** Get the amount of free samples in the audio buffer.     **/
/*************************************************************/
unsigned int GetFreeAudio(void)
{
#if defined(FMSX) && defined(ALTSOUND)
  return(0);
#else
  return(!SndRate? 0:RPtr>=WPtr? RPtr-WPtr:RPtr-WPtr+SndSize);
#endif
}

/** WriteAudio() *********************************************/
/** Write up to a given number of samples to audio buffer.  **/
/** Returns the number of samples written.                  **/
/*************************************************************/
unsigned int WriteAudio(sample *Data,unsigned int Length)
{
#if defined(FMSX) && defined(ALTSOUND)
  return(0);
#else
  unsigned int J;

  /* Require audio to be initialized */
  if(!SndRate) return(0);

  /* Copy audio samples */
  for(J=0;(J<Length)&&(RPtr!=WPtr);++J)
  {
    SndData[WPtr++]=Data[J];
    if(WPtr>=SndSize) WPtr=0;
  }

  /* Return number of samples copied */
  return(J);
#endif
}

#if defined(FMSX) && defined(ALTSOUND)
/* wrapper functions to actual sound emulation */
void WriteOPLL (int R,int V) { OPLL_writeReg(opll,R,V); }
void WriteAUDIO(int R,int V) { OPLWrite(fm_opl,R,V); }
void Write2212 (int R,int V) { SCC_write(scc,R,V); }
void WritePSG  (int R,int V) { PSG_writeReg(psg,R,V); }
int  ReadAUDIO (int R)       { return OPLRead(fm_opl,R); }
int  ReadPSG   (int R)       { return PSG_readReg(psg,R); }
#endif
