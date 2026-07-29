$(info using ps2sdk mc/pad drivers)
MCMAN_SOURCE = $(PS2SDK)/iop/irx/mcman.irx
PADMAN_SOURCE = $(PS2SDK)/iop/irx/padman.irx
MCSERV_SOURCE = $(PS2SDK)/iop/irx/mcserv.irx
SIO2MAN_SOURCE = $(PS2SDK)/iop/irx/sio2man.irx
DVRDRV_SOURCE = $(PS2SDK)/iop/irx/dvrdrv.irx
DVRFILE_SOURCE = $(PS2SDK)/iop/irx/dvrfile.irx

ifeq ($(wildcard $(MCMAN_SOURCE)),)
  $(error Missing $(MCMAN_SOURCE). Update PS2SDK/toolchain container)
endif
ifeq ($(wildcard $(MCSERV_SOURCE)),)
  $(error Missing $(MCSERV_SOURCE). Update PS2SDK/toolchain container)
endif
ifeq ($(wildcard $(SIO2MAN_SOURCE)),)
  $(error Missing $(SIO2MAN_SOURCE). Update PS2SDK/toolchain container)
endif
ifeq ($(wildcard $(PADMAN_SOURCE)),)
  $(error Missing $(PADMAN_SOURCE). Update PS2SDK/toolchain container)
endif
ifeq ($(DVRP),1)
  ifeq ($(wildcard $(DVRDRV_SOURCE)),)
    $(error Missing $(DVRDRV_SOURCE). Update PS2SDK/toolchain container)
  endif
  ifeq ($(wildcard $(DVRFILE_SOURCE)),)
    $(error Missing $(DVRFILE_SOURCE). Update PS2SDK/toolchain container)
  endif
endif

EXTFLASH_SOURCE = iop/__precompiled/extflash.irx
XFROMMAN_SOURCE = iop/__precompiled/xfromman.irx
XFORMSERV_SOURCE = $(PS2SDK)/iop/irx/xfromserv.irx
SECRSIF_SOURCE = $(PS2SDK)/iop/irx/secrsif.irx
EXPLOIT_IOPRP_SOURCE := exploits/IOPRP_LTS.IMG
ifeq ($(wildcard $(SECRSIF_SOURCE)),)
  ifneq ($(wildcard thirdparty/KELFbinder-UMCS-main/iop/secrsif.irx),)
    SECRSIF_SOURCE = thirdparty/KELFbinder-UMCS-main/iop/secrsif.irx
  endif
endif
ifeq ($(wildcard $(SECRSIF_SOURCE)),)
  $(error Missing secrsif.irx. Update PS2SDK/toolchain container for exploit MagicGate signer)
endif
ifeq ($(wildcard $(EXPLOIT_IOPRP_SOURCE)),)
  $(error Missing $(EXPLOIT_IOPRP_SOURCE). Required for exploit MagicGate signer)
endif
ifeq ($(XFROM),1)
  ifneq ($(wildcard $(PS2SDK)/iop/irx/extflash.irx),)
    EXTFLASH_SOURCE = $(PS2SDK)/iop/irx/extflash.irx
  endif
  ifneq ($(wildcard $(PS2SDK)/iop/irx/xfromman.irx),)
    XFROMMAN_SOURCE = $(PS2SDK)/iop/irx/xfromman.irx
  endif
  ifeq ($(wildcard $(XFORMSERV_SOURCE)),)
    $(error Missing xfromserv.irx. Update PS2SDK/toolchain container for XFROM support)
  endif
endif

EXPLOIT_SYSTEM_XLF_SOURCE := exploits/SYSTEM.XLF
EXPLOIT_XSYSTEM_XLF_SOURCE := exploits/XSYSTEM.XLF
EXPLOIT_DTL_SYSTEM_XLF_SOURCE := exploits/DTL-SYSTEM.XLF
EXPLOIT_XLF_HEADER_SIZE := 128
EXPLOIT_OSDSYS_KERNEL_SOURCE := exploits/OSDSYS.KERNEL
EXPLOIT_OSD110_KERNEL_SOURCE := exploits/OSD110.KERNEL
EXPLOIT_JPN_SYS_SOURCE := exploits/JPN.sys
EXPLOIT_USA_SYS_SOURCE := exploits/USA.sys
EXPLOIT_EUR_SYS_SOURCE := exploits/EUR.sys
EXPLOIT_CHN_SYS_SOURCE := exploits/CHN.sys
EXPLOIT_LIST_ICN_SOURCE := exploits/list.icn

ifeq ($(wildcard $(EXPLOIT_SYSTEM_XLF_SOURCE)),)
  $(error Missing $(EXPLOIT_SYSTEM_XLF_SOURCE))
endif
ifeq ($(wildcard $(EXPLOIT_XSYSTEM_XLF_SOURCE)),)
  $(error Missing $(EXPLOIT_XSYSTEM_XLF_SOURCE))
endif
ifeq ($(wildcard $(EXPLOIT_DTL_SYSTEM_XLF_SOURCE)),)
  $(error Missing $(EXPLOIT_DTL_SYSTEM_XLF_SOURCE))
endif
ifeq ($(wildcard $(EXPLOIT_OSDSYS_KERNEL_SOURCE)),)
  $(error Missing $(EXPLOIT_OSDSYS_KERNEL_SOURCE))
endif
ifeq ($(wildcard $(EXPLOIT_OSD110_KERNEL_SOURCE)),)
  $(error Missing $(EXPLOIT_OSD110_KERNEL_SOURCE))
endif
ifeq ($(wildcard $(EXPLOIT_JPN_SYS_SOURCE)),)
  $(error Missing $(EXPLOIT_JPN_SYS_SOURCE))
