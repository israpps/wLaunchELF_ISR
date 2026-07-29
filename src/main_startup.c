//--------------------------------------------------------------
//File name:   main_startup.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "init.h"
#include "main_gameid.h"
#include "main_startup.h"
#include <stdlib.h>

extern char default_OSDSYS_path[30];
extern char default_OSDSYS_path2[30];
extern char rough_region;

static unsigned short ROMVersion;
static int hide_hdd_ata_devices = -1;

static void normalizeBootPath(char *path);
static int isSlashRootedBootPathDevice(const char *path);
static int isDevicePathTerminator(char c);
static int isBootUsbMassPath(const char *path);
static int isBootAtaPath(const char *path);
static int isBootHddPath(const char *path);
static int isBootXfromPath(const char *path);
static void initializeBootExecPath(void);
static void buildBootBlockPath(char *dst, size_t dst_size, const char *prefix, const char *partition, const char *path_part);
static int getRomverPrefixNumber(void);

// Parse network configuration from IPCONFIG.DAT
// Now completely rewritten to fix some problems
void getIpConfig(void)
{
	int fd;
	int i;
	int len;
	int preferred_port;
	int ports_to_try[2];
	int port_ix;
	char c;
	char buf[IPCONF_MAX_LEN];
	char path[MAX_PATH];
	char candidate[MAX_PATH];
	char loaded_path[MAX_PATH];
	size_t dir_len;
	static const char ipconfig_name[] = "IPCONFIG.DAT";

	fd = -1;
	len = 0;
	LoadedIPConfigPath[0] = '\0';
	loaded_path[0] = '\0';

	// Prefer IPCONFIG in the same directory as the launched ELF.
	dir_len = strnlen(LaunchElfDir, sizeof(candidate));
	if (dir_len < sizeof(candidate) && (dir_len + sizeof(ipconfig_name)) <= sizeof(candidate)) {
		memcpy(candidate, LaunchElfDir, dir_len);
		memcpy(candidate + dir_len, ipconfig_name, sizeof(ipconfig_name));
		if (genFixPath(candidate, path) >= 0) {
			fd = genOpen(path, FIO_O_RDONLY);
			if (fd >= 0)
				snprintf(loaded_path, sizeof(loaded_path), "%s", candidate);
		}
	}

	// Fallback to SYS-CONF on memory cards, preferring current MC slot.
	if (fd < 0) {
		preferred_port = 0;
		if (!strncmp(LaunchElfDir, "mc1", 3))
			preferred_port = 1;
		ports_to_try[0] = preferred_port;
		ports_to_try[1] = preferred_port ^ 1;

		for (port_ix = 0; port_ix < 2 && fd < 0; port_ix++) {
			snprintf(candidate, sizeof(candidate), "mc%d:/SYS-CONF/IPCONFIG.DAT", ports_to_try[port_ix]);
			if (genFixPath(candidate, path) >= 0) {
				fd = genOpen(path, FIO_O_RDONLY);
				if (fd >= 0)
					snprintf(loaded_path, sizeof(loaded_path), "%s", candidate);
			}
		}
	}

	if (fd >= 0) {
		memset(buf, 0, IPCONF_MAX_LEN);
		len = genRead(fd, buf, IPCONF_MAX_LEN - 1);  //Save a byte for termination
		genClose(fd);
	}

	if ((fd >= 0) && (len > 0)) {
		snprintf(LoadedIPConfigPath, sizeof(LoadedIPConfigPath), "%s", loaded_path);
		buf[len] = '\0';                          //Ensure string termination, regardless of file content
		for (i = 0; ((c = buf[i]) != '\0'); i++)  //Clear out spaces and any CR/LF
			if ((c == ' ') || (c == '\r') || (c == '\n'))
				buf[i] = '\0';
		snprintf(ip, sizeof(ip), "%.15s", buf);
		i = strlen(ip) + 1;
		snprintf(netmask, sizeof(netmask), "%.15s", buf + i);
		i += strlen(netmask) + 1;
		snprintf(gw, sizeof(gw), "%.15s", buf + i);
	}

	memset(if_conf, 0, IPCONF_MAX_LEN);
	i = 0;
	memcpy(if_conf + i, ip, strlen(ip) + 1);
	i += strlen(ip) + 1;
	memcpy(if_conf + i, netmask, strlen(netmask) + 1);
	i += strlen(netmask) + 1;
	memcpy(if_conf + i, gw, strlen(gw) + 1);
	i += strlen(gw) + 1;
	if_conf_len = i;
	snprintf(netConfig, sizeof(netConfig), "%s:  %-15s %-15s %-15s", LNG(Net_Config), ip, netmask, gw);
}

