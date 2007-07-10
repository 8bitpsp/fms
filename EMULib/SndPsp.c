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

#include "Sound.h"

int MasterVolume = 200;                  /* These are made public so  */
int MasterSwitch = (1<<SND_CHANNELS)-1;  /* that LibWin.c can access  */
static int SndRate      = 0;
static int NoiseGen;

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

static void PspSetSound(int Channel,int Type);
static void PspDrum(int Type,int Force);
static void PspSetChannels(int Volume,int Switch);
static const signed char *PspGetWave(int Channel);
static void PspSetWave(int Channel,const signed char *Data,int Length,int Rate);
static void PspSound(int Channel,int Freq,int Volume);
void AudioCallback(void* buf, unsigned int *length, void *userdata);

#if defined(FMSX) && defined(ALTSOUND)
#define MSX_CLK 3579545

static OPLL   *opll;
static FM_OPL *fm_opl;
static PSG    *psg;
static SCC    *scc;

const int Use2413 = 0;     /* MSX-MUSIC emulation (1=enable)  */
const int Use8950 = 0;     /* MSX-AUDIO emulation (1=enable)  */

float FactorPSG  = 3.00;  /* Volume percentage of PSG        */
float FactorSCC  = 3.00;  /* Volume percentage of SCC        */
float Factor2413 = 3.00;  /* Volume percentage of MSX-MUSIC  */
float Factor8950 = 2.25;  /* Volume percentage of MSX-AUDIO  */
#else
static int Wave[SND_BUFSIZE];
#endif

/** StopSound() **********************************************/
/** Temporarily suspend sound.                              **/
/*************************************************************/
void StopSound(void) { pspAudioSetChannelCallback(0, 0, 0); }

/** ResumeSound() ********************************************/
/** Resume sound after StopSound().                         **/
/*************************************************************/
void ResumeSound(void) { pspAudioSetChannelCallback(0, AudioCallback, 0); }

/** InitSound() **********************************************/
/** Initialize sound. Returns 0 on failure, effective rate  **/
/** otherwise. Special cases of rate argument: 0 - return 1 **/
/** and be silent                                           **/
/*************************************************************/
unsigned int InitSound(unsigned int Rate,unsigned int Delay)
{
  int J;

  /* Set driver functions */
  SndDriver.SetSound    = PspSetSound;
  SndDriver.Drum        = PspDrum;
  SndDriver.SetChannels = PspSetChannels;
  SndDriver.Sound       = PspSound;
  SndDriver.SetWave     = PspSetWave;
  SndDriver.GetWave     = PspGetWave;

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

  /* Reset channels */
  for(J=0;J<SND_CHANNELS;J++)
  {
    CH[J].Freq   = 0;
    CH[J].Volume = 0;
    CH[J].Count  = 0;
  }

  /* Reset variables */
  NoiseGen = 1;
  SndRate  = 0;

  /* Only 44100 supported */
  if(Rate != 44100) return(1);
  SndRate = Rate;

  /* Set current instrments (if reopening device) */
  for(J=0;J<SND_CHANNELS;J++) PspSetSound(J,CH[J].Type);

  ResumeSound();

  /* Done */
  return(SndRate);
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

/** TrashSound() *********************************************/
/** Close all devices and free all allocated resources.     **/
/*************************************************************/
void TrashSound(void)
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

  if(SndRate)
  {
    /* Shutdown wave audio */
    StopSound();

    /* Done */
    SndRate=0;
  }
}

/** PspSetSound() ********************************************/
/** Set sound type for a given channel.                     **/
/*************************************************************/
void PspSetSound(int Channel,int Type)
{
  if(!SndRate||(Channel<0)||(Channel>=SND_CHANNELS)) return;

  CH[Channel].Type=Type;
}

/** PspDrum() ************************************************/
/** Hit a drum of a given type with given force.            **/
/*************************************************************/
void PspDrum(int Type,int Force)
{
  if(!SndRate) return;
  Force=Force<0? 0:Force>255? 255:Force;
}

/** PspSetChannels() *****************************************/
/** Set overall sound volume and switch channels on/off.    **/
/*************************************************************/
void PspSetChannels(int Volume,int Switch)
{
  if(!SndRate) return;
  Volume=Volume<0? 0:Volume>255? 255:Volume;
  Switch&=(1<<SND_CHANNELS)-1;

  MasterVolume=Volume;
  MasterSwitch=Switch;
}

/** GetWave() ************************************************/
/** Get current read position for the buffer set with the   **/
/** SetWave() call. Returns 0 if no buffer has been set, or **/
/** if there is no playrate set (i.e. wave is instrument).  **/
/*************************************************************/
const signed char *PspGetWave(int Channel)
{
  /* Channel has to be valid */
  if((Channel<0)||(Channel>=SND_CHANNELS)) return(0);
  /* Return current read position */
  return(
    CH[Channel].Rate&&(CH[Channel].Type==SND_WAVE)?
    CH[Channel].Data+CH[Channel].Pos:0
  );
}


/** SetWave() ************************************************/
/** Set waveform for a given channel. The channel will be   **/
/** marked with sound type SND_WAVE. Set Rate=0 if you want **/
/** waveform to be an instrument or set it to the waveform  **/
/** own playback rate.                                      **/
/*************************************************************/
void PspSetWave(int Channel,const signed char *Data,int Length,int Rate)
{
  /* Channel has to be valid */
  if((Channel<0)||(Channel>=SND_CHANNELS)) return;

  /* Set channel parameters */
  CH[Channel].Type   = SND_WAVE;
  CH[Channel].Length = Length;
  CH[Channel].Rate   = Rate;
  CH[Channel].Data   = Data;
  CH[Channel].Pos    = 0;
  CH[Channel].Count  = 0;
}

unsigned int RenderAudio(unsigned int Samples)
{
  /* Stub; audio is rendered via a callback */
  return 0;
}

/** AudioCallback() ******************************************/
/** Called by the system to render sound                    **/
/*************************************************************/
void AudioCallback(void* buf, unsigned int *length, void *userdata)
{
  PspSample *OutBuf = (PspSample*)buf;

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

    OutBuf[J].Left = OutBuf[J].Right = (R>32767)?32767:(R<-32768)?-32768:R;
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
    OutBuf[J].Left = OutBuf[J].Right = I>32767? 32767:I<-32768? -32768:I;
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
