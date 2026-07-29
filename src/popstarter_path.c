#include "popstarter_internal.h"

void initPopstarterCandidates(POPSTARTER_CANDIDATE_LIST *candidates)
{
	if (candidates != NULL)
		candidates->count = 0;
}

int addPopstarterCandidate(POPSTARTER_CANDIDATE_LIST *candidates, const char *path)
{
	if (candidates == NULL || path == NULL || path[0] == '\0' ||
	    candidates->count >= POPSTARTER_MAX_CANDIDATES)
		return 0;

	snprintf(candidates->entry[candidates->count].path, MAX_PATH, "%s", path);
	candidates->count++;
	return 1;
}

int isHddDevicePath(const char *path)
{
	return (path != NULL &&
	        !strncmp(path, "hdd", 3) &&
	        path[3] >= '0' && path[3] <= '9' &&
	        path[4] == ':' &&
	        path[5] == '/');
}

static const char *findHddPfsSeparator(const char *path)
{
	const char *p;

	if (path == NULL)
		return NULL;

	for (p = path; *p != '\0'; p++) {
		if (p[0] == ':' &&
		    p[1] != '\0' &&
		    p[2] != '\0' &&
		    p[3] != '\0' &&
		    wle_ascii_tolower((unsigned char)p[1]) == 'p' &&
		    wle_ascii_tolower((unsigned char)p[2]) == 'f' &&
		    wle_ascii_tolower((unsigned char)p[3]) == 's' &&
		    p[4] == ':')
			return p;
	}

	return NULL;
}

int splitHddPfsPath(const char *path, char *hdd_device, size_t hdd_device_size,
                    char *partition, size_t partition_size, const char **subpath)
{
	const char *partition_start;
	const char *pfs_sep;
	size_t partition_len;

	if (path == NULL || hdd_device == NULL || hdd_device_size < 6 ||
	    partition == NULL || partition_size == 0 || subpath == NULL)
		return 0;
	if (strncmp(path, "hdd", 3) || path[3] < '0' || path[3] > '9')
		return 0;

	if (path[4] != ':' || path[5] == '/')
		return 0;
	partition_start = path + 5;

	pfs_sep = findHddPfsSeparator(partition_start);
	if (pfs_sep == NULL)
		return 0;

	partition_len = pfs_sep - partition_start;
	if (partition_len == 0 || partition_len >= partition_size)
		return 0;

	snprintf(hdd_device, hdd_device_size, "hdd%c:", path[3]);
	memcpy(partition, partition_start, partition_len);
	partition[partition_len] = '\0';
	*subpath = pfs_sep + 5;
	return 1;
}

int isHddPfsPath(const char *path)
{
	char hdd_device[6];
	char partition[MAX_PART_NAME + 1];
	const char *subpath;

	return splitHddPfsPath(path, hdd_device, sizeof(hdd_device), partition, sizeof(partition), &subpath);
}

int splitPopstarterHddLaunchPath(const char *path, char *hdd_device, size_t hdd_device_size,
                                 char *partition, size_t partition_size, const char **subpath)
{
	const char *partition_start;
	size_t partition_len;

	if (path == NULL || hdd_device == NULL || hdd_device_size < 6 ||
	    partition == NULL || partition_size == 0 || subpath == NULL)
		return POPSTARTER_ERR_UNSUPPORTED_PATH;

	if (isHddDevicePath(path)) {
		memcpy(hdd_device, path, 5);
		hdd_device[5] = '\0';
		partition_start = path + 6;
		*subpath = strchr(partition_start, '/');
		if (*subpath == NULL)
			return POPSTARTER_ERR_UNSUPPORTED_PATH;

		partition_len = *subpath - partition_start;
		if (partition_len == 0 || partition_len >= partition_size)
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
		memcpy(partition, partition_start, partition_len);
		partition[partition_len] = '\0';
		return POPSTARTER_OK;
	}

	if (splitHddPfsPath(path, hdd_device, hdd_device_size, partition, partition_size, subpath))
		return POPSTARTER_OK;

	return 0;
}

int isHddPartyPath(const char *party)
{
	return (party != NULL &&
	        !strncmp(party, "hdd", 3) &&
	        party[3] >= '0' && party[3] <= '9' &&
	        party[4] == ':');
}

