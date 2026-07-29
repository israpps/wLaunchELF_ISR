#.SILENT:

# ---{ BUILD CONFIGURATION }--- #
MMCE ?= 1
DS34 ?= 0
TMANIP ?= 1
ETH ?= 0
UDPFS ?= 1
EXFAT ?= 1
DVRP ?= 1
IOP_RESET ?= 1
XFROM ?= 1
UDPTTY ?= 0
MX4SIO ?= 1
SIO2MAN ?= 1
PPC_UART ?= 0
SIO_DEBUG ?= 0
DEBUG ?= 0
LCDVD ?= LATEST#or LEGACY
# ----------------------------- #
.SILENT:

BIN_NAME = $(HAS_EXFAT)$(HAS_DS34)$(HAS_ETH)$(HAS_UDPFS)$(HAS_MX4SIO)$(HAS_MMCE)$(HAS_DVRP)$(HAS_XFROM)$(HAS_EESIO)$(HAS_UDPTTY)$(HAS_PPCTTY)$(HAS_IOP_RESET)
ifeq ($(DEBUG), 0)
  EE_BIN = UNC-BOOT$(BIN_NAME).ELF
  EE_BIN_PKD = BOOT$(BIN_NAME).ELF
else
  EE_BIN = UNC-BOOT.ELF
  EE_BIN_PKD = BOOT.ELF
endif
EE_OBJS = main.o main_actions.o main_boot.o main_modules.o main_menu.o main_title.o main_info_screens.o main_console_info.o main_gameid.o main_gameid_table.o main_history.o init.o main_startup.o main_fileops.o main_exploit_installer.o config.o config_screen.o config_startup.o config_network.o config_advanced.o gui.o gui_colors.o virtual_keyboard.o elf.o draw.o draw_gs.o draw_text.o loader_elf.o filer.o filer_device.o filer_mount.o filer_fileops.o filer_actions.o filer_browser.o filer_copy.o \
	gui_sort.o gui_texteditor.o gui_hdd0_format.o psu_functions.o \
	poweroff_irx.o iomanx_irx.o filexio_irx.o ps2dev9_irx.o dev9_poweroff_irx.o \
	ps2hdd_irx.o ps2fs_irx.o usbd_irx.o mcman_irx.o mcserv_irx.o \
	cdvd_irx.o xparam_irx.o vmcman_irx.o ps2kbd_irx.o \
	hdd.o hdd_header_injector.o hdl_rpc.o hdl_info_irx.o editor.o editor_menu.o editor_input.o editor_rules.o editor_file.o timer.o icon.o lang.o popstarter.o popstarter_path.o popstarter_vcd.o popstarter_gameid.o popstarter_elf.o \
	font_uLE.o makeicon.o chkesr.o allowdvdv_irx.o \
	exploit_system_xlf_header.o exploit_xsystem_xlf_header.o exploit_dtl_system_xlf_header.o exploit_xlf_body.o \
	exploit_osdsys_kernel.o exploit_osd110_kernel.o \
	exploit_jpn_sys.o exploit_usa_sys.o exploit_eur_sys.o exploit_chn_sys.o \
	exploit_list_icn.o \
	secrsif_irx.o exploit_ioprp_img.o

EE_INCS := -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include -Iinclude

EE_LDFLAGS := -L$(PS2DEV)/gsKit/lib -L$(PS2SDK)/ports/lib -Liop/oldlibs/libcdvd/lib -s -Wl,--no-warn-mismatch
EE_MATH_LIB := -lm
ifneq ($(wildcard $(PS2SDK)/ee/lib/libmf.a),)
EE_MATH_LIB := -lmf
endif

EE_LIBS = -lgskit -ldmakit -lmc -lhdd -lkbd $(EE_MATH_LIB) \
			-lcdvd -lc -lfileXio -lpatches -lpoweroff -ldebug
EE_CFLAGS := -mgpopt -G10240 -G0 -DNEWLIB_PORT_AWARE -D_EE

ifneq ($(DEBUG), 0)
    EE_CFLAGS += -DULE_DEBUG_BUILD
endif

