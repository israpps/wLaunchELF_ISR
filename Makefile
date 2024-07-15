#.SILENT:

# ---{ BUILD CONFIGURATION }--- #
SIO_DEBUG ?= 0
DS34 ?= 0
SMB ?= 0
TMANIP ?= 1
ETH ?= 1
EXFAT ?= 0
DVRP ?= 0
IOP_RESET ?= 1
XFROM ?= 0
UDPTTY ?= 0
MX4SIO ?= 0
SIO2MAN ?= 0
PPC_UART ?= 0
SD2PSX ?= 0
DEBUG ?= 0
LCDVD ?= LATEST#or LEGACY
# ----------------------------- #
.SILENT:

BIN_NAME = $(HAS_EXFAT)$(HAS_DS34)$(HAS_ETH)$(HAS_SMB)$(HAS_DVRP)$(HAS_XFROM)$(HAS_MX4SIO)$(HAS_SD2PSX)$(HAS_EESIO)$(HAS_UDPTTY)$(HAS_PPCTTY)$(HAS_IOP_RESET)
ifeq ($(DEBUG), 0)
  EE_BIN = UNC-BOOT$(BIN_NAME).ELF
  EE_BIN_PKD = BOOT$(BIN_NAME).ELF
else
  EE_BIN = UNC-BOOT.ELF
  EE_BIN_PKD = BOOT.ELF
endif
EE_OBJS = main.o config.o elf.o draw.o loader_elf.o filer.o \
	poweroff_irx.o iomanx_irx.o filexio_irx.o ps2atad_irx.o ps2dev9_irx.o \
	ps2hdd_irx.o ps2fs_irx.o usbd_irx.o mcman_irx.o mcserv_irx.o \
	cdvd_irx.o vmc_fs_irx.o ps2kbd_irx.o \
	hdd.o hdl_rpc.o hdl_info_irx.o editor.o timer.o jpgviewer.o icon.o lang.o \
	font_uLE.o makeicon.o chkesr.o allowdvdv_irx.o

EE_INCS := -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include -Iinclude

EE_LDFLAGS := -L$(PS2DEV)/gsKit/lib -L$(PS2SDK)/ports/lib -Liop/oldlibs/libcdvd/lib -s
EE_LIBS = -lgskit -ldmakit -ljpeg -lmc -lhdd -lkbd -lmf \
	-lcdvd -lc -lfileXio -lpatches -lpoweroff -ldebug
EE_CFLAGS := -mgpopt -G10240 -G0 -DNEWLIB_PORT_AWARE -D_EE

BIN2S = @bin2s

ifeq ($(SMB),1)
    EE_OBJS += smbman.o
    HAS_SMB = -SMB
    EE_CFLAGS += -DSMB
endif

ifeq ($(LCDVD),LEGACY)
  $(info -- Building with legacy libcdvd)
  EE_CFLAGS += -DLIBCDVD_LEGACY
  CDVD_SOURCE = iop/cdvd.irx
else
  EE_INCS += -Iiop/oldlibs/libcdvd/ee
  EE_LIBS += -lcdvdfs
  EE_CFLAGS += -DLIBCDVD_LATEST
  CDVD_SOURCE = iop/__precompiled/cdfs.irx
endif

ifeq ($(XFROM),1)
    HAS_XFROM = -XFROM
    GENERIC_READDIR = 1
    EE_CFLAGS += -DXFROM
endif

ifeq ($(SD2PSX),1)
    HAS_SD2PSX = -SD2PSX
    GENERIC_READDIR = 1
    EE_CFLAGS += -DSUPPORT_SPECIAL_MEMCARDS
    EE_OBJS += sd2psxman.o 
endif

ifeq ($(DS34),1)
    EE_OBJS += ds34usb.o libds34usb.a ds34bt.o libds34bt.a pad_ds34.o 
    HAS_DS34 = -DS34
    EE_CFLAGS += -DDS34
else
    EE_OBJS += pad.o
endif

ifeq ($(DVRP),1)
    EE_OBJS += dvrdrv_irx.o dvrfile_irx.o
    EE_CFLAGS += -DDVRP
    HAS_DVRP = -DVRP
endif

ifeq ($(MX4SIO),1)
    ifneq ($(EXFAT),1)
        $(error MX4SIO Requested on build without BDM)
    else
        EE_OBJS += mx4sio_bd.o
        EE_CFLAGS += -DMX4SIO
        HAS_MX4SIO = -MX4SIO
        SIO2MAN = 1
    endif
endif

ifeq ($(SIO2MAN),1)
    EE_OBJS += sio2man.o padman.o
    EE_CFLAGS += -DHOMEBREW_SIO2MAN
    EE_LIBS += -lpadx
else
    EE_LIBS += -lpad
endif

ifeq ($(GENERIC_READDIR),1)
    EE_CFLAGS += -DGENERIC_READDIR
endif

ifeq ($(SIO_DEBUG),1)
    EE_CFLAGS += -DSIO_DEBUG
    HAS_EESIO = -SIO_DEBUG
endif

