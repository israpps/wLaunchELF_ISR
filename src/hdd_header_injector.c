#include "launchelf.h"
#include "config_private.h"
#include "hdd_header_injector.h"

#include <hdd-ioctl.h>

#define APA_HEADER_SECTORS 4096
#define APA_HEADER_SIZE (APA_HEADER_SECTORS * 512)
#define APA_HEADER_SECTOR_SIZE 512
#define APA_HEADER_ATTRIBUTE_AREA_OFFSET 0x1000
#define APA_HEADER_ATTRIBUTE_AREA_SIZE (APA_HEADER_SIZE - APA_HEADER_ATTRIBUTE_AREA_OFFSET)
#define APA_HEADER_MAGIC "PS2ICON3D"
#define APA_HEADER_SYSTEM_CNF_MAX_SIZE (0x1400 - 0x1200)
#define APA_HEADER_SYSTEM_CNF_TEMP_FILE_FORMAT "WLEHC%02d.TMP"
#define APA_HEADER_SYSTEM_CNF_TEMP_FILE_TRIES 100
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

typedef struct
{
	u32 offset;
	u32 size;
} HddHeaderAttributeFile;

typedef struct
{
	char magic[9];
	unsigned char unused[3];
	u32 version;
	HddHeaderAttributeFile system_cnf;
	HddHeaderAttributeFile icon_sys;
	HddHeaderAttributeFile list_icon;
	HddHeaderAttributeFile delete_icon;
	HddHeaderAttributeFile elf;
	HddHeaderAttributeFile ioprp;
} HddHeaderAttributeAreaHeader;

