//---------------------------------------------------------------------------
//File name:   main.c
//---------------------------------------------------------------------------
#include "launchelf.h"
#include "init.h"
#include "main_actions.h"
#include "main_boot.h"
#include "main_menu.h"
#include "main_modules.h"
#include "main_startup.h"

//#define DEBUG
#ifdef DEBUG
#define dbgprintf(args...) scr_printf(args)
#define dbginit_scr() init_scr()
#else
#define dbgprintf(args...) \
	do {                   \
	} while (0)
#define dbginit_scr() \
	do {              \
	} while (0)
#endif

int TV_mode;
char LaunchElfDir[MAX_PATH], LaunchElfBootDir[MAX_PATH], mainMsg[MAX_PATH];
char LoadedConfigPath[MAX_PATH], LoadedIPConfigPath[MAX_PATH];
char CNF[MAX_NAME];
int swapKeys;

u64 WaitTime;
u64 CurrTime;

char if_conf[IPCONF_MAX_LEN];
int if_conf_len;

char ip[16] = "192.168.0.10";
char netmask[16] = "255.255.255.0";
char gw[16] = "192.168.0.1";

char netConfig[IPCONF_MAX_LEN + 64];  //Adjust size as needed

#ifdef MX4SIO
u8 mx4sio_driver_running = 0;
#endif

u8 console_is_PSX = 0;
u8 boot_show_all_devices = 0;

static int boot_argc;
static char *boot_argv[8];
static char boot_path[MAX_PATH];

//Variables for SYSTEM.CNF processing
int BootDiscType = 0;
char SystemCnf_BOOT[MAX_PATH];
char SystemCnf_BOOT2[MAX_PATH];
char SystemCnf_VER[10];    //Arbitrary. Real value should always be shorter
char SystemCnf_VMODE[10];  //Arbitrary, same deal. As yet unused

char default_ESR_path[] = "mc:/BOOT/ESR.ELF";
char default_OSDSYS_path[30];
char default_OSDSYS_path2[30];

char ROMVER_data[16];  //16 byte file read from rom0:ROMVER at init
char rough_region;     //E==Europe, A==US, I==Japan, H==Asia, C==China

int cdmode;      //Last detected disc type
int old_cdmode;  //used for disc change detection
int uLE_cdmode;  //used for smart disc detection

typedef struct
{
	int type;
	char name[16];
} DiscType;

DiscType DiscTypes[] = {
    {SCECdNODISC, "!"},
    {SCECdDETCT, "??"},
    {SCECdDETCTCD, "CD ?"},
    {SCECdDETCTDVDS, "DVD-SL ?"},
    {SCECdDETCTDVDD, "DVD-DL ?"},
    {SCECdUNKNOWN, "Unknown"},
    {SCECdPSCD, "PS1 CD"},
    {SCECdPSCDDA, "PS1 CDDA"},
    {SCECdPS2CD, "PS2 CD"},
    {SCECdPS2CDDA, "PS2 CDDA"},
    {SCECdPS2DVD, "PS2 DVD"},
    {SCECdESRDVD_0, "ESR DVD (off)"},
    {SCECdESRDVD_1, "ESR DVD (on)"},
    {SCECdCDDA, "Audio CD"},
    {SCECdDVDV, "Video DVD"},
    {SCECdIllegalMedia, "Unsupported"},
    {0x00, ""}  //end of list
};              //ends DiscTypes array

//Static function declarations
static void Execute(char *pathin);
//---------------------------------------------------------------------------
//executable code
//---------------------------------------------------------------------------
// Execute. Execute an action. May be called recursively.
// For any path specified, its device must be accessible.
//------------------------------
static void Execute(char *pathin)
{
	MainExecuteContext exec_ctx;

	exec_ctx.boot_argc = boot_argc;
	exec_ctx.boot_argv = boot_argv;
	exec_ctx.boot_path = boot_path;
	exec_ctx.main_msg = mainMsg;
	exec_ctx.cnf_path = CNF;
	exec_ctx.default_esr_path = default_ESR_path;
	exec_ctx.default_osdsys_path = default_OSDSYS_path;
	exec_ctx.default_osdsys_path2 = default_OSDSYS_path2;
	exec_ctx.rough_region = rough_region;
	exec_ctx.romver_data = ROMVER_data;

	ExecuteMainAction(pathin, &exec_ctx);
}
//------------------------------
//endfunc Execute
//---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	int event, post_event = 0;
	char RunPath[MAX_PATH];
	int nElfs = 0;
	enum BOOT_DEVICE boot;
	int CNF_error = -1;  //assume error until CNF correctly loaded
	MainMenuState menu_state;

	captureBootArguments(argc, argv, &boot_argc, boot_argv, 8);
	boot = performEarlyBootInitialization((argc > 0) ? argv[0] : NULL, boot_path, sizeof(boot_path), mainMsg, CNF, sizeof(CNF), &CNF_error);

	MainMenuState_Init(&menu_state);
	initializeRuntimeInputModules(boot);
	MainMenuState_BeginTimers(&menu_state);
	initializeRuntimeDisplayModules(CNF_error, mainMsg);

	//Here nearly everything is ready for the main menu event loop
	//But before we start that, we need to validate CNF_Path
	validateConfiguredCnfPath();

	RunPath[0] = 0;  //Nothing to run yet
	cdmode = -1;     //flag unchecked cdmode state
	event = 1;       //event = initial entry
	DPRINTF("starting main menu event loop\n");
	//----- Start of main menu event loop -----
	while (1) {
		int DiscType_ix;

		//Background event section
		uLE_cdStop();              //Test disc state and if needed stop disc (updates cdmode)
		if (cdmode == old_cdmode)  //if disc detection did not change state
			goto done_discControl;

		event |= 4;  //event |= disc change detection
		if (cdmode <= 0)
			sprintf(mainMsg, "%s ", LNG(No_Disc));
		else if (cdmode >= 1 && cdmode <= 4)
			sprintf(mainMsg, "%s == ", LNG(Detecting_Disc));
		else  //if(cdmode>=5)
			sprintf(mainMsg, "%s == ", LNG(Stop_Disc));

		DiscType_ix = 0;
		{
			int i;
			for (i = 0; DiscTypes[i].name[0]; i++)
				if (DiscTypes[i].type == uLE_cdmode)
					DiscType_ix = i;
		}

		sprintf(mainMsg + strlen(mainMsg), DiscTypes[DiscType_ix].name);
		//Comment out the debug output below when not needed
		/*
			sprintf(mainMsg+strlen(mainMsg),
				"  cdmode==%d  uLE_cdmode==%d  type_ix==%d",
				cdmode, uLE_cdmode, DiscType_ix);
			//*/
		done_discControl:
		MainMenuState_UpdateTimers(&menu_state, &event);

		//Display section
		if (event || post_event) {  //NB: We need to update two frame buffers per event
			nElfs = drawMainMenuScreen(&menu_state, mainMsg);
		}
		drawScr();
		post_event = event;
		event = 0;

		MainMenuHandlePad(&menu_state, nElfs, RunPath, sizeof(RunPath), &event);
		MainMenuHandleAutoLaunch(&menu_state, RunPath, sizeof(RunPath), &event);

		if (RunPath[0]) {
			MainMenuMarkActionTaken(&menu_state);
			Execute(RunPath);
			RunPath[0] = 0;
		}
	}  //ends while(1)
	   //----- End of main menu event loop -----
}
//------------------------------
//endfunc main
//---------------------------------------------------------------------------
//End of file: main.c
//---------------------------------------------------------------------------
