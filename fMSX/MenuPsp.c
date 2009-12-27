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

#include "pl_psp.h"
#include "ui.h"
#include "pl_file.h"
#include "video.h"
#include "ctrl.h"
#include "image.h"
#include "pl_ini.h"
#include "pl_util.h"

#ifdef MINIZIP
#include "unzip.h"
#endif

#include "LibPsp.h"
#include "MenuPsp.h"
#include "Sound.h"

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
#define SYSTEM_MSXMUSIC    17
#define SYSTEM_MSXAUDIO    18
#define SYSTEM_HIRES       19
#define SYSTEM_OSI         20

#define OPTION_DISPLAY_MODE  1
#define OPTION_FRAME_LIMITER 2
#define OPTION_FRAMESKIP     3
#define OPTION_VSYNC         4
#define OPTION_CLOCK_FREQ    5
#define OPTION_SHOW_FPS      6
#define OPTION_CONTROL_MODE  7
#define OPTION_ANIMATE       8

extern PspImage *Screen;

extern int UseSound;
#ifdef ALTSOUND
extern int Use2413; /* MSX-MUSIC emulation (1=enable)  */
extern int Use8950; /* MSX-AUDIO emulation (1=enable)  */
#else
int Use2413 = 0; /* Stub variables */
int Use8950 = 0;
#endif

int ShowStatus=0;
int DisplayMode;
int FrameLimiter;
int VSync;
int ClockFreq;
int Frameskip;
int ShowFps;
int HiresEnabled;

static PspImage *Background;
static PspImage *NoSaveIcon;

char *ROM[MAXCARTS];
char *Drive[MAXDRIVES];
char *CartPath;
char *DiskPath;
char LoadedDrive[MAXDRIVES][1024];
char LoadedROM[MAXDRIVES][1024];

static unsigned int Crc32[MAXCARTS];
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
static const char* GetScreenshotName();
static void LoadOptions();
static int  SaveOptions();

static PspImage* LoadStateIcon(const char *path);
static int LoadState(const char *path);
static PspImage* SaveState(const char *path, const PspImage *icon);

#define CART_TYPE_AUTODETECT 8

typedef struct {
  unsigned long  Crc;
  unsigned short RomType;
} RomTypeMapping;

/* WARNING: RomTypeMapping is padded to 8 bytes; overriding it */
/* to write only 6. If RomTypeMapping changes, adjust this number */
/* accordingly! */
#define ROMTYPE_MAPPING_SIZE 6
#define MAX_ROMTYPE_MAPPINGS 500

RomTypeMapping RomTypeMappings[MAX_ROMTYPE_MAPPINGS] = 
{
  { 0x089c8164, 0x05 }, /* Zanac Ex */
  { 0xebccb796, 0x04 }, /* Mon Mon Monster */
  { 0x00000000, 0x00 }  /* End marker */
};

