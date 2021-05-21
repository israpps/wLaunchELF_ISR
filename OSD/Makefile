EE_TARGET=hddosd-headers
EE_BIN = $(EE_TARGET).elf
EE_BIN_PACKED = $(EE_TARGET)-packed.elf
EE_BIN_STRIPPED = $(EE_TARGET)-stripped.elf

EE_BIN_RAW = $(EE_TARGET).bin
EE_OBJS = main.o  pad.o  ps2_asm.o   timer.o hddosd-headers.o   $(EE_IOP_OBJS)
EE_IOP_OBJS = IOMANX_irx.o FILEXIO_irx.o SIO2MAN_irx.o PADMAN_irx.o DEV9_irx.o ATAD_irx.o HDD_irx.o PFS_irx.o USBD_irx.o USBHDFSD_irx.o

EE_LDFLAGS = -L$(PS2SDK)/ports/lib -Wl,--gc-sections  
EE_LIBS = -ldebug -lc -lkernel -lpatches -lfileXio   -lm -lpadx -lhdd
EE_INCS= -I$(GSKIT)/include -I$(PS2SDK)/ports/include
EE_CFLAGS=-Os -ffunction-sections -fdata-sections -DNEWLIB_PORT_AWARE


all:
	$(MAKE) $(EE_BIN_PACKED) 
	



clean:
	
	rm -f *.elf *.ELF *.irx *.o *.s *.bin *_irx.c 

DEV9_irx.c: $(PS2SDK)/iop/irx/ps2dev9.irx
	bin2c $(PS2SDK)/iop/irx/ps2dev9.irx DEV9_irx.c DEV9_irx

IOMANX_irx.c: $(PS2SDK)/iop/irx/iomanX.irx
	bin2c $(PS2SDK)/iop/irx/iomanX.irx IOMANX_irx.c IOMANX_irx

FILEXIO_irx.c: $(PS2SDK)/iop/irx/fileXio.irx
	bin2c $(PS2SDK)/iop/irx/fileXio.irx FILEXIO_irx.c FILEXIO_irx

SIO2MAN_irx.c: $(PS2SDK)/iop/irx/freesio2.irx
	bin2c $(PS2SDK)/iop/irx/freesio2.irx SIO2MAN_irx.c SIO2MAN_irx

PADMAN_irx.c: $(PS2SDK)/iop/irx/freepad.irx
	bin2c $(PS2SDK)/iop/irx/freepad.irx PADMAN_irx.c PADMAN_irx


ATAD_irx.c: $(PS2SDK)/iop/irx/ps2atad.irx
	bin2c $(PS2SDK)/iop/irx/ps2atad.irx ATAD_irx.c ATAD_irx

HDD_irx.c: $(PS2SDK)/iop/irx/ps2hdd-osd.irx
	bin2c $(PS2SDK)/iop/irx/ps2hdd-osd.irx HDD_irx.c HDD_irx

PFS_irx.c: $(PS2SDK)/iop/irx/ps2fs.irx
	bin2c $(PS2SDK)/iop/irx/ps2fs.irx PFS_irx.c PFS_irx

USBD_irx.c: $(PS2SDK)/iop/irx/usbd.irx
	bin2c $(PS2SDK)/iop/irx/usbd.irx USBD_irx.c USBD_irx

USBHDFSD_irx.c: $(PS2SDK)/iop/irx/usbhdfsd.irx
	bin2c $(PS2SDK)/iop/irx/usbhdfsd.irx USBHDFSD_irx.c USBHDFSD_irx
	



$(EE_BIN_STRIPPED): $(EE_BIN)
	$(EE_STRIP) -s -R .comment -R .gnu.version --strip-unneeded -o $@ $<

$(EE_BIN_PACKED): $(EE_BIN_STRIPPED)
	ps2-packer -v $< $@
	


include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

