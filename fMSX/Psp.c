/** fMSX: portable MSX emulator ******************************/
/**                                                         **/
/**                           Psp.c                         **/
/**                                                         **/
/** This file contains PSP-dependent subroutines and        **/
/** drivers. It includes common drivers from Common.h.      **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2005                 **/
/**               Elan Feingold   1995                      **/
/**               Ville Hallik    1996                      **/
/**               Akop Karapetyan 2007                      **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

/** Private #includes ****************************************/
#include "MSX.h"
#include "Sound.h"

#include "video.h"
#include "ctrl.h"
#include "kybd.h"
#include "perf.h"
#include "util.h"
#include "fileio.h"
#include "MenuPsp.h"
#include "LibPsp.h"

/** PSP SDK includes *****************************************/
#include <psprtc.h>

/** Standard Unix/X #includes ********************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/** Public parameters ****************************************/
int UseSound  = 44100;          /* Sound driver frequency    */
extern int FrameLimiter;
extern int VSync;
extern int ShowFps;
extern int HiresEnabled;
extern int ShowStatus;
extern char *ScreenshotPath;

/** Various variables ****************************************/
typedef unsigned short pixel;

static unsigned int BPal[256],XPal[80],XPal0; 
static byte JoyState;
static int MouseState;

/** Sound-related definitions ********************************/
#ifdef SOUND
static int SndSwitch = (1<<MAXCHANNELS)-1;
static int SndVolume = 255/MAXCHANNELS;
#endif

extern WD1793 FDC;
extern char *ROM[MAXCARTS];
extern char *Drive[MAXDRIVES];
extern const u64 ButtonMask[];
extern const int ButtonMapId[];
extern struct GameConfig GameConfig;

extern int DisplayMode;
extern int Frameskip;

PspImage *Screen;
static int ScreenX;
static int ScreenY;
static int ScreenW;
static int ScreenH;

static int FlashMenu;
static int TicksPerUpdate;
static u32 TicksPerSecond;
static u64 LastTick;
static u64 CurrentTick;
static int Frame;
static int ClearScreen;
static int LastScrMode=-1;
static int IconFlashed;

static int PrevDiskHeld;
static int NextDiskHeld;
static int ShowKybdHeld;

static int MessageTimer;
static char Message[250];

static PspKeyboardLayout *KeyLayout;
static PspFpsCounter FpsCounter;

static void OpenMenu();
static void ResetInput();
static void HandleSpecialInput(int code, int on);
static void ResetView();

static int GetKeyStatus(unsigned int code);
static inline void HandleKeyboardInput(unsigned int code, int on);

/** InitMachine() ********************************************/
/** Allocate resources needed by Unix/X-dependent code.     **/
/*************************************************************/
int InitMachine(void)
{
  int J,I;

  /* Initialize screen buffer */
  if (!(Screen = pspImageCreateVram(512, HEIGHT, PSP_IMAGE_16BPP)))
    return(0);
  Screen->Viewport.Width = WIDTH;

  pspImageClear(Screen, 0x8000);

  /* Initialize keyboard */
  if (!(KeyLayout = pspKybdLoadLayout("msx.lyt", 
    GetKeyStatus, HandleKeyboardInput)))
  {
    pspImageDestroy(Screen);
    return(0);
  }

  /* Initialize menu */
  InitMenu();
  FlashMenu=1;

  /* Reset all variables */
  JoyState=0;
  MouseState=0;
  ShowKybdHeld=0;
  Frame=0;
  NextDiskHeld=0;
  PrevDiskHeld=0;
  MessageTimer=0;
  pspPerfInitFps(&FpsCounter);
  ClearScreen=0;

  /* Reset the palette */
  for(J=0;J<16;J++) XPal[J]=0;
  XPal0=0;

  /* Set SCREEN8 colors */
  for(J=0;J<64;J++)
  {
    I=(J&0x03)+(J&0x0C)*16+(J&0x30)/2;
    XPal[J+16]=RGB((J&0x30)*255/48,(J&0x0C)*255/12,(J&0x03)*255/3);
    BPal[I]=BPal[I|0x04]=BPal[I|0x20]=BPal[I|0x24]=XPal[J+16];
  }

#ifdef SOUND
  /* Initialize sound */
  if(InitSound(UseSound, 0))
    SetChannels(SndVolume,SndSwitch);
#endif

  /* Init screen position */
  ScreenW = Screen->Viewport.Width;
  ScreenH = Screen->Viewport.Height;
  ScreenX = ScreenY = 0;

  return(1);
}

