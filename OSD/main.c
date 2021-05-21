/**********************************************************************
 * 							PS2 APA header injection
 * 
 * 							alexparrado (2021)  
 * main.c
 * ********************************************************************/

//Header files
#include <kernel.h>
#include <fileXio_rpc.h>
#include <loadfile.h>
#include <fileio.h>
#include <iopheap.h>
#include <iopcontrol.h>
#include <fcntl.h>
#include <sbv_patches.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <fcntl.h>
#include <io_common.h>
#include <debug.h>

#include "hddosd-headers.h"

//Stack size for HDD modules thread
#define SYSTEM_INIT_THREAD_STACK_SIZE 0x1000

//Well known SIO2MAN module
extern unsigned char SIO2MAN_irx[];
extern unsigned int size_SIO2MAN_irx;

//Required module por gamepad
extern unsigned char PADMAN_irx[];
extern unsigned int size_PADMAN_irx;

//Required modules for HDD access
extern unsigned char IOMANX_irx[];
extern unsigned int size_IOMANX_irx;

extern unsigned char FILEXIO_irx[];
extern unsigned int size_FILEXIO_irx;

extern unsigned char DEV9_irx[];
extern unsigned int size_DEV9_irx;

extern unsigned char ATAD_irx[];
extern unsigned int size_ATAD_irx;

extern unsigned char HDD_irx[]; //!!!ps2hdd-osd
extern unsigned int size_HDD_irx;

extern unsigned char PFS_irx[];
extern unsigned int size_PFS_irx;

//Required modules for USB access
extern unsigned char USBD_irx[];
extern unsigned int size_USBD_irx;

extern unsigned char USBHDFSD_irx[];
extern unsigned int size_USBHDFSD_irx;

int file_exists(char *filepath)
{
	int fdn;

	scr_printf("%s\n", filepath);

	fdn = open(filepath, O_RDONLY);
	if (fdn < 0)
		return 0;
	close(fdn);

	return 1;
}

//To perform delay
void delay(u64 msec)
{
	u64 start;

	TimerInit();

	start = Timer();

	while (Timer() <= (start + msec))
		;

	TimerEnd();
}

struct SystemInitParams
{
	int InitCompleteSema;
	unsigned int flags;
};

//HDD modules load (FMCB installer source)
static void SystemInitThread(struct SystemInitParams *SystemInitParams)
{
	static const char PFS_args[] = "-n\0"
								   "24\0"
								   "-o\0"
								   "8";
	int i;

	if (SifExecModuleBuffer(ATAD_irx, size_ATAD_irx, 0, NULL, NULL) >= 0)
	{
		SifExecModuleBuffer(HDD_irx, size_HDD_irx, 0, NULL, NULL);
		SifExecModuleBuffer(PFS_irx, size_PFS_irx, sizeof(PFS_args), PFS_args, NULL);
	}

	SifExitIopHeap();
	SifLoadFileExit();

	SignalSema(SystemInitParams->InitCompleteSema);
	ExitDeleteThread();
}

//Function to create thread (FMCB installer source code)
int SysCreateThread(void *function, void *stack, unsigned int StackSize, void *arg, int priority)
{
	ee_thread_t ThreadData;
	int ThreadID;

	ThreadData.func = function;
	ThreadData.stack = stack;
	ThreadData.stack_size = StackSize;
	ThreadData.gp_reg = &_gp;
	ThreadData.initial_priority = priority;
	ThreadData.attr = ThreadData.option = 0;

	if ((ThreadID = CreateThread(&ThreadData)) >= 0)
	{
		if (StartThread(ThreadID, arg) < 0)
		{
			DeleteThread(ThreadID);
			ThreadID = -1;
		}
	}

	return ThreadID;
}

//IOP reset
void ResetIOP()
{

	SifInitRpc(0);
	while (!SifIopReset("", 0))
	{
	};
	while (!SifIopSync())
	{
	};
	SifInitRpc(0);
}

//PS2 initialization
void InitPS2()
{
	int ret;
	ee_sema_t sema;
	static unsigned char SysInitThreadStack[SYSTEM_INIT_THREAD_STACK_SIZE] __attribute__((aligned(16)));
	static struct SystemInitParams InitThreadParams;

	sema.init_count = 0;
	sema.max_count = 1;
	sema.attr = sema.option = 0;
	InitThreadParams.InitCompleteSema = CreateSema(&sema);
	InitThreadParams.flags = 0;

	ResetIOP();

	SifInitIopHeap();
	SifLoadFileInit();
	fioInit();

	sbv_patch_enable_lmb();
	sbv_patch_disable_prefix_check();

	SifExecModuleBuffer(IOMANX_irx, size_IOMANX_irx, 0, NULL, NULL);
	SifExecModuleBuffer(FILEXIO_irx, size_FILEXIO_irx, 0, NULL, NULL);

	fileXioInit();

	SifExecModuleBuffer(DEV9_irx, size_DEV9_irx, 0, NULL, &stat);
	SifExecModuleBuffer(SIO2MAN_irx, size_SIO2MAN_irx, 0, NULL, NULL);
	SifExecModuleBuffer(PADMAN_irx, size_PADMAN_irx, 0, NULL, NULL);
	SifExecModuleBuffer(USBD_irx, size_USBD_irx, 0, NULL, NULL);
	SifExecModuleBuffer(USBHDFSD_irx, size_USBHDFSD_irx, 0, NULL, NULL);

	SysCreateThread(SystemInitThread, SysInitThreadStack, SYSTEM_INIT_THREAD_STACK_SIZE, &InitThreadParams, 0x2);

	WaitSema(InitThreadParams.InitCompleteSema);

	PadInitPads();
}

//Test parameters
#define PARTY "hdd0:PP.SOFTDEV2.APPS"
#define SYSTEMCNF "mass:system.cnf"
#define ICONSYS "mass:icon.sys"
#define LISTICO "mass:list.ico"


//Main function
int main(int argc, char *argv[])
{

	header_info_t info;

	iox_stat_t statFile;

	InitPS2();
	init_scr();

	scr_printf("\n\nAPA headers demo by alexparrado\n\n");

	//Waits for USB device
	delay(5000);

	scr_printf("OK1\n");

	//Function argument setup
	info.systemCnf = SYSTEMCNF;
	info.iconSys = ICONSYS;
	info.listIco = LISTICO;
	info.partition = PARTY;

	scr_printf("Injecting to %s partition...\n",info.partition);

	//Starts header injection
	if (WriteAPAHeader(info) < 0)
		scr_printf("An error occurred when injecting header\n");
	else
	scr_printf("Header injection succeeded\n");

	scr_printf("Exiting\n");
	//fileXioUmount("pfs0:");

	SleepThread();

	return 0;
}