ifeq ($(PPC_UART),1)
    EE_CFLAGS += -DPOWERPC_UART
    HAS_PPCTTY = -PPCTTY
    EE_OBJS += ppctty.o
    ifeq ($(UDPTTY),1)
    $(error Both PPCTTY and UDPTTY enabled simultaneously)
    endif
endif

ifeq ($(IOP_RESET),0)
    EE_CFLAGS += -DNO_IOP_RESET
    HAS_IOP_RESET = -NO_IOP_RESET
endif

ifeq ($(ETH),1)
    EE_OBJS += ps2smap_irx.o ps2ftpd_irx.o ps2host_irx.o ps2netfs_irx.o ps2ip_irx.o
    EE_CFLAGS += -DETH
else
    HAS_ETH = -NO_NETWORK
endif

ifeq ($(UDPTTY),1)
    ifneq ($(ETH),1)
    $(error UDPTTY requested on build without network modules)
    else
      EE_OBJS += udptty.o
      HAS_UDPTTY = -UDPTTY
      EE_CFLAGS += -DUDPTTY -DCOMMON_PRINTF
    endif
endif

ifeq ($(TMANIP),1)
    EE_CFLAGS += -DTMANIP
endif

ifeq ($(TMANIP),2)
    EE_CFLAGS += -DTMANIP
    EE_CFLAGS += -DTMANIP_MORON
endif

ifeq ($(EXFAT),1)
    EE_OBJS += bdm_irx.o bdmfs_fatfs_irx.o usbmass_bd_irx.o
    EE_CFLAGS += -DEXFAT
    HAS_EXFAT = -EXFAT
else
    EE_OBJS += usbhdfsd_irx.o
endif

ifeq ($(DEFAULT_COLORS),1)
@echo using default colors
else
EE_CFLAGS += -DCUSTOM_COLORS
endif

ifeq ($(IOPTRAP),1)
    EE_OBJS += ioptrap_irx.o
    EE_CFLAGS += -DIOPTRAP
endif


EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/
EE_SRC_DIR = src/
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%) # remap all EE_OBJ to obj subdir

.PHONY: all run reset clean rebuild isoclean iso

all: githash.h $(EE_BIN_PKD)

$(EE_BIN_PKD): $(EE_BIN)
	ps2-packer $< $@
ifeq ($(IOP_RESET),0)
	@echo "-------------{COMPILATION PERFORMED WITHOUT IOP RESET}-------------"
endif

$(EE_OBJS_DIR):
	mkdir $@

$(EE_ASM_DIR):
	mkdir $@

run: all
	ps2client -h 192.168.0.10 -t 1 execee host:$(EE_BIN)
reset: clean
	ps2client -h 192.168.0.10 reset

githash.h:
	printf '#ifndef ULE_VERDATE\n#define ULE_VERDATE "' > $@ && \
	git show -s --format=%cd --date=local | tr -d "\n" >> $@ && \
	printf '"\n#endif\n' >> $@
	printf '#ifndef GIT_HASH\n#define GIT_HASH "' >> $@ && \
	git rev-parse --short HEAD | tr -d "\n" >> $@ && \
	printf '"\n#endif\n' >> $@

clean:
	$(MAKE) -C loader clean
	$(MAKE) -C iop/hdl_info clean
	$(MAKE) -C iop/ps2host clean
	$(MAKE) -C iop/vmc_fs clean
	$(MAKE) -C iop/AllowDVDV clean
	$(MAKE) -C iop/oldlibs/libcdvd clean
	$(MAKE) -C iop/oldlibs/ps2ftpd clean
	@rm -f githash.h $(EE_BIN) $(EE_BIN_PKD)
	@rm -rf $(EE_OBJS_DIR)
	@rm -rf $(EE_ASM_DIR)
	@rm -f iop/*.irx

rebuild: clean all

info:
	$(info -------- wLaunchELF 4.43x_isr --------)
	$(info EE_BIN = $(EE_BIN))
	$(info EE_BIN_PKD = $(EE_BIN_PKD))
	$(info EE_OBJS = $(EE_OBJS))
	$(info TMANIP=$(TMANIP), SIO_DEBUG=$(SIO_DEBUG), DS34=$(DS34), ETH=$(ETH))
	$(info EXFAT=$(EXFAT), XFROM=$(XFROM), UDPTTY=$(UDPTTY), MX4SIO=$(MX4SIO))
	$(info IOP_RESET=$(IOP_RESET))

#special recipe for compiling and dumping obj to subfolder
$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	@echo -e "\033[1m CC  - $@\033[0m"
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o: $(EE_ASM_DIR)%.s | $(EE_OBJS_DIR)
	@echo -e "\033[1m ASM - $@\033[0m"
	$(EE_AS) $(EE_ASFLAGS) $< -o $@

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.cpp | $(EE_OBJS_DIR)
	@echo -e "\033[1m CXX - $@\033[0m"
	$(EE_CXX) $(EE_CXXFLAGS) $(EE_INCS) -c $< -o $@
#'\033[1m \033[0m'

include embed.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
