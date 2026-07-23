#include "launchelf.h"
#include "init.h"
#include <unistd.h>
#include <iopcontrol_special.h>

#define IMPORT_BIN2C(_n) \
    extern u8 _n[];   \
    extern int size_##_n

//IRX for togleable features
#ifdef ETH
IMPORT_BIN2C(ps2ip_irx);
IMPORT_BIN2C(ps2smap_irx);
IMPORT_BIN2C(ps2host_irx);
IMPORT_BIN2C(ps2netfs_irx);
IMPORT_BIN2C(ps2ftpd_irx);
#endif

#ifdef UDPFS
IMPORT_BIN2C(udpfs_smap_irx);
IMPORT_BIN2C(udpfs_ministack_irx);
IMPORT_BIN2C(udpfs_ioman_irx);
#endif

#ifdef IOPTRAP
IMPORT_BIN2C(ioptrap_irx);
#endif


#ifdef UDPTTY
IMPORT_BIN2C(udptty_irx);
#endif

#ifdef MX4SIO
IMPORT_BIN2C(mx4sio_bd_irx);
#endif

#ifdef HOMEBREW_SIO2MAN
IMPORT_BIN2C(sio2man_irx);
IMPORT_BIN2C(padman_irx);
#endif

#ifdef POWERPC_UART
IMPORT_BIN2C(ppctty_irx);
#endif

IMPORT_BIN2C(usbd_irx);
#ifdef EXFAT
IMPORT_BIN2C(bdm_irx);
IMPORT_BIN2C(bdmfs_fatfs_irx);
IMPORT_BIN2C(usbmass_bd_irx);
IMPORT_BIN2C(ata_bd_irx);
#else
IMPORT_BIN2C(usb_mass_irx);
#endif

#ifdef DS34
IMPORT_BIN2C(ds34usb_irx);
IMPORT_BIN2C(ds34bt_irx);
#endif

#ifdef DVRP
IMPORT_BIN2C(ps2atad_irx);
IMPORT_BIN2C(dvrdrv_irx);
IMPORT_BIN2C(dvrfile_irx);
#endif

#ifdef MMCE
IMPORT_BIN2C(mmceman_irx);
#endif

// Mandatory IRX
IMPORT_BIN2C(iomanx_irx);
IMPORT_BIN2C(filexio_irx);
IMPORT_BIN2C(ps2dev9_irx);
IMPORT_BIN2C(dev9_poweroff_irx);
IMPORT_BIN2C(vmcman_irx);
IMPORT_BIN2C(ps2hdd_irx);
IMPORT_BIN2C(ps2fs_irx);
IMPORT_BIN2C(poweroff_irx);
IMPORT_BIN2C(loader_elf);
IMPORT_BIN2C(iopmod_irx);
IMPORT_BIN2C(cdvd_irx);
IMPORT_BIN2C(xparam_irx);
IMPORT_BIN2C(ps2kbd_irx);
IMPORT_BIN2C(hdl_info_irx);
IMPORT_BIN2C(mcman_irx);
IMPORT_BIN2C(mcserv_irx);
IMPORT_BIN2C(allowdvdv_irx);

/* Runtime init state moved from main.c */
//State of module collections
static u8 have_HDD_modules = 0;
static u8 have_basic_modules = 0;
static u8 have_filexio_ready = 0;
static u8 have_filexio_rwbuf_tuned = 0;
static u8 have_mc_rpc_ready = 0;
static unsigned int iop_reset_generation = 0;

#define USB_MASS_BDMFS_SETTLE_MS 1000

//State of Uncheckable Modules (invalid header)
static u8 have_cdvd = 0;
static u8 have_usbd = 0;
#ifdef DS34
static u8 have_ds34 = 0;
#endif
static u8 have_usb_mass = 0;

static u8 have_ps2smap = 0;
static u8 have_ps2host = 0;
static u8 have_ps2ip = 0;
static u8 have_NetModules = 0;
static u8 have_ps2netfs = 0;
#ifdef UDPFS
static u8 have_udpfs_smap = 0;
static u8 have_udpfs_ministack = 0;
static u8 have_udpfs_ioman = 0;
#endif

static u8 have_ps2ftpd = 0;
static u8 have_ps2kbd = 0;
static u8 have_hdl_info = 0;

//State of Checkable Modules (valid header)
static u8 have_poweroff = 0;
static u8 have_ps2dev9 = 0;
static u8 have_ps2hdd = 0;
static u8 have_ps2fs = 0;
static u8 have_vmcman = 0;
static int vmcman_last_error = 0;
#ifdef EXFAT
static u8 have_bdm = 0;
static u8 have_bdmfs = 0;
static u8 have_ata_bd = 0;
#endif
#ifdef XFROM
static u8 have_Flash_modules = 0;
static u8 xfromserv_loaded = 0;
#endif
static u8 have_secrsif = 0;
static u8 have_exploit_signer_iop = 0;
#ifdef MMCE
static u8 have_mmce = 0;
#endif
#ifdef MX4SIO
static u8 have_mx4sio = 0;
#endif

#ifdef DVRP
static u8 have_DVRP_HDD_modules = 0;
static u8 have_ps2atad = 0;
static u8 have_dvrdrv = 0;
static u8 have_dvrfile = 0;
#endif

//State of whether the UI has been initialized.
//Use this to determine whether code that loads a device's driver(s) can print onto the screen.
static u8 is_early_init = 1;
static u8 done_setupPowerOff = 0;

#define FILEXIO_RWBUF_NET_PRIMARY (128 * 1024)
#define FILEXIO_RWBUF_NET_FALLBACK (64 * 1024)
static u8 ps2kbd_opened = 0;

unsigned int getIopResetGeneration(void)
{
	return iop_reset_generation;
}

/*
 * Tracks driver stacks proven loaded in the current IOP session.
 * Reset() clears these so the next path access can rebuild an accurate view.
 */
enum storage_driver_stack_mode {
	STORAGE_STACK_DEFAULT = 0,
#ifdef MMCE
	STORAGE_STACK_MMCE,
#endif
#ifdef MX4SIO
	STORAGE_STACK_MX4SIO,
#endif
};
static int storage_driver_stack_mode = STORAGE_STACK_DEFAULT;

enum block_storage_stack_mode {
	BLOCK_STACK_NONE = 0,
	BLOCK_STACK_HDD = (1 << 0),
#ifdef EXFAT
	BLOCK_STACK_ATA = (1 << 1),
#endif
};
static int block_storage_stack_mode = BLOCK_STACK_NONE;

#if defined(ETH) && defined(UDPFS)
enum network_stack_mode {
	NETWORK_STACK_NONE = 0,
	NETWORK_STACK_ETH,
	NETWORK_STACK_UDPFS,
};
static int network_stack_mode = NETWORK_STACK_NONE;
#endif

//State of whether DEV9 was successfully loaded or not.
static u8 ps2dev9_loaded = 0;

/* Globals owned by main.c but needed by init flow. */
extern u8 console_is_PSX;
extern int old_cdmode;
extern int uLE_cdmode;
#ifdef MX4SIO
extern u8 mx4sio_driver_running;
#endif

static void delay(int count);
static void initsbv_patches(void);
static void load_ps2dev9(void);
static void prepareDev9Poweroff(void);
static void stopUsbMassForPoweroff(void);
#ifdef ETH
static void load_ps2ip(void);
#endif
static int load_ps2hdd_stack(int with_ata_bd);
static void showLoadingModulesMsg(const char *device_name);
static void showRebootingIopMsg(void);
#ifdef ETH
static void load_ps2ftpd(void);
static void load_ps2netfs(void);
#endif
#ifdef UDPFS
static void load_udpfs_stack(void);
#endif
static void loadCoreCdvdModule(void);
static void loadBasicModules(void);
static int loadExternalFile(char *argPath, void **fileBaseP, int *fileSizeP);
static int loadExternalModule(char *modPath, void *defBase, int defSize);
static void loadUsbDModule(void);
static void waitForUsbMassBdmfsSettle(void);
static void loadKbdModules(void);
static int pathUsesUsbMass(const char *path);
void loadUsbModules(void);
void ensureCoreIoStackReady(void);
void setupPowerOff(void);
void startKbd(void);
void Reset(void);
static void clearIopModuleState(void);
static void resetUsbMassScanState(void);
static void resetUsbMassRuntimeState(void);
static void resetDriverStackLoadTracking(void);
static void resetRuntimeDeviceState(int show_status);
static void switchStorageDriverStack(int target_mode);
static void switchBlockStorageStack(int target_mode);
#if defined(ETH) && defined(UDPFS)
static void switchNetworkStack(int target_mode);
#endif
#ifdef EXFAT
static int loadBdmCoreModules(void);
static int loadAtaBlockDriver(void);
static void pauseAfterAtaBlockDriverLoad(void);
#endif
#ifdef DVRP
static int load_ps2atad_stack(void);
static void switchPsxHddDriverStack(int use_dvr_stack);
#endif
#ifdef DS34
static void stopDs34Input(void);
#endif
void closeAllAndPoweroff(void);
static void poweroffHandler(int i);

