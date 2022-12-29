#.SILENT:

# ---{ BUILD CONFIGURATION }--- #
SIO_DEBUG ?= 0
SMB ?= 0
TMANIP ?= 1
ETH ?= 1
EXFAT ?= 0
DVRP ?= 0
IOP_RESET ?= 1
# ----------------------------- #

BIN_NAME = BOOT
EE_BIN = $(BIN_NAME)-UNC.ELF
EE_BIN_PKD = $(BIN_NAME).ELF
EE_OBJS = main.o pad.o config.o elf.o draw.o loader_elf.o filer.o \
	poweroff_irx.o iomanx_irx.o filexio_irx.o ps2atad_irx.o ps2dev9_irx.o\
	ps2hdd_irx.o ps2fs_irx.o usbd_irx.o mcman_irx.o mcserv_irx.o\
	cdvd_irx.o vmc_fs_irx.o ps2kbd_irx.o\
	hdd.o hdl_rpc.o hdl_info_irx.o editor.o timer.o jpgviewer.o icon.o lang.o\
	font_uLE.o makeicon.o chkesr.o allowdvdv_irx.o ds34usb.o libds34usb.a ds34bt.o libds34bt.a
ifeq ($(SMB),1)
    EE_OBJS += smbman.o
    BIN_NAME += -SMB
endif

EE_INCS := -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include -Ioldlibs/libcdvd/ee

EE_LDFLAGS := -L$(PS2DEV)/gsKit/lib -L$(PS2SDK)/ports/lib -Loldlibs/libcdvd/lib -s
EE_LIBS = -lgskit -ldmakit -ljpeg -lpad -lmc -lhdd -lcdvdfs -lkbd -lmf \
	-lcdvd -lc -lfileXio -lpatches -lpoweroff -ldebug -lc -lsior
EE_CFLAGS := -mgpopt -G10240 -G0 -DNEWLIB_PORT_AWARE -D_EE

ifeq ($(SMB),1)
    EE_CFLAGS += -DSMB
endif

ifeq ($(DVRP),1)
    EE_OBJS += dvrdrv_irx.o dvrfile_irx.o
    EE_CFLAGS += -DDVRP
    BIN_NAME += -DVRP
endif

ifeq ($(SIO_DEBUG),1)
    EE_CFLAGS += -DSIO_DEBUG
    EE_OBJS += sior_irx.o
    BIN_NAME += -EE_SIO
endif

ifeq ($(IOP_RESET),0)
    EE_CFLAGS += -DNO_IOP_RESET
    BIN_NAME += -NO_IOP_RESET
endif

ifeq ($(ETH),1)
    EE_OBJS += ps2smap_irx.o ps2ftpd_irx.o ps2host_irx.o ps2netfs_irx.o ps2ip_irx.o
    EE_CFLAGS += -DETH
    BIN_NAME += -ETH
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
    BIN_NAME += -EXFAT
else
    EE_OBJS += usbhdfsd_irx.o
endif

ifeq ($(DEFAULT_COLORS),1)
@echo using default colors
else
EE_CFLAGS += -DCUSTOM_COLORS
endif

.PHONY: all run reset clean rebuild

all: githash.h $(EE_BIN_PKD)

$(EE_BIN_PKD): $(EE_BIN)
	ps2-packer $< $@
ifeq ($(IOP_RESET),0)
	@echo "-------------{COMPILATION PERFORMED WITHOUT IOP RESET}-------------"
endif

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

mcman_irx.s: $(PS2SDK)/iop/irx/mcman.irx
	bin2s $< $@ mcman_irx

mcserv_irx.s: $(PS2SDK)/iop/irx/mcserv.irx
	bin2s $< $@ mcserv_irx

usbd_irx.s: $(PS2SDK)/iop/irx/usbd.irx
	bin2s $< $@ usbd_irx

ifeq ($(EXFAT),1)
bdm_irx.s: iop/bdm.irx
	bin2s $< $@ bdm_irx

bdmfs_fatfs_irx.s: iop/bdmfs_fatfs.irx
	bin2s $< $@ bdmfs_fatfs_irx

usbmass_bd_irx.s: iop/usbmass_bd.irx
	bin2s $< $@ usbmass_bd_irx
else
usbhdfsd_irx.s: $(PS2SDK)/iop/irx/usbhdfsd.irx
	bin2s $< $@ usb_mass_irx
endif

oldlibs/libcdvd/lib/cdvd.irx: oldlibs/libcdvd
	$(MAKE) -C $<

cdvd_irx.s: oldlibs/libcdvd/lib/cdvd.irx
	bin2s $< $@ cdvd_irx

poweroff_irx.s: $(PS2SDK)/iop/irx/poweroff.irx
	bin2s $< $@ poweroff_irx