# Locate bin2s across old/new PS2SDK layouts and PATH.
BIN2S_TOOL ?=
ifneq ($(wildcard $(PS2SDK)/bin/bin2s),)
BIN2S_TOOL := $(PS2SDK)/bin/bin2s
else ifneq ($(wildcard $(PS2DEV)/bin/bin2s),)
BIN2S_TOOL := $(PS2DEV)/bin/bin2s
else ifneq ($(wildcard /usr/local/ps2dev/bin/bin2s),)
BIN2S_TOOL := /usr/local/ps2dev/bin/bin2s
else ifneq ($(wildcard $(CURDIR)/scripts/bin2s-fallback.sh),)
BIN2S_TOOL := sh $(CURDIR)/scripts/bin2s-fallback.sh
else
BIN2S_TOOL := $(shell command -v bin2s 2>/dev/null)
endif
ifeq ($(strip $(BIN2S_TOOL)),)
$(error bin2s not found. Install PS2SDK tools, or set BIN2S_TOOL=/full/path/to/bin2s)
endif
BIN2S = @$(BIN2S_TOOL)

ifeq ($(LCDVD),LEGACY)
  $(info -- Building with legacy libcdvd)
  EE_CFLAGS += -DLIBCDVD_LEGACY
  CDVD_SOURCE = iop/cdvd.irx
  EE_INCS += -Iiop/oldlibs/libcdvd/ee
  EE_LIBS += -lcdvdfs
else
  EE_CFLAGS += -DLIBCDVD_LATEST
  CDVD_SOURCE = iop/__precompiled/cdfs.irx
  ifneq ($(wildcard $(PS2SDK)/iop/irx/cdfs.irx),)
    CDVD_SOURCE = $(PS2SDK)/iop/irx/cdfs.irx
  endif
endif
SECR_LIB_SOURCE := thirdparty/ps2sdk-master/ee/rpc/secr/src/libsecr.c
IOPREBOOT_INC_DIR := $(PS2SDK)/ee/iopreboot/include
ifneq ($(wildcard thirdparty/ps2sdk-master/ee/iopreboot/include/iopcontrol_special.h),)
    IOPREBOOT_INC_DIR := thirdparty/ps2sdk-master/ee/iopreboot/include
endif
EE_INCS += -Ithirdparty/ps2sdk-master/ee/rpc/secr/include -Ithirdparty/ps2sdk-master/common/include -I$(IOPREBOOT_INC_DIR)
EE_LIBS += -liopreboot
ifneq ($(wildcard $(PS2SDK)/ee/lib/libsecr.a),)
    EE_LIBS += -lsecr
else ifneq ($(wildcard $(SECR_LIB_SOURCE)),)
    EE_OBJS += libsecr.o
else
    $(error Missing libsecr. Update PS2SDK or provide thirdparty/ps2sdk-master)
endif

ifeq ($(XFROM),1)
    HAS_XFROM = -XFROM
    EE_CFLAGS += -DXFROM
    EE_OBJS += xfromman_irx.o xfromserv_irx.o extflash_irx.o
endif

ifeq ($(DS34),1)
    EE_OBJS += ds34usb.o libds34usb.a ds34bt.o libds34bt.a pad_ds34.o 
    HAS_DS34 = -DS34
    EE_CFLAGS += -DDS34
else
    EE_OBJS += pad.o
endif

ifeq ($(DVRP),1)
    EE_OBJS += ps2atad_irx.o dvrdrv_irx.o dvrfile_irx.o
    EE_CFLAGS += -DDVRP
    HAS_DVRP = -DVRP
endif

ifeq ($(MMCE),1)
    EE_OBJS += mmceman_irx.o
    EE_CFLAGS += -DMMCE
    HAS_MMCE = -MMCE
    SIO2MAN = 1
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
ifeq ($(UDPFS),1)
    HAS_ETH = -ETH
endif
endif

ifeq ($(UDPFS),1)
    EE_OBJS += udpfs_smap_irx.o udpfs_ministack_irx.o udpfs_ioman_irx.o
    EE_CFLAGS += -DUDPFS
    HAS_UDPFS = -UDPFS
endif

ifneq ($(ETH),1)
ifneq ($(UDPFS),1)
    HAS_ETH = -NO_NETWORK
