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

#include "adhoc.h"
#include "audio.h"
#include "video.h"
#include "psp.h"
#include "ctrl.h"
#include "file.h"

#include "MSX.h"
#include "MenuPsp.h"
#include "Sound.h"

#if (_PSP_FW_VERSION == 150)
/* Kernel mode */
PSP_MODULE_INFO(PSP_APP_NAME, 0x1000, 1, 1);
PSP_MAIN_THREAD_ATTR(0);
#else
/* User mode */
PSP_MODULE_INFO(PSP_APP_NAME, 0, 1, 1);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
#endif

static char app_path[PSP_FILE_MAX_PATH_LEN];

static void ExitCallback(void* arg)
{
  ExitNow = 1; 
  ExitPSP = 1;
}

int user_main()
{
  /* Initialize PSP */
  pspInit(app_path);
  pspAudioInit(SND_BUFSIZE, 0);
  pspCtrlInit();
  pspVideoInit();

  /* Initialize callbacks */
  pspRegisterCallback(PSP_EXIT_CALLBACK, ExitCallback, NULL);
  pspStartCallbackThread();

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
  pspAudioShutdown();
  pspVideoShutdown();
  pspShutdown();

  return(0);
}

int main(int argc,char *argv[])
{
  int ret_val = 0;
  strcpy(app_path, (char*)argv[0]);

  pspAdhocLoadDrivers();

#if (_PSP_FW_VERSION == 150)
  /* Create user thread */
  SceUID thid = sceKernelCreateThread("User Mode Thread", user_main,
    0x11, // default priority
    256 * 1024, // stack size (256KB is regular default)
    PSP_THREAD_ATTR_USER, NULL);

  /* Start user thread; wait for it to complete */
  sceKernelStartThread(thid, 0, 0);
  sceKernelWaitThreadEnd(thid, NULL);
#else
  ret_val = user_main();
#endif

  return ret_val;
}