void setInitPhaseComplete(void)
{
	is_early_init = 0;
}

void closeKeyboardIfOpened(void)
{
	if (ps2kbd_opened) {
		PS2KbdClose();
		ps2kbd_opened = 0;
	}
}

static void delay(int count)
{
	int i;
	int ret;
	for (i = 0; i < count; i++) {
		ret = 0x01000000;
		while (ret--)
			asm("nop\nnop\nnop\nnop");
	}
}
//------------------------------
//endfunc delay
//---------------------------------------------------------------------------
static void initsbv_patches(void)
{
	DPRINTF("Init MrBrown sbv_patches\n");
	sbv_patch_enable_lmb();
	sbv_patch_disable_prefix_check();
}
//------------------------------
//endfunc initsbv_patches
//---------------------------------------------------------------------------
static void showLoadingModulesMsg(const char *device_name)
{
	char msg[64];

	if (is_early_init)
		return;

	snprintf(msg, sizeof(msg), LNG(Loading_Drivers), device_name);
	drawMsg(msg);
}

static void showRebootingIopMsg(void)
{
	if (!is_early_init)
		drawMsg(LNG(Rebooting_IOP));
}

static void load_ps2dev9(void)
{
	int ID, rcode;

	if (!have_ps2dev9) {
		ID = SifExecModuleBuffer(ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &rcode);
		DPRINTF(" [DEV9]: ID=%d, ret=%d\n", ID, rcode);
		ps2dev9_loaded = (ID >= 0 && rcode == 0);  //DEV9.IRX must have successfully loaded and returned RESIDENT END.
		have_ps2dev9 = 1;
	}
}
//------------------------------
//endfunc load_ps2dev9
//---------------------------------------------------------------------------
static void prepareDev9Poweroff(void)
{
	int ret, ID __attribute__((unused));

	if (!ps2dev9_loaded)
		return;

	ID = SifExecModuleBuffer(dev9_poweroff_irx, size_dev9_poweroff_irx, 0, NULL, &ret);
	DPRINTF(" [DEV9_POWEROFF]: ID=%d, ret=%d\n", ID, ret);
}
//------------------------------
//endfunc prepareDev9Poweroff
//---------------------------------------------------------------------------
static int haveDev9StorageForPoweroff(void)
{
	if (have_ps2hdd || have_ps2fs || have_HDD_modules)
		return 1;
#ifdef EXFAT
	if (have_ata_bd)
		return 1;
#endif
#ifdef DVRP
	if (have_ps2atad || have_DVRP_HDD_modules)
		return 1;
#endif

	return 0;
}
//------------------------------
//endfunc haveDev9StorageForPoweroff
//---------------------------------------------------------------------------
static void idleDev9StorageForPoweroff(void)
{
	if (!haveDev9StorageForPoweroff())
		return;

	fileXioDevctl("hdd0:", HDIOC_IDLEIMM, NULL, 0, NULL, 0);
	fileXioDevctl("hdd1:", HDIOC_IDLEIMM, NULL, 0, NULL, 0);
}
//------------------------------
//endfunc idleDev9StorageForPoweroff
//---------------------------------------------------------------------------
static void stopUsbMassForPoweroff(void)
{
	if (!have_usb_mass)
		return;

#ifdef EXFAT
	/*
	 * With BDMFS active, "mass:" is the shared FAT device prefix. If ATA_BD
	 * is mounted there, USBMASS_DEVCTL_STOP_ALL reaches ata_bd_stop(), which
	 * sends an ATA standby command. After DEV9 shutdown that can sit until an
	 * ATA timeout, so only use this legacy USB stop when ATA_BD is absent.
	 */
	if (have_ata_bd) {
		DPRINTF(" [POWEROFF]: skipping mass stop while ATA_BD is active\n");
		return;
	}
#endif

	fileXioDevctl("mass:", USBMASS_DEVCTL_STOP_ALL, NULL, 0, NULL, 0);
}
//------------------------------
//endfunc stopUsbMassForPoweroff
//---------------------------------------------------------------------------
#ifdef ETH
static void load_ps2ip(void)
{
	int ret, ID __attribute__((unused));

#if defined(ETH) && defined(UDPFS)
	switchNetworkStack(NETWORK_STACK_ETH);
#endif
	load_ps2dev9();
	if (!ps2dev9_loaded) {
		DPRINTF(" [NET]: skipping PS2IP/SMAP because DEV9 failed to initialize\n");
		return;
	}

	if (!have_ps2ip) {
		ID = SifExecModuleBuffer(ps2ip_irx, size_ps2ip_irx, 0, NULL, &ret);
		DPRINTF(" [PS2IP]: ID=%d, ret=%d\n", ID, ret);
		have_ps2ip = (ID >= 0 && ret >= 0);
		if (!have_ps2ip)
			return;
	}
	if (!have_ps2smap) {
		ID = SifExecModuleBuffer(ps2smap_irx, size_ps2smap_irx,
		                    if_conf_len, &if_conf[0], &ret);
		DPRINTF(" [SMAP]: ID=%d, ret=%d\n", ID, ret);
		have_ps2smap = (ID >= 0 && ret >= 0);
	}
}
#endif
//------------------------------
//endfunc load_ps2ip
//---------------------------------------------------------------------------
static int load_ps2hdd_stack(int with_ata_bd)
{
	int ret, ID __attribute__((unused));
	int dev9_ready = 0;
	static char hddarg[] = "-o"
	                       "\0"
	                       "4"
	                       "\0"
	                       "-n"
	                       "\0"
	                       "20";
	/* Allow up to four simultaneously mounted PFS partitions. */
	static char pfsarg[] = "-m"
	                       "\0"
	                       "4"
	                       "\0"
	                       "-o"
	                       "\0"
	                       "10"
	                       "\0"
	                       "-n"
	                       "\0"
	                       "40";

#ifdef EXFAT
	/*
	 * Match nhddl-rewrite stack ordering:
	 * BDM -> BDMFS -> DEV9 -> ATA_BD -> short pause -> PS2HDD/PS2FS.
	 */
	if (with_ata_bd && !loadAtaBlockDriver()) {
		DPRINTF(" [HDD]: skipping HDD/FS load because ATA_BD stack did not initialize\n");
		return 0;
	}
	if (with_ata_bd)
		dev9_ready = ps2dev9_loaded;
#endif
#ifndef EXFAT
	(void)with_ata_bd;
#endif
	if (!dev9_ready) {
		load_ps2dev9();
		if (!ps2dev9_loaded) {
			DPRINTF(" [HDD]: skipping HDD/FS load because DEV9 failed to initialize\n");
			return 0;
		}
	}

	if (!have_ps2hdd) {
		ID = SifExecModuleBuffer(ps2hdd_irx, size_ps2hdd_irx, sizeof(hddarg), hddarg, &ret);
		DPRINTF(" [PS2HDD]: ID=%d, ret=%d\n", ID, ret);
		have_ps2hdd = (ID >= 0 && ret >= 0);
	}
	if (have_ps2hdd && !have_ps2fs) {
		ID = SifExecModuleBuffer(ps2fs_irx, size_ps2fs_irx, sizeof(pfsarg), pfsarg, &ret);
		DPRINTF(" [PS2FS]: ID=%d, ret=%d\n", ID, ret);
		have_ps2fs = (ID >= 0 && ret >= 0);
	}
	if (have_ps2hdd && have_ps2fs)
		prepareDev9Poweroff();

	return (have_ps2hdd && have_ps2fs);
}
//------------------------------
//endfunc load_ps2hdd_stack
//---------------------------------------------------------------------------
#ifdef DVRP
static int load_ps2atad_stack(void)
{
	int ret, ID __attribute__((unused));
	static char hddarg[] = "-o"
	                       "\0"
	                       "4"
	                       "\0"
	                       "-n"
	                       "\0"
	                       "20";
	static char pfsarg[] = "-m"
	                       "\0"
	                       "4"
	                       "\0"
	                       "-o"
	                       "\0"
	                       "10"
	                       "\0"
	                       "-n"
	                       "\0"
	                       "40";

	load_ps2dev9();
	if (!ps2dev9_loaded) {
		DPRINTF(" [ATAD]: skipping load because DEV9 failed to initialize\n");
		return 0;
	}

	if (!have_ps2atad) {
		ID = SifExecModuleBuffer(ps2atad_irx, size_ps2atad_irx, 0, NULL, &ret);
		DPRINTF(" [PS2ATAD]: ID=%d, ret=%d\n", ID, ret);
		have_ps2atad = (ID >= 0 && ret >= 0);
	}
	if (!have_ps2atad)
		return 0;

	if (!have_ps2hdd) {
		ID = SifExecModuleBuffer(ps2hdd_irx, size_ps2hdd_irx, sizeof(hddarg), hddarg, &ret);
		DPRINTF(" [PS2HDD]: ID=%d, ret=%d\n", ID, ret);
		have_ps2hdd = (ID >= 0 && ret >= 0);
	}
	if (have_ps2hdd && !have_ps2fs) {
		ID = SifExecModuleBuffer(ps2fs_irx, size_ps2fs_irx, sizeof(pfsarg), pfsarg, &ret);
		DPRINTF(" [PS2FS]: ID=%d, ret=%d\n", ID, ret);
		have_ps2fs = (ID >= 0 && ret >= 0);
	}
	if (have_ps2hdd && have_ps2fs)
		prepareDev9Poweroff();

	return (have_ps2hdd && have_ps2fs);
}
//------------------------------
//endfunc load_ps2atad_stack
//---------------------------------------------------------------------------
#endif
IMPORT_BIN2C(secrsif_irx);
IMPORT_BIN2C(exploit_ioprp_img);
int loadSecrSifModule(void)
{
	int ID __attribute__((unused)), ret;

	ensureCoreIoStackReady();
	if (!have_secrsif) {
		ID = SifExecModuleBuffer(secrsif_irx, size_secrsif_irx, 0, NULL, &ret);
		DPRINTF(" [SECRSIF]: ID=%d, ret=%d\n", ID, ret);
		have_secrsif = (ID >= 0 && ret >= 0);
	}

	return have_secrsif;
}
//------------------------------
//endfunc loadSecrSifModule
//---------------------------------------------------------------------------
int prepareExploitSignerIop(void)
{
#ifdef NO_IOP_RESET
	return 0;
#else
	int ret;

	if (have_exploit_signer_iop) {
		ensureCoreIoStackReady();
#ifdef DS34
		loadDs34InputModules();
#endif
		setupPad();
		return loadSecrSifModule();
	}

#ifdef DS34
	stopDs34Input();
#endif
	closeKeyboardIfOpened();
	unmountAll();
	showRebootingIopMsg();

	SifInitRpc(0);
	ret = SifIopRebootBuffer(exploit_ioprp_img, size_exploit_ioprp_img);
	if (ret <= 0) {
		SifInitRpc(0);
		SifLoadFileInit();
		initsbv_patches();
		clearIopModuleState();
		iop_reset_generation++;
		ensureCoreIoStackReady();
#ifdef DS34
		loadDs34InputModules();
#endif
		setupPad();
		return 0;
	}
	while (!SifIopSync()) {
	};
	SifInitRpc(0);
	SifLoadFileInit();
	initsbv_patches();

	clearIopModuleState();
	iop_reset_generation++;
	ensureCoreIoStackReady();
#ifdef DS34
	loadDs34InputModules();
#endif
	setupPad();
	if (!loadSecrSifModule())
		return 0;

	have_exploit_signer_iop = 1;
	DPRINTF(" [EXPLOIT_IOP]: signer IOP ready\n");
	return 1;
#endif
}
//------------------------------
//endfunc prepareExploitSignerIop
//---------------------------------------------------------------------------
#ifdef XFROM
IMPORT_BIN2C(extflash_irx);
IMPORT_BIN2C(xfromman_irx);
IMPORT_BIN2C(xfromserv_irx);
static void load_pflash(void)
{
	int ID __attribute__((unused)), ret;
	ID = SifExecModuleBuffer(extflash_irx, size_extflash_irx, 0, NULL, &ret);
	DPRINTF(" [PFLASH]: ID=%d, ret=%d\n", ID, ret);
	ID = SifExecModuleBuffer(xfromman_irx, size_xfromman_irx, 0, NULL, &ret);
	DPRINTF(" [XFROMMAN]: ID=%d, ret=%d\n", ID, ret);
	ID = SifExecModuleBuffer(xfromserv_irx, size_xfromserv_irx, 0, NULL, &ret);
	DPRINTF(" [XFROMSERV]: ID=%d, ret=%d\n", ID, ret);
	xfromserv_loaded = (ID >= 0 && ret == 0);
	if (xfromserv_loaded)
		xfromInit(MC_TYPE_MC);
}
//------------------------------
//endfunc load_pflash
//---------------------------------------------------------------------------
#endif
#ifdef ETH
void load_ps2host(void)
{
	int ret, ID __attribute__((unused));

#if defined(ETH) && defined(UDPFS)
	switchNetworkStack(NETWORK_STACK_ETH);
#endif
	if (!have_ps2host || !have_ps2ip || !have_ps2smap || !ps2dev9_loaded)
		showLoadingModulesMsg("host");

	ensureCoreIoStackReady();
	setupPowerOff();  //resolves the stall out when opening host: from LaunchELF's FileBrowser
	load_ps2ip();
	if (!(have_ps2ip && have_ps2smap))
		return;
	if (!have_ps2host) {
		ID = SifExecModuleBuffer(ps2host_irx, size_ps2host_irx, 0, NULL, &ret);
		DPRINTF(" [PS2HOST]: ID=%d, ret=%d\n", ID, ret);
		have_ps2host = (ID >= 0 && ret >= 0);
	}
}
//------------------------------
//endfunc load_ps2host
//---------------------------------------------------------------------------
#endif

