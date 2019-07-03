ColEm PSP
=========

ColecoVision emulator

&copy; 2007-2010 Akop Karapetyan  
&copy; 1994-2010 Marat Fayzullin

For documentation on fMSX PSP, see [README.md](README.md).

New Features
------------

#### Version 2.6.1 (September 12 2010)

*   Now clearing all RAM to zeros (Heist works)
*   Now padding smaller ROMs with 0xFFs in LoadROM()
*   Fixed Reset9918() to reset VDP completely (Boulder Dash and Frogger)
*   Implemented SCREEN2 table address masking, thanks to Daniel Bienvenu
*   New save file format – not compatible with previous version

Installation
------------

Unzip `colempsp.zip` into `/PSP/GAME/` folder on the memory stick.

System ROM’s must reside in the same folder as `EBOOT.PBP` file, either uncompressed, or inside `SYSTEM.ZIP`. Game ROM’s can reside anywhere (the ROMS subdirectory is recommended, but not necessary).

Controls
--------

During emulation:

| PSP controls                    | Emulated controls            |
| ------------------------------- | ---------------------------- |
| D-pad up/down/left/right        | Joystick up/down/left/right  |
| Analog stick up/down/left/right | Joystick up/down/left/right  |
| [ ] (square)                    | Joystick Left button         |
| X (cross)                       | Joystick Right button        |
| O (circle)                      | Joystick Blue button         |
| ^ (triangle)                    | Joystick Purple button       |
| [R]                             | Show virtual keypad          |
| [L] + [R]                       | Return to emulator menu      |

When the virtual keypad is on:

| PSP controls                    | Function                 |
| ------------------------------- | ------------------------ |
| Directional pad                 | select key               |
| [ ] (square)                    | press key                |
| O (circle)                      | hold down/release key    |
| ^ (triangle)                    | release all held keys    |

Only certain keys can be held down.

Keyboard mappings can be modified for each game. By default, button configuration changes are not retained after a mapping is modified. To save changes, press [ ] (square) after desired mapping is configured.

Compiling
---------

To compile, ensure that [zlib](svn://svn.pspdev.org/psp/trunk/zlib) and [libpng](svn://svn.pspdev.org/psp/trunk/libpng) are installed, and run make:

`make -f ColEm.mak`

To enable zipped ROM support, `#define MINIZIP` in the makefile.

Version History
---------------

#### 2.5.2 (May 16 2010)

*   PSPLIB updated to latest version
*   Images are now saved under `PSP/PHOTOS/ColEm PSP`
*   Screenshot previews added to file browser
*   Time rewind – use Control tab to set any button as the ‘Rewind’ button

#### 2.5.1 (August 15 2008)

*   Maintenance update, mainly to synchronize with the main release

#### 2.4.1 (March 05 2008)

*   PSP version brought up to date with the official release (changes are mostly internal)
*   Replaced the rendering engine with a more stable one
*   When switching games, latest save state will be automatically highlighted

#### 2.2.2 (July 24 2007)

*   Fixed buffer overflow error affecting units without a battery

#### 2.2.1 (June 21 2007)

*   Initial release

Credits
-------

Marat Fayzullin (ColEm)  
Simon Tatham (fixed.fd font)  
Gilles Vollant (minizip)  
Ruka (PNG saving/loading code)