static int RomTypeCustomStart     = MAX_ROMTYPE_MAPPINGS;
static int RomTypeMappingModified = 0;

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
PL_MENU_OPTIONS_BEGIN(ButtonMapOptions)
    /* Unmapped */
  PL_MENU_OPTION("None", 0)
    /* Special */
  PL_MENU_OPTION("Special: Open Menu",       (SPC|SPC_MENU))
  PL_MENU_OPTION("Special: Show keyboard",   (SPC|SPC_KYBD))
  PL_MENU_OPTION("Special: Previous volume", (SPC|SPC_PDISK))
  PL_MENU_OPTION("Special: Next volume",     (SPC|SPC_NDISK))
  PL_MENU_OPTION("Special: Fast forward",    (SPC|SPC_FF))
    /* Joystick */
  PL_MENU_OPTION("Joystick Up",       (JST|JST_UP))
  PL_MENU_OPTION("Joystick Down",     (JST|JST_DOWN))
  PL_MENU_OPTION("Joystick Left",     (JST|JST_LEFT))
  PL_MENU_OPTION("Joystick Right",    (JST|JST_RIGHT))
  PL_MENU_OPTION("Joystick Button A", (JST|JST_FIREA))
  PL_MENU_OPTION("Joystick Button B", (JST|JST_FIREB))
    /* Directional */
  PL_MENU_OPTION("Keyboard Up",    (KBD|KBD_UP))
  PL_MENU_OPTION("Keyboard Down",  (KBD|KBD_DOWN)) 
  PL_MENU_OPTION("Keyboard Left",  (KBD|KBD_LEFT))
  PL_MENU_OPTION("Keyboard Right", (KBD|KBD_RIGHT))
    /* Etc... */
  PL_MENU_OPTION("Spacebar",  (KBD|KBD_SPACE))  
  PL_MENU_OPTION("Return",    (KBD|KBD_ENTER))
  PL_MENU_OPTION("Escape",    (KBD|KBD_ESCAPE))
  PL_MENU_OPTION("Backspace", (KBD|KBD_BS))
  PL_MENU_OPTION("Tab",       (KBD|KBD_TAB))
  PL_MENU_OPTION("Select",    (KBD|KBD_SELECT))
  PL_MENU_OPTION("Home",      (KBD|KBD_HOME))
  PL_MENU_OPTION("Delete",    (KBD|KBD_DELETE))
  PL_MENU_OPTION("Insert",    (KBD|KBD_INSERT))
  PL_MENU_OPTION("Stop",      (KBD|KBD_STOP))
    /* Function keys */
  PL_MENU_OPTION("F1", (KBD|KBD_F1)) 
  PL_MENU_OPTION("F2", (KBD|KBD_F2))
  PL_MENU_OPTION("F3", (KBD|KBD_F3))
  PL_MENU_OPTION("F4", (KBD|KBD_F4))
  PL_MENU_OPTION("F5", (KBD|KBD_F5))
    /* Numbers */
  PL_MENU_OPTION("0", (KBD|0x30)) PL_MENU_OPTION("1", (KBD|0x31)) 
  PL_MENU_OPTION("2", (KBD|0x32)) PL_MENU_OPTION("3", (KBD|0x33))
  PL_MENU_OPTION("4", (KBD|0x34)) PL_MENU_OPTION("5", (KBD|0x35))
  PL_MENU_OPTION("6", (KBD|0x36)) PL_MENU_OPTION("7", (KBD|0x37))
  PL_MENU_OPTION("8", (KBD|0x38)) PL_MENU_OPTION("9", (KBD|0x39))
    /* Alphabet */
  PL_MENU_OPTION("A", (KBD|0x41)) PL_MENU_OPTION("B", (KBD|0x42)) 
  PL_MENU_OPTION("C", (KBD|0x43)) PL_MENU_OPTION("D", (KBD|0x44))
  PL_MENU_OPTION("E", (KBD|0x45)) PL_MENU_OPTION("F", (KBD|0x46))
  PL_MENU_OPTION("G", (KBD|0x47)) PL_MENU_OPTION("H", (KBD|0x48))
  PL_MENU_OPTION("I", (KBD|0x49)) PL_MENU_OPTION("J", (KBD|0x4A))
  PL_MENU_OPTION("K", (KBD|0x4B)) PL_MENU_OPTION("L", (KBD|0x4C))
  PL_MENU_OPTION("M", (KBD|0x4D)) PL_MENU_OPTION("N", (KBD|0x4E))
  PL_MENU_OPTION("O", (KBD|0x4F)) PL_MENU_OPTION("P", (KBD|0x50))
  PL_MENU_OPTION("Q", (KBD|0x51)) PL_MENU_OPTION("R", (KBD|0x52))
  PL_MENU_OPTION("S", (KBD|0x53)) PL_MENU_OPTION("T", (KBD|0x54))
  PL_MENU_OPTION("U", (KBD|0x55)) PL_MENU_OPTION("V", (KBD|0x56)) 
  PL_MENU_OPTION("W", (KBD|0x57)) PL_MENU_OPTION("X", (KBD|0x58))
  PL_MENU_OPTION("Y", (KBD|0x59)) PL_MENU_OPTION("Z", (KBD|0x5A)) 
    /* Symbols */
  PL_MENU_OPTION("' (Single quote)",  (KBD|0x27)) 
  PL_MENU_OPTION(", (Comma)",         (KBD|0x2C))
  PL_MENU_OPTION("- (Minus)",         (KBD|0x2D))
  PL_MENU_OPTION(". (Period)",        (KBD|0x2E))
  PL_MENU_OPTION("/ (Forward slash)", (KBD|0x2F))
  PL_MENU_OPTION("; (Semicolon)",     (KBD|0x3B))
  PL_MENU_OPTION("= (Equals)",        (KBD|0x3D))
  PL_MENU_OPTION("[ (Left bracket)",  (KBD|0x5B))
  PL_MENU_OPTION("] (Right bracket)", (KBD|0x5D))
  PL_MENU_OPTION("\\ (Backslash)",    (KBD|0x5C))
  PL_MENU_OPTION("` (Backquote)",     (KBD|0x60))
    /* Numpad */
  PL_MENU_OPTION("Num. pad 0", (KBD|KBD_NUMPAD0)) 
  PL_MENU_OPTION("Num. pad 1", (KBD|KBD_NUMPAD1))
  PL_MENU_OPTION("Num. pad 2", (KBD|KBD_NUMPAD2))
  PL_MENU_OPTION("Num. pad 3", (KBD|KBD_NUMPAD3))
  PL_MENU_OPTION("Num. pad 4", (KBD|KBD_NUMPAD4))
  PL_MENU_OPTION("Num. pad 5", (KBD|KBD_NUMPAD5))
  PL_MENU_OPTION("Num. pad 6", (KBD|KBD_NUMPAD6))
  PL_MENU_OPTION("Num. pad 7", (KBD|KBD_NUMPAD7))
  PL_MENU_OPTION("Num. pad 8", (KBD|KBD_NUMPAD8))
  PL_MENU_OPTION("Num. pad 9", (KBD|KBD_NUMPAD9))
    /* State keys */
  PL_MENU_OPTION("Shift",       (KBD|KBD_SHIFT))
  PL_MENU_OPTION("Control",     (KBD|KBD_CONTROL))
  PL_MENU_OPTION("Graph",       (KBD|KBD_GRAPH))
  PL_MENU_OPTION("Caps Lock",   (KBD|KBD_CAPSLOCK))
  PL_MENU_OPTION("Country key", (KBD|KBD_COUNTRY))
PL_MENU_OPTIONS_END
PL_MENU_OPTIONS_BEGIN(MapperTypeOptions)
  PL_MENU_OPTION("Autodetect",   CART_TYPE_AUTODETECT)
  PL_MENU_OPTION("Generic 8kB",  0)
  PL_MENU_OPTION("Generic 16kB", 1)
  PL_MENU_OPTION("Konami5 8kB",  2)
  PL_MENU_OPTION("Konami4 8kB",  3)
  PL_MENU_OPTION("ASCII 8kB",    4)
  PL_MENU_OPTION("ASCII 16kB",   5)
  PL_MENU_OPTION("GameMaster2",  6)
  PL_MENU_OPTION("FMPAC",        7)