/** TrashMachine() *******************************************/
/** Deallocate all resources taken by InitMachine().        **/
/*************************************************************/
void TrashMachine(void)
{
#ifdef SOUND
  StopSound();
  TrashSound();
#endif

  /* Destroy screen buffer */
  if (Screen) pspImageDestroy(Screen);

  /* Destroy keyboard */
  pspKybdDestroyLayout(KeyLayout);

  /* Destroy menu */
  TrashMenu();
}

/** PutImage() ***********************************************/
/** Put an image on the screen.                             **/
/*************************************************************/
void PutImage(void)
{
  /* Skip frames, if necessary */
  if (++Frame <= Frameskip) return;
  Frame = 0;

  pspVideoBegin();

  /* Clear the buffer first, if necessary */
  if (ClearScreen >= 0)
  {
    ClearScreen--;
    pspVideoClearScreen();
  }

  /* If screen mode changes readjust viewport */
  if (LastScrMode!=ScrMode) ResetView();

  /* Draw the screen */
  pspVideoPutImage(Screen, ScreenX, ScreenY, ScreenW, ScreenH);

  /* Draw keyboard */
  if (ShowKybdHeld)
    pspKybdRender(KeyLayout);

  /* Draw message (if any) */
  if (MessageTimer)
  {
    MessageTimer--;
    int width = pspFontGetTextWidth(&PspStockFont, Message);
    int height = pspFontGetLineHeight(&PspStockFont);

    pspVideoFillRect(0, 0, width, height, PSP_COLOR_BLACK);
    pspVideoPrint(&PspStockFont, 0, 0, Message, PSP_COLOR_WHITE);

    if (MessageTimer < 1) 
      ClearScreen = 1;
  }

  /* Display FPS */
  if (ShowFps)
  {
    float fps = pspPerfGetFps(&FpsCounter);

    static char fps_display[16];
    sprintf(fps_display, " %3.02f ", fps);

    int width = pspFontGetTextWidth(&PspStockFont, fps_display);
    int height = pspFontGetLineHeight(&PspStockFont);

    pspVideoFillRect(SCR_WIDTH - width, 0, SCR_WIDTH, height, PSP_COLOR_BLACK);
    pspVideoPrint(&PspStockFont, SCR_WIDTH - width, 0, fps_display, PSP_COLOR_WHITE);
  }

  /* Status indicators */
  if (ShowStatus)
  {
    /* "Disk busy" indicator */
    if (FDC.R[0]&F_BUSY)
    {
      IconFlashed = 1;
      pspVideoPrint(&PspStockFont, 0, 0, PSP_CHAR_FLOPPY, PSP_COLOR_RED);
    }
    else if (IconFlashed) ClearScreen = 1;
  }

  pspVideoEnd();

  /* Wait if needed */
  if (FrameLimiter)
  {
    do { sceRtcGetCurrentTick(&CurrentTick); }
    while (CurrentTick - LastTick < TicksPerUpdate);
    LastTick = CurrentTick;
  }

  /* Wait for VSync signal */
  if (VSync) pspVideoWaitVSync();

  /* Swap buffers */
  pspVideoSwapBuffers();
}

/** GetKeyboardStatus() **************************************/
/** Gets status of specific MSX key (set/unset).            **/
/*************************************************************/
static int GetKeyStatus(unsigned int code)
{
  return !(KeyState[Keys[code][0]] & Keys[code][1]);
}

/** HandleKeyboardInput() ************************************/
/** Modify MSX keyboard matrix.                             **/
/*************************************************************/
static inline void HandleKeyboardInput(unsigned int code, int on)
{
  if (on) KBD_SET(code);
  else KBD_RES(code);
}