endif
ifeq ($(wildcard $(EXPLOIT_USA_SYS_SOURCE)),)
  $(error Missing $(EXPLOIT_USA_SYS_SOURCE))
endif
ifeq ($(wildcard $(EXPLOIT_EUR_SYS_SOURCE)),)
  $(error Missing $(EXPLOIT_EUR_SYS_SOURCE))
endif
ifeq ($(wildcard $(EXPLOIT_CHN_SYS_SOURCE)),)
  $(error Missing $(EXPLOIT_CHN_SYS_SOURCE))
endif
ifeq ($(wildcard $(EXPLOIT_LIST_ICN_SOURCE)),)
  $(error Missing $(EXPLOIT_LIST_ICN_SOURCE))
endif

XPARAM_SOURCE :=
XPARAM_AUTOGEN := iop/__generated/xparam.irx
XPARAM_SDK_ROOT :=
XPARAM_SDK_MODULE_DIR :=

ifneq ($(wildcard $(PS2SDK)/iop/irx/xparam.irx),)
XPARAM_SOURCE := $(PS2SDK)/iop/irx/xparam.irx
endif

ifeq ($(strip $(XPARAM_SOURCE)),)
ifneq ($(PS2SDKSRC),)
ifneq ($(wildcard $(PS2SDKSRC)/iop/deckard/xparam/Makefile),)
XPARAM_SDK_ROOT := $(PS2SDKSRC)
XPARAM_SDK_MODULE_DIR := $(PS2SDKSRC)/iop/deckard/xparam
endif
endif
endif

ifeq ($(strip $(XPARAM_SOURCE)),)
ifneq ($(wildcard $(PS2SDK)/iop/deckard/xparam/Makefile),)
XPARAM_SDK_ROOT := $(PS2SDK)
XPARAM_SDK_MODULE_DIR := $(PS2SDK)/iop/deckard/xparam
endif
endif

ifeq ($(strip $(XPARAM_SOURCE)),)
ifneq ($(strip $(XPARAM_SDK_MODULE_DIR)),)
XPARAM_SOURCE := $(XPARAM_AUTOGEN)
endif
endif

ifeq ($(strip $(XPARAM_SOURCE)),)
$(error Missing xparam.irx. Update PS2SDK, or provide PS2SDKSRC with iop/deckard/xparam sources)
endif

# Prefer newer storage stack modules:
# 1) installed PS2SDK IRX
# 2) auto-build from PS2SDK sources
# 3) bundled fallback IRX
BDM_SOURCE :=
BDM_AUTOGEN := iop/__generated/bdm.irx
BDM_SDK_ROOT :=
BDM_SDK_MODULE_DIR :=

BDMFS_FATFS_SOURCE :=
BDMFS_FATFS_AUTOGEN := iop/__generated/bdmfs_fatfs.irx
BDMFS_FATFS_SDK_ROOT :=
BDMFS_FATFS_SDK_MODULE_DIR :=

USBMASS_BD_SOURCE :=
USBMASS_BD_AUTOGEN := iop/__generated/usbmass_bd.irx
USBMASS_BD_SDK_ROOT :=
USBMASS_BD_SDK_MODULE_DIR :=

