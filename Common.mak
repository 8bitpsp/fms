PSPSDK=$(shell psp-config --pspsdk-path)

DEFINES += -DPSP -DLSB_FIRST -DBPP16 -DSOUND -DZLIB

MINIZIP=minizip
PSPLIB=psplib
Z80=Z80
EMULIB=EMULib
DATA=data

BUILD_Z80=$(Z80)/Z80.o $(Z80)/Debug.o
BUILD_PSPLIB=$(PSPLIB)/psp.o $(PSPLIB)/font.o $(PSPLIB)/image.o \
             $(PSPLIB)/video.o $(PSPLIB)/audio.o $(PSPLIB)/fileio.o \
             $(PSPLIB)/menu.o $(PSPLIB)/ui.o $(PSPLIB)/ctrl.o \
             $(PSPLIB)/kybd.o $(PSPLIB)/perf.o
BUILD_EMULIB=$(EMULIB)/Sound.o $(EMULIB)/SndPsp.o \
             $(EMULIB)/INIFile.o $(EMULIB)/LibPsp.o

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

include $(PSPSDK)/lib/build.mak

#PSPLIB dependencies

$(PSPLIB)/audio.o:  $(PSPLIB)/audio.c $(PSPLIB)/audio.h
$(PSPLIB)/perf.o:   $(PSPLIB)/perf.c $(PSPLIB)/perf.h
$(PSPLIB)/ctrl.o:   $(PSPLIB)/ctrl.c $(PSPLIB)/ctrl.h
$(PSPLIB)/fileio.o: $(PSPLIB)/fileio.c $(PSPLIB)/fileio.h
$(PSPLIB)/font.o:   $(PSPLIB)/font.c $(PSPLIB)/font.h \
                    $(PSPLIB)/stockfont.h
$(PSPLIB)/image.o:  $(PSPLIB)/image.c $(PSPLIB)/image.h
$(PSPLIB)/kybd.o:   $(PSPLIB)/kybd.c $(PSPLIB)/kybd.h \
                    $(PSPLIB)/ctrl.c $(PSPLIB)/video.c \
                    $(PSPLIB)/image.c $(PSPLIB)/font.c
$(PSPLIB)/menu.o:   $(PSPLIB)/menu.c $(PSPLIB)/menu.h
$(PSPLIB)/psp.o:    $(PSPLIB)/psp.c $(PSPLIB)/psp.h \
                    $(PSPLIB)/fileio.c
$(PSPLIB)/ui.o:     $(PSPLIB)/ui.c $(PSPLIB)/ui.h \
                    $(PSPLIB)/video.c $(PSPLIB)/menu.c \
                    $(PSPLIB)/psp.c $(PSPLIB)/fileio.c \
                    $(PSPLIB)/ctrl.c
$(PSPLIB)/video.o:  $(PSPLIB)/video.c $(PSPLIB)/video.h \
                    $(PSPLIB)/font.c $(PSPLIB)/image.c
$(PSPLIB)/stockfont.h: $(DATA)/genfont $(PSPLIB)/stockfont.fd
	$< < $(word 2,$^) > $@
$(DATA)/genfont: $(PSPLIB)/genfont.c
	cc $< -o $@
