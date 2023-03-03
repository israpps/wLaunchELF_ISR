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
SIOR ?= 0
# ----------------------------- #
.SILENT:

BIN_NAME = BOOT$(HAS_EXFAT)$(HAS_DS34)$(HAS_ETH)$(HAS_IOP_RESET)$(HAS_SMB)$(HAS_DVRP)$(HAS_XFROM)$(HAS_MX4SIO)$(HAS_EESIO)
EE_BIN = UNC-$(BIN_NAME).ELF
EE_BIN_PKD = $(BIN_NAME).ELF
EE_OBJS = main.o config.o elf.o draw.o loader_elf.o filer.o \
	poweroff_irx.o iomanx_irx.o filexio_irx.o ps2atad_irx.o ps2dev9_irx.o\
	ps2hdd_irx.o ps2fs_irx.o usbd_irx.o mcman_irx.o mcserv_irx.o\
	cdvd_irx.o vmc_fs_irx.o ps2kbd_irx.o\
	hdd.o hdl_rpc.o hdl_info_irx.o editor.o timer.o jpgviewer.o icon.o lang.o\
	font_uLE.o makeicon.o chkesr.o allowdvdv_irx.o

EE_INCS := -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include -Ioldlibs/libcdvd/ee

EE_LDFLAGS := -L$(PS2DEV)/gsKit/lib -L$(PS2SDK)/ports/lib -Loldlibs/libcdvd/lib -s
EE_LIBS = -lgskit -ldmakit -ljpeg -lmc -lhdd -lcdvdfs -lkbd -lmf \
	-lcdvd -lc -lfileXio -lpatches -lpoweroff -ldebug -lc
EE_CFLAGS := -mgpopt -G10240 -G0 -DNEWLIB_PORT_AWARE -D_EE

BIN2S = @bin2s

ifeq ($(SMB),1)
    EE_OBJS += smbman.o
    HAS_SMB = -SMB
    EE_CFLAGS += -DSMB
endif

ifeq ($(XFROM),1)
    HAS_XFROM = -XFROM
    EE_CFLAGS += -DXFROM
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
    EE_OBJS += mx4sio_bd.o
    EE_CFLAGS += -DMX4SIO
    HAS_MX4SIO = -MX4SIO
    SIO2MAN = 1
endif

ifeq ($(SIO2MAN),1)
    EE_OBJS += sio2man.o padman.o
    EE_CFLAGS += -DHOMEBREW_SIO2MAN
    EE_LIBS += -lpadx
else
    EE_LIBS += -lpad
endif

ifeq ($(SIO_DEBUG),1)
    EE_CFLAGS += -DSIO_DEBUG
    EE_OBJS += sior_irx.o
    HAS_EESIO = -SIO_DEBUG
	ifeq ($(SIOR),1)
        EE_LIBS += -lsior
        EE_CFLAGS += -DSIOR
	endif
endif

ifeq ($(IOP_RESET),0)
    EE_CFLAGS += -DNO_IOP_RESET
    HAS_IOP_RESET = -NO_IOP_RESET
endif

ifeq ($(ETH),1)
    EE_OBJS += ps2smap_irx.o ps2ftpd_irx.o ps2host_irx.o ps2netfs_irx.o ps2ip_irx.o
    EE_CFLAGS += -DETH
	ifeq ($(UDPTTY),1)
	  EE_OBJS += udptty.o
	  HAS_UDPTTY = -UDPTTY
	  EE_CFLAGS += -DUDPTTY
	endif
else
    HAS_ETH = -NO_NETWORK
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


EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%) # remap all EE_OBJ to obj subdir

.PHONY: all run reset clean rebuild

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

current_flags:
	@echo "SMB: set to 1 to build wLe with smb support"
	@echo "DEFAULT_COLORS - set to 1 to use default uLaunchELF colors, otherwise, custom values will be used"
	@echo "TMANIP: set to 1 to compile with time manipulation function, if set to 2 the function will manipulate the date of a specific folder (to avoid issues caused by noobs) (the specific folder name used is the macro HACK_FOLDER, wich is defined at launchelf.h)"
	@echo "LANG: use a custom language file to compile wLe (by now only SPA and ENG are available)"
	@echo "DVRP: support for PSX DESR encrypted HDD area"

clean:
	$(MAKE) -C hdl_info clean
	$(MAKE) -C ps2host clean
	$(MAKE) -C loader clean
	$(MAKE) -C vmc_fs clean
	$(MAKE) -C AllowDVDV clean
	$(MAKE) -C oldlibs/libcdvd clean
	$(MAKE) -C oldlibs/ps2ftpd clean
	rm -f githash.h *.s *.o $(EE_BIN) $(EE_BIN_PKD)

rebuild: clean all


$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	@echo " CC  - $@"
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o: $(EE_ASM_DIR)%.s | $(EE_OBJS_DIR)
	@echo " ASM - $@"
	$(EE_AS) $(EE_ASFLAGS) $< -o $@

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.cpp | $(EE_OBJS_DIR)
	@echo " CXX - $@"
	$(EE_CXX) $(EE_CXXFLAGS) $(EE_INCS) -c $< -o $@


include embed.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