IOMANX_SOURCE := iop/__precompiled/iomanX.irx
FILEXIO_SOURCE := iop/__precompiled/fileXio.irx
MMCEMAN_AUTOGEN := iop/__generated/mmceman.irx
MMCEMAN_SOURCE := iop/__precompiled/mmceman.irx
MMCEMAN_LOCAL_DIR := iop/mmceman
MMCEMAN_LOCAL_SOURCES := $(wildcard $(MMCEMAN_LOCAL_DIR)/Makefile $(MMCEMAN_LOCAL_DIR)/src/* $(MMCEMAN_LOCAL_DIR)/include/*)
VMCMAN_AUTOGEN := iop/__generated/vmcman.irx
VMCMAN_SOURCE := $(VMCMAN_AUTOGEN)
VMCMAN_LOCAL_DIR := iop/vmcman
VMCMAN_LOCAL_SOURCES := $(wildcard $(VMCMAN_LOCAL_DIR)/Makefile $(VMCMAN_LOCAL_DIR)/src/* $(VMCMAN_LOCAL_DIR)/include/*)
VMCMAN_SDK_ROOT :=

ifneq ($(wildcard $(MMCEMAN_LOCAL_DIR)/Makefile),)
MMCEMAN_SOURCE := $(MMCEMAN_AUTOGEN)
endif

ifneq ($(MMCEMAN_SOURCE),$(MMCEMAN_AUTOGEN))
ifeq ($(wildcard $(MMCEMAN_SOURCE)),)
$(error Missing $(MMCEMAN_SOURCE). Add iop/__precompiled/mmceman.irx or provide iop/mmceman sources)
endif
endif

ifneq ($(wildcard $(PS2SDK)/iop/irx/bdm.irx),)
BDM_SOURCE := $(PS2SDK)/iop/irx/bdm.irx
endif
ifneq ($(wildcard $(PS2SDK)/iop/irx/bdmfs_fatfs.irx),)
BDMFS_FATFS_SOURCE := $(PS2SDK)/iop/irx/bdmfs_fatfs.irx
endif
ifneq ($(wildcard $(PS2SDK)/iop/irx/usbmass_bd.irx),)
USBMASS_BD_SOURCE := $(PS2SDK)/iop/irx/usbmass_bd.irx
endif

ifeq ($(strip $(BDM_SOURCE)),)
ifneq ($(PS2SDKSRC),)
ifneq ($(wildcard $(PS2SDKSRC)/iop/fs/bdm/Makefile),)
BDM_SDK_ROOT := $(PS2SDKSRC)
BDM_SDK_MODULE_DIR := $(PS2SDKSRC)/iop/fs/bdm
endif
endif
endif

ifeq ($(strip $(BDM_SOURCE)),)
ifneq ($(wildcard $(PS2SDK)/iop/fs/bdm/Makefile),)
BDM_SDK_ROOT := $(PS2SDK)
BDM_SDK_MODULE_DIR := $(PS2SDK)/iop/fs/bdm
endif
endif

ifeq ($(strip $(BDM_SOURCE)),)
ifneq ($(strip $(BDM_SDK_MODULE_DIR)),)
BDM_SOURCE := $(BDM_AUTOGEN)
endif
endif

ifeq ($(strip $(BDM_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/bdm.irx),)
BDM_SOURCE := iop/__precompiled/bdm.irx
endif
endif

ifeq ($(strip $(BDM_SOURCE)),)
$(error Missing bdm.irx. Update PS2SDK, add iop/__precompiled/bdm.irx, or provide PS2SDKSRC with iop/fs/bdm sources)
endif

ifeq ($(strip $(BDMFS_FATFS_SOURCE)),)
ifneq ($(PS2SDKSRC),)
ifneq ($(wildcard $(PS2SDKSRC)/iop/fs/bdmfs_fatfs/Makefile),)
BDMFS_FATFS_SDK_ROOT := $(PS2SDKSRC)
BDMFS_FATFS_SDK_MODULE_DIR := $(PS2SDKSRC)/iop/fs/bdmfs_fatfs
endif
endif
endif

ifeq ($(strip $(BDMFS_FATFS_SOURCE)),)
ifneq ($(wildcard $(PS2SDK)/iop/fs/bdmfs_fatfs/Makefile),)
BDMFS_FATFS_SDK_ROOT := $(PS2SDK)
BDMFS_FATFS_SDK_MODULE_DIR := $(PS2SDK)/iop/fs/bdmfs_fatfs
endif
endif

ifeq ($(strip $(BDMFS_FATFS_SOURCE)),)
ifneq ($(strip $(BDMFS_FATFS_SDK_MODULE_DIR)),)
BDMFS_FATFS_SOURCE := $(BDMFS_FATFS_AUTOGEN)
endif
endif

ifeq ($(strip $(BDMFS_FATFS_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/bdmfs_fatfs.irx),)
BDMFS_FATFS_SOURCE := iop/__precompiled/bdmfs_fatfs.irx
endif
endif

ifeq ($(strip $(BDMFS_FATFS_SOURCE)),)
$(error Missing bdmfs_fatfs.irx. Update PS2SDK, add iop/__precompiled/bdmfs_fatfs.irx, or provide PS2SDKSRC with iop/fs/bdmfs_fatfs sources)
endif

ifeq ($(strip $(USBMASS_BD_SOURCE)),)
ifneq ($(PS2SDKSRC),)
ifneq ($(wildcard $(PS2SDKSRC)/iop/usb/usbmass_bd/Makefile),)
USBMASS_BD_SDK_ROOT := $(PS2SDKSRC)
USBMASS_BD_SDK_MODULE_DIR := $(PS2SDKSRC)/iop/usb/usbmass_bd
endif
endif
endif

ifeq ($(strip $(USBMASS_BD_SOURCE)),)
ifneq ($(wildcard $(PS2SDK)/iop/usb/usbmass_bd/Makefile),)
USBMASS_BD_SDK_ROOT := $(PS2SDK)
USBMASS_BD_SDK_MODULE_DIR := $(PS2SDK)/iop/usb/usbmass_bd
endif
endif

ifeq ($(strip $(USBMASS_BD_SOURCE)),)
ifneq ($(strip $(USBMASS_BD_SDK_MODULE_DIR)),)
USBMASS_BD_SOURCE := $(USBMASS_BD_AUTOGEN)
endif
endif

ifeq ($(strip $(USBMASS_BD_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/usbmass_bd.irx),)
USBMASS_BD_SOURCE := iop/__precompiled/usbmass_bd.irx
endif
endif

ifeq ($(strip $(USBMASS_BD_SOURCE)),)
$(error Missing usbmass_bd.irx. Update PS2SDK, add iop/__precompiled/usbmass_bd.irx, or provide PS2SDKSRC with iop/usb/usbmass_bd sources)
endif

ifneq ($(wildcard $(PS2SDK)/iop/irx/iomanX.irx),)
IOMANX_SOURCE := $(PS2SDK)/iop/irx/iomanX.irx
endif
ifneq ($(wildcard $(PS2SDK)/iop/irx/fileXio.irx),)
FILEXIO_SOURCE := $(PS2SDK)/iop/irx/fileXio.irx
endif

ifneq ($(PS2SDKSRC),)
ifneq ($(wildcard $(PS2SDKSRC)/Defs.make),)
VMCMAN_SDK_ROOT := $(PS2SDKSRC)
endif
endif

ifeq ($(strip $(VMCMAN_SDK_ROOT)),)
ifneq ($(wildcard $(PS2SDK)/Defs.make),)
VMCMAN_SDK_ROOT := $(PS2SDK)
endif
endif

ifeq ($(strip $(VMCMAN_SDK_ROOT)),)
$(error Missing PS2SDK source tree for local vmcman build. Set PS2SDKSRC to a ps2sdk checkout)
endif



#---{ MC }---#
$(EE_ASM_DIR)mcman_irx.s: $(MCMAN_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mcman_irx

$(EE_ASM_DIR)mcserv_irx.s: $(MCSERV_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mcserv_irx

$(EE_ASM_DIR)sio2man.s: $(SIO2MAN_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ sio2man_irx

# MX4SIO_BD module selection:
# 1) installed PS2SDK IRX (preferred)
# 2) auto-build from PS2SDK sources (when available)
# 3) bundled fallback IRX
MX4SIO_BD_SOURCE :=
MX4SIO_BD_AUTOGEN := iop/__generated/mx4sio_bd.irx
MX4SIO_BD_SDK_ROOT :=
MX4SIO_BD_SDK_MODULE_DIR :=

ifneq ($(wildcard $(PS2SDK)/iop/irx/mx4sio_bd.irx),)
MX4SIO_BD_SOURCE := $(PS2SDK)/iop/irx/mx4sio_bd.irx
endif

ifeq ($(strip $(MX4SIO_BD_SOURCE)),)
ifneq ($(PS2SDKSRC),)
ifneq ($(wildcard $(PS2SDKSRC)/iop/sio/mx4sio_bd/Makefile),)
MX4SIO_BD_SDK_ROOT := $(PS2SDKSRC)
MX4SIO_BD_SDK_MODULE_DIR := $(PS2SDKSRC)/iop/sio/mx4sio_bd
endif
endif
endif

ifeq ($(strip $(MX4SIO_BD_SOURCE)),)
ifneq ($(wildcard $(PS2SDK)/iop/sio/mx4sio_bd/Makefile),)
MX4SIO_BD_SDK_ROOT := $(PS2SDK)
MX4SIO_BD_SDK_MODULE_DIR := $(PS2SDK)/iop/sio/mx4sio_bd
endif
endif

ifeq ($(strip $(MX4SIO_BD_SOURCE)),)
ifneq ($(strip $(MX4SIO_BD_SDK_MODULE_DIR)),)
MX4SIO_BD_SOURCE := $(MX4SIO_BD_AUTOGEN)
endif
endif

ifeq ($(strip $(MX4SIO_BD_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/mx4sio_bd.irx),)
MX4SIO_BD_SOURCE := iop/__precompiled/mx4sio_bd.irx
endif
endif

ifeq ($(strip $(MX4SIO_BD_SOURCE)),)
$(error Missing mx4sio_bd.irx. Update PS2SDK, add iop/__precompiled/mx4sio_bd.irx, or provide PS2SDKSRC with iop/sio/mx4sio_bd sources)
endif

$(MX4SIO_BD_AUTOGEN): | iop/__generated
	$(MAKE) -C $(MX4SIO_BD_SDK_MODULE_DIR) \
		PS2SDKSRC=$(MX4SIO_BD_SDK_ROOT) \
		PS2SDK=$(MX4SIO_BD_SDK_ROOT) \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/mx4sio_bd_obj)/ \
		IOP_BIN=mx4sio_bd.irx

$(EE_ASM_DIR)mx4sio_bd.s: $(MX4SIO_BD_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mx4sio_bd_irx
	
$(EE_ASM_DIR)mmceman_irx.s: $(MMCEMAN_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mmceman_irx
 
$(EE_ASM_DIR)extflash_irx.s: $(EXTFLASH_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ extflash_irx

$(EE_ASM_DIR)xfromman_irx.s: $(XFROMMAN_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ xfromman_irx

$(EE_ASM_DIR)xfromserv_irx.s: $(XFORMSERV_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ xfromserv_irx

$(EE_ASM_DIR)secrsif_irx.s: $(SECRSIF_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ secrsif_irx

$(EE_ASM_DIR)exploit_ioprp_img.s: $(EXPLOIT_IOPRP_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ exploit_ioprp_img

$(EE_ASM_DIR)exploit_system_xlf_header.s: $(EXPLOIT_SYSTEM_XLF_SOURCE) | $(EE_ASM_DIR)
	@{ \
		echo "/* Auto-generated by embed.make */"; \
		echo "    .section .rodata"; \
		echo "    .align 4"; \
		echo "    .global exploit_system_xlf_header"; \
		echo "    .global size_exploit_system_xlf_header"; \
		echo "exploit_system_xlf_header:"; \
		echo "    .incbin \"$(abspath $(EXPLOIT_SYSTEM_XLF_SOURCE))\", 0, $(EXPLOIT_XLF_HEADER_SIZE)"; \
		echo "exploit_system_xlf_header_end:"; \
		echo "    .align 2"; \
		echo "size_exploit_system_xlf_header:"; \
		echo "    .word exploit_system_xlf_header_end - exploit_system_xlf_header"; \
	} > $@.tmp
	@mv $@.tmp $@

