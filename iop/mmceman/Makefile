$(info -- MMCEMAN)
#Build full MMCEMAN for use with loader / homebrew
PS2SDKSRC ?= $(PS2SDK)
IOP_BIN ?= mmceman.irx
IOP_SRC_DIR ?= src/
IOP_INC_DIR ?= include/
IOP_WARNFLAGS ?= -Wall

IOP_OBJS = ioplib.o sio2man_hook.o mmce_sio2.o mmce_cmds.o mmce_fs.o imports.o mmceman.o
IOP_IMPORT_INCS += \
	sio/sio2man \
	system/dmacman \
	system/intrman \
	system/iomanx \
	system/loadcore \
	system/stdio \
	system/sysclib \
	system/threadman

#Needed for iomanX
IOP_CFLAGS += -DIOMANX_OLD_NAME_COMPATIBILITY=0 -DIOMANX_OLD_NAME_ADDDELDRV=0

all: $(IOP_BIN)

clean:
	rm -f -r $(IOP_OBJS_DIR) $(IOP_BIN) $(IOP_BIN:.irx=.notiopmod.elf) $(IOP_BIN:.irx=.notiopmod.stripped.elf)

ifeq ($(DEBUG), 1)
  $(info -- MMCEMAN debug enabled)
  IOP_CFLAGS += -DDEBUG
  IOP_CFLAGS += -DDEBUG=1
endif

include $(PS2SDKSRC)/Defs.make
include $(PS2SDKSRC)/iop/Rules.make