static const HddHeaderFileSpec hdd_header_files[HDD_HEADER_FILE_COUNT] = {
    {"system.cnf", 0x1200, 0x1010, 0x1014, APA_HEADER_SYSTEM_CNF_MAX_SIZE, 1},
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

static int hddHeaderBuildPartitionPath(char *out, size_t out_size, const char *partition_name)
{
	int written;

	if (out == NULL || out_size == 0 || partition_name == NULL || partition_name[0] == '\0')
		return -EINVAL;

	written = snprintf(out, out_size, "hdd0:%s", partition_name);

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

static int hddHeaderReadSector(u32 lba, void *sector)
{
	hddAtaTransfer_t *transfer = (hddAtaTransfer_t *)hdd_header_io_buffer;

	if (sector == NULL)
		return -EINVAL;

	transfer->lba = lba;
	transfer->size = 1;
	return fileXioDevctl("hdd0:", HDIOC_READSECTOR, hdd_header_io_buffer,
	                     sizeof(hddAtaTransfer_t), sector, APA_HEADER_SECTOR_SIZE);
}

static int hddHeaderWriteSector(u32 lba, const void *sector)
{
	hddAtaTransfer_t *transfer = (hddAtaTransfer_t *)hdd_header_io_buffer;

	if (sector == NULL)
		return -EINVAL;

	transfer->lba = lba;
	transfer->size = 1;
	memcpy(transfer->data, sector, APA_HEADER_SECTOR_SIZE);
	return fileXioDevctl("hdd0:", HDIOC_WRITESECTOR, hdd_header_io_buffer,
	                     APA_HEADER_SECTOR_SIZE + sizeof(hddAtaTransfer_t), NULL, 0);
}

static int hddHeaderReadSectors(u32 partition_sector)
{
	int i;
	int ret = 0;

	for (i = 0; i < APA_HEADER_SECTORS; i++) {
		ret = hddHeaderReadSector(partition_sector + i, hdd_header_buffer + APA_HEADER_SECTOR_SIZE * i);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int hddHeaderWriteSectors(u32 partition_sector)
{
	int i;
	int ret = 0;

	for (i = 0; i < APA_HEADER_SECTORS; i++) {
		ret = hddHeaderWriteSector(partition_sector + i, hdd_header_buffer + APA_HEADER_SECTOR_SIZE * i);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int hddHeaderGetPartitionSector(const char *partition_name, u32 *partition_sector)
{
	char partition_path[MAX_NAME + 6];
	iox_stat_t stat;
	int ret;

	if (partition_sector == NULL)
		return -EINVAL;

	ret = hddHeaderBuildPartitionPath(partition_path, sizeof(partition_path), partition_name);
	if (ret < 0)
		return ret;

	ret = fileXioGetStat(partition_path, &stat);
	if (ret < 0)
		return ret;

	*partition_sector = stat.private_5;
	return 0;
}

static u32 hddHeaderAttributeFileCapacity(const HddHeaderAttributeAreaHeader *header, const HddHeaderAttributeFile *file)
{
	const HddHeaderAttributeFile *files;
	u32 capacity;
	int i;

	if (header == NULL || file == NULL || file->offset >= APA_HEADER_ATTRIBUTE_AREA_SIZE)
		return 0;

	capacity = APA_HEADER_ATTRIBUTE_AREA_SIZE - file->offset;
	files = &header->system_cnf;
	for (i = 0; i < 6; i++) {
		if (&files[i] == file || files[i].offset == 0 || files[i].size == 0)
			continue;
		if (files[i].offset > file->offset && files[i].offset - file->offset < capacity)
			capacity = files[i].offset - file->offset;
	}

	if (capacity > APA_HEADER_SYSTEM_CNF_MAX_SIZE)
		capacity = APA_HEADER_SYSTEM_CNF_MAX_SIZE;

	return capacity;
}

static int hddHeaderValidateSystemCnfEntry(const HddHeaderAttributeAreaHeader *header)
{
	u32 capacity;

	if (header == NULL)
		return -EINVAL;
	if (memcmp(header->magic, APA_HEADER_MAGIC, strlen(APA_HEADER_MAGIC)))
		return -ENOENT;
	if (header->system_cnf.offset < APA_HEADER_SECTOR_SIZE || header->system_cnf.size == 0)
		return -ENOENT;

	capacity = hddHeaderAttributeFileCapacity(header, &header->system_cnf);
	if (capacity == 0)
		return -EINVAL;
	if (header->system_cnf.size > capacity)
		return -EFBIG;

	return 0;
}

static int hddHeaderReadAttributeHeader(u32 partition_sector, HddHeaderAttributeAreaHeader *header)
{
	int ret;

	if (header == NULL)
		return -EINVAL;

	ret = hddHeaderReadSector(partition_sector + APA_HEADER_ATTRIBUTE_AREA_OFFSET / APA_HEADER_SECTOR_SIZE,
	                          hdd_header_buffer);
	if (ret < 0)
		return ret;

	memcpy(header, hdd_header_buffer, sizeof(*header));
	return 0;
}

static int hddHeaderReadRawRange(u32 partition_sector, u32 raw_offset, void *data, u32 size)
{
	unsigned char *out = (unsigned char *)data;
	u32 copied = 0;
	int ret;

	if (data == NULL && size > 0)
		return -EINVAL;
	if (raw_offset >= APA_HEADER_SIZE || size > APA_HEADER_SIZE - raw_offset)
		return -EINVAL;

	while (copied < size) {
		u32 pos = raw_offset + copied;
		u32 sector_offset = pos % APA_HEADER_SECTOR_SIZE;
		u32 chunk = APA_HEADER_SECTOR_SIZE - sector_offset;

		if (chunk > size - copied)
			chunk = size - copied;

		ret = hddHeaderReadSector(partition_sector + pos / APA_HEADER_SECTOR_SIZE, hdd_header_buffer);
		if (ret < 0)
			return ret;

		memcpy(out + copied, hdd_header_buffer + sector_offset, chunk);
		copied += chunk;
	}

	return 0;
}

static int hddHeaderWriteRawRange(u32 partition_sector, u32 raw_offset, const void *data, u32 size)
{
	const unsigned char *in = (const unsigned char *)data;
	u32 copied = 0;
	int ret;

	if (data == NULL && size > 0)
		return -EINVAL;
	if (raw_offset >= APA_HEADER_SIZE || size > APA_HEADER_SIZE - raw_offset)
		return -EINVAL;

	while (copied < size) {
		u32 pos = raw_offset + copied;
		u32 sector_offset = pos % APA_HEADER_SECTOR_SIZE;
		u32 chunk = APA_HEADER_SECTOR_SIZE - sector_offset;

		if (chunk > size - copied)
			chunk = size - copied;

		ret = hddHeaderReadSector(partition_sector + pos / APA_HEADER_SECTOR_SIZE, hdd_header_buffer);
		if (ret < 0)
			return ret;

		memcpy(hdd_header_buffer + sector_offset, in + copied, chunk);

		ret = hddHeaderWriteSector(partition_sector + pos / APA_HEADER_SECTOR_SIZE, hdd_header_buffer);
		if (ret < 0)
			return ret;

		copied += chunk;
	}

	return 0;
}

static int hddHeaderReadSystemCnf(u32 partition_sector, HddHeaderAttributeAreaHeader *header, unsigned char *data, u32 data_size, u32 *cnf_size)
{
	u32 raw_offset;
	int ret;

	if (data == NULL || cnf_size == NULL)
		return -EINVAL;

	ret = hddHeaderReadAttributeHeader(partition_sector, header);
	if (ret < 0)
		return ret;
	ret = hddHeaderValidateSystemCnfEntry(header);
	if (ret < 0)
		return ret;
	if (header->system_cnf.size > data_size)
		return -EFBIG;

	raw_offset = APA_HEADER_ATTRIBUTE_AREA_OFFSET + header->system_cnf.offset;
	ret = hddHeaderReadRawRange(partition_sector, raw_offset, data, header->system_cnf.size);
	if (ret < 0)
		return ret;

	*cnf_size = header->system_cnf.size;
	return 0;
}

static int hddHeaderWriteSystemCnf(u32 partition_sector, HddHeaderAttributeAreaHeader *header, const unsigned char *data, u32 data_size)
{
	unsigned char cnf_buffer[APA_HEADER_SYSTEM_CNF_MAX_SIZE];
	u32 capacity;
	u32 raw_offset;
	int ret;

	if (header == NULL || data == NULL || data_size == 0)
		return -EINVAL;

	ret = hddHeaderValidateSystemCnfEntry(header);
	if (ret < 0)
		return ret;

	capacity = hddHeaderAttributeFileCapacity(header, &header->system_cnf);
	if (data_size > capacity)
		return -EFBIG;

	memset(cnf_buffer, 0, sizeof(cnf_buffer));
	memcpy(cnf_buffer, data, data_size);

	raw_offset = APA_HEADER_ATTRIBUTE_AREA_OFFSET + header->system_cnf.offset;
	ret = hddHeaderWriteRawRange(partition_sector, raw_offset, cnf_buffer, capacity);
	if (ret < 0)
		return ret;

	header->system_cnf.size = data_size;
	ret = hddHeaderReadSector(partition_sector + APA_HEADER_ATTRIBUTE_AREA_OFFSET / APA_HEADER_SECTOR_SIZE,
	                          hdd_header_buffer);
	if (ret < 0)
		return ret;

	memcpy(hdd_header_buffer, header, sizeof(*header));
	return hddHeaderWriteSector(partition_sector + APA_HEADER_ATTRIBUTE_AREA_OFFSET / APA_HEADER_SECTOR_SIZE,
	                            hdd_header_buffer);
}

static int hddHeaderWriteTempFile(const char *path, const unsigned char *data, u32 size)
{
	char fixed_path[MAX_PATH];
	int fd;
	int ret;

	if (path == NULL || data == NULL)
		return -EINVAL;

	if (genFixPath(path, fixed_path) < 0) {
		snprintf(fixed_path, sizeof(fixed_path), "%s", path);
		fixed_path[sizeof(fixed_path) - 1] = '\0';
	}

	fd = genOpen(fixed_path, FIO_O_WRONLY | FIO_O_TRUNC | FIO_O_CREAT);
	if (fd < 0)
		return fd;

	ret = (genWrite(fd, (void *)data, size) == (int)size) ? 0 : -EIO;
	genClose(fd);
	return ret;
}

static int hddHeaderReadTempFile(const char *path, unsigned char *data, u32 data_size, u32 *size)
{
	char fixed_path[MAX_PATH];
	s64 file_size;
	int fd;
	int ret;

	if (path == NULL || data == NULL || size == NULL)
		return -EINVAL;

	if (genFixPath(path, fixed_path) < 0) {
		snprintf(fixed_path, sizeof(fixed_path), "%s", path);
		fixed_path[sizeof(fixed_path) - 1] = '\0';
	}

	fd = genOpen(fixed_path, FIO_O_RDONLY);
	if (fd < 0)
		return fd;

	file_size = genLseek(fd, 0, SEEK_END);
	if (file_size < 0) {
		genClose(fd);
		return (int)file_size;
	}
	if (file_size > data_size) {
		genClose(fd);
		return -EFBIG;
	}

	ret = (genLseek(fd, 0, SEEK_SET) >= 0 &&
	       genRead(fd, data, (int)file_size) == (int)file_size) ?
	          0 :
	          -EIO;
	genClose(fd);
	if (ret < 0)
		return ret;

	*size = (u32)file_size;
	return 0;
}

static int hddHeaderTempFileExists(const char *path)
{
	char fixed_path[MAX_PATH];
	int fd;

	if (path == NULL || path[0] == '\0')
		return 0;

	if (genFixPath(path, fixed_path) < 0) {
		snprintf(fixed_path, sizeof(fixed_path), "%s", path);
		fixed_path[sizeof(fixed_path) - 1] = '\0';
	}

	fd = genOpen(fixed_path, FIO_O_RDONLY);
	if (fd < 0)
		return 0;

	genClose(fd);
	return 1;
}

static int hddHeaderBuildTempPath(char *path, size_t path_size)
{
	char name[16];
	int i;

	if (path == NULL || path_size == 0)
		return -EINVAL;

	for (i = 0; i < APA_HEADER_SYSTEM_CNF_TEMP_FILE_TRIES; i++) {
		snprintf(name, sizeof(name), APA_HEADER_SYSTEM_CNF_TEMP_FILE_FORMAT, i);
		configBuildSysconfPath(path, path_size, name);
		if (!hddHeaderTempFileExists(path))
			return 0;
	}

	path[0] = '\0';
	return -EEXIST;
}

static void hddHeaderRemoveTempFile(const char *path)
{
	char fixed_path[MAX_PATH];

	if (path == NULL || path[0] == '\0')
		return;

	if (genFixPath(path, fixed_path) < 0) {
		snprintf(fixed_path, sizeof(fixed_path), "%s", path);
		fixed_path[sizeof(fixed_path) - 1] = '\0';
	}
	genRemove(fixed_path);
}

int HddPartitionHeaderSystemCnfExists(const char *partition_name)
{
	HddHeaderAttributeAreaHeader header;
	u32 partition_sector;
	int ret;

	ret = hddHeaderGetPartitionSector(partition_name, &partition_sector);
	if (ret < 0)
		return ret;

	ret = hddHeaderReadAttributeHeader(partition_sector, &header);
	if (ret < 0)
		return ret;

	ret = hddHeaderValidateSystemCnfEntry(&header);
	return (ret < 0) ? ret : 1;
}

int EditHddPartitionHeaderSystemCnf(const char *partition_name)
{
	HddHeaderAttributeAreaHeader header;
	unsigned char original[APA_HEADER_SYSTEM_CNF_MAX_SIZE];
	unsigned char edited[APA_HEADER_SYSTEM_CNF_MAX_SIZE];
	char temp_path[MAX_PATH];
	u32 partition_sector;
	u32 original_size;
	u32 edited_size;
	int ret;

	ret = hddHeaderGetPartitionSector(partition_name, &partition_sector);
	if (ret < 0)
		return ret;

	ret = hddHeaderReadSystemCnf(partition_sector, &header, original, sizeof(original), &original_size);
	if (ret < 0)
		return ret;

	ret = hddHeaderBuildTempPath(temp_path, sizeof(temp_path));
	if (ret < 0)
		return ret;
	configEnsureSysconfDir(temp_path);

	ret = hddHeaderWriteTempFile(temp_path, original, original_size);
	if (ret < 0) {
		hddHeaderRemoveTempFile(temp_path);
		return ret;
	}

	TextEditor(temp_path);

	ret = hddHeaderReadTempFile(temp_path, edited, sizeof(edited), &edited_size);
	hddHeaderRemoveTempFile(temp_path);
	if (ret < 0)
		return ret;
	if (edited_size == 0)
		return -EINVAL;
	if (edited_size == original_size && !memcmp(original, edited, original_size))
		return 0;

	ret = hddHeaderWriteSystemCnf(partition_sector, &header, edited, edited_size);
	return (ret < 0) ? ret : 1;
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

	data_offset = spec->data_offset - APA_HEADER_ATTRIBUTE_AREA_OFFSET;
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

	memcpy(hdd_header_buffer + APA_HEADER_ATTRIBUTE_AREA_OFFSET, APA_HEADER_MAGIC, strlen(APA_HEADER_MAGIC));
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

	ret = hddHeaderBuildPartitionPath(partition_path, sizeof(partition_path), partition_name);
	if (ret < 0)
		return ret;

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
