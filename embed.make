
$(EE_ASM_DIR)mcman_irx.s: $(PS2SDK)/iop/irx/mcman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mcman_irx

$(EE_ASM_DIR)mcserv_irx.s: $(PS2SDK)/iop/irx/mcserv.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mcserv_irx

$(EE_ASM_DIR)usbd_irx.s: $(PS2SDK)/iop/irx/usbd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usbd_irx

ifeq ($(EXFAT),1)
$(EE_ASM_DIR)bdm_irx.s: iop/bdm.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ bdm_irx

$(EE_ASM_DIR)bdmfs_fatfs_irx.s: iop/bdmfs_fatfs.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ bdmfs_fatfs_irx

$(EE_ASM_DIR)usbmass_bd_irx.s: iop/usbmass_bd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usbmass_bd_irx
else
$(EE_ASM_DIR)usbhdfsd_irx.s: $(PS2SDK)/iop/irx/usbhdfsd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usb_mass_irx
endif

oldlibs/libcdvd/lib/cdvd.irx: oldlibs/libcdvd
	$(MAKE) -C $<

$(EE_ASM_DIR)cdvd_irx.s: oldlibs/libcdvd/lib/cdvd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ cdvd_irx

$(EE_ASM_DIR)poweroff_irx.s: $(PS2SDK)/iop/irx/poweroff.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ poweroff_irx

$(EE_ASM_DIR)iomanx_irx.s: $(PS2SDK)/iop/irx/iomanX.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ iomanx_irx

$(EE_ASM_DIR)filexio_irx.s: $(PS2SDK)/iop/irx/fileXio.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ filexio_irx

$(EE_ASM_DIR)ps2dev9_irx.s: $(PS2SDK)/iop/irx/ps2dev9.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2dev9_irx
	
$(EE_ASM_DIR)mx4sio_bd.s: iop/mx4sio_bd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mx4sio_bd_irx

ifeq ($(ETH),1)
$(EE_ASM_DIR)ps2ip_irx.s: $(PS2SDK)/iop/irx/ps2ip.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2ip_irx

$(EE_ASM_DIR)udptty.s: $(PS2SDK)/iop/irx/udptty.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ udptty_irx

$(EE_ASM_DIR)ps2smap_irx.s: $(PS2DEV)/ps2eth/smap/ps2smap.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2smap_irx

$(EE_ASM_DIR)ps2ftpd_irx.s: oldlibs/ps2ftpd/bin/ps2ftpd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2ftpd_irx

$(EE_ASM_DIR)ps2netfs_irx.s: $(PS2SDK)/iop/irx/ps2netfs.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2netfs_irx

$(EE_ASM_DIR)ps2host_irx.s: ps2host/ps2host.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2host_irx
endif

oldlibs/ps2ftpd/bin/ps2ftpd.irx: oldlibs/ps2ftpd
	$(MAKE) -C $<

$(EE_ASM_DIR)ps2atad_irx.s: $(PS2SDK)/iop/irx/ps2atad.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2atad_irx

$(EE_ASM_DIR)ps2hdd_irx.s: $(PS2SDK)/iop/irx/ps2hdd-osd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2hdd_irx

$(EE_ASM_DIR)ps2fs_irx.s: $(PS2SDK)/iop/irx/ps2fs.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2fs_irx
	
ifeq ($(DVRP),1)
$(EE_ASM_DIR)dvrdrv_irx.s: iop/dvrdrv.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ dvrdrv_irx

$(EE_ASM_DIR)dvrfile_irx.s: iop/dvrfile.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ dvrfile_irx
endif

hdl_info/hdl_info.irx: hdl_info
	$(MAKE) -C $<

$(EE_ASM_DIR)hdl_info_irx.s: hdl_info/hdl_info.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ hdl_info_irx

ps2host/ps2host.irx: ps2host
	$(MAKE) -C $<

ds34usb/ee/libds34usb.a: ds34usb/ee
	$(MAKE) -C $<

ds34usb/iop/ds34usb.irx: ds34usb/iop
	$(MAKE) -C $<

ds34bt/ee/libds34bt.a: ds34bt/ee
	$(MAKE) -C $<

ds34bt/iop/ds34bt.irx: ds34bt/iop
	$(MAKE) -C $<

$(EE_ASM_DIR)ds34usb.s: ds34usb/iop/ds34usb.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ds34usb_irx

libds34usb.a: ds34usb/ee/libds34usb.a
	cp $< $@	

$(EE_ASM_DIR)ds34bt.s: ds34bt/iop/ds34bt.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ds34bt_irx

libds34bt.a: ds34bt/ee/libds34bt.a
	cp $< $@

$(EE_ASM_DIR)padman.s: $(PS2SDK)/iop/irx/padman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ padman_irx

$(EE_ASM_DIR)sio2man.s: $(PS2SDK)/iop/irx/sio2man.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ sio2man_irx

ifeq ($(SMB),1)
$(EE_ASM_DIR)smbman_irx.s: $(PS2SDK)/iop/irx/smbman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ smbman_irx
endif

vmc_fs/vmc_fs.irx: vmc_fs
	$(MAKE) -C $<

$(EE_ASM_DIR)vmc_fs_irx.s: vmc_fs/vmc_fs.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ vmc_fs_irx

loader/loader.elf: loader
	$(MAKE) -C $<

$(EE_ASM_DIR)loader_elf.s: loader/loader.elf | $(EE_ASM_DIR)
	$(BIN2S) $< $@ loader_elf

$(EE_ASM_DIR)ps2kbd_irx.s: $(PS2SDK)/iop/irx/ps2kbd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2kbd_irx

$(EE_ASM_DIR)sior_irx.s: $(PS2SDK)/iop/irx/sior.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ sior_irx

AllowDVDV/AllowDVDV.irx: AllowDVDV
	$(MAKE) -C $<

$(EE_ASM_DIR)allowdvdv_irx.s: AllowDVDV/AllowDVDV.irx
	$(BIN2S) $< $@ allowdvdv_irx
