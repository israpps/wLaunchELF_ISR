//--------------------------------------------------------------
//File name:   filer_device.c
//--------------------------------------------------------------
#include "filer_internal.h"

enum {
	TRANSFER_STORAGE_DEFAULT = 0,
	TRANSFER_STORAGE_MMCE,
	TRANSFER_STORAGE_MX4SIO
};

#if defined(ETH) || defined(UDPFS)
extern int host_error;
#endif

static int transferStorageGateForPath(const char *path)
{
	if (path == NULL)
		return TRANSFER_STORAGE_DEFAULT;
#ifdef MMCE
	if (!strncmp(path, "mmce", 4))
		return TRANSFER_STORAGE_MMCE;
#endif
#ifdef MX4SIO
	if (!strncmp(path, "mx4sio", 6))
		return TRANSFER_STORAGE_MX4SIO;
#endif
	return TRANSFER_STORAGE_DEFAULT;
}

static int isMx4sioPath(const char *path)
{
#ifdef MX4SIO
	return (path != NULL && !strncmp(path, "mx4sio", 6));
#else
	(void)path;
	return FALSE;
#endif
}

static int isMc1Path(const char *path)
{
	return (path != NULL && !strncmp(path, "mc1:", 4));
}

static int transferUsesMx4sioAndMc1(const char *src_path, const char *dst_path)
{
	return ((isMx4sioPath(src_path) && isMc1Path(dst_path)) ||
	        (isMc1Path(src_path) && isMx4sioPath(dst_path)));
}

int ensurePathDeviceStackReady(const char *path)
{
	if (path == NULL || path[0] == '\0')
		return TRUE;
	if (!strncmp(path, "mc", 2) || !strncmp(path, "vmc", 3))
		return TRUE;
#ifdef XFROM
	if (!strncmp(path, "xfrom", 5))
		return loadFlashModules();
#endif
	if (!strncmp(path, "mass", 4) || !strncmp(path, "usb", 3)) {
		loadUsbModules();
		return (USB_mass_loaded != 0);
	}
#ifdef MMCE
	if (!strncmp(path, "mmce", 4)) {
		return loadMmceModules();
	}
#endif
#ifdef MX4SIO
	if (!strncmp(path, "mx4sio", 6))
		return loadMx4sioModules();
#endif
#ifdef EXFAT
	if (!strncmp(path, "ata", 3)) {
		return loadAtaModules();
	}
#endif
	if (!strncmp(path, "hdd", 3) || !strncmp(path, "pfs", 3)) {
		return loadHddModules();
	}
#ifdef DVRP
	if (!strncmp(path, "dvr_hdd", 7) || !strncmp(path, "dvr_pfs", 7)) {
		return loadDVRPHddModules();
	}
#endif
	if (!strncmp(path, "cdfs", 4)) {
		loadCdModules();
		return TRUE;
	}
#ifdef ETH
	if (!strncmp(path, "host", 4)) {
		initHOST();
		return !host_error;
	}
#endif
#ifdef UDPFS
	if (!strncmp(path, "udpfs", 5)) {
		return load_udpfs();
	}
#endif
	return TRUE;
}
int prepareTransferDeviceStacks(const char *src_path, const char *dst_path)
{
	int src_storage = transferStorageGateForPath(src_path);
	int dst_storage = transferStorageGateForPath(dst_path);

	if (transferUsesMx4sioAndMc1(src_path, dst_path))
		return TRANSFER_STACK_INCOMPATIBLE;

	if (src_storage != TRANSFER_STORAGE_DEFAULT &&
	    dst_storage != TRANSFER_STORAGE_DEFAULT &&
	    src_storage != dst_storage)
		return TRANSFER_STACK_INCOMPATIBLE;

	if (!ensurePathDeviceStackReady(src_path))
		return TRANSFER_STACK_LOAD_FAILED;
	if (!ensurePathDeviceStackReady(dst_path))
		return TRANSFER_STACK_LOAD_FAILED;
	return TRANSFER_STACK_READY;
}
//--------------------------------------------------------------
//End of file: filer_device.c
//--------------------------------------------------------------
