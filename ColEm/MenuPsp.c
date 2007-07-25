/** ColEm: portable ColecoVision emulator ********************/
/**                                                         **/
/**                         MenuPsp.c                       **/
/**                                                         **/
/** This file contains routines used for menu navigation    **/
/** for the PSP version of ColEm                            **/
/**                                                         **/
/** Copyright (C) Akop Karapetyan 2007                      **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#include <string.h>
#include <malloc.h>

#include <time.h>
#include <psptypes.h>
#include <psprtc.h>
#include <pspkernel.h>

#include "Coleco.h"
#include "INIFile.h"

#include "psp.h"
#include "ui.h"
#include "fileio.h"
#include "video.h"
#include "ctrl.h"
#include "image.h"

#ifdef MINIZIP
#include "unzip.h"
#endif

#include "LibPsp.h"
#include "MenuPsp.h"

#define SYSTEM_SCRNSHOT    1
#define SYSTEM_RESET       2
#define SYSTEM_TIMING      3

#define OPTION_DISPLAY_MODE 1
#define OPTION_SYNC_FREQ    2
#define OPTION_FRAMESKIP    3
#define OPTION_VSYNC        4
#define OPTION_CLOCK_FREQ   5
#define OPTION_SHOW_FPS     6
#define OPTION_CONTROL_MODE 7

#define TAB_QUICKLOAD 0
#define TAB_STATE     1
#define TAB_CONTROL   2
#define TAB_OPTION    3
#define TAB_SYSTEM    4
#define TAB_ABOUT     5
#define TAB_MAX       TAB_SYSTEM

PspImage *Screen;
int DisplayMode;
int UpdateFreq;
int VSync;
int ClockFreq;
int Frameskip;
int ShowFps;

static char *GameName;
static char *RomPath;
static char *Quickload;
static PspImage *Background;
static PspImage *NoSaveIcon;

static int TabIndex;
static int ExitMenu;
static int ControlMode;

static const char *ConfigDir = "conf";
static const char *SaveStateDir = "states";
static const char *DefConfigFile = "default";
static const char *ScreenshotDir = "screens";
static const char *OptionsFile = "colem.ini";
static const char *NullConfigFile = "NOCART";

static const char
  *QuickloadFilter[] = { "ROM", "ROM.GZ", "ZIP", '\0' },
  PresentSlotText[] = "\026\244\020 Save\t\026\001\020 Load\t\026\243\020 Delete",
  EmptySlotText[] = "\026\244\020 Save",
  ControlHelpText[] = "\026\250\020 Change mapping\t\026\001\020 Save to \271\t\026\244\020 Set as default\t\026\243\020 Load defaults";

char *ScreenshotPath;
static char *SaveStatePath;

/* Define various menu options */
static const PspMenuOptionDef
  ToggleOptions[] = {
    { "Disabled", (void*)0 },
    { "Enabled", (void*)1 },
    { NULL, NULL } },
  ScreenSizeOptions[] = {
    { "Actual size", (void*)DISPLAY_MODE_UNSCALED },
    { "4:3 scaled (fit height)", (void*)DISPLAY_MODE_FIT_HEIGHT },
    { "16:9 scaled (fit screen)", (void*)DISPLAY_MODE_FILL_SCREEN },
    { NULL, NULL } },
  FrameLimitOptions[] = {
    { "Disabled", (void*)0 },
    { "60 fps (NTSC)", (void*)60 },
    { "50 fps (PAL)", (void*)50 },
    { NULL, NULL } },
  FrameSkipOptions[] = {
    { "No skipping", (void*)0 },
    { "Skip 1 frame", (void*)1 },
    { "Skip 2 frames", (void*)2 },
    { "Skip 3 frames", (void*)3 },
    { "Skip 4 frames", (void*)4 },
    { "Skip 5 frames", (void*)5 },
    { NULL, NULL } },
  PspClockFreqOptions[] = {
    { "222 MHz", (void*)222 },
    { "300 MHz", (void*)300 },
    { "333 MHz", (void*)333 },
    { NULL, NULL } },
  TimingOptions[] = {
    { "NTSC", (void*)0 },
    { "PAL",  (void*)CV_PAL },
    { NULL, NULL } },
  ControlModeOptions[] = {
    { "\026\242\020 cancels, \026\241\020 confirms (US)", (void*)0 },
    { "\026\241\020 cancels, \026\242\020 confirms (Japan)", (void*)1 },
    { NULL, NULL } },
  ButtonMapOptions[] = {
    /* Unmapped */
    { "None", (void*)0 },
    /* Special */
    { "Special: Open Menu", (void*)(SPC|SPC_MENU) },  
    { "Special: Show keyboard", (void*)(SPC|SPC_KYBD) },  
    /* Joystick */
    { "Joystick Up",          (void*)(JST|JST_UP) },
    { "Joystick Down",        (void*)(JST|JST_DOWN) },
    { "Joystick Left",        (void*)(JST|JST_LEFT) },
    { "Joystick Right",       (void*)(JST|JST_RIGHT) },
    { "Joystick Left Fire",   (void*)(JST|JST_FIREL) },
    { "Joystick Right Fire",  (void*)(JST|JST_FIRER) },
    { "Joystick Purple Fire", (void*)(JST|JST_PURPLE) },
    { "Joystick Blue Fire",   (void*)(JST|JST_BLUE) },
    /* Digits */
    { "Keypad 0", (void*)(JST|JST_0) },
    { "Keypad 1", (void*)(JST|JST_1) },
    { "Keypad 2", (void*)(JST|JST_2) },
    { "Keypad 3", (void*)(JST|JST_3) },
    { "Keypad 4", (void*)(JST|JST_4) },
    { "Keypad 5", (void*)(JST|JST_5) },
    { "Keypad 6", (void*)(JST|JST_6) },
    { "Keypad 7", (void*)(JST|JST_7) },
    { "Keypad 8", (void*)(JST|JST_8) },
    { "Keypad 9", (void*)(JST|JST_9) },
    { "Keypad *", (void*)(JST|JST_STAR) }, 
    { "Keypad #", (void*)(JST|JST_POUND) }, 
    /* End */
    { NULL, NULL } };

