#ifndef LIBCDVD_LEGACY
#error LIBCDVD_LEGACY not defined, but legacy readCD() included
#endif
int readCD(const char *path, FILEINFO *info, int max)
{
	static struct TocEntry TocEntryList[MAX_ENTRY];
	char dir[MAX_PATH];
	int i, j, n;
	u64 wait_start;

	if (sceCdGetDiskType() <= SCECdUNKNOWN) {
		wait_start = Timer();
		while ((Timer() < wait_start + 500) && !uLE_cdDiscValid()) {
			if (cdmode == SCECdNODISC)
				return 0;
		}
		if (cdmode == SCECdNODISC)
			return 0;
		if ((cdmode < SCECdPSCD) || (cdmode > SCECdPS2DVD)) {
			uLE_cdStop();
			return 0;
		}
	}

	strcpy(dir, &path[5]);
	CDVD_FlushCache();
	n = CDVD_GetDir(dir, NULL, CDVD_GET_FILES_AND_DIRS, TocEntryList, MAX_ENTRY, dir);

	for (i = j = 0; i < n; i++) {
		if (TocEntryList[i].fileProperties & 0x02 &&
		    (!strcmp(TocEntryList[i].filename, ".") ||
		     !strcmp(TocEntryList[i].filename, "..")))
			continue;  //Skip pseudopaths "." and ".."
		strcpy(info[j].name, TocEntryList[i].filename);
		clear_mcTable(&info[j].stats);
		if (TocEntryList[i].fileProperties & 0x02) {
			info[j].stats.AttrFile = MC_ATTR_norm_folder;
		} else {
			info[j].stats.AttrFile = MC_ATTR_norm_file;
			info[j].stats.FileSizeByte = TocEntryList[i].fileSize;
			info[j].stats.Reserve2 = 0;  //Assuming a CD can't have a single 4GB file
		}
		j++;
	}

	size_valid = 1;

	return j;
}