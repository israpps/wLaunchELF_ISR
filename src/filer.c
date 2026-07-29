//--------------------------------------------------------------
//File name:   filer.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "filer_copy.h"
#include "gui_hdd0_format.h"
#include "gui_sort.h"
#include "filer_shared.h"
#include "filer_internal.h"
#include "init.h"
#include "main_startup.h"

#define MC_ATTR_norm_folder 0x8427  //Normal folder on PS2 MC
#define MC_ATTR_norm_file 0x8497    //file (PS2/PS1) on PS2 MC

static int getHideHddMode(void)
{
	int mode = (setting != NULL) ? setting->Hide_Hdd : HIDE_HDD_HDD1_ATA1;

	if (boot_show_all_devices)
		return HIDE_HDD_SHOW_ALL;

	if (shouldHideHddAtaDevices())
		return HIDE_HDD_HDD01_ATA01;

	return (mode >= 0 && mode < HIDE_HDD_COUNT) ? mode : HIDE_HDD_HDD1_ATA1;
}

static const unsigned char hide_hdd_unit_masks[HIDE_HDD_COUNT] = {0, 2, 3, 0, 0, 2, 3};
static const unsigned char hide_ata_unit_masks[HIDE_HDD_COUNT] = {0, 0, 0, 2, 3, 2, 3};

static int shouldShowHddDevice(int unit)
{
	return (unit >= 0 && unit < 2 && !(hide_hdd_unit_masks[getHideHddMode()] & (1 << unit)));
}

static int shouldShowAtaDevice(int unit)
{
	return (unit >= 0 && unit < 2 && !(hide_ata_unit_masks[getHideHddMode()] & (1 << unit)));
}

static void clearMcTable(sceMcTblGetDir *mcT)
{
	memset((void *)mcT, 0, sizeof(sceMcTblGetDir));
}






int PasteProgress_f = 0;   //Flags progress report having been made in Pasting
int PasteMode;             //Top-level PasteMode flag
int PM_flag[MAX_RECURSE];  //PasteMode flag for each 'copy' recursion level
int PM_file[MAX_RECURSE];  //PasteMode attribute file descriptors

char mountedParty[MOUNT_LIMIT][MAX_NAME];
int latestMount = -1;
int vmcMounted[2] = {0, 0};                          //flags true for mounted VMC false for unmounted
int vmc_PartyIndex[2] = {-1, -1};                    //PFS index for each VMC, unless -1
int Party_vmcIndex[MOUNT_LIMIT] = {-1, -1, -1, -1};  //VMC for each PFS, unless -1
unsigned char *elisaFnt = NULL;
int elisa_failed = FALSE;  //Set at failure to load font, cleared at browser entry
u64 freeSpace;
int mcfreeSpace;
int mctype_PSx;  //dlanor: Needed for proper scaling of mcfreespace
int vfreeSpace;  //flags validity of freespace value
int browser_cut;
int nclipFiles, nmarks;
unsigned int clipIopResetGeneration;
#ifdef DVRP
static int ndvrpparties;
char mountedDVRPParty[MOUNT_LIMIT][MAX_NAME];
int latestDVRPMount = -1;
#endif

int file_show = 1;  //dlanor: 0==name_only, 1==name+size+time, 2==title+size+time
int file_sort = 1;  //dlanor: 0==none, 1==name, 2==title, 3==mtime
int size_valid = 0;
int time_valid = 0;
static char hddParties[2][MAX_PARTITIONS][MAX_PART_NAME + 1];
static int hddNparties[2] = {0, 0};
#ifdef DVRP
static char dvrParties[MAX_PARTITIONS][MAX_PART_NAME + 1];
#endif
char clipPath[MAX_PATH], LastDir[MAX_NAME], marks[MAX_ENTRY];
FILEINFO clipFiles[MAX_ENTRY];
int fileMode = FIO_S_IRUSR | FIO_S_IWUSR | FIO_S_IXUSR | FIO_S_IRGRP | FIO_S_IWGRP | FIO_S_IXGRP | FIO_S_IROTH | FIO_S_IWOTH | FIO_S_IXOTH;

#ifdef ULE_DEBUG_BUILD
#ifndef FILEOP_TRACE
#define FILEOP_TRACE 1
#endif
#else
#undef FILEOP_TRACE
#define FILEOP_TRACE 0
#endif

char cnfmode_extU[CNFMODE_CNT][4] = {
    "*",    // cnfmode FALSE
    "ELF",  // cnfmode TRUE
    "IRX",  // cnfmode USBD_IRX_CNF
    "IRX",  // cnfmode USBKBD_IRX_CNF
    "KBD",  // cnfmode KBDMAP_FILE_CNF
    "CNF",  // cnfmode CNF_PATH_CNF
    "*",    // cnfmode TEXT_CNF
    "",     // cnfmode DIR_CNF
    "IRX",  // cnfmode USBMASS_IRX_CNF
    "LNG",  // cnfmode LANG_CNF
    "FNT",  // cnfmode FONT_CNF
    "*",    // cnfmode SAVE_CNF
    "ELF"   // cnfmode ELF_FILE_CNF
};

char cnfmode_extL[CNFMODE_CNT][4] = {
    "*",    // cnfmode FALSE
    "elf",  // cnfmode TRUE
    "irx",  // cnfmode USBD_IRX_CNF
    "irx",  // cnfmode USBKBD_IRX_CNF
    "kbd",  // cnfmode KBDMAP_FILE_CNF
    "cnf",  // cnfmode CNF_PATH_CNF
    "*",    // cnfmode TEXT_CNF
    "",     // cnfmode DIR_CNF
    "irx",  // cnfmode USBMASS_IRX_CNF
    "lng",  // cnfmode LANG_CNF
    "fnt",  // cnfmode FONT_CNF
    "*",    // cnfmode SAVE_CNF
    "elf"   // cnfmode ELF_FILE_CNF
};
#if defined(ETH) || defined(UDPFS)
int host_ready = 0;
int host_error = 0;
int host_elflist = 0;
int host_use_Bsl = 1;  //By default assume that host paths use backslash
#endif
#ifdef UDPFS
int udpfs_dir_open_failed = 0;
#endif
u64 written_size;            //Used for pasting progress report
u64 PasteTime;               //Used for pasting progress report

int PSU_content;  //Used to count PSU content headers for the main header

//USB_mass definitions for multiple drive usage
char USB_mass_ix[10] = {'0', 0, 0, 0, 0, 0, 0, 0, 0, 0};
int USB_mass_max_drives = USB_MASS_MAX_DRIVES;
u64 USB_mass_scan_time = 0;
int USB_mass_scanned = 0;  //0==Not_found_OR_No_Multi 1==found_Multi_mass_once
int USB_mass_loaded = 0;   //0==none, 1==internal, 2==external

int readGENERIC(const char *path, FILEINFO *info, int max);

#define USB_BROWSER_MAX_DRIVES 2
#define USB_DISCOVERY_ATTEMPTS 3
#define USB_DISCOVERY_SETTLE_MS 500
#define ROOT_MC_POLL_INTERVAL_MS 500
#define MC_BROWSER_RETRY_COUNT 4
#define MC_BROWSER_RETRY_DELAY_US 100000

static int root_mc_present[2] = {1, 1};
static int root_mc_poll_active = 0;
static int root_mc_poll_port = 0;
static int root_mc_poll_type = 0;
static int root_mc_poll_free = 0;
static int root_mc_poll_format = 0;
static u64 root_mc_next_poll_time = 0;
static int discard_next_mx4sio_root_listing = 0;

static int mapUsbPathToFatFsPath(const char *usb_path, char *fatfs_path)
{
	const char *suffix;
	int unit = 0;

	if (strncmp(usb_path, "usb", 3))
		return -1;

	if (usb_path[3] == ':')
		suffix = usb_path + 4;
	else if (usb_path[3] >= '0' && usb_path[3] <= '9' && usb_path[4] == ':') {
		unit = usb_path[3] - '0';
		suffix = usb_path + 5;
	} else
		return -1;

	if (*suffix == 0)
		suffix = "/";

#ifdef EXFAT
	snprintf(fatfs_path, MAX_PATH, "usb%d:%s", unit, suffix);
#else
	if (unit == 0)
		snprintf(fatfs_path, MAX_PATH, "mass:%s", suffix);
	else
		snprintf(fatfs_path, MAX_PATH, "mass%d:%s", unit, suffix);
#endif

	return 0;
}

