/** fMSX: portable MSX emulator ******************************/
/**                                                         **/
/**                         MenuPsp.c                       **/
/**                                                         **/
/** This file contains routines used for menu navigation    **/
/** for the PSP version of fMSX                             **/
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

#include "MSX.h"

#include "psp.h"
#include "ui.h"
#include "fileio.h"
#include "video.h"
#include "ctrl.h"
#include "image.h"
#include "init.h"
#include "util.h"

#ifdef MINIZIP
#include "unzip.h"
#endif

#include "LibPsp.h"
#include "MenuPsp.h"

#define TAB_QUICKLOAD 0
#define TAB_STATE     1
#define TAB_CONTROL   2
#define TAB_OPTION    3
#define TAB_SYSTEM    4
#define TAB_ABOUT     5
#define TAB_MAX       TAB_SYSTEM

#define SYSTEM_CART_A      2
#define SYSTEM_CART_B      3
#define SYSTEM_DRIVE_A     4
#define SYSTEM_DRIVE_B     5
#define SYSTEM_SCRNSHOT    7
#define SYSTEM_RESET       8
#define SYSTEM_CART_A_TYPE 10
#define SYSTEM_CART_B_TYPE 11
#define SYSTEM_TIMING      13
#define SYSTEM_MODEL       14
#define SYSTEM_RAMPAGES    15
#define SYSTEM_VRAMPAGES   16

#define OPTION_DISPLAY_MODE  1
#define OPTION_FRAME_LIMITER 2
#define OPTION_FRAMESKIP     3
#define OPTION_VSYNC         4
#define OPTION_CLOCK_FREQ    5
#define OPTION_SHOW_FPS      6
#define OPTION_CONTROL_MODE  7

extern PspImage *Screen;

int DisplayMode;
int FrameLimiter;
int VSync;
int ClockFreq;
int Frameskip;
int ShowFps;

static PspImage *Background;
static PspImage *NoSaveIcon;

char *ROM[MAXCARTS];
char *Drive[MAXDRIVES];
char *CartPath;
char *DiskPath;
static char *Quickload;
static int TabIndex;
static int ExitMenu;
static int ControlMode;

static const char EmptySlot[] = "[VACANT]";
static const char *CartFilter[] = { "ROM", "ROM.GZ", "ZIP", '\0' };
static const char *DiskFilter[] = { "DSK", "DSK.GZ", "ZIP", '\0' };
static const char *QuickloadFilter[] = { "ROM", "DSK", "ROM.GZ", "DSK.GZ", "ZIP", '\0' };

static const char *ConfigDir = "conf";
static const char *SaveStateDir = "states";
static const char *ScreenshotDir = "screens";
static const char *DefConfigFile = "default";
static const char *BasicConfigFile = "BASIC";
static const char *OptionsFile = "fmsx.ini";

char *ScreenshotPath;
static char *SaveStatePath;

static void DisplayStateTab();
static void DisplayControlTab();

static void InitGameConfig(struct GameConfig *config);
static int  SaveGameConfig(const char *filename, const struct GameConfig *config);
static int  LoadGameConfig(const char *filename, struct GameConfig *config);

static int LoadResource(const char *filename, int slot);
static const char* GetConfigName();
static void LoadOptions();
static int  SaveOptions();

static PspImage* LoadStateIcon(const char *path);
static int LoadState(const char *path);
static PspImage* SaveState(const char *path, PspImage *icon);

// TODO: 
#define WIDTH  272
#define HEIGHT 228

#define CART_TYPE_AUTODETECT 8

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

/* Game configuration (includes button maps) */
struct GameConfig GameConfig;

/* Default configuration */
struct GameConfig DefaultConfig = 
{
  {
    JST|JST_UP,    /* Analog Up    */
    JST|JST_DOWN,  /* Analog Down  */
    JST|JST_LEFT,  /* Analog Left  */
    JST|JST_RIGHT, /* Analog Right */
    KBD|KBD_UP,    /* D-pad Up     */
    KBD|KBD_DOWN,  /* D-pad Down   */
    KBD|KBD_LEFT,  /* D-pad Left   */
    KBD|KBD_RIGHT, /* D-pad Right  */
    JST|JST_FIREA, /* Square       */
    JST|JST_FIREB, /* Cross        */
    KBD|KBD_SPACE, /* Circle       */
    0,             /* Triangle     */
    0,             /* L Trigger    */
    SPC|SPC_KYBD,  /* R Trigger    */
    0,             /* Select       */
    KBD|KBD_F1,    /* Start        */
    SPC|SPC_MENU,  /* L+R Triggers */
    0,             /* Start+Select */
    SPC|SPC_PDISK, /* Select + L   */
    SPC|SPC_NDISK, /* Select + R   */
  }
};