//scanSystemCnf will check for a standard variable of a SYSTEM.CNF file
static int scanSystemCnf(char *name, char *value)
{
	if (!strcmp(name, "BOOT"))
		strncat(SystemCnf_BOOT, value, MAX_PATH - 1);
	else if (!strcmp(name, "BOOT2"))
		strncat(SystemCnf_BOOT2, value, MAX_PATH - 1);
	else if (!strcmp(name, "VER"))
		strncat(SystemCnf_VER, value, 9);
	else if (!strcmp(name, "VMODE"))
		strncat(SystemCnf_VMODE, value, 9);
	else
		return 0;  //when no matching variable
	return 1;      //when matching variable found
}

//readSystemCnf will read standard settings from a SYSTEM.CNF file
int readSystemCnf(void)
{
	int var_cnt;
	char *RAM_p, *CNF_p, *name, *value;
	char generic_gameid[12];

	BootDiscType = 0;
	SystemCnf_BOOT[0] = '\0';
	SystemCnf_BOOT2[0] = '\0';
	SystemCnf_VER[0] = '\0';
	SystemCnf_VMODE[0] = '\0';

	if ((RAM_p = preloadCNF("cdrom0:\\SYSTEM.CNF;1")) != NULL) {
		CNF_p = RAM_p;
		for (var_cnt = 0; get_CNF_string(&CNF_p, &name, &value); var_cnt++)
			scanSystemCnf(name, value);
		free(RAM_p);
	}

	if (SystemCnf_BOOT2[0])
		BootDiscType = 2;
	else if (SystemCnf_BOOT[0])
		BootDiscType = 1;

	if (!SystemCnf_BOOT[0])
		strcpy(SystemCnf_BOOT, "???");
	if (!SystemCnf_VER[0])
		strcpy(SystemCnf_VER, "???");

	if (RAM_p == NULL) {  //if SYSTEM.CNF was not found test for PS1 special cases
		if (buildPS1GenericDiscGameID(generic_gameid, sizeof(generic_gameid))) {
			strcpy(SystemCnf_BOOT, generic_gameid);
			BootDiscType = 1;
		} else if (wleExists("cdrom0:\\PSX.EXE;1")) {
			BootDiscType = 1;
		}
	}

	return BootDiscType;  //0==none, 1==PS1, 2==PS2
}

