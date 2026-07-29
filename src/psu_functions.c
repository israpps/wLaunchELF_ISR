#include "psu_functions.h"
#include "init.h"
#include <string.h>

#define PSU_HEADER_SIZE ((int)sizeof(psu_header))

void clear_psu_header(psu_header *psu)
{
	memset((void *)psu, 0, sizeof(psu_header));
}

void pad_psu_header(psu_header *psu)
{
	memset((void *)psu, 0xFF, sizeof(psu_header));
}

int psu_backup_create_root_image(const char *path, const mcT_header *folder_stats, int *out_fd, int *psu_content)
{
	psu_header psu_head;
	int fd;

	fd = genOpen(path, FIO_O_WRONLY | FIO_O_TRUNC | FIO_O_CREAT);
	if (fd < 0)
		return -1;

	clear_psu_header(&psu_head);
	*psu_content = 2;
	psu_head.attr = folder_stats->attr;
	psu_head.size = *psu_content;
	psu_head.cTime = folder_stats->cTime;
	psu_head.mTime = folder_stats->mTime;
	memcpy(psu_head.name, folder_stats->name, sizeof(psu_head.name));
	psu_head.unknown_1_u16 = folder_stats->unknown_1_u16;
	psu_head.unknown_2_u64 = folder_stats->unknown_2_u64;
	if (genWrite(fd, (void *)&psu_head, sizeof(psu_head)) != PSU_HEADER_SIZE)
		goto fail;

	clear_psu_header(&psu_head);
	psu_head.attr = 0x8427;  //Standard folder attr, for pseudo "." and ".."
	psu_head.cTime = folder_stats->cTime;
	psu_head.mTime = folder_stats->mTime;
	psu_head.name[0] = '.';
	if (genWrite(fd, (void *)&psu_head, sizeof(psu_head)) != PSU_HEADER_SIZE)
		goto fail;
	psu_head.name[1] = '.';
	if (genWrite(fd, (void *)&psu_head, sizeof(psu_head)) != PSU_HEADER_SIZE)
		goto fail;

	*out_fd = fd;
	return 0;

fail:
	genClose(fd);
	return -1;
}

int psu_backup_finalize_root_image(int out_fd, const mcT_header *folder_stats, int psu_content)
{
	psu_header psu_head;
	int result = 0;

	genLseek(out_fd, 0, SEEK_SET);
	clear_psu_header(&psu_head);
	psu_head.attr = folder_stats->attr;
	psu_head.size = psu_content;
	psu_head.cTime = folder_stats->cTime;
	psu_head.mTime = folder_stats->mTime;
	memcpy(psu_head.name, folder_stats->name, sizeof(psu_head.name));
	psu_head.unknown_1_u16 = folder_stats->unknown_1_u16;
	psu_head.unknown_2_u64 = folder_stats->unknown_2_u64;
	if (genWrite(out_fd, (void *)&psu_head, sizeof(psu_head)) != PSU_HEADER_SIZE)
		result = -1;
	genLseek(out_fd, 0, SEEK_END);
	genClose(out_fd);
	return result;
}

int psu_backup_begin_file_entry(int psu_fd, const mcT_header *file_stats, int *psu_pad_size, int *psu_content)
{
	psu_header psu_head;

	clear_psu_header(&psu_head);
	psu_head.attr = file_stats->attr;
	psu_head.size = file_stats->size;
	psu_head.cTime = file_stats->cTime;
	psu_head.mTime = file_stats->mTime;
	memcpy(psu_head.name, file_stats->name, sizeof(psu_head.name));
	if (genWrite(psu_fd, (void *)&psu_head, sizeof(psu_head)) != PSU_HEADER_SIZE)
		return -1;

	if (psu_head.size & 0x3FF)
		*psu_pad_size = 0x400 - (psu_head.size & 0x3FF);
	else
		*psu_pad_size = 0;
	(*psu_content)++;
	return 0;
}

