PSPSDK=$(shell psp-config --pspsdk-path)
PSPLIB=libpsp

DEFINES += -DPSP -DLSB_FIRST -DBPP16 -DSOUND -DZLIB \
           -DPSP_APP_VER=\"$(PSP_APP_VER)\" -DPSP_APP_NAME="\"$(PSP_APP_NAME)\""

MINIZIP=minizip
Z80=Z80
EMULIB=EMULib
DATA=data

BUILD_Z80=$(Z80)/Z80.o $(Z80)/Debug.o
BUILD_EMULIB=$(EMULIB)/Sound.o $(EMULIB)/SndPsp.o $(EMULIB)/LibPsp.o

ifdef MINIZIP
BUILD_MINIZIP=$(MINIZIP)/ioapi.o $(MINIZIP)/unzip.o $(MINIZIP)/zip.o
DEFINES += -DMINIZIP -DBPS16
endif

INCDIR += EMULib Z80 $(PSPLIB) minizip
CFLAGS += -O2 -G0 -Wall $(DEFINES)
CXXFLAGS += $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS += $(CFLAGS)

LIBDIR += $(PSPLIB)
LIBS += -lpsplib -lpspgu -lpspaudio -lpsprtc -lpsppower -lpng -lz -lm \
        -lpspwlan -lpspnet_adhoc -lpspnet_adhocctl -lpspnet_adhocmatching
EXTRA_TARGETS=EBOOT.PBP

PSP_EBOOT_TITLE=$(PSP_APP_NAME) $(PSP_APP_VER)

export PSP_FW_VERSION

all: build_psplib
clean: clean_psplib

include $(PSPSDK)/lib/build.mak

build_psplib:
	cd $(PSPLIB) ; $(MAKE)

clean_psplib:
	cd $(PSPLIB) ; $(MAKE) clean

ifeq ($(PSP_FW_VERSION), 150)
all: kxploit
endif