endif
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
    EE_OBJS += bdm_irx.o bdmfs_fatfs_irx.o usbmass_bd_irx.o ata_bd_irx.o
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
DS34_OFF_OBJ_DIR = obj-ds34-off/
DS34_OFF_ASM_DIR = asm-ds34-off/
DS34_ON_OBJ_DIR = obj-ds34-on/
DS34_ON_ASM_DIR = asm-ds34-on/
NO_PSX_NO_DS34_OBJ_DIR = obj-no-psx-no-ds34/
NO_PSX_NO_DS34_ASM_DIR = asm-no-psx-no-ds34/
NO_PSX_DS34_OBJ_DIR = obj-no-psx-ds34/
NO_PSX_DS34_ASM_DIR = asm-no-psx-ds34/
PSX_NO_DS34_OBJ_DIR = obj-psx-no-ds34/
PSX_NO_DS34_ASM_DIR = asm-psx-no-ds34/
PSX_DS34_OBJ_DIR = obj-psx-ds34/
PSX_DS34_ASM_DIR = asm-psx-ds34/
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%) # remap all EE_OBJ to obj subdir

all: githash.h $(EE_BIN_PKD)
all-ds34-off:
	$(MAKE) all DS34=0 EE_OBJS_DIR=$(DS34_OFF_OBJ_DIR) EE_ASM_DIR=$(DS34_OFF_ASM_DIR)

all-ds34-on:
	$(MAKE) all DS34=1 EE_OBJS_DIR=$(DS34_ON_OBJ_DIR) EE_ASM_DIR=$(DS34_ON_ASM_DIR)

all-ds34-variants: all-ds34-off all-ds34-on

all-no-psx-no-ds34:
	$(MAKE) all DS34=0 DVRP=0 XFROM=0 EE_OBJS_DIR=$(NO_PSX_NO_DS34_OBJ_DIR) EE_ASM_DIR=$(NO_PSX_NO_DS34_ASM_DIR)

all-no-psx-ds34:
	$(MAKE) all DS34=1 DVRP=0 XFROM=0 EE_OBJS_DIR=$(NO_PSX_DS34_OBJ_DIR) EE_ASM_DIR=$(NO_PSX_DS34_ASM_DIR)

all-psx-no-ds34:
	$(MAKE) all DS34=0 DVRP=1 XFROM=1 EE_OBJS_DIR=$(PSX_NO_DS34_OBJ_DIR) EE_ASM_DIR=$(PSX_NO_DS34_ASM_DIR)

all-psx-ds34:
	$(MAKE) all DS34=1 DVRP=1 XFROM=1 EE_OBJS_DIR=$(PSX_DS34_OBJ_DIR) EE_ASM_DIR=$(PSX_DS34_ASM_DIR)

all-ci-variants: all-psx-no-ds34 all-psx-ds34

# CI profile helpers (kept aligned with .github/workflows/compile.yml).
CI_RESOLVE_SCRIPT := scripts/ci/resolve_make_args.sh
CI_STALE_AUDIT_SCRIPT := scripts/stale_code_audit.sh
CI_PSX_PROFILE ?= psx
CI_PAD_PROFILE ?= no-ds34
CI_STORAGE_PROFILE ?= all

ci-build-profile:
	@FLAGS=`$(CI_RESOLVE_SCRIPT) "$(CI_PSX_PROFILE)" "$(CI_PAD_PROFILE)" "$(CI_STORAGE_PROFILE)" "$(DEBUG)" "$(PPC_UART)"`; \
	echo "Resolved build profile flags: $$FLAGS"; \
	$(MAKE) rebuild $$FLAGS

ci-build-storage-matrix:
	$(MAKE) ci-build-profile CI_PSX_PROFILE=psx CI_PAD_PROFILE=no-ds34 CI_STORAGE_PROFILE=usb
	$(MAKE) ci-build-profile CI_PSX_PROFILE=psx CI_PAD_PROFILE=no-ds34 CI_STORAGE_PROFILE=mmce
	$(MAKE) ci-build-profile CI_PSX_PROFILE=psx CI_PAD_PROFILE=no-ds34 CI_STORAGE_PROFILE=mx4sio
	$(MAKE) ci-build-profile CI_PSX_PROFILE=psx CI_PAD_PROFILE=no-ds34 CI_STORAGE_PROFILE=minimal