/* Define menu lists */
static const PspMenuItemDef 
  SystemMenuDef[] = {
    { "\tConfiguration", NULL, NULL, -1, NULL },
    { "CPU Timing",         (void*)SYSTEM_TIMING,
      TimingOptions,       -1, 
      "\026\250\020 Select video timing mode (PAL/NTSC)" },
    { "\tSystem", NULL, NULL, -1, NULL },
    { "Reset",            (void*)SYSTEM_RESET,
      NULL,             -1, "\026\001\020 Reset" },
    { "Save screenshot",  (void*)SYSTEM_SCRNSHOT,
      NULL,             -1, "\026\001\020 Save screenshot" },
    { NULL, NULL }
  },
  ControlMenuDef[] = {
    { PSP_CHAR_ANALUP,     (void*)MAP_ANALOG_UP,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_ANALDOWN,   (void*)MAP_ANALOG_DOWN,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_ANALLEFT,   (void*)MAP_ANALOG_LEFT,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_ANALRIGHT,  (void*)MAP_ANALOG_RIGHT,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_UP,         (void*)MAP_BUTTON_UP,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_DOWN,       (void*)MAP_BUTTON_DOWN,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_LEFT,       (void*)MAP_BUTTON_LEFT,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_RIGHT,      (void*)MAP_BUTTON_RIGHT,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_SQUARE,     (void*)MAP_BUTTON_SQUARE,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_CROSS,      (void*)MAP_BUTTON_CROSS,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_CIRCLE,     (void*)MAP_BUTTON_CIRCLE,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_TRIANGLE,   (void*)MAP_BUTTON_TRIANGLE,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_LTRIGGER,   (void*)MAP_BUTTON_LTRIGGER,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_RTRIGGER,   (void*)MAP_BUTTON_RTRIGGER,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_SELECT,     (void*)MAP_BUTTON_SELECT,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_START,      (void*)MAP_BUTTON_START,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_LTRIGGER"+"PSP_CHAR_RTRIGGER,
                           (void*)MAP_BUTTON_LRTRIGGERS,
      ButtonMapOptions, -1, ControlHelpText },
    { PSP_CHAR_START"+"PSP_CHAR_SELECT,
                           (void*)MAP_BUTTON_STARTSELECT,
      ButtonMapOptions, -1, ControlHelpText },
    { NULL, NULL }
  },
  OptionMenuDef[] = {
    { "\tVideo", NULL, NULL, -1, NULL },
    { "Screen size",         (void*)OPTION_DISPLAY_MODE, 
      ScreenSizeOptions,   -1, "\026\250\020 Change screen size" },
    { "\tPerformance", NULL, NULL, -1, NULL },
    { "Frame limiter",       (void*)OPTION_SYNC_FREQ, 
      FrameLimitOptions,   -1, "\026\250\020 Change screen update frequency" },
    { "Frame skipping",      (void*)OPTION_FRAMESKIP,
      FrameSkipOptions,    -1, "\026\250\020 Change number of frames skipped per update" },
    { "VSync",               (void*)OPTION_VSYNC,
      ToggleOptions,       -1, "\026\250\020 Enable to reduce tearing; disable to increase speed" },
    { "PSP clock frequency", (void*)OPTION_CLOCK_FREQ,
      PspClockFreqOptions, -1, 
      "\026\250\020 Larger values: faster emulation, faster battery depletion (default: 222MHz)" },
    { "Show FPS counter",    (void*)OPTION_SHOW_FPS,
      ToggleOptions,       -1, "\026\250\020 Show/hide the frames-per-second counter" },
    { "\tMenu", NULL, NULL, -1, NULL },
    { "Button mode",        (void*)OPTION_CONTROL_MODE,
      ControlModeOptions,  -1, "\026\250\020 Change OK and Cancel button mapping" },
    { NULL, NULL }
  };

// TODO:
#define WIDTH  272
#define HEIGHT 200

/* Game configuration (includes button maps) */
struct GameConfig GameConfig;

/* Default configuration */
struct GameConfig DefaultConfig = 
{
  {
    JST|JST_UP,      /* Analog Up    */
    JST|JST_DOWN,    /* Analog Down  */
    JST|JST_LEFT,    /* Analog Left  */
    JST|JST_RIGHT,   /* Analog Right */
    JST|JST_UP,      /* D-pad Up     */
    JST|JST_DOWN,    /* D-pad Down   */
    JST|JST_LEFT,    /* D-pad Left   */
    JST|JST_RIGHT,   /* D-pad Right  */
    JST|JST_FIREL,   /* Square       */
    JST|JST_FIRER,   /* Cross        */
    JST|JST_BLUE,    /* Circle       */
    JST|JST_PURPLE,  /* Triangle     */
    0,               /* L Trigger    */
    SPC|SPC_KYBD,    /* R Trigger    */
    0,               /* Select       */
    0,               /* Start        */
    SPC|SPC_MENU,    /* L+R Triggers */
    0,               /* Start+Select */
  }
};

/* Button masks */
const u64 ButtonMask[] = 
{
  PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER, 
  PSP_CTRL_START    | PSP_CTRL_SELECT,
  PSP_CTRL_ANALUP,    PSP_CTRL_ANALDOWN,
  PSP_CTRL_ANALLEFT,  PSP_CTRL_ANALRIGHT,
  PSP_CTRL_UP,        PSP_CTRL_DOWN,
  PSP_CTRL_LEFT,      PSP_CTRL_RIGHT,
  PSP_CTRL_SQUARE,    PSP_CTRL_CROSS,
  PSP_CTRL_CIRCLE,    PSP_CTRL_TRIANGLE,
  PSP_CTRL_LTRIGGER,  PSP_CTRL_RTRIGGER,
  PSP_CTRL_SELECT,    PSP_CTRL_START,
  0 /* End */
};

