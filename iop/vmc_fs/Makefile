IOP_OBJS = vmc_fs.o vmc_io.o vmc_misc.o vmc_fat.o vmc_ps2.o imports.o
IOP_BIN_DIR = ../
IOP_BIN = vmc_fs.irx

IOP_CFLAGS  += -Wall -fno-builtin
IOP_LDFLAGS += -s
PROFILING ?= 0
ifneq ($(PROFILING), 0)
  IOP_CFLAGS += -DPROFILING=$(PROFILING)
endif

all: $(IOP_BIN)

clean:
	rm -f $(IOP_OBJS) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include $(IOP_BIN_DIR)Rules.make