$(EE_ASM_DIR)exploit_xsystem_xlf_header.s: $(EXPLOIT_XSYSTEM_XLF_SOURCE) | $(EE_ASM_DIR)
	@{ \
		echo "/* Auto-generated by embed.make */"; \
		echo "    .section .rodata"; \
		echo "    .align 4"; \
		echo "    .global exploit_xsystem_xlf_header"; \
		echo "    .global size_exploit_xsystem_xlf_header"; \
		echo "exploit_xsystem_xlf_header:"; \
		echo "    .incbin \"$(abspath $(EXPLOIT_XSYSTEM_XLF_SOURCE))\", 0, $(EXPLOIT_XLF_HEADER_SIZE)"; \
		echo "exploit_xsystem_xlf_header_end:"; \
		echo "    .align 2"; \
		echo "size_exploit_xsystem_xlf_header:"; \
		echo "    .word exploit_xsystem_xlf_header_end - exploit_xsystem_xlf_header"; \
	} > $@.tmp
	@mv $@.tmp $@

$(EE_ASM_DIR)exploit_dtl_system_xlf_header.s: $(EXPLOIT_DTL_SYSTEM_XLF_SOURCE) | $(EE_ASM_DIR)
	@{ \
		echo "/* Auto-generated by embed.make */"; \
		echo "    .section .rodata"; \
		echo "    .align 4"; \
		echo "    .global exploit_dtl_system_xlf_header"; \
		echo "    .global size_exploit_dtl_system_xlf_header"; \
		echo "exploit_dtl_system_xlf_header:"; \
		echo "    .incbin \"$(abspath $(EXPLOIT_DTL_SYSTEM_XLF_SOURCE))\", 0, $(EXPLOIT_XLF_HEADER_SIZE)"; \
		echo "exploit_dtl_system_xlf_header_end:"; \
		echo "    .align 2"; \
		echo "size_exploit_dtl_system_xlf_header:"; \
		echo "    .word exploit_dtl_system_xlf_header_end - exploit_dtl_system_xlf_header"; \
	} > $@.tmp
	@mv $@.tmp $@

