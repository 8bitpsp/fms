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

#include "pl_psp.h"
#include "ui.h"
#include "pl_file.h"
#include "video.h"
#include "ctrl.h"
#include "image.h"
#include "pl_ini.h"
#include "pl_util.h"
#include "pl_rewind.h"

#ifdef MINIZIP
#include "unzip.h"
#endif

#include "LibPsp.h"
#include "MenuPsp.h"

#define SYSTEM_SCRNSHOT    1
#define SYSTEM_RESET       2
#define SYSTEM_TIMING      3

#define OPTION_DISPLAY_MODE  1
#define OPTION_FRAME_LIMITER 2
#define OPTION_FRAMESKIP     3
#define OPTION_VSYNC         4
#define OPTION_CLOCK_FREQ    5
#define OPTION_SHOW_FPS      6
#define OPTION_CONTROL_MODE  7
#define OPTION_ANIMATE       8

#define TAB_QUICKLOAD 0
#define TAB_STATE     1
#define TAB_CONTROL   2
#define TAB_OPTION    3
#define TAB_SYSTEM    4
#define TAB_ABOUT     5
#define TAB_MAX       TAB_SYSTEM

extern PspImage *Screen;
extern pl_rewind Rewinder;

int DisplayMode;
int FrameLimiter;
int VSync;
int ClockFreq;
int Frameskip;
int ShowFps;
int RewindSaveRate;
static int ControlsChanged;

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
static const char *OptionsFile = "colem.ini";
static const char *NullConfigFile = "NOCART";

static const char
  *QuickloadFilter[] = { "ROM", "ROM.GZ", "ZIP", '\0' },
  PresentSlotText[] = "\026\244\020 Save\t\026\001\020 Load\t\026\243\020 Delete",
  EmptySlotText[] = "\026\244\020 Save",
  ControlHelpText[] = "\026\250\020 Change mapping\t\026\244\020 Set as default\t\026\243\020 Load defaults";

char ScreenshotPath[1024];
static char SaveStatePath[1024];
static char ConfigPath[1024];

/* Define various menu options */
PL_MENU_OPTIONS_BEGIN(ToggleOptions)
  PL_MENU_OPTION("Disabled", 0)
  PL_MENU_OPTION("Enabled", 1)
PL_MENU_OPTIONS_END
PL_MENU_OPTIONS_BEGIN(ScreenSizeOptions)
  PL_MENU_OPTION("Actual size", DISPLAY_MODE_UNSCALED)
  PL_MENU_OPTION("4:3 scaled (fit height)", DISPLAY_MODE_FIT_HEIGHT)
  PL_MENU_OPTION("16:9 scaled (fit screen)", DISPLAY_MODE_FILL_SCREEN)
PL_MENU_OPTIONS_END
PL_MENU_OPTIONS_BEGIN(PspClockFreqOptions)
  PL_MENU_OPTION("222 MHz", 222)
  PL_MENU_OPTION("266 MHz", 266)
  PL_MENU_OPTION("300 MHz", 300)
  PL_MENU_OPTION("333 MHz", 333)
PL_MENU_OPTIONS_END
PL_MENU_OPTIONS_BEGIN(ControlModeOptions)
  PL_MENU_OPTION("\026\242\020 cancels, \026\241\020 confirms (US)", 0)
  PL_MENU_OPTION("\026\241\020 cancels, \026\242\020 confirms (Japan)", 1)
PL_MENU_OPTIONS_END
PL_MENU_OPTIONS_BEGIN(FrameSkipOptions)
  PL_MENU_OPTION("No skipping",  0)
  PL_MENU_OPTION("Skip 1 frame", 1)
  PL_MENU_OPTION("Skip 2 frame", 2)
  PL_MENU_OPTION("Skip 3 frame", 3)
  PL_MENU_OPTION("Skip 4 frame", 4)
  PL_MENU_OPTION("Skip 5 frame", 5)