/* Button map ID's */
const int ButtonMapId[] = 
{
  MAP_BUTTON_LRTRIGGERS, 
  MAP_BUTTON_STARTSELECT,
  MAP_ANALOG_UP,       MAP_ANALOG_DOWN,
  MAP_ANALOG_LEFT,     MAP_ANALOG_RIGHT,
  MAP_BUTTON_UP,       MAP_BUTTON_DOWN,
  MAP_BUTTON_LEFT,     MAP_BUTTON_RIGHT,
  MAP_BUTTON_SQUARE,   MAP_BUTTON_CROSS,
  MAP_BUTTON_CIRCLE,   MAP_BUTTON_TRIANGLE,
  MAP_BUTTON_LTRIGGER, MAP_BUTTON_RTRIGGER,
  MAP_BUTTON_SELECT,   MAP_BUTTON_START,
  -1 /* End */
};

/* Tab labels */
static const char *TabLabel[] = 
{
  "Game",
  "Save/Load", 
  "Controls",
  "Options",
  "System",
  "aboot"
};

static const char* GetConfigName();

static void DisplayStateTab();
static void DisplayControlTab();
PspImage*   LoadStateIcon(const char *path);
int         LoadState(const char *path);
PspImage*   SaveState(const char *path, PspImage *icon);

static void LoadOptions();
static void InitOptionDefaults();
static int  SaveOptions();

static int LoadResource(const char *filename);

int  OnGenericCancel(const void *uiobject, const void *param);
void OnGenericRender(const void *uiobject, const void *item_obj);
int  OnGenericButtonPress(const PspUiFileBrowser *browser, const char *path, 
       u32 button_mask);

void OnSystemRender(const void *uiobject, const void *item_obj);

int  OnQuickloadOk(const void *browser, const void *path);

int  OnSaveStateOk(const void *gallery, const void *item);
int  OnSaveStateButtonPress(const PspUiGallery *gallery, PspMenuItem* item, 
       u32 button_mask);

int  OnMenuOk(const void *menu, const void *item);
int  OnMenuItemChanged(const struct PspUiMenu *uimenu, PspMenuItem* item, 
       const PspMenuOption* option);
int  OnMenuButtonPress(const struct PspUiMenu *uimenu, PspMenuItem* item, 
       u32 button_mask);

void OnSplashRender(const void *uiobject, const void *null);
int  OnSplashButtonPress(const struct PspUiSplash *splash, u32 button_mask);
const char* 
     OnSplashGetStatusBarText(const struct PspUiSplash *splash);

static void InitGameConfig(struct GameConfig *config);
static int  SaveGameConfig(const char *filename, const struct GameConfig *config);
static int  LoadGameConfig(const char *filename, struct GameConfig *config);

PspUiFileBrowser QuickloadBrowser = 
{
  OnGenericRender,
  OnQuickloadOk,
  OnGenericCancel,
  OnGenericButtonPress,
  QuickloadFilter,
  0
};

PspUiGallery SaveStateGallery = 
{
  NULL,                        /* PspMenu */
  OnGenericRender,             /* OnRender() */
  OnSaveStateOk,               /* OnOk() */
  OnGenericCancel,             /* OnCancel() */
  OnSaveStateButtonPress,      /* OnButtonPress() */
  NULL                         /* Userdata */
};

PspUiMenu OptionUiMenu =
{
  NULL,                  /* PspMenu */
  OnGenericRender,       /* OnRender() */
  OnMenuOk,              /* OnOk() */
  OnGenericCancel,       /* OnCancel() */
  OnMenuButtonPress,     /* OnButtonPress() */
  OnMenuItemChanged,     /* OnItemChanged() */
};

PspUiMenu ControlUiMenu =
{
  NULL,                  /* PspMenu */
  OnGenericRender,       /* OnRender() */
  OnMenuOk,              /* OnOk() */
  OnGenericCancel,       /* OnCancel() */
  OnMenuButtonPress,     /* OnButtonPress() */
  OnMenuItemChanged,     /* OnItemChanged() */
};

PspUiMenu SystemUiMenu =
{
  NULL,                  /* PspMenu */
  OnSystemRender,        /* OnRender() */
  OnMenuOk,              /* OnOk() */
  OnGenericCancel,       /* OnCancel() */
  OnMenuButtonPress,     /* OnButtonPress() */
  OnMenuItemChanged,     /* OnItemChanged() */
};

PspUiSplash SplashScreen = 
{
  OnSplashRender,
  OnGenericCancel,
  OnSplashButtonPress,
  OnSplashGetStatusBarText
};