static int splitDevicePath(const char *path, char *device, size_t device_size, const char **suffix)
{
	const char *colon;
	size_t len;

	if (path == NULL || device == NULL || device_size == 0 || suffix == NULL)
		return 0;

	colon = strchr(path, ':');
	if (colon == NULL)
		return 0;

	len = colon - path + 1;
	if (len >= device_size)
		return 0;

	memcpy(device, path, len);
	device[len] = '\0';
	*suffix = colon + 1;
	return 1;
}

static int deviceMatchesBase(const char *device, const char *base)
{
	size_t len;

	if (device == NULL || base == NULL)
		return 0;

	len = strlen(base);
	if (strncmp(device, base, len))
		return 0;
	if (device[len] == ':')
		return 1;
	if (device[len] >= '0' && device[len] <= '9' && device[len + 1] == ':')
		return 1;
	return 0;
}

int prefixCaseCmp(const char *value, const char *prefix, size_t prefix_len)
{
	size_t i;

	for (i = 0; i < prefix_len; i++) {
		if (value[i] == '\0')
			return 1;
		if (wle_ascii_tolower((unsigned char)value[i]) != wle_ascii_tolower((unsigned char)prefix[i]))
			return 1;
	}
	return 0;
}

static int isFatStylePopstarterDevice(const char *device)
{
	if (deviceMatchesBase(device, "mass") || deviceMatchesBase(device, "usb"))
		return 1;
#ifdef MMCE
	if (deviceMatchesBase(device, "mmce"))
		return 1;
#endif
#ifdef MX4SIO
	if (deviceMatchesBase(device, "mx4sio"))
		return 1;
#endif
#ifdef EXFAT
	if (deviceMatchesBase(device, "ata"))
		return 1;
#endif
	return 0;
}

static int startsWithPopsDir(const char *suffix, const char **relative_vcd)
{
	if (suffix == NULL || relative_vcd == NULL)
		return 0;

	if (*suffix == '/')
		suffix++;
	if (prefixCaseCmp(suffix, "POPS/", 5))
		return 0;
	if (suffix[5] == '\0')
		return 0;

	*relative_vcd = suffix + 5;
	return 1;
}

static void replaceFinalExtWithElf(char *path)
{
	size_t len;

	len = strlen(path);
	if (len >= 4)
		strcpy(path + len - 4, ".elf");
}

static int buildArg(char *arg, size_t arg_size, const char *prefix, const char *name)
{
	int len;

	len = snprintf(arg, arg_size, "%s%s", prefix, name);
	if (len < 0 || len >= (int)arg_size)
		return 0;
	replaceFinalExtWithElf(arg);
	return 1;
}

static int buildHddPartitionArg(char *arg, size_t arg_size, const char *partition)
{
	int len;

	len = snprintf(arg, arg_size, "uLE:%s.elf", partition);
	return (len > 0 && len < (int)arg_size);
}

static int openHddPfsPathForRead(const char *path, char *opened_path, size_t opened_path_size)
{
	char hdd_device[6];
	char partition[MAX_PART_NAME + 1];
	char party[MAX_NAME];
	char fixed_path[MAX_PATH];
	const char *subpath;
	int pfs_ix;
	int fd;

	if (!splitHddPfsPath(path, hdd_device, sizeof(hdd_device), partition, sizeof(partition), &subpath))
		return -1;

	if (*subpath == '\0')
		subpath = "/";
	if (*subpath != '/')
		return -1;

	loadHddModules();
	snprintf(party, sizeof(party), "%s%s", hdd_device, partition);
	pfs_ix = mountParty(party);
	if (pfs_ix < 0)
		return -1;

	snprintf(fixed_path, sizeof(fixed_path), "pfs%d:%s", pfs_ix, subpath);
	fd = genOpen(fixed_path, FIO_O_RDONLY);
	if (fd >= 0 && opened_path != NULL && opened_path_size > 0)
		snprintf(opened_path, opened_path_size, "%s", fixed_path);
	return fd;
}

