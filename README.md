fMSX PSP
========

MSX emulator

&copy; 2007-2010 Akop Karapetyan  
&copy; 1994-2010 Marat Fayzullin

For documentation on ColEm PSP, see [README_ColEm.md](README_ColEm.md).

New Features
------------

#### Version 3.5.41 (March 17 2010)

*   Fixed ‘Button Mode’ (US/JP) switch, so that it will correctly initialize on startup

Installation
------------

Unzip `fmsxpsp.zip` into `/PSP/GAME/` folder on the memory stick.

fMSX PSP supports cartridge and disk loading from ZIP files. System ROM’s may reside in a `SYSTEM.ZIP` file in the same folder as `EBOOT.PBP`. `PAINTER.ROM`, or `MSXDOS2.ROM` may also be zipped, but must reside in separate ZIP files (`FMPAC.ZIP`, `MSXDOS2.ZIP`, etc..). fMSX PSP will attempt to load the three automatically if free cartridge slots are available.

All other game ROM files must reside in separate ZIP files.

Controls
--------

During emulation:

| PSP controls                    | Emulated controls            |
| ------------------------------- | ---------------------------- |
| D-pad up/down/left/right        | Keyboard up/down/left/right  |
| Analog stick up/down/left/right | Joystick up/down/left/right  |
| [ ] (square)                    | Joystick button A            |
| X (cross)                       | Joystick button B            |
| O (circle)                      | Spacebar                     |
| [R]                             | Show virtual keypad          |
| [L] + [R]                       | Return to emulator menu      |
| Select + [L]                    | Previous disk volume         |
| Select + [R]                    | Next disk volume             |
| Start                           | F1                           |

When the virtual keyboard is on:

| PSP controls                    | Function                 |
| ------------------------------- | ------------------------ |
| Directional pad                 | select key               |
| [ ] (square)                    | press key                |
| O (circle)                      | hold down/release key    |
| ^ (triangle)                    | release all held keys    |

Only certain keys can be held down.

Keyboard mappings can be modified for each game. By default, button configuration changes are not retained after a mapping is modified. To save changes, press O (circle) after desired mapping is configured. To set the mapping as the default mapping, press [ ] (square) – to load the default mapping press ^ (triangle).

Sound
-----

fMSX PSP uses the PSG, SCC and MSX-MUSIC sound emulation engine by Mitsutaka Okazaki and MSX-AUDIO emulation engine by Tatsuyuki Satoh. While PSG and SCC emulation is always enabled, MSX-MUSIC and MSX-AUDIO are disabled by default, as they tax the PSP and are not used by most MSX programs/games.

The emulator can run at full speed (at 333MHz, without V-Sync) with only one of MSX-AUDIO or MSX-MUSIC enabled. With both enabled, emulator performance is significantly reduced. I recommend keeping MSX-AUDIO and MSX-MUSIC emulation disabled, enabling it only when necessary.

Display
-------

Until version 3.4.5, high-resolution screen modes (screens 6, 7 and text-80) were rendered on a 256-pixel wide canvas by skipping every other column. This works very well for most graphics modes – the deficiency is mostly noticeable when rendering text in 80-column mode.

Version 3.4.5 adds optional high-resolution rendering – image is rendered on a 512-pixel canvas, then squeezed horizontally to take up 256 pixels, with the idea that bilinear filtering engine offered by the PSP can provide a less coarse “squeezed” image than just skipping every other column. Unfortunately, the differences are rather disappointing – the biggest improvement is noticeable in text-80 mode, while the added task of rendering to a 512-pixel image slows down emulation. My suggestion, therefore, is to keep the hi-res rendering engine disabled, unless you really need more detail (to see the actual image, set ‘Display Size’ in options to ‘Actual size’ – though this will only show the first 480 pixels, due to the PSP screen limitation)

Disk Writing
------------

Versions of fMSX prior to 3.3 mapped MSX disk sectors to the file containing the disk image, and reads/writes to the MSX disk were implemented as reads/writes to the image on the storage device (in this case, the memory stick).