PL_MENU_OPTIONS_END
PL_MENU_OPTIONS_BEGIN(TimingOptions)
  PL_MENU_OPTION("NTSC (60 Hz)", CV_NTSC)
  PL_MENU_OPTION("PAL (50 Hz)",  CV_PAL)
PL_MENU_OPTIONS_END
PL_MENU_OPTIONS_BEGIN(ButtonMapOptions)
  /* Unmapped */
  PL_MENU_OPTION("None", 0)
  /* Special */
  PL_MENU_OPTION("Special: Open Menu",     (SPC|SPC_MENU))
  PL_MENU_OPTION("Special: Show keyboard", (SPC|SPC_KYBD))
  PL_MENU_OPTION("Special: Rewind", (SPC|SPC_REWIND))
  /* Joystick */
  PL_MENU_OPTION("Joystick Up",   (JST|JST_UP))
  PL_MENU_OPTION("Joystick Down", (JST|JST_DOWN))
  PL_MENU_OPTION("Joystick Left", (JST|JST_LEFT))
  PL_MENU_OPTION("Joystick Right",(JST|JST_RIGHT))
  PL_MENU_OPTION("Joystick Left Fire",  (JST|JST_FIREL))
  PL_MENU_OPTION("Joystick Right Fire", (JST|JST_FIRER))
  PL_MENU_OPTION("Joystick Purple Fire",(JST|JST_PURPLE))
  PL_MENU_OPTION("Joystick Blue Fire",  (JST|JST_BLUE))
  /* Digits */
  PL_MENU_OPTION("Keypad 0", (JST|JST_0))
  PL_MENU_OPTION("Keypad 1", (JST|JST_1))
  PL_MENU_OPTION("Keypad 2", (JST|JST_2))
  PL_MENU_OPTION("Keypad 3", (JST|JST_3))
  PL_MENU_OPTION("Keypad 4", (JST|JST_4))
  PL_MENU_OPTION("Keypad 5", (JST|JST_5))
  PL_MENU_OPTION("Keypad 6", (JST|JST_6))
  PL_MENU_OPTION("Keypad 7", (JST|JST_7))
  PL_MENU_OPTION("Keypad 8", (JST|JST_8))
  PL_MENU_OPTION("Keypad 9", (JST|JST_9))
  PL_MENU_OPTION("Keypad *", (JST|JST_STAR))
  PL_MENU_OPTION("Keypad #", (JST|JST_POUND)) 
PL_MENU_OPTIONS_END

/* Define menu lists */
PL_MENU_ITEMS_BEGIN(SystemMenuDef)
  PL_MENU_HEADER("Configuration")
  PL_MENU_ITEM("CPU Timing", SYSTEM_TIMING, TimingOptions,
      "\026\250\020 Select video timing mode (PAL/NTSC)")
  PL_MENU_HEADER("System")
  PL_MENU_ITEM("Reset", SYSTEM_RESET, NULL, "\026\001\020 Reset system" )
  PL_MENU_ITEM("Save screenshot",  SYSTEM_SCRNSHOT, NULL,
      "\026\001\020 Save screenshot")
