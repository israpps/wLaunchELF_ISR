//--------------------------------------------------------------
//File name:   filer_copy.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "filer_copy.h"
#include "filer_actions.h"
#include "filer_shared.h"
#include "gui_hdd0_format.h"
#include "psu_functions.h"
#include "psu_types.h"

#define MC_SFI 0xFEED  //flag value used for mcSetFileInfo at MC file restoration

#define MC_ATTR_norm_folder 0x8427  //Normal folder on PS2 MC
#define MC_ATTR_norm_file 0x8497    //file (PS2/PS1) on PS2 MC

#define HDD_ATTR_save_folder 0xC4A7      //PS2 save file folder on HDD __common partition
#define HDD_ATTR_save_prot_folder 0xC4AF //PS2 protected save file folder on HDD __common partition

#ifdef ULE_DEBUG_BUILD
#ifndef FILEOP_TRACE
#define FILEOP_TRACE 1
#endif
#else
#undef FILEOP_TRACE
#define FILEOP_TRACE 0
#endif

#define TRANSFER_ETA_UPDATE_MS 1000
#define COPY_BUFFER_FAST_DEFAULT 0x100000

static int isMemoryCardLikePath(const char *path)
{
	return (!strncmp(path, "mc", 2) || !strncmp(path, "vmc", 3)
#ifdef XFROM
	        || !strncmp(path, "xfrom", 5)
#endif
	        );
}

static int isHddRootPath(const char *path)
{
	return (!strncmp(path, "hdd", 3) && path[3] >= '0' && path[3] <= '9' && path[4] == ':' && path[5] == '/' && path[6] == '\0');
}

static int isHddCommonParty(const char *party)
{
	return (!strncmp(party, "hdd", 3) && party[3] >= '0' && party[3] <= '9' && !strcmp(party + 4, ":__common"));
}

static int calcTransferSpeed(u64 bytes, int millis)
{
	u64 bytes_per_second;

	if (millis <= 0)
		return -1;
	if (bytes == 0)
		return 0;

	bytes_per_second = (bytes * 1000ULL) / (u64)millis;
	if (bytes_per_second > 0x7fffffffULL)
		return 0x7fffffff;
	return (int)bytes_per_second;
}

static int calcRemainingTime(u64 remaining_size, int bytes_per_second)
{
	u64 time_left;

	if (bytes_per_second <= 0)
		return -1;

	time_left = remaining_size / (u64)bytes_per_second;
	return (time_left > 0x7fffffffULL) ? 0x7fffffff : (int)time_left;
}

static int smoothTransferSpeed(int previous_speed, int sample_speed)
{
	u64 smoothed_speed;

	if (sample_speed <= 0)
		return sample_speed;
	if (previous_speed <= 0)
		return sample_speed;

	smoothed_speed = ((u64)previous_speed * 3ULL + (u64)sample_speed) / 4ULL;
	if (smoothed_speed > 0x7fffffffULL)
		return 0x7fffffff;
	return (int)smoothed_speed;
}

static int applyMcInfoToMcPath(const char *path, const sceMcTblGetDir *info)
{
	int dummy, mctype, ret = 0;

	if (strncmp(path, "mc", 2))
		return -1;

	mcGetInfo(path[2] - '0', 0, &mctype, &dummy, &dummy);
	mcSync(0, NULL, &dummy);
	mcSetFileInfo(path[2] - '0', 0, &path[4], info, MC_SFI);
	mcSync(0, NULL, &ret);
#if FILEOP_TRACE
	printf("[MC_STAT] setinfo path='%s' attr=0x%x reserve1=0x%x reserve2=0x%x size=%u "
	       "ctime=%04d-%02d-%02d %02d:%02d:%02d mtime=%04d-%02d-%02d %02d:%02d:%02d ret=%d mctype=%d\n",
	       path, info->AttrFile, info->Reserve1, info->Reserve2, info->FileSizeByte,
	       info->_Create.Year, info->_Create.Month, info->_Create.Day,
	       info->_Create.Hour, info->_Create.Min, info->_Create.Sec,
	       info->_Modify.Year, info->_Modify.Month, info->_Modify.Day,
	       info->_Modify.Hour, info->_Modify.Min, info->_Modify.Sec, ret, mctype);
#endif
	return ret;
}

static void fillVmcStatFromMcInfo(iox_stat_t *stat, const sceMcTblGetDir *info)
{
	memset(stat, 0, sizeof(*stat));
	if (info->AttrFile & sceMcFileAttrSubdir)
		stat->mode = FIO_S_IFDIR;
	else
		stat->mode = FIO_S_IFREG;
	if (info->AttrFile & sceMcFileAttrReadable)
		stat->mode |= FIO_S_IRUSR | FIO_S_IRGRP | FIO_S_IROTH;
	if (info->AttrFile & sceMcFileAttrWriteable)
		stat->mode |= FIO_S_IWUSR | FIO_S_IWGRP | FIO_S_IWOTH;
	if (info->AttrFile & sceMcFileAttrExecutable)
		stat->mode |= FIO_S_IXUSR | FIO_S_IXGRP | FIO_S_IXOTH;

	stat->size = info->FileSizeByte;
	stat->attr = info->Reserve2;
	stat->private_0 = info->AttrFile;
	stat->private_1 = info->Reserve1;
	stat->private_2 = info->Reserve2;
	memcpy(stat->ctime, (void *)&info->_Create, 8);
	memcpy(stat->mtime, (void *)&info->_Modify, 8);
	memcpy(stat->atime, stat->mtime, 8);
}

static int applyMcInfoToVmcPath(const char *path, const sceMcTblGetDir *info)
{
	iox_stat_t stat;
	char fixed_path[MAX_PATH];
	int len, ret;

	snprintf(fixed_path, sizeof(fixed_path), "%s", path);
	len = strlen(fixed_path);
	if (len > 6 && fixed_path[len - 1] == '/')
		fixed_path[len - 1] = '\0';

	fillVmcStatFromMcInfo(&stat, info);
	ret = fileXioChStat(fixed_path, &stat, MC_SFI);
#if FILEOP_TRACE
	printf("[VMC_STAT] chstat path='%s' attr=0x%x reserve1=0x%x reserve2=0x%x size=%u "
	       "ctime=%04d-%02d-%02d %02d:%02d:%02d mtime=%04d-%02d-%02d %02d:%02d:%02d ret=%d\n",
	       fixed_path, info->AttrFile, info->Reserve1, info->Reserve2, info->FileSizeByte,
	       info->_Create.Year, info->_Create.Month, info->_Create.Day,
	       info->_Create.Hour, info->_Create.Min, info->_Create.Sec,
	       info->_Modify.Year, info->_Modify.Month, info->_Modify.Day,
	       info->_Modify.Hour, info->_Modify.Min, info->_Modify.Sec, ret);
#endif
	return ret;
}

