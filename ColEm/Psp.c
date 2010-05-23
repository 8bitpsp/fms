/** ColEm: Portable Coleco Emulator **************************/
/**                                                         **/
/**                         Psp.c                           **/
/**                                                         **/
/** This file contains Psp-dependent subroutines and        **/
/** drivers. It includes common drivers from Common.h.      **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2007                 **/
/**               Akop Karapetyan 2007                      **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#include "Coleco.h"
#include "Sound.h"

#include "video.h"
#include "ctrl.h"
#include "pl_vk.h"
#include "pl_gfx.h"
#include "pl_perf.h"
#include "pl_rewind.h"

#include "MenuPsp.h"

#include <psprtc.h>

#include <string.h>

#define WIDTH       272                   /* Buffer width    */
#define HEIGHT      200                   /* Buffer height   */

int UseSound    = 44100;   /* Audio sampling frequency (Hz)  */
int SndSwitch;             /* Mask of enabled sound channels */
int SndVolume;             /* Master volume for audio        */

PspImage *Screen;                        /* Screen canvas */
pl_rewind Rewinder;
static pixel ColecoScreen[WIDTH*HEIGHT]; /* Actual display buffer  */

extern int RewindSaveRate;
extern int FrameLimiter;
extern int SoundSuspended;
extern int DisplayMode;
extern int Frameskip;
extern int VSync;
extern int ShowFps;
extern const u64 ButtonMask[];
extern const int ButtonMapId[];

extern struct GameConfig GameConfig;
extern char *ScreenshotPath;

static pl_vk_layout KeyLayout;
static pl_perf_counter FpsCounter;

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
static int ClearBufferCount;
static int ShowKybdHeld;
static int Keypad;
static int FramesUntilSave;

static int Rewinding;
static u8 RewindEnabled;

static void OpenMenu();
static void ResetInput();
static void HandleSpecialInput(int code, int on);
static inline void HandlePadInput(unsigned int code, int on);
static inline void CopyScreen();

/** InitMachine() ********************************************/
/** Allocate resources needed by machine-dependent code.    **/
/*************************************************************/
int InitMachine(void)
{
  /* Initialize screen canvas */
  if (!(Screen = pspImageCreateVram(512, HEIGHT, PSP_IMAGE_16BPP)))
    return(0);
  Screen->Viewport.Width = WIDTH;
  pspImageClear(Screen, 0x8000);

  /* Init local vars */
  Frame=0;
  ClearBufferCount=0;
  ShowKybdHeld=0;

  /* Initialize keypad */
  pl_vk_load(&KeyLayout, "system/coleco.l2", 
                         "system/coleco.png", NULL, HandlePadInput);
  ResetInput();

  pl_perf_init_counter(&FpsCounter);

  /* Initialize menu */
  InitMenu();
  FlashMenu=1;

  /* Initialize video */
  ScrWidth  = WIDTH;
  ScrHeight = HEIGHT;
  ScrBuffer = ColecoScreen;
  ScreenW = Screen->Viewport.Width;
  ScreenH = Screen->Viewport.Height;
  ScreenX = ScreenY = 0;

  /* Initialize audio */
  if (InitSound(UseSound,150))
  {
    SndSwitch=(1<<SN76489_CHANNELS)-1;
    SndVolume=255/SN76489_CHANNELS;
    SetChannels(SndVolume,SndSwitch);
  }

  /* Initialize rewinder */
  pl_rewind_init(&Rewinder,
    SaveSTAToBuffer,
    LoadSTAFromBuffer,
    GetSTABufferSize);

  return(1);
}

/** HandlePadInput() *****************************************/
/** Modify gamepad matrix.                                  **/
/*************************************************************/
static inline void HandlePadInput(unsigned int code, int on)
{
  if(on) Keypad|=code;
  else Keypad&=~code;
}