PL_MENU_OPTIONS_END
PL_MENU_OPTIONS_BEGIN(TimingOptions)
  PL_MENU_OPTION("NTSC (60 Hz)", MSX_NTSC)
  PL_MENU_OPTION("PAL (50 Hz)",  MSX_PAL)
PL_MENU_OPTIONS_END
PL_MENU_OPTIONS_BEGIN(ModelOptions)
  PL_MENU_OPTION("MSX1",  MSX_MSX1)
  PL_MENU_OPTION("MSX2",  MSX_MSX2)
  PL_MENU_OPTION("MSX2+", MSX_MSX2P)
PL_MENU_OPTIONS_END
PL_MENU_OPTIONS_BEGIN(RAMOptions)
  PL_MENU_OPTION("Default",0)
  PL_MENU_OPTION("64kB",   4)
  PL_MENU_OPTION("128kB",  8)
  PL_MENU_OPTION("256kB", 16)
  PL_MENU_OPTION("512kB", 32)
  PL_MENU_OPTION("1MB",   64)
  PL_MENU_OPTION("2MB",  128)
  PL_MENU_OPTION("4MB",  256)
PL_MENU_OPTIONS_END
PL_MENU_OPTIONS_BEGIN(VRAMOptions)
  PL_MENU_OPTION("Default", 0)
  PL_MENU_OPTION("32kB",  2)
  PL_MENU_OPTION("64kB",  4)
  PL_MENU_OPTION("128kB", 8)
PL_MENU_OPTIONS_END
PL_MENU_OPTIONS_BEGIN(CartNameOptions)
  PL_MENU_OPTION(EmptySlot, NULL)
PL_MENU_OPTIONS_END

static const char 
  ControlHelpText[] = "\026\250\020 Change mapping\t\026\001\020 Save to \271\t\026\244\020 Set as default\t\026\243\020 Load defaults",
  LoadedDeviceText[] = "\026\001\020 Browse\t\026\243\020 Eject",
  EmptyDeviceText[] = "\026\001\020 Browse",
  PresentSlotText[] = "\026\244\020 Save\t\026\001\020 Load\t\026\243\020 Delete",
  EmptySlotText[] = "\026\244\020 Save";

/* Define menu lists */
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
  PL_MENU_ITEM(PSP_CHAR_SELECT"+"PSP_CHAR_LTRIGGER, MAP_BUTTON_SELECTLTRIG, 
      ButtonMapOptions, ControlHelpText)
  PL_MENU_ITEM(PSP_CHAR_SELECT"+"PSP_CHAR_RTRIGGER, MAP_BUTTON_SELECTRTRIG, 
      ButtonMapOptions, ControlHelpText)
PL_MENU_ITEMS_END
PL_MENU_ITEMS_BEGIN(SystemMenuDef)
#ifdef ALTSOUND
  PL_MENU_HEADER("Audio")
  PL_MENU_ITEM("MSX Audio emulation", SYSTEM_MSXAUDIO, ToggleOptions,
      "\026\250\020 Toggle MSX Music emulation")
  PL_MENU_ITEM("MSX Music emulation", SYSTEM_MSXMUSIC, ToggleOptions,
      "\026\250\020 Toggle MSX Audio emulation")
#endif
  PL_MENU_HEADER("Video")
  PL_MENU_ITEM("High-resolution renderer", SYSTEM_HIRES, ToggleOptions,
      "\026\250\020 Toggle hi-res rendering for wide modes (6, 7, 80-text)")
  PL_MENU_HEADER("Cartridges")
  PL_MENU_ITEM("Slot A", SYSTEM_CART_A, CartNameOptions, EmptyDeviceText)
  PL_MENU_ITEM("Type", SYSTEM_CART_A_TYPE, MapperTypeOptions, 
      "\026\250\020 Select cartridge type")
  PL_MENU_ITEM("Slot B", SYSTEM_CART_B, CartNameOptions, EmptyDeviceText)
  PL_MENU_ITEM("Type", SYSTEM_CART_B_TYPE, MapperTypeOptions,
      "\026\250\020 Select cartridge type")
  PL_MENU_HEADER("Drives")
  PL_MENU_ITEM("Drive A", SYSTEM_DRIVE_A, CartNameOptions, EmptyDeviceText)
  PL_MENU_ITEM("Drive B", SYSTEM_DRIVE_B, CartNameOptions, EmptyDeviceText)
  PL_MENU_HEADER("System")
  PL_MENU_ITEM("Model", SYSTEM_MODEL,ModelOptions,
      "\026\250\020 Select MSX model")
  PL_MENU_ITEM("CPU Timing", SYSTEM_TIMING, TimingOptions,
      "\026\250\020 Select video timing mode (PAL/NTSC)")
  PL_MENU_ITEM("RAM", SYSTEM_RAMPAGES, RAMOptions, 
      "\026\250\020 Change amount of system memory")
  PL_MENU_ITEM("Video RAM", SYSTEM_VRAMPAGES, VRAMOptions,
      "\026\250\020 Change amount of video memory")
  PL_MENU_HEADER("Interface")
  PL_MENU_ITEM("On-screen Indicators", SYSTEM_OSI,ToggleOptions,
      "\026\250\020 Show/hide on-screen indicators (floppy, etc...)")
  PL_MENU_HEADER("Options")
  PL_MENU_ITEM("Reset", SYSTEM_RESET, NULL, "\026\001\020 Reset MSX" )
  PL_MENU_ITEM("Save screenshot",  SYSTEM_SCRNSHOT, NULL,
      "\026\001\020 Save screenshot")