#ifdef UDPFS
static void load_udpfs_stack(void)
{
	int ret, ID __attribute__((unused));
	char ministack_arg[32];

#if defined(ETH) && defined(UDPFS)
	switchNetworkStack(NETWORK_STACK_UDPFS);
#endif
	if (!have_udpfs_smap || !have_udpfs_ministack || !have_udpfs_ioman || !ps2dev9_loaded)
		showLoadingModulesMsg("udpfs");

	ensureCoreIoStackReady();
	setupPowerOff();
	getIpConfig();
	load_ps2dev9();
	if (!ps2dev9_loaded) {
		DPRINTF(" [UDPFS]: skipping load because DEV9 failed to initialize\n");
		return;
	}

	if (!have_udpfs_smap) {
		ID = SifExecModuleBuffer(udpfs_smap_irx, size_udpfs_smap_irx, 0, NULL, &ret);
		DPRINTF(" [UDPFS_SMAP]: ID=%d, ret=%d\n", ID, ret);
		have_udpfs_smap = (ID >= 0 && ret >= 0);
		if (!have_udpfs_smap)
			return;
	}

	if (!have_udpfs_ministack) {
		snprintf(ministack_arg, sizeof(ministack_arg), "ip=%s", ip);
		ID = SifExecModuleBuffer(udpfs_ministack_irx, size_udpfs_ministack_irx,
		                         (int)strlen(ministack_arg) + 1, ministack_arg, &ret);
		DPRINTF(" [UDPFS_MINISTACK]: ID=%d, ret=%d, arg=%s\n", ID, ret, ministack_arg);
		have_udpfs_ministack = (ID >= 0 && ret >= 0);
		if (!have_udpfs_ministack)
			return;
	}

	if (!have_udpfs_ioman) {
		ID = SifExecModuleBuffer(udpfs_ioman_irx, size_udpfs_ioman_irx, 0, NULL, &ret);
		DPRINTF(" [UDPFS_IOMAN]: ID=%d, ret=%d\n", ID, ret);
		have_udpfs_ioman = (ID >= 0 && ret >= 0);
	}
}