#ifdef XFROM
static const char *getXfromRelativePath(const char *path)
{
	const char *relative_path = strchr(path, ':');

	if (relative_path == NULL)
		return path;
	relative_path++;
	if (*relative_path == '/')
		relative_path++;
	return relative_path;
}

static int applyMcInfoToXfromPath(const char *path, const sceMcTblGetDir *info)
{
	int dummy, ret = 0;

	if (strncmp(path, "xfrom", 5))
		return -1;

	xfromGetInfo(0, 0, &dummy, &dummy, &dummy);
	xfromSync(0, NULL, &dummy);
	xfromSetFileInfo(0, 0, getXfromRelativePath(path), info, MC_SFI);
	xfromSync(0, NULL, &ret);
#if FILEOP_TRACE
	printf("[XFROM_STAT] setinfo path='%s' attr=0x%x reserve1=0x%x reserve2=0x%x size=%u "
	       "ctime=%04d-%02d-%02d %02d:%02d:%02d mtime=%04d-%02d-%02d %02d:%02d:%02d ret=%d\n",
	       path, info->AttrFile, info->Reserve1, info->Reserve2, info->FileSizeByte,
	       info->_Create.Year, info->_Create.Month, info->_Create.Day,
	       info->_Create.Hour, info->_Create.Min, info->_Create.Sec,
	       info->_Modify.Year, info->_Modify.Month, info->_Modify.Day,
	       info->_Modify.Hour, info->_Modify.Min, info->_Modify.Sec, ret);
#endif
	return ret;
}
#endif

