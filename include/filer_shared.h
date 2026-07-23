#ifndef FILER_SHARED_H
#define FILER_SHARED_H

#include "launchelf.h"

typedef struct
{
	unsigned char unknown;
	unsigned char sec;    // date/time (second)
	unsigned char min;    // date/time (minute)
	unsigned char hour;   // date/time (hour)
	unsigned char day;    // date/time (day)
	unsigned char month;  // date/time (month)
	unsigned short year;  // date/time (year)
} PS2TIME __attribute__((aligned(2)));

enum {
	COPY,
	CUT,
	PASTE,
	PSUPASTE,
	DELETE,
	RENAME,
	NEWDIR,
	NEWICON,
	MOUNTVMC0,
	MOUNTVMC1,
	GETSIZE,
	OPEN_TEXTEDITOR,
	LAUNCH_ELF_ARGS,
	TIMEMANIP,
	TITLE_CFG,
	NUM_MENU
};

#define PM_NORMAL 0       //PasteMode value for normal copies
#define PM_PSU_BACKUP 1   //PasteMode value for gamesave backup from MC to PSU
#define PM_PSU_RESTORE 2  //PasteMode value for gamesave restore to MC from PSU
#define PM_RENAME 3       //PasteMode value for normal copies with new names
#define MAX_RECURSE 16    //Maximum folder recursion for copy/PSU operations

#define TRANSFER_STACK_READY 0
#define TRANSFER_STACK_LOAD_FAILED -1
#define TRANSFER_STACK_INCOMPATIBLE -2

extern int PasteProgress_f;
extern int PasteMode;
extern int PM_flag[MAX_RECURSE];
extern int PM_file[MAX_RECURSE];

extern unsigned char *elisaFnt;
extern int elisa_failed;
extern u64 freeSpace;
extern int mcfreeSpace;
extern int mctype_PSx;
extern int vfreeSpace;
extern int browser_cut;
extern int nclipFiles;
extern int nmarks;
extern unsigned int clipIopResetGeneration;
extern u64 written_size;
extern u64 PasteTime;
extern int PSU_content;
extern int fileMode;

extern int file_show;
extern int file_sort;
extern int size_valid;
extern int time_valid;

extern char cnfmode_extU[CNFMODE_CNT][4];
extern char cnfmode_extL[CNFMODE_CNT][4];
extern char clipPath[MAX_PATH];
extern char LastDir[MAX_NAME];
extern char marks[MAX_ENTRY];
extern FILEINFO clipFiles[MAX_ENTRY];

#if defined(ETH) || defined(UDPFS)
extern int host_elflist;
#endif
#ifdef UDPFS
extern int udpfs_dir_open_failed;
#endif

int getDir(const char *path, FILEINFO *info);
int pollRootMemoryCardDevices(void);
void discardNextMx4sioRootListing(const char *path);
int ensurePathDeviceStackReady(const char *path);
int prepareTransferDeviceStacks(const char *src_path, const char *dst_path);
int copy(char *outPath, const char *inPath, FILEINFO file, int recurses);

#endif