void InitMenu()
{
  int i;
  PspMenuItem *item;

  /* Initialize local vars */
  Quickload = NULL;
  RomPath = NULL;
  GameName = NULL;
  TabIndex = TAB_ABOUT;
  Background = NULL;
  ControlMode = 0;

  /* Initialize paths */
  SaveStatePath 
    = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(SaveStateDir) + 2));
  sprintf(SaveStatePath, "%s%s/", pspGetAppDirectory(), SaveStateDir);
  ScreenshotPath 
    = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(ScreenshotDir) + 2));
  sprintf(ScreenshotPath, "%s%s/", pspGetAppDirectory(), ScreenshotDir);

  /* Load the background image */
  Background = pspImageLoadPng("background.png");

  /* Init NoSaveState icon image */
  NoSaveIcon=pspImageCreate(136, 100, PSP_IMAGE_16BPP);
  pspImageClear(NoSaveIcon, RGB(0xff,0xff,0xff));

  /* Initialize state menu */
  SaveStateGallery.Menu = pspMenuCreate();
  for (i = 0; i < 10; i++)
  {
    item = pspMenuAppendItem(SaveStateGallery.Menu, NULL, (void*)i);
    pspMenuSetHelpText(item, EmptySlotText);
  }

  /* Initialize options menu */
  OptionUiMenu.Menu = pspMenuCreate();
  pspMenuLoad(OptionUiMenu.Menu, OptionMenuDef);

  /* Initialize system menu */
  SystemUiMenu.Menu = pspMenuCreate();
  pspMenuLoad(SystemUiMenu.Menu, SystemMenuDef);

  /* Initialize control menu */
  ControlUiMenu.Menu = pspMenuCreate();
  pspMenuLoad(ControlUiMenu.Menu, ControlMenuDef);

  /* Initialize screen buffer */
  Screen = pspImageCreate(WIDTH, HEIGHT, PSP_IMAGE_16BPP);
  pspImageClear(Screen, 0x8000);

  /* Initialize options */
  LoadOptions();

  /* Load default configuration */
  LoadGameConfig(DefConfigFile, &DefaultConfig);

  /* Initialize configuration */
  InitGameConfig(&GameConfig);

  /* Initialize UI components */
  UiMetric.Background = Background;
  UiMetric.Font = &PspStockFont;
  UiMetric.Left = 8;
  UiMetric.Top = 24;
  UiMetric.Right = 472;
  UiMetric.Bottom = 240;
  UiMetric.OkButton = (!ControlMode) ? PSP_CTRL_CROSS : PSP_CTRL_CIRCLE;
  UiMetric.CancelButton = (!ControlMode) ? PSP_CTRL_CIRCLE : PSP_CTRL_CROSS;
  UiMetric.ScrollbarColor = PSP_COLOR_WHITE;
  UiMetric.ScrollbarBgColor = 0x77777777;
  UiMetric.ScrollbarWidth = 10;
  UiMetric.TextColor = PSP_COLOR_WHITE;
  UiMetric.SelectedColor = PSP_COLOR_WHITE;
  UiMetric.SelectedBgColor = COLOR(0xdd,0x3f,0x3f,0xff);
  UiMetric.StatusBarColor = PSP_COLOR_WHITE;
  UiMetric.BrowserFileColor = PSP_COLOR_WHITE;
  UiMetric.BrowserDirectoryColor = PSP_COLOR_YELLOW;
  UiMetric.GalleryIconsPerRow = 5;
  UiMetric.GalleryIconMarginWidth = 8;
  UiMetric.MenuItemMargin = 20;
  UiMetric.MenuSelOptionBg = COLOR(0xdd,0x3f,0x3f,0xff);
  UiMetric.MenuOptionBoxColor = COLOR(0xe0,0xe0,0xe0,0xff);
  UiMetric.MenuOptionBoxBg = COLOR(0x90,0x90,0x90,0xd0);
  UiMetric.MenuDecorColor = COLOR(0xc0,0xc0,0xc0,0xff);
  UiMetric.DialogFogColor = COLOR(0,0,0,88);
  UiMetric.DialogBorderColor = PSP_COLOR_WHITE;
  UiMetric.DialogBgColor = PSP_COLOR_GRAY;
  UiMetric.TitlePadding = 4;
  UiMetric.TitleColor = PSP_COLOR_YELLOW;
  UiMetric.MenuFps = 30;
  UiMetric.TabBgColor = COLOR(0xb8,0xb8,0xb8,0xff);
}

int OnQuickloadOk(const void *browser, const void *path)
{
  /* Load disk or cartridge */
  if (!LoadResource(path))
    return 0;

  if (GameName) free(GameName);
  GameName = strdup(pspFileIoGetFilename(path));

  /* Load game configuration */
  if (!LoadGameConfig(GetConfigName(), &GameConfig))
    pspUiAlert("ERROR: Configuration not loaded");

  ExitMenu = 1;

  return 1;
}

/* Loads a cartridge or disk image into specified slot */
static int LoadResource(const char *filename)
{
  char *file_to_load = NULL;

#ifdef MINIZIP
  /* Check if it's a zip file */
  if (pspFileIoEndsWith(filename, "ZIP"))
  {
    unzFile zip;
    unz_file_info fi;
    unz_global_info gi;
    char arch_file[255], loadable_file[255];
    loadable_file[0] = '\0';

    /* Open archive for reading */
    if (!(zip = unzOpen(filename)))
    {
      pspUiAlert("Error opening compressed file");
      return 0;
    }

    /* Get global ZIP file information */
    if (unzGetGlobalInfo(zip, &gi) != UNZ_OK)
    {
      pspUiAlert("Error reading compressed file information");
      unzClose(zip);
      return 0;
    }

    int i;
    for (i = 0; i < gi.number_entry; i++)
    {
      /* Get name of the archived file */
      if (unzGetCurrentFileInfo(zip, &fi, arch_file, 
        sizeof(arch_file), NULL, 0, NULL, 0) != UNZ_OK)
      {
        pspUiAlert("Error reading compressed file information");
        unzClose(zip);
        return 0;
      }

      /* For ROM's, just load the first available one */
      if (pspFileIoEndsWith(arch_file, "ROM"))
      {
        strcpy(loadable_file, arch_file);
        break;
      }

      /* Go to the next file in the archive */
      if (i + 1 < gi.number_entry)
      {
        if (unzGoToNextFile(zip) != UNZ_OK)
        {
          pspUiAlert("Error parsing compressed files");
          unzClose(zip);
          return 0;
        }
      }
    }

    /* Done reading */
    unzClose(zip);

    if (!loadable_file[0])
    {
      pspUiAlert("No loadable files found in the archive");
      return 0;
    }

    /* Build the virtual compressed filename */
    if (!(file_to_load = malloc(sizeof(char) * (strlen(filename) + strlen(loadable_file) + 2))))
      return 0;

    sprintf(file_to_load, "%s/%s", filename, loadable_file);
  }
  else
#endif
  file_to_load = strdup(filename);

  /* Load cartridge */
  if (pspFileIoEndsWith(file_to_load, "ROM") || pspFileIoEndsWith(file_to_load, "ROM.GZ"))
  {
    if (!LoadROM(file_to_load))
    {
      free(file_to_load);
      pspUiAlert("Error loading cartridge");
      return 0;
    }

    /* Set path as new cart path */
    free(RomPath);
    RomPath = pspFileIoGetParentDirectory(filename);

    if (Quickload) free(Quickload);
    Quickload = strdup(filename);
  }

  free(file_to_load);

  /* Clear the screen */
  pspImageClear(Screen, 0x8000);

  return 1;
}

int OnGenericCancel(const void *uiobject, const void *param)
{
  ExitMenu = 1;
  return 1;
}

