//--------------------------------------------------------------
//File name:   main_boot.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "init.h"
#include "main_boot.h"

void captureBootArguments(int argc, char *argv[], int *boot_argc, char *boot_argv[], int boot_argv_cap)
{
	int i;

	if (boot_argc == NULL || boot_argv == NULL || boot_argv_cap <= 0)
		return;

	*boot_argc = argc;
	for (i = 0; (i < argc) && (i < boot_argv_cap); i++)
		boot_argv[i] = argv[i];
}

static void mapDefaultFileBrowserLaunchKeyToOkButton(void)
{
	int ok_lk;

	if (setting == NULL)
		return;

	ok_lk = setting->swapKeys ? SETTING_LK_CROSS : SETTING_LK_CIRCLE;
	setting->LK_Path[SETTING_LK_CIRCLE][0] = '\0';
	setting->LK_Flag[SETTING_LK_CIRCLE] = 0;
	setting->LK_Path[SETTING_LK_CROSS][0] = '\0';
	setting->LK_Flag[SETTING_LK_CROSS] = 0;
	strcpy(setting->LK_Path[ok_lk], setting->Misc_FileBrowser);
	setting->LK_Flag[ok_lk] = 1;
}

enum BOOT_DEVICE performEarlyBootInitialization(const char *arg0, char *boot_path, size_t boot_path_len, char *main_msg, char *cnf_path, size_t cnf_path_len, int *cnf_error)
{
	enum BOOT_DEVICE boot;
	int local_cnf_error;
	char local_cnf_path[MAX_NAME];
	char *cnf_path_buf;
	size_t cnf_path_buf_len;

	Reset();
	Init_Default_Language();
	if (wleExists("rom0:PSXVER")) {
		console_is_PSX = 1;
		DPRINTF("# Console is PSX-DESR\n");
	}
	boot = prepareBootDeviceAndPath(arg0, boot_path, boot_path_len);
	bringUpBootDeviceStack(boot);
	initializeBootDisplayDefaults();

	cnf_path_buf = cnf_path;
	cnf_path_buf_len = cnf_path_len;
	if (cnf_path_buf == NULL || cnf_path_buf_len == 0) {
		cnf_path_buf = local_cnf_path;
		cnf_path_buf_len = sizeof(local_cnf_path);
	}
	snprintf(cnf_path_buf, cnf_path_buf_len, "%s", "LAUNCHELF.CNF");
	local_cnf_error = loadConfig(main_msg, cnf_path_buf);
	if (local_cnf_error < 0) {
		/* No config loaded: default pad mapping from ROM region.
		 * ROMVER_data[4] is the region letter.
		 * J/C => Circle=OK/FileBrowser, Cross=Cancel (swapKeys=FALSE)
		 * others => Cross=OK/FileBrowser, Circle=Cancel (swapKeys=TRUE)
		 */
		if (ROMVER_data[0] == '\0')
			uLE_InitializeRegion();
		setting->swapKeys = ((ROMVER_data[4] == 'J') || (ROMVER_data[4] == 'C')) ? FALSE : TRUE;
		mapDefaultFileBrowserLaunchKeyToOkButton();
	}
	bringUpBootNetworkStack(boot);
	initializeBootGraphics();

	swapKeys = setting->swapKeys;
	if (cnf_error != NULL)
		*cnf_error = local_cnf_error;

	return boot;
}