PL_MENU_ITEMS_END
PL_MENU_ITEMS_BEGIN(ControlMenuDef)
  PL_MENU_ITEM(PSP_CHAR_ANALUP, MAP_ANALOG_UP, ButtonMapOptions, 
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_ANALDOWN, MAP_ANALOG_DOWN, ButtonMapOptions,
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_ANALLEFT, MAP_ANALOG_LEFT, ButtonMapOptions,
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_ANALRIGHT, MAP_ANALOG_RIGHT, ButtonMapOptions,
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_UP, MAP_BUTTON_UP, ButtonMapOptions,
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_DOWN, MAP_BUTTON_DOWN, ButtonMapOptions,
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_LEFT, MAP_BUTTON_LEFT, ButtonMapOptions, 
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_RIGHT, MAP_BUTTON_RIGHT, ButtonMapOptions,
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_SQUARE, MAP_BUTTON_SQUARE, ButtonMapOptions,
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_CROSS, MAP_BUTTON_CROSS, ButtonMapOptions, 
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_CIRCLE, MAP_BUTTON_CIRCLE, ButtonMapOptions, 
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_TRIANGLE, MAP_BUTTON_TRIANGLE, ButtonMapOptions,
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_LTRIGGER, MAP_BUTTON_LTRIGGER, ButtonMapOptions, 
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_RTRIGGER, MAP_BUTTON_RTRIGGER, ButtonMapOptions,
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_SELECT, MAP_BUTTON_SELECT, ButtonMapOptions,
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_START, MAP_BUTTON_START, ButtonMapOptions,
      ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_LTRIGGER"+"PSP_CHAR_RTRIGGER, MAP_BUTTON_LRTRIGGERS,
      ButtonMapOptions, ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_START"+"PSP_CHAR_SELECT, MAP_BUTTON_STARTSELECT,
      ButtonMapOptions, ControlHelpText)
PL_MENU_ITEMS_END
PL_MENU_ITEMS_BEGIN(OptionMenuDef)
  PL_MENU_HEADER("Video")
  PL_MENU_ITEM("Screen size", OPTION_DISPLAY_MODE, ScreenSizeOptions, 
    "\026\250\020 Change screen size")
  PL_MENU_HEADER("Performance")
  PL_MENU_ITEM("Frame limiter", OPTION_FRAME_LIMITER, ToggleOptions,
    "\026\250\020 Enable/disable correct FPS emulation")
  PL_MENU_ITEM("Frame skipping", OPTION_FRAMESKIP, FrameSkipOptions,
    "\026\250\020 Change number of frames skipped per update")
  PL_MENU_ITEM("VSync", OPTION_VSYNC, ToggleOptions,
    "\026\250\020 Enable to reduce tearing; disable to increase speed")
  PL_MENU_ITEM("PSP clock frequency", OPTION_CLOCK_FREQ, PspClockFreqOptions, 
    "\026\250\020 Larger values: faster emulation, faster battery depletion (default: 222MHz)")
  PL_MENU_ITEM("Show FPS counter", OPTION_SHOW_FPS, ToggleOptions, 
    "\026\250\020 Show/hide the frames-per-second counter")
  PL_MENU_HEADER("Menu")
  PL_MENU_ITEM("Button mode", OPTION_CONTROL_MODE, ControlModeOptions,
    "\026\250\020 Change OK and Cancel button mapping")
  PL_MENU_ITEM("Animations", OPTION_ANIMATE, ToggleOptions,
    "\026\250\020 Enable/disable menu animations")
PL_MENU_ITEMS_END

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
  "About"
};

static const char* GetConfigName();

static void DisplayStateTab();
static void DisplayControlTab();
PspImage*   LoadStateIcon(const char *path);
int         LoadState(const char *path);
PspImage*   SaveState(const char *path, PspImage *icon);

static void LoadOptions();
static int  SaveOptions();

static int LoadResource(const char *filename);

int  OnGenericCancel(const void *uiobject, const void *param);
void OnGenericRender(const void *uiobject, const void *item_obj);
int  OnGenericButtonPress(const PspUiFileBrowser *browser, const char *path, 
       u32 button_mask);

void OnSystemRender(const void *uiobject, const void *item_obj);

int  OnQuickloadOk(const void *browser, const void *path);

int  OnSaveStateOk(const void *gallery, const void *item);
int  OnSaveStateButtonPress(const PspUiGallery *gallery, pl_menu_item* item, 
       u32 button_mask);

int  OnMenuOk(const void *menu, const void *item);
int  OnMenuItemChanged(const struct PspUiMenu *uimenu, pl_menu_item* item, 
       const pl_menu_option* option);