int load_udpfs(void)
{
	load_udpfs_stack();
	return have_udpfs_ioman;
}
//------------------------------
//endfunc load_udpfs
//---------------------------------------------------------------------------
int reloadUdpfsModules(void)
{
	resetRuntimeDeviceState(TRUE);
	load_udpfs_stack();
	return have_udpfs_ioman;
}
//------------------------------
//endfunc reloadUdpfsModules
//---------------------------------------------------------------------------
#endif
#ifdef DVRP
static void load_ps2dvr(void)
{
	int ret, ID __attribute__((unused));

	if (!have_dvrdrv || !have_dvrfile || !have_ps2hdd || !have_ps2fs)
		showLoadingModulesMsg("dvr");

	if (!load_ps2hdd_stack(1)) {
		DPRINTF(" [DVR]: skipping load because HDD stack failed to initialize\n");
		return;
	}

	if (!have_dvrdrv) {
		ID = SifExecModuleBuffer(dvrdrv_irx, size_dvrdrv_irx, 0, NULL, &ret);
		DPRINTF(" [DVRDRV]: ID=%d, ret=%d\n", ID, ret);
		have_dvrdrv = (ID >= 0 && ret >= 0);
	}
	if (have_dvrdrv && !have_dvrfile) {
		ID = SifExecModuleBuffer(dvrfile_irx, size_dvrfile_irx, 0, NULL, &ret);
		DPRINTF(" [DVRFILE]: ID=%d, ret=%d\n", ID, ret);
		have_dvrfile = (ID >= 0 && ret >= 0);
	}
}
//------------------------------
//endfunc load_ps2dvr
//---------------------------------------------------------------------------
#endif

static int vmcmanDeviceRegistered(void)
{
	int fd;

	DPRINTF(" [VMCMAN]: probing vmc0:\n");
	fd = fileXioDopen("vmc0:");
	if (fd >= 0) {
		fileXioDclose(fd);
		vmcman_last_error = 0;
		DPRINTF(" [VMCMAN]: vmc0: registered fd=%d\n", fd);
		return TRUE;
	}

	/* An unmounted but registered vmc device returns NOT_MOUNT, not ENODEV. */
	DPRINTF(" [VMCMAN]: device probe ret=%d\n", fd);
	if (fd != -ENODEV) {
		vmcman_last_error = 0;
		DPRINTF(" [VMCMAN]: vmc0: registered via ret=%d\n", fd);
		return TRUE;
	}
	vmcman_last_error = fd;
	DPRINTF(" [VMCMAN]: vmc0: not registered ret=%d\n", fd);
	return FALSE;
}

static int load_vmcman_module(void)
{
	int ret = 0, ID __attribute__((unused));

	if (!have_vmcman) {
		DPRINTF(" [VMCMAN]: loading size=%d\n", size_vmcman_irx);
		ID = SifExecModuleBuffer(vmcman_irx, size_vmcman_irx, 0, NULL, &ret);
		DPRINTF(" [VMCMAN]: ID=%d, ret=%d\n", ID, ret);
		if (ID < 0) {
			vmcman_last_error = ID;
			return 0;
		}
		have_vmcman = 1;
		vmcman_last_error = 0;
		if (ret < 0)
			DPRINTF(" [VMCMAN]: start ret=%d; probing vmc: anyway\n", ret);
	}
	return have_vmcman;
}

int load_vmcman(void)
{
	ensureCoreIoStackReady();
	if (!load_vmcman_module()) {
		DPRINTF(" [VMCMAN]: module load failed err=%d\n", vmcman_last_error);
		return 0;
	}
	if (!vmcmanDeviceRegistered()) {
		DPRINTF(" [VMCMAN]: probe failed err=%d\n", vmcman_last_error);
		have_vmcman = 0;
	}
	return have_vmcman;
}

