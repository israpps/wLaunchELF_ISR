//--------------------------------------------------------------
//File name:   main_modules.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "init.h"
#include "main_modules.h"

void validateConfiguredCnfPath(void)
{
	char cnf_path[MAX_PATH];

	if (setting->CNF_Path[0] != '\0') {
		if (genFixPath(setting->CNF_Path, cnf_path) >= 0)
			strcpy(LaunchElfDir, setting->CNF_Path);
	}
}

void initializeRuntimeInputModules(enum BOOT_DEVICE boot_device)
{
	//It's time to load and init drivers
	DPRINTF("Getting IPCONFIG\n");
	if (boot_device != BOOT_DEVICE_HOST)
		getIpConfig();

	WaitTime = Timer();
#ifdef DS34
	loadDs34InputModules();
#endif
	DPRINTF("setup pad\n");
	setupPad();  //Comment out this line when using early setupPad above
	waitAnyPadReady();
	if (readpad_no_KB() && (new_pad & PAD_SELECT))
		boot_show_all_devices = 1;
	else {
		/* Keep non-Select boot-held launch keys visible to the main menu. */
		clearPadPressState();
	}
	DPRINTF("Starting keyboard\n");
	startKbd();
	WaitTime = Timer();
}

void initializeRuntimeDisplayModules(int cnf_error, char *main_msg)
{
	Load_External_Language();
	loadConfiguredBootFont();

	gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x00, 0x00));
	if (cnf_error < 0)
		sprintf(main_msg, "%s", LNG(Failed_To_Load));
	else
		sprintf(main_msg, "%s", LNG(Loaded_Config));
}