#ifdef EXFAT
static int mapMassPathToUsbFatFsPath(const char *mass_path, char *usb_path)
{
	const char *suffix;
	int unit = 0;

	if (strncmp(mass_path, "mass", 4))
		return -1;

	if (mass_path[4] == ':')
		suffix = mass_path + 5;
	else if (mass_path[4] >= '0' && mass_path[4] <= '9' && mass_path[5] == ':') {
		unit = mass_path[4] - '0';
		suffix = mass_path + 6;
	} else
		return -1;

	if (*suffix == 0)
		suffix = "/";

	snprintf(usb_path, MAX_PATH, "usb%d:%s", unit, suffix);
	return 0;
}
#endif

static int isSimpleDeviceRootPath(const char *path)
{
	const char *colon = strchr(path, ':');

	return (colon != NULL && (colon[1] == '\0' || (colon[1] == '/' && colon[2] == '\0')));
}

static int readGENERICWithFirstOpenRetry(const char *path, FILEINFO *info, int max, int wait_budget_ms)
{
	u64 wait_start, backoff_start;
	int dd, is_root, n;

	n = readGENERIC(path, info, max);
	if (n > 0 || wait_budget_ms <= 0)
		return n;

	is_root = isSimpleDeviceRootPath(path);
	if (!is_root) {
		dd = fileXioDopen(path);
		if (dd >= 0) {
			fileXioDclose(dd);
			return n;
		}
	}

	wait_start = Timer();
	while (Timer() < wait_start + wait_budget_ms) {
		dd = fileXioDopen(path);
		if (dd >= 0) {
			fileXioDclose(dd);
			n = readGENERIC(path, info, max);
			if (n > 0 || !is_root)
				break;
		}

		backoff_start = Timer();
		while (Timer() < backoff_start + 100) {
		}
	}

	return n;
}

//char debugs[4096]; //For debug display strings. Comment it out when unused
//--------------------------------------------------------------
//executable code
//--------------------------------------------------------------
//--------------------------------------------------------------
#ifdef DVRP
//--------------------------------------------------------------
// The above modified for the DVRP.
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
#endif
//--------------------------------------------------------------
// unmountAll can unmount all mountpoints from 0 to MOUNT_LIMIT,
// but unlike the individual unmountParty, it will only do so
// for mountpoints indicated as used by the matching string in
// the string array 'mountedParty'.
// From v4.23 this routine is also used to unmount VMC devices
//------------------------------
//ends unmountAll
//--------------------------------------------------------------


//--------------------------------------------------------------
//--------------------------------------------------------------
static int waitMemoryCardReadyForBrowse(int port)
{
	int attempt, start_ret, ret;

	ret = sceMcResFailDetect;
	for (attempt = 0; attempt < MC_BROWSER_RETRY_COUNT; attempt++) {
		mcSync(0, NULL, NULL);
		start_ret = mcGetInfo(port, 0, &mctype_PSx, NULL, NULL);
		if (start_ret < 0)
			ret = start_ret;
		else
			mcSync(0, NULL, &ret);

		if (ret == sceMcResSucceed)
			return sceMcResSucceed;
		if (ret == sceMcResChangedCard) {
			if (attempt + 1 == MC_BROWSER_RETRY_COUNT)
				return sceMcResSucceed;
		} else
			break;
		DelayThread(MC_BROWSER_RETRY_DELAY_US);
	}

	return ret;
}

int readMC(const char *path, FILEINFO *info, int max)
{
	static sceMcTblGetDir mcDir[MAX_ENTRY] __attribute__((aligned(64)));
	char dir[MAX_PATH];
	int i, j, port, ret;

	port = path[2] - '0';
	if (port < 0 || port > 1)
		return 0;

	ensureMemoryCardPortAccessible(port);

	ret = waitMemoryCardReadyForBrowse(port);
	if (mctype_PSx == 2)  //PS2 MC ?
		time_valid = 1;
	size_valid = 1;
	if (ret != sceMcResSucceed)
		return 0;

	strcpy(dir, &path[4]);
	strcat(dir, "*");
	for (i = 0; i < MC_BROWSER_RETRY_COUNT; i++) {
		mcGetDir(port, 0, dir, 0, MAX_ENTRY - 2, mcDir);
		mcSync(0, NULL, &ret);
		if (ret >= 0)
			break;
		if (ret != sceMcResChangedCard)
			break;
		if (waitMemoryCardReadyForBrowse(port) != sceMcResSucceed)
			break;
		DelayThread(MC_BROWSER_RETRY_DELAY_US);
	}
	if (ret < 0)
		return 0;

	for (i = j = 0; i < ret; i++) {
		if (mcDir[i].AttrFile & sceMcFileAttrSubdir &&
		    (!strcmp((char *)mcDir[i].EntryName, ".") || !strcmp((char *)mcDir[i].EntryName, "..")))
			continue;  //Skip pseudopaths "." and ".."
		strcpy(info[j].name, (char *)mcDir[i].EntryName);
		info[j].stats = mcDir[i];
		j++;
	}

	return j;
}
//------------------------------
//endfunc readMC
//--------------------------------------------------------------
#ifdef XFROM
static const char *getXfromRelativePath(const char *path)
{
	const char *relative_path = strchr(path, ':');

	if (relative_path == NULL)
		return path;
	relative_path++;
	if (*relative_path == '/')
		relative_path++;
	return relative_path;
}

int readXFROM(const char *path, FILEINFO *info, int max)
{
	static sceMcTblGetDir mcDir[MAX_ENTRY] __attribute__((aligned(64)));
	char dir[MAX_PATH];
	int i, j, ret, limit;

	if (!console_is_PSX || !loadFlashModules())
		return 0;

	xfromSync(0, NULL, NULL);

	xfromGetInfo(0, 0, &mctype_PSx, NULL, NULL);
	xfromSync(0, NULL, &ret);
	if (mctype_PSx == 2)  //PS2 MC ?
		time_valid = 1;
	size_valid = 1;

	strncpy(dir, getXfromRelativePath(path), sizeof(dir) - 2);
	dir[sizeof(dir) - 2] = '\0';
	strcat(dir, "*");

	limit = (max < MAX_ENTRY - 2) ? max : MAX_ENTRY - 2;
	xfromGetDir(0, 0, dir, 0, limit, mcDir);
	xfromSync(0, NULL, &ret);
	if (ret < 0)
		return 0;

	for (i = j = 0; i < ret && j < max; i++) {
		if (mcDir[i].AttrFile & sceMcFileAttrSubdir &&
		    (!strcmp((char *)mcDir[i].EntryName, ".") || !strcmp((char *)mcDir[i].EntryName, "..")))
			continue;  //Skip pseudopaths "." and ".."
		strcpy(info[j].name, (char *)mcDir[i].EntryName);
		info[j].stats = mcDir[i];
		j++;
	}

	return j;
}
//------------------------------
//endfunc readXFROM
//--------------------------------------------------------------
#endif
#ifndef LIBCDVD_LEGACY
int readCD(const char *path, FILEINFO *info, int max)
{
	iox_dirent_t record;
	int n = 0, dd = -1;
	u64 wait_start;

	if (sceCdGetDiskType() <= SCECdUNKNOWN) {
		wait_start = Timer();
		while ((Timer() < wait_start + 500) && !uLE_cdDiscValid()) {
			if (cdmode == SCECdNODISC)
				return 0;
		}
		if (cdmode == SCECdNODISC)
			return 0;
		if ((cdmode < SCECdPSCD) || (cdmode > SCECdPS2DVD)) {
			uLE_cdStop();
			return 0;
		}
	}
	if ((dd = fileXioDopen(path)) < 0)
		goto exit;  //exit if error opening directory
	while (fileXioDread(dd, &record) > 0) {
		if ((FIO_S_ISDIR(record.stat.mode)) && (!strcmp(record.name, ".") || !strcmp(record.name, "..")))
			continue;  //Skip entry if pseudo-folder "." or ".."
		strcpy(info[n].name, record.name);
		clearMcTable(&info[n].stats);
		if (FIO_S_ISDIR(record.stat.mode)) {
			info[n].stats.AttrFile = MC_ATTR_norm_folder;
		} else if (FIO_S_ISREG(record.stat.mode)) {
			info[n].stats.AttrFile = MC_ATTR_norm_file;
			info[n].stats.FileSizeByte = record.stat.size;
			info[n].stats.Reserve2 = 0;
		} else
			continue;  //Skip entry which is neither a file nor a folder
		snprintf((char *)info[n].stats.EntryName, 32, "%.31s", info[n].name);
		memcpy((void *)&info[n].stats._Create, record.stat.ctime, 8);
		memcpy((void *)&info[n].stats._Modify, record.stat.mtime, 8);
		n++;
		if (n == max)
			break;
	}  //ends while
	size_valid = 1;

exit:
	if (dd >= 0)
		fileXioDclose(dd);  //Close directory if opened above
	return n;
}
#else
#include "snippets/readCD.h"
#endif
//------------------------------
//endfunc readCD
//--------------------------------------------------------------
static int getHddDeviceFromPath(const char *path, char *device)
{
	if (strncmp(path, "hdd", 3) || path[3] < '0' || path[3] > '9' || path[4] != ':')
		return -1;

	if (device != NULL) {
		memcpy(device, path, 5);
		device[5] = '\0';
	}
	return 0;
}