int  OnMenuButtonPress(const struct PspUiMenu *uimenu, pl_menu_item* item, 
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
  OnGenericRender,             /* OnRender() */
  OnSaveStateOk,               /* OnOk() */
  OnGenericCancel,             /* OnCancel() */
  OnSaveStateButtonPress,      /* OnButtonPress() */
  NULL                         /* Userdata */
};

PspUiMenu OptionUiMenu =
{
  OnGenericRender,       /* OnRender() */
  OnMenuOk,              /* OnOk() */
  OnGenericCancel,       /* OnCancel() */
  OnMenuButtonPress,     /* OnButtonPress() */
  OnMenuItemChanged,     /* OnItemChanged() */
};

PspUiMenu ControlUiMenu =
{
  OnGenericRender,       /* OnRender() */
  OnMenuOk,              /* OnOk() */
  OnGenericCancel,       /* OnCancel() */
  OnMenuButtonPress,     /* OnButtonPress() */
  OnMenuItemChanged,     /* OnItemChanged() */
};

PspUiMenu SystemUiMenu =
{
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

  /* Initialize local vars */
  Quickload = NULL;
  RomPath = NULL;
  GameName = NULL;
  TabIndex = TAB_ABOUT;
  Background = NULL;
  ControlMode = 0;

  /* Initialize paths */
  sprintf(SaveStatePath, "%s%s/", pl_psp_get_app_directory(), SaveStateDir);
  sprintf(ScreenshotPath, "ms0:/PSP/PHOTO/%s/", PSP_APP_NAME);
  sprintf(ConfigPath, "%s%s/", pl_psp_get_app_directory(), ConfigDir);

  if (!pl_file_exists(SaveStatePath))
    pl_file_mkdir_recursive(SaveStatePath);
  if (!pl_file_exists(ConfigPath))
    pl_file_mkdir_recursive(ConfigPath);

  /* Load the background image */
  Background = pspImageLoadPng("background.png");

  /* Init NoSaveState icon image */
  NoSaveIcon=pspImageCreate(136, 100, PSP_IMAGE_16BPP);
  pspImageClear(NoSaveIcon, RGB(0xff,0xff,0xff));

  /* Initialize state menu */
  pl_menu_item *item;
  for (i = 0; i < 10; i++)
  {
    item = pl_menu_append_item(&SaveStateGallery.Menu, i, NULL);
    pl_menu_set_item_help_text(item, EmptySlotText);
  }

  /* Initialize system menu */
  pl_menu_create(&SystemUiMenu.Menu, SystemMenuDef);

  /* Initialize options menu */
  pl_menu_create(&OptionUiMenu.Menu, OptionMenuDef);

  /* Initialize control menu */
  pl_menu_create(&ControlUiMenu.Menu, ControlMenuDef);

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
  UiMetric.TitlePadding = 4;
  UiMetric.TitleColor = PSP_COLOR_YELLOW;
  UiMetric.MenuFps = 30;
  UiMetric.TabBgColor = COLOR(0xb8,0xb8,0xb8,0xff);
  UiMetric.BrowserScreenshotPath = ScreenshotPath;
  UiMetric.BrowserScreenshotDelay = 30;
}

int OnQuickloadOk(const void *browser, const void *path)
{
  /* Load disk or cartridge */
  if (!LoadResource(path))
    return 0;

  pl_rewind_reset(&Rewinder);

  if (GameName) free(GameName);
  GameName = strdup(pl_file_get_filename(path));

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
  if (pl_file_is_of_type(filename, "ZIP"))
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
      if (pl_file_is_of_type(arch_file, "ROM"))
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
  if (pl_file_is_of_type(file_to_load, "ROM") || pl_file_is_of_type(file_to_load, "ROM.GZ"))
  {
    if (!LoadROM(file_to_load))
    {
      free(file_to_load);
      pspUiAlert("Error loading cartridge");
      return 0;
    }

    /* Set path as new cart path */
    free(RomPath);
    RomPath = (char*)malloc(1024);
    pl_file_get_parent_directory(filename, RomPath, 1024);

    if (Quickload) free(Quickload);
    Quickload = strdup(filename);
  }

  /* Clear the screen */
  pspImageClear(Screen, 0x8000);

  /* Reset selected state */
  SaveStateGallery.Menu.selected = NULL;

  free(file_to_load);

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
    PSP_APP_NAME" version "PSP_APP_VER" ("__DATE__")",
    "\026http://psp.akop.org/colem",
    " ",
    "2007-2010 Akop Karapetyan",
    "1994-2007 Marat Fayzullin",
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
  else return 0;

  return 1;
}