void OnSplashRender(const void *splash, const void *null)
{
  int fh, i, x, y, height;
  const char *lines[] = 
  { 
    "ColEm-PSP version "VERSION" ("__DATE__")", 
    "\026http://psp.akop.org/colem",
    " ",
    "2007 Akop Karapetyan (port)",
    "1994-2007 Marat Fayzullin (emulation)",
    NULL
  };

  fh = pspFontGetLineHeight(UiMetric.Font);

  for (i = 0; lines[i]; i++);
  height = fh * (i - 1);

  /* Render lines */
  for (i = 0, y = SCR_HEIGHT / 2 - height / 2; lines[i]; i++, y += fh)
  {
    x = SCR_WIDTH / 2 - pspFontGetTextWidth(UiMetric.Font, lines[i]) / 2;
    pspVideoPrint(UiMetric.Font, x, y, lines[i], UiMetric.TextColor);
  }

  /* Render PSP status */
  OnGenericRender(splash, null);
}

int OnSplashButtonPress(const struct PspUiSplash *splash, u32 button_mask)
{
  return OnGenericButtonPress(NULL, NULL, button_mask);
}

const char* OnSplashGetStatusBarText(const struct PspUiSplash *splash)
{
  return "\026\255\020/\026\256\020 Switch tabs";
}

int OnGenericButtonPress(const PspUiFileBrowser *browser, const char *path, 
      u32 button_mask)
{
  int tab_index;

  /* If L or R are pressed, switch tabs */
  if (button_mask & PSP_CTRL_LTRIGGER)
  {
    TabIndex--;
    do
    {
      tab_index = TabIndex;
      if (!GameName && (TabIndex == TAB_STATE || TabIndex == TAB_CONTROL)) TabIndex--;
      if (TabIndex < 0) TabIndex = TAB_MAX;
    } while (tab_index != TabIndex);
  }
  else if (button_mask & PSP_CTRL_RTRIGGER)
  {
    TabIndex++;
    do
    {
      tab_index = TabIndex;
      if (!GameName && (TabIndex == TAB_STATE || TabIndex == TAB_CONTROL)) TabIndex++;
      if (TabIndex > TAB_MAX) TabIndex = 0;
    } while (tab_index != TabIndex);
  }
  else if ((button_mask & (PSP_CTRL_START | PSP_CTRL_SELECT)) 
    == (PSP_CTRL_START | PSP_CTRL_SELECT))
  {
    if (CaptureVramBuffer(ScreenshotPath, "ui"))
      pspUiAlert("Saved successfully");
    else
      pspUiAlert("ERROR: Not saved");
    return 0;
  }
  else return 0;

  return 1;
}

void DisplayMenu()
{
  ExitMenu = 0;
  PspMenuItem *item;

  /* Set normal clock frequency */
  pspSetClockFrequency(222);
  /* Set buttons to autorepeat */
  pspCtrlSetPollingMode(PSP_CTRL_AUTOREPEAT);

  /* Menu loop */
  while (!ExitPSP && !ExitMenu)
  {
    /* Display appropriate tab */
    switch (TabIndex)
    {
    case TAB_QUICKLOAD:
      pspUiOpenBrowser(&QuickloadBrowser, Quickload ? Quickload : RomPath);
      break;
    case TAB_STATE:
      DisplayStateTab();
      break;
    case TAB_OPTION:
      /* Init menu options */
      item = pspMenuFindItemByUserdata(OptionUiMenu.Menu, (void*)OPTION_DISPLAY_MODE);
      pspMenuSelectOptionByValue(item, (void*)DisplayMode);
      item = pspMenuFindItemByUserdata(OptionUiMenu.Menu, (void*)OPTION_SYNC_FREQ);
      pspMenuSelectOptionByValue(item, (void*)UpdateFreq);
      item = pspMenuFindItemByUserdata(OptionUiMenu.Menu, (void*)OPTION_FRAMESKIP);
      pspMenuSelectOptionByValue(item, (void*)(int)Frameskip);
      item = pspMenuFindItemByUserdata(OptionUiMenu.Menu, (void*)OPTION_VSYNC);
      pspMenuSelectOptionByValue(item, (void*)VSync);
      item = pspMenuFindItemByUserdata(OptionUiMenu.Menu, (void*)OPTION_CLOCK_FREQ);
      pspMenuSelectOptionByValue(item, (void*)ClockFreq);
      item = pspMenuFindItemByUserdata(OptionUiMenu.Menu, (void*)OPTION_SHOW_FPS);
      pspMenuSelectOptionByValue(item, (void*)ShowFps);
      item = pspMenuFindItemByUserdata(OptionUiMenu.Menu, (void*)OPTION_CONTROL_MODE);
      pspMenuSelectOptionByValue(item, (void*)ControlMode);
      pspUiOpenMenu(&OptionUiMenu, NULL);
      break;
    case TAB_SYSTEM:
      /* Init system configuration */
      item = pspMenuFindItemByUserdata(SystemUiMenu.Menu, (void*)SYSTEM_TIMING);
      pspMenuSelectOptionByValue(item, (void*)(Mode & CV_PAL));

      pspUiOpenMenu(&SystemUiMenu, NULL);
      break;
    case TAB_CONTROL:
      DisplayControlTab();
      break;
    case TAB_ABOUT:
      pspUiSplashScreen(&SplashScreen);
      break;
    }
  }

  if (!ExitPSP)
  {
    /* Set clock frequency during emulation */
    pspSetClockFrequency(ClockFreq);
    /* Set buttons to normal mode */
    pspCtrlSetPollingMode(PSP_CTRL_NORMAL);
  }
}

/* Handles drawing of generic items */
void OnGenericRender(const void *uiobject, const void *item_obj)
{
  static char status[128];
  pspUiGetStatusString(status, sizeof(status));

  int height = pspFontGetLineHeight(UiMetric.Font);
  int width = pspFontGetTextWidth(UiMetric.Font, status);
  pspVideoPrint(UiMetric.Font, SCR_WIDTH - width, 0, status, PSP_COLOR_WHITE);

  /* Draw tabs */
  int i, x;
  for (i = 0, x = 5; i <= TAB_MAX; i++)
  {
    if (!GameName)
      if (i == TAB_STATE || i == TAB_CONTROL) continue;

    /* Determine width of text */
    width = pspFontGetTextWidth(UiMetric.Font, TabLabel[i]);

    /* Draw background of active tab */
    if (i == TabIndex) 
      pspVideoFillRect(x - 5, 0, x + width + 5, height + 1, 
        UiMetric.TabBgColor);

    /* Draw name of tab */
    pspVideoPrint(UiMetric.Font, x, 0, TabLabel[i], PSP_COLOR_WHITE);

    x += width + 10;
  }
}