$(EE_ASM_DIR)exploit_xlf_body.s: $(EXPLOIT_SYSTEM_XLF_SOURCE) | $(EE_ASM_DIR)
	@{ \
		echo "/* Auto-generated by embed.make */"; \
		echo "    .section .rodata"; \
		echo "    .align 4"; \
		echo "    .global exploit_xlf_body"; \
		echo "    .global size_exploit_xlf_body"; \
		echo "exploit_xlf_body:"; \
		echo "    .incbin \"$(abspath $(EXPLOIT_SYSTEM_XLF_SOURCE))\", $(EXPLOIT_XLF_HEADER_SIZE)"; \
		echo "exploit_xlf_body_end:"; \
		echo "    .align 2"; \
		echo "size_exploit_xlf_body:"; \
		echo "    .word exploit_xlf_body_end - exploit_xlf_body"; \
	} > $@.tmp
	@mv $@.tmp $@

$(EE_ASM_DIR)exploit_osdsys_kernel.s: $(EXPLOIT_OSDSYS_KERNEL_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ exploit_osdsys_kernel

$(EE_ASM_DIR)exploit_osd110_kernel.s: $(EXPLOIT_OSD110_KERNEL_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ exploit_osd110_kernel

$(EE_ASM_DIR)exploit_jpn_sys.s: $(EXPLOIT_JPN_SYS_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ exploit_jpn_sys

$(EE_ASM_DIR)exploit_usa_sys.s: $(EXPLOIT_USA_SYS_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ exploit_usa_sys

$(EE_ASM_DIR)exploit_eur_sys.s: $(EXPLOIT_EUR_SYS_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ exploit_eur_sys

$(EE_ASM_DIR)exploit_chn_sys.s: $(EXPLOIT_CHN_SYS_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ exploit_chn_sys

$(EE_ASM_DIR)exploit_list_icn.s: $(EXPLOIT_LIST_ICN_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ exploit_list_icn

#---{ USB }---#

$(EE_ASM_DIR)usbd_irx.s: $(PS2SDK)/iop/irx/usbd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usbd_irx
ifeq ($(EXFAT),1)
$(BDM_AUTOGEN): | iop/__generated
	$(MAKE) -C $(BDM_SDK_MODULE_DIR) \
		PS2SDKSRC=$(BDM_SDK_ROOT) \
		PS2SDK=$(BDM_SDK_ROOT) \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/bdm_obj)/ \
		IOP_BIN=bdm.irx

$(BDMFS_FATFS_AUTOGEN): | iop/__generated
	$(MAKE) -C $(BDMFS_FATFS_SDK_MODULE_DIR) \
		PS2SDKSRC=$(BDMFS_FATFS_SDK_ROOT) \
		PS2SDK=$(BDMFS_FATFS_SDK_ROOT) \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/bdmfs_fatfs_obj)/ \
		IOP_BIN=bdmfs_fatfs.irx

$(USBMASS_BD_AUTOGEN): | iop/__generated
	$(MAKE) -C $(USBMASS_BD_SDK_MODULE_DIR) \
		PS2SDKSRC=$(USBMASS_BD_SDK_ROOT) \
		PS2SDK=$(USBMASS_BD_SDK_ROOT) \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/usbmass_bd_obj)/ \
		IOP_BIN=usbmass_bd.irx

$(EE_ASM_DIR)bdm_irx.s:$(BDM_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ bdm_irx

$(EE_ASM_DIR)bdmfs_fatfs_irx.s:$(BDMFS_FATFS_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ bdmfs_fatfs_irx

$(EE_ASM_DIR)usbmass_bd_irx.s:$(USBMASS_BD_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usbmass_bd_irx

# ATA BDM module is not present in older PS2SDK releases.
# Preferred order:
# 1) installed PS2SDK IRX
# 2) bundled fallback IRX
# 3) auto-build from PS2SDK sources (when available)
ATA_BD_SOURCE :=
ATA_BD_AUTOGEN := iop/__generated/ata_bd.irx
ATA_BD_SDK_ROOT :=
ATA_BD_SDK_MODULE_DIR :=

ifneq ($(wildcard $(PS2SDK)/iop/irx/ata_bd.irx),)
ATA_BD_SOURCE := $(PS2SDK)/iop/irx/ata_bd.irx
endif

ifeq ($(strip $(ATA_BD_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/ata_bd.irx),)
ATA_BD_SOURCE := iop/__precompiled/ata_bd.irx
endif
endif

ifeq ($(strip $(ATA_BD_SOURCE)),)
ifneq ($(PS2SDKSRC),)
ifneq ($(wildcard $(PS2SDKSRC)/iop/dev9/ata_bd/Makefile),)
ATA_BD_SDK_ROOT := $(PS2SDKSRC)
ATA_BD_SDK_MODULE_DIR := $(PS2SDKSRC)/iop/dev9/ata_bd
endif
endif
endif

ifeq ($(strip $(ATA_BD_SOURCE)),)
ifneq ($(wildcard $(PS2SDK)/iop/dev9/ata_bd/Makefile),)
ATA_BD_SDK_ROOT := $(PS2SDK)
ATA_BD_SDK_MODULE_DIR := $(PS2SDK)/iop/dev9/ata_bd
endif
endif

ifeq ($(strip $(ATA_BD_SOURCE)),)
ifneq ($(strip $(ATA_BD_SDK_MODULE_DIR)),)
ATA_BD_SOURCE := $(ATA_BD_AUTOGEN)
endif
endif

ifeq ($(strip $(ATA_BD_SOURCE)),)
$(error Missing ata_bd.irx. Update PS2SDK, add iop/__precompiled/ata_bd.irx, or provide PS2SDKSRC with iop/dev9/ata_bd sources)
endif

$(ATA_BD_AUTOGEN): | iop/__generated
	$(MAKE) -C $(ATA_BD_SDK_MODULE_DIR) \
		PS2SDKSRC=$(ATA_BD_SDK_ROOT) \
		PS2SDK=$(ATA_BD_SDK_ROOT) \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/ata_bd_obj)/ \
		IOP_BIN=ata_bd.irx

$(EE_ASM_DIR)ata_bd_irx.s:$(ATA_BD_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ata_bd_irx
else
$(EE_ASM_DIR)usbhdfsd_irx.s: $(PS2SDK)/iop/irx/usbhdfsd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usb_mass_irx
endif

# ----- #

iop/cdvd.irx: iop/oldlibs/libcdvd
	$(MAKE) -C $<

$(EE_ASM_DIR)cdvd_irx.s: $(CDVD_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ cdvd_irx

ifneq ($(filter iop/__generated/xparam.irx,$(XPARAM_SOURCE)),)
$(XPARAM_SOURCE): | iop/__generated
	$(MAKE) -C $(XPARAM_SDK_MODULE_DIR) PS2SDKSRC=$(XPARAM_SDK_ROOT) PS2SDK=$(XPARAM_SDK_ROOT)
	if [ -f "$(XPARAM_SDK_MODULE_DIR)/xparam.irx" ]; then cp "$(XPARAM_SDK_MODULE_DIR)/xparam.irx" "$@"; elif [ -f "$(XPARAM_SDK_MODULE_DIR)/irx/xparam.irx" ]; then cp "$(XPARAM_SDK_MODULE_DIR)/irx/xparam.irx" "$@"; else echo "xparam.irx was not produced by $(XPARAM_SDK_MODULE_DIR)" >&2; exit 1; fi
endif

$(EE_ASM_DIR)xparam_irx.s: $(XPARAM_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ xparam_irx

$(EE_ASM_DIR)ioptrap_irx.s: $(PS2SDK)/iop/irx/ioptrap.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ioptrap_irx

$(EE_ASM_DIR)poweroff_irx.s: $(PS2SDK)/iop/irx/poweroff.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ poweroff_irx

$(EE_ASM_DIR)iomanx_irx.s: $(IOMANX_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ iomanx_irx

$(EE_ASM_DIR)filexio_irx.s: $(FILEXIO_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ filexio_irx

iop/__generated:
	mkdir -p $@

$(VMCMAN_AUTOGEN): $(VMCMAN_LOCAL_SOURCES) | iop/__generated
	$(MAKE) -C $(VMCMAN_LOCAL_DIR) \
		PS2SDKSRC=$(VMCMAN_SDK_ROOT) \
		PS2SDK=$(VMCMAN_SDK_ROOT) \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/vmcman_obj)/ \
		IOP_BIN=$(abspath $@) \
		DEBUG=$(DEBUG)
	test -s $@

$(MMCEMAN_AUTOGEN): $(MMCEMAN_LOCAL_SOURCES) | iop/__generated
	$(MAKE) -C $(MMCEMAN_LOCAL_DIR) \
		PS2SDKSRC=$(VMCMAN_SDK_ROOT) \
		PS2SDK=$(VMCMAN_SDK_ROOT) \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/mmceman_obj)/ \
		IOP_BIN=$(abspath $@) \
		DEBUG=$(DEBUG)
	test -s $@

$(EE_ASM_DIR)vmcman_irx.c: $(VMCMAN_SOURCE) scripts/bin2c-fallback.sh | $(EE_ASM_DIR)
	sh scripts/bin2c-fallback.sh $< $@ vmcman_irx

$(EE_OBJS_DIR)vmcman_irx.o: $(EE_ASM_DIR)vmcman_irx.c | $(EE_OBJS_DIR)
	@echo -e "\033[1m CC  - $@\033[0m"
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_ASM_DIR)ps2dev9_irx.s: $(PS2SDK)/iop/irx/ps2dev9.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2dev9_irx

LOCAL_DEV9_POWEROFF_IRX := iop/dev9_poweroff/dev9_poweroff.irx
DEV9_POWEROFF_DEPS := $(wildcard iop/dev9_poweroff/*.[ch]) iop/dev9_poweroff/imports.lst iop/dev9_poweroff/Makefile

$(LOCAL_DEV9_POWEROFF_IRX): $(DEV9_POWEROFF_DEPS)
	$(MAKE) -C iop/dev9_poweroff

$(EE_ASM_DIR)dev9_poweroff_irx.s: $(LOCAL_DEV9_POWEROFF_IRX) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ dev9_poweroff_irx

ifeq ($(ETH),1)
$(EE_ASM_DIR)ps2ip_irx.s: $(PS2SDK)/iop/irx/ps2ip.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2ip_irx

$(EE_ASM_DIR)udptty.s: $(PS2SDK)/iop/irx/udptty.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ udptty_irx

# ETH stack policy:
# Preserve historical wLaunchELF SMAP preference/order first, then fallback
# to current PS2SDK variants.
PS2SMAP_SOURCE :=
ifneq ($(wildcard $(PS2DEV)/ps2eth/smap/ps2smap.irx),)
PS2SMAP_SOURCE := $(PS2DEV)/ps2eth/smap/ps2smap.irx
else ifneq ($(wildcard iop/__precompiled/ps2smap.irx),)
PS2SMAP_SOURCE := iop/__precompiled/ps2smap.irx
else ifneq ($(wildcard $(PS2SDK)/iop/irx/ps2smap.irx),)
PS2SMAP_SOURCE := $(PS2SDK)/iop/irx/ps2smap.irx
else ifneq ($(wildcard $(PS2SDK)/iop/irx/smap-ps2ip.irx),)
PS2SMAP_SOURCE := $(PS2SDK)/iop/irx/smap-ps2ip.irx
endif

ifeq ($(strip $(PS2SMAP_SOURCE)),)
$(error Missing SMAP driver. Provide $(PS2DEV)/ps2eth/smap/ps2smap.irx, iop/__precompiled/ps2smap.irx, or PS2SDK smap modules.)
endif

$(EE_ASM_DIR)ps2smap_irx.s: $(PS2SMAP_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2smap_irx

$(EE_ASM_DIR)ps2ftpd_irx.s: iop/ps2ftpd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2ftpd_irx

$(EE_ASM_DIR)ps2netfs_irx.s: $(PS2SDK)/iop/irx/ps2netfs.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2netfs_irx

$(EE_ASM_DIR)ps2host_irx.s: iop/ps2host.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2host_irx
endif

ifeq ($(UDPFS),1)
UDPFS_THIRDPARTY_ROOT := thirdparty/nhddl-legacy/iop/udpfs
UDPFS_SMAP_SOURCE :=
UDPFS_MINISTACK_SOURCE :=
UDPFS_IOMAN_SOURCE :=

ifneq ($(wildcard $(UDPFS_THIRDPARTY_ROOT)/smap/Makefile),)
UDPFS_SMAP_SOURCE := iop/__generated/udpfs_smap.irx
endif
ifneq ($(wildcard $(UDPFS_THIRDPARTY_ROOT)/ministack/Makefile),)
UDPFS_MINISTACK_SOURCE := iop/__generated/udpfs_ministack.irx
endif
ifneq ($(wildcard $(UDPFS_THIRDPARTY_ROOT)/udpfs/Makefile),)
UDPFS_IOMAN_SOURCE := iop/__generated/udpfs_ioman.irx
endif

ifeq ($(strip $(UDPFS_SMAP_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/udpfs_smap.irx),)
UDPFS_SMAP_SOURCE := iop/__precompiled/udpfs_smap.irx
endif
endif
ifeq ($(strip $(UDPFS_MINISTACK_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/udpfs_ministack.irx),)
UDPFS_MINISTACK_SOURCE := iop/__precompiled/udpfs_ministack.irx
endif
endif
ifeq ($(strip $(UDPFS_IOMAN_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/udpfs_ioman.irx),)
UDPFS_IOMAN_SOURCE := iop/__precompiled/udpfs_ioman.irx
endif
endif

ifeq ($(strip $(UDPFS_SMAP_SOURCE)),)
$(error Missing udpfs_smap.irx. Provide $(UDPFS_THIRDPARTY_ROOT)/smap sources or iop/__precompiled/udpfs_smap.irx)
endif
ifeq ($(strip $(UDPFS_MINISTACK_SOURCE)),)
$(error Missing udpfs_ministack.irx. Provide $(UDPFS_THIRDPARTY_ROOT)/ministack sources or iop/__precompiled/udpfs_ministack.irx)
endif
ifeq ($(strip $(UDPFS_IOMAN_SOURCE)),)
$(error Missing udpfs_ioman.irx. Provide $(UDPFS_THIRDPARTY_ROOT)/udpfs sources or iop/__precompiled/udpfs_ioman.irx)
endif

ifneq ($(filter iop/__generated/udpfs_smap.irx,$(UDPFS_SMAP_SOURCE)),)
$(UDPFS_SMAP_SOURCE): | iop/__generated
	$(MAKE) -C $(UDPFS_THIRDPARTY_ROOT)/smap \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/udpfs_smap_obj)/ \
		IOP_BIN=udpfs_smap.irx
endif

ifneq ($(filter iop/__generated/udpfs_ministack.irx,$(UDPFS_MINISTACK_SOURCE)),)
$(UDPFS_MINISTACK_SOURCE): | iop/__generated
	$(MAKE) -C $(UDPFS_THIRDPARTY_ROOT)/ministack \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/udpfs_ministack_obj)/ \
		IOP_BIN=udpfs_ministack.irx
endif

ifneq ($(filter iop/__generated/udpfs_ioman.irx,$(UDPFS_IOMAN_SOURCE)),)
$(UDPFS_IOMAN_SOURCE): | iop/__generated
	$(MAKE) -C $(UDPFS_THIRDPARTY_ROOT)/udpfs UDPFS_IOMAN=1 \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/udpfs_ioman_obj)/ \
		IOP_BIN=udpfs_ioman.irx
endif

$(EE_ASM_DIR)udpfs_smap_irx.s: $(UDPFS_SMAP_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ udpfs_smap_irx

$(EE_ASM_DIR)udpfs_ministack_irx.s: $(UDPFS_MINISTACK_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ udpfs_ministack_irx

$(EE_ASM_DIR)udpfs_ioman_irx.s: $(UDPFS_IOMAN_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ udpfs_ioman_irx
endif

iop/ps2ftpd.irx: iop/oldlibs/ps2ftpd
	$(MAKE) -C $<

LOCAL_PS2HDD_OSD_IRX := iop/ps2hdd_osd/ps2hdd-osd.irx
PS2HDD_OSD_SOURCE ?= $(LOCAL_PS2HDD_OSD_IRX)
PS2HDD_OSD_DEPS := $(wildcard iop/ps2hdd_osd/*.[ch]) iop/ps2hdd_osd/imports.lst iop/ps2hdd_osd/Makefile

$(LOCAL_PS2HDD_OSD_IRX): $(PS2HDD_OSD_DEPS)
	$(MAKE) -C iop/ps2hdd_osd

$(EE_ASM_DIR)ps2hdd_irx.s: $(PS2HDD_OSD_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2hdd_irx

$(EE_ASM_DIR)ps2fs_irx.s: $(PS2SDK)/iop/irx/ps2fs.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2fs_irx
	
ifeq ($(DVRP),1)
$(EE_ASM_DIR)ps2atad_irx.s: $(PS2SDK)/iop/irx/ps2atad.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2atad_irx

$(EE_ASM_DIR)dvrdrv_irx.s: $(DVRDRV_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ dvrdrv_irx

$(EE_ASM_DIR)dvrfile_irx.s: $(DVRFILE_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ dvrfile_irx
endif

iop/hdl_info.irx: iop/hdl_info
	$(MAKE) -C $<

$(EE_ASM_DIR)hdl_info_irx.s: iop/hdl_info.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ hdl_info_irx

iop/ps2host.irx: iop/ps2host
	$(MAKE) -C $<

iop/ds34usb/ee/libds34usb.a: iop/ds34usb/ee
	$(MAKE) -C $<

iop/ds34usb.irx: iop/ds34usb/iop
	$(MAKE) -C $<

iop/ds34bt/ee/libds34bt.a: iop/ds34bt/ee
	$(MAKE) -C $<

iop/ds34bt.irx: iop/ds34bt/iop
	$(MAKE) -C $<

$(EE_ASM_DIR)ds34usb.s: iop/ds34usb.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ds34usb_irx

$(EE_OBJS_DIR)libds34usb.a: iop/ds34usb/ee/libds34usb.a
	cp $< $@	

$(EE_OBJS_DIR)libds34bt.a: iop/ds34bt/ee/libds34bt.a
	cp $< $@

$(EE_ASM_DIR)ds34bt.s: iop/ds34bt.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ds34bt_irx

$(EE_ASM_DIR)padman.s: $(PADMAN_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ padman_irx

loader/loader.elf: loader
	$(MAKE) -C $<

$(EE_ASM_DIR)loader_elf.s: loader/loader.elf | $(EE_ASM_DIR)
	$(BIN2S) $< $@ loader_elf

$(EE_ASM_DIR)ps2kbd_irx.s: $(PS2SDK)/iop/irx/ps2kbd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2kbd_irx

$(EE_ASM_DIR)sior_irx.s: $(PS2SDK)/iop/irx/sior.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ sior_irx

$(EE_ASM_DIR)ppctty.s:iop/__precompiled/ppctty.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ppctty_irx

iop/AllowDVDV.irx: iop/AllowDVDV
	$(MAKE) -C $<

$(EE_ASM_DIR)allowdvdv_irx.s: iop/AllowDVDV.irx
	$(BIN2S) $< $@ allowdvdv_irx

ISO_BIN_DIR ?= iso/
ISO_BIN ?= wLE_ISR$(BIN_NAME).iso
ISO_BIN_ELF ?= ISRA_000.00
SYSTEMCNF_VERSION ?= 1.00
SYSTEMCNF_VMODE ?= PAL
ISO_BIN_DUMMYSIZE ?= 64
iso: $(ISO_BIN_DIR) $(ISO_BIN_DIR)system.cnf $(EE_BIN_PKD)
	cp $(EE_BIN_PKD) $(ISO_BIN_DIR)$(ISO_BIN_ELF)
	echo "Extra Build features: $(BIN_NAME)" > "$(ISO_BIN_DIR)BUILD_OPT.TXT"
	dd if=/dev/zero  of="$(ISO_BIN_DIR)DUMMY.BIN"  bs=1M  count=$(ISO_BIN_DUMMYSIZE)
	mkisofs -o "$(ISO_BIN)" "$(ISO_BIN_DIR)"
	$(info ISO generation finished.)
ifeq ($(PACK_ISO), YES)
	$(info ISO will be compressed)
	zip -q -j -9 wLE_ISR_ISO$(BIN_NAME).zip $(ISO_BIN)
endif

isoclean:
	rm -rf $(ISO_BIN_DIR)

$(ISO_BIN_DIR):
	mkdir $@

$(ISO_BIN_DIR)system.cnf: $(ISO_BIN_DIR)
	$(info - generating 'system.cnf' ...)
	echo "BOOT2 = cdrom0:\$(ISO_BIN_ELF);1" >"$@"
	echo VER = $(SYSTEMCNF_VERSION)>>"$@"
	echo VMODE = $(SYSTEMCNF_VMODE)>>"$@"