int get_vmcman_last_error(void)
{
	return vmcman_last_error;
}
//------------------------------
//endfunc load_vmcman
//---------------------------------------------------------------------------
#ifdef ETH
static void load_ps2ftpd(void)
{
	int ret, ID __attribute__((unused));
	int arglen;
	char *arg_p;

	arg_p = "-anonymous";
	arglen = strlen(arg_p);

	load_ps2ip();
	if (!(have_ps2ip && have_ps2smap))
		return;
	if (!have_ps2ftpd) {
		ID = SifExecModuleBuffer(ps2ftpd_irx, size_ps2ftpd_irx, arglen, arg_p, &ret);
		DPRINTF(" [PS2FTPD]: ID=%d, ret=%d\n", ID, ret);
		have_ps2ftpd = (ID >= 0 && ret >= 0);
	}
}
#endif
//------------------------------
//endfunc load_ps2ftpd
//---------------------------------------------------------------------------
#ifdef ETH
static void load_ps2netfs(void)
{
	int ret, ID __attribute__((unused));

	load_ps2ip();
	if (!(have_ps2ip && have_ps2smap))
		return;
	if (!have_ps2netfs) {
		ID = SifExecModuleBuffer(ps2netfs_irx, size_ps2netfs_irx, 0, NULL, &ret);
		DPRINTF(" [NETFS]: ID=%d, ret=%d\n", ID, ret);
		have_ps2netfs = (ID >= 0 && ret >= 0);
	}
}
#endif
//------------------------------
//endfunc load_ps2netfs
//---------------------------------------------------------------------------
static void loadBasicModules(void)
{
	int ret, id __attribute__((unused));

	if (have_basic_modules)
		return;

#ifdef IOPTRAP
	id = SifExecModuleBuffer(ioptrap_irx, size_ioptrap_irx, 0, NULL, &ret);
	DPRINTF(" [IOP TRAP]: id=%d ret=%d\n", id, ret);
#endif

	id = SifExecModuleBuffer(iomanx_irx, size_iomanx_irx, 0, NULL, &ret);
	DPRINTF(" [IOMANX]: id=%d ret=%d\n", id, ret);
	id = SifExecModuleBuffer(filexio_irx, size_filexio_irx, 0, NULL, &ret);
	DPRINTF(" [FILEXIO]: id=%d ret=%d\n", id, ret);

	id = SifExecModuleBuffer(allowdvdv_irx, size_allowdvdv_irx, 0, NULL, &ret);  //unlocks cdvd for reading on psx dvr
	DPRINTF(" [ALLOWDVD]: id=%d ret=%d\n", id, ret);
#ifdef HOMEBREW_SIO2MAN
	id = SifExecModuleBuffer(sio2man_irx, size_sio2man_irx, 0, NULL, &ret);
	DPRINTF(" [SIO2MAN]: id=%d ret=%d\n", id, ret);
#else
	id = SifLoadModule("rom0:SIO2MAN", 0, NULL);
	DPRINTF(" [rom0:SIO2MAN]: id=%d\n", id);
#endif

#if defined(SIO_DEBUG)
	// I call this just after SIO2MAN have been loaded
	sio_init(38400, 0, 0, 0, 0);
#endif

#if defined(SIO_DEBUG)
	DPRINTF("Hello from EE SIO!\n");
#endif

	id = SifExecModuleBuffer(mcman_irx, size_mcman_irx, 0, NULL, &ret);  //Home
	DPRINTF(" [MCMAN]: id=%d ret=%d\n", id, ret);
	//SifLoadModule("rom0:MCMAN", 0, NULL); //Sony
	id = SifExecModuleBuffer(mcserv_irx, size_mcserv_irx, 0, NULL, &ret);  //Home
	DPRINTF(" [MCSERV]: id=%d ret=%d\n", id, ret);
	//SifLoadModule("rom0:MCSERV", 0, NULL); //Sony
#ifdef HOMEBREW_SIO2MAN
	id = SifExecModuleBuffer(padman_irx, size_padman_irx, 0, NULL, &ret);  //Home
	DPRINTF(" [PADMAN]: id=%d ret=%d\n", id, ret);
#else
	id = SifLoadModule("rom0:PADMAN", 0, NULL);
	DPRINTF(" [rom0:PADMAN]: id=%d\n", id);
#endif

	loadCoreCdvdModule();
	have_basic_modules = 1;
}
//------------------------------
//endfunc loadBasicModules
//---------------------------------------------------------------------------
void ensureCoreIoStackReady(void)
{
	loadBasicModules();

	if (!have_filexio_ready) {
		fileXioInit();
		/*
		 * Use a larger FILEXIO IOP-side RW buffer for better throughput.
		 * This mainly helps network filesystems (UDPFS/HOST) by reducing
		 * tiny 16KB IOP write batches.
		 */
		if (!have_filexio_rwbuf_tuned) {
			int rwbuf_ret = fileXioSetRWBufferSize(FILEXIO_RWBUF_NET_PRIMARY);
			if (rwbuf_ret < 0) {
				rwbuf_ret = fileXioSetRWBufferSize(FILEXIO_RWBUF_NET_FALLBACK);
				if (rwbuf_ret < 0) {
					DPRINTF(" [FILEXIO_RWB]: keeping default (ret=%d)\n", rwbuf_ret);
				} else {
					DPRINTF(" [FILEXIO_RWB]: set %d bytes\n", FILEXIO_RWBUF_NET_FALLBACK);
					have_filexio_rwbuf_tuned = 1;
				}
			} else {
				DPRINTF(" [FILEXIO_RWB]: set %d bytes\n", FILEXIO_RWBUF_NET_PRIMARY);
				have_filexio_rwbuf_tuned = 1;
			}
		}
		have_filexio_ready = 1;
	}

	if (!have_mc_rpc_ready) {
		DPRINTF("Initializing mc rpc\n");
#ifdef HOMEBREW_SIO2MAN
		mcInit(MC_TYPE_XMC);
#else
		mcInit(MC_TYPE_MC);
#endif
		have_mc_rpc_ready = 1;
	}

	load_vmcman_module();
}
//------------------------------
//endfunc ensureCoreIoStackReady
//---------------------------------------------------------------------------
static void loadCoreCdvdModule(void)
{
	int ret, id __attribute__((unused));

	if (!have_cdvd) {
		sceCdInit(SCECdINoD);  // SCECdINoD init without check for a disc. Reduces risk of a lockup if the drive is in a erroneous state.
		id = SifExecModuleBuffer(cdvd_irx, size_cdvd_irx, 0, NULL, &ret);
		LCDVD_INIT();
		DPRINTF(" [CDVD]: id=%d, ret=%d\n", id, ret);
		have_cdvd = 1;
	}
}
//---------------------------------------------------------------------------
void loadCdModules(void)
{
	loadBasicModules();
	loadCoreCdvdModule();
}
//------------------------------
//endfunc loadCdModules
//---------------------------------------------------------------------------
void applyXPARAM(const char *gameID)
{
	int ret, id __attribute__((unused));

	if (gameID == NULL || gameID[0] == '\0')
		return;

	id = SifExecModuleBuffer(xparam_irx, size_xparam_irx, strlen(gameID) + 1, (char *)gameID, &ret);
	DPRINTF(" [XPARAM]: id=%d ret=%d title='%s'\n", id, ret, gameID);
}
//---------------------------------------------------------------------------
int uLE_cdDiscValid(void)  //returns 1 if disc valid, else returns 0
{
	cdmode = sceCdGetDiskType();

	switch (cdmode) {
		case SCECdPSCD:
		case SCECdPSCDDA:
		case SCECdPS2CD:
		case SCECdPS2CDDA:
		case SCECdPS2DVD:
		//	case SCECdESRDVD_0:
		//	case SCECdESRDVD_1:
		case SCECdCDDA:
		case SCECdDVDV:
			return 1;
		case SCECdNODISC:
		case SCECdDETCT:
		case SCECdDETCTCD:
		case SCECdDETCTDVDS:
		case SCECdDETCTDVDD:
		case SCECdUNKNOWN:
		case SCECdIllegalMedia:
		default:
			return 0;
	}
}
//------------------------------
//endfunc uLE_cdDiscValid
//---------------------------------------------------------------------------
int uLE_cdStop(void)
{
	int test;

	old_cdmode = cdmode;
	test = uLE_cdDiscValid();
	uLE_cdmode = cdmode;
	if (test) {                     //if stable detection of a real disc is achieved
		if ((cdmode != old_cdmode)  //if this was a new detection
		    && ((cdmode == SCECdDVDV) || (cdmode == SCECdPS2DVD))) {
			test = Check_ESR_Disc();
			DPRINTF("Check_ESR_Disc => %d\n", test);
			if (test > 0) {  //ESR Disc ?
				uLE_cdmode = (cdmode == SCECdPS2DVD) ? SCECdESRDVD_1 : SCECdESRDVD_0;
			}
		}
		LCDVD_STOP();
		sceCdSync(0);
	}
	return uLE_cdmode;
}
//------------------------------
//endfunc uLE_cdStop
//---------------------------------------------------------------------------
static void getExternalFilePath(const char *argPath, char *filePath)
{
	char *pathSep;

	pathSep = strchr(argPath, '/');

	if (!strncmp(argPath, "mass", 4)) {
		//Loading some module from USB mass:
		//This won't be for USB drivers, as mass: must first be accessible.
		strcpy(filePath, argPath);
		if (pathSep && (pathSep - argPath < 7) && pathSep[-1] == ':')
			strcpy(filePath + (pathSep - argPath), pathSep + 1);

	} else if (!strncmp(argPath, "hdd", 3) && argPath[3] >= '0' && argPath[3] <= '9' && argPath[4] == ':' && argPath[5] == '/') {
		//Loading some module from HDD
		char party[MAX_PATH];
		char *p;

		loadHddModules();
		sprintf(party, "hdd%c:%s", argPath[3], argPath + 6);
		p = strchr(party, '/');
		sprintf(filePath, "pfs0:%s", p);
		*p = 0;
		mountParty(party);
#ifdef DVRP
	} else if (!strncmp(argPath, "dvr_hdd0:/", 10)) {
		if (!console_is_PSX) {
			genFixPath(argPath, filePath);
			return;
		}
		//Loading some module from HDD
		char party[MAX_PATH];
		char *p;

		loadDVRPHddModules();
		sprintf(party, "dvr_hdd0:%s", argPath + 10);
		p = strchr(party, '/');
		sprintf(filePath, "dvr_pfs0:%s", p);
		*p = 0;
		filePath[7] = (getDVRPPartyMountIndex(party) == 1) ? '1' : '0';
		mountDVRPParty(party);

#endif
	} else if (!strncmp(argPath, "cdfs", 4)) {
		loadCdModules();
		strcpy(filePath, argPath);
		LCDVD_FLUSHCACHE();
		LCDVD_DISKREADY(0);
	} else {
		genFixPath(argPath, filePath);
	}
}
//------------------------------
//endfunc getExternalFilePath
//---------------------------------------------------------------------------
// loadExternalFile below will use the given path, and read the
// indicated file into a buffer it allocates for that purpose.
// The buffer address and size are returned via pointer variables,
// and the size is also returned as function value. Both those
// instances of size will be forced to Zero if any error occurs,
// and in such cases the buffer pointer returned will be NULL.
// NB: Release of the allocated memory block, if/when it is not
// needed anymore, is entirely the responsibility of the caller,
// though, of course, none is allocated if the file is not found.
//---------------------------------------------------------------------------
static int loadExternalFile(char *argPath, void **fileBaseP, int *fileSizeP)
{  //The first three variables are local variants similar to the arguments
	char filePath[MAX_PATH];
	void *fileBase;
	int fileSize, fd, seek_size, read_size, total, rd;

	fileBase = NULL;
	fileSize = 0;

	getExternalFilePath(argPath, filePath);

	//Here 'filePath' is a valid path for file I/O operations
	//Which means we can now use generic fileXio I/O
	fd = genOpen(filePath, FIO_O_RDONLY);
	if (fd >= 0) {
		seek_size = genLseek(fd, 0, SEEK_END);
		if (seek_size > 0 && genLseek(fd, 0, SEEK_SET) >= 0) {
			fileSize = seek_size;
			fileBase = memalign(64, fileSize);
			if (fileBase != NULL) {
				total = 0;
				while (total < fileSize) {
					read_size = fileSize - total;
					rd = genRead(fd, (u8 *)fileBase + total, read_size);
					if (rd <= 0)
						break;
					total += rd;
				}
				if (total != fileSize) {
					free(fileBase);
					fileBase = NULL;
					fileSize = 0;
				}
			} else {
				fileSize = 0;
			}
		}
		genClose(fd);
	}
	*fileBaseP = fileBase;
	*fileSizeP = fileSize;
	return fileSize;
}
//------------------------------
//endfunc loadExternalFile
//---------------------------------------------------------------------------
// loadExternalModule below will use the given path and attempt
// to load the indicated module. If the file is not
// found, or some error occurs in its reading,
// then the default internal module specified by the 2nd and 3rd
// arguments will be used, except if the base is NULL or the size
// is zero, in which case a value of 0 is returned. A value of
// 0 is also returned if loading of default module fails. But
// normally the value returned will be 1 for an internal default
// module, but 2 for an external module..
//---------------------------------------------------------------------------
static int loadExternalModule(char *modPath, void *defBase, int defSize)
{
	char filePath[MAX_PATH];
	int ext_OK, def_OK;  //Flags success for external and default module
	int dummy;
	DPRINTF("%s: looking for [%s]\n", __FUNCTION__, modPath);
	ext_OK = 0;
	def_OK = 0;
	getExternalFilePath(modPath, filePath);

	ext_OK = (SifLoadModule(filePath, 0, NULL) >= 0);
	if (!ext_OK) {
		DPRINTF("\tNot found!\n");
		if (defBase && defSize) {
			def_OK = SifExecModuleBuffer(defBase, defSize, 0, NULL, &dummy);
			DPRINTF("\tLoaded default embedded version: ID=%d, ret=%d\n", def_OK, dummy);
		}
	}
	if (ext_OK)
		return 2;
	if (def_OK)
		return 1;
	return 0;
}
//------------------------------
//endfunc loadExternalModule
//---------------------------------------------------------------------------
static void loadUsbDModule(void)
{
	int ret, ID __attribute__((unused));

	DPRINTF(" [USBD]:\n");
	if (!have_usbd) {
		ID = SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, &ret);
		DPRINTF(" [USBD] ID=%d, ret=%d\n", ID, ret);
		have_usbd = (ID >= 0 && ret >= 0);
	}
}
//------------------------------
//endfunc loadUsbDModule
//---------------------------------------------------------------------------
static void waitForUsbMassBdmfsSettle(void)
{
	u64 ready_time;

	ready_time = Timer() + USB_MASS_BDMFS_SETTLE_MS;
	while (Timer() < ready_time) {
	}
}
//------------------------------
//endfunc waitForUsbMassBdmfsSettle
//---------------------------------------------------------------------------
static int pathUsesUsbMass(const char *path)
{
	if (path == NULL)
		return 0;
	if (!strncmp(path, "uLE:/", 5))
		return pathUsesUsbMass(LaunchElfDir);
	if (!strncmp(path, "mass", 4) && (path[4] == ':' || (path[4] >= '0' && path[4] <= '9' && path[5] == ':')))
		return 1;
	if (!strncmp(path, "usb", 3) && (path[3] == ':' || (path[3] >= '0' && path[3] <= '9' && path[4] == ':')))
		return 1;
	return 0;
}
//------------------------------
//endfunc pathUsesUsbMass
//---------------------------------------------------------------------------
#ifdef DS34
static void loadDs34Modules(void)
{
	DPRINTF("Loading DS34\n");
	if (!have_ds34 &&
	    loadExternalModule("DS34USB.IRX", &ds34usb_irx, size_ds34usb_irx) &&
	    loadExternalModule("DS34BT.IRX", &ds34bt_irx, size_ds34bt_irx))
		have_ds34 = 1;
}
//------------------------------
//endfunc loadDs34Modules
//---------------------------------------------------------------------------
void loadDs34InputModules(void)
{
	if (!have_ds34)
		showLoadingModulesMsg("ds34");

	ensureCoreIoStackReady();
	loadUsbDModule();
	loadDs34Modules();
}
//------------------------------
//endfunc loadDs34InputModules
//---------------------------------------------------------------------------
#endif
void loadUsbModules(void)
{
	int ret, ID __attribute__((unused));

	if (!have_usbd || !have_usb_mass)
		showLoadingModulesMsg("usb");

	ensureCoreIoStackReady();
	loadUsbDModule();

#ifdef EXFAT
	if (have_usbd && !have_usb_mass) {
		int loaded_ok = 1;
		/*
		 * EXFAT builds use the embedded BDM stack.
		 * There is no dedicated external override path in current config.
		 */
		if (!have_bdm) {
			ID = SifExecModuleBuffer(bdm_irx, size_bdm_irx, 0, NULL, &ret);
			DPRINTF(" [BDM] ID=%d, ret=%d\n", ID, ret);
			have_bdm = (ID >= 0 && ret >= 0);
		}
		if (!have_bdm)
			loaded_ok = 0;
		if (loaded_ok && !have_bdmfs) {
			ID = SifExecModuleBuffer(bdmfs_fatfs_irx, size_bdmfs_fatfs_irx, 0, NULL, &ret);
			DPRINTF(" [BDMFS_FATFS] ID=%d, ret=%d\n", ID, ret);
			have_bdmfs = (ID >= 0 && ret >= 0);
			if (have_bdmfs)
				waitForUsbMassBdmfsSettle();
		}
		if (!have_bdmfs)
			loaded_ok = 0;
		if (loaded_ok) {
			ID = SifExecModuleBuffer(usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL, &ret);
			DPRINTF(" [USBMASS_BD] ID=%d, ret=%d\n", ID, ret);
			if (ID < 0 || ret < 0)
				loaded_ok = 0;
		}
		if (loaded_ok) {
			delay(3);
			USB_mass_loaded = 1;
			have_usb_mass = 1;
		}
	}
#else
	if (have_usbd && !have_usb_mass) {
		ID = SifExecModuleBuffer(usb_mass_irx, size_usb_mass_irx, 0, NULL, &ret);
		DPRINTF(" [USBMASS] ID=%d, ret=%d\n", ID, ret);
		if (ID >= 0 && ret >= 0) {
			delay(3);
			USB_mass_loaded = 1;  //internal embedded driver
			have_usb_mass = 1;
		}
	}
#endif

	if (USB_mass_loaded == 1)                       //if using the internal mass driver
		USB_mass_max_drives = USB_MASS_MAX_DRIVES;  //allow multiple drives
	else
		USB_mass_max_drives = 1;  //else allow only one mass drive

#ifdef DS34
	loadDs34Modules();
#endif
}
//------------------------------
//endfunc loadUsbModules
//---------------------------------------------------------------------------
static void loadKbdModules(void)
{
	if (pathUsesUsbMass(setting->usbkbd_file))
		loadUsbModules();
	if (!have_usbd || !have_ps2kbd)
		showLoadingModulesMsg("usbkbd");

	ensureCoreIoStackReady();
	loadUsbDModule();
	if ((have_usbd && !have_ps2kbd) && (loadExternalModule(setting->usbkbd_file, &ps2kbd_irx, size_ps2kbd_irx)))
		have_ps2kbd = 1;
}
//------------------------------
//endfunc loadKbdModules
//---------------------------------------------------------------------------
static void resetUsbMassScanState(void)
{
	USB_mass_scanned = 0;
	USB_mass_scan_time = 0;
	memset(USB_mass_ix, 0, sizeof(USB_mass_ix));
	USB_mass_ix[0] = '0';
}

