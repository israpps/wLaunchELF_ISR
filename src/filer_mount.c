//--------------------------------------------------------------
//File name:   filer_mount.c
//--------------------------------------------------------------
#include "filer_internal.h"

#ifdef ULE_DEBUG_BUILD
#ifndef FILEOP_TRACE
#define FILEOP_TRACE 1
#endif
#else
#undef FILEOP_TRACE
#define FILEOP_TRACE 0
#endif

int mountParty(const char *party)
{
	int i, j;
	char pfs_str[6];

	for (i = 0; i < MOUNT_LIMIT; i++) {  //Here we check already mounted PFS indexes
		if (!strcmp(party, mountedParty[i]))
			goto return_i;
	}

	for (i = 0, j = -1; i < MOUNT_LIMIT; i++) {  //Here we search for a free PFS index
		if (mountedParty[i][0] == 0) {
			j = i;
			break;
		}
	}

	if (j == -1) {  //Here we search for a suitable PFS index to unmount
		for (i = 0; i < MOUNT_LIMIT; i++) {
			if ((i != latestMount) && (Party_vmcIndex[i] < 0)) {
				j = i;
				break;
			}
		}
		if (j < 0) {
#if FILEOP_TRACE
			printf("[HDD_MOUNT] no-slot %s\n", party);
#endif
			return -1;  //no usable mountpoint available
		}
		unmountParty(j);
	}
	//Here j is the index of a free PFS mountpoint
	//But 'free' only means that the main uLE program isn't using it
	//If the ftp server is running, that may have used the mountpoints

	//RA NB: The old code to reclaim FTP partitions was seriously bugged...

	i = j;
	strcpy(pfs_str, "pfs0:");

	pfs_str[3] = '0' + i;
	{
		int mount_ret = fileXioMount(pfs_str, party, FIO_MT_RDWR);
		if (mount_ret < 0) {                                                  //if FTP stole it
#if FILEOP_TRACE
			printf("[HDD_MOUNT] mount-failed %s:pfs%d:/ ret=%d\n", party, i, mount_ret);
#endif
			for (i = 0; i < MOUNT_LIMIT; i++) {                               //for loop to kill FTP partition mountpoints
				if ((i != latestMount) && (Party_vmcIndex[i] < 0)) {  //if unneeded by uLE
					unmountParty(i);                                  //unmount partition mountpoint
					pfs_str[3] = '0' + i;                             //prepare to reuse that mountpoint
					mount_ret = fileXioMount(pfs_str, party, FIO_MT_RDWR);
					if (mount_ret >= 0)
						break;  //break from the loop on successful mount
#if FILEOP_TRACE
					printf("[HDD_MOUNT] retry-failed %s:pfs%d:/ ret=%d\n", party, i, mount_ret);
#endif
				}               //ends if unneeded by uLE
			}                   //ends for loop to kill FTP partition mountpoints
			//Here i indicates what happened above with the following meanings:
			//0..MOUNT_LIMIT-1==Success, MOUNT_LIMIT==Failure
			if (i >= MOUNT_LIMIT) {
#if FILEOP_TRACE
				printf("[HDD_MOUNT] failed %s\n", party);
#endif
				return -1;
			}
		}  //ends if clause for mountpoints stolen by FTP
	}
	strcpy(mountedParty[i], party);
return_i:
	latestMount = i;
	return i;
}
void unmountParty(int party_ix)
{
	char pfs_str[6];

	if (party_ix < 0 || party_ix >= MOUNT_LIMIT) {
#if FILEOP_TRACE
		printf("[HDD_MOUNT] unmount-invalid index=%d\n", party_ix);
#endif
		return;
	}

	strcpy(pfs_str, "pfs0:");
	pfs_str[3] += party_ix;
	if (fileXioUmount(pfs_str) < 0) {
#if FILEOP_TRACE
		printf("[HDD_MOUNT] unmount-failed %s:pfs%d:/\n", mountedParty[party_ix], party_ix);
#endif
		return;  //leave variables unchanged if unmount failed (remember true state)
	}
	if (party_ix < MOUNT_LIMIT) {
		mountedParty[party_ix][0] = 0;
	}
	if (latestMount == party_ix)
		latestMount = -1;
}