stale-audit:
	$(CI_STALE_AUDIT_SCRIPT) build

info:
	$(info available build options:)
	$(info   EXFAT		enable BDM and EXFAT support for it)
	$(info   ETH		include host/netfs network stack)
	$(info   UDPFS		include UDPFS network stack)
	$(info   DS34		include PS3/PS4 controller support)
	$(info   MX4SIO		support for SDCard connected to memory card slot 2)
	$(info   MMCE		support for direct SDCard access on SD2PSX or memcardpro2)
	$(info ----------)
	$(info   IOPTRAP		load exception handler module to IOP)
	$(info   UDPTTY		transfer stdout to UDP broadcast)
	$(info   PPC_UART	transfer stdout to DECKARD UART)
	$(info   SIO_DEBUG 	transfer EE stdout to EE UART)
	$(info ----------)
	$(info build shortcuts:)
	$(info   all-ds34-off		builds without DS34 in isolated obj dirs)
	$(info   all-ds34-on		builds with DS34 in isolated obj dirs)
	$(info   all-ds34-variants	builds both DS34 variants)
	$(info   all-ci-variants	builds PSX DS34 off/on artifacts)
	$(info   ci-build-profile	uses scripts/ci/resolve_make_args.sh profiles)
	$(info   ci-build-storage-matrix	quick storage-stack coverage build set)
	$(info   stale-audit		generates build/stale-code-report.txt)

.PHONY: all all-ds34-off all-ds34-on all-ds34-variants all-no-psx-no-ds34 all-no-psx-ds34 all-psx-no-ds34 all-psx-ds34 all-ci-variants ci-build-profile ci-build-storage-matrix stale-audit clean-ds34-variants clean-ci-variants run reset clean rebuild isoclean iso

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
	$(MAKE) -C iop/AllowDVDV clean
	$(MAKE) -C iop/oldlibs/libcdvd clean
	$(MAKE) -C iop/oldlibs/ps2ftpd clean
	$(MAKE) -C iop/ps2hdd_osd clean
	$(MAKE) -C iop/dev9_poweroff clean
	@rm -f githash.h $(EE_BIN) $(EE_BIN_PKD)
	@rm -rf $(EE_OBJS_DIR)
	@rm -rf $(EE_ASM_DIR)
	@rm -rf iop/__generated
	@rm -f iop/*.irx

clean-ds34-variants:
	@rm -rf $(DS34_OFF_OBJ_DIR) $(DS34_OFF_ASM_DIR) $(DS34_ON_OBJ_DIR) $(DS34_ON_ASM_DIR)

clean-ci-variants:
	@rm -rf $(NO_PSX_NO_DS34_OBJ_DIR) $(NO_PSX_NO_DS34_ASM_DIR) $(NO_PSX_DS34_OBJ_DIR) $(NO_PSX_DS34_ASM_DIR) $(PSX_NO_DS34_OBJ_DIR) $(PSX_NO_DS34_ASM_DIR) $(PSX_DS34_OBJ_DIR) $(PSX_DS34_ASM_DIR)

rebuild: clean all

info2:
	$(info -------- wLaunchELF 4.50_R3Z --------)
	$(info EE_BIN = $(EE_BIN))
	$(info EE_BIN_PKD = $(EE_BIN_PKD))
	$(info EE_OBJS = $(EE_OBJS))
	$(info TMANIP=$(TMANIP), SIO_DEBUG=$(SIO_DEBUG), DS34=$(DS34), ETH=$(ETH), UDPFS=$(UDPFS))
	$(info EXFAT=$(EXFAT), XFROM=$(XFROM), UDPTTY=$(UDPTTY), MX4SIO=$(MX4SIO))
	$(info MMCE=$(MMCE), IOP_RESET=$(IOP_RESET))

#special recipe for compiling and dumping obj to subfolder
$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	@echo -e "\033[1m CC  - $@\033[0m"
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)libsecr.o: $(SECR_LIB_SOURCE) | $(EE_OBJS_DIR)
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