//------------------------------
//Function to infer region from ROMVER and select default TV mode
//------------------------------
int uLE_InitializeRegion(void)
{
	int read_len;
	static int TVMode = -1;
	int i;
	int ROMVER_fd;
	char path_buf[32];
	static const char *romver_paths[] = {
	    "rom0:ROMVER",
	};

	// Keep default mode available even before ROMVER can be read.
	if (TVMode < 0)
		TVMode = TV_mode_NTSC;

	// If we already have ROMVER, keep the cached region/mode.
	if (ROMVER_data[0] != '\0')
		return TVMode;

	ensureCoreIoStackReady();

	memset(ROMVER_data, 0, sizeof(ROMVER_data));

	read_len = -1;
	for (i = 0; i < (int)(sizeof(romver_paths) / sizeof(romver_paths[0])); i++) {
		strncpy(path_buf, romver_paths[i], sizeof(path_buf) - 1);
		path_buf[sizeof(path_buf) - 1] = '\0';
		ROMVER_fd = genOpen(path_buf, FIO_O_RDONLY);
		if (ROMVER_fd < 0)
			continue;
		read_len = genRead(ROMVER_fd, ROMVER_data, sizeof(ROMVER_data) - 1);
		genClose(ROMVER_fd);
		if (read_len > 0)
			break;
		memset(ROMVER_data, 0, sizeof(ROMVER_data));
	}

	if (read_len <= 0 || ROMVER_data[0] == '\0') {
		memset(ROMVER_data, 0, sizeof(ROMVER_data));
		rough_region = 'X';
		return TVMode;
	}

	ROMVER_data[sizeof(ROMVER_data) - 1] = '\0';

	switch (ROMVER_data[4]) {
		case 'J':
			rough_region = 'I';
			break;
		case 'E':
			rough_region = 'E';
			break;
		case 'A':
		case 'H':  //Asia shares the same letter as USA.
			rough_region = 'A';
			break;
		case 'C':
			rough_region = 'C';
			break;
		default:
			rough_region = 'X';
	}

	if (ROMVER_data[4] == 'E')
		TVMode = TV_mode_PAL;  //PAL mode is identified by 'E' for Europe
	else
		TVMode = TV_mode_NTSC;  //All other cases need NTSC

	return TVMode;
}
//------------------------------
//endfunc uLE_InitializeRegion
//---------------------------------------------------------------------------
static void initializeBootExecPath(void)
{
	char file[12];
	char romver_prefix[5];

	uLE_InitializeRegion();
	strncpy(romver_prefix, ROMVER_data, 4);
	romver_prefix[4] = '\0';
	ROMVersion = strtoul(romver_prefix, NULL, 16);
	hide_hdd_ata_devices = (getRomverPrefixNumber() >= 220);

	//Handle special cases, before osdmain.elf was supported.
	switch (ROMVER_data[4]) {
		case 'E':
			if (!strncmp(ROMVER_data, "0120", 4))
				strcpy(file, "osd130.elf");
			else
				strcpy(file, "osdmain.elf");
			break;
		case 'A':
			if (!strncmp(ROMVER_data, "0110", 4))
				strcpy(file, "osd120.elf");
			else
				strcpy(file, "osdmain.elf");
			break;
		case 'J':
			if (!strncmp(ROMVER_data, "0100", 4))
				strcpy(file, "osdsys.elf");
			else if (!strncmp(ROMVER_data, "0101", 4))
				strcpy(file, "osd110.elf");
			else
				strcpy(file, "osdmain.elf");
			break;
		default:  //Asia and China
			strcpy(file, "osdmain.elf");
	}

	sprintf(default_OSDSYS_path, "mc:/B%cEXEC-SYSTEM/%s", rough_region, file);
	if (ROMVersion >= 0x230)
		sprintf(default_OSDSYS_path2, "/Incompatible Unit (0x%03x)", (ROMVersion) & ~0x0F);
	else
		sprintf(default_OSDSYS_path2, "mc:/B%cEXEC-SYSTEM/%s", rough_region, file);
}
//------------------------------
//endfunc initializeBootExecPath
//---------------------------------------------------------------------------
static void normalizeBootPath(char *path)
{
	char *colon;
	int i;

	if (path == NULL || path[0] == '\0')
		return;

	for (i = 0; path[i] != '\0'; i++) {
		if (path[i] == '\\')
			path[i] = '/';
	}

	// Legacy SwapMagic path normalization: mass0:/path -> mass:/path
	if (!strncmp(path, "mass0:", 6))
		memmove(path + 4, path + 5, strlen(path + 5) + 1);

	// Legacy ATA alias normalization: ata:/path -> ata0:/path
	if (!strncmp(path, "ata:", 4) && strlen(path) + 1 < MAX_PATH) {
		memmove(path + 4, path + 3, strlen(path + 3) + 1);
		path[3] = '0';
	}

	colon = strchr(path, ':');
	if (colon != NULL && colon[1] != '\0' && colon[1] != '/' &&
	    isSlashRootedBootPathDevice(path) && strlen(path) + 1 < MAX_PATH) {
		memmove(colon + 2, colon + 1, strlen(colon + 1) + 1);
		colon[1] = '/';
	}
}
//------------------------------
//endfunc normalizeBootPath
//---------------------------------------------------------------------------
static int isSlashRootedBootPathDevice(const char *path)
{
	if (isBootHddPath(path))
		return 0;

	if (isBootUsbMassPath(path) || isBootAtaPath(path) || isBootXfromPath(path))
		return 1;
	if (!strncmp(path, "mc", 2) && path[2] >= '0' && path[2] <= '9' && path[3] == ':')
		return 1;
	if (!strncmp(path, "mmce", 4) && path[4] >= '0' && path[4] <= '9' && path[5] == ':')
		return 1;
	if (!strncmp(path, "mx4sio", 6) && (path[6] == ':' || (path[6] >= '0' && path[6] <= '9' && path[7] == ':')))
		return 1;
	if (!strncmp(path, "host:", 5) || !strncmp(path, "udpfs:", 6))
		return 1;

	return 0;
}
//------------------------------
//endfunc isSlashRootedBootPathDevice
//---------------------------------------------------------------------------
static int isDevicePathTerminator(char c)
{
	return (c == '\0' || c == ':' || c == '/' || c == '\\');
}
//------------------------------
//endfunc isDevicePathTerminator
//---------------------------------------------------------------------------
static int isBootUsbMassPath(const char *path)
{
	if (path == NULL)
		return 0;

	if (!strncmp(path, "mass", 4))
		return (isDevicePathTerminator(path[4]) ||
		        (path[4] >= '0' && path[4] <= '9' && isDevicePathTerminator(path[5])));

	if (!strncmp(path, "usb", 3))
		return (isDevicePathTerminator(path[3]) ||
		        (path[3] >= '0' && path[3] <= '9' && isDevicePathTerminator(path[4])));

	return 0;
}
//------------------------------
//endfunc isBootUsbMassPath
//---------------------------------------------------------------------------
static int isBootAtaPath(const char *path)
{
	if (path == NULL || strncmp(path, "ata", 3))
		return 0;

	return ((path[3] == '0' || path[3] == '1') && isDevicePathTerminator(path[4]));
}
//------------------------------
//endfunc isBootAtaPath
//---------------------------------------------------------------------------
static int isBootHddPath(const char *path)
{
	if (path == NULL || strncmp(path, "hdd", 3))
		return 0;

	return ((path[3] == '0' || path[3] == '1') && path[4] == ':');
}
//------------------------------
//endfunc isBootHddPath
//---------------------------------------------------------------------------
static int isBootXfromPath(const char *path)
{
	if (path == NULL || strncmp(path, "xfrom", 5))
		return 0;

	return (isDevicePathTerminator(path[5]) ||
	        (path[5] >= '0' && path[5] <= '9' && isDevicePathTerminator(path[6])));
}
//------------------------------
//endfunc isBootXfromPath
//---------------------------------------------------------------------------
static void buildBootBlockPath(char *dst, size_t dst_size, const char *prefix, const char *partition, const char *path_part)
{
	size_t len;
	size_t chunk;
	size_t room;

	if (dst == NULL || dst_size == 0)
		return;

	if (prefix == NULL)
		prefix = "";
	if (partition == NULL)
		partition = "";
	if (path_part == NULL)
		path_part = "";

	dst[0] = '\0';
	len = 0;

	room = dst_size - 1 - len;
	chunk = strlen(prefix);
	if (chunk > room)
		chunk = room;
	memcpy(dst + len, prefix, chunk);
	len += chunk;
	dst[len] = '\0';

	room = dst_size - 1 - len;
	chunk = strlen(partition);
	if (chunk > room)
		chunk = room;
	memcpy(dst + len, partition, chunk);
	len += chunk;
	dst[len] = '\0';

	if (path_part[0] != '/') {
		room = dst_size - 1 - len;
		chunk = strlen("/");
		if (chunk > room)
			chunk = room;
		memcpy(dst + len, "/", chunk);
		len += chunk;
		dst[len] = '\0';
	}

	room = dst_size - 1 - len;
	chunk = strlen(path_part);
	if (chunk > room)
		chunk = room;
	memcpy(dst + len, path_part, chunk);
	len += chunk;
	dst[len] = '\0';
}
//------------------------------
//endfunc buildBootBlockPath
//---------------------------------------------------------------------------
enum BOOT_DEVICE prepareBootDeviceAndPath(const char *arg0, char *boot_path, size_t boot_path_len)
{
	enum BOOT_DEVICE boot = BOOT_DEV_UNKNOWN;
	char *p;