/** OpenMenu() ***********************************************/
/** Displays emulator menu                                  **/
/*************************************************************/
static void OpenMenu()
{
  /* Stop sound & display menu */
  StopSound();
  DisplayMenu();

  float ratio;

  /* Recompute screen size/position */
  switch (DisplayMode)
  {
  default:
  case DISPLAY_MODE_UNSCALED:
    ScreenW = Screen->Viewport.Width;
    ScreenH = Screen->Viewport.Height;
    break;
  case DISPLAY_MODE_FIT_HEIGHT:
    ratio = (float)SCR_HEIGHT / (float)Screen->Viewport.Height;
    ScreenW = (float)Screen->Viewport.Width * ratio;
    ScreenH = SCR_HEIGHT;
    break;
  case DISPLAY_MODE_FILL_SCREEN:
    ScreenW = SCR_WIDTH;
    ScreenH = SCR_HEIGHT;
    break;
  }

  ScreenX = (SCR_WIDTH / 2) - (ScreenW / 2);
  ScreenY = (SCR_HEIGHT / 2) - (ScreenH / 2);

  /* Reset FPS counter */
  pl_perf_init_counter(&FpsCounter);

  /* Determine if at least 1 button is mapped to 'rewind' */
  Rewinding = 0;
  RewindEnabled = 0;

  int i;
  for (i = 0; ButtonMapId[i] >= 0; i++)
  {
    int code = GameConfig.ButtonMap[ButtonMapId[i]];
    if ((code & SPC) && (CODE_MASK(code) == SPC_REWIND))
    {
      RewindEnabled = 1; /* Rewind button is mapped */
      break;
    }
  }

  FramesUntilSave = 0;

  /* Recompute update frequency */
  TicksPerSecond = sceRtcGetTickResolution();
  if (FrameLimiter)
  {
    int UpdateFreq = (Mode & CV_PAL) == CV_NTSC ? 60 : 50;
    TicksPerUpdate = TicksPerSecond / (UpdateFreq / (Frameskip + 1));
    sceRtcGetCurrentTick(&LastTick);
  }

  ShowKybdHeld=0;
  Frame=0;
  ClearBufferCount=2;

  /* Reinitialize input matrix & resume sound */
  ResetInput();
  ResumeSound();

  pspVideoWaitVSync();
}

/** ResetInput() *********************************************/
/** Clear joystick/keyboard matrix                          **/
/*************************************************************/
static void ResetInput()
{
  Keypad=0;
}

/** TrashMachine() *******************************************/
/** Deallocate all resources taken by InitMachine().        **/
/*************************************************************/
void TrashMachine(void)
{
  /* Trash images */
  if (Screen) pspImageDestroy(Screen);

  TrashSound();
 
  /* Destroy keyboard */
  pl_vk_destroy(&KeyLayout);

  pl_rewind_destroy(&Rewinder);

  /* Destroy menu */
  TrashMenu();
}

/** CopyScreen() *********************************************/
/** Copy contents of VDP to the Screen bitmap               **/
/*************************************************************/
static inline void CopyScreen()
{
  int i,
    canvasPitch = Screen->Width * sizeof(pixel),
    vdpPitch = WIDTH * sizeof(pixel);

  for (i = 0; i < HEIGHT; i++)
    memcpy(Screen->Pixels + canvasPitch * i,
      ColecoScreen + WIDTH * i, vdpPitch);
}

/** RefreshScreen() ******************************************/
/** Refresh screen. This function is called in the end of   **/
/** refresh cycle to show the entire screen.                **/
/*************************************************************/
void RefreshScreen(void *Buffer,int Width,int Height)
{
  /* Skip frames, if necessary */
  if (++Frame <= Frameskip) return;
  Frame = 0;

  pspVideoBegin();

  /* Clear the buffer first, if necessary */
  if (ClearBufferCount > 0)
  {
    ClearBufferCount--;
    pspVideoClearScreen();
  }

  /* Draw the screen */
  CopyScreen();
  pl_gfx_put_image(Screen, ScreenX, ScreenY, ScreenW, ScreenH);

  /* Draw keyboard */
  if (ShowKybdHeld)
    pl_vk_render(&KeyLayout);

  /* Display FPS */
  if (ShowFps)
  {
    float fps = pl_perf_update_counter(&FpsCounter);

    static char fps_display[16];
    sprintf(fps_display, " %3.02f ", fps);

    int width = pspFontGetTextWidth(&PspStockFont, fps_display);
    int height = pspFontGetLineHeight(&PspStockFont);

    pspVideoFillRect(SCR_WIDTH - width, 0, SCR_WIDTH, height, PSP_COLOR_BLACK);
    pspVideoPrint(&PspStockFont, SCR_WIDTH - width, 0, fps_display, PSP_COLOR_WHITE);
  }

  /* Wait if needed */
  if (FrameLimiter)
  {
    do { sceRtcGetCurrentTick(&CurrentTick); }
    while (CurrentTick - LastTick < TicksPerUpdate);
    LastTick = CurrentTick;
  }

  /* Wait for VSync signal */
  if (VSync) pspVideoWaitVSync();

  pspVideoEnd();

  /* Swap buffers */
  pspVideoSwapBuffers();
}