Version 3.3 of fMSX includes a much more faithful emulation of the floppy disk controller, which no longer involves direct writing to the storage device – the disk image is loaded into memory instead. Versions 3.3.1 and 3.3.2 of fMSX stopped automatically writing the changes to disk – starting with 3.3.25, disk changes need to be explicitly committed to memory stick. This can either be done by loading a new disk in the same slot via the System tab (not the Game tab), or by ejecting the disk. Either way, you will be asked if you want to commit the changes to the image before ejecting or loading another image.

Note that ZIP compressed files cannot be updated.

**Before committing changes to storage, make sure the MSX floppy drive is not in use**

Multi-Volume Disk Images
------------------------

fMSX PSP can load multi-volume games from a single ZIP file. To load such a game, store all files in the same ZIP. It is important that the archived files are numbered 1-0, with ‘0’ representing the 10th volume (archiving sequence is not important) – fMSX PSP will attempt to load the first disk in the sequence. As an example, a ZIP file could contain the following disk images:

* snatcher1.dsk
* snatcher2.dsk
* snatcher3.dsk

To switch disks mid-game, map any of the buttons to the ‘Special: Previous volume’ or ‘Special: Next volume’ actions in the control menu. When a ‘volume switch’ button is pressed, and the next disk image in the sequence is found, an icon will appear with the number of the volume loaded.

Note that multi-volume files do not have to be ZIP\-compressed for the ‘volume switch’ feature to work – volume switching will also work for uncompressed DSK files, as long as they’re named as described above.

**Changes to the disk image will not be saved when using this feature**

Frequently Asked Questions
--------------------------

**Why do I not hear any MSX Music/MSX Audio sound?**

**Why is there no music in Aleste/FMPAC\-based games?**

For FMPAC/MSX Music, enable MSX Music emulation in System Settings; for MSX Audio, enable MSX Audio emulation. This will slow down emulation noticeably, so you may also need to increase CPU frequency and disable vertical sync. Alternatively, you can recompile the emulator with the custom engines disabled, to use Marat’s faster (though less accurate) FMPAC emulation engine. Check fMSX.mak for instructions

**Why does the emulator refuse to load a saved game?**

There are a number of reasons why this will happen; some of them are:

*   fMSX save state format has changed between versions
*   Currently set RAM/VRAM sizes do not match with RAM/VRAM settings set when the state was saved
*   System model (MSX1/2/2+) currently set does not match the system model set when the state was saved
*   System ROM’s currently in use do not match to those used when the state was saved (e.g. save state was made with Yamaha YIS ROM’s, while the system ROM’s currently in use are Phillips)
*   Any combination of the above

For all but the first reason, resetting the configuration so that it matches the configuration when state was saved will do the trick.

Compiling
---------