	LaunchElfDir[0] = '\0';
	LaunchElfBootDir[0] = '\0';
	if (boot_path != NULL && boot_path_len > 0)
		boot_path[0] = '\0';

	if (arg0 != NULL && arg0[0] != '\0') {
		snprintf(LaunchElfDir, sizeof(LaunchElfDir), "%s", arg0);
		if (boot_path != NULL && boot_path_len > 0)
			snprintf(boot_path, boot_path_len, "%s", arg0);
		normalizeBootPath(LaunchElfDir);
		if (boot_path != NULL && boot_path_len > 0)
			normalizeBootPath(boot_path);

		if (isBootUsbMassPath(LaunchElfDir)) {
			boot = BOOT_DEVICE_MASS;
		} else if (!strncmp(LaunchElfDir, "mc", 2)) {
			boot = BOOT_DEVICE_MC;
#ifdef XFROM
		} else if (console_is_PSX && isBootXfromPath(LaunchElfDir)) {
			boot = BOOT_DEVICE_XFROM;
#endif
#ifdef MMCE
		} else if (!strncmp(LaunchElfDir, "mmce", 4)) {
			boot = BOOT_DEVICE_MMCE;
#endif
#ifdef MX4SIO
		} else if (!strncmp(LaunchElfDir, "mx4sio", 6)) {
			boot = BOOT_DEVICE_MX4SIO;
#endif
		} else if (isBootAtaPath(LaunchElfDir)) {
			boot = BOOT_DEVICE_ATA;
		} else if (!strncmp(LaunchElfDir, "cd", 2)) {
			boot = BOOT_DEVICE_CDVD;
			snprintf(LaunchElfDir, sizeof(LaunchElfDir), "%s", "mc0:/SYS-CONF/");  //Default to mc0 as a writable location.
		} else if (isBootHddPath(LaunchElfDir) && boot_path != NULL && boot_path_len > 0) {
			//Booting from the HDD requires special handling for HDD-based paths.
			char hdd_prefix[7];
			char temp[MAX_PATH];
			char *t;
			/* Change boot_path to contain a path to the block device.
				Standard HDD path format: hddN:partition:pfs:path/to/file
				However, (older) homebrew may not use this format. */
			snprintf(hdd_prefix, sizeof(hdd_prefix), "hdd%c:/", LaunchElfDir[3]);
			snprintf(temp, sizeof(temp), "%s", boot_path + 5);  //Skip "hddN:" when copying.
			t = strchr(temp, ':');                              //Check if the separator between the block device & the path exists.
				if (t != NULL) {
					char *path_part;
					*(t) = 0;                      //If it does, get the block device name.
					path_part = strchr(t + 1, ':');  //Get the path to the file
					if (path_part != NULL) {
						buildBootBlockPath(LaunchElfDir, sizeof(LaunchElfDir), hdd_prefix, temp, path_part + 1);
					}
				}

			boot = BOOT_DEVICE_HDD;
		}
	}

#if defined(ETH) || defined(UDPFS)
#ifdef ETH
	if (!strncmp(LaunchElfDir, "host", 4))
		boot = BOOT_DEVICE_HOST;
#endif
#ifdef UDPFS
	if (!strncmp(LaunchElfDir, "udpfs", 5))
		boot = BOOT_DEVICE_UDPFS;
#endif
#endif
	DPRINTF("Boot device is %d\n", boot);