static void resetUsbMassRuntimeState(void)
{
	USB_mass_loaded = 0;
	resetUsbMassScanState();
}

static void resetDriverStackLoadTracking(void)
{
	storage_driver_stack_mode = STORAGE_STACK_DEFAULT;
	block_storage_stack_mode = BLOCK_STACK_NONE;
#if defined(ETH) && defined(UDPFS)
	network_stack_mode = NETWORK_STACK_NONE;
#endif
}

#ifdef DS34
static void stopDs34Input(void)
{
	if (is_early_init)
		return;

	WaitSema(semRunning);
	isRunning = 0;
	SignalSema(semRunning);
	WaitSema(semFinish);
	ds34usb_reset();
	ds34bt_reset();
}
#endif

static void resetRuntimeDeviceState(int show_status)
{
#ifdef DS34
	stopDs34Input();
#endif
	if (show_status)
		showRebootingIopMsg();
	unmountAll();
	Reset();
#ifdef DS34
	loadDs34InputModules();
#endif
	setupPad();
	DPRINTF("Starting keyboard\n");
	startKbd();
}

void rebootIopAndReloadCoreStack(void)
{
	resetRuntimeDeviceState(TRUE);
}

void rebootIopAndReloadCoreStackSilent(void)
{
	resetRuntimeDeviceState(FALSE);
}

static void switchStorageDriverStack(int target_mode)
{
#if defined(MMCE) || defined(MX4SIO)
	if (storage_driver_stack_mode == target_mode)
		return;

	if (storage_driver_stack_mode != STORAGE_STACK_DEFAULT) {
		DPRINTF("Switching storage driver stack (%d -> %d), resetting IOP\n", storage_driver_stack_mode, target_mode);
		resetRuntimeDeviceState(TRUE);
	}
#else
	(void)target_mode;
#endif
}