/* Button masks */
const u64 ButtonMask[] = 
{
  PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER, 
  PSP_CTRL_START    | PSP_CTRL_SELECT,
  PSP_CTRL_SELECT   | PSP_CTRL_LTRIGGER,
  PSP_CTRL_SELECT   | PSP_CTRL_RTRIGGER,
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
  MAP_BUTTON_SELECTLTRIG, 
  MAP_BUTTON_SELECTRTRIG,
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

/* Define various menu options */
static const PspMenuOptionDef
  ToggleOptions[] = {
    MENU_OPTION("Disabled", 0),
    MENU_OPTION("Enabled",  1),
    MENU_END_OPTIONS
  },
  ScreenSizeOptions[] = {
    MENU_OPTION("Actual size", DISPLAY_MODE_UNSCALED),
    MENU_OPTION("4:3 scaled (fit height)", DISPLAY_MODE_FIT_HEIGHT),
    MENU_OPTION("16:9 scaled (fit screen)", DISPLAY_MODE_FILL_SCREEN),
    MENU_END_OPTIONS
  },
  FrameSkipOptions[] = {
    MENU_OPTION("No skipping", 0),
    MENU_OPTION("Skip 1 frame", 1),
    MENU_OPTION("Skip 2 frames", 2),
    MENU_OPTION("Skip 3 frames", 3),
    MENU_OPTION("Skip 4 frames", 4),
    MENU_OPTION("Skip 5 frames", 5),
    MENU_END_OPTIONS
  },
  PspClockFreqOptions[] = {
    MENU_OPTION("222 MHz", 222),
    MENU_OPTION("300 MHz", 300),
    MENU_OPTION("333 MHz", 333),
    MENU_END_OPTIONS
  },
  ButtonMapOptions[] = {
    /* Unmapped */
    MENU_OPTION("None", 0),
    /* Special */
    MENU_OPTION("Special: Open Menu",       (SPC|SPC_MENU)),  
    MENU_OPTION("Special: Show keyboard",   (SPC|SPC_KYBD)),
    MENU_OPTION("Special: Previous volume", (SPC|SPC_PDISK)), 
    MENU_OPTION("Special: Next volume",     (SPC|SPC_NDISK)),
    /* Joystick */
    MENU_OPTION("Joystick Up",       (JST|JST_UP)),
    MENU_OPTION("Joystick Down",     (JST|JST_DOWN)),
    MENU_OPTION("Joystick Left",     (JST|JST_LEFT)),
    MENU_OPTION("Joystick Right",    (JST|JST_RIGHT)),
    MENU_OPTION("Joystick Button A", (JST|JST_FIREA)),
    MENU_OPTION("Joystick Button B", (JST|JST_FIREB)),
    /* Directional */
    MENU_OPTION("Keyboard Up",    (KBD|KBD_UP)),
    MENU_OPTION("Keyboard Down",  (KBD|KBD_DOWN)), 
    MENU_OPTION("Keyboard Left",  (KBD|KBD_LEFT)),
    MENU_OPTION("Keyboard Right", (KBD|KBD_RIGHT)),
    /* Etc... */
    MENU_OPTION("Spacebar",  (KBD|KBD_SPACE)),  
    MENU_OPTION("Return",    (KBD|KBD_ENTER)),
    MENU_OPTION("Escape",    (KBD|KBD_ESCAPE)),
    MENU_OPTION("Backspace", (KBD|KBD_BS)),
    MENU_OPTION("Tab",       (KBD|KBD_TAB)),
    MENU_OPTION("Select",    (KBD|KBD_SELECT)),
    MENU_OPTION("Home",      (KBD|KBD_HOME)),
    MENU_OPTION("Delete",    (KBD|KBD_DELETE)),
    MENU_OPTION("Insert",    (KBD|KBD_INSERT)),
    MENU_OPTION("Stop",      (KBD|KBD_STOP)),
    /* Function keys */
    MENU_OPTION("F1", (KBD|KBD_F1)), 
    MENU_OPTION("F2", (KBD|KBD_F2)),
    MENU_OPTION("F3", (KBD|KBD_F3)),
    MENU_OPTION("F4", (KBD|KBD_F4)),
    MENU_OPTION("F5", (KBD|KBD_F5)),
    /* Numbers */
    MENU_OPTION("0", (KBD|0x30)), MENU_OPTION("1", (KBD|0x31)), 
    MENU_OPTION("2", (KBD|0x32)), MENU_OPTION("3", (KBD|0x33)),
    MENU_OPTION("4", (KBD|0x34)), MENU_OPTION("5", (KBD|0x35)),
    MENU_OPTION("6", (KBD|0x36)), MENU_OPTION("7", (KBD|0x37)),
    MENU_OPTION("8", (KBD|0x38)), MENU_OPTION("9", (KBD|0x39)),
    /* Alphabet */
    MENU_OPTION("A", (KBD|0x41)), MENU_OPTION("B", (KBD|0x42)), 
    MENU_OPTION("C", (KBD|0x43)), MENU_OPTION("D", (KBD|0x44)),
    MENU_OPTION("E", (KBD|0x45)), MENU_OPTION("F", (KBD|0x46)),
    MENU_OPTION("G", (KBD|0x47)), MENU_OPTION("H", (KBD|0x48)),
    MENU_OPTION("I", (KBD|0x49)), MENU_OPTION("J", (KBD|0x4A)),
    MENU_OPTION("K", (KBD|0x4B)), MENU_OPTION("L", (KBD|0x4C)),
    MENU_OPTION("M", (KBD|0x4D)), MENU_OPTION("N", (KBD|0x4E)),
    MENU_OPTION("O", (KBD|0x4F)), MENU_OPTION("P", (KBD|0x50)),
    MENU_OPTION("Q", (KBD|0x51)), MENU_OPTION("R", (KBD|0x52)),
    MENU_OPTION("S", (KBD|0x53)), MENU_OPTION("T", (KBD|0x54)),
    MENU_OPTION("U", (KBD|0x55)), MENU_OPTION("V", (KBD|0x56)), 
    MENU_OPTION("W", (KBD|0x57)), MENU_OPTION("X", (KBD|0x58)),
    MENU_OPTION("Y", (KBD|0x59)), MENU_OPTION("Z", (KBD|0x5A)), 
    /* Symbols */
    MENU_OPTION("' (Single quote)",  (KBD|0x27)), 
    MENU_OPTION(", (Comma)",         (KBD|0x2C)),
    MENU_OPTION("- (Minus)",         (KBD|0x2D)),
    MENU_OPTION(". (Period)",        (KBD|0x2E)),
    MENU_OPTION("/ (Forward slash)", (KBD|0x2F)),
    MENU_OPTION("; (Semicolon)",     (KBD|0x3B)),
    MENU_OPTION("= (Equals)",        (KBD|0x3D)),
    MENU_OPTION("[ (Left bracket)",  (KBD|0x5B)),
    MENU_OPTION("] (Right bracket)", (KBD|0x5D)),
    MENU_OPTION("\\ (Backslash)",    (KBD|0x5C)),
    MENU_OPTION("` (Backquote)",     (KBD|0x60)),
    /* Numpad */
    MENU_OPTION("Num. pad 0", (KBD|KBD_NUMPAD0)), 
    MENU_OPTION("Num. pad 1", (KBD|KBD_NUMPAD1)),
    MENU_OPTION("Num. pad 2", (KBD|KBD_NUMPAD2)),
    MENU_OPTION("Num. pad 3", (KBD|KBD_NUMPAD3)),
    MENU_OPTION("Num. pad 4", (KBD|KBD_NUMPAD4)),
    MENU_OPTION("Num. pad 5", (KBD|KBD_NUMPAD5)),
    MENU_OPTION("Num. pad 6", (KBD|KBD_NUMPAD6)),
    MENU_OPTION("Num. pad 7", (KBD|KBD_NUMPAD7)),
    MENU_OPTION("Num. pad 8", (KBD|KBD_NUMPAD8)),
    MENU_OPTION("Num. pad 9", (KBD|KBD_NUMPAD9)),
    /* State keys */
    MENU_OPTION("Shift",       (KBD|KBD_SHIFT)),
    MENU_OPTION("Control",     (KBD|KBD_CONTROL)),
    MENU_OPTION("Graph",       (KBD|KBD_GRAPH)),
    MENU_OPTION("Caps Lock",   (KBD|KBD_CAPSLOCK)),
    MENU_OPTION("Country key", (KBD|KBD_COUNTRY)),
    MENU_END_OPTIONS
  },
  MapperTypeOptions[] = {
    MENU_OPTION("Autodetect",   CART_TYPE_AUTODETECT),
    MENU_OPTION("Generic 8kB",  0),
    MENU_OPTION("Generic 16kB", 1),
    MENU_OPTION("Konami5 8kB",  2),
    MENU_OPTION("Konami4 8kB",  3),
    MENU_OPTION("ASCII 8kB",    4),
    MENU_OPTION("ASCII 16kB",   5),
    MENU_OPTION("GameMaster2",  6),
    MENU_OPTION("FMPAC",        7),
    MENU_END_OPTIONS
  },
  TimingOptions[] = {
    MENU_OPTION("NTSC (60 Hz)", MSX_NTSC),
    MENU_OPTION("PAL (50 Hz)",  MSX_PAL),
    MENU_END_OPTIONS
  },
  ModelOptions[] = {
    MENU_OPTION("MSX1",  MSX_MSX1),
    MENU_OPTION("MSX2",  MSX_MSX2),
    MENU_OPTION("MSX2+", MSX_MSX2P),
    MENU_END_OPTIONS
  },
  RAMOptions[] = {
    MENU_OPTION("Default",0),
    MENU_OPTION("64kB",   4),
    MENU_OPTION("128kB",  8),
  	MENU_OPTION("256kB", 16),
  	MENU_OPTION("512kB", 32),
  	MENU_OPTION("1MB",   64),
  	MENU_OPTION("2MB",  128),
  	MENU_OPTION("4MB",  256),
    MENU_END_OPTIONS
  },
  VRAMOptions[] = {
    MENU_OPTION("Default", 0),
    MENU_OPTION("32kB",  2),
    MENU_OPTION("64kB",  4),
    MENU_OPTION("128kB", 8),
    MENU_END_OPTIONS
  },
  CartNameOptions[] = {
    MENU_OPTION(EmptySlot, NULL),
    MENU_END_OPTIONS
  },
  ControlModeOptions[] = {
    MENU_OPTION("\026\242\020 cancels, \026\241\020 confirms (US)", 0),
    MENU_OPTION("\026\241\020 cancels, \026\242\020 confirms (Japan)", 1),
    MENU_END_OPTIONS
  };

static const char 
  ControlHelpText[] = "\026\250\020 Change mapping\t\026\001\020 Save to \271\t\026\244\020 Set as default\t\026\243\020 Load defaults",
  LoadedDeviceText[] = "\026\001\020 Browse\t\026\243\020 Eject",
  EmptyDeviceText[] = "\026\001\020 Browse",
  PresentSlotText[] = "\026\244\020 Save\t\026\001\020 Load\t\026\243\020 Delete",
  EmptySlotText[] = "\026\244\020 Save";

/* Define menu lists */
static const PspMenuItemDef 
  SystemMenuDef[] = {
    MENU_HEADER("Cartridges"),
    MENU_ITEM("Slot A", SYSTEM_CART_A, CartNameOptions, 0, EmptyDeviceText),
    MENU_ITEM("Type", SYSTEM_CART_A_TYPE, MapperTypeOptions, 0, 
      "\026\250\020 Select cartridge type"),
    MENU_ITEM("Slot B", SYSTEM_CART_B, CartNameOptions, 0, EmptyDeviceText),
    MENU_ITEM("Type", SYSTEM_CART_B_TYPE, MapperTypeOptions, 0, 
      "\026\250\020 Select cartridge type"),
    MENU_HEADER("Drives"),
    MENU_ITEM("Drive A", SYSTEM_DRIVE_A, CartNameOptions, 0, EmptyDeviceText),
    MENU_ITEM("Drive B", SYSTEM_DRIVE_B, CartNameOptions, 0, EmptyDeviceText),
    MENU_HEADER("Configuration"),
    MENU_ITEM("System", SYSTEM_MODEL,ModelOptions, -1, 
      "\026\250\020 Select MSX model"),
    MENU_ITEM("CPU Timing", SYSTEM_TIMING, TimingOptions, -1,
      "\026\250\020 Select video timing mode (PAL/NTSC)"),
    MENU_ITEM("System RAM", SYSTEM_RAMPAGES, RAMOptions, -1, 
      "\026\250\020 Change amount of system memory"),
    MENU_ITEM("System VRAM", SYSTEM_VRAMPAGES, VRAMOptions, -1, 
      "\026\250\020 Change amount of video memory"),
    MENU_HEADER("System"),
    MENU_ITEM("Reset", SYSTEM_RESET, NULL, -1, "\026\001\020 Reset MSX" ),
    MENU_ITEM("Save screenshot",  SYSTEM_SCRNSHOT, NULL,  -1, 
      "\026\001\020 Save screenshot"),
    MENU_END_ITEMS
  },
  ControlMenuDef[] = {
    MENU_ITEM(PSP_CHAR_ANALUP, MAP_ANALOG_UP, ButtonMapOptions, -1, 
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_ANALDOWN, MAP_ANALOG_DOWN, ButtonMapOptions, -1,
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_ANALLEFT, MAP_ANALOG_LEFT, ButtonMapOptions, -1,
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_ANALRIGHT, MAP_ANALOG_RIGHT, ButtonMapOptions, -1, 
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_UP, MAP_BUTTON_UP, ButtonMapOptions, -1,
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_DOWN, MAP_BUTTON_DOWN, ButtonMapOptions, -1,
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_LEFT, MAP_BUTTON_LEFT, ButtonMapOptions, -1, 
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_RIGHT, MAP_BUTTON_RIGHT, ButtonMapOptions, -1, 
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_SQUARE, MAP_BUTTON_SQUARE, ButtonMapOptions, -1,
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_CROSS, MAP_BUTTON_CROSS, ButtonMapOptions, -1, 
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_CIRCLE, MAP_BUTTON_CIRCLE, ButtonMapOptions, -1, 
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_TRIANGLE, MAP_BUTTON_TRIANGLE, ButtonMapOptions, -1,
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_LTRIGGER, MAP_BUTTON_LTRIGGER, ButtonMapOptions, -1, 
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_RTRIGGER, MAP_BUTTON_RTRIGGER, ButtonMapOptions, -1, 
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_SELECT, MAP_BUTTON_SELECT, ButtonMapOptions, -1,
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_START, MAP_BUTTON_START, ButtonMapOptions, -1,
      ControlHelpText),
    MENU_ITEM(PSP_CHAR_LTRIGGER"+"PSP_CHAR_RTRIGGER, MAP_BUTTON_LRTRIGGERS,
      ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_START"+"PSP_CHAR_SELECT, MAP_BUTTON_STARTSELECT,
      ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_SELECT"+"PSP_CHAR_LTRIGGER, MAP_BUTTON_SELECTLTRIG, 
      ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_SELECT"+"PSP_CHAR_RTRIGGER, MAP_BUTTON_SELECTRTRIG, 
      ButtonMapOptions, -1, ControlHelpText),
    MENU_END_ITEMS
  },
  OptionMenuDef[] = {
    MENU_HEADER("Video"),
    MENU_ITEM("Screen size", OPTION_DISPLAY_MODE, ScreenSizeOptions, -1, 
      "\026\250\020 Change screen size"),
    MENU_HEADER("Performance"),
    MENU_ITEM("Frame limiter", OPTION_FRAME_LIMITER, ToggleOptions, -1,
      "\026\250\020 Enable/disable correct FPS emulation"),
    MENU_ITEM("Frame skipping", OPTION_FRAMESKIP, FrameSkipOptions, -1,
      "\026\250\020 Change number of frames skipped per update"),
    MENU_ITEM("VSync", OPTION_VSYNC, ToggleOptions, -1,
      "\026\250\020 Enable to reduce tearing; disable to increase speed"),
    MENU_ITEM("PSP clock frequency", OPTION_CLOCK_FREQ, PspClockFreqOptions, 
      -1, "\026\250\020 Larger values: faster emulation, faster battery depletion (default: 222MHz)"),
    MENU_ITEM("Show FPS counter", OPTION_SHOW_FPS, ToggleOptions, -1, 
      "\026\250\020 Show/hide the frames-per-second counter"),
    MENU_HEADER("Menu"),
    MENU_ITEM("Button mode", OPTION_CONTROL_MODE, ControlModeOptions, -1,
      "\026\250\020 Change OK and Cancel button mapping"),
    MENU_END_ITEMS
  };

int OnGenericButtonPress(const PspUiFileBrowser *browser, const char *path, 
      u32 button_mask);
int OnSaveStateButtonPress(const PspUiGallery *gallery, PspMenuItem* item, 
       u32 button_mask);
int OnMenuButtonPress(const struct PspUiMenu *uimenu, PspMenuItem* item, 
      u32 button_mask);
int OnSplashButtonPress(const struct PspUiSplash *splash, u32 button_mask);

int OnGenericCancel(const void *uiobject, const void *param);

int OnFileOk(const void *browser, const void *path);
int OnQuickloadOk(const void *browser, const void *path);
int OnSaveStateOk(const void *gallery, const void *item);
int OnMenuOk(const void *menu, const void *item);

void OnGenericRender(const void *uiobject, const void *item_obj);
void OnSystemRender(const void *uiobject, const void *item_obj);
void OnSplashRender(const void *uiobject, const void *null);

int OnMenuItemChanged(const struct PspUiMenu *uimenu, PspMenuItem* item, 
      const PspMenuOption* option);

const char* OnSplashGetStatusBarText(const struct PspUiSplash *splash);

PspUiFileBrowser QuickloadBrowser = 
{
  OnGenericRender,
  OnQuickloadOk,
  OnGenericCancel,
  OnGenericButtonPress,
  QuickloadFilter,
  0
};

PspUiFileBrowser FileBrowser = 
{
  OnGenericRender,
  OnFileOk,
  NULL,
  NULL,
  NULL,
  NULL
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

  /* Reset variables */
  for (i = 0; i < MAXCARTS; i++) ROM[i] = NULL;
  for (i = 0; i < MAXDRIVES; i++) Drive[i] = NULL;
  Quickload = NULL;
  TabIndex = TAB_ABOUT;
  Background = NULL;
  ControlMode = 0;
  CartPath = NULL;
  DiskPath = NULL;

  PspMenuItem *item;

  /* Initialize paths */
  SaveStatePath 
    = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(SaveStateDir) + 2));
  sprintf(SaveStatePath, "%s%s/", pspGetAppDirectory(), SaveStateDir);
  ScreenshotPath 
    = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(ScreenshotDir) + 2));
  sprintf(ScreenshotPath, "%s%s/", pspGetAppDirectory(), ScreenshotDir);

  /* Initialize system menu */
  SystemUiMenu.Menu = pspMenuCreate();
  pspMenuLoad(SystemUiMenu.Menu, SystemMenuDef);

  /* Initialize options menu */
  OptionUiMenu.Menu = pspMenuCreate();
  pspMenuLoad(OptionUiMenu.Menu, OptionMenuDef);

  /* Initialize control menu */
  ControlUiMenu.Menu = pspMenuCreate();
  pspMenuLoad(ControlUiMenu.Menu, ControlMenuDef);

  /* Load the background image */
  Background = pspImageLoadPng("background.png");

  /* Init NoSaveState icon image */
  NoSaveIcon=pspImageCreate(136, 114, PSP_IMAGE_16BPP);
  pspImageClear(NoSaveIcon, RGB(66,66,66));

  /* Initialize state menu */
  SaveStateGallery.Menu = pspMenuCreate();
  for (i = 0; i < 10; i++)
  {
    item = pspMenuAppendItem(SaveStateGallery.Menu, NULL, i);
    pspMenuSetHelpText(item, EmptySlotText);
  }

  /* Load default configuration */
  LoadGameConfig(DefConfigFile, &DefaultConfig);

  /* Initialize configuration */
  InitGameConfig(&GameConfig);

  /* Initialize options */
  LoadOptions();

  /* Initialize UI components */
  UiMetric.Background = Background;
  UiMetric.Font = &PspStockFont;
  UiMetric.Left = 8;
  UiMetric.Top = 24;
  UiMetric.Right = 472;
  UiMetric.Bottom = 240;
  UiMetric.OkButton = (!ControlMode) ? PSP_CTRL_CROSS : PSP_CTRL_CIRCLE;
  UiMetric.CancelButton = (!ControlMode) ? PSP_CTRL_CIRCLE : PSP_CTRL_CROSS;
  UiMetric.ScrollbarColor = PSP_COLOR_GRAY;
  UiMetric.ScrollbarBgColor = 0x44ffffff;
  UiMetric.ScrollbarWidth = 10;
  UiMetric.DialogBorderColor = PSP_COLOR_GRAY;
  UiMetric.DialogBgColor = PSP_COLOR_DARKGRAY;
  UiMetric.TextColor = PSP_COLOR_GRAY;
  UiMetric.SelectedColor = PSP_COLOR_GREEN;
  UiMetric.SelectedBgColor = COLOR(0,0,0,0x55);
  UiMetric.StatusBarColor = PSP_COLOR_WHITE;
  UiMetric.BrowserFileColor = PSP_COLOR_GRAY;
  UiMetric.BrowserDirectoryColor = PSP_COLOR_YELLOW;
  UiMetric.GalleryIconsPerRow = 5;
  UiMetric.GalleryIconMarginWidth = 8;
  UiMetric.MenuItemMargin = 20;
  UiMetric.MenuSelOptionBg = PSP_COLOR_BLACK;
  UiMetric.MenuOptionBoxColor = PSP_COLOR_GRAY;
  UiMetric.MenuOptionBoxBg = COLOR(0, 0, 0, 0xBB);
  UiMetric.MenuDecorColor = PSP_COLOR_YELLOW;
  UiMetric.DialogFogColor = COLOR(0, 0, 0, 88);
  UiMetric.TitlePadding = 4;
  UiMetric.TitleColor = PSP_COLOR_WHITE;
  UiMetric.MenuFps = 30;
  UiMetric.TabBgColor = 0xff959595;
}

