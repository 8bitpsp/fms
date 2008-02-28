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

#if defined(FMSX) && defined(ALTSOUND)
#include "fmopl.h"
#include "emu2149.h"
#include "emu2212.h"
#include "emu2413.h"
#endif

#include "audio.h"
#include "Sound.h"

static struct
{
  int Type;                       /* Channel type (SND_*)             */
  int Freq;                       /* Channel frequency (Hz)           */
  int Volume;                     /* Channel volume (0..255)          */

  const signed char *Data;        /* Wave data (-128..127 each)       */
  int Length;                     /* Wave length in Data              */
  int Rate;                       /* Wave playback rate (or 0Hz)      */
  int Pos;                        /* Wave current position in Data    */

  int Count;                      /* Phase counter                    */
} CH[SND_CHANNELS] =
{
  { SND_MELODIC,0,0,0,0,0,0 },
  { SND_MELODIC,0,0,0,0,0,0 },
  { SND_MELODIC,0,0,0,0,0,0 },
  { SND_MELODIC,0,0,0,0,0,0 },
  { SND_MELODIC,0,0,0,0,0,0 },
  { SND_MELODIC,0,0,0,0,0,0 },
  { SND_MELODIC,0,0,0,0,0,0 },
  { SND_MELODIC,0,0,0,0,0,0 },
  { SND_MELODIC,0,0,0,0,0,0 },
  { SND_MELODIC,0,0,0,0,0,0 },
  { SND_MELODIC,0,0,0,0,0,0 },
  { SND_MELODIC,0,0,0,0,0,0 },
  { SND_MELODIC,0,0,0,0,0,0 },
  { SND_MELODIC,0,0,0,0,0,0 },
  { SND_MELODIC,0,0,0,0,0,0 },
  { SND_MELODIC,0,0,0,0,0,0 }
};

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
static int Wave[SND_BUFSIZE];
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
#endif

  /* Only 44100 supported */
  if(Rate != 44100) return(SndRate = 0);

  /* Done */
  return(SndRate = Rate);
}

/** PspSound() ***********************************************/
/** Set sound volume and frequency for a given channel.     **/
/*************************************************************/
void PspSound(int Channel,int Freq,int Volume)
{
  if(!SndRate||(Channel<0)||(Channel>=SND_CHANNELS)) return;
  Volume=Volume<0? 0:Volume>255? 255:Volume;

  /* Store frequency and volume */
  CH[Channel].Freq   = Freq;
  CH[Channel].Volume = Volume;
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
#endif

  /* Shutdown wave audio */
  StopSound();
  SndRate = 0;
}

/** RenderAudio() ********************************************/
/** Render given number of melodic sound samples into an    **/
/** integer buffer for mixing.                              **/
/*************************************************************/
void RenderAudio(int *Wave,unsigned int Samples)
{ /* Stub; audio is rendered via a callback */ }

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
  unsigned int Samples = *length;
  register int N,J,K,I,L,L1,L2,V,A1,A2;

  /* Reset some variables to keep compiler happy */
  L=N=A2=0;

  /* Truncate to available space */
  J       = *length;
  Samples = Samples>SND_BUFSIZE? SND_BUFSIZE:Samples;
  Samples = Samples>J? J:Samples;
  if(!Samples) return;

  /* Clear mixing buffer */
  for(J=0;J<Samples;J++) Wave[J]=0;

  /* Waveform generator */
  for(J=0;J<SND_CHANNELS;J++)
    if(CH[J].Freq&&(V=CH[J].Volume)&&(MasterSwitch&(1<<J)))
      switch(CH[J].Type)
      {
        case SND_WAVE: /* Custom Waveform */
          /* Waveform data must have correct length! */
          if(CH[J].Length<=0) break;
          /* Start counting */
          K  = CH[J].Rate>0? (SndRate<<15)/CH[J].Freq/CH[J].Rate
                           : (SndRate<<15)/CH[J].Freq/CH[J].Length;
          L1 = CH[J].Pos%CH[J].Length;
          L2 = CH[J].Count;
          A1 = CH[J].Data[L1]*V;
          /* If expecting interpolation... */
          if(L2<K)
          {
            /* Compute interpolation parameters */
            A2 = CH[J].Data[(L1+1)%CH[J].Length]*V;
            L  = (L2>>15)+1;
            N  = ((K-(L2&0x7FFF))>>15)+1;
          }
          /* Add waveform to the buffer */
          for(I=0;I<Samples;I++)
            if(L2<K)
            {
              /* Interpolate linearly */
              Wave[I]+=A1+L*(A2-A1)/N;
              /* Next waveform step */
              L2+=0x8000;
              /* Next interpolation step */
              L++;
            }
            else
            {
              L1 = (L1+L2/K)%CH[J].Length;
              L2 = (L2%K)+0x8000;
              A1 = CH[J].Data[L1]*V;
              Wave[I]+=A1;
              /* If expecting interpolation... */
              if(L2<K)
              {
                /* Compute interpolation parameters */
                A2 = CH[J].Data[(L1+1)%CH[J].Length]*V;
                L  = 1;
                N  = ((K-L2)>>15)+1;
              }
            }
          /* End counting */
          CH[J].Pos   = L1;
          CH[J].Count = L2;
          break;

        case SND_NOISE: /* White Noise */
          /* For high frequencies, recompute volume */
          if(CH[J].Freq<=SndRate) K=0x10000*CH[J].Freq/SndRate;
          else { V=V*SndRate/CH[J].Freq;K=0x10000; }
          L1=CH[J].Count;
          for(I=0;I<Samples;I++)
          {
            L1+=K;
            if(L1&0xFFFF0000)
            {
              if((NoiseGen<<=1)&0x80000000) NoiseGen^=0x08000001;
              L1&=0xFFFF;
            }
            Wave[I]+=(NoiseGen&1? 127:-128)*V;
          }
          CH[J].Count=L1;
          break;

        case SND_MELODIC:  /* Melodic Sound   */
        case SND_TRIANGLE: /* Triangular Wave */
        default:           /* Default Sound   */
          /* Do not allow frequencies that are too high */
          if(CH[J].Freq>=SndRate/2) break;
          K=0x10000*CH[J].Freq/SndRate;
          L1=CH[J].Count;
          for(I=0;I<Samples;I++,L1+=K)
            Wave[I]+=(L1&0x8000? 127:-128)*V;
          CH[J].Count=L1&0xFFFF;
          break;
      }

  /* Mix and convert waveforms */
  for(J=0;J<Samples;J++)
  {
    I=(Wave[J]*MasterVolume)/255;
    (OutBuf++)->Channel = I>32767? 32767:I<-32768? -32768:I;
  }
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
