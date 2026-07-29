#ifndef HDD_HEADER_INJECTOR_H
#define HDD_HEADER_INJECTOR_H

#include <stddef.h>

#define HDD_HEADER_SOURCE_PARTITION_NAME_MAX 32

typedef struct
{
	char name[HDD_HEADER_SOURCE_PARTITION_NAME_MAX + 1];
} HddHeaderSourcePartition;

int HddHeaderListSourcePartitions(const char *source_device, HddHeaderSourcePartition *partitions, int max_partitions, int *invalid_count);
int HddPartitionHeaderSystemCnfExists(const char *partition_name);
int EditHddPartitionHeaderSystemCnf(const char *partition_name);
int InjectHddPartitionHeaderFromSource(const char *partition_name, const char *source_device, char *source_dir, size_t source_dir_size,
                                       int *pfs_files_copied, int *pfs_copy_error);

#endif