PL_MENU_ITEMS_END

int OnGenericButtonPress(const PspUiFileBrowser *browser, const char *path, 
      u32 button_mask);
int OnSaveStateButtonPress(const PspUiGallery *gallery, pl_menu_item* item, 
       u32 button_mask);
int OnMenuButtonPress(const struct PspUiMenu *uimenu, pl_menu_item* item, 
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

int OnMenuItemChanged(const struct PspUiMenu *uimenu, pl_menu_item* item, 
      const pl_menu_option* option);

const char* OnSplashGetStatusBarText(const struct PspUiSplash *splash);

static void SetRomType(unsigned long crc, unsigned short rom_type);
static unsigned short GetRomType(unsigned long crc);
static int SaveRomTypeMappings();
static int LoadRomTypeMappings();

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

  /* Reset variables */
  for (i = 0; i < MAXCARTS; i++) ROM[i] = NULL;
  for (i = 0; i < MAXDRIVES; i++) Drive[i] = NULL;
  Quickload = NULL;
  TabIndex = TAB_ABOUT;
  Background = NULL;
  ControlMode = 0;
  CartPath = NULL;
  DiskPath = NULL;

  /* Initialize paths */
  SaveStatePath 
    = (char*)malloc(sizeof(char) * (strlen(pl_psp_get_app_directory()) + strlen(SaveStateDir) + 2));
  sprintf(SaveStatePath, "%s%s/", pl_psp_get_app_directory(), SaveStateDir);
  ScreenshotPath = (char*)malloc(sizeof(char) * 1024);
  sprintf(ScreenshotPath, "ms0:/PSP/PHOTO/%s/", PSP_APP_NAME);

  /* Initialize system menu */
  pl_menu_create(&SystemUiMenu.Menu, SystemMenuDef);

  /* Initialize options menu */
  pl_menu_create(&OptionUiMenu.Menu, OptionMenuDef);

  /* Initialize control menu */
  pl_menu_create(&ControlUiMenu.Menu, ControlMenuDef);

  /* Load the background image */
  Background = pspImageLoadPng("background.png");

  /* Init NoSaveState icon image */
  NoSaveIcon=pspImageCreate(136, 114, PSP_IMAGE_16BPP);
  pspImageClear(NoSaveIcon, RGB(0x00,0x00,0x55));

  /* Initialize state menu */
  pl_menu_item *item;
  for (i = 0; i < 10; i++)
  {
    item = pl_menu_append_item(&SaveStateGallery.Menu, i, NULL);
    pl_menu_set_item_help_text(item, EmptySlotText);
  }

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
  UiMetric.ScrollbarColor = PSP_COLOR_GRAY;
  UiMetric.ScrollbarBgColor = 0x44ffffff;
  UiMetric.ScrollbarWidth = 10;
  UiMetric.TextColor = PSP_COLOR_GRAY;
  UiMetric.SelectedColor = COLOR(0xff,0xff,0,0xff);
  UiMetric.SelectedBgColor = COLOR(0,0,0,0x55);
  UiMetric.StatusBarColor = PSP_COLOR_WHITE;
  UiMetric.BrowserFileColor = PSP_COLOR_GRAY;
  UiMetric.BrowserDirectoryColor = PSP_COLOR_YELLOW;
  UiMetric.GalleryIconsPerRow = 5;
  UiMetric.GalleryIconMarginWidth = 16;
  UiMetric.MenuItemMargin = 20;
  UiMetric.MenuSelOptionBg = PSP_COLOR_BLACK;
  UiMetric.MenuOptionBoxColor = PSP_COLOR_GRAY;
  UiMetric.MenuOptionBoxBg = COLOR(0, 0, 0, 0xBB);
  UiMetric.MenuDecorColor = UiMetric.SelectedColor;
  UiMetric.DialogFogColor = COLOR(0, 0, 0, 88);
  UiMetric.TitlePadding = 4;
  UiMetric.TitleColor = PSP_COLOR_WHITE;
  UiMetric.MenuFps = 30;
  UiMetric.TabBgColor = PSP_COLOR_WHITE;
  UiMetric.BrowserScreenshotPath = ScreenshotPath;
  UiMetric.BrowserScreenshotDelay = 30;

  /* Initialize ROM type mappings */
  RomTypeMappingModified = 0;
  for (i = 0; RomTypeMappings[i].Crc != 0 && i < MAX_ROMTYPE_MAPPINGS; i++);
  RomTypeCustomStart = i; /* Custom ROM types start here */

  /* Initialize options */
  LoadOptions();
  LoadRomTypeMappings();

  /* If this is going to take a while... */
  if (Use2413 || Use8950) 
    pspUiFlashMessage("Initializing sound emulation, please wait...");
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
  pl_menu_item *item;
  for (i = 0; userdata[i]; i++)
  {
    item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, userdata[i]);
    pl_menu_update_option(item->options, EmptySlot, NULL);
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

int OnMenuItemChanged(const struct PspUiMenu *uimenu, pl_menu_item* item, 
  const pl_menu_option* option)
{
  if (uimenu == &OptionUiMenu)
  {
    switch((int)item->id)
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
  else if (uimenu == &ControlUiMenu)
  {
    GameConfig.ButtonMap[item->id] = (unsigned int)option->value;
  }
  else if (uimenu == &SystemUiMenu)
  {
    switch(item->id)
    {
    case SYSTEM_HIRES:
      HiresEnabled = (int)option->value;
      break;

    case SYSTEM_MSXAUDIO:
      if ((int)option->value != Use8950)
      {
        /* Restart sound engine */
        TrashAudio();
        if ((int)option->value || Use2413)
          pspUiFlashMessage("Please wait, reinitializing sound engine...");
        Use8950 = (int)option->value;
        if (UseSound && !InitSound(UseSound, 0))
          pspUiAlert("Sound initialization failed");
      }
      break;

    case SYSTEM_MSXMUSIC:
      if ((int)option->value != Use2413)
      {
        /* Restart sound engine */
        TrashAudio();
        if ((int)option->value || Use8950)
          pspUiFlashMessage("Please wait, reinitializing sound engine...");
        Use2413 = (int)option->value;
        if (UseSound && !InitSound(UseSound, 0))
          pspUiAlert("Sound initialization failed");
      }
      break;

    case SYSTEM_CART_A_TYPE:
    case SYSTEM_CART_B_TYPE:

      if (!pspUiConfirm("This will reset the system. Proceed?"))
        return 0;

      SETROMTYPE(item->id - SYSTEM_CART_A_TYPE, (int)option->value);
      SetRomType(Crc32[item->id - SYSTEM_CART_A_TYPE], 
        (unsigned short)(int)option->value);

      ResetMSX(Mode,RAMPages,VRAMPages);
      break;

    case SYSTEM_TIMING:
      if ((int)option->value != (Mode & MSX_VIDEO))
      {
        if (!pspUiConfirm("This will reset the system. Proceed?"))
          return 0;

        ResetMSX((Mode&~MSX_VIDEO)|(int)option->value,RAMPages,VRAMPages);
      }
      break;
    case SYSTEM_MODEL:
      if ((int)option->value != (Mode & MSX_MODEL))
      {
        if (!pspUiConfirm("This will reset the system. Proceed?"))
          return 0;

        ResetMSX((Mode&~MSX_MODEL)|(int)option->value,RAMPages,VRAMPages);

        pl_menu_item *ram_item, *vram_item;
        ram_item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_RAMPAGES);
        vram_item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_VRAMPAGES);

        if ((int)ram_item->selected->value != RAMPages 
          || (int)vram_item->selected->value != VRAMPages)
        {
          pspUiAlert("Memory settings have been modified to fit the system");

    		  pl_menu_select_option_by_value(ram_item, (void*)RAMPages);
    		  pl_menu_select_option_by_value(vram_item, (void*)VRAMPages);
    		}
      }
      break;
    case SYSTEM_RAMPAGES:
      if ((int)option->value != RAMPages)
      {
        if (!pspUiConfirm("This will reset the system. Proceed?"))
          return 0;

        ResetMSX(Mode,(int)option->value,VRAMPages);

        /* See if fMSX readjusted memory size for validity */
        if ((int)option->value != RAMPages)
    		{
    		  if (option->value)
    		    pspUiAlert("Your selection has been modified to fit the system");

    		  pl_menu_select_option_by_value(item, (void*)RAMPages);
          return 0;
    		}
      }
      break;
    case SYSTEM_VRAMPAGES:
      if ((int)option->value != VRAMPages)
      {
        if (!pspUiConfirm("This will reset the system. Proceed?"))
          return 0;

        ResetMSX(Mode,RAMPages,(int)option->value);

        /* See if fMSX readjusted memory size for validity */
        if ((int)option->value != VRAMPages)
    		{
    		  if (option->value)
    		    pspUiAlert("Your selection has been modified to fit the system");

    		  pl_menu_select_option_by_value(item, (void*)VRAMPages);
          return 0;
    		}
      }
      break;
    case SYSTEM_OSI:
      ShowStatus = (int)option->value;
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
    opt = ((const pl_menu_item*)sel_item)->id;

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
      if (!pl_util_save_image_seq(ScreenshotPath, GetScreenshotName(), Screen))
        pspUiAlert("ERROR: Screenshot not saved");
      else
        pspUiAlert("Screenshot saved successfully");
      break;
    }
  }

  return 0;
}