To compile, ensure that [zlib](svn://svn.pspdev.org/psp/trunk/zlib) and [libpng](svn://svn.pspdev.org/psp/trunk/libpng) are installed, and run make:

`make -f fMSX.mak`

To enable the alternative sound engines, `#define MSXAUDIO` and/or `MSXMUSIC` in the Makefile. To enable zipped ROM support, `#define MINIZIP`.

Version History
---------------

#### 3.5.40 (March 01 2010)

*   Improved saved state format – will automatically switch to appropriate system settings
*   PSPLIB updated to latest version
*   Images are now saved under `PSP/PHOTOS/fMSX PSP`
*   Virtual keyboard updated, option for “toggle display” mode
*   Screenshot previews in file browser

#### 3.5.35 (June 29 2008)

*   Bugfix – many games, especially MegaROM games did not load correctly in the previous version (thanks Victor)
*   Added Fast Forward button mapping – while disabled by default, you can map any button to fast-forward emulation in the Controls menu
*   Custom mapper types can now be embedded in source code. This version includes built-in support for Zanac Ex and Mon Mon Monster. Contact me if you’d like to add additional mappings

#### 3.5.3 (June 22 2008)

*   Games/applications that require MSX Audio/MSX Music will no longer crash the emulator if either is disabled in System settings – there will simply be no Y2413/YM8950 audio (PSG and SCC will still render as usual, however)
*   ROM type settings are now saved between sessions – up to 500 different ROM’s can be tracked by CRC32 value. This adds a slight loading delay when the emulator starts up, as well as a saving delay when the emulator exits (if any changes are made during the session)

#### 3.5.2 (May 30 2008)

*   FMPAC/MSX Music bugfix — corrected MSX Music/MSX Audio labeling in System settings – MSX Music now correctly refers to FMPAC/YM2413 chip; MSX Audio refers to Y8950 chip. If the emulator was previously freezing with FMPAC audio, double-check to make sure MSX Music is enabled (disable MSX Audio to gain some extra speed) and try again

#### 3.5.1 (February 29 2008)

*   PSP version brought up to par with Marat’s official version
*   Sound should now function correctly following a state load (in previous versions, sound would often stop)
*   MSX MUSIC bug – FMPAC ROM is loaded correctly (though still functions incorrectly, see next release)
*   When switching games, latest save state will be automatically highlighted

#### 3.4.5 (November 05 2007)

*   MSXMUSIC and MSXAUDIO emulation can now be toggled on and off at any time
*   FDD (Floppy Disk Drive) activity indicator – displays an icon whenever the virtual floppy drive is busy
*   Optional (and discouraged) hi-resolution renderer for screen modes 6, 7 and text-80

#### 3.4.1 (October 01 2007)

*   fMSX upgraded to version 3.4
*   Changes include fixes to the Z80 CPU emulation

#### 3.3.25 (August 23 2007)

*   This update adds the ability to write changes to (uncompressed) disk images (see section on Disk Writing in the documentation for details and warnings)

#### 3.3.2 (July 24 2007)

*   60% increase in rendering speed—MSX2 titles run at 100% speed at 222MHz
*   Fixed buffer overflow error affecting units without a battery (thanks to Alpha Beta for tracking it down)

#### 3.3.1 (June 17 2007)

*   Virtual keyboard improvements. The virtual keyboard now requires an external file – ‘msx.lyt’ in the emulator directory—the file contains keyboard layout information
*   fMSX updated to version 3.3
*   Lots of minor bug fixes

#### 3.2.1 (June 04 2007)

*   Added menu navigation options – select from US PSP navigation (X confirms, O cancels) or Japanese PSP navigation (O confirms, X cancels—also used by most homebrew emulators)
*   Added RAM/VRAM adjustment options
*   Added HBlank/VBlank period selection (PAL/NTSC)
*   Added MSX model selection (MSX1, MSX2, MSX2+)
*   Improved accuracy of frames-per-second counter
*   User interface improvements
*   fMSX updated to version 3.2
*   Rendering speed increase, switched to 15bit color (these mostly affect menu rendering)
*   Various bug fixes

#### 3.1.3 (May 05 2007)

*   Support for multi-volume disk images – multiple disk images per ZIP file and in-game volume switching
*   Improved virtual keyboard navigation, rendering speed increase
*   System ROM’s can be stored in a single ZIP file in the application’s directory
*   Integrated zipped ROM and DSK file support (fast loading from ZIP files)
*   PSG/SCC sound emulation engine by Mitsutaka Okazaki
*   Approximately 18% gain in rendering speed
*   Frames-per-second counter
*   PAL (50Hz) timing
*   Various small improvements and bugfixes

#### 3.1.2 (April 24 2007)

*   Added support for cartridge type selection in the System menu. This adds support for games like Zanac Ex, which are not detected correctly
*   Preliminary ZIP file support. Emulator will load ROM or DSK files from a ZIP file, provided there’s only a single compressed file per ZIP. System ROMs must still be uncompressed (or GZip compressed)
*   GZip file support – emulator will load cartridges and disk images from .gz compressed files. Save states are also much smaller in size
*   GUI changes, wording changes

#### 3.1.1 (April 20 2007)

*   Initial release

Credits
-------

Marat Fayzullin (fMSX)  
Simon Tatham (fixed.fd font)  
Gilles Vollant (minizip)  
Vincent van Dam (alt. sound emulation/hi-res video rendering)  
Mitsutaka Okazaki (PSG/SCC/MSXMUSIC sound emulation engine)  
Tatsuyuki Satoh (MSXAUDIO sound emulation engine)  
Ruka (PNG saving/loading code)
