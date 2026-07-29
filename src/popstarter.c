//--------------------------------------------------------------
// File name:   popstarter.c
//--------------------------------------------------------------
#include "popstarter_internal.h"
#include "main_gameid.h"

static void setPopstarterMessage(char *message, size_t message_size, int result, const char *vcd_path, const char *popstarter_path)
{
	if (message == NULL || message_size == 0)
		return;

	switch (result) {
		case POPSTARTER_ERR_NOT_VCD:
		case POPSTARTER_ERR_INVALID_VCD:
			snprintf(message, message_size, "This file isn't a POPS VCD: %s", vcd_path);
			break;
		case POPSTARTER_ERR_UNSUPPORTED_PATH:
			snprintf(message, message_size, "POPStarter does not support that VCD path: %s", vcd_path);
			break;
		case POPSTARTER_ERR_OPEN_VCD:
			snprintf(message, message_size, "Cannot open POPS VCD: %s", vcd_path);
			break;
		case POPSTARTER_ERR_OPEN_POPSTARTER:
			snprintf(message, message_size, "Cannot find POPStarter ELF: %s", popstarter_path);
			break;
		case POPSTARTER_ERR_INVALID_POPSTARTER:
			snprintf(message, message_size, "POPStarter ELF is not an ELF: %s", popstarter_path);
			break;
		default:
			snprintf(message, message_size, "POPStarter launch failed: %s", vcd_path);
			break;
	}
	message[message_size - 1] = '\0';
}

int IsPopstarterVcdPath(const char *path)
{
	return (path != NULL && genCmpFileExt(path, "VCD"));
}

int LaunchPopstarterVcd(const char *path, char *message, size_t message_size)
{
	char arg0[MAX_PATH];
	char launch_gameid[12];
	POPSTARTER_CANDIDATE_LIST popstarter_candidates;
	char popstarter_path[MAX_PATH];
	char popstarter_fullpath[MAX_PATH];
	char popstarter_party[MAX_PATH];
	int exec_kind;
	int have_launch_gameid;
	int reboot_iop_elf_load;
	int show_launch_gameid;
	int ret;

	arg0[0] = '\0';
	launch_gameid[0] = '\0';
	popstarter_path[0] = '\0';
	popstarter_fullpath[0] = '\0';
	popstarter_party[0] = '\0';
	exec_kind = 1;
	have_launch_gameid = 0;
	reboot_iop_elf_load = 0;
	show_launch_gameid = 0;
	initPopstarterCandidates(&popstarter_candidates);

	ret = prepareLaunch(path, arg0, sizeof(arg0), &popstarter_candidates);
	if (ret < 0)
		goto fail;

	ret = validateVcd(path);
	if (ret < 0)
		goto fail;

	have_launch_gameid = buildPopstarterVcdGameID(path, launch_gameid, sizeof(launch_gameid));

	ret = preparePopstarterCandidateLaunch(&popstarter_candidates, popstarter_path, sizeof(popstarter_path),
	                                       popstarter_fullpath, sizeof(popstarter_fullpath),
	                                       popstarter_party, sizeof(popstarter_party), &exec_kind);
	if (ret < 0)
		goto fail;

	if (setting != NULL) {
		reboot_iop_elf_load = setting->reboot_iop_elf_load;
		show_launch_gameid = have_launch_gameid && !setting->cdrom_disable_gameid;
	} else
		show_launch_gameid = have_launch_gameid;
	if (isHddDevicePath(path) || isHddPfsPath(path) || isHddPartyPath(popstarter_party))
		unmountAll();
	CleanUpForExec();
	if (show_launch_gameid)
		displayRetroGemGameID(launch_gameid, 2);
	RunLoaderElf(popstarter_fullpath, popstarter_party, arg0, exec_kind, reboot_iop_elf_load);
	return POPSTARTER_OK;

fail:
	setPopstarterMessage(message, message_size, ret, path, popstarter_path);
	return ret;
}

//--------------------------------------------------------------
// End of file: popstarter.c
//--------------------------------------------------------------