void DisplayMenu()
{
  ExitMenu = 0;
  pl_menu_item *item;

  /* Set normal clock frequency */
  pl_psp_set_clock_freq(222);
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
      item = pl_menu_find_item_by_id(&OptionUiMenu.Menu, OPTION_DISPLAY_MODE);
      pl_menu_select_option_by_value(item, (void*)DisplayMode);
      item = pl_menu_find_item_by_id(&OptionUiMenu.Menu, OPTION_FRAME_LIMITER);
      pl_menu_select_option_by_value(item, (void*)FrameLimiter);
      item = pl_menu_find_item_by_id(&OptionUiMenu.Menu, OPTION_FRAMESKIP);
      pl_menu_select_option_by_value(item, (void*)(int)Frameskip);
      item = pl_menu_find_item_by_id(&OptionUiMenu.Menu, OPTION_VSYNC);
      pl_menu_select_option_by_value(item, (void*)VSync);
      item = pl_menu_find_item_by_id(&OptionUiMenu.Menu, OPTION_CLOCK_FREQ);
      pl_menu_select_option_by_value(item, (void*)ClockFreq);
      item = pl_menu_find_item_by_id(&OptionUiMenu.Menu, OPTION_SHOW_FPS);
      pl_menu_select_option_by_value(item, (void*)ShowFps);
      item = pl_menu_find_item_by_id(&OptionUiMenu.Menu, OPTION_CONTROL_MODE);
      pl_menu_select_option_by_value(item, (void*)ControlMode);
      item = pl_menu_find_item_by_id(&OptionUiMenu.Menu, OPTION_ANIMATE);
      pl_menu_select_option_by_value(item, (void*)UiMetric.Animate);
      pspUiOpenMenu(&OptionUiMenu, NULL);
      break;
    case TAB_SYSTEM:
      /* Init system configuration */
      item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_TIMING);
      pl_menu_select_option_by_value(item, (void*)(Mode & CV_PAL));

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
    pl_psp_set_clock_freq(ClockFreq);
    /* Set buttons to normal mode */
    pspCtrlSetPollingMode(PSP_CTRL_NORMAL);
  }
}