int OnMenuItemChanged(const struct PspUiMenu *uimenu, PspMenuItem* item, 
      const PspMenuOption* option)
{
  if (uimenu == &ControlUiMenu)
  {
    GameConfig.ButtonMap[(int)item->Userdata] = (unsigned int)option->Value;
  }
  else if (uimenu == &SystemUiMenu)
  {
    switch((int)item->Userdata)
    {
    case SYSTEM_TIMING:
      if ((int)option->Value != (Mode & CV_PAL))
      {
        if (!pspUiConfirm("This will reset the system. Proceed?"))
          return 0;

        ResetColeco((Mode&~CV_PAL)|(int)option->Value);
      }
      break;
    }
  }
  else if (uimenu == &OptionUiMenu)
  {
    switch((int)item->Userdata)
    {
    case OPTION_DISPLAY_MODE:
      DisplayMode = (int)option->Value;
      break;
    case OPTION_SYNC_FREQ:
      UpdateFreq = (int)option->Value;
      break;
    case OPTION_FRAMESKIP:
      Frameskip = (int)option->Value;
      break;
    case OPTION_VSYNC:
      VSync = (int)option->Value;
      break;
    case OPTION_CLOCK_FREQ:
      ClockFreq = (int)option->Value;
      break;
    case OPTION_SHOW_FPS:
      ShowFps = (int)option->Value;
      break;
    case OPTION_CONTROL_MODE:
      ControlMode = (int)option->Value;
      UiMetric.OkButton = (!ControlMode) ? PSP_CTRL_CROSS : PSP_CTRL_CIRCLE;
      UiMetric.CancelButton = (!ControlMode) ? PSP_CTRL_CIRCLE : PSP_CTRL_CROSS;
      break;
    }
  }

  return 1;
}

int OnMenuOk(const void *uimenu, const void* sel_item)
{
  if (uimenu == &ControlUiMenu)
  {
    /* Save to MS */
    if (SaveGameConfig(GetConfigName(), &GameConfig))
      pspUiAlert("Changes saved");
    else
      pspUiAlert("ERROR: Changes not saved");
  }
  else if (uimenu == &SystemUiMenu)
  {
    switch ((int)((const PspMenuItem*)sel_item)->Userdata)
    {
    case SYSTEM_RESET:

      /* Reset system */
      if (pspUiConfirm("Reset the system?"))
      {
        ExitMenu = 1;
        ResetColeco(Mode);
        return 1;
      }
      break;

    case SYSTEM_SCRNSHOT:

      /* Save screenshot */
      if (!SaveScreenshot(ScreenshotPath, GetConfigName(), Screen))
        pspUiAlert("ERROR: Screenshot not saved");
      else
        pspUiAlert("Screenshot saved successfully");
      break;
    }
  }

  return 0;
}

int OnMenuButtonPress(const struct PspUiMenu *uimenu, PspMenuItem* sel_item, 
      u32 button_mask)
{
  if (uimenu == &ControlUiMenu)
  {
    if (button_mask & PSP_CTRL_SQUARE)
    {
      /* Save to MS as default mapping */
      if (SaveGameConfig(DefConfigFile, &GameConfig))
      {
        /* Modify in-memory defaults */
        memcpy(&DefaultConfig, &GameConfig, sizeof(struct GameConfig));
        pspUiAlert("Changes saved");
      }
      else
        pspUiAlert("ERROR: Changes not saved");

      return 0;
    }
    else if (button_mask & PSP_CTRL_TRIANGLE)
    {
      PspMenuItem *item;
      int i;

      /* Load default mapping */
      memcpy(&GameConfig, &DefaultConfig, sizeof(struct GameConfig));

      /* Modify the menu */
      for (item = ControlUiMenu.Menu->First, i = 0; item; item = item->Next, i++)
        pspMenuSelectOptionByValue(item, (void*)DefaultConfig.ButtonMap[i]);

      return 0;
    }
  }

  return OnGenericButtonPress(NULL, NULL, button_mask);
}

int OnSaveStateOk(const void *gallery, const void *item)
{
  char *path;
  const char *config_name = GetConfigName();

  path = (char*)malloc(strlen(SaveStatePath) + strlen(config_name) + 8);
  sprintf(path, "%s%s_%02i.sta", SaveStatePath, config_name, 
    (int)((const PspMenuItem*)item)->Userdata);

  if (pspFileIoCheckIfExists(path) && pspUiConfirm("Load state?"))
  {
    if (LoadState(path))
    {
      ExitMenu = 1;
      pspMenuFindItemByUserdata(((const PspUiGallery*)gallery)->Menu, 
        ((const PspMenuItem*)item)->Userdata);
      free(path);

      return 1;
    }
    pspUiAlert("ERROR: State failed to load");
  }

  free(path);
  return 0;
}