static int isHddRootPath(const char *path)
{
	return (getHddDeviceFromPath(path, NULL) >= 0 && path[5] == '/' && path[6] == '\0');
}

static int isAtaPath(const char *path)
{
	return (!strncmp(path, "ata", 3) && ((path[3] >= '0' && path[3] <= '9' && path[4] == ':') || path[3] == ':'));
}

static const char *canonicalizeAtaPath(const char *path, char *buffer)
{
	if (!strncmp(path, "ata:", 4)) {
		if (path[4] == '\0')
			strcpy(buffer, "ata0:/");
		else
			snprintf(buffer, MAX_PATH, "ata0:%s", path + 4);
		return buffer;
	}
	if (!strncmp(path, "ata", 3) && path[3] >= '0' && path[3] <= '9' && path[4] == ':' && path[5] == '\0') {
		snprintf(buffer, MAX_PATH, "ata%c:/", path[3]);
		return buffer;
	}

	return path;
}

static int getHddUnitFromDevice(const char *device)
{
	if (device == NULL || strncmp(device, "hdd", 3) || device[3] < '0' || device[3] > '1' || device[4] != ':')
		return -1;

	return device[3] - '0';
}

static void setPartyListForDevice(const char *device)
{
	iox_dirent_t dirEnt;
	int hddFd, unit;

	unit = getHddUnitFromDevice(device);
	if (unit < 0)
		return;

	hddNparties[unit] = 0;

	if ((hddFd = fileXioDopen(device)) < 0)
		return;
	while (fileXioDread(hddFd, &dirEnt) > 0) {
		if (hddNparties[unit] >= MAX_PARTITIONS)
			break;
		if ((dirEnt.stat.attr != ATTR_MAIN_PARTITION) || (dirEnt.stat.mode != FS_TYPE_PFS))
			continue;

		//Patch this to see if new CB versions use valid PFS format
		//NB: All CodeBreaker versions up to v9.3 use invalid formats
		/*	if(!strncmp(dirEnt.name, "PP.",3)){
			int len = strlen(dirEnt.name);
			if(!strcmp(dirEnt.name+len-4, ".PCB"))
				continue;
		}

		if(!strncmp(dirEnt.name, "__", 2) &&
			strcmp(dirEnt.name, "__boot") &&
			strcmp(dirEnt.name, "__net") &&
			strcmp(dirEnt.name, "__system") &&
			strcmp(dirEnt.name, "__sysconf") &&
			strcmp(dirEnt.name, "__contents") &&   // this is where PSBBN used to store it's downloaded contents. Adding it is useful.
			strcmp(dirEnt.name, "__common"))
			continue;
	*/
		{
			size_t len = 0;
			while ((len < MAX_PART_NAME) && (dirEnt.name[len] != '\0')) {
				len++;
			}
			memcpy(hddParties[unit][hddNparties[unit]], dirEnt.name, len);
			hddParties[unit][hddNparties[unit]][len] = '\0';
			hddNparties[unit]++;
		}
	}
	fileXioDclose(hddFd);
}

void setPartyList(void)
{
	setPartyListForDevice("hdd0:");
}
#ifdef DVRP
void setDVRPPartyList(void)
{
	iox_dirent_t dirEnt;
	int hddFd;

	ndvrpparties = 0;

	if ((hddFd = fileXioDopen("dvr_hdd0:")) < 0)
		return;
	while (fileXioDread(hddFd, &dirEnt) > 0) {
		if (ndvrpparties >= MAX_PARTITIONS)
			break;
		if ((dirEnt.stat.attr != ATTR_MAIN_PARTITION) || (dirEnt.stat.mode != FS_TYPE_PFS))
			continue;

		//Patch this to see if new CB versions use valid PFS format
		//NB: All CodeBreaker versions up to v9.3 use invalid formats
		/*	if(!strncmp(dirEnt.name, "PP.",3)){
			int len = strlen(dirEnt.name);
			if(!strcmp(dirEnt.name+len-4, ".PCB"))
				continue;
		}
		if(!strncmp(dirEnt.name, "__", 2) &&
			strcmp(dirEnt.name, "__boot") &&
			strcmp(dirEnt.name, "__net") &&
			strcmp(dirEnt.name, "__system") &&
			strcmp(dirEnt.name, "__sysconf") &&
			strcmp(dirEnt.name, "__contents") &&   // this is where PSBBN used to store it's downloaded contents. Adding it is useful.
			strcmp(dirEnt.name, "__common"))
			continue;
	*/
		{
			size_t len = 0;
			while ((len < MAX_PART_NAME) && (dirEnt.name[len] != '\0')) {
				len++;
			}
			memcpy(dvrParties[ndvrpparties], dirEnt.name, len);
			dvrParties[ndvrpparties][len] = '\0';
			ndvrpparties++;
		}
	}
	fileXioDclose(hddFd);
}
//--------------------------------------------------------------
#endif

void invalidatePartitionCaches(void)
{
	hddNparties[0] = 0;
	hddNparties[1] = 0;
#ifdef DVRP
	ndvrpparties = 0;
#endif
}

