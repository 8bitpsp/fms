/** ColEm: portable ColecoVision emulator ********************/
/**                                                         **/
/**                         MenuPsp.h                       **/
/**                                                         **/
/** This file contains declarations for routines dealing    **/
/** with menu navigation.                                   **/
/**                                                         **/
/** Copyright (C) Akop Karapetyan 2007                      **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

#ifndef _MENUPSP_H
#define _MENUPSP_H

#define DISPLAY_MODE_UNSCALED    0
#define DISPLAY_MODE_FIT_HEIGHT  1
#define DISPLAY_MODE_FILL_SCREEN 2

#define JST 0x10000
#define SPC 0x20000

#define CODE_MASK(x) (x & 0xffff)

#define SPC_MENU               1
#define SPC_KYBD               2

#define MAP_BUTTONS            18

#define MAP_ANALOG_UP          0
#define MAP_ANALOG_DOWN        1
#define MAP_ANALOG_LEFT        2 
#define MAP_ANALOG_RIGHT       3
#define MAP_BUTTON_UP          4
#define MAP_BUTTON_DOWN        5
#define MAP_BUTTON_LEFT        6
#define MAP_BUTTON_RIGHT       7
#define MAP_BUTTON_SQUARE      8
#define MAP_BUTTON_CROSS       9
#define MAP_BUTTON_CIRCLE      10
#define MAP_BUTTON_TRIANGLE    11
#define MAP_BUTTON_LTRIGGER    12
#define MAP_BUTTON_RTRIGGER    13
#define MAP_BUTTON_SELECT      14
#define MAP_BUTTON_START       15
#define MAP_BUTTON_LRTRIGGERS  16
#define MAP_BUTTON_STARTSELECT 17

struct GameConfig
{
  unsigned int ButtonMap[MAP_BUTTONS];
};

void InitMenu();
void DisplayMenu();
void TrashMenu();

#endif // _MENUPSP_H