int psu_backup_write_padding(int psu_fd, int psu_pad_size)
{
	psu_header psu_head;
	int bytes;

	pad_psu_header(&psu_head);
	while (psu_pad_size > 0) {
		bytes = (psu_pad_size >= PSU_HEADER_SIZE) ? PSU_HEADER_SIZE : psu_pad_size;
		if (genWrite(psu_fd, (void *)&psu_head, bytes) != bytes)
			return -1;
		psu_pad_size -= bytes;
	}
	return 0;
}

int psu_restore_open_root_image(const char *path, FILEINFO *folder_file, int *in_fd, int *psu_content)
{
	psu_header psu_head;
	mcT_header *mc_head = (mcT_header *)&folder_file->stats;
	int fd;

	fd = genOpen(path, FIO_O_RDONLY);
	if (fd < 0)
		return -1;
	if (genRead(fd, (void *)&psu_head, sizeof(psu_head)) != PSU_HEADER_SIZE) {
		genClose(fd);
		return -1;
	}

	mc_head->attr = psu_head.attr;
	*psu_content = psu_head.size;
	mc_head->size = 0;
	mc_head->cTime = psu_head.cTime;
	mc_head->mTime = psu_head.mTime;
	memcpy(mc_head->name, psu_head.name, sizeof(psu_head.name));
	mc_head->unknown_1_u16 = psu_head.unknown_1_u16;
	mc_head->unknown_2_u64 = psu_head.unknown_2_u64;

	memcpy(folder_file->name, psu_head.name, sizeof(psu_head.name));
	folder_file->name[sizeof(psu_head.name)] = 0;

	*in_fd = fd;
	return 0;
}

int psu_restore_read_file_entry(int psu_fd, FILEINFO *file, int *psu_pad_size)
{
	psu_header psu_head;
	mcT_header *mc_head = (mcT_header *)&file->stats;

	if (genRead(psu_fd, (void *)&psu_head, sizeof(psu_head)) != PSU_HEADER_SIZE)
		return -1;
	if ((psu_head.size == 0) && (psu_head.attr & sceMcFileAttrSubdir))
		return 1;  //Dummy/Pseudo folder entry
	if (psu_head.attr & sceMcFileAttrSubdir)
		return -1;  //Unexpected subfolder in PSU payload

	if (psu_head.size & 0x3FF)
		*psu_pad_size = 0x400 - (psu_head.size & 0x3FF);
	else
		*psu_pad_size = 0;

	mc_head->attr = psu_head.attr;
	mc_head->size = psu_head.size;
	mc_head->cTime = psu_head.cTime;
	mc_head->mTime = psu_head.mTime;
	memcpy(mc_head->name, psu_head.name, sizeof(psu_head.name));
	mc_head->unknown_1_u16 = psu_head.unknown_1_u16;
	mc_head->unknown_2_u64 = psu_head.unknown_2_u64;

	memcpy(file->name, psu_head.name, sizeof(psu_head.name));
	file->name[sizeof(psu_head.name)] = 0;

	return 0;
}

int psu_restore_apply_entry_stats_to_mc(const char *out_dir, const sceMcTblGetDir *stats, int set_mode)
{
	char path[MAX_PATH];
	int dummy;

	if (strncmp(out_dir, "mc", 2))
		return -1;
	if (snprintf(path, sizeof(path), "%s%.32s", out_dir, (const char *)stats->EntryName) >= (int)sizeof(path))
		return -1;

	ensureMemoryCardPortAccessible(path[2] - '0');
	mcGetInfo(path[2] - '0', 0, &dummy, &dummy, &dummy);
	mcSync(0, NULL, &dummy);
	mcSetFileInfo(path[2] - '0', 0, &path[4], (sceMcTblGetDir *)stats, set_mode);
	mcSync(0, NULL, &dummy);
	return 0;
}