//--------------------------------------------------------------
// The following group of file handling functions are used to allow
// the main program to access files without having to deal with the
// difference between device-specific needs directly in each call.
// Even so, special paths are assumed to be already prepared so
// as to be accepted by fileXio calls (so HDD file access use "pfs")
// Generic functions for this purpose that have been added so far are:
// genInit(void), genLimObjName(uLE_path, reserve),
// genFixPath(uLE_path, gen_path),
// genOpen(path, mode), genClose(fd), genDopen(path), genDclose(fd),
// genLseek(fd,where,how), genRead(fd,buf,size), genWrite(fd,buf,size)
// genRemove(path), genRmdir(path)
//--------------------------------------------------------------
//------------------------------
//endfunc genLimObjName
//--------------------------------------------------------------
int genFixPath(const char *inp_path, char *gen_path)
{
	char uLE_path[MAX_PATH], loc_path[MAX_PATH], ata_path[MAX_PATH], hdd_device[6], party[MAX_NAME], *p;
	const char *canonical_path;
	char *pathSep;
	int part_ix, hdd_unit;

	part_ix = 99;  //Assume valid non-HDD path
	if (!uLE_related(uLE_path, inp_path))
		part_ix = -99;           //Assume invalid uLE_related path
	strcpy(gen_path, uLE_path);  //Assume no path patching needed
	pathSep = strchr(uLE_path, '/');

	if (!strncmp(uLE_path, "mc1:", 4)) {
		ensureMemoryCardPortAccessible(1);
	} else if (!strncmp(uLE_path, "cdfs", 4)) {  //if using CD or DVD disc path
		LCDVD_FLUSHCACHE();
		LCDVD_DISKREADY(0);
		//end of clause for using a CD or DVD path

	} else if (!strncmp(uLE_path, "usb", 3)) {  //if using USB alias path
		char fatfs_path[MAX_PATH];
		char *fatfs_sep;

		loadUsbModules();
		if (mapUsbPathToFatFsPath(uLE_path, fatfs_path) < 0) {
			part_ix = -1;
		} else {
			strcpy(gen_path, fatfs_path);
			fatfs_sep = strchr(gen_path, '/');
			if (fatfs_sep && (fatfs_sep - gen_path < 7) && fatfs_sep[-1] == ':')
				strcpy(gen_path + (fatfs_sep - gen_path), fatfs_sep + 1);
		}
		//end of clause for using a USB alias path

	} else if (!strncmp(uLE_path, "mass", 4)) {  //if using USB mass: path
		loadUsbModules();
#ifdef EXFAT
		if (mapMassPathToUsbFatFsPath(uLE_path, gen_path) < 0)
			part_ix = -1;
		else {
			pathSep = strchr(gen_path, '/');
			if (pathSep && (pathSep - gen_path < 7) && pathSep[-1] == ':')
				strcpy(gen_path + (pathSep - gen_path), pathSep + 1);
		}
#else
		if (pathSep && (pathSep - uLE_path < 7) && pathSep[-1] == ':')
			strcpy(gen_path + (pathSep - uLE_path), pathSep + 1);
#endif
		//end of clause for using a USB mass: path

	}
#if defined(ETH) || defined(UDPFS)
	else if (!strncmp(uLE_path, "host", 4) || !strncmp(uLE_path, "udpfs", 5)) {
		makeHostPath(gen_path, uLE_path);
	}
#endif
	else if (isAtaPath(uLE_path)) {  //if using ATA BDM path
#ifdef EXFAT
		loadAtaModules();
#endif
		canonical_path = canonicalizeAtaPath(uLE_path, ata_path);
		strcpy(gen_path, canonical_path);
		pathSep = strchr(gen_path, '/');
		if (pathSep && (pathSep - gen_path < 7) && pathSep[-1] == ':')
			strcpy(gen_path + (pathSep - gen_path), pathSep + 1);

#ifdef MX4SIO
	} else if (!strncmp(uLE_path, "mx4sio", 6)) {
		if (!mx4sio_driver_running && !loadMx4sioModules())
			part_ix = -1;
		if ((part_ix >= 0) && pathSep && (pathSep - uLE_path < 10) && pathSep[-1] == ':')
			strcpy(gen_path + (pathSep - uLE_path), pathSep + 1);
#endif
#ifdef MMCE
	} else if (!strncmp(uLE_path, "mmce", 4)) {
		loadMmceModules();
#endif
	} else if (getHddDeviceFromPath(uLE_path, hdd_device) >= 0 && uLE_path[5] == '/') {  //If using HDD path
		hdd_unit = getHddUnitFromDevice(hdd_device);
		if (hdd_unit < 0) {
			part_ix = -1;
			genLimObjName(gen_path, 0);
			return part_ix;
		}
		//Get path on HDD unit, LaunchELF's format (e.g. hdd0:/partition/path/to/file)
		strcpy(loc_path, uLE_path + 6);
		if ((p = strchr(loc_path, '/')) != NULL) {
			//Extract path to file within partition. Make a new path, relative to the filesystem root.
			//hddN:/partition/path/to/file becomes pfs0:/path/to/file.
			sprintf(gen_path, "pfs0:%s", p);
			*p = 0;  //null-terminate the block device path (hddN:/partition).
		} else {
			//Otherwise, default to /
			strcpy(gen_path, "pfs0:/");
		}
		//Generate standard path to the block device (i.e. hddN:/partition results in hddN:partition)
		sprintf(party, "%s%s", hdd_device, loc_path);
		if (hddNparties[hdd_unit] == 0) {
			//No partitions recognized? Load modules & populate partition list.
			loadHddModules();
			setPartyListForDevice(hdd_device);
		}
		//Mount the partition.
		if ((part_ix = mountParty(party)) >= 0)
			gen_path[3] = part_ix + '0';
#ifdef DVRP
	} else if (!strncmp(uLE_path, "dvr_hdd0:/", 10)) {  //If using DVRP HDD path
		if (!console_is_PSX) {
			part_ix = -1;
			genLimObjName(gen_path, 0);
			return part_ix;
		}
		//Get path on DVR HDD unit, LaunchELF's format (e.g. dvr_hdd0:/partition/path/to/file)
		strcpy(loc_path, uLE_path + 10);
		if ((p = strchr(loc_path, '/')) != NULL) {
			//Extract path to file within partition. Make a new path, relative to the filesystem root.
			//dvr_hdd0:/partition/path/to/file becomes dvr_pfs0:/path/to/file.
			sprintf(gen_path, "dvr_pfs0:%s", p);
			*p = 0;  //null-terminate the block device path (dvr_hdd0:/partition).
		} else {
			//Otherwise, default to /
			strcpy(gen_path, "dvr_pfs0:/");
		}
		//Generate standard path to the block device (i.e. dvr_hdd0:/partition results in hdd0:partition)
		sprintf(party, "dvr_hdd0:%s", loc_path);
		//If partition is __xdata, use dvr_pfs1:
		gen_path[7] = (getDVRPPartyMountIndex(party) == 1) ? '1' : '0';
		if (ndvrpparties == 0) {
			//No partitions recognized? Load modules & populate partition list.
			loadDVRPHddModules();
			setDVRPPartyList();
		}
		//Mount the partition.
		if ((part_ix = mountDVRPParty(party)) >= 0)
			gen_path[7] = part_ix + '0';
		//end of clause for using an HDD path
#endif
	}
	genLimObjName(gen_path, 0);
	return part_ix;
	//non-HDD Path => 99, Good HDD Path => 0-3, Bad Path => negative
}
//------------------------------
//endfunc genFixPath
//--------------------------------------------------------------
//--------------------------------------------------------------
int readVMC(const char *path, FILEINFO *info, int max)
{
	iox_dirent_t dirbuf;
	char dir[MAX_PATH];
	int i = 0, fd;

	strcpy(dir, path);
	if ((fd = fileXioDopen(dir)) < 0)
		return 0;

	while (fileXioDread(fd, &dirbuf) > 0) {
		unsigned int vmc_mc_mode = dirbuf.stat.private_0;

		if (!strcmp(dirbuf.name, ".") || !strcmp(dirbuf.name, ".."))
			continue;  //Skip pseudopaths "." and ".."

		strcpy(info[i].name, dirbuf.name);
		clearMcTable(&info[i].stats);
		if (vmc_mc_mode & sceMcFileAttrExists) {
			info[i].stats.AttrFile = vmc_mc_mode;
			info[i].stats.Reserve1 = (u16)dirbuf.stat.private_1;
			info[i].stats.Reserve2 = dirbuf.stat.private_2;
			if (!(vmc_mc_mode & sceMcFileAttrSubdir))
				info[i].stats.FileSizeByte = dirbuf.stat.size;
		} else if (FIO_S_ISDIR(dirbuf.stat.mode)) {
			info[i].stats.AttrFile = MC_ATTR_norm_folder;
		} else if (FIO_S_ISREG(dirbuf.stat.mode)) {
			info[i].stats.AttrFile = MC_ATTR_norm_file;
			info[i].stats.FileSizeByte = dirbuf.stat.size;
		} else if (dirbuf.stat.mode & sceMcFileAttrSubdir) {
			info[i].stats.AttrFile = dirbuf.stat.mode;
		} else if (dirbuf.stat.mode & sceMcFileAttrFile) {
			info[i].stats.AttrFile = dirbuf.stat.mode;
			info[i].stats.FileSizeByte = dirbuf.stat.size;
		} else
			continue;  //Skip entry which is neither a file nor a folder
		snprintf((char *)info[i].stats.EntryName, 32, "%.31s", info[i].name);
		memcpy((void *)&info[i].stats._Create, dirbuf.stat.ctime, 8);
		memcpy((void *)&info[i].stats._Modify, dirbuf.stat.mtime, 8);
		i++;
		if (i == max)
			break;
	}

	fileXioDclose(fd);

	size_valid = 1;
	time_valid = 1;

	return i;
}
//------------------------------
//endfunc readVMC
//--------------------------------------------------------------
int readHDD(const char *path, FILEINFO *info, int max)
{
	iox_dirent_t dirbuf;
	char hdd_device[6], party[MAX_PATH], dir[MAX_PATH];
	int i = 0, fd, ret, hdd_unit;

	if (getHddDeviceFromPath(path, hdd_device) < 0)
		return 0;

	hdd_unit = getHddUnitFromDevice(hdd_device);
	if (hdd_unit < 0)
		return 0;

	if (hddNparties[hdd_unit] == 0) {
		loadHddModules();
		setPartyListForDevice(hdd_device);
	}

	if (isHddRootPath(path)) {
		unmountHddPartiesNotNeededByClipboard();
		for (i = 0; i < hddNparties[hdd_unit]; i++) {
			strcpy(info[i].name, hddParties[hdd_unit][i]);
			info[i].stats.AttrFile = MC_ATTR_norm_folder;
		}
		return hddNparties[hdd_unit];
	}

	getHddParty(path, NULL, party, dir);
	ret = mountParty(party);
	if (ret < 0)
		return 0;
	dir[3] = ret + '0';

	if ((fd = fileXioDopen(dir)) < 0)
		return 0;

	while (fileXioDread(fd, &dirbuf) > 0) {
		if (dirbuf.stat.mode & FIO_S_IFDIR &&
		    (!strcmp(dirbuf.name, ".") || !strcmp(dirbuf.name, "..")))
			continue;  //Skip pseudopaths "." and ".."

		strcpy(info[i].name, dirbuf.name);
		clearMcTable(&info[i].stats);
		if (dirbuf.stat.mode & FIO_S_IFDIR) {
			info[i].stats.AttrFile = MC_ATTR_norm_folder;
		} else if (dirbuf.stat.mode & FIO_S_IFREG) {
			info[i].stats.AttrFile = MC_ATTR_norm_file;
			info[i].stats.FileSizeByte = dirbuf.stat.size;
			info[i].stats.Reserve2 = dirbuf.stat.hisize;
		} else
			continue;  //Skip entry which is neither a file nor a folder
		snprintf((char *)info[i].stats.EntryName, 32, "%.31s", info[i].name);
		memcpy((void *)&info[i].stats._Create, dirbuf.stat.ctime, 8);
		memcpy((void *)&info[i].stats._Modify, dirbuf.stat.mtime, 8);
		i++;
		if (i == max)
			break;
	}

	fileXioDclose(fd);

	size_valid = 1;
	time_valid = 1;

	return i;
}
//------------------------------
//endfunc readHDD
//--------------------------------------------------------------
#ifdef DVRP
int readHDDDVRP(const char *path, FILEINFO *info, int max)
{
	iox_dirent_t dirbuf;
	char party[MAX_PATH], dir[MAX_PATH];
	int i = 0, fd, ret;

	if (!console_is_PSX)
		return 0;

	if (ndvrpparties == 0) {
		loadDVRPHddModules();
		setDVRPPartyList();
	}

	if (!strcmp(path, "dvr_hdd0:/")) {
		for (i = 0; i < ndvrpparties; i++) {
			strcpy(info[i].name, dvrParties[i]);
			info[i].stats.AttrFile = MC_ATTR_norm_folder;
		}
		return ndvrpparties;
	}

	getHddDVRPParty(path, NULL, party, dir);
	ret = mountDVRPParty(party);
	if (ret < 0)
		return 0;
	dir[7] = ret + '0';

	if ((fd = fileXioDopen(dir)) < 0)
		return 0;

	while (fileXioDread(fd, &dirbuf) > 0) {
		if (dirbuf.stat.mode & FIO_S_IFDIR &&
		    (!strcmp(dirbuf.name, ".") || !strcmp(dirbuf.name, "..")))
			continue;  //Skip pseudopaths "." and ".."

		strcpy(info[i].name, dirbuf.name);
		clearMcTable(&info[i].stats);
		if (dirbuf.stat.mode & FIO_S_IFDIR) {
			info[i].stats.AttrFile = MC_ATTR_norm_folder;
		} else if (dirbuf.stat.mode & FIO_S_IFREG) {
			info[i].stats.AttrFile = MC_ATTR_norm_file;
			info[i].stats.FileSizeByte = dirbuf.stat.size;
			info[i].stats.Reserve2 = dirbuf.stat.hisize;
		} else
			continue;  //Skip entry which is neither a file nor a folder
		snprintf((char *)info[i].stats.EntryName, 32, "%.31s", info[i].name);
		memcpy((void *)&info[i].stats._Create, dirbuf.stat.ctime, 8);
		memcpy((void *)&info[i].stats._Modify, dirbuf.stat.mtime, 8);
		i++;
		if (i == max)
			break;
	}

	fileXioDclose(fd);

	size_valid = 1;
	time_valid = 1;

	return i;
}
//------------------------------
//endfunc readHDDDVRP
//--------------------------------------------------------------
#endif

