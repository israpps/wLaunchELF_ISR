#include "launchelf.h"
#include "hdd_header_injector.h"

#include <hdd-ioctl.h>

#define APA_HEADER_SECTORS 4096
#define APA_HEADER_SIZE (APA_HEADER_SECTORS * 512)
#define APA_HEADER_MAGIC "PS2ICON3D"
#define HDD_HEADER_SOURCE_WAIT_MS 3000
#define HDD_HEADER_SOURCE_POLL_MS 500
#define HDD_HEADER_COPY_BUFFER_SIZE 32768
#define HDD_HEADER_PFS_DIR_MODE (FIO_S_IRUSR | FIO_S_IWUSR | FIO_S_IXUSR | \
                                 FIO_S_IRGRP | FIO_S_IWGRP | FIO_S_IXGRP | \
                                 FIO_S_IROTH | FIO_S_IWOTH | FIO_S_IXOTH)

enum HddHeaderFileKind {
	HDD_HEADER_SYSTEM_CNF = 0,
	HDD_HEADER_ICON_SYS,
	HDD_HEADER_LIST_ICO,
	HDD_HEADER_BOOT_KELF,

	HDD_HEADER_FILE_COUNT
};

typedef struct
{
	const char *name;
	u32 data_offset;
	u32 offset_offset;
	u32 size_offset;
	u32 max_size;
	int required;
} HddHeaderFileSpec;

typedef struct
{
	const char *source_name;
	const char *alternate_source_name;
	const char *dest_path;
	int needs_res_dir;
} HddHeaderPfsFileSpec;

static const HddHeaderFileSpec hdd_header_files[HDD_HEADER_FILE_COUNT] = {
    {"system.cnf", 0x1200, 0x1010, 0x1014, 0x1400 - 0x1200, 1},
    {"icon.sys", 0x1400, 0x1018, 0x101C, 0x1800 - 0x1400, 0},
    {"list.ico", 0x1800, 0x1020, 0x1024, 0x111000 - 0x1800, 0},
    {"boot.kelf", 0x111000, 0x1030, 0x1034, APA_HEADER_SIZE - 0x111000, 0},
};

static const HddHeaderPfsFileSpec hdd_header_pfs_files[] = {
    {"info.sys", NULL, "pfs0:/res/info.sys", 1},
    {"jkt_001.png", NULL, "pfs0:/res/jkt_001.png", 1},
    {"jkt_002.png", NULL, "pfs0:/res/jkt_002.png", 1},
    {"jkt_cp.png", NULL, "pfs0:/res/jkt_cp.png", 1},
    {"BOOT.ELF", "boot.elf", "pfs0:/BOOT.ELF", 0},
};

static unsigned char hdd_header_io_buffer[512 + sizeof(hddAtaTransfer_t)] __attribute__((aligned(64)));
static unsigned char hdd_header_buffer[APA_HEADER_SIZE] __attribute__((aligned(64)));
static unsigned char hdd_header_copy_buffer[HDD_HEADER_COPY_BUFFER_SIZE] __attribute__((aligned(64)));

static void hddHeaderDelay(int ms)
{
	u64 end_time;

	end_time = Timer() + ms;
	while (Timer() < end_time) {
	}
}

static int hddHeaderJoinPath(char *out, size_t out_size, const char *dir, const char *file)
{
	const char *separator;
	size_t dir_len;
	int written;

	if (out == NULL || out_size == 0 || dir == NULL || file == NULL)
		return -EINVAL;

	dir_len = strlen(dir);
	separator = (dir_len > 0 && dir[dir_len - 1] == '/') ? "" : "/";
	written = snprintf(out, out_size, "%s%s%s", dir, separator, file);

	return (written >= 0 && written < (int)out_size) ? 0 : -ENAMETOOLONG;
}

static int hddHeaderGetFileSize(const char *path, u32 *size)
{
	char fixed_path[MAX_PATH];
	s64 file_size;
	int fd;
	int ret;

	ret = genFixPath(path, fixed_path);
	if (ret < 0)
		return ret;

	fd = genOpen(fixed_path, FIO_O_RDONLY);
	if (fd < 0)
		return fd;

	file_size = genLseek(fd, 0, SEEK_END);
	genClose(fd);
	if (file_size < 0)
		return (int)file_size;
	if (file_size > 0xFFFFFFFFLL)
		return -EFBIG;

	if (size != NULL)
		*size = (u32)file_size;
	return 0;
}