/* Handles drawing of generic items */
void OnGenericRender(const void *uiobject, const void *item_obj)
{
  int height = pspFontGetLineHeight(UiMetric.Font);
  int width;

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

int OnMenuItemChanged(const struct PspUiMenu *uimenu, pl_menu_item* item, 
      const pl_menu_option* option)
{
  if (uimenu == &ControlUiMenu)
  {
    ControlsChanged = 1;
    GameConfig.ButtonMap[item->id] = (unsigned int)option->value;
  }
  else if (uimenu == &SystemUiMenu)
  {
    switch(item->id)
    {
    case SYSTEM_TIMING:
      if ((int)option->value != (Mode & CV_PAL))
      {
        if (!pspUiConfirm("This will reset the system. Proceed?"))
          return 0;

        pl_rewind_reset(&Rewinder);
        ResetColeco((Mode&~CV_PAL)|(int)option->value);
      }
      break;
    }
  }
  else if (uimenu == &OptionUiMenu)
  {
    switch(item->id)
    {
    case OPTION_DISPLAY_MODE:
      DisplayMode = (int)option->value;
      break;
    case OPTION_FRAME_LIMITER:
      FrameLimiter = (int)option->value;
      break;
    case OPTION_FRAMESKIP:
      Frameskip = (int)option->value;
      break;
    case OPTION_VSYNC:
      VSync = (int)option->value;
      break;
    case OPTION_CLOCK_FREQ:
      ClockFreq = (int)option->value;
      break;
    case OPTION_SHOW_FPS:
      ShowFps = (int)option->value;
      break;
    case OPTION_CONTROL_MODE:
      ControlMode = (int)option->value;
      UiMetric.OkButton = (!ControlMode) ? PSP_CTRL_CROSS : PSP_CTRL_CIRCLE;
      UiMetric.CancelButton = (!ControlMode) ? PSP_CTRL_CIRCLE : PSP_CTRL_CROSS;
      break;
    case OPTION_ANIMATE:
      UiMetric.Animate = (int)option->value;
      break;
    }
  }

  return 1;
}

int OnMenuOk(const void *uimenu, const void* sel_item)
{
  if (uimenu == &SystemUiMenu)
  {
    switch (((const pl_menu_item*)sel_item)->id)
    {
    case SYSTEM_RESET:

      /* Reset system */
      if (pspUiConfirm("Reset the system?"))
      {
        ExitMenu = 1;
        pl_rewind_reset(&Rewinder);
        ResetColeco(Mode);
        return 1;
      }
      break;

    case SYSTEM_SCRNSHOT:

      /* Save screenshot */
      if (!pl_util_save_image_seq(ScreenshotPath, GetConfigName(), Screen))
        pspUiAlert("ERROR: Screenshot not saved");
      else
        pspUiAlert("Screenshot saved successfully");
      break;
    }
  }

  return 0;
}

int OnMenuButtonPress(const struct PspUiMenu *uimenu, pl_menu_item* sel_item, 
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
      pl_menu_item *item;
      int i;

      /* Load default mapping */
      ControlsChanged = 1;
      memcpy(&GameConfig, &DefaultConfig, sizeof(struct GameConfig));

      /* Modify the menu */
      for (item = ControlUiMenu.Menu.items, i = 0; item; item = item->next, i++)
        pl_menu_select_option_by_value(item, (void*)DefaultConfig.ButtonMap[i]);

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
    ((const pl_menu_item*)item)->id);

  if (pl_file_exists(path) && pspUiConfirm("Load state?"))
  {
    if (LoadState(path))
    {
      ExitMenu = 1;

      pl_rewind_reset(&Rewinder);
      pl_menu_find_item_by_id(&((const PspUiGallery*)gallery)->Menu, 
        ((const pl_menu_item*)item)->id);
      free(path);

      return 1;
    }
    pspUiAlert("ERROR: State failed to load");
  }

  free(path);
  return 0;
}