static void waitUsbDiscoverySettle(void)
{
	u64 wait_start;

	wait_start = Timer();
	while (Timer() < wait_start + USB_DISCOVERY_SETTLE_MS) {
	}
}

static void scanUsbMassDevices(int force)
{
	int i, found, max_drives;
	iox_stat_t chk_stat;
#ifdef EXFAT
	char usb_path[8] = "usb0:/";
#else
	char usb_path[8] = "mass0:/";
#endif

	loadUsbModules();
	if (!USB_mass_loaded) {
		if (force) {
			for (i = 0; i < 10; i++)
				USB_mass_ix[i] = 0;
			USB_mass_scanned = 0;
		}
		return;
	}

	if ((USB_mass_max_drives < 1)
	    || (!force && ((USB_mass_max_drives < 2)  //No need for dynamic lists with only one drive
	                  || (USB_mass_scanned && ((Timer() - USB_mass_scan_time) < 5000)))))
		return;

	max_drives = USB_mass_max_drives;
	if (max_drives > USB_BROWSER_MAX_DRIVES)
		max_drives = USB_BROWSER_MAX_DRIVES;

	found = 0;
	for (i = 0; i < max_drives; i++) {
#ifdef EXFAT
		usb_path[3] = '0' + i;
#else
		usb_path[4] = '0' + i;
#endif
		if (fileXioGetStat(usb_path, &chk_stat) < 0) {
			USB_mass_ix[i] = 0;
			continue;
		}
		USB_mass_ix[i] = '0' + i;
		found = 1;
	}  //ends for loop
	for (i = max_drives; i < 10; i++)
		USB_mass_ix[i] = 0;

	USB_mass_scanned = found;
	if (found)
		USB_mass_scan_time = Timer();
}

void scan_USB_mass(void)
{
	scanUsbMassDevices(0);
}
//------------------------------
//endfunc scan_USB_mass
//--------------------------------------------------------------
int prepareUsbRootBrowse(void)
{
	int attempt, first_unit, unit;

	first_unit = -1;
	for (attempt = 0; attempt < USB_DISCOVERY_ATTEMPTS; attempt++) {
		scanUsbMassDevices(1);
		first_unit = -1;
		for (unit = 0; unit < USB_BROWSER_MAX_DRIVES; unit++) {
			if (USB_mass_ix[unit]) {
				first_unit = unit;
				break;
			}
		}
		if ((USB_mass_ix[0] && USB_mass_ix[1]) || attempt == USB_DISCOVERY_ATTEMPTS - 1)
			break;
		waitUsbDiscoverySettle();
	}

	return first_unit;
}

//------------------------------
//endfunc prepareUsbRootBrowse
//--------------------------------------------------------------
static int addRootUsbDeviceEntry(FILEINFO *files, int nfiles, int unit)
{
	if (unit < 0) {
		strcpy(files[nfiles].name, "usb:");
	} else if (unit <= 9) {
		snprintf(files[nfiles].name, sizeof(files[nfiles].name), "usb%d:", unit);
	} else
		return nfiles;

	files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;

	return nfiles;
}

static int addRootUsbDeviceEntries(FILEINFO *files, int nfiles)
{
	int i, added;

	added = 0;
	if (USB_mass_scanned) {
		for (i = 0; (i < USB_mass_max_drives) && (i < USB_BROWSER_MAX_DRIVES); i++) {
			if (USB_mass_ix[i] == 0)
				continue;

			nfiles = addRootUsbDeviceEntry(files, nfiles, i);
			added = 1;
		}
	}

	if (!added)
		nfiles = addRootUsbDeviceEntry(files, nfiles, -1);

	return nfiles;
}

static int isMx4sioRootPath(const char *path)
{
	if (path == NULL)
		return FALSE;

	return (!strcmp(path, "mx4sio:") || !strcmp(path, "mx4sio:/") ||
	        !strcmp(path, "mx4sio0:") || !strcmp(path, "mx4sio0:/"));
}

void discardNextMx4sioRootListing(const char *path)
{
	if (isMx4sioRootPath(path))
		discard_next_mx4sio_root_listing = 1;
}