/** Keyboard() ***********************************************/
/** Check for keyboard events, parse them, and modify MSX   **/
/** keyboard/joystick, or handle special keys               **/
/*************************************************************/
void Keyboard(void)
{
  int i;

  /* If starting up, flash the menu */
  if (FlashMenu)
  {
    FlashMenu=0;

    /* Unload all carts (fMSX bug?) */
    for (i = 0; i < MAXCARTS; i++) LoadCart(NULL, i, 0);

    /* Display the menu */
    OpenMenu();

    /* Reset the system */
    ResetMSX(Mode,RAMPages,VRAMPages);
  }

  SceCtrlData pad;

  /* Check the input */
  if (pspCtrlPollControls(&pad))
  {
    /* Clear joystick state */
    JoyState=0x00;

    if (ShowKybdHeld)
      pspKybdNavigate(KeyLayout, &pad);

    MouseState=pad.Lx|(pad.Ly<<8);
#ifdef PSP_DEBUG
    if ((pad.Buttons & (PSP_CTRL_SELECT | PSP_CTRL_START))
      == (PSP_CTRL_SELECT | PSP_CTRL_START))
        pspUtilSaveVramSeq(ScreenshotPath, "game");
#endif

    /* Parse input */
    int on, code;
    for (i = 0; ButtonMapId[i] >= 0; i++)
    {
      code = GameConfig.ButtonMap[ButtonMapId[i]];
      on = (pad.Buttons & ButtonMask[i]) == ButtonMask[i];

      /* Check to see if a button set is pressed. If so, unset it, so it */
      /* doesn't trigger any other combination presses. */
      if (on) pad.Buttons &= ~ButtonMask[i];

      if (code & KBD && !ShowKybdHeld)
      {
        /* Keybord state change */
        HandleKeyboardInput(CODE_MASK(code), on);
      }
      else if (code & JST && !ShowKybdHeld)
      {
        /* Joystick state change */
        if (on) JoyState|=CODE_MASK(code);
      }
      else if (code & SPC)
      {
        /* Emulator-specific input */
        HandleSpecialInput(CODE_MASK(code), on);
      }
    }
  }
}

/** Joystick() ***********************************************/
/** Query position of a joystick connected to port N.       **/
/** Returns 0.0.F2.F1.R.L.D.U.                              **/
/*************************************************************/
unsigned int Joystick(void) { return(JoyState); }

/** Mouse() **************************************************/
/** Query coordinates of a mouse connected to port N.       **/
/** Returns F2.F1.Y.Y.Y.Y.Y.Y.Y.Y.X.X.X.X.X.X.X.X.          **/
/*************************************************************/
unsigned int Mouse(byte N) { return(MouseState); }

/** SetColor() ***********************************************/
/** Set color N (0..15) to R,G,B.                           **/
/*************************************************************/
void SetColor(register byte N,register byte R,register byte G,register byte B)
{
  if (N) XPal[N]=RGB(R, G, B);
  else XPal0=RGB(R, G, B);
}

/** ResetInput() *********************************************/
/** Clear joystick/keyboard matrix                          **/
/*************************************************************/
static void ResetInput()
{
  JoyState=0x00;
  memset((void*)KeyState, 0xFF, sizeof(KeyState));
}

/** OpenMenu() ***********************************************/
/** Displays fMSX-PSP menu                                  **/
/*************************************************************/
static void OpenMenu()
{
  /* Stop sound & display menu */
  StopSound();
  DisplayMenu();

  /* Reset view */
  ResetView();

  /* Reset FPS counter */
  pspPerfInitFps(&FpsCounter);

  /* Recompute update frequency */
  TicksPerSecond = sceRtcGetTickResolution();
  if (FrameLimiter)
  {
    int UpdateFreq = (Mode & MSX_VIDEO) == MSX_NTSC ? 60 : 50;
    TicksPerUpdate = TicksPerSecond / (UpdateFreq / (Frameskip + 1));
    sceRtcGetCurrentTick(&LastTick);
  }

  Frame = 0;
  ShowKybdHeld = 0;
  ClearScreen = 1;
  PrevDiskHeld = NextDiskHeld = 0;
  IconFlashed = 0;

  /* Reinitialize input matrix & resume sound */
  ResetInput();
  ResumeSound();

  pspVideoWaitVSync();
}