int  OnMenuButtonPress(const struct PspUiMenu *uimenu, 
  pl_menu_item* sel_item, 
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
      memcpy(&GameConfig, &DefaultConfig, sizeof(struct GameConfig));

      /* Modify the menu */
      for (item = ControlUiMenu.Menu.items, i = 0; item; item = item->next, i++)
        pl_menu_select_option_by_value(item, (void*)DefaultConfig.ButtonMap[i]);

      return 0;
    }
  }
  else if (uimenu == &SystemUiMenu)
  {
    int opt, slot;
    pl_menu_item *item;

    if (button_mask & PSP_CTRL_TRIANGLE)
    {
      opt = sel_item->id;

      switch (opt)
      {
      case SYSTEM_CART_A:
      case SYSTEM_CART_B:

        /* Eject cartridge */
        slot = opt - SYSTEM_CART_A;
        if (ROM[slot]) { free(ROM[slot]); ROM[slot] = NULL; }
        pl_menu_update_option(sel_item->options, EmptySlot, NULL);
        pl_menu_set_item_help_text(sel_item, EmptyDeviceText);
        LoadCart(NULL, slot, 0);

        /* Reset cartridge type */
        item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_CART_A_TYPE + slot);
        pl_menu_select_option_by_value(item, (void*)CART_TYPE_AUTODETECT);

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
        pl_menu_update_option(sel_item->options, EmptySlot, NULL);
        pl_menu_set_item_help_text(sel_item, EmptyDeviceText);
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
  else return 0;

  return 1;
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
  ShowStatus = pl_ini_get_int(&init, "Video", "Show Status Indicators", 1);
  ControlMode = pl_ini_get_int(&init, "Menu", "Control Mode", 0);
  UiMetric.Animate = pl_ini_get_int(&init, "Menu", "Animate", 1);

  Mode = (Mode&~MSX_VIDEO) 
    | pl_ini_get_int(&init, "System", "Timing", Mode & MSX_VIDEO);
  Mode = (Mode&~MSX_MODEL) 
    | pl_ini_get_int(&init, "System", "Model", Mode & MSX_MODEL);
  RAMPages = pl_ini_get_int(&init, "System", "RAM Pages", RAMPages);
  VRAMPages = pl_ini_get_int(&init, "System", "VRAM Pages", VRAMPages);

  HiresEnabled = pl_ini_get_int(&init, "Video", "Hires Renderer", 0);

#ifdef ALTSOUND
  Use2413 = pl_ini_get_int(&init, "Audio", "MSX Music", 0);
  Use8950 = pl_ini_get_int(&init, "Audio", "MSX Audio", 0);
#endif

  if (DiskPath) free(DiskPath);
  if (CartPath) free(CartPath); 
  if (Quickload) free(Quickload);

  DiskPath = (char*)malloc(1024);
  CartPath = (char*)malloc(1024);
  Quickload = (char*)malloc(1024);
  pl_ini_get_string(&init, "File", "Disk Path", NULL, DiskPath, 1024);
  pl_ini_get_string(&init, "File", "Cart Path", NULL, CartPath, 1024);
  pl_ini_get_string(&init, "File", "Game Path", NULL, Quickload, 1024);

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
  pl_ini_set_int(&init, "Video", "Show Status Indicators", ShowStatus);
  pl_ini_set_int(&init, "Menu", "Control Mode", ControlMode);
  pl_ini_set_int(&init, "Menu", "Animate", UiMetric.Animate);

  pl_ini_set_int(&init, "System", "Timing", Mode & MSX_VIDEO);
  pl_ini_set_int(&init, "System", "Model", Mode & MSX_MODEL);
  pl_ini_set_int(&init, "System", "RAM Pages", RAMPages);
  pl_ini_set_int(&init, "System", "VRAM Pages", VRAMPages);

#ifdef ALTSOUND
  pl_ini_set_int(&init, "Audio", "MSX Audio", Use8950);
  pl_ini_set_int(&init, "Audio", "MSX Music", Use2413);
#endif

  pl_ini_set_int(&init, "Video", "Hires Renderer", HiresEnabled);

  if (Quickload)
  {
    char *qlpath = (char*)malloc(1024);

    pl_file_get_parent_directory(Quickload, qlpath, 1024);
    pl_ini_set_string(&init, "File", "Game Path", qlpath);
    free(qlpath);
  }

  if (DiskPath) pl_ini_set_string(&init, "File", "Disk Path", DiskPath);
  if (CartPath) pl_ini_set_string(&init, "File", "Cart Path", CartPath);

  /* Save INI file */
  int status = pl_ini_save(&init, path);

  /* Clean up */
  pl_ini_destroy(&init);
  free(path);

  return status;
}

void DisplayMenu()
{
  pl_menu_item *item;
  ExitMenu = 0;

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
      pspUiOpenBrowser(&QuickloadBrowser, Quickload);
      break;
    case TAB_STATE:
      DisplayStateTab();
      break;
    case TAB_SYSTEM:
      /* Init system configuration */
      item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_HIRES);
      pl_menu_select_option_by_value(item, (void*)HiresEnabled);
      item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_TIMING);
      pl_menu_select_option_by_value(item, (void*)(Mode & MSX_VIDEO));
      item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_MODEL);
      pl_menu_select_option_by_value(item, (void*)(Mode & MSX_MODEL));
      item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_RAMPAGES);
      pl_menu_select_option_by_value(item, (void*)RAMPages);
      item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_VRAMPAGES);
      pl_menu_select_option_by_value(item, (void*)VRAMPages);