static int memoryCardResultIndicatesPresent(int ret)
{
	return (ret == -1 || ret == 0);
}

static int rootMemoryCardExists(int port)
{
	if (port < 0 || port > 1)
		return FALSE;

	if (boot_show_all_devices || setting == NULL || !setting->Hide_MCMMCE)
		return TRUE;

	return root_mc_present[port];
}

int pollRootMemoryCardDevices(void)
{
	int result, sync, changed, start_ret;

	if (root_mc_poll_active) {
		sync = mcSync(1, NULL, &result);
		if (sync == 0)
			return FALSE;

		root_mc_poll_active = 0;
		root_mc_next_poll_time = Timer() + ROOT_MC_POLL_INTERVAL_MS;
		if (sync < 0)
			return FALSE;

		changed = (root_mc_present[root_mc_poll_port] != memoryCardResultIndicatesPresent(result));
		root_mc_present[root_mc_poll_port] = memoryCardResultIndicatesPresent(result);
		root_mc_poll_port = (root_mc_poll_port + 1) & 1;
		return changed;
	}

	if (Timer() < root_mc_next_poll_time)
		return FALSE;

	start_ret = mcGetInfo(root_mc_poll_port, 0, &root_mc_poll_type, &root_mc_poll_free, &root_mc_poll_format);
	if (start_ret == 0)
		root_mc_poll_active = 1;
	else
		root_mc_next_poll_time = Timer() + ROOT_MC_POLL_INTERVAL_MS;

	return FALSE;
}

//------------------------------
//endfunc addRootUsbDeviceEntries
//--------------------------------------------------------------
int readGENERIC(const char *path, FILEINFO *info, int max)
{
	iox_dirent_t record;
	int n = 0, dd = -1;

	if ((dd = fileXioDopen(path)) < 0)
		goto exit;  //exit if error opening directory
	while (fileXioDread(dd, &record) > 0) {
		if ((FIO_S_ISDIR(record.stat.mode)) && (!strcmp(record.name, ".") || !strcmp(record.name, "..")))
			continue;  //Skip entry if pseudo-folder "." or ".."

		strcpy(info[n].name, record.name);
		clearMcTable(&info[n].stats);
		if (FIO_S_ISDIR(record.stat.mode)) {
			info[n].stats.AttrFile = MC_ATTR_norm_folder;
		} else if (FIO_S_ISREG(record.stat.mode)) {
			info[n].stats.AttrFile = MC_ATTR_norm_file;
			info[n].stats.FileSizeByte = record.stat.size;
			info[n].stats.Reserve2 = record.stat.hisize;
		} else {
			DPRINTF("%s:UNKNOWN_FILEMODE:('%s', 0x%x, siz:%d, attr:%d)\n", record.name, record.stat.mode, record.stat.size, record.stat.attr);
			continue;  //Skip entry which is neither a file nor a folder
		}
		snprintf((char *)info[n].stats.EntryName, 32, "%.31s", info[n].name);
		memcpy((void *)&info[n].stats._Create, record.stat.ctime, 8);
		memcpy((void *)&info[n].stats._Modify, record.stat.mtime, 8);
		n++;
		if (n == max)
			break;
	}  //ends while
	size_valid = 1;
	time_valid = 1;

exit:
	if (dd >= 0)
		fileXioDclose(dd);  //Close directory if opened above
	return n;
}
//------------------------------
//endfunc readGENERIC
//--------------------------------------------------------------
#ifdef MX4SIO
static void drainGENERIC(const char *path)
{
	iox_dirent_t record;
	int dd;

	if ((dd = fileXioDopen(path)) < 0)
		return;
	while (fileXioDread(dd, &record) > 0) {
	}
	fileXioDclose(dd);
}
#endif

#if defined(ETH) || defined(UDPFS)
char *makeHostPath(char *dp, char *sp)
{
#ifdef ETH
	int i;
	const char *src;
	char tmp[MAX_PATH];

	if (!strncmp(sp, "host:", 5)) {
		src = sp + 5;
		if (*src == '/')
			src++;
		snprintf(tmp, sizeof(tmp), "host:%s", src);

		if (!host_use_Bsl)
			return strcpy(dp, tmp);

		strcpy(dp, tmp);
		for (i = 5; dp[i] != '\0'; i++)
			if (dp[i] == '/')
				dp[i] = '\\';
		return dp;
	}
#endif
#ifdef UDPFS
	if (!strncmp(sp, "udpfs:", 6))
		goto copy_passthrough;
#endif

copy_passthrough:
	if (dp != sp)
		strcpy(dp, sp);
	return dp;
}
#endif
//--------------------------------------------------------------
char *makeFslPath(char *dp, char *sp)
{
	int i;
	char ch;

	for (i = 0; i < MAX_PATH - 1; i++) {
		ch = sp[i];
		if (ch == '\\')
			dp[i] = '/';
		else
			dp[i] = ch;
		if (!ch)
			break;
	}
	return dp;
}
//--------------------------------------------------------------
#if defined(ETH) || defined(UDPFS)
#ifdef UDPFS
static void initUDPFS(void)
{
	int fd;

	load_udpfs();
	host_error = 0;
	host_elflist = 0;
	if ((fd = fileXioDopen("udpfs:/")) >= 0)
		fileXioDclose(fd);
	else
		host_error = 1;
	host_ready = 1;
}
#endif