	if (((p = strrchr(LaunchElfDir, '/')) == NULL) && ((p = strrchr(LaunchElfDir, '\\')) == NULL))
		p = strrchr(LaunchElfDir, ':');
	if (p != NULL)
		*(p + 1) = 0;
	//The above cuts away the ELF filename from LaunchElfDir, leaving a pure path
	snprintf(LaunchElfBootDir, sizeof(LaunchElfBootDir), "%s", LaunchElfDir);

	LastDir[0] = 0;

	return boot;
}
//------------------------------
//endfunc prepareBootDeviceAndPath
//---------------------------------------------------------------------------
void bringUpBootDeviceStack(enum BOOT_DEVICE boot_device)
{
	switch (boot_device) {
		case BOOT_DEVICE_MASS:
			if (isBootUsbMassPath(LaunchElfDir)) {
				DPRINTF("Loading USB modules for boot\n");
				loadUsbModules();
			}
			break;
#ifdef MMCE
		case BOOT_DEVICE_MMCE:
			loadMmceModules();
			break;
#endif
#ifdef MX4SIO
		case BOOT_DEVICE_MX4SIO:
			loadMx4sioModules();
			break;
#endif
		case BOOT_DEVICE_ATA:
#ifdef EXFAT
			loadAtaModules();
#endif
			break;
		case BOOT_DEVICE_HDD:
			loadHddModules();
			break;
		case BOOT_DEVICE_XFROM:
#ifdef XFROM
			loadFlashModules();
#endif
			break;
		case BOOT_DEVICE_UDPFS:
#ifdef UDPFS
			load_udpfs();
#endif
			break;
		default:
			break;
	}
}
//------------------------------
//endfunc bringUpBootDeviceStack
//---------------------------------------------------------------------------
void initializeBootDisplayDefaults(void)
{
	TV_mode = uLE_InitializeRegion();  //Let console region decide default TV_mode
	Frame_end_y = Menu_end_y + 4;
	Menu_tooltip_y = Frame_end_y + LINE_THICKNESS + 2;
	initializeBootExecPath();
}
//------------------------------
//endfunc initializeBootDisplayDefaults
//---------------------------------------------------------------------------
void bringUpBootNetworkStack(enum BOOT_DEVICE boot_device)
{
#if defined(ETH) || defined(UDPFS)
	if (boot_device == BOOT_DEVICE_HOST) {
		//If booted from a network filesystem device, bring that stack up now.
		getIpConfig();
#ifdef ETH
		initHOST();
#endif
	}
#else
	(void)boot_device;
#endif
}
//------------------------------
//endfunc bringUpBootNetworkStack
//---------------------------------------------------------------------------
void initializeBootGraphics(void)
{
	DPRINTF("setupGS()\n");
	setupGS();
	gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x00, 0x00));
	loadFont("");
	setInitPhaseComplete();
}
//------------------------------
//endfunc initializeBootGraphics
//---------------------------------------------------------------------------
void loadConfiguredBootFont(void)
{
	loadFont(setting->font_file);
}
//------------------------------
//endfunc loadConfiguredBootFont
//---------------------------------------------------------------------------
unsigned short getROMVersion(void)
{
	return ROMVersion;
}

static int getRomverPrefixNumber(void)
{
	char romver_prefix[5];
	char *end;
	unsigned long value;

	if (ROMVER_data[0] == '\0')
		return -1;

	strncpy(romver_prefix, ROMVER_data, 4);
	romver_prefix[4] = '\0';
	value = strtoul(romver_prefix, &end, 10);
	if (end == romver_prefix)
		return -1;
	if (value > 9999UL)
		value = 9999UL;

	return (int)value;
}

int shouldHideHddAtaDevices(void)
{
	int romver_prefix;

	if (hide_hdd_ata_devices >= 0)
		return hide_hdd_ata_devices;

	uLE_InitializeRegion();
	romver_prefix = getRomverPrefixNumber();
	hide_hdd_ata_devices = (romver_prefix >= 220);
	return hide_hdd_ata_devices;
}
