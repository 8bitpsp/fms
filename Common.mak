PSPSDK=$(shell psp-config --pspsdk-path)

DEFINES += -DPSP -DLSB_FIRST -DBPP16 -DSOUND -DZLIB \
           -DPSP_APP_VER=\"$(PSP_APP_VER)\" -DPSP_APP_NAME="\"$(PSP_APP_NAME)\""

MINIZIP=minizip
PSPLIB=psplib
Z80=Z80
EMULIB=EMULib
DATA=data

BUILD_Z80=$(Z80)/Z80.o $(Z80)/Debug.o
BUILD_PSPLIB=$(PSPLIB)/psp.o $(PSPLIB)/font.o $(PSPLIB)/image.o \
             $(PSPLIB)/video.o $(PSPLIB)/audio.o $(PSPLIB)/fileio.o \
             $(PSPLIB)/menu.o $(PSPLIB)/ui.o $(PSPLIB)/ctrl.o \
             $(PSPLIB)/kybd.o $(PSPLIB)/perf.o $(PSPLIB)/init.o \
             $(PSPLIB)/util.o
BUILD_EMULIB=$(EMULIB)/Sound.o $(EMULIB)/SndPsp.o $(EMULIB)/LibPsp.o

ifdef MINIZIP
BUILD_MINIZIP=$(MINIZIP)/ioapi.o $(MINIZIP)/unzip.o $(MINIZIP)/zip.o
DEFINES += -DMINIZIP
endif

INCDIR +=./EMULib ./Z80 ./psplib ./minizip
CFLAGS += -O2 -G0 -Wall $(DEFINES)
CXXFLAGS += $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS += $(CFLAGS)

LIBDIR +=
LIBS += -lpspgu -lpspaudio -lpsprtc -lpsppower -lpng -lz -lm
EXTRA_TARGETS=EBOOT.PBP

PSP_EBOOT_TITLE=$(PSP_APP_NAME) $(PSP_APP_VER)

include $(PSPSDK)/lib/build.mak

include $(PSPLIB)/build.mak