#ifdef ETH
void initHOST(void)
{
	int fd;

	load_ps2host();
	host_error = 0;
	if ((fd = fileXioOpen("host:elflist.txt", FIO_O_RDONLY, 0)) >= 0) {
		fileXioClose(fd);
		host_elflist = 1;
	} else {
		host_elflist = 0;
		if ((fd = fileXioDopen("host:")) >= 0)
			fileXioDclose(fd);
		else
			host_error = 1;
	}
	host_ready = 1;
}
#endif
#ifdef ETH
int readHOST(const char *path, FILEINFO *info, int max)
{
	iox_dirent_t hostcontent;
	int hfd, rv, hostcount = 0;
	char host_path[MAX_PATH];
	char Win_path[MAX_PATH];

	initHOST();
	snprintf(host_path, MAX_PATH, "%s", path);

	if (!strncmp(path, "host:/", 6))
		strcpy(host_path + 5, path + 6);
	if (host_elflist && !strcmp(host_path, "host:")) {
		int size, contentptr;
		char *elflisttxt, elflistchar;
		char host_next[MAX_PATH];

		if ((hfd = fileXioOpen("host:elflist.txt", FIO_O_RDONLY, 0)) < 0)
			return 0;
		if ((size = fileXioLseek(hfd, 0, SEEK_END)) <= 0) {
			fileXioClose(hfd);
			return 0;
		}
		elflisttxt = (char *)memalign(64, size);
		fileXioLseek(hfd, 0, SEEK_SET);
		fileXioRead(hfd, elflisttxt, size);
		fileXioClose(hfd);
		contentptr = 0;
		for (rv = 0; rv <= size; rv++) {
			elflistchar = elflisttxt[rv];
			if ((elflistchar == 0x0a) || (rv == size)) {
				host_next[contentptr] = 0;
				snprintf(host_path, sizeof(host_path), "host:%.*s", (int)(sizeof(host_path) - sizeof("host:")), host_next);
				clearMcTable(&info[hostcount].stats);
				if ((hfd = fileXioOpen(makeHostPath(Win_path, host_path), FIO_O_RDONLY, 0)) >= 0) {
					fileXioClose(hfd);
					info[hostcount].stats.AttrFile = MC_ATTR_norm_file;
					makeFslPath(info[hostcount++].name, host_next);
				} else if ((hfd = fileXioDopen(Win_path)) >= 0) {
					fileXioDclose(hfd);
					info[hostcount].stats.AttrFile = MC_ATTR_norm_folder;
					makeFslPath(info[hostcount++].name, host_next);
				}
				contentptr = 0;
				if (hostcount > max)
					break;
			} else if ((elflistchar != 0x0d) && (contentptr < (MAX_PATH - 1))) {
				host_next[contentptr++] = elflistchar;
			}
		}
		free(elflisttxt);
		return hostcount - 1;
	}

	if ((hfd = fileXioDopen(makeHostPath(Win_path, host_path))) < 0)
		return 0;
	strcpy(host_path, Win_path);
	while ((rv = fileXioDread(hfd, &hostcontent))) {
		if (strcmp(hostcontent.name, ".") && strcmp(hostcontent.name, "..")) {
			size_valid = 1;
			time_valid = 1;
			snprintf(Win_path, MAX_PATH - 1, "%s%s", host_path, hostcontent.name);
			strcpy(info[hostcount].name, hostcontent.name);
			clearMcTable(&info[hostcount].stats);

			if (!(hostcontent.stat.mode & FIO_S_IFDIR))  //if not a directory
				info[hostcount].stats.AttrFile = MC_ATTR_norm_file;
			else
				info[hostcount].stats.AttrFile = MC_ATTR_norm_folder;

			info[hostcount].stats.FileSizeByte = hostcontent.stat.size;
			info[hostcount].stats.Reserve2 = hostcontent.stat.hisize;  //taking an unused(?) unknown for the high bits
			memcpy((void *)&info[hostcount].stats._Create, hostcontent.stat.ctime, 8);
			info[hostcount].stats._Create.Year += 1900;
			memcpy((void *)&info[hostcount].stats._Modify, hostcontent.stat.mtime, 8);
			info[hostcount].stats._Modify.Year += 1900;
			hostcount++;
			if (hostcount >= max)
				break;
		}
	}
	fileXioDclose(hfd);
	strcpy(info[hostcount].name, "\0");
	return hostcount;
}
#endif
#ifdef UDPFS
int readUDPFS(const char *path, FILEINFO *info, int max)
{
	iox_dirent_t dirent;
	int fd, count = 0;

	udpfs_dir_open_failed = 0;
	initUDPFS();
	if (host_error) {
		udpfs_dir_open_failed = 1;
		return 0;
	}
	if ((fd = fileXioDopen(path)) < 0) {
		udpfs_dir_open_failed = 1;
		return 0;
	}

	while (fileXioDread(fd, &dirent) > 0) {
		if (strcmp(dirent.name, ".") && strcmp(dirent.name, "..")) {
			size_valid = 1;
			time_valid = 1;
			strcpy(info[count].name, dirent.name);
			clearMcTable(&info[count].stats);

			if (!(dirent.stat.mode & FIO_S_IFDIR))
				info[count].stats.AttrFile = MC_ATTR_norm_file;
			else
				info[count].stats.AttrFile = MC_ATTR_norm_folder;

			info[count].stats.FileSizeByte = dirent.stat.size;
			info[count].stats.Reserve2 = dirent.stat.hisize;
			memcpy((void *)&info[count].stats._Create, dirent.stat.ctime, 8);
			memcpy((void *)&info[count].stats._Modify, dirent.stat.mtime, 8);
			count++;
			if (count >= max)
				break;
		}
	}
	fileXioDclose(fd);
	strcpy(info[count].name, "\0");
	return count;
}
#endif
#endif
//------------------------------
//endfunc readHOST/readUDPFS
//--------------------------------------------------------------
int getDir(const char *path, FILEINFO *info)
{
	int max = MAX_ENTRY - 2;
	int n;

	if (!strncmp(path, "mc", 2))
		n = readMC(path, info, max);
#ifdef XFROM
	else if (!strncmp(path, "xfrom", 5))
		n = readXFROM(path, info, max);
#endif
	else if (!strncmp(path, "hdd", 3))
		n = readHDD(path, info, max);
#ifdef DVRP
	else if (console_is_PSX && !strncmp(path, "dvr_hdd", 7))
		n = readHDDDVRP(path, info, max);
#endif
	else if (!strncmp(path, "mass", 4)) {
		int force_usb_scan = (!strcmp(path, "mass:") || !strcmp(path, "mass:/"));
#ifdef EXFAT
		char usb_path[MAX_PATH];

		if (mapMassPathToUsbFatFsPath(path, usb_path) < 0)
			return 0;
		if (force_usb_scan)
			scanUsbMassDevices(1);
		else if (!USB_mass_scanned)
			scan_USB_mass();
		n = readGENERICWithFirstOpenRetry(usb_path, info, max, 2000);
#else
		if (force_usb_scan)
			scanUsbMassDevices(1);
		else if (!USB_mass_scanned)
			scan_USB_mass();
		n = readGENERICWithFirstOpenRetry(path, info, max, 2000);
#endif
		if (force_usb_scan)
			scanUsbMassDevices(1);
	}
	else if (!strncmp(path, "usb", 3)) {
		char usb_path[MAX_PATH];
		int force_usb_scan = (!strcmp(path, "usb:") || !strcmp(path, "usb:/"));

		if (mapUsbPathToFatFsPath(path, usb_path) < 0)
			return 0;
		if (force_usb_scan)
			scanUsbMassDevices(1);
		else if (!USB_mass_scanned)
			scan_USB_mass();
		n = readGENERICWithFirstOpenRetry(usb_path, info, max, 2000);
		if (force_usb_scan)
			scanUsbMassDevices(1);
	}
	else if (isAtaPath(path)) {
		char ata_path[MAX_PATH];
		const char *read_path;
#ifdef EXFAT
		u64 wait_start;
		u64 backoff_start;
		int dd;
		int is_ata_root;
		int wait_budget_ms;
		int path_ready;

		loadAtaModules();
		read_path = canonicalizeAtaPath(path, ata_path);
		is_ata_root = (!strncmp(read_path, "ata", 3) && read_path[3] >= '0' && read_path[3] <= '9' && read_path[4] == ':' &&
		               (read_path[5] == '\0' || (read_path[5] == '/' && read_path[6] == '\0')));
		wait_budget_ms = is_ata_root ? 2000 : 750;
#else
		read_path = canonicalizeAtaPath(path, ata_path);
#endif
		n = readGENERIC(read_path, info, max);
#ifdef EXFAT
		// First browse after loading ATA_BD can race module/device readiness.
		// Retry briefly so ataN:/ populates on first open instead of requiring re-entry.
		if (n == 0 && wait_budget_ms > 0) {
			wait_start = Timer();
			while (Timer() < wait_start + wait_budget_ms) {
				path_ready = 0;

				dd = fileXioDopen(read_path);
				if (dd >= 0) {
					fileXioDclose(dd);
					path_ready = 1;
				}

				if (path_ready) {
					n = readGENERIC(read_path, info, max);
					if (n > 0)
						break;
					if (!is_ata_root)
						break;
				}

				// Avoid tight spinning while IOP-side BDM mount events settle.
				backoff_start = Timer();
				while (Timer() < backoff_start + 100) {
				}
			}
		}
#endif
	}
	else if (!strncmp(path, "cdfs", 4)) {
		loadCdModules();
		n = readCD(path, info, max);
	}
#if defined(ETH) || defined(UDPFS)
#ifdef ETH
	else if (!strncmp(path, "host", 4))
		n = readHOST(path, info, max);
#endif
#ifdef UDPFS
	else if (!strncmp(path, "udpfs", 5))
		n = readUDPFS(path, info, max);
#endif
#endif
#ifdef MMCE
	else if (!strncmp(path, "mmce", 4)) {
		loadMmceModules();
		n = readGENERIC(path, info, max);
	}
#endif
#ifdef MX4SIO
	else if (!strncmp(path, "mx4sio", 6)) {
		char indexed_path[MAX_PATH];
		char indexed_path_alt[MAX_PATH];
		char path_alt[MAX_PATH];
		const char *suffix;
		u64 wait_start;
		u64 backoff_start;
		int is_root;
		int wait_budget_ms;
		int path_ready;
		int indexed_ready;
		int dd;
		int has_indexed_path = 0;
		int has_indexed_path_alt = 0;
		int has_path_alt = 0;

		if (!mx4sio_driver_running)
			if (!loadMx4sioModules())
				return 0;

		is_root = isMx4sioRootPath(path);
		wait_budget_ms = is_root ? 2000 : 750;

		indexed_path[0] = '\0';
		indexed_path_alt[0] = '\0';
		path_alt[0] = '\0';

		if (is_root) {
			if (!strcmp(path, "mx4sio:"))
				strcpy(path_alt, "mx4sio:/");
			else
				strcpy(path_alt, "mx4sio:");
			has_path_alt = 1;
		}

		if (is_root && discard_next_mx4sio_root_listing) {
			discard_next_mx4sio_root_listing = 0;
			drainGENERIC(path);
		}

		n = readGENERIC(path, info, max);
		if ((n == 0) && has_path_alt)
			n = readGENERIC(path_alt, info, max);

		if (path[6] == ':') {
			suffix = path + 7;
			if (*suffix == '\0') {
				/*
				 * Some stacks accept "mx4sio0:", others need "mx4sio0:/".
				 * Probe both forms for first-open compatibility.
				 */
				strcpy(indexed_path, "mx4sio0:");
				strcpy(indexed_path_alt, "mx4sio0:/");
				has_indexed_path = 1;
				has_indexed_path_alt = 1;
			} else {
				/* Try indexed typed-device form for stacks that require explicit unit number. */
				snprintf(indexed_path, sizeof(indexed_path), "mx4sio0:%s", suffix);
				has_indexed_path = 1;
				if (!strcmp(suffix, "/")) {
					strcpy(indexed_path_alt, "mx4sio0:");
					has_indexed_path_alt = 1;
				}
			}
		}

		if ((n == 0) && has_indexed_path)
			n = readGENERIC(indexed_path, info, max);
		if ((n == 0) && has_indexed_path_alt)
			n = readGENERIC(indexed_path_alt, info, max);

		/* First browse after loading MX4SIO can race mount readiness; retry briefly. */
		if (n == 0 && wait_budget_ms > 0) {
			wait_start = Timer();
			while (Timer() < wait_start + wait_budget_ms) {
				path_ready = 0;
				indexed_ready = 0;

				dd = fileXioDopen(path);
				if (dd >= 0) {
					fileXioDclose(dd);
					path_ready = 1;
				}

				if (indexed_path[0] != '\0') {
					dd = fileXioDopen(indexed_path);
					if (dd >= 0) {
						fileXioDclose(dd);
						indexed_ready = 1;
					}
				}
				if (!indexed_ready && indexed_path_alt[0] != '\0') {
					dd = fileXioDopen(indexed_path_alt);
					if (dd >= 0) {
						fileXioDclose(dd);
						indexed_ready = 1;
					}
				}

				if (path_ready || indexed_ready) {
					n = 0;
					if (path_ready)
						n = readGENERIC(path, info, max);
					if (n == 0 && has_path_alt)
						n = readGENERIC(path_alt, info, max);
					if (n == 0 && indexed_ready) {
						n = readGENERIC(indexed_path, info, max);
						if (n == 0 && indexed_path_alt[0] != '\0')
							n = readGENERIC(indexed_path_alt, info, max);
					}
					if (n > 0 || !is_root)
						break;
				}

				backoff_start = Timer();
				while (Timer() < backoff_start + 100) {
				}
			}
		}
	}
#endif
	else if (!strncmp(path, "vmc", 3))
		n = readVMC(path, info, max);
	else
		return 0;

	return n;
}
//--------------------------------------------------------------
int setFileList(const char *path, const char *ext, FILEINFO *files, int cnfmode)
{
	int nfiles, i, j, ret, allow_usb_devices;

	size_valid = 0;
	time_valid = 0;
	nfiles = 0;
	if (path[0] == 0) {
		//-- Start case for browser root pseudo folder with device links --
		if (USB_mass_scanned)  //if mass drives were scanned in earlier browsing
			scan_USB_mass();   //then allow another scan here (timer dependent)
		allow_usb_devices = ((cnfmode != USBD_IRX_CNF) && (cnfmode != USBKBD_IRX_CNF) && (cnfmode != USBMASS_IRX_CNF));

		if (rootMemoryCardExists(0)) {
			strcpy(files[nfiles].name, "mc0:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
		}
		if (rootMemoryCardExists(1)) {
			strcpy(files[nfiles].name, "mc1:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
		}

		if (allow_usb_devices) {
			nfiles = addRootUsbDeviceEntries(files, nfiles);
		}

#ifdef MMCE
		if (rootMemoryCardExists(0)) {
			strcpy(files[nfiles].name, "mmce0:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
		}
		if (rootMemoryCardExists(1)) {
			strcpy(files[nfiles].name, "mmce1:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
		}
#endif

#ifdef MX4SIO
		if (allow_usb_devices) {
			strcpy(files[nfiles].name, "mx4sio:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
		}
#endif

		if (shouldShowHddDevice(0)) {
			strcpy(files[nfiles].name, "hdd0:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
		}
		if (shouldShowHddDevice(1)) {
			strcpy(files[nfiles].name, "hdd1:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
		}

#ifdef EXFAT
		if (allow_usb_devices && shouldShowAtaDevice(0)) {
			strcpy(files[nfiles].name, "ata0:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
		}
		if (allow_usb_devices && shouldShowAtaDevice(1)) {
			strcpy(files[nfiles].name, "ata1:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
		}
#endif

			strcpy(files[nfiles].name, "cdfs:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;

#ifdef XFROM
			if (console_is_PSX || boot_show_all_devices) {
				strcpy(files[nfiles].name, "xfrom0:");
				files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
			}
#endif

#ifdef DVRP
			if (console_is_PSX || boot_show_all_devices) {
				strcpy(files[nfiles].name, "dvr_hdd0:");
				files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
			}
#endif
			if (!cnfmode) {
				//This condition blocks selecting any CONFIG items on PC
				//or in a virtual memory card
#ifdef ETH
				strcpy(files[nfiles].name, "host:");
				files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
#endif
				if (vmcMounted[0]) {
					strcpy(files[nfiles].name, "vmc0:");
					files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
				}
				if (vmcMounted[1]) {
					strcpy(files[nfiles].name, "vmc1:");
					files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
				}
			}
#ifdef UDPFS
			strcpy(files[nfiles].name, "udpfs:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
#endif
			if (cnfmode < 2) {
				//This condition blocks use of MISC pseudo-device for driver path picks.
				//And allows this device only for launch keys and for normal browsing.
				memset(&files[nfiles], 0, sizeof(files[nfiles]));
				nfiles++;
				strcpy(files[nfiles].name, LNG(MISC));
			files[nfiles].stats.AttrFile = sceMcFileAttrSubdir;
			nfiles++;
		}
		for (i = 0; i < nfiles; i++)
			files[i].title[0] = 0;
		vfreeSpace = FALSE;
		//-- End case for browser root pseudo folder with device links --
	} else if (!strcmp(path, setting->Misc)) {
		//-- Start case for MISC command pseudo folder with function links --
		nfiles = 0;
		strcpy(files[nfiles].name, "..");
		files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
		if (cnfmode) {  //Stop recursive FileBrowser entry, only allow it for launch keys
			strcpy(files[nfiles].name, LNG(FileBrowser));
			files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		}
		strcpy(files[nfiles].name, LNG(PS2Browser));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(PS2Disc));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(PS2PowerOff));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(OSDSYS));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(HddManager));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(Exploit_Installer));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(TextEditor));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(Configure));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		//Next 2 lines add an optional font test routine
		strcpy(files[nfiles].name, LNG(ShowFont));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(Debug_Info));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(About_uLE));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(Build_Info));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(Reboot_IOP));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		for (i = 0; i < nfiles; i++)
			files[i].title[0] = 0;
		//-- End case for MISC command pseudo folder with function links --
	} else {
		//-- Start case for normal folder with file/folder links --
		strcpy(files[0].name, "..");
		files[0].stats.AttrFile = sceMcFileAttrSubdir;
		nfiles = getDir(path, &files[1]) + 1;
		if (strcmp(ext, "*")) {
			for (i = j = 1; i < nfiles; i++) {
				if (files[i].stats.AttrFile & sceMcFileAttrSubdir)
					files[j++] = files[i];
				else {
					if (genCmpFileExt(files[i].name, ext))
						files[j++] = files[i];
				}
			}
			nfiles = j;
		}
		if ((file_show == 2) || (file_sort == 2)) {
			for (i = 1; i < nfiles; i++) {
				ret = filerGetGameTitle(path, &files[i], files[i].title);
				if (ret < 0)
					files[i].title[0] = 0;
			}
		}
		if (isHddRootPath(path) || !strcmp(path, "dvr_hdd0:/"))
			vfreeSpace = FALSE;
		else if (nfiles > 1)
			sort(&files[1], 0, nfiles - 2);
		//-- End case for normal folder with file/folder links --
	}
	return nfiles;
}
//------------------------------
//endfunc setFileList
//--------------------------------------------------------------
//End of file: filer.c
//--------------------------------------------------------------