iomanx_irx.s: $(PS2SDK)/iop/irx/iomanX.irx
	bin2s $< $@ iomanx_irx

filexio_irx.s: $(PS2SDK)/iop/irx/fileXio.irx
	bin2s $< $@ filexio_irx

ps2dev9_irx.s: $(PS2SDK)/iop/irx/ps2dev9.irx
	bin2s $< $@ ps2dev9_irx
	
ifeq ($(ETH),1)
ps2ip_irx.s: $(PS2SDK)/iop/irx/ps2ip.irx
	bin2s $< $@ ps2ip_irx

ps2smap_irx.s: $(PS2DEV)/ps2eth/smap/ps2smap.irx
	bin2s $< $@ ps2smap_irx
endif

oldlibs/ps2ftpd/bin/ps2ftpd.irx: oldlibs/ps2ftpd
	$(MAKE) -C $<

ifeq ($(ETH),1)
ps2ftpd_irx.s: oldlibs/ps2ftpd/bin/ps2ftpd.irx
	bin2s $< $@ ps2ftpd_irx
endif

ps2atad_irx.s: $(PS2SDK)/iop/irx/ps2atad.irx
	bin2s $< $@ ps2atad_irx

ps2hdd_irx.s: $(PS2SDK)/iop/irx/ps2hdd-osd.irx
	bin2s $< $@ ps2hdd_irx

ps2fs_irx.s: $(PS2SDK)/iop/irx/ps2fs.irx
	bin2s $< $@ ps2fs_irx
	
ifeq ($(DVRP),1)
dvrdrv_irx.s: iop/dvrdrv.irx
	bin2s $< $@ dvrdrv_irx

dvrfile_irx.s: iop/dvrfile.irx
	bin2s $< $@ dvrfile_irx
endif

ifeq ($(ETH),1)
ps2netfs_irx.s: $(PS2SDK)/iop/irx/ps2netfs.irx
	bin2s $< $@ ps2netfs_irx
endif

hdl_info/hdl_info.irx: hdl_info
	$(MAKE) -C $<

hdl_info_irx.s: hdl_info/hdl_info.irx
	bin2s $< $@ hdl_info_irx

ps2host/ps2host.irx: ps2host
	$(MAKE) -C $<

ifeq ($(ETH),1)
ps2host_irx.s: ps2host/ps2host.irx
	bin2s $< $@ ps2host_irx
endif

ds34usb/ee/libds34usb.a: ds34usb/ee
	$(MAKE) -C $<

ds34usb/iop/ds34usb.irx: ds34usb/iop
	$(MAKE) -C $<

ds34bt/ee/libds34bt.a: ds34bt/ee
	$(MAKE) -C $<

ds34bt/iop/ds34bt.irx: ds34bt/iop
	$(MAKE) -C $<

ds34usb.s: ds34usb/iop/ds34usb.irx
	@bin2s $< $@ ds34usb_irx

libds34usb.a: ds34usb/ee/libds34usb.a
	cp $< $@	

ds34bt.s: ds34bt/iop/ds34bt.irx
	@bin2s $< $@ ds34bt_irx

libds34bt.a: ds34bt/ee/libds34bt.a
	cp $< $@

ifeq ($(SMB),1)
smbman_irx.s: $(PS2SDK)/iop/irx/smbman.irx
	bin2s $< $@ smbman_irx
endif

vmc_fs/vmc_fs.irx: vmc_fs
	$(MAKE) -C $<

vmc_fs_irx.s: vmc_fs/vmc_fs.irx
	bin2s $< $@ vmc_fs_irx

loader/loader.elf: loader
	$(MAKE) -C $<

loader_elf.s: loader/loader.elf
	bin2s $< $@ loader_elf

ps2kbd_irx.s: $(PS2SDK)/iop/irx/ps2kbd.irx
	bin2s $< $@ ps2kbd_irx

sior_irx.s: $(PS2SDK)/iop/irx/sior.irx
	bin2s $< $@ sior_irx

AllowDVDV/AllowDVDV.irx: AllowDVDV
	$(MAKE) -C $<

allowdvdv_irx.s: AllowDVDV/AllowDVDV.irx
	bin2s $< $@ allowdvdv_irx

clean:
	$(MAKE) -C hdl_info clean
	$(MAKE) -C ps2host clean
	$(MAKE) -C loader clean
	$(MAKE) -C vmc_fs clean
	$(MAKE) -C AllowDVDV clean
	$(MAKE) -C oldlibs/libcdvd clean
	$(MAKE) -C oldlibs/ps2ftpd clean
	rm -f githash.h *.s $(EE_OBJS) $(EE_BIN) $(EE_BIN_PKD)

rebuild: clean all

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