static void switchBlockStorageStack(int target_mode)
{
	if ((block_storage_stack_mode & target_mode) == target_mode)
		return;

#ifdef EXFAT
	/*
	 * ATA_BD + APA stack can coexist in one IOP session.
	 * Only force a reset for genuinely incompatible future stacks.
	 */
	if ((block_storage_stack_mode | target_mode) == (BLOCK_STACK_HDD | BLOCK_STACK_ATA)) {
		return;
	}
#endif

	if (block_storage_stack_mode != BLOCK_STACK_NONE) {
		DPRINTF("Switching block storage stack (%d -> %d), resetting IOP\n", block_storage_stack_mode, target_mode);
		resetRuntimeDeviceState(TRUE);
	}
}

#if defined(ETH) && defined(UDPFS)
static void switchNetworkStack(int target_mode)
{
	if (network_stack_mode == target_mode)
		return;

	if (network_stack_mode != NETWORK_STACK_NONE) {
		DPRINTF("Switching network stack (%d -> %d), resetting IOP\n", network_stack_mode, target_mode);
		resetRuntimeDeviceState(TRUE);
	}
	network_stack_mode = target_mode;
}
#endif

#ifdef DVRP
static void switchPsxHddDriverStack(int use_dvr_stack)
{
	if (!console_is_PSX)
		return;

	if (use_dvr_stack) {
		if (have_DVRP_HDD_modules)
			return;
		DPRINTF("Loading PSX DVR HDD stack without resetting IOP\n");
	} else {
		if (have_HDD_modules)
			return;
		if (!have_DVRP_HDD_modules && !have_dvrdrv && !have_dvrfile)
			return;
		DPRINTF("Loading PSX APA HDD stack without resetting IOP\n");
	}
}
#endif

#ifdef MMCE
int loadMmceModules(void)
{
	int ret, id __attribute__((unused));

	if (!have_mmce || storage_driver_stack_mode != STORAGE_STACK_MMCE)
		showLoadingModulesMsg("mmce");

	ensureCoreIoStackReady();
	switchStorageDriverStack(STORAGE_STACK_MMCE);
	if (!have_mmce) {
		id = SifExecModuleBuffer(mmceman_irx, size_mmceman_irx, 0, NULL, &ret);
		DPRINTF(" [MMCE]: id=%d ret=%d\n", id, ret);
		have_mmce = (id >= 0 && ret >= 0);
	}
	if (have_mmce)
		storage_driver_stack_mode = STORAGE_STACK_MMCE;
	return have_mmce;
}
#endif

#ifdef MX4SIO
int loadMx4sioModules(void)
{
	int ret, id __attribute__((unused));

	if (!have_mx4sio || storage_driver_stack_mode != STORAGE_STACK_MX4SIO)
		showLoadingModulesMsg("mx4sio");

	ensureCoreIoStackReady();
	switchStorageDriverStack(STORAGE_STACK_MX4SIO);
#ifdef EXFAT
	if (!loadBdmCoreModules())
		return 0;
#else
	if (!have_usb_mass)
		loadUsbModules();
#endif
	if (!have_mx4sio) {
		id = SifExecModuleBuffer(mx4sio_bd_irx, size_mx4sio_bd_irx, 0, NULL, &ret);
		DPRINTF(" [MX4SIO_BD]: id=%d ret=%d\n", id, ret);
		/* Newer stacks can return non-negative resident statuses; treat those as success. */
		mx4sio_driver_running = (id >= 0 && ret >= 0);
		have_mx4sio = mx4sio_driver_running;
		if (have_mx4sio)
			resetUsbMassScanState();
	}
	if (have_mx4sio)
		storage_driver_stack_mode = STORAGE_STACK_MX4SIO;

	return have_mx4sio;
}
#endif

#ifdef EXFAT
static int loadBdmCoreModules(void)
{
	int ret, id __attribute__((unused));

	if (!have_bdm) {
		id = SifExecModuleBuffer(bdm_irx, size_bdm_irx, 0, NULL, &ret);
		DPRINTF(" [BDM]: id=%d ret=%d\n", id, ret);
		have_bdm = (id >= 0 && ret >= 0);
	}
	if (have_bdm && !have_bdmfs) {
		id = SifExecModuleBuffer(bdmfs_fatfs_irx, size_bdmfs_fatfs_irx, 0, NULL, &ret);
		DPRINTF(" [BDMFS_FATFS]: id=%d ret=%d\n", id, ret);
		have_bdmfs = (id >= 0 && ret >= 0);
	}

	return (have_bdm && have_bdmfs);
}

static void pauseAfterAtaBlockDriverLoad(void)
{
	/* Keep a brief settle window before loading/using APA stack modules. */
	sleep(1);
}

static int loadAtaBlockDriver(void)
{
	int ret, id __attribute__((unused));

	if (!loadBdmCoreModules()) {
		DPRINTF(" [ATA_BD]: skipping load because BDM core stack failed to initialize\n");
		return 0;
	}
	load_ps2dev9();
	if (!ps2dev9_loaded) {
		DPRINTF(" [ATA_BD]: skipping load because DEV9 failed to initialize\n");
		return 0;
	}
	if (!have_ata_bd) {
		id = SifExecModuleBuffer(ata_bd_irx, size_ata_bd_irx, 0, NULL, &ret);
		DPRINTF(" [ATA_BD]: id=%d ret=%d\n", id, ret);
		have_ata_bd = (id >= 0 && ret >= 0);
		if (have_ata_bd)
			pauseAfterAtaBlockDriverLoad();
	}

	return have_ata_bd;
}

int loadAtaModules(void)
{
	int needs_feedback;

	if ((block_storage_stack_mode & BLOCK_STACK_ATA) &&
	    have_ata_bd && ps2dev9_loaded && have_bdm && have_bdmfs)
		return 1;

	needs_feedback = (!have_ata_bd ||
	                  !ps2dev9_loaded ||
	                  !have_bdm ||
	                  !have_bdmfs ||
	                  !(block_storage_stack_mode & BLOCK_STACK_ATA));
	if (needs_feedback)
		showLoadingModulesMsg("ata");

	switchBlockStorageStack(BLOCK_STACK_ATA);
	ensureCoreIoStackReady();
	setupPowerOff();
	loadAtaBlockDriver();
	if (have_ata_bd) {
		prepareDev9Poweroff();
		block_storage_stack_mode |= BLOCK_STACK_ATA;
	}
	return have_ata_bd;
}
#endif