int OnQuickloadOk(const void *browser, const void *path)
{
  int i;

  /* Trash any ROM or disk selections */
  for (i = 0; i < MAXCARTS; i++)  if (ROM[i]) { free(ROM[i]); ROM[i] = NULL; }
  for (i = 0; i < MAXDRIVES; i++) if (Drive[i]) { free(Drive[i]); Drive[i] = NULL; }

  /* Eject all cartridges and disks */
  for (i = 0; i < MAXCARTS; i++) LoadCart(NULL, i, 0);
  for (i = 0; i < MAXDRIVES; i++) ChangeDisk(i, NULL);

  /* Reinitialize menu */
  int userdata[] = { SYSTEM_CART_A, SYSTEM_CART_B, SYSTEM_DRIVE_A, SYSTEM_DRIVE_B, 0 };
  PspMenuItem *item;
  for (i = 0; userdata[i]; i++)
  {
    item = pspMenuFindItemById(SystemUiMenu.Menu, userdata[i]);
    pspMenuModifyOption(item->Options, EmptySlot, NULL);
  }

  if (Quickload) free(Quickload);
  Quickload = strdup(path);

  /* Load disk or cartridge */
  if (!LoadResource(path, 0))
    return 0;

  /* Load game configuration */
  if (!LoadGameConfig(GetConfigName(), &GameConfig))
    ;//pspUiAlert("ERROR: Configuration not loaded");

  ExitMenu = 1;

  /* Restart MSX */
  ResetMSX(Mode,RAMPages,VRAMPages);
  return 1;
}