/** Mouse() **************************************************/
/** This function is called to poll mouse. It should return **/
/** the X coordinate (-512..+512) in the lower 16 bits, the **/
/** Y coordinate (-512..+512) in the next 14 bits, and the  **/
/** mouse buttons in the upper 2 bits. All values should be **/
/** counted from the screen center!                         **/
/*************************************************************/
unsigned int Mouse(void)
{
  unsigned int X,Y;

  X = 0;//GetMouse();
  Y = ((((X>>16)&0x0FFF)<<10)/200-512)&0x3FFF;
  Y = (Y<<16)|((((X&0x0FFF)<<10)/320-512)&0xFFFF);
  return(Y|(X&0xC0000000));
}

/** Joystick() ***********************************************/
/** This function is called to poll joysticks. It should    **/
/** return 2x16bit values in a following way:               **/
/**                                                         **/
/**      x.FIRE-B.x.x.L.D.R.U.x.FIRE-A.x.x.N3.N2.N1.N0      **/
/**                                                         **/
/** Least significant bit on the right. Active value is 1.  **/
/*************************************************************/
unsigned int Joystick(void)
{
  unsigned int Joy=0;
  SceCtrlData pad;

  /* If starting up, flash the menu */
  if (FlashMenu)
  {
    FlashMenu=0;

    /* Display the menu */
    OpenMenu();

    /* Reset the system */
    ResetColeco(Mode);
  }

  if (pspCtrlPollControls(&pad))
  {
    if (ShowKybdHeld) pl_vk_navigate(&KeyLayout, &pad);

#ifdef PSP_DEBUG
    if ((pad.Buttons & (PSP_CTRL_SELECT | PSP_CTRL_START))
      == (PSP_CTRL_SELECT | PSP_CTRL_START))
        pspUtilSaveVramSeq(ScreenshotPath, "game");
#endif

    /* Parse input */
    int i, on, code;
    for (i = 0; ButtonMapId[i] >= 0; i++)
    {
      code = GameConfig.ButtonMap[ButtonMapId[i]];
      on = (pad.Buttons & ButtonMask[i]) == ButtonMask[i];

      /* Check to see if a button set is pressed. If so, unset it, so it */
      /* doesn't trigger any other combination presses. */
      if (on) pad.Buttons &= ~ButtonMask[i];

      if (!Rewinding)
      {
        if (code & JST && !ShowKybdHeld)
        {
          /* Joystick state change */
          if (on) Joy|=CODE_MASK(code);
        }
      }

      if (code & SPC)
      {
        /* Emulator-specific input */
        HandleSpecialInput(CODE_MASK(code), on);
      }
    }

    /* Rewind/save state */
    if (!Rewinding)
    {
      if (--FramesUntilSave <= 0)
      {
        FramesUntilSave = RewindSaveRate;
        pl_rewind_save(&Rewinder);
      }
    }
    else
    {
      FramesUntilSave = RewindSaveRate;
      pl_rewind_restore(&Rewinder);
    }
  }

  return(Joy|Keypad);
}

/** SetColor() ***********************************************/
/** Set color N to (R,G,B).                                 **/
/*************************************************************/
int SetColor(byte N,byte R,byte G,byte B)
{
  return(RGB(R,G,B));
}
/** HandleSpecialInput() *************************************/
/** Handles emulator-specific buttons                       **/
/*************************************************************/
static void HandleSpecialInput(int code, int on)
{
  switch (code)
  {
  case SPC_KYBD:

    if (ShowKybdHeld != on)
    {
      if (on) pl_vk_reinit(&KeyLayout);
      else
      {
        ClearBufferCount = 2;
        pl_vk_release_all(&KeyLayout);
      }
    }

    ShowKybdHeld = on;
    break;
  case SPC_REWIND:
    Rewinding = on;
    SoundSuspended = on;
    break;
  case SPC_MENU:
    if (on) OpenMenu();
    break;
  }
}

