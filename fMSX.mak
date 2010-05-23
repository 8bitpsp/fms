# Comment out to completely disable MSX Audio
MSXAUDIO=fMSX/msxaudio
# Comment out to use Marat's faster, less accurate engine
MSXMUSIC=fMSX/msxmusic
FMSX=fMSX
PSP_APP_NAME=fMSX PSP
PSP_APP_VER=3.5.41

PSP_FW_VERSION=200

DEFINES=-DFMSX -DDISK

ifdef MSXAUDIO
ifdef MSXMUSIC
BUILD_MSXMUSIC=$(MSXMUSIC)/emu2413.o $(MSXMUSIC)/emu2212.o $(MSXMUSIC)/emu2149.o
BUILD_MSXAUDIO=$(MSXAUDIO)/fmopl.o $(MSXAUDIO)/ymdeltat.o
BUILD_SOUNDLIB=$(BUILD_MSXMUSIC) $(BUILD_MSXAUDIO)
DEFINES += -DALTSOUND
endif
endif

BUILD_FMSX=$(FMSX)/I8251.o $(EMULIB)/I8255.o $(EMULIB)/SCC.o $(EMULIB)/WD1793.o \
           $(EMULIB)/AY8910.o $(EMULIB)/YM2413.o $(FMSX)/V9938.o \
           $(FMSX)/Patch.o $(FMSX)/MSX.o $(EMULIB)/Floppy.o $(EMULIB)/FDIDisk.o \
           $(FMSX)/Psp.o $(FMSX)/MenuPsp.o $(FMSX)/fMSX.o
OBJS=$(BUILD_MINIZIP) $(BUILD_EMULIB) $(BUILD_SOUNDLIB) \
     $(BUILD_Z80) $(BUILD_FMSX)

INCDIR += $(MSXAUDIO) $(MSXMUSIC)

TARGET=fmsxpsp
PSP_EBOOT_ICON=data/fmsx-icon.png

include Common.mak