int getDVRPPartyMountIndex(const char *party)
{
	if (party == NULL)
		return -1;
	if (strcmp(party, "dvr_hdd0:__xdata") == 0)
		return 1;
	if (strcmp(party, "dvr_hdd0:__xcontents") == 0)
		return 0;
	return -1;
}

#ifdef DVRP
int mountDVRPParty(const char *party)
{
	int i;

	for (i = 0; i < MOUNT_LIMIT; i++) {  //Here we check already mounted PFS indexes
		if (!strcmp(party, mountedDVRPParty[i]))
			goto return_i;
	}

	i = getDVRPPartyMountIndex(party);
	if (i < 0) {
		return -1;
	}
	strcpy(mountedDVRPParty[i], party);
return_i:
	latestDVRPMount = i;
	return i;
}
void unmountDVRPParty(int party_ix)
{
	if (party_ix < MOUNT_LIMIT) {
		mountedDVRPParty[party_ix][0] = 0;
	}
	if (latestDVRPMount == party_ix)
		latestDVRPMount = -1;
}
#endif
void unmountAll(void)
{
	char pfs_str[6];
	char vmc_str[6];
	int i;

	strcpy(vmc_str, "vmc0:");
	for (i = 0; i < 2; i++) {
		if (vmcMounted[i]) {
			vmc_str[3] = '0' + i;
			fileXioUmount(vmc_str);
			vmcMounted[i] = 0;
			vmc_PartyIndex[i] = -1;
		}
	}

	strcpy(pfs_str, "pfs0:");
	for (i = 0; i < MOUNT_LIMIT; i++) {
		Party_vmcIndex[i] = -1;
		if (mountedParty[i][0] != 0) {
			pfs_str[3] = '0' + i;
			fileXioUmount(pfs_str);
			mountedParty[i][0] = 0;
		}
	}
	latestMount = -1;
#ifdef DVRP
	for (i = 0; i < MOUNT_LIMIT; i++) {
		if (mountedDVRPParty[i][0] != 0) {
			mountedDVRPParty[i][0] = 0;
		}
	}
	latestDVRPMount = -1;
	#endif
}
static int getHddPartyFromPath(const char *path, char *party, size_t party_size)
{
	const char *start;
	const char *end;

	if (strncmp(path, "hdd", 3) || path[3] < '0' || path[3] > '9' || path[4] != ':' || path[5] != '/')
		return 0;
	start = path + 6;
	if (start[0] == '\0')
		return 0;
	end = strchr(start, '/');
	if (end == NULL)
		end = start + strlen(start);
	if (end <= start)
		return 0;
	snprintf(party, party_size, "hdd%c:%.*s", path[3], (int)(end - start), start);
	return 1;
}
static int clipboardUsesHddParty(const char *party)
{
	char clipParty[MAX_NAME];

	if (nclipFiles <= 0 || clipPath[0] == '\0')
		return 0;
	if (!getHddPartyFromPath(clipPath, clipParty, sizeof(clipParty)))
		return 0;
	return (strcmp(clipParty, party) == 0);
}
void unmountHddPartiesNotNeededByClipboard(void)
{
	int i;

	for (i = 0; i < MOUNT_LIMIT; i++) {
		if (mountedParty[i][0] == 0)
			continue;
		/*
		 * Keep the source partition mounted only when clipboard entries still come
		 * from that partition. Otherwise release mounts while at hdd0:/.
		 */
		if (clipboardUsesHddParty(mountedParty[i]))
			continue;
		if (Party_vmcIndex[i] >= 0)
			continue;
		unmountParty(i);
	}
}
//--------------------------------------------------------------
//End of file: filer_mount.c
//--------------------------------------------------------------