int OnSaveStateButtonPress(const PspUiGallery *gallery, 
      PspMenuItem *sel, 
      u32 button_mask)
{
  if (button_mask & PSP_CTRL_SQUARE 
    || button_mask & PSP_CTRL_TRIANGLE)
  {
    char *path;
    char caption[32];
    const char *config_name = GetConfigName();
    PspMenuItem *item = pspMenuFindItemByUserdata(gallery->Menu, sel->Userdata);

    path = (char*)malloc(strlen(SaveStatePath) + strlen(config_name) + 8);
    sprintf(path, "%s%s_%02i.sta", 
      SaveStatePath, 
      config_name, 
      (int)item->Userdata);

    do /* not a real loop; flow control construct */
    {
      if (button_mask & PSP_CTRL_SQUARE)
      {
        if (pspFileIoCheckIfExists(path) && !pspUiConfirm("Overwrite existing state?"))
          break;

        pspUiFlashMessage("Saving, please wait ...");

        PspImage *icon;
        if (!(icon = SaveState(path, Screen)))
        {
          pspUiAlert("ERROR: State not saved");
          break;
        }

        SceIoStat stat;

        /* Trash the old icon (if any) */
        if (item->Icon && item->Icon != NoSaveIcon)
          pspImageDestroy((PspImage*)item->Icon);

        /* Update icon, help text */
        item->Icon = icon;
        pspMenuSetHelpText(item, PresentSlotText);

        /* Get file modification time/date */
        if (sceIoGetstat(path, &stat) < 0)
          sprintf(caption, "ERROR");
        else
          sprintf(caption, "%02i/%02i/%02i %02i:%02i", 
            stat.st_mtime.month,
            stat.st_mtime.day,
            stat.st_mtime.year - (stat.st_mtime.year / 100) * 100,
            stat.st_mtime.hour,
            stat.st_mtime.minute);

        pspMenuSetCaption(item, caption);
      }
      else if (button_mask & PSP_CTRL_TRIANGLE)
      {
        if (!pspFileIoCheckIfExists(path) || !pspUiConfirm("Delete state?"))
          break;

        if (!pspFileIoDelete(path))
        {
          pspUiAlert("ERROR: State not deleted");
          break;
        }

        /* Trash the old icon (if any) */
        if (item->Icon && item->Icon != NoSaveIcon)
          pspImageDestroy((PspImage*)item->Icon);

        /* Update icon, caption */
        item->Icon = NoSaveIcon;
        pspMenuSetHelpText(item, EmptySlotText);
        pspMenuSetCaption(item, "Empty");
      }
    } while (0);

    if (path) free(path);
    return 0;
  }

  return OnGenericButtonPress(NULL, NULL, button_mask);
}

/* Handles any special drawing for the system menu */
void OnSystemRender(const void *uiobject, const void *item_obj)
{
  int w, h, x, y;
  w = WIDTH / 2;
  h = HEIGHT / 2;
  x = SCR_WIDTH - w - 8;
  y = SCR_HEIGHT - h - 80;

  /* Draw a small representation of the screen */
  pspVideoShadowRect(x, y, x + w - 1, y + h - 1, PSP_COLOR_BLACK, 3);
  pspVideoPutImage(Screen, x, y, w, h);
  pspVideoDrawRect(x, y, x + w - 1, y + h - 1, PSP_COLOR_GRAY);

  OnGenericRender(uiobject, item_obj);
}

static void DisplayControlTab()
{
  PspMenuItem *item;
  char *game_name = strdup(GetConfigName());
  char *dot = strrchr(game_name, '.');
  int i;
  if (dot) *dot='\0';

  /* Load current button mappings */
  for (item = ControlUiMenu.Menu->First, i = 0; item; item = item->Next, i++)
    pspMenuSelectOptionByValue(item, (void*)GameConfig.ButtonMap[i]);

  pspUiOpenMenu(&ControlUiMenu, game_name);
  free(game_name);
}

static void DisplayStateTab()
{
  PspMenuItem *item;
  SceIoStat stat;
  char caption[32];
  const char *config_name = GetConfigName();

  if (config_name)
  {
    char *path = (char*)malloc(strlen(SaveStatePath) + strlen(config_name) + 8);
    char *game_name = strdup(config_name);
    char *dot = strrchr(game_name, '.');
    if (dot) *dot='\0';
  
    /* Initialize icons */
    for (item = SaveStateGallery.Menu->First; item; item = item->Next)
    {
      sprintf(path, "%s%s_%02i.sta", 
        SaveStatePath, 
        config_name, 
        (int)item->Userdata);
  
      if (pspFileIoCheckIfExists(path))
      {
        if (sceIoGetstat(path, &stat) < 0)
          sprintf(caption, "ERROR");
        else
          sprintf(caption, "%02i/%02i/%02i %02i:%02i", 
            stat.st_mtime.month,
            stat.st_mtime.day,
            stat.st_mtime.year - (stat.st_mtime.year / 100) * 100,
            stat.st_mtime.hour,
            stat.st_mtime.minute);
  
        pspMenuSetCaption(item, caption);
        item->Icon = LoadStateIcon(path);
        pspMenuSetHelpText(item, PresentSlotText);
      }
      else
      {
        pspMenuSetCaption(item, "Empty");
        item->Icon = NoSaveIcon;
        pspMenuSetHelpText(item, EmptySlotText);
      }
    }
  
    free(path);
    pspUiOpenGallery(&SaveStateGallery, game_name);
    free(game_name);
  
    /* Destroy any icons */
    for (item = SaveStateGallery.Menu->First; item; item = item->Next)
      if (item->Icon != NULL && item->Icon != NoSaveIcon)
        pspImageDestroy((PspImage*)item->Icon);
  }
  else
  {
    pspUiAlert("NULL selected file");
    pspUiSplashScreen(&SplashScreen);
  }
}

/* Load options */
static void LoadOptions()
{
  char *path = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(OptionsFile) + 1));
  sprintf(path, "%s%s", pspGetAppDirectory(), OptionsFile);

  /* Initialize INI structure */
  INIFile ini;
  InitINI(&ini);

  /* Read the file */
  if (!LoadINI(&ini, path))
  {
    /* File does not exist; load defaults */
    TrashINI(&ini);
    InitOptionDefaults();
  }
  else
  {
    char path[PSP_FILEIO_MAX_PATH_LEN + 1];

    /* Load values */
    DisplayMode = INIGetInteger(&ini, "Video", "Display Mode", DISPLAY_MODE_UNSCALED);
    UpdateFreq = INIGetInteger(&ini, "Video", "Update Frequency", 60);
    Frameskip = INIGetInteger(&ini, "Video", "Frameskip", 1);
    VSync = INIGetInteger(&ini, "Video", "VSync", 0);
    ClockFreq = INIGetInteger(&ini, "Video", "PSP Clock Frequency", 222);
    ShowFps = INIGetInteger(&ini, "Video", "Show FPS", 0);
    ControlMode = INIGetInteger(&ini, "Menu", "Control Mode", 0);

    Mode = (Mode&~CV_PAL) | INIGetInteger(&ini, "System", "Timing", Mode & CV_PAL);

    if (RomPath) { free(RomPath); RomPath = NULL; }
    if (INIGetString(&ini, "File", "ROM Path", path, PSP_FILEIO_MAX_PATH_LEN))
      RomPath = strdup(path);
  }

  /* Clean up */
  free(path);
  TrashINI(&ini);
}

