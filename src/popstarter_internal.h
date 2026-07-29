#ifndef POPSTARTER_INTERNAL_H
#define POPSTARTER_INTERNAL_H

#include <stddef.h>

#include "launchelf.h"

enum POPSTARTER_RESULT {
	POPSTARTER_OK = 1,
	POPSTARTER_ERR_UNSUPPORTED_PATH = -10,
	POPSTARTER_ERR_INVALID_VCD = -11,
	POPSTARTER_ERR_NOT_VCD = -12,
	POPSTARTER_ERR_OPEN_VCD = -13,
	POPSTARTER_ERR_OPEN_POPSTARTER = -15,
	POPSTARTER_ERR_INVALID_POPSTARTER = -17
};

#define POPSTARTER_MAX_CANDIDATES 4
#define POPSTARTER_VCD_IMAGE_OFFSET 0x100000
#define POPSTARTER_VCD_RAW_SECTOR_SIZE 2352
#define POPSTARTER_CD_SECTOR_DATA_SIZE 2048
#define POPSTARTER_ISO_PVD_LBA 16
#define POPSTARTER_ISO_PVD_SEARCH_SECTORS 16
#define POPSTARTER_ISO_ROOT_RECORD_OFFSET 156
#define POPSTARTER_ISO_MAX_ROOT_DIR_SECTORS 64

typedef struct
{
	char path[MAX_PATH];
} POPSTARTER_CANDIDATE;

typedef struct
{
	POPSTARTER_CANDIDATE entry[POPSTARTER_MAX_CANDIDATES];
	int count;
} POPSTARTER_CANDIDATE_LIST;

void initPopstarterCandidates(POPSTARTER_CANDIDATE_LIST *candidates);
int addPopstarterCandidate(POPSTARTER_CANDIDATE_LIST *candidates, const char *path);

int isHddDevicePath(const char *path);
int splitHddPfsPath(const char *path, char *hdd_device, size_t hdd_device_size,
                    char *partition, size_t partition_size, const char **subpath);
int isHddPfsPath(const char *path);
int splitPopstarterHddLaunchPath(const char *path, char *hdd_device, size_t hdd_device_size,
                                 char *partition, size_t partition_size, const char **subpath);
int isHddPartyPath(const char *party);
int prefixCaseCmp(const char *value, const char *prefix, size_t prefix_len);
int openPathForRead(const char *path, char *opened_path, size_t opened_path_size);
int prepareLaunch(const char *path, char *arg, size_t arg_size, POPSTARTER_CANDIDATE_LIST *candidates);

int validateVcd(const char *path);
int readVcdIsoPrimaryVolumeDescriptor(int fd, unsigned char *pvd);
int readVcdSystemCnf(int fd, char *buffer, size_t buffer_size);

int buildPopstarterVcdGameID(const char *path, char *gameID, size_t gameID_len);

int preparePopstarterCandidateLaunch(const POPSTARTER_CANDIDATE_LIST *candidates,
                                     char *selected_path, size_t selected_path_size,
                                     char *fullpath, size_t fullpath_size,
                                     char *party, size_t party_size, int *exec_kind);

#endif
