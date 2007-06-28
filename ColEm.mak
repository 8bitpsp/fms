COLEM=ColEm

DEFINES=-DCOLEM

BUILD_COLEM=$(EMULIB)/SN76489.o $(EMULIB)/TMS9918.o $(EMULIB)/DRV9918.o \
            $(COLEM)/Coleco.o $(COLEM)/MenuPsp.o $(COLEM)/Psp.o $(COLEM)/ColEm.o

OBJS=$(BUILD_MINIZIP) $(BUILD_PSPLIB) $(BUILD_EMULIB) $(BUILD_Z80) \
     $(BUILD_COLEM)

TARGET=colempsp
PSP_EBOOT_TITLE=ColEm PSP 2.2.1
PSP_EBOOT_ICON=data/colem-icon.png

include Common.mak
