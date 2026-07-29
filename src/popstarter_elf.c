#include "popstarter_internal.h"

static int checkExecutablePath(const char *path, int *exec_kind)
{
	char tmp[MAX_PATH];
	int kind;

	if (path == NULL || path[0] == '\0' || exec_kind == NULL)
		return 0;

	snprintf(tmp, sizeof(tmp), "%s", path);
	kind = checkELFheader(tmp);
	if (kind <= 0)
		return 0;

	*exec_kind = kind;
	return 1;
}

static int prepareHddPfsElfLaunch(const char *path, char *fullpath, size_t fullpath_size,
                                  char *party, size_t party_size, int *exec_kind)
{
	char hdd_device[6];
	char partition[MAX_PART_NAME + 1];
	char browser_path[MAX_PATH];
	const char *subpath;

	if (!splitHddPfsPath(path, hdd_device, sizeof(hdd_device), partition, sizeof(partition), &subpath))
		return 0;
	if (subpath == NULL || subpath[0] == '\0')
		subpath = "/";
	if (subpath[0] != '/')
		return POPSTARTER_ERR_UNSUPPORTED_PATH;

	snprintf(browser_path, sizeof(browser_path), "%s/%s%s", hdd_device, partition, subpath);
	if (!checkExecutablePath(browser_path, exec_kind))
		return POPSTARTER_ERR_INVALID_POPSTARTER;

	snprintf(party, party_size, "%s%s", hdd_device, partition);
	snprintf(fullpath, fullpath_size, "pfs0:%s", subpath);
	return POPSTARTER_OK;
}

static int preparePopstarterElfLaunch(const char *path, char *fullpath, size_t fullpath_size, char *party, size_t party_size, int *exec_kind)
{
	char tmp[MAX_PATH];
	char *p;
	int fd;
	int ret;

	if (path == NULL || path[0] == '\0' || fullpath == NULL || fullpath_size == 0 ||
	    party == NULL || party_size == 0 || exec_kind == NULL)
		return POPSTARTER_ERR_OPEN_POPSTARTER;

	fullpath[0] = '\0';
	party[0] = '\0';

	fd = openPathForRead(path, NULL, 0);
	if (fd < 0)
		return POPSTARTER_ERR_OPEN_POPSTARTER;
	genClose(fd);

	ret = prepareHddPfsElfLaunch(path, fullpath, fullpath_size, party, party_size, exec_kind);
	if (ret != 0)
		return ret;

	if (!strncmp(path, "mc:/", 4)) {
		snprintf(tmp, sizeof(tmp), "mc0:%s", path + 3);
		if (checkExecutablePath(tmp, exec_kind)) {
			snprintf(fullpath, fullpath_size, "%s", tmp);
			return POPSTARTER_OK;
		}

		snprintf(tmp, sizeof(tmp), "mc1:%s", path + 3);
		if (checkExecutablePath(tmp, exec_kind)) {
			snprintf(fullpath, fullpath_size, "%s", tmp);
			return POPSTARTER_OK;
		}
		return POPSTARTER_ERR_INVALID_POPSTARTER;
	}

	if (isHddDevicePath(path)) {
		loadHddModules();
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(party, party_size, "hdd%c:%s", path[3], path + 6);
		p = strchr(party, '/');
		if (p == NULL)
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
		snprintf(fullpath, fullpath_size, "pfs0:%s", p);
		*p = '\0';
		return POPSTARTER_OK;
	}

#ifdef MX4SIO
	if (!strncmp(path, "mx4sio", 6)) {
		if (!mx4sio_driver_running && !loadMx4sioModules())
			return POPSTARTER_ERR_OPEN_POPSTARTER;
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(fullpath, fullpath_size, "%s", path);
		return POPSTARTER_OK;
	}
#endif

#ifdef MMCE
	if (!strncmp(path, "mmce", 4)) {
		loadMmceModules();
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(fullpath, fullpath_size, "%s", path);
		return POPSTARTER_OK;
	}
#endif

	if (!strncmp(path, "usb", 3)) {
		char *pathSep;

		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		if (genFixPath(path, fullpath) < 0)
			return POPSTARTER_ERR_OPEN_POPSTARTER;
		pathSep = strchr(fullpath, '/');
		if (pathSep && (pathSep - fullpath < 7) && pathSep[-1] == ':')
			strcpy(fullpath + (pathSep - fullpath), pathSep + 1);
		return POPSTARTER_OK;
	}

	if (!strncmp(path, "ata", 3)) {
		char *pathSep;

#ifdef EXFAT
		loadAtaModules();
#endif
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		if (!strncmp(path, "ata:", 4))
			snprintf(fullpath, fullpath_size, "ata0:%s", path + 4);
		else
			snprintf(fullpath, fullpath_size, "%s", path);
		pathSep = strchr(fullpath, '/');
		if (pathSep && (pathSep - fullpath < 7) && pathSep[-1] == ':')
			strcpy(fullpath + (pathSep - fullpath), pathSep + 1);
		return POPSTARTER_OK;
	}

	if (!strncmp(path, "mass", 4)) {
		char *pathSep;

		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(fullpath, fullpath_size, "%s", path);
		pathSep = strchr(path, '/');
		if (pathSep && (pathSep - path < 7) && pathSep[-1] == ':')
			strcpy(fullpath + (pathSep - path), pathSep + 1);
		return POPSTARTER_OK;
	}

#if defined(ETH) || defined(UDPFS)
	if (!strncmp(path, "host:", 5)) {
#ifdef ETH
		initHOST();
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(fullpath, fullpath_size, "host:%s", (path[5] == '/') ? path + 6 : path + 5);
		makeHostPath(fullpath, fullpath);
		return POPSTARTER_OK;
#else
		return POPSTARTER_ERR_UNSUPPORTED_PATH;
#endif
	}

	if (!strncmp(path, "udpfs:", 6)) {
#ifdef UDPFS
		load_udpfs();
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(fullpath, fullpath_size, "%s", path);
		return POPSTARTER_OK;
#else
		return POPSTARTER_ERR_UNSUPPORTED_PATH;
#endif
	}
#endif

	if (!checkExecutablePath(path, exec_kind))
		return POPSTARTER_ERR_INVALID_POPSTARTER;

	snprintf(fullpath, fullpath_size, "%s", path);
	return POPSTARTER_OK;
}

int preparePopstarterCandidateLaunch(const POPSTARTER_CANDIDATE_LIST *candidates,
                                     char *selected_path, size_t selected_path_size,
                                     char *fullpath, size_t fullpath_size,
                                     char *party, size_t party_size, int *exec_kind)
{
	int i;
	int ret;

	if (candidates == NULL || candidates->count <= 0)
		return POPSTARTER_ERR_OPEN_POPSTARTER;

	ret = POPSTARTER_ERR_OPEN_POPSTARTER;
	for (i = 0; i < candidates->count; i++) {
		if (selected_path != NULL && selected_path_size > 0)
			snprintf(selected_path, selected_path_size, "%s", candidates->entry[i].path);

		ret = preparePopstarterElfLaunch(candidates->entry[i].path, fullpath, fullpath_size, party, party_size, exec_kind);
		if (ret == POPSTARTER_OK)
			return POPSTARTER_OK;
		if (ret != POPSTARTER_ERR_OPEN_POPSTARTER)
			return ret;
	}

	return ret;
}