int OnSaveStateButtonPress(const PspUiGallery *gallery, 
      pl_menu_item *sel, 
      u32 button_mask)
{
  if (button_mask & PSP_CTRL_SQUARE 
    || button_mask & PSP_CTRL_TRIANGLE)
  {
    char *path;
    char caption[32];
    const char *config_name = GetConfigName();
    pl_menu_item *item = pl_menu_find_item_by_id(&gallery->Menu, sel->id);

    path = (char*)malloc(strlen(SaveStatePath) + strlen(config_name) + 8);
    sprintf(path, "%s%s_%02i.sta", SaveStatePath, config_name, item->id);

    do /* not a real loop; flow control construct */
    {
      if (button_mask & PSP_CTRL_SQUARE)
      {
        if (pl_file_exists(path) && !pspUiConfirm("Overwrite existing state?"))
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
        if (item->param && item->param != NoSaveIcon)
          pspImageDestroy((PspImage*)item->param);

        /* Update icon, help text */
        item->param = icon;
        pl_menu_set_item_help_text(item, PresentSlotText);

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

        pl_menu_set_item_caption(item, caption);
      }
      else if (button_mask & PSP_CTRL_TRIANGLE)
      {
        if (!pl_file_exists(path) || !pspUiConfirm("Delete state?"))
          break;

        if (!pl_file_rm(path))
        {
          pspUiAlert("ERROR: State not deleted");
          break;
        }

        /* Trash the old icon (if any) */
        if (item->param && item->param != NoSaveIcon)
          pspImageDestroy((PspImage*)item->param);

        /* Update icon, caption */
        item->param = NoSaveIcon;
        pl_menu_set_item_help_text(item, EmptySlotText);
        pl_menu_set_item_caption(item, "Empty");
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
  w = Screen->Viewport.Width / 2;
  h = Screen->Viewport.Height / 2;
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
  pl_menu_item *item;
  char *game_name = strdup(GetConfigName());
  char *dot = strrchr(game_name, '.');
  if (dot) *dot='\0';
  int i;

  /* Load current button mappings */
  for (item = ControlUiMenu.Menu.items, i = 0; item; item = item->next, i++)
    pl_menu_select_option_by_value(item, (void*)GameConfig.ButtonMap[i]);

  ControlsChanged = 0;
  pspUiOpenMenu(&ControlUiMenu, game_name);

  if (ControlsChanged)
  {
    /* Save to MS */
    if (!SaveGameConfig(GetConfigName(), &GameConfig))
      pspUiAlert("ERROR: Changes not saved");
  }

  free(game_name);
}

static void DisplayStateTab()
{
  pl_menu_item *item, *sel = NULL;
  SceIoStat stat;
  char caption[32];
  const char *config_name = GetConfigName();
  ScePspDateTime latest;

  memset(&latest,0,sizeof(latest));

  if (config_name)
  {
    char *path = (char*)malloc(strlen(SaveStatePath) + strlen(config_name) + 8);
    char *game_name = strdup(config_name);
    char *dot = strrchr(game_name, '.');
    if (dot) *dot='\0';
  
    /* Initialize icons */
    for (item = SaveStateGallery.Menu.items; item; item = item->next)
    {
      sprintf(path, "%s%s_%02i.sta", SaveStatePath, config_name, item->id);
  
      if (pl_file_exists(path))
      {
        if (sceIoGetstat(path, &stat) < 0)
          sprintf(caption, "ERROR");
        else
				{
		      /* Determine the latest save state */
		      if (pl_util_date_compare(&latest, &stat.st_mtime) < 0)
		      {
		        sel = item;
		        latest = stat.st_mtime;
		      }

          sprintf(caption, "%02i/%02i/%02i %02i:%02i", 
            stat.st_mtime.month,
            stat.st_mtime.day,
            stat.st_mtime.year - (stat.st_mtime.year / 100) * 100,
            stat.st_mtime.hour,
            stat.st_mtime.minute);
				}
  
        pl_menu_set_item_caption(item, caption);
        item->param = LoadStateIcon(path);
        pl_menu_set_item_help_text(item, PresentSlotText);
      }
      else
      {
        pl_menu_set_item_caption(item, "Empty");
        item->param = NoSaveIcon;
        pl_menu_set_item_help_text(item, EmptySlotText);
      }
    }

		/* Highlight the latest save state if none are selected */
		if (SaveStateGallery.Menu.selected == NULL)
		  SaveStateGallery.Menu.selected = sel;
  
    free(path);
    pspUiOpenGallery(&SaveStateGallery, game_name);
    free(game_name);
  
    /* Destroy any icons */
    for (item = SaveStateGallery.Menu.items; item; item = item->next)
      if (item->param != NULL && item->param != NoSaveIcon)
        pspImageDestroy((PspImage*)item->param);
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
  char *path = (char*)malloc(sizeof(char) * (strlen(pl_psp_get_app_directory()) + strlen(OptionsFile) + 1));
  sprintf(path, "%s%s", pl_psp_get_app_directory(), OptionsFile);

  /* Initialize INI structure */
  pl_ini_file init;
  pl_ini_load(&init, path);

  /* Load values */
  DisplayMode = pl_ini_get_int(&init, "Video", "Display Mode", 
    DISPLAY_MODE_UNSCALED);
  FrameLimiter = pl_ini_get_int(&init, "Video", "Frame Limiter", 1);
  Frameskip = pl_ini_get_int(&init, "Video", "Frameskip", 0);
  VSync = pl_ini_get_int(&init, "Video", "VSync", 0);
  ClockFreq = pl_ini_get_int(&init, "Video", "PSP Clock Frequency", 222);
  ShowFps = pl_ini_get_int(&init, "Video", "Show FPS", 0);
  ControlMode = pl_ini_get_int(&init, "Menu", "Control Mode", 0);
  UiMetric.Animate = pl_ini_get_int(&init, "Menu", "Animate", 1);
  RewindSaveRate = pl_ini_get_int(&init, "Enhancements", "Rewind Save Rate", 5);

  Mode = (Mode&~CV_PAL) 
    | pl_ini_get_int(&init, "System", "Timing", Mode & CV_PAL);

  if (RomPath) free(RomPath);
  RomPath = (char*)malloc(1024);
  pl_ini_get_string(&init, "File", "ROM Path", pl_psp_get_app_directory(), RomPath, 1024);

  /* Clean up */
  free(path);
  pl_ini_destroy(&init);
}

/* Save options */
static int SaveOptions()
{
  char *path = (char*)malloc(sizeof(char) * (strlen(pl_psp_get_app_directory()) + strlen(OptionsFile) + 1));
  sprintf(path, "%s%s", pl_psp_get_app_directory(), OptionsFile);

  /* Initialize INI structure */
  pl_ini_file init;
  pl_ini_create(&init);

  /* Set values */
  pl_ini_set_int(&init, "Video", "Display Mode", DisplayMode);
  pl_ini_set_int(&init, "Video", "Frame Limiter", FrameLimiter);
  pl_ini_set_int(&init, "Video", "Frameskip", Frameskip);
  pl_ini_set_int(&init, "Video", "VSync", VSync);
  pl_ini_set_int(&init, "Video", "PSP Clock Frequency", ClockFreq);
  pl_ini_set_int(&init, "Video", "Show FPS", ShowFps);
  pl_ini_set_int(&init, "Menu", "Control Mode", ControlMode);
  pl_ini_set_int(&init, "Menu", "Animate", UiMetric.Animate);
  pl_ini_set_int(&init, "Enhancements", "Rewind Save Rate", RewindSaveRate);

  pl_ini_set_int(&init, "System", "Timing", Mode & CV_PAL);

  if (RomPath) pl_ini_set_string(&init, "File", "ROM Path", RomPath);

  /* Save INI file */
  int status = pl_ini_save(&init, path);

  /* Clean up */
  pl_ini_destroy(&init);
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

/* Gets configuration name */
static const char* GetConfigName()
{
  if (Quickload) return pl_file_get_filename(Quickload);
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
  char path[1024];
  sprintf(path, "%s%s.cnf", ConfigPath, filename);

  /* If no configuration, load defaults */
  if (!pl_file_exists(path))
  {
    InitGameConfig(config);
    return 1;
  }

  /* Open file for reading */
  FILE *file = fopen(path, "r");
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
  char path[1024];
  sprintf(path, "%s%s.cnf", ConfigPath, filename);

  /* Open file for writing */
  FILE *file = fopen(path, "w");
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
  pl_menu_destroy(&SystemUiMenu.Menu);
  pl_menu_destroy(&OptionUiMenu.Menu);
  pl_menu_destroy(&ControlUiMenu.Menu);
  pl_menu_destroy(&SaveStateGallery.Menu);

  /* Trash images */
  if (Background) pspImageDestroy(Background);
  if (NoSaveIcon) pspImageDestroy(NoSaveIcon);

  if (RomPath) free(RomPath);
  if (GameName) free(GameName);
}