#ifdef ALTSOUND
      item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_MSXAUDIO);
      pl_menu_select_option_by_value(item, (void*)Use8950);
      item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_MSXMUSIC);
      pl_menu_select_option_by_value(item, (void*)Use2413);
#endif
      item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_OSI);
      pl_menu_select_option_by_value(item, (void*)ShowStatus);
      item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_CART_A);
      pl_menu_select_option_by_index(item, 0);
      item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_CART_B);
      pl_menu_select_option_by_index(item, 0);
      item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_DRIVE_A);
      pl_menu_select_option_by_index(item, 0);
      item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_DRIVE_B);
      pl_menu_select_option_by_index(item, 0);
      item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_CART_A_TYPE);
      pl_menu_select_option_by_index(item, 0);
      item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_CART_B_TYPE);
      pl_menu_select_option_by_index(item, 0);

      pspUiOpenMenu(&SystemUiMenu, NULL);
      break;
    case TAB_CONTROL:
      DisplayControlTab();
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

/* Gets configuration name */
static const char* GetConfigName()
{
  if (ROM[0]) return pl_file_get_filename(ROM[0]);
  if (Drive[0]) return pl_file_get_filename(Drive[0]);

  return BasicConfigFile;
}

static const char* GetScreenshotName()
{
  if (ROM[0]) return pl_file_get_filename(LoadedROM[0]);
  if (Drive[0]) return pl_file_get_filename(LoadedDrive[0]);

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
  if (!(path = (char*)malloc(sizeof(char) * (strlen(pl_psp_get_app_directory()) + strlen(ConfigDir) + strlen(filename) + 6))))
    return 0;
  sprintf(path, "%s%s/%s.cnf", pl_psp_get_app_directory(), ConfigDir, filename);

  /* If no configuration, load defaults */
  if (!pl_file_exists(path))
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
  if (!(path = (char*)malloc(sizeof(char) * (strlen(pl_psp_get_app_directory()) + strlen(ConfigDir) + strlen(filename) + 6))))
    return 0;
  sprintf(path, "%s%s/%s.cnf", pl_psp_get_app_directory(), ConfigDir, filename);

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
  int height = pspFontGetLineHeight(UiMetric.Font);
  int width;

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
  w = (Screen->Viewport.Width > WIDTH) 
    ? Screen->Viewport.Width >> 2 /* High resolution */
    : Screen->Viewport.Width >> 1;
  h = Screen->Viewport.Height >> 1;
  x = UiMetric.Right - w - UiMetric.ScrollbarWidth;
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
  pl_menu_item *item = NULL;
  char *file_to_load = NULL;
  int compressed = 0;

#ifdef MINIZIP
  /* Check if it's a zip file */
  if (pl_file_is_of_type(filename, "ZIP"))
  {
    unzFile zip;
    unz_file_info fi;
    unz_global_info gi;
    char arch_file[255], fmsx_file[255];
    fmsx_file[0] = '\0';
    compressed = 1;

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
      if (pl_file_is_of_type(arch_file, "ROM"))
      {
        Crc32[slot] = fi.crc;
        strcpy(fmsx_file, arch_file);
        break;
      }
      else if (pl_file_is_of_type(arch_file, "DSK"))
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
  if (pl_file_is_of_type(file_to_load, "DSK") || pl_file_is_of_type(file_to_load, "DSK.GZ")) 
  {
    if (!ChangeDisk(slot, file_to_load))
    {
      free(file_to_load);
      pspUiAlert("Error loading disk");
      return 0;
    }

    /* Set path as new disk path */
    free(DiskPath);
    DiskPath = (char*)malloc(1024);
    pl_file_get_parent_directory(filename, DiskPath, 1024);

    free(Drive[slot]);
    Drive[slot] = file_to_load;
    strcpy(LoadedDrive[slot], filename);

    item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_DRIVE_A + slot);
    pl_menu_set_item_help_text(item, LoadedDeviceText);
  }
  /* Load cartridge */
  else if (pl_file_is_of_type(file_to_load, "ROM") || pl_file_is_of_type(file_to_load, "ROM.GZ"))
  {
    /* If uncompressed, get file CRC */
    if (!compressed)
      pl_util_compute_crc32_file(file_to_load, &Crc32[slot]);

    /* Initialize cart type */
    unsigned int cart_type = GetRomType(Crc32[slot]);

    if (!LoadCart(file_to_load, slot, ROMGUESS(slot)|ROMTYPE(slot)))
    {
      free(file_to_load);
      pspUiAlert("Error loading cartridge");
      return 0;
    }

    /* For some reason, setting ROM type for autodetect   */
    /* causes MEGAROM and other games to stop functioning */
    if (cart_type != CART_TYPE_AUTODETECT)
      SETROMTYPE(slot, cart_type);

    /* Set path as new cart path */
    free(CartPath);
    CartPath = (char*)malloc(1024);
    pl_file_get_parent_directory(filename, CartPath, 1024);

    /* Reset cartridge type */
    item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_CART_A_TYPE + slot);
    pl_menu_select_option_by_value(item, (void*)cart_type);

    free(ROM[slot]);
    ROM[slot] = file_to_load;
    strcpy(LoadedROM[slot], filename);

    item = pl_menu_find_item_by_id(&SystemUiMenu.Menu, SYSTEM_CART_A + slot);
    pl_menu_set_item_help_text(item, LoadedDeviceText);
  }
  else
  {
    free(file_to_load);
  }

  /* Update menu item */
  if (item) 
    pl_menu_update_option(item->options, pl_file_get_filename(filename), NULL);

  /* Clear the screen */
  pspImageClear(Screen, 0x8000);

  /* Reset selected state */
  SaveStateGallery.Menu.selected = NULL;

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