int OnGenericCancel(const void *uiobject, const void* param)
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
    "\026http://psp.akop.org/fmsx",
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
    pspVideoPrint(UiMetric.Font, x, y, lines[i], PSP_COLOR_GRAY);
  }

  /* Render PSP status */
  OnGenericRender(splash, null);
}

int OnSplashButtonPress(const struct PspUiSplash *splash, 
  u32 button_mask)
{
  return OnGenericButtonPress(NULL, NULL, button_mask);
}

int OnMenuItemChanged(const struct PspUiMenu *uimenu, 
  PspMenuItem* item, 
  const PspMenuOption* option)
{
  if (uimenu == &OptionUiMenu)
  {
    switch((int)item->ID)
    {
    case OPTION_DISPLAY_MODE:
      DisplayMode = (int)option->Value;
      break;
    case OPTION_FRAME_LIMITER:
      FrameLimiter = (int)option->Value;
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
  else if (uimenu == &ControlUiMenu)
  {
    GameConfig.ButtonMap[item->ID] = (unsigned int)option->Value;
  }
  else if (uimenu == &SystemUiMenu)
  {
    switch(item->ID)
    {
    case SYSTEM_CART_A_TYPE:
    case SYSTEM_CART_B_TYPE:

      if (!pspUiConfirm("This will reset the system. Proceed?"))
        return 0;

      SETROMTYPE(item->ID - SYSTEM_CART_A_TYPE, (int)option->Value);
      ResetMSX(Mode,RAMPages,VRAMPages);
      break;

    case SYSTEM_TIMING:
      if ((int)option->Value != (Mode & MSX_VIDEO))
      {
        if (!pspUiConfirm("This will reset the system. Proceed?"))
          return 0;

        ResetMSX((Mode&~MSX_VIDEO)|(int)option->Value,RAMPages,VRAMPages);
      }
      break;
    case SYSTEM_MODEL:
      if ((int)option->Value != (Mode & MSX_MODEL))
      {
        if (!pspUiConfirm("This will reset the system. Proceed?"))
          return 0;

        ResetMSX((Mode&~MSX_MODEL)|(int)option->Value,RAMPages,VRAMPages);

        PspMenuItem *ram_item, *vram_item;
        ram_item = pspMenuFindItemById(SystemUiMenu.Menu, SYSTEM_RAMPAGES);
        vram_item = pspMenuFindItemById(SystemUiMenu.Menu, SYSTEM_VRAMPAGES);

        if ((int)ram_item->Selected->Value != RAMPages 
          || (int)vram_item->Selected->Value != VRAMPages)
        {
          pspUiAlert("Memory settings have been modified to fit the system");

    		  pspMenuSelectOptionByValue(ram_item, (void*)RAMPages);
    		  pspMenuSelectOptionByValue(vram_item, (void*)VRAMPages);
    		}
      }
      break;
    case SYSTEM_RAMPAGES:
      if ((int)option->Value != RAMPages)
      {
        if (!pspUiConfirm("This will reset the system. Proceed?"))
          return 0;

        ResetMSX(Mode,(int)option->Value,VRAMPages);

        /* See if fMSX readjusted memory size for validity */
        if ((int)option->Value != RAMPages)
    		{
    		  if (option->Value)
    		    pspUiAlert("Your selection has been modified to fit the system");

    		  pspMenuSelectOptionByValue(item, (void*)RAMPages);
          return 0;
    		}
      }
      break;
    case SYSTEM_VRAMPAGES:
      if ((int)option->Value != VRAMPages)
      {
        if (!pspUiConfirm("This will reset the system. Proceed?"))
          return 0;

        ResetMSX(Mode,RAMPages,(int)option->Value);

        /* See if fMSX readjusted memory size for validity */
        if ((int)option->Value != VRAMPages)
    		{
    		  if (option->Value)
    		    pspUiAlert("Your selection has been modified to fit the system");

    		  pspMenuSelectOptionByValue(item, (void*)VRAMPages);
          return 0;
    		}
      }
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
    int opt, slot;
    opt = ((const PspMenuItem*)sel_item)->ID;

    switch (opt)
    {
    case SYSTEM_CART_A:
    case SYSTEM_CART_B:

      /* Load cartridge */
      slot = opt - SYSTEM_CART_A;
      FileBrowser.Userdata = (void*)slot;
      FileBrowser.Filter = CartFilter;
      pspUiOpenBrowser(&FileBrowser, CartPath);
      break;

    case SYSTEM_DRIVE_A:
    case SYSTEM_DRIVE_B:

      slot = opt - SYSTEM_DRIVE_A;

      if (Drive[slot])
      {
        /* Test to see if it's a real file or archived name */
        FILE *file = fopen(Drive[slot], "r");
        fclose(file);

        /* See if the user wants to save changes to disk */
        if (file)
        {
	        switch (pspUiYesNoCancel("Save changes to the currently loaded disk?"))
	        {
	        case PSP_UI_YES: 
	          pspUiFlashMessage("Saving disk image, please wait...");
	          SaveFDI(&FDD[slot], Drive[slot], FMT_AUTO);
	          break;
	        case PSP_UI_NO:
	          break;
	        case PSP_UI_CANCEL:
	        default:
	          goto cancel_load;
	        }
        }
      }

      /* Open browser window */
      FileBrowser.Userdata = (void*)slot;
      FileBrowser.Filter = DiskFilter;
      pspUiOpenBrowser(&FileBrowser, DiskPath);
cancel_load:
      break;

    case SYSTEM_RESET:

      /* Reset system */
      if (pspUiConfirm("Reset the system?"))
      {
        ExitMenu = 1;
        ResetMSX(Mode,RAMPages,VRAMPages);
        return 1;
      }
      break;

    case SYSTEM_SCRNSHOT:

      /* Save screenshot */
      if (!pspUtilSavePngSeq(ScreenshotPath, GetConfigName(), Screen))
        pspUiAlert("ERROR: Screenshot not saved");
      else
        pspUiAlert("Screenshot saved successfully");
      break;
    }
  }

  return 0;
}

int  OnMenuButtonPress(const struct PspUiMenu *uimenu, 
  PspMenuItem* sel_item, 
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
  else if (uimenu == &SystemUiMenu)
  {
    int opt, slot;
    PspMenuItem *item;

    if (button_mask & PSP_CTRL_TRIANGLE)
    {
      opt = sel_item->ID;

      switch (opt)
      {
      case SYSTEM_CART_A:
      case SYSTEM_CART_B:

        /* Eject cartridge */
        slot = opt - SYSTEM_CART_A;
        if (ROM[slot]) { free(ROM[slot]); ROM[slot] = NULL; }
        pspMenuModifyOption(sel_item->Options, EmptySlot, NULL);
        pspMenuSetHelpText(sel_item, EmptyDeviceText);
        LoadCart(NULL, slot, 0);

        /* Reset cartridge type */
        item = pspMenuFindItemById(SystemUiMenu.Menu, SYSTEM_CART_A_TYPE + slot);
        pspMenuSelectOptionByValue(item, (void*)CART_TYPE_AUTODETECT);

        /* Load game configuration */
        if (!LoadGameConfig(GetConfigName(), &GameConfig))
          ; //pspUiAlert("ERROR: Configuration not loaded");

        break;

      case SYSTEM_DRIVE_A:
      case SYSTEM_DRIVE_B:

        slot = opt - SYSTEM_DRIVE_A;
        if (!Drive[slot]) 
          break;
        else
        {
          /* Test to see if it's a real file or archived name */
          FILE *file = fopen(Drive[slot], "r");
          fclose(file);

          /* See if the user wants to save changes to disk */
          if (file)
          {
		        switch (pspUiYesNoCancel("Save changes to disk before ejecting?"))
		        {
		        case PSP_UI_YES: 
	            pspUiFlashMessage("Saving disk image, please wait...");
		          SaveFDI(&FDD[slot], Drive[slot], FMT_AUTO);
		          break;
		        case PSP_UI_NO:
		          break;
		        case PSP_UI_CANCEL:
		        default:
		          goto cancel_eject;
		        }
          }
        }

        /* Eject disk */
        free(Drive[slot]); 
        Drive[slot] = NULL;
        pspMenuModifyOption(sel_item->Options, EmptySlot, NULL);
        pspMenuSetHelpText(sel_item, EmptyDeviceText);
        ChangeDisk(slot, NULL);

        /* Load game configuration */
        if (!LoadGameConfig(GetConfigName(), &GameConfig))
          ; //pspUiAlert("ERROR: Configuration not loaded");

cancel_eject:
        break;
      }

      return 0;
    }
  }

  return OnGenericButtonPress(NULL, NULL, button_mask);
}

const char* OnSplashGetStatusBarText(const struct PspUiSplash *splash)
{
  return "\026\255\020/\026\256\020 Switch tabs";
}

int OnGenericButtonPress(const PspUiFileBrowser *browser, 
  const char *path, u32 button_mask)
{
  /* If L or R are pressed, switch tabs */
  if (button_mask & PSP_CTRL_LTRIGGER)
  { if (--TabIndex < 0) TabIndex=TAB_MAX; }
  else if (button_mask & PSP_CTRL_RTRIGGER)
  { if (++TabIndex > TAB_MAX) TabIndex=0; }
  else if ((button_mask & (PSP_CTRL_START | PSP_CTRL_SELECT)) 
    == (PSP_CTRL_START | PSP_CTRL_SELECT))
  {
    if (pspUtilSaveVramSeq(ScreenshotPath, "UI"))
      pspUiAlert("Saved successfully");
    else
      pspUiAlert("ERROR: Not saved");
    return 0;
  }
  else return 0;

  return 1;
}

/* Load options */
static void LoadOptions()
{
  char *path = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(OptionsFile) + 1));
  sprintf(path, "%s%s", pspGetAppDirectory(), OptionsFile);

  /* Initialize INI structure */
  PspInit *init = pspInitCreate();

  /* Read the file */
  pspInitLoad(init, path);

  /* Load values */
  DisplayMode = pspInitGetInt(init, "Video", "Display Mode", 
    DISPLAY_MODE_UNSCALED);
  FrameLimiter = pspInitGetInt(init, "Video", "Frame Limiter", 1);
  Frameskip = pspInitGetInt(init, "Video", "Frameskip", 0);
  VSync = pspInitGetInt(init, "Video", "VSync", 0);
  ClockFreq = pspInitGetInt(init, "Video", "PSP Clock Frequency", 222);
  ShowFps = pspInitGetInt(init, "Video", "Show FPS", 0);
  ControlMode = pspInitGetInt(init, "Menu", "Control Mode", 0);

  Mode = (Mode&~MSX_VIDEO) 
    | pspInitGetInt(init, "System", "Timing", Mode & MSX_VIDEO);
  Mode = (Mode&~MSX_MODEL) 
    | pspInitGetInt(init, "System", "Model", Mode & MSX_MODEL);
  RAMPages = pspInitGetInt(init, "System", "RAM Pages", RAMPages);
  VRAMPages = pspInitGetInt(init, "System", "VRAM Pages", VRAMPages);

  if (DiskPath) free(DiskPath);
  if (CartPath) free(CartPath); 
  if (Quickload) free(Quickload);

  DiskPath = pspInitGetString(init, "File", "Disk Path", NULL);
  CartPath = pspInitGetString(init, "File", "Cart Path", NULL);
  Quickload = pspInitGetString(init, "File", "Game Path", NULL);

  /* Clean up */
  free(path);
  pspInitDestroy(init);
}

/* Save options */
static int SaveOptions()
{
  char *path = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(OptionsFile) + 1));
  sprintf(path, "%s%s", pspGetAppDirectory(), OptionsFile);

  /* Initialize INI structure */
  PspInit *init = pspInitCreate();

  /* Set values */
  pspInitSetInt(init, "Video", "Display Mode", DisplayMode);
  pspInitSetInt(init, "Video", "Frame Limiter", FrameLimiter);
  pspInitSetInt(init, "Video", "Frameskip", Frameskip);
  pspInitSetInt(init, "Video", "VSync", VSync);
  pspInitSetInt(init, "Video", "PSP Clock Frequency", ClockFreq);
  pspInitSetInt(init, "Video", "Show FPS", ShowFps);
  pspInitSetInt(init, "Menu", "Control Mode", ControlMode);

  pspInitSetInt(init, "System", "Timing", Mode & MSX_VIDEO);
  pspInitSetInt(init, "System", "Model", Mode & MSX_MODEL);
  pspInitSetInt(init, "System", "RAM Pages", RAMPages);
  pspInitSetInt(init, "System", "VRAM Pages", VRAMPages);

  if (Quickload)
  {
    char *qlpath = pspFileIoGetParentDirectory(Quickload);
    pspInitSetString(init, "File", "Game Path", qlpath);
    free(qlpath);
  }

  if (DiskPath) pspInitSetString(init, "File", "Disk Path", DiskPath);
  if (CartPath) pspInitSetString(init, "File", "Cart Path", CartPath);

  /* Save INI file */
  int status = pspInitSave(init, path);

  /* Clean up */
  pspInitDestroy(init);
  free(path);

  return status;
}

