#include "popstarter_internal.h"

static u32 readIsoLe32(const unsigned char *value)
{
	return ((u32)value[0]) |
	       ((u32)value[1] << 8) |
	       ((u32)value[2] << 16) |
	       ((u32)value[3] << 24);
}

static size_t minSize(size_t lhs, size_t rhs)
{
	return (lhs < rhs) ? lhs : rhs;
}

int validateVcd(const char *path)
{
	unsigned char datacheck[32] __attribute__((aligned(16)));
	s64 size;
	int fd;

	fd = openPathForRead(path, NULL, 0);
	if (fd < 0)
		return POPSTARTER_ERR_OPEN_VCD;

	size = genLseek(fd, 0, SEEK_END);
	if (size < 1102672) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}

	genLseek(fd, 0, SEEK_SET);
	if (genRead(fd, datacheck, 4) != 4) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}
	if (datacheck[0] != 0x41 || datacheck[1] != 0x00 || datacheck[2] != 0xA0 || datacheck[3] != 0x00) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}

	if (genLseek(fd, 1024, SEEK_SET) < 0) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}
	if (genRead(fd, datacheck, 16) != 16) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}
	if (datacheck[0] != 0x6B || datacheck[1] != 0x48 || datacheck[2] != 0x6E ||
	    datacheck[8] != datacheck[12] ||
	    datacheck[9] != datacheck[13] ||
	    datacheck[10] != datacheck[14] ||
	    datacheck[11] != datacheck[15]) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}

	genClose(fd);
	return POPSTARTER_OK;
}

static int readVcdIsoSector(int fd, u32 lba, unsigned char *data)
{
	unsigned char raw[POPSTARTER_VCD_RAW_SECTOR_SIZE] __attribute__((aligned(16)));
	s64 offset;
	int data_offset;

	if (data == NULL)
		return 0;

	offset = (s64)POPSTARTER_VCD_IMAGE_OFFSET + ((s64)lba * POPSTARTER_VCD_RAW_SECTOR_SIZE);
	if (genLseek(fd, offset, SEEK_SET) < 0)
		return 0;
	if (genRead(fd, raw, sizeof(raw)) != (int)sizeof(raw))
		return 0;

	if (raw[15] == 1) {
		data_offset = 16;
	} else if (raw[15] == 2) {
		if (raw[18] & 0x20)
			return 0;
		data_offset = 24;
	} else {
		return 0;
	}

	memcpy(data, raw + data_offset, POPSTARTER_CD_SECTOR_DATA_SIZE);
	return 1;
}

int readVcdIsoPrimaryVolumeDescriptor(int fd, unsigned char *pvd)
{
	unsigned char sector[POPSTARTER_CD_SECTOR_DATA_SIZE] __attribute__((aligned(16)));
	u32 lba;
	int i;

	if (pvd == NULL)
		return 0;

	for (i = 0; i < POPSTARTER_ISO_PVD_SEARCH_SECTORS; i++) {
		lba = POPSTARTER_ISO_PVD_LBA + i;
		if (!readVcdIsoSector(fd, lba, sector))
			return 0;
		if (!memcmp(sector + 1, "CD001", 5)) {
			if (sector[0] == 1) {
				memcpy(pvd, sector, POPSTARTER_CD_SECTOR_DATA_SIZE);
				return 1;
			}
			if (sector[0] == 255)
				return 0;
		}
	}

	return 0;
}

static int isoNameMatches(const unsigned char *name, int name_len, const char *target)
{
	int target_len;
	int i;

	if (name == NULL || target == NULL || name_len <= 0)
		return 0;

	target_len = strlen(target);
	if (name_len < target_len)
		return 0;

	for (i = 0; i < target_len; i++) {
		if (wle_ascii_tolower(name[i]) != wle_ascii_tolower((unsigned char)target[i]))
			return 0;
	}

	return (name_len == target_len || name[target_len] == ';');
}

static int readVcdIsoFileContents(int fd, u32 extent_lba, u32 file_size, char *buffer, size_t buffer_size)
{
	unsigned char sector[POPSTARTER_CD_SECTOR_DATA_SIZE] __attribute__((aligned(16)));
	size_t copied;
	size_t copy_len;
	size_t remaining_file;
	size_t remaining_buffer;
	u32 sector_index;

	if (buffer == NULL || buffer_size == 0 || file_size == 0)
		return 0;

	copied = 0;
	sector_index = 0;
	while (copied < (buffer_size - 1) && copied < file_size) {
		if (!readVcdIsoSector(fd, extent_lba + sector_index, sector))
			break;

		remaining_file = file_size - copied;
		remaining_buffer = buffer_size - 1 - copied;
		copy_len = minSize(POPSTARTER_CD_SECTOR_DATA_SIZE, remaining_file);
		copy_len = minSize(copy_len, remaining_buffer);
		memcpy(buffer + copied, sector, copy_len);
		copied += copy_len;
		sector_index++;
	}
	buffer[copied] = '\0';

	return copied > 0;
}

int readVcdSystemCnf(int fd, char *buffer, size_t buffer_size)
{
	unsigned char pvd[POPSTARTER_CD_SECTOR_DATA_SIZE] __attribute__((aligned(16)));
	unsigned char sector[POPSTARTER_CD_SECTOR_DATA_SIZE] __attribute__((aligned(16)));
	const unsigned char *root_record;
	u32 root_lba;
	u32 root_size;
	u32 root_sectors;
	u32 sector_index;
	int pos;

	if (buffer == NULL || buffer_size == 0)
		return 0;

	buffer[0] = '\0';
	if (!readVcdIsoPrimaryVolumeDescriptor(fd, pvd))
		return 0;

	root_record = pvd + POPSTARTER_ISO_ROOT_RECORD_OFFSET;
	if (root_record[0] < 34)
		return 0;

	root_lba = readIsoLe32(root_record + 2);
	root_size = readIsoLe32(root_record + 10);
	root_sectors = (root_size + POPSTARTER_CD_SECTOR_DATA_SIZE - 1) / POPSTARTER_CD_SECTOR_DATA_SIZE;
	if (root_sectors > POPSTARTER_ISO_MAX_ROOT_DIR_SECTORS)
		root_sectors = POPSTARTER_ISO_MAX_ROOT_DIR_SECTORS;

	for (sector_index = 0; sector_index < root_sectors; sector_index++) {
		if (!readVcdIsoSector(fd, root_lba + sector_index, sector))
			return 0;

		pos = 0;
		while (pos < POPSTARTER_CD_SECTOR_DATA_SIZE) {
			int record_len;
			int name_len;
			int file_flags;

			record_len = sector[pos];
			if (record_len == 0)
				break;
			if (record_len < 34 || pos + record_len > POPSTARTER_CD_SECTOR_DATA_SIZE)
				break;

			file_flags = sector[pos + 25];
			name_len = sector[pos + 32];
			if (!(file_flags & 0x02) &&
			    record_len >= 33 + name_len &&
			    isoNameMatches(sector + pos + 33, name_len, "SYSTEM.CNF")) {
				u32 file_lba;
				u32 file_size;

				file_lba = readIsoLe32(sector + pos + 2);
				file_size = readIsoLe32(sector + pos + 10);
				return readVcdIsoFileContents(fd, file_lba, file_size, buffer, buffer_size);
			}

			pos += record_len;
		}
	}

	return 0;
}