static int hddHeaderSourceHasFile(const char *dir, const HddHeaderFileSpec *spec)
{
	char path[MAX_PATH];
	u32 size;
	int ret;

	ret = hddHeaderJoinPath(path, sizeof(path), dir, spec->name);
	if (ret < 0)
		return ret;

	ret = hddHeaderGetFileSize(path, &size);
	if (ret < 0 && !strcmp(spec->name, "boot.kelf")) {
		ret = hddHeaderJoinPath(path, sizeof(path), dir, "BOOT.KELF");
		if (ret < 0)
			return ret;
		ret = hddHeaderGetFileSize(path, &size);
	}
	if (ret < 0)
		return ret;
	if (size > spec->max_size)
		return -EFBIG;

	return 0;
}

static int hddHeaderFileRequired(const HddHeaderFileSpec *spec)
{
	if (spec->required)
		return 1;
	if (!console_is_PSX && (!strcmp(spec->name, "icon.sys") || !strcmp(spec->name, "list.ico")))
		return 1;

	return 0;
}

static int hddHeaderSourceReady(const char *dir)
{
	int i;
	int ret;

	for (i = 0; i < HDD_HEADER_FILE_COUNT; i++) {
		if (!hddHeaderFileRequired(&hdd_header_files[i]))
			continue;

		ret = hddHeaderSourceHasFile(dir, &hdd_header_files[i]);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int hddHeaderBuildCandidateDir(char *out, size_t out_size, const char *source_device, const char *partition_name)
{
	int written;

	if (out == NULL || out_size == 0 || source_device == NULL || partition_name == NULL)
		return -EINVAL;

	written = snprintf(out, out_size, "%s/__Headers/%s/", source_device, partition_name);

	return (written >= 0 && written < (int)out_size) ? 0 : -ENAMETOOLONG;
}

static int hddHeaderBuildSourceRootDir(char *out, size_t out_size, const char *source_device)
{
	int written;

	if (out == NULL || out_size == 0 || source_device == NULL)
		return -EINVAL;

	written = snprintf(out, out_size, "%s/__Headers/", source_device);

	return (written >= 0 && written < (int)out_size) ? 0 : -ENAMETOOLONG;
}

static int hddHeaderPrepareSourceDevice(const char *source_device)
{
	if (source_device == NULL)
		return -EINVAL;

	if (!strncmp(source_device, "usb", 3)) {
		prepareUsbRootBrowse();
		return 0;
	}

#ifdef MMCE
	if (!strncmp(source_device, "mmce", 4))
		return loadMmceModules() ? 0 : -ENODEV;
#else
	if (!strncmp(source_device, "mmce", 4))
		return -ENODEV;
#endif

#ifdef UDPFS
	if (!strncmp(source_device, "udpfs", 5))
		return load_udpfs() ? 0 : -ENODEV;
#else
	if (!strncmp(source_device, "udpfs", 5))
		return -ENODEV;
#endif

	return 0;
}

static int hddHeaderFindSourceDir(const char *source_device, const char *partition_name, char *source_dir, size_t source_dir_size)
{
	char candidate[MAX_PATH];
	int ret;

	ret = hddHeaderBuildCandidateDir(candidate, sizeof(candidate), source_device, partition_name);
	if (ret < 0)
		return ret;

	ret = hddHeaderSourceReady(candidate);
	if (ret >= 0) {
		if (source_dir != NULL && source_dir_size > 0) {
			snprintf(source_dir, source_dir_size, "%s", candidate);
			source_dir[source_dir_size - 1] = '\0';
		}
		return 0;
	}

	return ret;
}

static int hddHeaderSourceRootReady(const char *source_device)
{
	char source_root[MAX_PATH];
	int fd;
	int ret;

	ret = hddHeaderBuildSourceRootDir(source_root, sizeof(source_root), source_device);
	if (ret < 0)
		return ret;

	fd = genDopen(source_root);
	if (fd < 0)
		return fd;

	genDclose(fd);
	return 0;
}

static int hddHeaderWaitForSourceRoot(const char *source_device)
{
	char msg[MAX_TEXT_LINE];
	u64 start_time;
	int ret = -ENOENT;

	start_time = Timer();
	while (Timer() < start_time + HDD_HEADER_SOURCE_WAIT_MS) {
		ret = hddHeaderPrepareSourceDevice(source_device);
		if (ret >= 0)
			ret = hddHeaderSourceRootReady(source_device);
		if (ret >= 0)
			return 0;

		snprintf(msg, sizeof(msg), "Waiting for header folders on %s", source_device);
		drawMsg(msg);
		hddHeaderDelay(HDD_HEADER_SOURCE_POLL_MS);
	}

	return ret;
}

static int hddHeaderSourceEntryIsDir(const char *source_root, const char *name, int mode)
{
	char path[MAX_PATH];
	iox_stat_t stat;
	int ret;

	if (FIO_S_ISDIR(mode))
		return 1;

	ret = hddHeaderJoinPath(path, sizeof(path), source_root, name);
	if (ret < 0)
		return 0;
	ret = genGetStat(path, &stat);
	if (ret < 0)
		return 0;

	return FIO_S_ISDIR(stat.mode);
}

int HddHeaderListSourcePartitions(const char *source_device, HddHeaderSourcePartition *partitions, int max_partitions, int *invalid_count)
{
	char source_root[MAX_PATH];
	iox_dirent_t dirent;
	size_t name_len;
	int invalid = 0;
	int count = 0;
	int fd;
	int ret;

	if (source_device == NULL || source_device[0] == '\0' || partitions == NULL || max_partitions <= 0)
		return -EINVAL;
	if (invalid_count != NULL)
		*invalid_count = 0;

	ret = hddHeaderWaitForSourceRoot(source_device);
	if (ret < 0)
		return ret;

	ret = hddHeaderBuildSourceRootDir(source_root, sizeof(source_root), source_device);
	if (ret < 0)
		return ret;

	fd = genDopen(source_root);
	if (fd < 0)
		return fd;

	while (fileXioDread(fd, &dirent) > 0) {
		if (!strcmp(dirent.name, ".") || !strcmp(dirent.name, ".."))
			continue;
		if (!hddHeaderSourceEntryIsDir(source_root, dirent.name, dirent.stat.mode))
			continue;

		name_len = strlen(dirent.name);
		if (name_len == 0 || name_len > HDD_HEADER_SOURCE_PARTITION_NAME_MAX) {
			invalid++;
			continue;
		}

		ret = hddHeaderFindSourceDir(source_device, dirent.name, NULL, 0);
		if (ret < 0) {
			invalid++;
			continue;
		}

		if (count >= max_partitions) {
			genDclose(fd);
			return -ENOMEM;
		}

		snprintf(partitions[count].name, sizeof(partitions[count].name), "%s", dirent.name);
		partitions[count].name[sizeof(partitions[count].name) - 1] = '\0';
		count++;
	}

	genDclose(fd);

	if (invalid_count != NULL)
		*invalid_count = invalid;

	return count;
}

static int hddHeaderWaitForSource(const char *source_device, const char *partition_name, char *source_dir, size_t source_dir_size)
{
	char msg[MAX_TEXT_LINE];
	u64 start_time;
	int ret = -ENOENT;

	start_time = Timer();
	while (Timer() < start_time + HDD_HEADER_SOURCE_WAIT_MS) {
		ret = hddHeaderPrepareSourceDevice(source_device);
		if (ret >= 0)
			ret = hddHeaderFindSourceDir(source_device, partition_name, source_dir, source_dir_size);
		if (ret >= 0)
			return 0;

		snprintf(msg, sizeof(msg), "Waiting for header files on %s %s", source_device, partition_name);
		drawMsg(msg);
		hddHeaderDelay(HDD_HEADER_SOURCE_POLL_MS);
	}

	return ret;
}

static int hddHeaderReadSectors(u32 partition_sector)
{
	hddAtaTransfer_t *transfer = (hddAtaTransfer_t *)hdd_header_io_buffer;
	int i;
	int ret = 0;

	for (i = 0; i < APA_HEADER_SECTORS; i++) {
		transfer->lba = partition_sector + i;
		transfer->size = 1;
		ret = fileXioDevctl("hdd0:", HDIOC_READSECTOR, hdd_header_io_buffer,
		                    sizeof(hddAtaTransfer_t), hdd_header_buffer + 512 * i, 512);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int hddHeaderWriteSectors(u32 partition_sector)
{
	hddAtaTransfer_t *transfer = (hddAtaTransfer_t *)hdd_header_io_buffer;
	int i;
	int ret = 0;

	for (i = 0; i < APA_HEADER_SECTORS; i++) {
		transfer->lba = partition_sector + i;
		transfer->size = 1;
		memcpy(transfer->data, hdd_header_buffer + 512 * i, 512);
		ret = fileXioDevctl("hdd0:", HDIOC_WRITESECTOR, hdd_header_io_buffer,
		                    512 + sizeof(hddAtaTransfer_t), NULL, 0);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int hddHeaderReadFileIntoHeader(const char *source_dir, const HddHeaderFileSpec *spec)
{
	char path[MAX_PATH];
	char fixed_path[MAX_PATH];
	u32 file_size;
	u32 data_offset;
	int fd;
	int ret;

	ret = hddHeaderJoinPath(path, sizeof(path), source_dir, spec->name);
	if (ret < 0)
		return ret;

	ret = hddHeaderGetFileSize(path, &file_size);
	if (ret < 0 && !strcmp(spec->name, "boot.kelf")) {
		ret = hddHeaderJoinPath(path, sizeof(path), source_dir, "BOOT.KELF");
		if (ret < 0)
			return ret;
		ret = hddHeaderGetFileSize(path, &file_size);
	}
	if (ret < 0)
		return hddHeaderFileRequired(spec) ? ret : 0;
	if (file_size > spec->max_size)
		return -EFBIG;

	ret = genFixPath(path, fixed_path);
	if (ret < 0)
		return ret;

	fd = genOpen(fixed_path, FIO_O_RDONLY);
	if (fd < 0 && !strcmp(spec->name, "boot.kelf")) {
		hddHeaderJoinPath(path, sizeof(path), source_dir, "BOOT.KELF");
		ret = genFixPath(path, fixed_path);
		if (ret < 0)
			return ret;
		fd = genOpen(fixed_path, FIO_O_RDONLY);
	}
	if (fd < 0)
		return fd;

	if (genRead(fd, hdd_header_buffer + spec->data_offset, file_size) != (int)file_size) {
		genClose(fd);
		return -EIO;
	}
	genClose(fd);

	data_offset = spec->data_offset - 0x1000;
	memcpy(hdd_header_buffer + spec->offset_offset, &data_offset, sizeof(data_offset));
	memcpy(hdd_header_buffer + spec->size_offset, &file_size, sizeof(file_size));

	if (!strcmp(spec->name, "list.ico")) {
		memcpy(hdd_header_buffer + 0x1028, &data_offset, sizeof(data_offset));
		memcpy(hdd_header_buffer + 0x102C, &file_size, sizeof(file_size));
	}

	return 0;
}

static int hddHeaderPatchFromSource(const char *source_dir)
{
	int i;
	int ret;

	memcpy(hdd_header_buffer + 0x1000, APA_HEADER_MAGIC, strlen(APA_HEADER_MAGIC));
	for (i = 0; i < HDD_HEADER_FILE_COUNT; i++) {
		ret = hddHeaderReadFileIntoHeader(source_dir, &hdd_header_files[i]);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int hddHeaderOpenOptionalSourceFile(const char *source_dir, const char *source_name, const char *alternate_source_name, int *fd_out)
{
	const char *names[2];
	char path[MAX_PATH];
	char fixed_path[MAX_PATH];
	int i;
	int fd;
	int ret;

	if (fd_out == NULL)
		return -EINVAL;

	*fd_out = -1;
	names[0] = source_name;
	names[1] = alternate_source_name;

	for (i = 0; i < 2; i++) {
		if (names[i] == NULL)
			continue;

		ret = hddHeaderJoinPath(path, sizeof(path), source_dir, names[i]);
		if (ret < 0)
			return ret;

		ret = genFixPath(path, fixed_path);
		if (ret < 0)
			return ret;

		fd = genOpen(fixed_path, FIO_O_RDONLY);
		if (fd >= 0) {
			*fd_out = fd;
			return 1;
		}
	}

	return 0;
}

static int hddHeaderHasOptionalPfsFiles(const char *source_dir)
{
	unsigned int i;
	int fd;
	int ret;

	for (i = 0; i < sizeof(hdd_header_pfs_files) / sizeof(hdd_header_pfs_files[0]); i++) {
		ret = hddHeaderOpenOptionalSourceFile(source_dir, hdd_header_pfs_files[i].source_name,
		                                      hdd_header_pfs_files[i].alternate_source_name, &fd);
		if (ret < 0)
			return ret;
		if (ret > 0) {
			genClose(fd);
			return 1;
		}
	}

	return 0;
}

static int hddHeaderEnsurePfsResDir(void)
{
	iox_stat_t stat;
	int ret;

	ret = fileXioMkdir("pfs0:/res", HDD_HEADER_PFS_DIR_MODE);
	if (ret >= 0)
		return 0;

	ret = fileXioGetStat("pfs0:/res", &stat);
	if (ret < 0)
		return ret;
	if (!FIO_S_ISDIR(stat.mode))
		return -EINVAL;

	return 0;
}

static int hddHeaderCopyOptionalPfsFile(const char *source_dir, const HddHeaderPfsFileSpec *spec)
{
	int in_fd = -1;
	int out_fd = -1;
	int ret;

	ret = hddHeaderOpenOptionalSourceFile(source_dir, spec->source_name, spec->alternate_source_name, &in_fd);
	if (ret <= 0)
		return ret;

	if (spec->needs_res_dir) {
		ret = hddHeaderEnsurePfsResDir();
		if (ret < 0)
			goto done;
	}

	out_fd = genOpen(spec->dest_path, FIO_O_WRONLY | FIO_O_TRUNC | FIO_O_CREAT);
	if (out_fd < 0) {
		ret = out_fd;
		goto done;
	}

	while (1) {
		int bytes_read = genRead(in_fd, hdd_header_copy_buffer, sizeof(hdd_header_copy_buffer));
		if (bytes_read < 0) {
			ret = bytes_read;
			break;
		}
		if (bytes_read == 0) {
			ret = 1;
			break;
		}
		if (genWrite(out_fd, hdd_header_copy_buffer, bytes_read) != bytes_read) {
			ret = -EIO;
			break;
		}
	}

done:
	if (out_fd >= 0)
		genClose(out_fd);
	if (in_fd >= 0)
		genClose(in_fd);

	return ret;
}

static int hddHeaderCopyOptionalPfsFiles(const char *partition_path, const char *source_dir, int *copied_count)
{
	unsigned int i;
	int copied = 0;
	int ret;

	if (copied_count != NULL)
		*copied_count = 0;

	ret = hddHeaderHasOptionalPfsFiles(source_dir);
	if (ret <= 0)
		return ret;

	fileXioUmount("pfs0:");
	ret = fileXioMount("pfs0:", partition_path, FIO_MT_RDWR);
	if (ret < 0)
		return ret;

	for (i = 0; i < sizeof(hdd_header_pfs_files) / sizeof(hdd_header_pfs_files[0]); i++) {
		ret = hddHeaderCopyOptionalPfsFile(source_dir, &hdd_header_pfs_files[i]);
		if (ret < 0)
			break;
		if (ret > 0)
			copied++;
	}

	fileXioUmount("pfs0:");

	if (copied_count != NULL)
		*copied_count = copied;

	return (ret < 0) ? ret : 0;
}

int InjectHddPartitionHeaderFromSource(const char *partition_name, const char *source_device, char *source_dir, size_t source_dir_size,
                                       int *pfs_files_copied, int *pfs_copy_error)
{
	char partition_path[MAX_NAME + 6];
	char resolved_source_dir[MAX_PATH];
	iox_stat_t stat;
	u32 partition_sector;
	int ret;

	if (partition_name == NULL || partition_name[0] == '\0')
		return -EINVAL;
	if (source_device == NULL || source_device[0] == '\0')
		return -EINVAL;
	if (pfs_files_copied != NULL)
		*pfs_files_copied = 0;
	if (pfs_copy_error != NULL)
		*pfs_copy_error = 0;

	ret = hddHeaderWaitForSource(source_device, partition_name, resolved_source_dir, sizeof(resolved_source_dir));
	if (ret < 0)
		return ret;
	if (source_dir != NULL && source_dir_size > 0) {
		snprintf(source_dir, source_dir_size, "%s", resolved_source_dir);
		source_dir[source_dir_size - 1] = '\0';
	}

	ret = snprintf(partition_path, sizeof(partition_path), "hdd0:%s", partition_name);
	if (ret < 0 || ret >= (int)sizeof(partition_path))
		return -ENAMETOOLONG;

	ret = fileXioDevctl("hdd0:", HDIOC_STATUS, NULL, 0, NULL, 0);
	if (ret != 0)
		return -EIO;

	ret = fileXioGetStat(partition_path, &stat);
	if (ret < 0)
		return ret;
	partition_sector = stat.private_5;

	ret = hddHeaderReadSectors(partition_sector);
	if (ret < 0)
		return ret;

	ret = hddHeaderPatchFromSource(resolved_source_dir);
	if (ret < 0)
		return ret;

	ret = hddHeaderWriteSectors(partition_sector);
	if (ret < 0)
		return ret;

	ret = hddHeaderCopyOptionalPfsFiles(partition_path, resolved_source_dir, pfs_files_copied);
	if (ret < 0 && pfs_copy_error != NULL)
		*pfs_copy_error = ret;

	return 0;
}
