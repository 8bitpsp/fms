/** ColEm: portable Coleco emulator **************************/
/**                                                         **/
/**                          ColEm.c                        **/
/**                                                         **/
/** This file contains generic main() procedure statrting   **/
/** the emulation.                                          **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2007                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pspkernel.h>

#include "audio.h"
#include "video.h"
#include "psp.h"
#include "ctrl.h"

#include "Coleco.h"
#include "Sound.h"

PSP_MODULE_INFO(PSP_APP_NAME, 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

static void ExitCallback(void* arg)
{
  ExitNow = 1; 
  ExitPSP = 1;
}

/** main() ***************************************************/
/** This is a main() function used in Unix and MSDOS ports. **/
/** It parses command line arguments, sets emulation        **/
/** parameters, and passes control to the emulation itself. **/
/*************************************************************/
int main(int argc,char *argv[])
{
  char *P;

#ifdef DEBUG
  CPU.Trap=0xFFFF;
  CPU.Trace=0;
#endif

  /* Initialize PSP */
  pspInit(argv[0]);
  pspAudioInit(SND_BUFSIZE);
  pspCtrlInit();
  pspVideoInit();

  /* Set home directory to where executable is */
  P=strrchr(argv[0],'/');
  if(P) { *P='\0';HomeDir=argv[0]; }

  /* Initialize callbacks */
  pspRegisterCallback(PSP_EXIT_CALLBACK, ExitCallback, NULL);
  pspStartCallbackThread();

  /* ColEm settings */
  Verbose=0;
  UPeriod=100;

  if(InitMachine())
  {
    StartColeco(NULL);
    TrashColeco();
    TrashMachine();
  }

  /* Release PSP resources */
  pspAudioShutdown();
  pspVideoShutdown();
  pspShutdown();

  return(0);
}