/** SwitchVolume() *******************************************/
/** Switches from current disk image to the next image in   **/
/** sequence                                                **/
/*************************************************************/
static void SwitchVolume(char *drive, int vol_iter)
{
  if (drive && pspFileIoEndsWith(drive, "DSK"))
  {
    int pos = strlen(drive) - 5;
    char digit = drive[pos];

    if (digit >= '0' && digit <= '9')
    {
      int curr_dsk = (digit == '0') ? 10 : ((int)digit - '0');
      int next_dsk = curr_dsk + vol_iter;

      if (next_dsk >= 1 && next_dsk <= 10)
      {
        char *disk = strdup(drive);
        disk[pos] = '0' + ((next_dsk < 10) ? next_dsk : 0);

        if (FileExistsArchived(disk))
        {
          if (ChangeDisk(0, disk))
          {
            drive[pos] = disk[pos];
            sprintf(Message, "\026"PSP_CHAR_FLOPPY" %i > %i", 
              curr_dsk, next_dsk);
          }
          else
          {
            sprintf(Message, "\022Error loading \274 %i", next_dsk);
          }

          MessageTimer = 60 * 6;
        }

        free(disk);
      }
    }
  }
}

/** HandleSpecialInput() *************************************/
/** Handles emulator-specific buttons                       **/
/*************************************************************/
static void HandleSpecialInput(int code, int on)
{
  switch (code)
  {
  case SPC_MENU:

    if (on) OpenMenu();
    break;

  case SPC_KYBD:

    if (ShowKybdHeld != on)
    {
      if (on) pspKybdReinit(KeyLayout);
      else
      {
        ClearScreen = 1;
        pspKybdReleaseAll(KeyLayout);
      }
    }

    ShowKybdHeld = on;
    break;

  case SPC_NDISK:

    if (NextDiskHeld != on && on)
      SwitchVolume(Drive[0], 1);

    NextDiskHeld = on;
    break;

  case SPC_PDISK:

    if (PrevDiskHeld != on && on)
      SwitchVolume(Drive[0], -1);

    PrevDiskHeld = on;
    break;
  }
}

void ResetView()
{
  float ratio;
  Screen->Viewport.Width = WIDTH;

  /* Recompute screen size/position */
  switch (DisplayMode)
  {
  default:
  case DISPLAY_MODE_UNSCALED:
    Screen->Viewport.Width = (IS_WIDESCREEN && HiresEnabled) 
      ? SCR_WIDTH : WIDTH;
    ScreenW = Screen->Viewport.Width;
    ScreenH = Screen->Viewport.Height;
    break;
  case DISPLAY_MODE_FIT_HEIGHT:
    Screen->Viewport.Width = (IS_WIDESCREEN && HiresEnabled) 
      ? HIRES_WIDTH : WIDTH;
    ratio = (float)SCR_HEIGHT / (float)Screen->Viewport.Height;
//    ScreenW = (float)((Screen->Viewport.Width==512)?256:WIDTH) * ratio - 1;
    ScreenW = (float)WIDTH * ratio - 1;
    ScreenH = SCR_HEIGHT;
    break;
  case DISPLAY_MODE_FILL_SCREEN:
    Screen->Viewport.Width = (IS_WIDESCREEN && HiresEnabled) 
      ? HIRES_WIDTH : WIDTH;
    ScreenW = SCR_WIDTH;
    ScreenH = SCR_HEIGHT;
    break;
  }

  ScreenX=(SCR_WIDTH / 2)-(ScreenW / 2);
  ScreenY=(SCR_HEIGHT / 2)-(ScreenH / 2);

  LastScrMode=ScrMode;
  ClearScreen=1;
}

#include "Common.h"