void loadHdlInfoModule(void)
{
	int ret, ID __attribute__((unused));

	if (!have_hdl_info) {
		drawMsg(LNG(Loading_HDL_Info_Module));
		ID = SifExecModuleBuffer(hdl_info_irx, size_hdl_info_irx, 0, NULL, &ret);
		DPRINTF(" [HDL_INFO]: ID=%d, ret=%d\n", ID, ret);
		ret = Hdl_Info_BindRpc();
		have_hdl_info = 1;
	}
}
//------------------------------
//endfunc loadHdlInfoModule
//---------------------------------------------------------------------------
void closeAllAndPoweroff(void)
{
	if (ps2dev9_loaded) {
		/* Close all files */
		fileXioDevctl("pfs:", PDIOC_CLOSEALL, NULL, 0, NULL, 0);
#ifdef DVRP
		fileXioDevctl("dvr_pfs:", PDIOC_CLOSEALL, NULL, 0, NULL, 0);
#endif
		/*
		 * Do not load modules here. This function is also used by the
		 * poweroff.irx callback; slow DEV9 callbacks are cleared when
		 * HDD/ATA driver stacks load.
		 */
		idleDev9StorageForPoweroff();

		/* Switch off DEV9 */
		while (fileXioDevctl("dev9x:", DDIOC_OFF, NULL, 0, NULL, 0) < 0) {
		};
	}

	// As required by some USB HDDs, issue the SCSI STOP UNIT command to avoid causing an emergency park.
	stopUsbMassForPoweroff();

	/* Power-off the PlayStation 2 console. */
	poweroffShutdown();
}
//------------------------------
//endfunc closeAllAndPoweroff
//---------------------------------------------------------------------------
static void poweroffHandler(int i)
{
	(void)i;

	/*
	 * poweroff.irx runs this from its callback thread. Do not draw, set up
	 * modules, or otherwise touch UI state here; MISC/PS2PowerOff does that
	 * before entering this same shutdown routine.
	 */
	closeAllAndPoweroff();
}
//------------------------------
//endfunc poweroffHandler
//---------------------------------------------------------------------------
void setupPowerOff(void)
{
	int ret, ID __attribute__((unused));

	if (!done_setupPowerOff) {
		if (!have_poweroff) {
			ID = SifExecModuleBuffer(poweroff_irx, size_poweroff_irx, 0, NULL, &ret);
			DPRINTF(" [POWEROFF]: ID=%d, ret=%d\n", ID, ret);
			have_poweroff = 1;
		}
		poweroffInit();
		poweroffSetCallback((void *)poweroffHandler, NULL);
		load_ps2dev9();
		done_setupPowerOff = 1;
	}
}
//------------------------------
//endfunc setupPowerOff
//---------------------------------------------------------------------------
int loadHddModules(void)
{
#ifdef DVRP
	switchPsxHddDriverStack(0);
#endif
	if ((block_storage_stack_mode & BLOCK_STACK_HDD) && have_HDD_modules)
		return 1;

	switchBlockStorageStack(BLOCK_STACK_HDD);
	ensureCoreIoStackReady();
	if (!have_HDD_modules) {
		showLoadingModulesMsg("hdd");
		setupPowerOff();
		load_ps2hdd_stack(1);  //also loads ps2hdd & ps2fs
		have_HDD_modules = (have_ps2hdd && have_ps2fs);
		if (!have_HDD_modules) {
			DPRINTF(" [HDD]: stack incomplete (HDD=%d FS=%d)\n",
			        have_ps2hdd, have_ps2fs);
		}
	}
	if (have_HDD_modules)
		block_storage_stack_mode |= BLOCK_STACK_HDD;
	return have_HDD_modules;
}
//------------------------------
//endfunc loadHddModules
//---------------------------------------------------------------------------
#ifdef XFROM
int loadFlashModules(void)
{
	if (!console_is_PSX)
		return 0;

	ensureCoreIoStackReady();
	if (!have_Flash_modules) {
		showLoadingModulesMsg("flash");
		load_ps2dev9();
		setupPowerOff();
		load_pflash();
		have_Flash_modules = TRUE;
	}
	return xfromserv_loaded;
}
//------------------------------
//endfunc loadFlashModules
//---------------------------------------------------------------------------
#endif
#ifdef DVRP
int loadDVRPHddModules(void)
{
	if (!console_is_PSX)
		return 0;

	switchPsxHddDriverStack(1);
	ensureCoreIoStackReady();
	if (!have_DVRP_HDD_modules) {
		showLoadingModulesMsg("dvr_hdd");
		setupPowerOff();
		load_ps2dvr();
		have_DVRP_HDD_modules = (have_dvrdrv && have_dvrfile);
		if (have_DVRP_HDD_modules) {
			sceCdNoticeGameStart(0, NULL);
		} else {
			DPRINTF(" [DVR_HDD]: stack incomplete (HDD=%d FS=%d DVRDRV=%d DVRFILE=%d)\n",
			        have_ps2hdd, have_ps2fs, have_dvrdrv, have_dvrfile);
		}
	}
	return have_DVRP_HDD_modules;
}
//------------------------------
//endfunc loadDVRPHddModules
//---------------------------------------------------------------------------
#endif


// Load Network modules by EP (modified by RA)
//------------------------------
#ifdef ETH
void loadNetModules(void)
{
	ensureCoreIoStackReady();
	if (!have_NetModules) {
		showLoadingModulesMsg("ps2net");

		getIpConfig();  //RA NB: I always get that info, early in init
		//             //But sometimes it is useful to do it again (HDD)
		// Also, my module checking makes some other tests redundant
		load_ps2netfs();  // loads ps2netfs from internal buffer
		load_ps2ftpd();   // loads ps2dftpd from internal buffer
		have_NetModules = (have_ps2netfs && have_ps2ftpd);
	}
}
#endif
//------------------------------
//endfunc loadNetModules
//---------------------------------------------------------------------------
void startKbd(void)
{
	int kbd_fd;
	void *mapBase;
	int mapSize;

	DPRINTF("Entering startKbd()\r\n");
	if (!setting->usbkbd_used || ps2kbd_opened)
		return;

	loadKbdModules();
	if (!have_ps2kbd)
		return;

	PS2KbdInit();
	ps2kbd_opened = 1;
	if (setting->kbdmap_file[0]) {
		if (pathUsesUsbMass(setting->kbdmap_file))
			loadUsbModules();
		if ((kbd_fd = fileXioOpen(PS2KBD_DEVFILE, FIO_O_RDONLY, 0)) >= 0) {
			DPRINTF("kbd_fd=%d; Loading Kbd map file \"%s\"\r\n", kbd_fd, setting->kbdmap_file);
			if (loadExternalFile(setting->kbdmap_file, &mapBase, &mapSize)) {
				if (mapSize == 0x600) {
					fileXioIoctl(kbd_fd, PS2KBD_IOCTL_SETKEYMAP, mapBase);
					fileXioIoctl(kbd_fd, PS2KBD_IOCTL_SETSPECIALMAP, mapBase + 0x300);
					fileXioIoctl(kbd_fd, PS2KBD_IOCTL_SETCTRLMAP, mapBase + 0x400);
					fileXioIoctl(kbd_fd, PS2KBD_IOCTL_SETALTMAP, mapBase + 0x500);
				}
				DPRINTF("Freeing buffer after setting Kbd maps\r\n");
				free(mapBase);
			}
			fileXioClose(kbd_fd);
		}
	}
}
//------------------------------
//endfunc startKbd
//---------------------------------------------------------------------------
int ensureUsbKeyboardReady(void)
{
	startKbd();
	return ps2kbd_opened;
}
//------------------------------
//endfunc ensureUsbKeyboardReady
//---------------------------------------------------------------------------
static void clearIopModuleState(void)
{
	have_basic_modules = 0;
	have_filexio_ready = 0;
	have_filexio_rwbuf_tuned = 0;
	have_mc_rpc_ready = 0;
	have_cdvd = 0;
	have_usbd = 0;
	have_usb_mass = 0;
#ifdef DS34
	have_ds34 = 0;
#endif
	have_ps2smap = 0;
	have_ps2host = 0;
	have_ps2ip = 0;
	have_ps2netfs = 0;
#ifdef UDPFS
	have_udpfs_smap = 0;
	have_udpfs_ministack = 0;
	have_udpfs_ioman = 0;
#endif
	have_vmcman = 0;
	have_ps2ftpd = 0;
	have_ps2kbd = 0;
	have_hdl_info = 0;
	have_NetModules = 0;
	have_HDD_modules = 0;
	have_poweroff = 0;
	have_ps2dev9 = 0;
	have_ps2hdd = 0;
	have_ps2fs = 0;
#ifdef EXFAT
	have_bdm = 0;
	have_bdmfs = 0;
	have_ata_bd = 0;
#endif
#ifdef MMCE
	have_mmce = 0;
#endif
#ifdef MX4SIO
	have_mx4sio = 0;
	mx4sio_driver_running = 0;
#endif
	resetDriverStackLoadTracking();
	ps2dev9_loaded = 0;
	done_setupPowerOff = 0;
	ps2kbd_opened = 0;
#ifdef XFROM
	have_Flash_modules = 0;
	xfromserv_loaded = 0;
#endif
	have_secrsif = 0;
	have_exploit_signer_iop = 0;
#ifdef DVRP
	have_DVRP_HDD_modules = 0;
	have_ps2atad = 0;
	have_dvrdrv = 0;
	have_dvrfile = 0;
#endif
	resetUsbMassRuntimeState();
	invalidatePartitionCaches();
}
//------------------------------
//endfunc clearIopModuleState
//---------------------------------------------------------------------------
// reboot IOP (original source by Hermes in BOOT.c - cogswaploader)
// dlanor: but changed now, as the original was badly bugged
void Reset()
{
#ifndef NO_IOP_RESET
	SifInitRpc(0);
	while (!SifIopReset("", 0)) {
	};
	while (!SifIopSync()) {
	};
	SifInitRpc(0);
#endif
	SifLoadFileInit();
	initsbv_patches();

	clearIopModuleState();

#ifdef POWERPC_UART
int i, d;
    i = SifExecModuleBuffer(&ppctty_irx, size_ppctty_irx, 0, NULL, &d);
    DPRINTF(" [PPCTTY]: id=%d, ret=%d\n", i, d);
#endif

#ifdef UDPTTY
int i, d;
	load_ps2ip();
	i = SifExecModuleBuffer(&udptty_irx, size_udptty_irx, 0, NULL, &d);
	DPRINTF(" [UDPTTY]: id=%d, ret=%d\n", i, d);
#endif
	ensureCoreIoStackReady();
	iop_reset_generation++;
	DPRINTF("RESET FINISHED\n");
	//	setupPad();
}
//------------------------------
//endfunc Reset