void DisplayMenu()
{
  PspMenuItem *item;
  ExitMenu = 0;

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
      pspUiOpenBrowser(&QuickloadBrowser, Quickload);
      break;
    case TAB_STATE:
      DisplayStateTab();
      break;
    case TAB_SYSTEM:
      /* Init system configuration */
      item = pspMenuFindItemById(SystemUiMenu.Menu, SYSTEM_TIMING);
      pspMenuSelectOptionByValue(item, (void*)(Mode & MSX_VIDEO));
      item = pspMenuFindItemById(SystemUiMenu.Menu, SYSTEM_MODEL);
      pspMenuSelectOptionByValue(item, (void*)(Mode & MSX_MODEL));
      item = pspMenuFindItemById(SystemUiMenu.Menu, SYSTEM_RAMPAGES);
      pspMenuSelectOptionByValue(item, (void*)RAMPages);
      item = pspMenuFindItemById(SystemUiMenu.Menu, SYSTEM_VRAMPAGES);
      pspMenuSelectOptionByValue(item, (void*)VRAMPages);

      pspUiOpenMenu(&SystemUiMenu, NULL);
      break;
    case TAB_CONTROL:
      DisplayControlTab();
      break;
    case TAB_OPTION:
      /* Init menu options */
      item = pspMenuFindItemById(OptionUiMenu.Menu, OPTION_DISPLAY_MODE);
      pspMenuSelectOptionByValue(item, (void*)DisplayMode);
      item = pspMenuFindItemById(OptionUiMenu.Menu, OPTION_FRAME_LIMITER);
      pspMenuSelectOptionByValue(item, (void*)FrameLimiter);
      item = pspMenuFindItemById(OptionUiMenu.Menu, OPTION_FRAMESKIP);
      pspMenuSelectOptionByValue(item, (void*)(int)Frameskip);
      item = pspMenuFindItemById(OptionUiMenu.Menu, OPTION_VSYNC);
      pspMenuSelectOptionByValue(item, (void*)VSync);
      item = pspMenuFindItemById(OptionUiMenu.Menu, OPTION_CLOCK_FREQ);
      pspMenuSelectOptionByValue(item, (void*)ClockFreq);
      item = pspMenuFindItemById(OptionUiMenu.Menu, OPTION_SHOW_FPS);
      pspMenuSelectOptionByValue(item, (void*)ShowFps);
      item = pspMenuFindItemById(OptionUiMenu.Menu, OPTION_CONTROL_MODE);
      pspMenuSelectOptionByValue(item, (void*)ControlMode);

      pspUiOpenMenu(&OptionUiMenu, NULL);
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

/* Gets configuration name */
static const char* GetConfigName()
{
  if (ROM[0]) return pspFileIoGetFilename(ROM[0]);
  if (Drive[0]) return pspFileIoGetFilename(Drive[0]);

  return BasicConfigFile;
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
  for (i = 0, x = 5; i <= TAB_MAX; i++, x += width + 10)
  {
    /* Determine width of text */
    width = pspFontGetTextWidth(UiMetric.Font, TabLabel[i]);

    if (i == TabIndex || !(uiobject == &FileBrowser))
    {
      /* Draw background of active tab */
      if (i == TabIndex) 
        pspVideoFillRect(x - 5, 0, x + width + 5, height + 1, 
          UiMetric.TabBgColor);

      /* Draw name of tab */
      pspVideoPrint(UiMetric.Font, x, 0, TabLabel[i], PSP_COLOR_WHITE);
    }
  }
}

/* Handles any special drawing for the system menu */
void OnSystemRender(const void *uiobject, const void *item_obj)
{
  int w, h, x, y;
  w = Screen->Viewport.Width >> 1;
  h = Screen->Viewport.Height >> 1;
  x = SCR_WIDTH - w - 8;
  y = SCR_HEIGHT - h - 56;

  /* Draw a small representation of the screen */
  pspVideoShadowRect(x, y, x + w - 1, y + h - 1, PSP_COLOR_BLACK, 3);
  pspVideoPutImage(Screen, x, y, w, h);
  pspVideoDrawRect(x, y, x + w - 1, y + h - 1, PSP_COLOR_GRAY);

  OnGenericRender(uiobject, item_obj);
}

int OnFileOk(const void *browser, const void *path)
{
  int slot = (int)((const PspUiFileBrowser*)browser)->Userdata;

  if (!LoadResource(path, slot))
    return 0;

  /* Load game configuration */
  if (!LoadGameConfig(GetConfigName(), &GameConfig))
    ;//pspUiAlert("ERROR: Configuration not loaded");

  return 1;
}

/* Loads a cartridge or disk image into specified slot */
static int LoadResource(const char *filename, int slot)
{
  PspMenuItem *item = NULL;
  char *file_to_load = NULL;

#ifdef MINIZIP
  /* Check if it's a zip file */
  if (pspFileIoEndsWith(filename, "ZIP"))
  {
    unzFile zip;
    unz_file_info fi;
    unz_global_info gi;
    char arch_file[255], fmsx_file[255];
    fmsx_file[0] = '\0';

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

    int min_dsk = 11, dsk, i;

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
        strcpy(fmsx_file, arch_file);
        break;
      }
      else if (pspFileIoEndsWith(arch_file, "DSK"))
      {
        /* Check the pre-extension char */
        char digit = arch_file[strlen(arch_file) - 5];

        if (digit >= '0' && digit <= '9')
        {
          /* Find the file with the lowest sequential number */
          /* Treat zero as #10 */
          dsk = (digit == 0) ? 10 : (int)digit - '0';

          if (dsk < min_dsk)
          {
            min_dsk = dsk;
            strcpy(fmsx_file, arch_file);
          }
        }
        else
        {
          /* Non-sequential DSK - load it immediately */
          strcpy(fmsx_file, arch_file);
          break;
        }
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

    if (!fmsx_file[0])
    {
      pspUiAlert("No loadable files found in the archive");
      return 0;
    }

    /* Build the virtual compressed filename */
    if (!(file_to_load = malloc(sizeof(char) * (strlen(filename) + strlen(fmsx_file) + 2))))
      return 0;

    sprintf(file_to_load, "%s/%s", filename, fmsx_file);
  }
  else
#endif
  file_to_load = strdup(filename);

  /* Load disk image */
  if (pspFileIoEndsWith(file_to_load, "DSK") || pspFileIoEndsWith(file_to_load, "DSK.GZ")) 
  {
    if (!ChangeDisk(slot, file_to_load))
    {
      free(file_to_load);
      pspUiAlert("Error loading disk");
      return 0;
    }

    /* Set path as new disk path */
    free(DiskPath);
    DiskPath = pspFileIoGetParentDirectory(filename);

    free(Drive[slot]);
    Drive[slot] = file_to_load;
    item = pspMenuFindItemById(SystemUiMenu.Menu, SYSTEM_DRIVE_A + slot);
    pspMenuSetHelpText(item, LoadedDeviceText);
  }
  /* Load cartridge */
  else if (pspFileIoEndsWith(file_to_load, "ROM") || pspFileIoEndsWith(file_to_load, "ROM.GZ"))
  {
    if (!LoadCart(file_to_load, slot, ROMGUESS(slot)|ROMTYPE(slot)))
    {
      free(file_to_load);
      pspUiAlert("Error loading cartridge");
      return 0;
    }

    /* Set path as new cart path */
    free(CartPath);
    CartPath = pspFileIoGetParentDirectory(filename);

    /* Reset cartridge type */
    item = pspMenuFindItemById(SystemUiMenu.Menu, SYSTEM_CART_A_TYPE + slot);
    pspMenuSelectOptionByValue(item, (void*)CART_TYPE_AUTODETECT);

    free(ROM[slot]);
    ROM[slot] = file_to_load;
    item = pspMenuFindItemById(SystemUiMenu.Menu, SYSTEM_CART_A + slot);
    pspMenuSetHelpText(item, LoadedDeviceText);
  }
  else
  {
    free(file_to_load);
  }

  /* Update menu item */
  if (item) 
    pspMenuModifyOption(item->Options, pspFileIoGetFilename(filename), NULL);

  /* Clear the screen */
  pspImageClear(Screen, 0x8000);

  return 1;
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

int OnSaveStateOk(const void *gallery, const void *item)
{
  char *path;
  const char *config_name = GetConfigName();

  path = (char*)malloc(strlen(SaveStatePath) + strlen(config_name) + 8);
  sprintf(path, "%s%s_%02i.sta", SaveStatePath, config_name, 
    ((const PspMenuItem*)item)->ID);

  if (pspFileIoCheckIfExists(path) && pspUiConfirm("Load state?"))
  {
    if (LoadState(path))
    {
      ExitMenu = 1;
      pspMenuFindItemById(((PspUiGallery*)gallery)->Menu, 
        ((PspMenuItem*)item)->ID);
      free(path);

      return 1;
    }

    pspUiAlert("ERROR: State failed to load\nSee documentation for possible reasons");
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
    PspMenuItem *item = pspMenuFindItemById(gallery->Menu, sel->ID);

    path = (char*)malloc(strlen(SaveStatePath) + strlen(config_name) + 8);
    sprintf(path, "%s%s_%02i.sta", SaveStatePath, config_name, item->ID);

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
  char *path = (char*)malloc(strlen(SaveStatePath) + strlen(config_name) + 8);
  char *game_name = strdup(config_name);
  char *dot = strrchr(game_name, '.');
  if (dot) *dot='\0';

  /* Initialize icons */
  for (item = SaveStateGallery.Menu->First; item; item = item->Next)
  {
    sprintf(path, "%s%s_%02i.sta", SaveStatePath, config_name, item->ID);

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

/* Clean up any used resources */
void TrashMenu()
{
  int i;

  /* Save options */
  SaveOptions();

  /* Free memory */
  for (i=0; i<MAXCARTS; i++) if (ROM[i]) free(ROM[i]);
  for (i=0; i<MAXDRIVES; i++) if (Drive[i]) free(Drive[i]);
  if (Quickload) free(Quickload);

  /* Trash menus */
  pspMenuDestroy(SystemUiMenu.Menu);
  pspMenuDestroy(OptionUiMenu.Menu);
  pspMenuDestroy(ControlUiMenu.Menu);
  pspMenuDestroy(SaveStateGallery.Menu);

  /* Trash images */
  if (Background) pspImageDestroy(Background);
  if (NoSaveIcon) pspImageDestroy(NoSaveIcon);

  if (CartPath) free(CartPath);
  if (DiskPath) free(DiskPath);
  free(ScreenshotPath);
  free(SaveStatePath);
}