// getGameTitle below is used to extract the real save title of
// an MC gamesave folder. Normally this is held in the icon.sys
// file of a PS2 game save, but that is not the case for saves
// on a PS1 MC, or similar saves backed up to a PS2 MC. Two new
// methods need to be used to extract such titles correctly, and
// these were added in v3.62, by me (dlanor).
// From v3.91 This routine also extracts titles from PSU files.
//--------------------------------------------------------------
int filerGetGameTitle(const char *path, const FILEINFO *file, unsigned char *out)
{
	char party[MAX_NAME], dir[MAX_PATH], tmpdir[MAX_PATH];
	int fd = -1, size, ret;
	psu_header PSU_head;
	int i, tst, PSU_content, psu_pad_pos;

	out[0] = '\0';  //Start by making an empty result string, for failures

	//Avoid title usage in browser root or partition list
	if (path[0] == 0 || isHddRootPath(path) || !strcmp(path, "dvr_hdd0:/"))
		return -1;

	if (!strncmp(path, "hdd", 3)) {
		ret = getHddParty(path, file, party, dir);
		if (ret < 0)
			return -1;
		ret = mountParty(party);
		if (ret < 0)
			return -1;
		dir[3] = ret + '0';
#ifdef DVRP
	} else if (!strncmp(path, "dvr_hdd", 7)) {
		ret = getHddDVRPParty(path, file, party, dir);
		if (ret < 0)
			return -1;
		ret = mountDVRPParty(party);
		if (ret < 0)
			return -1;
		dir[7] = ret + '0';
#endif
	} else {
		strcpy(dir, path);
		strcat(dir, file->name);
		if (file->stats.AttrFile & sceMcFileAttrSubdir)
			strcat(dir, "/");
	}

	ret = -1;  //Assume that result will be failure, to simplify aborts

	if ((file->stats.AttrFile & sceMcFileAttrSubdir) == 0) {
		//Here we know that the object needing a title is a file
		strcpy(tmpdir, dir);                //Copy the pathname for file access
		if (!genCmpFileExt(tmpdir, "psu"))  //Find the extension, if any. If it's anything other than a PSU file
			goto get_PS1_GameTitle;         //then it may be a PS1 save
		//Here we know that the object needing a title is a PSU file
		if ((fd = genOpen(tmpdir, FIO_O_RDONLY)) < 0)
			goto finish;  //Abort if open fails
		tst = genRead(fd, (void *)&PSU_head, sizeof(PSU_head));
		if (tst != sizeof(PSU_head))
			goto finish;  //Abort if read fails
		PSU_content = PSU_head.size;
		for (i = 0; i < PSU_content; i++) {
			tst = genRead(fd, (void *)&PSU_head, sizeof(PSU_head));
			if (tst != sizeof(PSU_head))
				goto finish;  //Abort if read fails
			PSU_head.name[sizeof(PSU_head.name) - 1] = '\0';
			if (!strcmp((char *)PSU_head.name, "icon.sys")) {
				genLseek(fd, 0xC0, SEEK_CUR);
				goto read_title;
			}
			if (PSU_head.size) {
				psu_pad_pos = (PSU_head.size + 0x3FF) & -0x400;
				genLseek(fd, psu_pad_pos, SEEK_CUR);
			}
			//Here the PSU file pointer is positioned for reading next header
		}  //ends for
		//Coming here means that the search for icon.sys failed
		goto finish;  //So go finish off this function
	}                 //ends if clause for files needing a title

	//Here we know that the object needing a title is a folder
	//First try to find a valid PS2 icon.sys file inside the folder
	strcpy(tmpdir, dir);
	strcat(tmpdir, "icon.sys");
	if ((fd = genOpen(tmpdir, FIO_O_RDONLY)) >= 0) {
		if ((size = genLseek(fd, 0, SEEK_END)) <= 0x100)
			goto finish;
		genLseek(fd, 0xC0, SEEK_SET);
		goto read_title;
	}
	//Next try to find a valid PS1 savefile inside the folder instead
	strcpy(tmpdir, dir);
	strcat(tmpdir, file->name);  //PS1 save file should have same name as folder

get_PS1_GameTitle:
	if ((fd = genOpen(tmpdir, FIO_O_RDONLY)) < 0)
		goto finish;  //PS1 gamesave file needed
	if ((size = genLseek(fd, 0, SEEK_END)) < 0x2000)
		goto finish;  //Min size is 8K
	if (size & 0x1FFF)
		goto finish;  //Size must be a multiple of 8K
	genLseek(fd, 0, SEEK_SET);
	genRead(fd, out, 2);
	if (strncmp((const char *)out, "SC", 2))
		goto finish;  //PS1 gamesaves always start with "SC"
	genLseek(fd, 4, SEEK_SET);

read_title:
	genRead(fd, out, 32 * 2);
	out[32 * 2] = 0;
	genClose(fd);
	fd = -1;
	ret = 0;

finish:
	if (fd >= 0)
		genClose(fd);
	return ret;
}
//--------------------------------------------------------------
//The function 'copy' below is called to copy a single object,
//indicated by 'file' from 'inPath' to 'outPath', but this may
//be either a single file or a folder. In the latter case the
//folder contents should also be copied, recursively.
//--------------------------------------------------------------
int copy(char *outPath, const char *inPath, FILEINFO file, int recurses)
{
	FILEINFO newfile, files[MAX_ENTRY];
	iox_stat_t iox_stat;
	char out[MAX_PATH], in[MAX_PATH], tmp[MAX_PATH],
	    progress[MAX_PATH * 4],
	    *buff = NULL, *buff2 = NULL, *copyBuff, inParty[MAX_NAME], outParty[MAX_NAME];
	int nfiles, i;
	u64 size = 0;
	int ret = -1, pfsout = -1, pfsin = -1, in_fd = -1, out_fd = -1, buffSize, bytesRead, bytesWritten;
	int buffCount = 1, buffIndex = 0;
	int dummy;
	int speed = 0;
	int remain_time = -1, TimeDiff = 0;
	int eta_sample_speed = -1, smooth_speed = -1, EtaTimeDiff = 0;
	u64 old_size = 0, SizeDiff = 0;
	u64 eta_old_size = 0, EtaSizeDiff = 0;
	u64 OldTime = 0LL;
	u64 EtaOldTime = 0LL;
	mcT_header *mcT_head_p = (mcT_header *)&file.stats;
	int psu_pad_size = 0, PSU_restart_f = 0;
	char *cp, *np;
#if FILEOP_TRACE
	int trace_net_copy = 0;
	int trace_vmc_copy = 0;
	unsigned int trace_chunk_index = 0;
	u64 chunk_remaining_before = 0;
#endif
	u64 CurrentTime = 0LL;

	if (recurses + 1 >= MAX_RECURSE)
		return -1;

#if FILEOP_TRACE
	trace_vmc_copy = (!strncmp(inPath, "vmc", 3) || !strncmp(outPath, "vmc", 3));
#endif

	if (!ensurePathDeviceStackReady(inPath) || !ensurePathDeviceStackReady(outPath)) {
#if FILEOP_TRACE
		if (trace_vmc_copy)
			printf("[VMC_COPY] stack-ready failed inPath='%s' outPath='%s' name='%s' recurse=%d\n",
			       inPath, outPath, file.name, recurses);
#endif
		return -1;
	}

	PM_flag[recurses + 1] = PM_NORMAL;  //assume normal mode for next level
	PM_file[recurses + 1] = -1;         //assume that no special file is needed

restart_copy:  //restart point for PM_PSU_RESTORE to reprocess modified arguments

	newfile = file;  //assume that no renaming is to be done

	if (PasteMode == PM_RENAME && recurses == 0) {  //if renaming requested and valid
		if (keyboard(newfile.name, 36) <= 0)        //if name entered by user made the result invalid
			strcpy(newfile.name, file.name);        //  recopy newname from file.name
	}                                               //ends if clause for renaming name entry
	//Here the struct 'newfile' is FILEINFO for destination, regardless of renaming
	//for non-renaming cases this is always identical to the struct 'file'

	if (!strncmp(inPath, "hdd", 3)) {
		if (getHddParty(inPath, &file, inParty, in) < 0)
			return -1;
		/* Re-resolve/mount source partition each copy so stale browser mounts do not break paste/move. */
		pfsin = mountParty(inParty);
		if (pfsin < 0)
			return -1;
		in[3] = pfsin + '0';
#if FILEOP_TRACE
		printf("[FILEOP] hdd-map src %s:pfs%d:%s recurse=%d\n",
		       inParty, pfsin, in + 5, recurses);
#endif
#ifdef DVRP
	} else if (!strncmp(inPath, "dvr_hdd", 7)) {
		if (getHddDVRPParty(inPath, &file, inParty, in) < 0)
			return -1;
		pfsin = mountDVRPParty(inParty);
		if (pfsin < 0)
			return -1;
		in[7] = pfsin + '0';
#endif
	} else
		sprintf(in, "%s%s", inPath, file.name);

	if (!strncmp(outPath, "hdd", 3)) {
		if (getHddParty(outPath, &newfile, outParty, out) < 0)
			return -1;
		/* Re-resolve/mount destination partition each copy so stale browser mounts do not break paste/move. */
		pfsout = mountParty(outParty);
		if (pfsout < 0)
			return -1;
		out[3] = pfsout + '0';
#if FILEOP_TRACE
		printf("[FILEOP] hdd-map dst %s:pfs%d:%s recurse=%d\n",
		       outParty, pfsout, out + 5, recurses);
#endif
#ifdef DVRP
	} else if (!strncmp(outPath, "dvr_hdd", 7)) {
		if (getHddDVRPParty(outPath, &newfile, outParty, out) < 0)
			return -1;
		pfsout = mountDVRPParty(outParty);
		if (pfsout < 0)
			return -1;
		out[7] = pfsout + '0';
#endif
	} else
		sprintf(out, "%s%s", outPath, newfile.name);

	if (!strcmp(in, out))
		return 0;  //if in and out are identical our work is done.

#if FILEOP_TRACE
	trace_net_copy = (!strncmp(in, "host", 4) || !strncmp(in, "udpfs", 5)
	                  || !strncmp(out, "host", 4) || !strncmp(out, "udpfs", 5));
	trace_vmc_copy = (trace_vmc_copy || !strncmp(in, "vmc", 3) || !strncmp(out, "vmc", 3));
	if (trace_vmc_copy) {
		printf("[VMC_COPY] start recurse=%d mode=%d inPath='%s' outPath='%s' in='%s' out='%s' name='%s' attr=0x%x size=%u:%u\n",
		       recurses, PM_flag[recurses], inPath, outPath, in, out, file.name,
		       file.stats.AttrFile, file.stats.Reserve2, file.stats.FileSizeByte);
	}
#endif

	//Here 'in' and 'out' are complete pathnames for the object to copy
	//patched to contain appropriate 'pfs' refs where args used 'hdd'
	//The physical device specifiers remain in 'inPath' and 'outPath'

	//Here we have an object to copy, which may be either a file or a folder
	if (file.stats.AttrFile & sceMcFileAttrSubdir) {
		//Here we have a folder to copy, starting with an attempt to create it
		//This is where we must act differently for PSU backup, creating a PSU file instead
		if (PasteMode == PM_PSU_BACKUP) {
			if (recurses)
				return -1;  //abort, as subfolders are not copied to PSU backups
			i = strlen(out) - 1;
			if (out[i] == '/')
				out[i] = 0;
			strcpy(tmp, out);
			np = tmp + strlen(tmp) - strlen(file.name);  //np = start of the pure filename
			cp = tmp + strlen(tmp);                      //cp = end of the pure filename
			if (!setting->PSU_HugeNames)
				cp = np;  //cp = start of the pure filename

			if ((file_show == 2) || setting->PSU_HugeNames) {  //at request, use game title
				ret = filerGetGameTitle(inPath, &file, file.title);
				if ((ret == 0) && file.title[0] && setting->PSU_HugeNames) {
					*cp++ = '_';
					*cp = '\0';
				}
				transcpy_sjis(cp, file.title);
			}
			//Here game title has been used for the name if requested, either alone
			//or combined with the original folder name (for PSU_HugeNames)

			if (np[0] == 0)             //If name is now empty (bad gamesave title)
				strcpy(np, file.name);  //revert to normal folder name

			for (i = 0; np[i];) {
				i = strcspn(np, "\"\\/:*?<>|");  //Filter out illegal name characters
				if (np[i])
					np[i] = '_';
			}
			//Here illegal characters, from either title or original folder name
			//have been filtered out (replaced by underscore) to ensure compatibility

			cp = tmp + strlen(tmp);        //set cp pointing to the end of the filename
			if (setting->PSU_DateNames) {  //at request, append modification timestamp string
				sprintf(cp, "_%04d-%02d-%02d_%02d-%02d-%02d",
				        mcT_head_p->mTime.year, mcT_head_p->mTime.month, mcT_head_p->mTime.day,
				        mcT_head_p->mTime.hour, mcT_head_p->mTime.min, mcT_head_p->mTime.sec);
			}
			//Here a timestamp has been added to the name if requested by PSU_DateNames

			genLimObjName(tmp, 4);  //Limit name to leave room for 4 characters more
			strcat(tmp, ".psu");    //add the PSU file extension
#if defined(ETH) || defined(UDPFS)
			if (!strncmp(tmp, "host:/", 6))
				makeHostPath(tmp, tmp);
#endif
			if (setting->PSU_DateNames && setting->PSU_NoOverwrite) {
				if (0 <= (out_fd = genOpen(tmp, FIO_O_RDONLY))) {  //Name conflict ?
					genClose(out_fd);
					out_fd = -1;
					return 0;
				}
			}
			//here tmp is the name of an existing file, to be removed before making new one
			genRemove(tmp);
			if (psu_backup_create_root_image(tmp, mcT_head_p, &out_fd, &PSU_content) < 0)
				return -1;
			PM_file[recurses + 1] = out_fd;
			PM_flag[recurses + 1] = PM_PSU_BACKUP;  //Set PSU backup mode for next level
		} else {  //any other PasteMode than PM_PSU_BACKUP
			ret = newdir(outPath, newfile.name);
			if (ret == -17) {  //NB: 'newdir' must return -17 for ALL pre-existing folder cases
				ret = filerGetGameTitle(outPath, &newfile, newfile.title);
				transcpy_sjis(tmp, newfile.title);
				sprintf(progress,
				        "%s:\n"
				        "%s%s/\n"
				        "\n"
				        "%s:\n",
				        LNG(Name_conflict_for_this_folder), outPath,
				        file.name, LNG(With_gamesave_title));
				if (tmp[0])
					strcat(progress, tmp);
				else
					strcat(progress, LNG(Undefined_Not_a_gamesave));
				sprintf(tmp, "\n\n%s ?", LNG(Do_you_wish_to_overwrite_it));
				strcat(progress, tmp);
				if (ynDialog(progress) < 0)
					return -1;
				if (PasteMode == PM_PSU_RESTORE) {
					if (filerConfirmExploitDelete(outPath, &newfile) < 0)
						return -1;
					ret = delete (outPath, &newfile);  //Attempt recursive delete
					if (ret < 0)
						return -1;
					if (newdir(outPath, newfile.name) < 0)
						return -1;
				} else if (!filerIsExploitProtectedPath(outPath, NULL) &&
				           filerConfirmExploitModify(outPath, &newfile) < 0) {
					return -1;
				}
				drawMsg(LNG(Pasting));
			} else if (ret < 0) {
#if FILEOP_TRACE
				if (trace_vmc_copy)
					printf("[VMC_COPY] mkdir failed outPath='%s' name='%s' out='%s' ret=%d recurse=%d\n",
					       outPath, newfile.name, out, ret, recurses);
#endif
				return -1;  //return error for failure to create destination folder
			}

			//Set save folder attributes for __common partition when copying to HDD to make it show up in the HDD-OSD Browser
			if (isHddCommonParty(outParty) && !(file.stats.AttrFile & sceMcFileAttrPDAExec)) {
				//Calculate the out level
				ret = 0;
				for (i = 0; i <= strlen(out); i++) {
					if (out[i] == '/')
						ret++;
				}
				if (ret == 3) { //Only set attributes for the second-level folder (out is pfs0:/?/?/)
					iox_stat.attr = (file.stats.AttrFile & sceMcFileAttrDupProhibit) ? HDD_ATTR_save_prot_folder : HDD_ATTR_save_folder;
					fileXioChStat(out, &iox_stat, FIO_CST_ATTR);
					iox_stat.attr = 0; //Reset attributes
				}
			}
		}
		//Here a destination folder, or a PSU file exists, ready to receive files
		sprintf(in, "%s%s/", inPath, file.name);      //restore phys dev spec to 'in'
		sprintf(out, "%s%s", outPath, newfile.name);  //restore phys dev spec to 'out'
		genLimObjName(out, 0);                        //Limit dest folder name
		strcat(out, "/");                             //Separate dest folder name

		if (PasteMode == PM_PSU_RESTORE && PSU_restart_f) {
			nfiles = PSU_content;
			for (i = 0; i < nfiles; i++) {
				ret = psu_restore_read_file_entry(PM_file[recurses + 1], &files[0], &psu_pad_size);
				if (ret > 0)  //Dummy/Pseudo folder entry
					continue;
				if (ret < 0) {
					ret = -1;
					break;
				}
				//Finally we can make the recursive call
				if ((ret = copy(out, in, files[0], recurses + 1)) < 0) {
#if FILEOP_TRACE
					if (trace_vmc_copy)
						printf("[VMC_COPY] psu child failed parent_in='%s' parent_out='%s' child='%s' ret=%d recurse=%d\n",
						       in, out, files[0].name, ret, recurses);
#endif
					break;
				}
				//We must also step past any file padding, for next header
				if (psu_pad_size)
					genLseek(PM_file[recurses + 1], psu_pad_size, SEEK_CUR);
				//finally, we must adjust attributes of the new file copy, to ensure
				//correct timestamps and attributes (requires MC-specific functions)
				if (!strncmp(out, "mc", 2))
					psu_restore_apply_entry_stats_to_mc(out, &files[0].stats, MC_SFI);
#ifdef XFROM
				else if (!strncmp(out, "xfrom", 5)) {
					strcpy(tmp, out);
					strncat(tmp, (const char *)files[0].stats.EntryName, 32);
					applyMcInfoToXfromPath(tmp, &files[0].stats);
				}
#endif
			}                                 //ends main for loop of valid PM_PSU_RESTORE mode
			genClose(PM_file[recurses + 1]);  //Close the PSU file
			                                  //Finally fix the stats of the containing folder
			                                  //It has to be done last, as timestamps would change when fixing files
			                                  //--- This has been moved to a later clause.
		} else {                              //Any other mode than a valid PM_PSU_RESTORE
			nfiles = getDir(in, files);
			for (i = 0; i < nfiles; i++) {
				if ((ret = copy(out, in, files[i], recurses + 1)) < 0) {
#if FILEOP_TRACE
					if (trace_vmc_copy)
						printf("[VMC_COPY] child failed parent_in='%s' parent_out='%s' child='%s' ret=%d recurse=%d\n",
						       in, out, files[i].name, ret, recurses);
#endif
					break;
				}
			}  //ends main for loop for all modes other than valid PM_PSU_RESTORE
		}
		//folder contents are copied by the recursive call above, with error handling below
		if (ret < 0) {
			if ((PM_flag[recurses + 1] == PM_PSU_BACKUP) && (PM_file[recurses + 1] >= 0))
				genClose(PM_file[recurses + 1]);
			return -1;
		}

		//Here folder contents have been copied error-free, but we also need to copy
		//attributes and timestamps, and close the attribute/PSU file if such was used
		//Lots of stuff need to be done here to make PSU operations work properly
		if (PM_flag[recurses + 1] == PM_PSU_BACKUP) {  //PSU Backup mode folder paste closure
			if (psu_backup_finalize_root_image(PM_file[recurses + 1], mcT_head_p, PSU_content) < 0)
				return -1;
		} else if (PM_flag[recurses + 1] == PM_NORMAL) {             //Normal mode folder paste closure
			if (!strncmp(out, "mc", 2)) {                            //Handle folder copied to MC
				ret = MC_SFI;                                   //default request for changing entire mcTable
				if (!isMemoryCardLikePath(in)) {                //Handle folder copied from non-MC/non-VMC to MC
					file.stats.AttrFile = MC_ATTR_norm_folder;  //normalize MC folder attribute
#if defined(ETH) || defined(UDPFS)
					if (!strncmp(in, "host", 4) || !strncmp(in, "udpfs", 5)) {  //Handle folder copied from host/udpfs to MC
						ret = 4;                                //request change only of main attribute for host:
					}                                           //ends host: source clause
#endif
				}                                               //ends non-MC/non-VMC source clause
				if (ret == MC_SFI)
					applyMcInfoToMcPath(out, &file.stats);
				else {
					mcGetInfo(out[2] - '0', 0, &dummy, &dummy, &dummy);  //Wakeup call
					mcSync(0, NULL, &dummy);
					mcSetFileInfo(out[2] - '0', 0, &out[4], &file.stats, ret);
					mcSync(0, NULL, &dummy);
				}
			} else if (!strncmp(out, "vmc", 3)) {               //Handle folder copied to VMC
				applyMcInfoToVmcPath(out, &file.stats);
#ifdef XFROM
			} else if (!strncmp(out, "xfrom", 5)) {             //Handle folder copied to XFROM
				if (!isMemoryCardLikePath(in))                  //Handle folder copied from non-MC/non-VMC/non-XFROM
					file.stats.AttrFile = MC_ATTR_norm_folder;  //normalize MC folder attribute
				applyMcInfoToXfromPath(out, &file.stats);
#endif
			} else {                                            //Handle folder copied to non-MC
				if (!strncmp(out, "host", 4) || !strncmp(out, "udpfs", 5)) {  //for files copied to host/udpfs: we skip Chstat
				} else if (!strncmp(out, "mass", 4)) {  //for files copied to mass: we skip Chstat
#ifdef MX4SIO
				} else if (!strncmp(out, "mx4sio", 6)) {  //for files copied to mx4sio: we skip Chstat
#endif
				} else if (!strncmp(out, "ata", 3)) {  //for files copied to ata: we skip Chstat
				} else {                                //for other devices we use fileXio_ stuff
					memcpy(iox_stat.ctime, (void *)&file.stats._Create, 8);
					memcpy(iox_stat.mtime, (void *)&file.stats._Modify, 8);
					memcpy(iox_stat.atime, iox_stat.mtime, 8);
					ret = FIO_CST_CT | FIO_CST_AT | FIO_CST_MT;  //Request timestamp stat change
#if defined(ETH) || defined(UDPFS)
					if (!strncmp(in, "host", 4) || !strncmp(in, "udpfs", 5)) {  //Handle folder copied from host/udpfs:
						ret = 0;                                 //Request NO stat change
					}
#endif
					dummy = fileXioChStat(out, &iox_stat, ret);
				}
			}
		}
		if ((PM_flag[recurses + 1] == PM_PSU_RESTORE) && !strncmp(out, "mc", 2)) {
			//Finally fix the stats of the containing folder
			//It has to be done last, as timestamps would change when fixing files
			applyMcInfoToMcPath(out, &file.stats);
		} else if ((PM_flag[recurses + 1] == PM_PSU_RESTORE) && !strncmp(out, "vmc", 3)) {
			applyMcInfoToVmcPath(out, &file.stats);
#ifdef XFROM
		} else if ((PM_flag[recurses + 1] == PM_PSU_RESTORE) && !strncmp(out, "xfrom", 5)) {
			applyMcInfoToXfromPath(out, &file.stats);
#endif
		}
		//the return code below is used if there were no errors copying a folder
		return 0;
	}

	//Here we know that the object to copy is a file, not a folder
	//But in PSU Restore mode we must treat PSU files as special folders, at level 0.
	//and recursively call copy with higher recurse level to process the contents
	if (PasteMode == PM_PSU_RESTORE && recurses == 0) {
		if (!genCmpFileExt(in, "psu"))
			goto non_PSU_RESTORE_init;  //if not a PSU file, go do normal pasting

		if (psu_restore_open_root_image(in, &file, &in_fd, &PSU_content) < 0)
			return -1;
		PM_file[recurses + 1] = in_fd;           //File descriptor for PSU
		PM_flag[recurses + 1] = PM_PSU_RESTORE;  //Mode flag for recursive entry
		//The helper above transformed the PSU header into a folder-like FILEINFO entry
		PSU_restart_f = 1;
		goto restart_copy;
	}
non_PSU_RESTORE_init:
	//It is now time to open the input file, indicated by 'in_fd'
	//But in PSU Restore mode we must use the already open PSU file instead
	if (PM_flag[recurses] == PM_PSU_RESTORE) {
		in_fd = PM_file[recurses];
		size = mcT_head_p->size;
		} else {  //Any other mode than PM_PSU_RESTORE
#if defined(ETH) || defined(UDPFS)
			if (!strncmp(in, "host:/", 6))
				makeHostPath(in, in);
#endif
			in_fd = genOpen(in, FIO_O_RDONLY);
			if (in_fd < 0) {
				ret = in_fd;
#if FILEOP_TRACE
				if (trace_vmc_copy)
					printf("[VMC_COPY] input open failed in='%s' ret=%d recurse=%d\n", in, ret, recurses);
#endif
				goto copy_file_exit;
			}
			{
				s64 in_size = genLseek(in_fd, 0, SEEK_END);
				if (in_size < 0) {
					ret = (int)in_size;
#if FILEOP_TRACE
					if (trace_vmc_copy)
						printf("[VMC_COPY] input size failed in='%s' fd=%d ret=%d recurse=%d\n",
						       in, in_fd, ret, recurses);
#endif
					goto copy_file_exit;
				}
				size = (u64)in_size;
			}
			genLseek(in_fd, 0, SEEK_SET);
		}

	//Here the input file has been opened, indicated by 'in_fd'
	//It is now time to open the output file, indicated by 'out_fd'
	//except in the case of a PSU backup, when we must add a header to PSU instead
	if (PM_flag[recurses] == PM_PSU_BACKUP) {
		out_fd = PM_file[recurses];
		if (psu_backup_begin_file_entry(out_fd, mcT_head_p, &psu_pad_size, &PSU_content) < 0) {
			ret = -EIO;
			goto copy_file_exit;
		}
	} else {            //Any other PasteMode than PM_PSU_BACKUP needs a new output file
#if defined(ETH) || defined(UDPFS)
		if (!strncmp(out, "host:/", 6))
			makeHostPath(out, out);
#endif
		genLimObjName(out, 0);                                //Limit dest file name
		dummy = genRemove(out);                               //Remove old file if present
#if FILEOP_TRACE
		if (trace_vmc_copy)
			printf("[VMC_COPY] remove existing out='%s' ret=%d recurse=%d\n", out, dummy, recurses);
#endif
		out_fd = genOpen(out, FIO_O_WRONLY | FIO_O_TRUNC | FIO_O_CREAT);  //Create new file
		if (out_fd < 0) {
			ret = out_fd;
#if FILEOP_TRACE
			if (trace_vmc_copy)
				printf("[VMC_COPY] output open failed out='%s' ret=%d recurse=%d\n", out, ret, recurses);
#endif
			goto copy_file_exit;
		}
	}

	//Here the output file has been opened, indicated by 'out_fd'

	/* Determine the block size. Since LaunchELF is single-threaded,
       using a large block size with a slow device will result in an unresponsive UI.
       To prevent a loss in performance, these values must each be in a multiple of the device's sector/page size.
       They must also be in multiples of 64, to prevent FILEIO from doing alignment correction in software. */
	buffSize = COPY_BUFFER_FAST_DEFAULT;  //First assume fast-device buffer size
	if (!strncmp(out, "mc", 2) || !strncmp(out, "mass", 4) || !strncmp(out, "vmc", 3))
		buffSize = 131072;  //Use  128KB if writing to USB (Flash RAM writes) or MC (pretty slow).
	                        //VMC contents should use the same size, as VMCs will often be stored on USB
#ifdef MX4SIO
	else if (!strncmp(out, "mx4sio", 6))
		buffSize = 131072;  //Use 128KB when writing to MX4SIO (FAT over SIO2).
#endif
	else if (!strncmp(in, "mc", 2))
		buffSize = 262144;  //Use 256KB if reading from MC (still pretty slow)
#if defined(ETH) || defined(UDPFS)
	else if (!strncmp(out, "host", 4))
		buffSize = 393216;  //Use 384KB if writing to HOST (acceptable)
#endif
	else if ((!strncmp(in, "mass", 4)) || (!strncmp(in, "host", 4))
#ifdef MX4SIO
	         || (!strncmp(in, "mx4sio", 6))
#endif
	        )
		buffSize = 524288;  //Use 512KB reading from USB or HOST (acceptable)

	if (size < (u64)buffSize)
		buffSize = (int)size;

	if (buffSize == 0) {
#if FILEOP_TRACE
		if (trace_net_copy || trace_vmc_copy) {
			printf("[FILEOP] copy-empty in=%s out=%s mode=%d recurse=%d\n",
			       in, out, PM_flag[recurses], recurses);
		}
#endif
		ret = 0;
		goto copy_file_data_done;
	}

	buff = (char *)memalign(64, buffSize);  //Attempt buffer allocation
	if (buff == NULL) {                     //if allocation fails
		ret = -ENOMEM;
		genClose(out_fd);
		out_fd = -1;
		if (PM_flag[recurses] != PM_PSU_BACKUP)
			genRemove(out);
		goto copy_file_exit_mem_err;
	}
	if (buffSize == COPY_BUFFER_FAST_DEFAULT) {
		buff2 = (char *)memalign(64, buffSize);
		if (buff2 != NULL)
			buffCount = 2;
#if FILEOP_TRACE
		else if (trace_net_copy || trace_vmc_copy)
			printf("[FILEOP] copy-double-buffer fallback in=%s out=%s buff=%d\n",
			       in, out, buffSize);
#endif
	}

	old_size = written_size;  //Note initial progress data pos
	OldTime = Timer();        //Note initial progress time
	eta_old_size = old_size;
	EtaOldTime = OldTime;
#if FILEOP_TRACE
	if (trace_net_copy || trace_vmc_copy) {
		printf("[FILEOP] copy-start in=%s out=%s size=%llu buff=%d buffers=%d mode=%d recurse=%d\n",
		       in, out, (unsigned long long)size, buffSize, buffCount, PM_flag[recurses], recurses);
	}
#endif

	while (size > 0) {  // ----- The main copying loop starts here -----

		if (size < (u64)buffSize)
			buffSize = (int)size;  //Adjust effective buffer size to remaining data

		CurrentTime = Timer();
		TimeDiff = CurrentTime - OldTime;
		OldTime = CurrentTime;
		SizeDiff = written_size - old_size;
		old_size = written_size;
		if (SizeDiff) {  //if anything was written this time
			speed = calcTransferSpeed(SizeDiff, TimeDiff);
		} else if (TimeDiff) {                     //if nothing written though time passed
			speed = 0;                             //set speed as zero
		} else {                                   //if nothing written and no time passed
			speed = -1;                            //set speed as unknown
		}

		if (CurrentTime - EtaOldTime >= TRANSFER_ETA_UPDATE_MS) {
			EtaTimeDiff = CurrentTime - EtaOldTime;
			EtaSizeDiff = written_size - eta_old_size;
			EtaOldTime = CurrentTime;
			eta_old_size = written_size;
			eta_sample_speed = calcTransferSpeed(EtaSizeDiff, EtaTimeDiff);
			if (eta_sample_speed > 0) {
				smooth_speed = smoothTransferSpeed(smooth_speed, eta_sample_speed);
				remain_time = calcRemainingTime(size, smooth_speed);
			} else if (eta_sample_speed == 0) {
				smooth_speed = 0;
				remain_time = -1;
			}
		}

		sprintf(progress, "%s : %s", LNG(Pasting_file), file.name);

		sprintf(tmp, "\n%s : ", LNG(Remain_Size));
		strcat(progress, tmp);
		if (size <= 1024)
			sprintf(tmp, "%llu %s", (unsigned long long)size, LNG(bytes));  // bytes
		else
			sprintf(tmp, "%llu %s", (unsigned long long)(size / 1024), LNG(Kbytes));  // Kbytes
		strcat(progress, tmp);

		sprintf(tmp, "\n%s: ", LNG(Current_Speed));
		strcat(progress, tmp);
		if (speed == -1)
			strcpy(tmp, LNG(Unknown));
		else if (speed <= 1024)
			sprintf(tmp, "%d %s/sec", speed, LNG(bytes));  // bytes/sec
		else
			sprintf(tmp, "%d %s/sec", speed / 1024, LNG(Kbytes));  //Kbytes/sec
		strcat(progress, tmp);

		sprintf(tmp, "\n%s : ", LNG(Remain_Time));
		strcat(progress, tmp);
		if (remain_time == -1)
			strcpy(tmp, LNG(Unknown));
		else if (remain_time < 60)
			sprintf(tmp, "%d sec", remain_time);  // sec
		else if (remain_time < 3600)
			sprintf(tmp, "%d m %d sec", remain_time / 60, remain_time % 60);  // min,sec
		else
			sprintf(tmp, "%d h %d m %d sec", remain_time / 3600, (remain_time % 3600) / 60, remain_time % 60);  // hour,min,sec
		strcat(progress, tmp);

		sprintf(tmp, "\n\n%s: ", LNG(Written_Total));
		strcat(progress, tmp);
		sprintf(tmp, "%llu %s", (unsigned long long)(written_size / 1024), LNG(Kbytes));  //Kbytes
		strcat(progress, tmp);

		sprintf(tmp, "\n%s: ", LNG(Average_Speed));
		strcat(progress, tmp);
		TimeDiff = Timer() - PasteTime;
		speed = calcTransferSpeed(written_size, TimeDiff);
		if (speed == -1)
			strcpy(tmp, LNG(Unknown));
		else {
			if (speed <= 1024)
				sprintf(tmp, "%d %s/sec", speed, LNG(bytes));  // bytes/sec
			else
				sprintf(tmp, "%d %s/sec", speed / 1024, LNG(Kbytes));  //Kbytes/sec
		}
		strcat(progress, tmp);

		if (PasteProgress_f)  //if progress report was used earlier in this pasting
			nonDialog(NULL);  //order cleanup for that screen area
		nonDialog(progress);  //Make new progress report
		PasteProgress_f = 1;  //and note that it was done for next time
		drawMsg(file.name);
#ifdef DS34
		if (readpad() && new_pad) {
#else
		if (readpad()) {
#endif
			if (-1 == ynDialog(LNG(Continue_transfer))) {
				genClose(out_fd);
				out_fd = -1;
				if (PM_flag[recurses] != PM_PSU_BACKUP)
					genRemove(out);
				ret = -1;             // flag generic error
				goto copy_file_exit;  // go deal with it
			}
			}
			copyBuff = (buffIndex == 0) ? buff : buff2;
			bytesRead = genRead(in_fd, copyBuff, buffSize);
			bytesWritten = (bytesRead == buffSize) ? genWrite(out_fd, copyBuff, buffSize) : 0;
#if FILEOP_TRACE
			chunk_remaining_before = size;
			if (trace_net_copy || trace_vmc_copy) {
				printf("[FILEOP] copy-chunk in=%s out=%s idx=%u buf=%d req=%d read=%d write=%d remain_before=%llu\n",
				       in, out, trace_chunk_index, buffIndex, buffSize, bytesRead, bytesWritten,
				       (unsigned long long)chunk_remaining_before);
			}
#endif
			if ((bytesRead != buffSize) || (bytesWritten != buffSize)) {
#if FILEOP_TRACE
				printf("[FILEOP] copy-chunk-error in=%s out=%s idx=%u req=%d read=%d write=%d remain=%llu\n",
				       in, out, trace_chunk_index, buffSize, bytesRead, bytesWritten,
				       (unsigned long long)size);
#endif
				genClose(out_fd);
				out_fd = -1;
				if (PM_flag[recurses] != PM_PSU_BACKUP)
					genRemove(out);
				ret = -EIO;  // flag generic I/O error
				goto copy_file_exit;
			}
			size -= buffSize;
			written_size += buffSize;
			if (buffCount > 1)
				buffIndex ^= 1;
#if FILEOP_TRACE
			trace_chunk_index++;
			if (trace_net_copy || trace_vmc_copy) {
				printf("[FILEOP] copy-chunk-ok in=%s out=%s idx=%u wrote=%d remain_after=%llu\n",
				       in, out, trace_chunk_index, bytesWritten,
				       (unsigned long long)size);
			}
#endif
		}  // ends while(size>0), ----- The main copying loop ends here -----
	ret = 0;
#if FILEOP_TRACE
	if (trace_net_copy || trace_vmc_copy) {
		printf("[FILEOP] copy-complete in=%s out=%s chunks=%u total_written=%llu\n",
		       in, out, trace_chunk_index, (unsigned long long)written_size);
	}
#endif
copy_file_data_done:
	//Here the file has been copied. without error, as indicated by 'ret' above
	//but we also need to copy attributes and timestamps (as yet only for MC)
	//For PSU backup output padding may be needed, but not output file closure
	if (PM_flag[recurses] == PM_PSU_BACKUP) {
		if (psu_backup_write_padding(out_fd, psu_pad_size) < 0) {
			ret = -EIO;
			goto copy_file_exit;
		}
		out_fd = -1;  //prevent output file closure below
		goto copy_file_exit;
	}

	if (out_fd >= 0) {
		dummy = genClose(out_fd);
#if FILEOP_TRACE
		if (trace_vmc_copy)
			printf("[VMC_COPY] output close ret=%d out='%s' recurse=%d\n", dummy, out, recurses);
#endif
		if (dummy < 0)
			ret = dummy;
		out_fd = -1;  //prevent dual closure attempt
		if (ret < 0)
			goto copy_file_exit;
	}

	if (!strncmp(out, "mc", 2)) {                                 //Handle file copied to MC
		ret = MC_SFI;                                 //default request for changing entire mcTable
		if (!isMemoryCardLikePath(in)) {              //Handle file copied from non-MC/non-VMC to MC
			file.stats.AttrFile = MC_ATTR_norm_file;  //normalize MC file attribute
#if defined(ETH) || defined(UDPFS)
			if (!strncmp(in, "host", 4) || !strncmp(in, "udpfs", 5)) {  //Handle folder copied from host/udpfs to MC
				ret = 4;                              //request change only of main attribute for host:
			}                                         //ends host: source clause
#endif
		}                                             //ends non-MC/non-VMC source clause
		if (ret == MC_SFI)
			applyMcInfoToMcPath(out, &file.stats);
		else {
			mcGetInfo(out[2] - '0', 0, &mctype_PSx, &dummy, &dummy);  //Wakeup call & MC type check
			mcSync(0, NULL, &dummy);
			if (mctype_PSx == 2) {                        //if copying to a PS2 MC
				mcSetFileInfo(out[2] - '0', 0, &out[4], &file.stats, ret);
				mcSync(0, NULL, &dummy);
			}
		}
	} else if (!strncmp(out, "vmc", 3)) {             //Handle file copied to VMC
		applyMcInfoToVmcPath(out, &file.stats);
#ifdef XFROM
	} else if (!strncmp(out, "xfrom", 5)) {           //Handle file copied to XFROM
		if (!isMemoryCardLikePath(in))                //Handle file copied from non-MC/non-VMC/non-XFROM
			file.stats.AttrFile = MC_ATTR_norm_file;  //normalize MC file attribute
		applyMcInfoToXfromPath(out, &file.stats);
#endif
	} else {                                          //Handle file copied to non-MC
		if (!strncmp(out, "host", 4) || !strncmp(out, "udpfs", 5)) {  //for files copied to host/udpfs: we skip Chstat
		} else if (!strncmp(out, "mass", 4)) {  //for files copied to mass: we skip Chstat
#ifdef MX4SIO
			} else if (!strncmp(out, "mx4sio", 6)) {  //for files copied to mx4sio: we skip Chstat
#endif
			} else if (!strncmp(out, "ata", 3)) {  //for files copied to ata: we skip Chstat
			} else {                                //for other devices we use fileXio_ stuff
			memcpy(iox_stat.ctime, (void *)&file.stats._Create, 8);
			memcpy(iox_stat.mtime, (void *)&file.stats._Modify, 8);
			memcpy(iox_stat.atime, iox_stat.mtime, 8);
			ret = FIO_CST_CT | FIO_CST_AT | FIO_CST_MT;  //Request timestamp stat change
#if defined(ETH) || defined(UDPFS)
			if (!strncmp(in, "host", 4) || !strncmp(in, "udpfs", 5)) {  //Handle file copied from host/udpfs:
				ret = 0;                                 //Request NO stat change
			}
#endif
			dummy = fileXioChStat(out, &iox_stat, ret);
		}
	}

//The code below is also used for all errors in copying a file,
//but those cases are distinguished by a negative value in 'ret'
copy_file_exit:
	free(buff2);
	free(buff);
copy_file_exit_mem_err:
#if FILEOP_TRACE
	if (trace_vmc_copy && ret < 0) {
		printf("[VMC_COPY] exit-error ret=%d recurse=%d in='%s' out='%s' in_fd=%d out_fd=%d remaining=%llu chunks=%u\n",
		       ret, recurses, in, out, in_fd, out_fd, (unsigned long long)size, trace_chunk_index);
	}
#endif
	if (PM_flag[recurses] != PM_PSU_RESTORE) {  //Avoid closing PSU file here for PSU Restore
		if (in_fd >= 0) {
			genClose(in_fd);
		}
	}
	if (out_fd >= 0) {
		genClose(out_fd);
	}
	return ret;
}
//------------------------------
//endfunc copy
//--------------------------------------------------------------
