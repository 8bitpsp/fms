/** fMSX: portable MSX emulator ******************************/
/**                                                         **/
/**                          fMSX.c                         **/
/**                                                         **/
/** This is the startup module that initializes PSP         **/
/** libraries and starts MSX emulation                      **/
/**                                                         **/
/** Copyright (C) Akop Karapetyan 2007                      **/
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

#include "MSX.h"
#include "MenuPsp.h"

PSP_MODULE_INFO("fMSX PSP", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

static void ExitCallback(void* arg)
{
  ExitNow = 1; 
  ExitPSP = 1;
}

/** main() ***************************************************/
/** This is a main() function used in PSP port.             **/
/*************************************************************/
int main(int argc,char *argv[])
{
#ifdef DEBUG
  CPU.Trap  = 0xFFFF;
  CPU.Trace = 0;
#endif

  /* Initialize PSP */
  pspInit(argv[0]);
  pspAudioInit();
  pspCtrlInit();
  pspVideoInit();

  /* Initialize callbacks */
  pspRegisterCallback(PSP_EXIT_CALLBACK, ExitCallback, NULL);
  pspStartCallbackThread();

  /* Redirect stdout/stderr */
  //sceKernelStdoutReopen("ms0:/stdout.txt", PSP_O_WRONLY, 0777);
  //sceKernelStderrReopen("ms0:/stderr.txt", PSP_O_WRONLY, 0777);

  int i;

  /* MSX settings */
  Verbose=0;
  UPeriod=100;

  /* Init joystick */
  SETJOYTYPE(0,1&0x03);

  /* Init cartridges */
  for (i=0; i<MAXCARTS; i++)
    ROMName[i]=NULL;

  /* Initialize disk images */
  for (i=0; i<MAXDRIVES; i++)
    DSKName[i]=NULL;

  /* Start fMSX! */
  if (InitMachine())
  {
    StartMSX(Mode,RAMPages,VRAMPages);
    TrashMSX();
    TrashMachine();
  }

  /* Release PSP resources */
  pspAudioEnd();
  pspVideoShutdown();
  pspShutdown();

  return(0);
}