int openPathForRead(const char *path, char *opened_path, size_t opened_path_size)
{
	char fixed_path[MAX_PATH];
	int fd;

	if (path == NULL || path[0] == '\0')
		return -1;

	fd = openHddPfsPathForRead(path, opened_path, opened_path_size);
	if (fd >= 0)
		return fd;

	if (genFixPath(path, fixed_path) >= 0) {
		fd = genOpen(fixed_path, FIO_O_RDONLY);
		if (fd >= 0) {
			if (opened_path != NULL && opened_path_size > 0)
				snprintf(opened_path, opened_path_size, "%s", fixed_path);
			return fd;
		}
	}

	if (!strncmp(path, "mc:/", 4)) {
		snprintf(fixed_path, sizeof(fixed_path), "mc0:%s", path + 3);
		fd = genOpen(fixed_path, FIO_O_RDONLY);
		if (fd >= 0) {
			if (opened_path != NULL && opened_path_size > 0)
				snprintf(opened_path, opened_path_size, "%s", fixed_path);
			return fd;
		}

		snprintf(fixed_path, sizeof(fixed_path), "mc1:%s", path + 3);
		fd = genOpen(fixed_path, FIO_O_RDONLY);
		if (fd >= 0) {
			if (opened_path != NULL && opened_path_size > 0)
				snprintf(opened_path, opened_path_size, "%s", fixed_path);
			return fd;
		}
	}

	if (opened_path != NULL && opened_path_size > 0)
		snprintf(opened_path, opened_path_size, "%s", path);
	return -1;
}

static int prepareHddLaunch(const char *path, char *arg, size_t arg_size, POPSTARTER_CANDIDATE_LIST *fallbacks)
{
	char hdd_device[6];
	char partition[MAX_PART_NAME + 1];
	char candidate[MAX_PATH];
	const char *subpath;
	const char *name;
	int is_pops_partition;
	int ret;

	ret = splitPopstarterHddLaunchPath(path, hdd_device, sizeof(hdd_device), partition, sizeof(partition), &subpath);
	if (ret <= 0)
		return ret;

	if (subpath == NULL || subpath[0] == '\0' || subpath[0] != '/')
		return POPSTARTER_ERR_UNSUPPORTED_PATH;

	is_pops_partition = !strncmp(partition, "__.POPS", 7);
	if (is_pops_partition) {
		name = strrchr(subpath, '/');
		if (name == NULL || !buildArg(arg, arg_size, "uLE:", name))
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
	} else {
		if (strncmp(partition, "__.", 3) && strncmp(partition, "PP.", 3))
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
		if (stricmp(subpath, "/IMAGE0.VCD"))
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
		if (!buildHddPartitionArg(arg, arg_size, partition))
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
	}

	snprintf(candidate, sizeof(candidate), "%s__common:pfs:/POPS/POPSTARTER.ELF", hdd_device);
	addPopstarterCandidate(fallbacks, candidate);
	snprintf(candidate, sizeof(candidate), "%s__system:pfs:/launcher/POPSTARTER.ELF", hdd_device);
	addPopstarterCandidate(fallbacks, candidate);
	return POPSTARTER_OK;
}

static int prepareFatStyleLaunch(const char *path, char *arg, size_t arg_size, POPSTARTER_CANDIDATE_LIST *fallbacks)
{
	char device[16];
	char candidate[MAX_PATH];
	const char *suffix;
	const char *relative_vcd;

	if (!splitDevicePath(path, device, sizeof(device), &suffix))
		return 0;
	if (!isFatStylePopstarterDevice(device))
		return 0;
	if (!startsWithPopsDir(suffix, &relative_vcd))
		return POPSTARTER_ERR_UNSUPPORTED_PATH;
	if (!buildArg(arg, arg_size, "uLE:XX.", relative_vcd))
		return POPSTARTER_ERR_UNSUPPORTED_PATH;

	snprintf(candidate, sizeof(candidate), "%s/POPS/POPSTARTER.ELF", device);
	addPopstarterCandidate(fallbacks, candidate);
	return POPSTARTER_OK;
}

int prepareLaunch(const char *path, char *arg, size_t arg_size, POPSTARTER_CANDIDATE_LIST *candidates)
{
	int ret;

	if (!IsPopstarterVcdPath(path))
		return POPSTARTER_ERR_NOT_VCD;

	initPopstarterCandidates(candidates);
	if (setting != NULL && setting->popstarter_file[0] != '\0')
		addPopstarterCandidate(candidates, setting->popstarter_file);

	ret = prepareHddLaunch(path, arg, arg_size, candidates);
	if (ret == 0)
		ret = prepareFatStyleLaunch(path, arg, arg_size, candidates);
	if (ret <= 0)
		return (ret == 0) ? POPSTARTER_ERR_UNSUPPORTED_PATH : ret;

	return POPSTARTER_OK;
}