PspImage* GetScreenThumbnail(const PspImage *icon)
{
  /* If it's a low-res image, use the library */
  if (icon->Viewport.Width <= WIDTH)
    return pspImageCreateThumbnail(icon);

  /* Hi-res image - manually scale 0.25x0.5*/
  PspImage *thumb;
  if (!(thumb = pspImageCreate(icon->Viewport.Width / 4, 
    icon->Viewport.Height / 2, PSP_IMAGE_16BPP)))
    return NULL;

  int i, j, k, l;
  for (i = 0, k = 0; i < icon->Viewport.Height; i+=2, k++)
    for (j = 0, l = 0; j < icon->Viewport.Width; j+=4, l++)
      ((unsigned short*)thumb->Pixels)[k * thumb->Width + l] 
        = ((unsigned short*)icon->Pixels)[i * icon->Width + j];

  return thumb;
}

/* Save state */
PspImage* SaveState(const char *path, const PspImage *icon)
{
  /* Save state */
  if (!SaveSTA(path))
    return NULL;

  /* Create thumbnail */
  PspImage *thumb = GetScreenThumbnail(icon);
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
    ((const pl_menu_item*)item)->id);

  if (pl_file_exists(path) && pspUiConfirm("Load state?"))
  {
    if (LoadState(path))
    {
      ExitMenu = 1;
      pl_menu_find_item_by_id(&((PspUiGallery*)gallery)->Menu, 
        ((pl_menu_item*)item)->id);
      free(path);

      return 1;
    }

    pspUiAlert("ERROR: State failed to load\nSee documentation for possible reasons");
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

static void DisplayControlTab()
{
  pl_menu_item *item;
  char *game_name = strdup(GetConfigName());
  char *dot = strrchr(game_name, '.');
  int i;
  if (dot) *dot='\0';

  /* Load current button mappings */
  for (item = ControlUiMenu.Menu.items, i = 0; item; item = item->next, i++)
    pl_menu_select_option_by_value(item, (void*)GameConfig.ButtonMap[i]);

  pspUiOpenMenu(&ControlUiMenu, game_name);
  free(game_name);
}

static void DisplayStateTab()
{
  pl_menu_item *item, *sel = NULL;
  SceIoStat stat;
  ScePspDateTime latest;
  char caption[32];
  const char *config_name = GetConfigName();
  char *path = (char*)malloc(strlen(SaveStatePath) + strlen(config_name) + 8);
  char *game_name = strdup(config_name);
  char *dot = strrchr(game_name, '.');
  if (dot) *dot='\0';

  memset(&latest,0,sizeof(latest));

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

  free(path);

  /* Highlight the latest save state if none are selected */
  if (SaveStateGallery.Menu.selected == NULL)
    SaveStateGallery.Menu.selected = sel;

  pspUiOpenGallery(&SaveStateGallery, game_name);
  free(game_name);

  /* Destroy any icons */
  for (item = SaveStateGallery.Menu.items; item; item = item->next)
    if (item->param != NULL && item->param != NoSaveIcon)
      pspImageDestroy((PspImage*)item->param);
}

static void SetRomType(unsigned long crc, unsigned short rom_type)
{
  /* Find current setting (if present) */
  int i;
  for (i = 0; RomTypeMappings[i].Crc != 0 && i < MAX_ROMTYPE_MAPPINGS; i++)
  {
    if (RomTypeMappings[i].Crc == crc)
    {
      if (rom_type == CART_TYPE_AUTODETECT)
      {
        /* Don't keep "autodetect" mappings; remove from list */
        /* Find the last mapping */
        int j;
        for (j = i + 1; RomTypeMappings[j].Crc != 0 && j < MAX_ROMTYPE_MAPPINGS; j++);
        if (--j != i)
          RomTypeMappings[i] = RomTypeMappings[j]; /* Copy last to current   */
        RomTypeMappings[j].Crc = 0;                /* Mark last as 'deleted' */
      }
      else
        /* Override current setting */
        RomTypeMappings[i].RomType = rom_type;

      RomTypeMappingModified = 1;
      return;
    }
  }

  /* New mapping */
  if (i < MAX_ROMTYPE_MAPPINGS)
  {
    RomTypeMappings[i].Crc = crc;
    RomTypeMappings[i].RomType = rom_type;
    RomTypeMappingModified = 1;
  }
}

static unsigned short GetRomType(unsigned long crc)
{
  /* Find current setting (if present) */
  int i;
  for (i = 0; RomTypeMappings[i].Crc != 0 && i < MAX_ROMTYPE_MAPPINGS; i++)
    if (RomTypeMappings[i].Crc == crc)
      return RomTypeMappings[i].RomType;

  return CART_TYPE_AUTODETECT;
}

static int SaveRomTypeMappings()
{
  /* Don't save if nothing changed */
  if (!RomTypeMappingModified)
    return 1;

  char file_path[PL_FILE_MAX_PATH_LEN];
  sprintf(file_path, "%srommaps.bin", pl_psp_get_app_directory());

  FILE *file;
  if ((file = fopen(file_path, "w")) == NULL)
    return 0;

  int i, error = 0;
  for (i = RomTypeCustomStart; 
       RomTypeMappings[i].Crc != 0 && i < MAX_ROMTYPE_MAPPINGS; i++)
  {
    if (fwrite(&RomTypeMappings[i], ROMTYPE_MAPPING_SIZE, 1, file) != 1)
    {
      error = 1;
      break;
    }
  }

  RomTypeMappingModified = 0;
  fclose(file);

  return !error;
}

static int LoadRomTypeMappings()
{
  char file_path[PL_FILE_MAX_PATH_LEN];
  sprintf(file_path, "%srommaps.bin", pl_psp_get_app_directory());

  FILE *file;
  if ((file = fopen(file_path, "r")) == NULL)
    return 0;

  /* Start overwriting from custom slot marker */
  int i = RomTypeCustomStart, error = 0;

  /* Read in the rest */
  while (fread(&RomTypeMappings[i], ROMTYPE_MAPPING_SIZE, 1, file) == 1)
  {
    if (++i >= MAX_ROMTYPE_MAPPINGS)
    {
      error = 1;
      break;
    }
  }

  fclose(file);
  return !error;
}

/* Clean up any used resources */
void TrashMenu()
{
  int i;

  /* Save options */
  SaveOptions();
  SaveRomTypeMappings();

  /* Free memory */
  for (i=0; i<MAXCARTS; i++) if (ROM[i]) free(ROM[i]);
  for (i=0; i<MAXDRIVES; i++) if (Drive[i]) free(Drive[i]);
  if (Quickload) free(Quickload);

  /* Trash images */
  if (Background) pspImageDestroy(Background);
  if (NoSaveIcon) pspImageDestroy(NoSaveIcon);

  if (CartPath) free(CartPath);
  if (DiskPath) free(DiskPath);
  free(ScreenshotPath);
  free(SaveStatePath);
}