/* Save options */
static int SaveOptions()
{
  char *path = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(OptionsFile) + 1));
  sprintf(path, "%s%s", pspGetAppDirectory(), OptionsFile);

  /* Initialize INI structure */
  INIFile ini;
  InitINI(&ini);

  /* Set values */
  INISetInteger(&ini, "Video", "Display Mode", DisplayMode);
  INISetInteger(&ini, "Video", "Update Frequency", UpdateFreq);
  INISetInteger(&ini, "Video", "Frameskip", Frameskip);
  INISetInteger(&ini, "Video", "VSync", VSync);
  INISetInteger(&ini, "Video", "PSP Clock Frequency", ClockFreq);
  INISetInteger(&ini, "Video", "Show FPS", ShowFps);
  INISetInteger(&ini, "Menu", "Control Mode", ControlMode);

  INISetInteger(&ini, "System", "Timing", Mode & CV_PAL);

  if (RomPath) INISetString(&ini, "File", "ROM Path", RomPath);

  /* Save INI file */
  int status = SaveINI(&ini, path);

  /* Clean up */
  TrashINI(&ini);
  free(path);

  return status;
}

/* Load state icon */
PspImage* LoadStateIcon(const char *path)
{
  /* Open file for reading */
  FILE *f = fopen(path, "r");
  if (!f) return NULL;

  /* Position pointer at the end and read the position of the image */
  long pos;
  fseek(f, -sizeof(long), SEEK_END);
  fread(&pos, sizeof(long), 1, f);

  /* Reposition to start of image */
  if (fseek(f, pos, SEEK_SET) != 0)
  {
    fclose(f);
    return 0;
  }

  /* Load image */
  PspImage *image = pspImageLoadPngFd(f);
  fclose(f);

  return image;
}

/* Load state */
int LoadState(const char *path)
{
  return LoadSTA(path);
}

/* Save state */
PspImage* SaveState(const char *path, PspImage *icon)
{
  /* Save state */
  if (!SaveSTA(path))
    return NULL;

  /* Create thumbnail */
  PspImage *thumb = pspImageCreateThumbnail(icon);
  if (!thumb) return NULL;

  /* Reopen file in append mode */
  FILE *f = fopen(path, "a");
  if (!f)
  {
    pspImageDestroy(thumb);
    return NULL;
  }

  long pos = ftell(f);

  /* Write the thumbnail */
  if (!pspImageSavePngFd(f, thumb))
  {
    pspImageDestroy(thumb);
    fclose(f);
    return NULL;
  }

  /* Write the position of the icon */
  fwrite(&pos, sizeof(long), 1, f);

  fclose(f);
  return thumb;
}

/* Initialize options to system defaults */
static void InitOptionDefaults()
{
  ControlMode = 0;
  DisplayMode = DISPLAY_MODE_UNSCALED;
  UpdateFreq = 60;
  Frameskip = 1;
  VSync = 0;
  ClockFreq = 222;
  Quickload = NULL;
  ShowFps = 0;
  RomPath = NULL;
}

/* Gets configuration name */
static const char* GetConfigName()
{
  if (Quickload) return pspFileIoGetFilename(Quickload);
  return NullConfigFile;
}

/* Initialize game configuration */
static void InitGameConfig(struct GameConfig *config)
{
  /* Initialize to default configuration */
  if (config != &DefaultConfig)
    memcpy(config, &DefaultConfig, sizeof(struct GameConfig));
}

/* Load game configuration */
static int LoadGameConfig(const char *filename, struct GameConfig *config)
{
  char *path;
  if (!(path = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(ConfigDir) + strlen(filename) + 6))))
    return 0;
  sprintf(path, "%s%s/%s.cnf", pspGetAppDirectory(), ConfigDir, filename);

  /* If no configuration, load defaults */
  if (!pspFileIoCheckIfExists(path))
  {
    free(path);
    InitGameConfig(config);
    return 1;
  }

  /* Open file for reading */
  FILE *file = fopen(path, "r");
  free(path);
  if (!file) return 0;

  /* Read contents of struct */
  int nread = fread(config, sizeof(struct GameConfig), 1, file);
  fclose(file);

  if (nread != 1)
  {
    InitGameConfig(config);
    return 0;
  }

  return 1;
}

/* Save game configuration */
static int SaveGameConfig(const char *filename, const struct GameConfig *config)
{
  char *path;
  if (!(path = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(ConfigDir) + strlen(filename) + 6))))
    return 0;
  sprintf(path, "%s%s/%s.cnf", pspGetAppDirectory(), ConfigDir, filename);

  /* Open file for writing */
  FILE *file = fopen(path, "w");
  free(path);
  if (!file) return 0;

  /* Write contents of struct */
  int nwritten = fwrite(config, sizeof(struct GameConfig), 1, file);
  fclose(file);

  return (nwritten == 1);
}

/* Clean up any used resources */
void TrashMenu()
{
  /* Save options */
  SaveOptions();

  /* Free memory */
  if (Quickload) free(Quickload);

  /* Trash menus */
  pspMenuDestroy(OptionUiMenu.Menu);
  pspMenuDestroy(SaveStateGallery.Menu);
  pspMenuDestroy(SystemUiMenu.Menu);
  pspMenuDestroy(ControlUiMenu.Menu);
 
  /* Trash images */
  if (Background) pspImageDestroy(Background);
  if (NoSaveIcon) pspImageDestroy(NoSaveIcon);
  if (Screen) pspImageDestroy(Screen);

  if (RomPath) free(RomPath);
  if (GameName) free(GameName);

  free(ScreenshotPath);
  free(SaveStatePath);
}
