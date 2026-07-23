//--------------------------------------------------------------
//File name:    hdd.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "hdd_header_injector.h"

#define MAX_PARTGB 128                  //Partition MAX in GB
#define MAX_PARTMB (MAX_PARTGB * 1024)  //Partition MAX in MB

typedef struct
{
	char Name[MAX_PART_NAME + 1];
	u64 RawSize;
	u64 TotalSize;
	u64 FreeSize;
	u64 UsedSize;
	GameInfo Game;  //Intended for HDL game info, implemented through IRX module
	int Count;
	int Treatment;
} PARTYINFO;

enum {               //For Treatment codes
	TREAT_PFS = 0,   //PFS partition for normal file access
	TREAT_HDL_RAW,   //HDL game partition before reading GameInfo data
	TREAT_HDL_GAME,  //HDL game partition after reading GameInfo data
	TREAT_SYSTEM,    //System partition that must never be modified (__mbr)
	TREAT_NOACCESS   //Inaccessible partition, in fact all other partitions
};

enum {  //For menu commands
	CREATE = 0,
	REMOVE,
	RENAME,
	INJECT_HEADER,
	EXPAND,
	FORMAT,
	NUM_MENU
};

enum {
	HEADER_INJECT_THIS_PARTITION = 0,
	HEADER_INJECT_MATCHING_PARTITIONS,
	HEADER_INJECT_MATCHING_CREATE_MISSING,
	HEADER_INJECT_CANCEL
};

enum {
	HEADER_CREATE_PROMPT_CANCEL = -1,
	HEADER_CREATE_PROMPT_SKIP = 0,
	HEADER_CREATE_PROMPT_CREATE = 1
};

/* EXPAND remains implemented below, but hidden until APA chain expansion is safe. */
static const int hdd_party_menu_items[] = {
	CREATE,
	REMOVE,
	RENAME,
	INJECT_HEADER,
	FORMAT,
};

#define SECTORS_PER_MB 2048  //Divide by this to convert from sector count to MB
#define MB 1048576
#define HDD_PARTITION_CREATE_CANCELLED (-EINTR)

/* local fallback for newer SDKs that do not provide floatlib degree trig helpers */
#ifndef WLE_DEG_TO_RADF
#define WLE_DEG_TO_RADF(_deg) ((float)(_deg) * 0.01745329251994329577f)
#endif
#ifndef cosdgf
#define cosdgf(_deg) cosf(WLE_DEG_TO_RADF(_deg))
#endif
#ifndef sindgf
#define sindgf(_deg) sinf(WLE_DEG_TO_RADF(_deg))
#endif

static PARTYINFO PartyInfo[MAX_PARTITIONS];
static int numParty;
static u32 hddSize, hddFree, hddFreeSpace, hddUsed;
static int hddConnected, hddFormated, hddRealStatus;
static HddHeaderSourcePartition hdd_header_source_partitions[MAX_PARTITIONS];

static int HddConfirmTwoButtonDialog(const char *message, const char *decline_label)
{
	char msg[512];
	int dh, dw, dx, dy;
	int sel = 0, a = 6, b = 4, c = 2, n, tw;
	int i, len, ret;
	int left_x, right_x, y;
	int button_width;
	int event, post_event = 0;

	snprintf(msg, sizeof(msg), "%s", message);
	msg[sizeof(msg) - 1] = '\0';

	for (i = 0, n = 1; msg[i] != 0; i++) {
		if (msg[i] == '\n') {
			msg[i] = 0;
			n++;
		}
	}
	for (i = len = tw = 0; i < n; i++) {
		ret = printXY(&msg[len], 0, 0, 0, FALSE, 0);
		if (ret > tw)
			tw = ret;
		len += strlen(&msg[len]) + 1;
	}

	button_width = (strlen(LNG(OK)) + strlen(decline_label) + 6) * FONT_WIDTH;
	if (tw < button_width)
		tw = button_width;

	dh = FONT_HEIGHT * (n + 1) + 2 * 2 + a + b + c;
	dw = 2 * 2 + a * 2 + tw;
	dx = (SCREEN_WIDTH - dw) / 2;
	dy = (SCREEN_HEIGHT - dh) / 2;
	left_x = dx + a + FONT_WIDTH;
	right_x = dx + dw - a - (strlen(decline_label) + 1) * FONT_WIDTH;
	y = dy + a + b + 2 + n * FONT_HEIGHT;

	event = 1;
	while (1) {
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_LEFT) {
				event |= 2;
				sel = 0;
			} else if (new_pad & PAD_RIGHT) {
				event |= 2;
				sel = 1;
			} else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				ret = -1;
				break;
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
				ret = (sel == 0) ? 1 : -1;
				break;
			}
		}

		if (event || post_event) {
			drawPopSprite(setting->color[COLOR_BACKGR],
			              dx, dy,
			              dx + dw, (dy + dh));
			drawFrame(dx, dy, dx + dw, (dy + dh), setting->color[COLOR_FRAME]);
			for (i = len = 0; i < n; i++) {
				printXY(&msg[len], dx + 2 + a, (dy + a + 2 + i * FONT_HEIGHT), setting->color[COLOR_TEXT], TRUE, 0);
				len += strlen(&msg[len]) + 1;
			}

			printXY(LNG(OK), left_x, y, setting->color[COLOR_TEXT], TRUE, 0);
			printXY(decline_label, right_x, y, setting->color[COLOR_TEXT], TRUE, 0);
			if (sel == 0)
				drawChar(LEFT_CUR, left_x - FONT_WIDTH, y, setting->color[COLOR_TEXT]);
			else
				drawChar(LEFT_CUR, right_x - FONT_WIDTH - 1, y, setting->color[COLOR_TEXT]);
		}
		drawLastMsg();
		post_event = event;
		event = 0;
	}
	drawSprite(setting->color[COLOR_BACKGR], dx, dy, dx + dw + 1, (dy + dh) + 1);
	drawScr();
	drawSprite(setting->color[COLOR_BACKGR], dx, dy, dx + dw + 1, (dy + dh) + 1);
	drawScr();
	return ret;
}

static int ConfirmProtectedPartitionCreate(const char *party, int skip_instead_of_cancel)
{
	if (party == NULL || strncmp(party, "__", 2))
		return 1;

	if (skip_instead_of_cancel)
		return (HddConfirmTwoButtonDialog(LNG(Protected_Partition_Create_Warning), LNG(Skip)) == 1);

	return (ynDialog(LNG(Protected_Partition_Create_Warning)) == 1);
}

static char DbgMsg[MAX_TEXT_LINE * 30];

typedef struct
{
	int Code;
	const char *ShortText;
	const char *InfoText;
} HDDSTATUSINFO;

static const HDDSTATUSINFO HddStatusInfo[] = {
	{3, "NO HDD", "No suitable HDD was detected by the HDD driver.\nCheck the drive, adapter, power, DEV9, or ATA stack."},
	{2, "LOCKED", "The HDD was detected, but the driver could not\ncomplete the unlock/usability check."},
	{1, "NO APA", "The HDD is usable, but no APA format was found.\nFormat the HDD before creating partitions."},
	{0, "READY", "The HDD is detected, usable, and APA formatted."},
	{-1, "EPERM", "Operation not permitted.\nThe HDD driver refused the status operation."},
	{-2, "ENOENT", "The HDD device/path was not found.\nhdd0: may not be registered by the driver."},
	{-3, "ESRCH", "A required service or RPC target was not found.\nHDD modules may not be running correctly."},
	{-5, "EIO", "I/O error while querying the HDD.\nCheck the drive, adapter, cabling, and power."},
	{-6, "ENXIO", "No such device or address.\nThe HDD hardware may be missing or unreachable."},
	{-11, "EAGAIN", "The HDD resource is temporarily unavailable.\nRetry after the HDD stack settles."},
	{-12, "ENOMEM", "Out of memory while querying HDD status.\nSystem or module memory may be exhausted."},
	{-13, "EACCES", "Access denied while querying HDD status.\nThe device or driver state refused access."},
	{-16, "EBUSY", "The HDD/device resource is busy.\nAnother operation or mount may be holding it."},
	{-19, "ENODEV", "No such device.\nHDD driver/device registration may be missing."},
};

static const HDDSTATUSINFO *GetHddStatusInfo(int status)
{
	int i;

	for (i = 0; i < (int)(sizeof(HddStatusInfo) / sizeof(HddStatusInfo[0])); i++) {
		if (HddStatusInfo[i].Code == status)
			return &HddStatusInfo[i];
	}

	return NULL;
}

static const char *GetHddStatusShortText(int status)
{
	const HDDSTATUSINFO *info = GetHddStatusInfo(status);

	return (info != NULL) ? info->ShortText : "UNKNOWN";
}

void DebugDisp(char *Message);

static void ShowHddStatusInfo(void)
{
	const HDDSTATUSINFO *info = GetHddStatusInfo(hddRealStatus);
	char message[MAX_TEXT_LINE * 30];

	if (info != NULL) {
		snprintf(message, sizeof(message), "%s\nNumber: %d\nStatus: %s\n\n%s",
		         LNG(HDD_STATUS), hddRealStatus, info->ShortText, info->InfoText);
	} else {
		snprintf(message, sizeof(message), "%s\nNumber: %d\nStatus: UNKNOWN\n\nNo specific HDD status description is available\nfor this code.",
		         LNG(HDD_STATUS), hddRealStatus);
	}

	DebugDisp(message);
}

//--------------------------------------------------------------
///*
void DebugDisp(char *Message)
{
	int done;

	for (done = 0; done == 0;) {
		nonDialog(Message);
		drawLastMsg();
		if (readpad() && new_pad) {
			done = 1;
		}
	}
}
//*/
//------------------------------
//endfunc DebugDisp
//--------------------------------------------------------------
/*
void DebugDispStat(iox_dirent_t *p)
{
	char buf[MAX_TEXT_LINE*24]="";
	int  stp, pos, i;
	unsigned int *ip = &p->stat.private_0;
	int *cp = (int *) p->stat.ctime;
	int *ap = (int *) p->stat.atime;
	int *mp = (int *) p->stat.mtime;

	pos = 0;
	sprintf(buf+pos,"dirent.name == \"%s\"\n%n", p->name, &stp); pos+=stp;
	sprintf(buf+pos,"dirent.unknown == %d\n%n", p->unknown, &stp); pos+=stp;
	sprintf(buf+pos,"  mode == 0x%08X, attr == 0x%08X\n%n",p->stat.mode,p->stat.attr,&stp); pos+=stp;
	sprintf(buf+pos,"hisize == 0x%08X, size == 0x%08X\n%n",p->stat.hisize,p->stat.size,&stp); pos+=stp;
	sprintf(buf+pos,"ctime == 0x%08X%08X                      \n%n", cp[1],cp[0], &stp); pos+=stp;
	sprintf(buf+pos,"atime == 0x%08X%08X\n%n", ap[1],ap[0], &stp); pos+=stp;
	sprintf(buf+pos,"mtime == 0x%08X%08X\n%n", mp[1],mp[0], &stp); pos+=stp;
	for(i=0; i<6; i++){
		sprintf(buf+pos,"private_%d == 0x%08X\n%n", i, ip[i], &stp); pos+=stp;
	}
	DebugDisp(buf);
}
//*/
//------------------------------
//endfunc DebugDispStat
//--------------------------------------------------------------
void GetHddInfo(void)
{
	iox_dirent_t infoDirEnt;
	int rv, hddFd = 0, partitionFd, i, Treat = TREAT_NOACCESS, tooManyPartitions;
	u32 size = 0, zoneFree, zoneSize;
	char tmp[MAX_PATH];
	char dbgtmp[MAX_PATH];
	//	char pfs_str[6];

	//	strcpy(pfs_str, "pfs0:");
	hddSize = 0, hddFree = 0, hddFreeSpace = 0, hddUsed = 0;
	hddConnected = 0, hddFormated = 0;
	numParty = 0;
	tooManyPartitions = 0;

	drawMsg(LNG(Reading_HDD_Information));
	hddRealStatus = fileXioDevctl("hdd0:", HDIOC_STATUS, NULL, 0, NULL, 0);
	DPRINTF("HDIOC_STATUS: %d\n", hddRealStatus);
	if (hddCheckPresent() < 0) {
		hddConnected = 0;
		goto end;
	} else
		hddConnected = 1;

	if (hddCheckFormatted() < 0) {
		hddFormated = 0;
		goto end;
	} else
		hddFormated = 1;

	hddSize = (u32)fileXioDevctl("hdd0:", HDIOC_TOTALSECTOR, NULL, 0, NULL, 0) / SECTORS_PER_MB;

	if ((hddFd = fileXioDopen("hdd0:")) < 0)
		return;

	while (fileXioDread(hddFd, &infoDirEnt) > 0) {  //Starts main while loop
		u64 found_size = infoDirEnt.stat.size / SECTORS_PER_MB;

		//DebugDispStat(&infoDirEnt);

		if (infoDirEnt.stat.mode == APA_TYPE_FREE)
			continue;
		hddUsed += found_size;
		for (i = 0; i < numParty; i++)  //Check with previous partitions
			if (!strcmp(infoDirEnt.name, PartyInfo[i].Name))
				break;
		if (i < numParty) {                      //found another reference to an old name
			PartyInfo[i].RawSize += found_size;  //Add new segment to old size
		} else {                                 //Starts clause for finding brand new name for PartyInfo[numParty]
			if (numParty >= MAX_PARTITIONS) {
				tooManyPartitions = 1;
				break;
			}

			sprintf(dbgtmp, "%s \"%s\"", LNG(Found), infoDirEnt.name);
			drawMsg(dbgtmp);

			memset(&PartyInfo[numParty], 0, sizeof(PARTYINFO));
			snprintf(PartyInfo[numParty].Name, sizeof(PartyInfo[numParty].Name), "%.31s", infoDirEnt.name);
			PartyInfo[numParty].RawSize = found_size;  //Store found segment size
			PartyInfo[numParty].Count = numParty;

			if (infoDirEnt.stat.mode == APA_TYPE_MBR)  //New test of partition type by 'mode'
				Treat = TREAT_SYSTEM;
			else if (infoDirEnt.stat.mode == 0x1337)
				Treat = TREAT_HDL_RAW;
			else if (infoDirEnt.stat.mode == APA_TYPE_PFS)
				Treat = TREAT_PFS;
			else
				Treat = TREAT_NOACCESS;
			PartyInfo[numParty].Treatment = Treat;

			if (Treat == TREAT_PFS) {  //Starts clause for TREAT_PFS
				sprintf(tmp, "hdd0:%s", PartyInfo[numParty].Name);
				partitionFd = fileXioOpen(tmp, FIO_O_RDONLY, 0);
				if (partitionFd >= 0) {
					for (i = 0, size = 0; i < infoDirEnt.stat.private_0 + 1; i++) {
						rv = fileXioIoctl2(partitionFd, HIOCGETSIZE, &i, 4, NULL, 0);
						size += (u32)rv / SECTORS_PER_MB;
					}
					PartyInfo[numParty].TotalSize = size;
					//			PartyInfo[numParty].RawSize = size;
					fileXioClose(partitionFd);


					//			mountParty(tmp);
					//			pfs_str[3] = '0'+latestMount;
					fileXioUmount("pfs0:");
					if (fileXioMount("pfs0:", tmp, FIO_MT_RDONLY) < 0)
						PartyInfo[numParty].Treatment = TREAT_NOACCESS;  //non-pfs partition
					else {
						zoneFree = (u32)fileXioDevctl("pfs0:", PDIOC_ZONEFREE, NULL, 0, NULL, 0);
						zoneSize = (u32)fileXioDevctl("pfs0:", PDIOC_ZONESZ, NULL, 0, NULL, 0);

						PartyInfo[numParty].FreeSize = (u64)zoneFree * zoneSize / MB;
						PartyInfo[numParty].UsedSize = PartyInfo[numParty].TotalSize - PartyInfo[numParty].FreeSize;
					}
				} else {  //Ends clause for partitionFd >= 0
					PartyInfo[numParty].Treatment = TREAT_NOACCESS;
					PartyInfo[numParty].FreeSize = 0;
					PartyInfo[numParty].UsedSize = 0;
				}
			}  //Ends clause for TREAT_PFS

			numParty++;
		}  //Ends clause for finding brand new name for PartyInfo[numParty]
	}      //ends main while loop
	fileXioDclose(hddFd);
	hddFreeSpace = (hddSize - hddUsed) & 0x7FFFFF80;  //free space rounded to useful area
	hddFree = (hddFreeSpace * 100) / hddSize;         //free space percentage

end:
	drawMsg(tooManyPartitions ? LNG(HDD_Information_Read_Overflow) : LNG(HDD_Information_Read));

	WaitTime = Timer();
	while (Timer() < WaitTime + 1500)
		;  // print operation result during 1.5 sec.
}
//------------------------------
//endfunc GetHddInfo
//--------------------------------------------------------------
int sizeSelector(int size)
{
	int x, y;
	int saveSize = size;
	float scrollBar;
	char c[MAX_PATH];
	int event, post_event = 0;

	int mSprite_X1 = SCREEN_WIDTH / 2 - 12 * FONT_WIDTH;   //Left edge of sprite
	int mSprite_Y1 = SCREEN_HEIGHT / 2 - 3 * FONT_HEIGHT;  //Top edge of sprite
	int mSprite_X2 = SCREEN_WIDTH / 2 + 12 * FONT_WIDTH;   //Right edge of sprite
	int mSprite_Y2 = SCREEN_HEIGHT / 2 + 3 * FONT_HEIGHT;  //Bottom edge of sprite

	event = 1;  //event = initial entry
	while (1) {
		//Pad response section
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_RIGHT) {
				event |= 2;  //event |= valid pad command
				if ((size += 128) > MAX_PARTMB)
					size = MAX_PARTMB;
			} else if (new_pad & PAD_LEFT) {
				event |= 2;  //event |= valid pad command
				if ((size -= 128) < saveSize)
					size = saveSize;
			} else if (new_pad & PAD_R1) {
				event |= 2;  //event |= valid pad command
				if (size < 1024)
					size = 1024;
				else {
					if ((size += 1024) > MAX_PARTMB)
						size = MAX_PARTMB;
				}
			} else if (new_pad & PAD_L1) {
				event |= 2;  //event |= valid pad command
				if ((size -= 1024) < saveSize)
					size = saveSize;
			} else if (new_pad & PAD_R2) {
				event |= 2;  //event |= valid pad command
				if (size < 10240)
					size = 10240;
				else {
					if ((size += 10240) > MAX_PARTMB)
						size = MAX_PARTMB;
				}
			} else if (new_pad & PAD_L2) {
				event |= 2;  //event |= valid pad command
				if ((size -= 10240) < saveSize)
					size = saveSize;
			} else if ((new_pad & PAD_TRIANGLE) || (!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				return -1;
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
				event |= 2;  //event |= valid pad command
				break;
			}
		}

		if (event || post_event) {  //NB: We need to update two frame buffers per event

			//Display section
			drawPopSprite(setting->color[COLOR_BACKGR],
			              mSprite_X1, mSprite_Y1,
			              mSprite_X2, mSprite_Y2);
			drawFrame(mSprite_X1, mSprite_Y1, mSprite_X2, mSprite_Y2, setting->color[COLOR_FRAME]);

			sprintf(c, "%d %s", size, LNG(MB));
			printXY(c, mSprite_X1 + 12 * FONT_WIDTH - (strlen(c) * FONT_WIDTH) / 2,
			        mSprite_Y1 + FONT_HEIGHT, setting->color[COLOR_TEXT], TRUE, 0);
			drawFrame(mSprite_X1 + 7 * FONT_WIDTH, mSprite_Y1 + FONT_HEIGHT / 2,
			          mSprite_X2 - 7 * FONT_WIDTH, mSprite_Y1 + FONT_HEIGHT * 2 + FONT_HEIGHT / 2, setting->color[COLOR_FRAME]);

			//RA NB: Next line assumes a scrollbar 19 characters wide (see below)
			sprintf(c, "128%s            %3d%s", LNG(MB), MAX_PARTGB, LNG(GB));
			printXY(c, mSprite_X1 + FONT_WIDTH, mSprite_Y1 + FONT_HEIGHT * 3, setting->color[COLOR_TEXT], TRUE, 0);

			drawOpSprite(setting->color[COLOR_FRAME],
			             mSprite_X1 + 2 * FONT_WIDTH + FONT_WIDTH / 2, mSprite_Y1 + FONT_HEIGHT * 5 - LINE_THICKNESS + 1,
			             mSprite_X2 - 2 * FONT_WIDTH - FONT_WIDTH / 2, mSprite_Y1 + FONT_HEIGHT * 5);


			//RA NB: Next line sets scroll position on a bar 19 chars wide (see above)
			scrollBar = (size * (19 * FONT_WIDTH) / (MAX_PARTMB - 128));

			drawOpSprite(setting->color[COLOR_FRAME],
			             mSprite_X1 + 2 * FONT_WIDTH + FONT_WIDTH / 2 + (int)scrollBar - LINE_THICKNESS + 1,
			             mSprite_Y1 + FONT_HEIGHT * 5 - FONT_HEIGHT / 2,
			             mSprite_X1 + 2 * FONT_WIDTH + FONT_WIDTH / 2 + (int)scrollBar,
			             mSprite_Y1 + FONT_HEIGHT * 5 + FONT_HEIGHT / 2);

			//Tooltip section
			x = SCREEN_MARGIN;
			y = Menu_tooltip_y;
			drawSprite(setting->color[COLOR_BACKGR],
			           0, y - 1,
			           SCREEN_WIDTH, y + 16);
			if (swapKeys)
				sprintf(c, "\xFF"
				           "1:%s \xFF"
				           "0:%s \xFF"
				           "3:%s \xFF"
				           "</\xFF"
				           "::-/+128%s L1/R1:-/+1%s L2/R2:-/+10%s",
				        LNG(OK), LNG(Cancel), LNG(Back), LNG(MB), LNG(GB), LNG(GB));
			else
				sprintf(c, "\xFF"
				           "0:%s \xFF"
				           "1:%s \xFF"
				           "3:%s \xFF"
				           "</\xFF"
				           "::-/+128%s L1/R1:-/+1%s L2/R2:-/+10%s",
				        LNG(OK), LNG(Cancel), LNG(Back), LNG(MB), LNG(GB), LNG(GB));
			printXY(c, x, y, setting->color[COLOR_SELECT], TRUE, 0);
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	}  //ends while
	return size;
}
//------------------------------
//endfunc sizeSelector
//--------------------------------------------------------------
int MenuParty(PARTYINFO Info)
{
	u64 color;
	char enable[NUM_MENU], tmp[64];
	int x, y, i, sel, cmd;
	int menu_count = sizeof(hdd_party_menu_items) / sizeof(hdd_party_menu_items[0]);
	int event, post_event = 0;

	int menu_len = strlen(LNG(Create)) > strlen(LNG(Remove)) ?
	                   strlen(LNG(Create)) :
	                   strlen(LNG(Remove));
	menu_len = strlen(LNG(Rename)) > menu_len ? strlen(LNG(Rename)) : menu_len;
	menu_len = strlen(LNG(Inject_Header)) > menu_len ? strlen(LNG(Inject_Header)) : menu_len;
	menu_len = strlen(LNG(Format)) > menu_len ? strlen(LNG(Format)) : menu_len;

	int menu_ch_w = menu_len + 1;                                 //Total characters in longest menu string
	int menu_ch_h = menu_count;                                   //Total number of menu lines
	int mSprite_Y1 = 64;                                          //Top edge of sprite
	int mSprite_X2 = SCREEN_WIDTH - 35;                           //Right edge of sprite
	int mSprite_X1 = mSprite_X2 - (menu_ch_w + 3) * FONT_WIDTH;   //Left edge of sprite
	int mSprite_Y2 = mSprite_Y1 + (menu_ch_h + 1) * FONT_HEIGHT;  //Bottom edge of sprite

	unmountAll();     //unmount all uLE-used mountpoints
	unmountParty(0);  //unconditionally unmount primary mountpoint
	unmountParty(1);  //unconditionally unmount secondary mountpoint

	memset(enable, TRUE, NUM_MENU);
	if (console_is_PSX) {
		enable[FORMAT] = FALSE;
	}

	if ((Info.Name[0] == '_') && (Info.Name[1] == '_')) {
		enable[REMOVE] = FALSE;
	}
	if (Info.Name[0] == '\0') {
		enable[REMOVE] = FALSE;
		enable[RENAME] = FALSE;
		enable[INJECT_HEADER] = FALSE;
	}
	if (Info.Treatment == TREAT_SYSTEM) {
		enable[REMOVE] = FALSE;
		enable[RENAME] = FALSE;
		enable[INJECT_HEADER] = FALSE;
	}
	if (Info.Treatment != TREAT_PFS) {
		enable[INJECT_HEADER] = FALSE;
	}
	/*if (Info.Treatment == TREAT_HDL_RAW) {
		enable[EXPAND] = FALSE;
	}
	if (Info.Treatment == TREAT_HDL_GAME) {
		enable[EXPAND] = FALSE;
	}
	if (Info.Treatment == TREAT_NOACCESS) {
		enable[EXPAND] = FALSE;
	}//*/
	enable[EXPAND] = FALSE;
	for (sel = 0; sel < menu_count; sel++)
		if (enable[hdd_party_menu_items[sel]] == TRUE)
			break;

	event = 1;  //event = initial entry
	while (1) {
		//Pad response section
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_UP && sel < menu_count) {
				event |= 2;  //event |= valid pad command
				do {
					sel--;
					if (sel < 0)
						sel = menu_count - 1;
				} while (!enable[hdd_party_menu_items[sel]]);
			} else if (new_pad & PAD_DOWN && sel < menu_count) {
				event |= 2;  //event |= valid pad command
				do {
					sel++;
					if (sel == menu_count)
						sel = 0;
				} while (!enable[hdd_party_menu_items[sel]]);
			} else if ((new_pad & PAD_TRIANGLE) || (!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				return -1;
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
				event |= 2;  //event |= valid pad command
				break;
			}
		}

		if (event || post_event) {  //NB: We need to update two frame buffers per event

			//Display section
			drawPopSprite(setting->color[COLOR_BACKGR],
			              mSprite_X1, mSprite_Y1,
			              mSprite_X2, mSprite_Y2);
			drawFrame(mSprite_X1, mSprite_Y1, mSprite_X2, mSprite_Y2, setting->color[COLOR_FRAME]);

			for (i = 0, y = mSprite_Y1 + FONT_HEIGHT / 2; i < menu_count; i++) {
				cmd = hdd_party_menu_items[i];
				if (cmd == CREATE)
					strcpy(tmp, LNG(Create));
				else if (cmd == REMOVE)
					strcpy(tmp, LNG(Remove));
				else if (cmd == RENAME)
					strcpy(tmp, LNG(Rename));
				else if (cmd == INJECT_HEADER)
					strcpy(tmp, LNG(Inject_Header));
				else if (cmd == FORMAT)
					strcpy(tmp, LNG(Format));

				if (enable[cmd])
					color = setting->color[COLOR_TEXT];
				else
					color = setting->color[COLOR_FRAME];

				printXY(tmp, mSprite_X1 + 2 * FONT_WIDTH, y, color, TRUE, 0);
				y += FONT_HEIGHT;
			}
			if (sel < menu_count)
				drawChar(LEFT_CUR, mSprite_X1 + FONT_WIDTH, mSprite_Y1 + (FONT_HEIGHT / 2 + sel * FONT_HEIGHT), setting->color[COLOR_TEXT]);

			//Tooltip section
			x = SCREEN_MARGIN;
			y = Menu_tooltip_y;
			drawSprite(setting->color[COLOR_BACKGR],
			           0, y - 1,
			           SCREEN_WIDTH, y + 16);
			if (swapKeys)
				sprintf(tmp, "\xFF"
				             "1:%s \xFF"
				             "0:%s \xFF"
				             "3:%s",
				        LNG(OK), LNG(Cancel), LNG(Back));
			else
				sprintf(tmp, "\xFF"
				             "0:%s \xFF"
				             "1:%s \xFF"
				             "3:%s",
				        LNG(OK), LNG(Cancel), LNG(Back));
			printXY(tmp, x, y, setting->color[COLOR_SELECT], TRUE, 0);
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	}  //ends while
	return hdd_party_menu_items[sel];
}
//------------------------------
//endfunc MenuParty
//--------------------------------------------------------------
static int MenuHeaderSource(const char **source_device)
{
	const char *items[4];
	const char *title = LNG(Header_Source);
	u64 color;
	char tmp[64];
	int x, y, i, sel, count;
	int menu_len, menu_ch_w, menu_ch_h;
	int mSprite_X1, mSprite_Y1, mSprite_X2, mSprite_Y2;
	int event, post_event = 0;

	if (source_device == NULL)
		return -1;

	count = 0;
	items[count++] = "usb:";
#ifdef MMCE
	items[count++] = "mmce0:";
	items[count++] = "mmce1:";
#endif
#ifdef UDPFS
	items[count++] = "udpfs:";
#endif

	menu_len = strlen(title);
	for (i = 0; i < count; i++)
		menu_len = strlen(items[i]) > menu_len ? strlen(items[i]) : menu_len;

	menu_ch_w = menu_len + 1;
	menu_ch_h = count + 1;
	mSprite_Y1 = 64;
	mSprite_X2 = SCREEN_WIDTH - 35;
	mSprite_X1 = mSprite_X2 - (menu_ch_w + 3) * FONT_WIDTH;
	mSprite_Y2 = mSprite_Y1 + (menu_ch_h + 1) * FONT_HEIGHT;

	sel = 0;
	event = 1;
	while (1) {
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_UP) {
				event |= 2;
				sel--;
				if (sel < 0)
					sel = count - 1;
			} else if (new_pad & PAD_DOWN) {
				event |= 2;
				sel++;
				if (sel == count)
					sel = 0;
			} else if ((new_pad & PAD_TRIANGLE) || (!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				return -1;
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
				event |= 2;
				break;
			}
		}

		if (event || post_event) {
			drawPopSprite(setting->color[COLOR_BACKGR],
			              mSprite_X1, mSprite_Y1,
			              mSprite_X2, mSprite_Y2);
			drawFrame(mSprite_X1, mSprite_Y1, mSprite_X2, mSprite_Y2, setting->color[COLOR_FRAME]);

			printXY(title, mSprite_X1 + 2 * FONT_WIDTH, mSprite_Y1 + FONT_HEIGHT / 2, setting->color[COLOR_SELECT], TRUE, 0);

			for (i = 0, y = mSprite_Y1 + FONT_HEIGHT / 2 + FONT_HEIGHT; i < count; i++) {
				strcpy(tmp, items[i]);
				color = setting->color[COLOR_TEXT];
				printXY(tmp, mSprite_X1 + 2 * FONT_WIDTH, y, color, TRUE, 0);
				y += FONT_HEIGHT;
			}
			drawChar(LEFT_CUR, mSprite_X1 + FONT_WIDTH, mSprite_Y1 + (FONT_HEIGHT / 2 + (sel + 1) * FONT_HEIGHT), setting->color[COLOR_TEXT]);

			x = SCREEN_MARGIN;
			y = Menu_tooltip_y;
			drawSprite(setting->color[COLOR_BACKGR],
			           0, y - 1,
			           SCREEN_WIDTH, y + 16);
			if (swapKeys)
				sprintf(tmp, "\xFF"
				             "1:%s \xFF"
				             "0:%s \xFF"
				             "3:%s",
				        LNG(OK), LNG(Cancel), LNG(Back));
			else
				sprintf(tmp, "\xFF"
				             "0:%s \xFF"
				             "1:%s \xFF"
				             "3:%s",
				        LNG(OK), LNG(Cancel), LNG(Back));
			printXY(tmp, x, y, setting->color[COLOR_SELECT], TRUE, 0);
		}
		drawScr();
		post_event = event;
		event = 0;
	}

	*source_device = items[sel];
	return sel;
}
//------------------------------
//endfunc MenuHeaderSource
//--------------------------------------------------------------
static int MenuHeaderInjectMode(void)
{
	const char *items[4];
	const char *title = LNG(Header_Inject_Mode);
	u64 color;
	char tmp[80];
	int x, y, i, sel, count;
	int menu_len, menu_ch_w, menu_ch_h;
	int mSprite_X1, mSprite_Y1, mSprite_X2, mSprite_Y2;
	int event, post_event = 0;

	count = 0;
	items[count++] = LNG(Header_Inject_This_Partition);
	items[count++] = LNG(Header_Inject_Matching_Partitions);
	items[count++] = LNG(Header_Inject_Matching_Create_Missing);
	items[count++] = LNG(Cancel);

	menu_len = strlen(title);
	for (i = 0; i < count; i++)
		menu_len = strlen(items[i]) > menu_len ? strlen(items[i]) : menu_len;

	menu_ch_w = menu_len + 1;
	menu_ch_h = count + 1;
	mSprite_Y1 = 64;
	mSprite_X2 = SCREEN_WIDTH - 35;
	mSprite_X1 = mSprite_X2 - (menu_ch_w + 3) * FONT_WIDTH;
	mSprite_Y2 = mSprite_Y1 + (menu_ch_h + 1) * FONT_HEIGHT;

	sel = 0;
	event = 1;
	while (1) {
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_UP) {
				event |= 2;
				sel--;
				if (sel < 0)
					sel = count - 1;
			} else if (new_pad & PAD_DOWN) {
				event |= 2;
				sel++;
				if (sel == count)
					sel = 0;
			} else if ((new_pad & PAD_TRIANGLE) || (!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				return HEADER_INJECT_CANCEL;
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
				event |= 2;
				break;
			}
		}

		if (event || post_event) {
			drawPopSprite(setting->color[COLOR_BACKGR],
			              mSprite_X1, mSprite_Y1,
			              mSprite_X2, mSprite_Y2);
			drawFrame(mSprite_X1, mSprite_Y1, mSprite_X2, mSprite_Y2, setting->color[COLOR_FRAME]);

			printXY(title, mSprite_X1 + 2 * FONT_WIDTH, mSprite_Y1 + FONT_HEIGHT / 2, setting->color[COLOR_SELECT], TRUE, 0);

			for (i = 0, y = mSprite_Y1 + FONT_HEIGHT / 2 + FONT_HEIGHT; i < count; i++) {
				strcpy(tmp, items[i]);
				color = setting->color[COLOR_TEXT];
				printXY(tmp, mSprite_X1 + 2 * FONT_WIDTH, y, color, TRUE, 0);
				y += FONT_HEIGHT;
			}
			drawChar(LEFT_CUR, mSprite_X1 + FONT_WIDTH, mSprite_Y1 + (FONT_HEIGHT / 2 + (sel + 1) * FONT_HEIGHT), setting->color[COLOR_TEXT]);

			x = SCREEN_MARGIN;
			y = Menu_tooltip_y;
			drawSprite(setting->color[COLOR_BACKGR],
			           0, y - 1,
			           SCREEN_WIDTH, y + 16);
			if (swapKeys)
				sprintf(tmp, "\xFF"
				             "1:%s \xFF"
				             "0:%s \xFF"
				             "3:%s",
				        LNG(OK), LNG(Cancel), LNG(Back));
			else
				sprintf(tmp, "\xFF"
				             "0:%s \xFF"
				             "1:%s \xFF"
				             "3:%s",
				        LNG(OK), LNG(Cancel), LNG(Back));
			printXY(tmp, x, y, setting->color[COLOR_SELECT], TRUE, 0);
		}
		drawScr();
		post_event = event;
		event = 0;
	}

	return (sel == HEADER_INJECT_CANCEL) ? HEADER_INJECT_CANCEL : sel;
}
//------------------------------
//endfunc MenuHeaderInjectMode
//--------------------------------------------------------------
int CreateParty(char *party, int size)
{
	int i, num = 1, ret = 0;
	//	int  remSize = size-2048;
	char tmpName[MAX_ENTRY];
	//	t_hddFilesystem hddFs[MAX_PARTITIONS];

	tmpName[0] = 0;
	sprintf(tmpName, "%s", party);
	for (i = 0; i < MAX_PARTITIONS; i++) {
		if (!strcmp(PartyInfo[i].Name, tmpName)) {
			sprintf(tmpName, "%s%d", party, num);
			num++;
		}
	}
	strcpy(party, tmpName);

	if (!ConfirmProtectedPartitionCreate(party, 0))
		return HDD_PARTITION_CREATE_CANCELLED;

	drawMsg(LNG(Creating_New_Partition));

	/*	if(remSize <= 0)*/
	ret = hddMakeFilesystem(size, party, FS_GROUP_APPLICATION);
	/*	else{
		ret = hddMakeFilesystem(2048, party, FS_GROUP_APPLICATION);
		hddGetFilesystemList(hddFs, MAX_PARTITIONS);
		for(i=0; i<MAX_PARTITIONS; i++){
			if(!strcmp(hddFs[i].filename+5, party))
				ret = hddExpandFilesystem(&hddFs[i], remSize);
		}
	}*/

	if (ret > 0) {
		GetHddInfo();
		drawMsg(LNG(New_Partition_Created));
	} else {
		drawMsg(LNG(Failed_Creating_New_Partition));
	}

	WaitTime = Timer();
	while (Timer() < WaitTime + 1500)
		;  // print operation result during 1.5 sec.

	return ret;
}
//------------------------------
//endfunc CreateParty
//--------------------------------------------------------------
static int CreatePartyExact(const char *party, int size, int skip_instead_of_cancel)
{
	char party_name[MAX_PART_NAME + 1];
	int ret;

	if (party == NULL || party[0] == '\0')
		return -EINVAL;

	snprintf(party_name, sizeof(party_name), "%s", party);
	party_name[sizeof(party_name) - 1] = '\0';

	if (!ConfirmProtectedPartitionCreate(party_name, skip_instead_of_cancel))
		return HDD_PARTITION_CREATE_CANCELLED;

	drawMsg(LNG(Creating_New_Partition));

	ret = hddMakeFilesystem(size, party_name, FS_GROUP_APPLICATION);

	if (ret > 0) {
		GetHddInfo();
		drawMsg(LNG(New_Partition_Created));
	} else {
		drawMsg(LNG(Failed_Creating_New_Partition));
	}

	WaitTime = Timer();
	while (Timer() < WaitTime + 1500)
		;  // print operation result during 1.5 sec.

	return ret;
}
//------------------------------
//endfunc CreatePartyExact
//--------------------------------------------------------------
int RemoveParty(PARTYINFO Info)
{
	int i, ret = 0;
	char tmpName[MAX_ENTRY];

	DPRINTF("Remove Partition: %d\n", Info.Count);

	drawMsg(LNG(Removing_Current_Partition));

	sprintf(tmpName, "hdd0:%s", Info.Name);
	ret = fileXioRemove(tmpName);

	if (ret == 0) {
		hddUsed -= Info.TotalSize;
		hddFreeSpace = (hddSize - hddUsed) & 0x7FFFFF80;  //free space rounded to useful area
		hddFree = (hddFreeSpace * 100) / hddSize;         //free space percentage
		numParty--;
		if (Info.Count == numParty) {
			memset(&PartyInfo[numParty], 0, sizeof(PARTYINFO));
		} else {
			for (i = Info.Count; i < numParty; i++) {
				memset(&PartyInfo[i], 0, sizeof(PARTYINFO));
				memcpy(&PartyInfo[i], &PartyInfo[i + 1], sizeof(PARTYINFO));
				PartyInfo[i].Count--;
			}
			memset(&PartyInfo[numParty], 0, sizeof(PARTYINFO));
		}
		drawMsg(LNG(Partition_Removed));
	} else {
		drawMsg(LNG(Failed_Removing_Current_Partition));
	}

	WaitTime = Timer();
	while (Timer() < WaitTime + 1500)
		;  // print operation result during 1.5 sec.

	return ret;
}
//------------------------------
//endfunc RemoveParty
//--------------------------------------------------------------
int RenameParty(PARTYINFO Info, char *newName)
{
	int i, num = 1, ret = 0;
	char in[MAX_ENTRY], out[MAX_ENTRY], tmpName[MAX_ENTRY];

	drawMsg(LNG(Renaming_Partition));

	in[0] = 0;
	out[0] = 0;
	tmpName[0] = 0;
	sprintf(tmpName, "%s", newName);
	if (!strcmp(Info.Name, tmpName)) //Exactly the same name entered.
		goto end;
	//If other partitions exist with the same name, append a number to keep the name unique.
	for (i = 0; i < numParty; i++) {
		if (!strcmp(PartyInfo[i].Name, tmpName)) {
			sprintf(tmpName, "%s%d", newName, num);
			num++;
		}
	}
	strcpy(newName, tmpName);
	if (strlen(newName) >= MAX_PART_NAME) {
		ret = -ENAMETOOLONG;
		goto end2;  //For this case HDL renaming can't be done
	}

	sprintf(in, "hdd0:%s", Info.Name);
	sprintf(out, "hdd0:%s", newName);
	ret = fileXioRename(in, out);

	if (ret == 0) {
		strncpy(PartyInfo[Info.Count].Name, newName, MAX_PART_NAME);
		PartyInfo[Info.Count].Name[MAX_PART_NAME] = '\0';
		drawMsg(LNG(Partition_Renamed));
	} else {
	end2:
		drawMsg(LNG(Failed_Renaming_Partition));
	}

	WaitTime = Timer();
	while (Timer() < WaitTime + 1500)
		;  // print operation result during 1.5 sec.

end:
	return ret;
}
//------------------------------
//endfunc RenameParty
//--------------------------------------------------------------
int RenameGame(PARTYINFO Info, char *newName)
{
	DPRINTF("Rename Game: %d\n", Info.Count);
	//int fd;
	int i, num = 1, ret = 0;
	char tmpName[MAX_ENTRY];

	drawMsg(LNG(Renaming_Game));

	if (!strcmp(Info.Game.Name, newName))
		goto end1;

	tmpName[0] = 0;
	strcpy(tmpName, newName);
	for (i = 0; i < MAX_PARTITIONS; i++) {
		if (!strcmp(PartyInfo[i].Game.Name, tmpName)) {
			sprintf(tmpName, "%s%d", newName, num);
			num++;
		}
	}
	strcpy(newName, tmpName);
	if (strlen(newName) >= MAX_PART_NAME) {
		ret = -ENAMETOOLONG;
		goto end2;  //For this case HDL renaming can't be done
	}

	ret = HdlRenameGame(Info.Game.Name, newName);

	if (ret == 0) {
		strcpy(PartyInfo[Info.Count].Game.Name, newName);
		/*	if(mountParty("hdd0:HDLoader Settings")>=0){
			if((fd=genOpen("pfs0:/gamelist.log", FIO_O_RDONLY)) >= 0){
				genClose(fd);
				if(fileXioRemove("pfs0:/gamelist.log")!=0)
					ret=0;
			}
			unmountParty(latestMount);
		}*/
	} else {
		sprintf(DbgMsg, "HdlRenameGame(\"%s\",\n  \"%s\")\n=> %d", Info.Game.Name, newName, ret);
		DebugDisp(DbgMsg);
	}


	if (ret != 0) {
		strcpy(PartyInfo[Info.Count].Game.Name, newName);
		drawMsg(LNG(Game_Renamed));
	} else {
	end2:
		drawMsg(LNG(Failed_Renaming_Game));
	}

	WaitTime = Timer();
	while (Timer() < WaitTime + 1500)
		;  // print operation result during 1.5 sec.

end1:
	return ret;
}
//------------------------------
//endfunc RenameGame
//--------------------------------------------------------------
int ExpandParty(PARTYINFO Info, int size)
{
	int i, ret = 0;
	char tmpName[MAX_ENTRY + 5];
	t_hddFilesystem hddFs[MAX_PARTITIONS];

	drawMsg(LNG(Expanding_Current_Partition));
	//printf("Expand Partition: %d\n", Info.Count);

	sprintf(tmpName, "hdd0:%s", Info.Name);

	hddGetFilesystemList(hddFs, MAX_PARTITIONS);
	for (i = 0; i < MAX_PARTITIONS; i++) {
		if (!strcmp(hddFs[i].filename, tmpName)) {
			ret = hddExpandFilesystem(&hddFs[i], size);
		}
	}

	if (ret > 0) {
		hddUsed += size;
		hddFreeSpace = (hddSize - hddUsed) & 0x7FFFFF80;  //free space rounded to useful area
		hddFree = (hddFreeSpace * 100) / hddSize;         //free space percentage

		PartyInfo[Info.Count].TotalSize += size;
		PartyInfo[Info.Count].FreeSize += size;
		drawMsg(LNG(Partition_Expanded));
	} else {
		drawMsg(LNG(Failed_Expanding_Current_Partition));
	}

	WaitTime = Timer();
	while (Timer() < WaitTime + 1500)
		;  // print operation result during 1.5 sec.

	return ret;
}
//------------------------------
//endfunc ExpandParty
//--------------------------------------------------------------
int FormatHdd(void)
{
	int ret = 0;

	drawMsg(LNG(Formating_HDD));

	ret = hddFormat();

	if (ret == 0) {
		drawMsg(LNG(HDD_Formated));
	} else {
		drawMsg(LNG(HDD_Format_Failed));
	}

	WaitTime = Timer();
	while (Timer() < WaitTime + 1500)
		;  // print operation result during 1.5 sec.

	GetHddInfo();

	return ret;
}
//------------------------------
//endfunc FormatHdd
//--------------------------------------------------------------
typedef struct
{
	int Injected;
	int Created;
	int Skipped;
	int Failed;
	int Invalid;
	int PfsCopyFailed;
	int Stopped;
} HDDHEADERBULKSTATS;

static int FindPartitionIndexByName(const char *partition_name)
{
	int i;

	if (partition_name == NULL)
		return -1;

	for (i = 0; i < numParty; i++) {
		if (!strcmp(PartyInfo[i].Name, partition_name))
			return i;
	}

	return -1;
}

static int FindInjectablePartitionIndexByName(const char *partition_name)
{
	int i;

	if (partition_name == NULL)
		return -1;

	for (i = 0; i < numParty; i++) {
		if (!strcmp(PartyInfo[i].Name, partition_name) && PartyInfo[i].Treatment == TREAT_PFS)
			return i;
	}

	return -1;
}

static int HeaderCreateMissingDialog(const char *partition_name)
{
	char msg[512];
	int dh, dw, dx, dy;
	int label_x[3];
	const char *labels[3];
	int sel = 0, a = 6, b = 4, c = 2, n, tw;
	int i, y, len, ret, labels_width;
	int event, post_event = 0;

	snprintf(msg, sizeof(msg), "Create missing partition?\nhdd0:%s", partition_name);
	msg[sizeof(msg) - 1] = '\0';

	labels[0] = LNG(OK);
	labels[1] = LNG(Skip);
	labels[2] = LNG(CANCEL);

	for (i = 0, n = 1; msg[i] != 0; i++) {
		if (msg[i] == '\n') {
			msg[i] = 0;
			n++;
		}
	}
	for (i = len = tw = 0; i < n; i++) {
		ret = printXY(&msg[len], 0, 0, 0, FALSE, 0);
		if (ret > tw)
			tw = ret;
		len += strlen(&msg[len]) + 1;
		}
		if (tw < 176)
			tw = 176;
		labels_width = (strlen(labels[0]) + strlen(labels[1]) + strlen(labels[2]) + 8) * FONT_WIDTH;
		if (tw < labels_width)
			tw = labels_width;

		dh = FONT_HEIGHT * (n + 1) + 2 * 2 + a + b + c;
	dw = 2 * 2 + a * 2 + tw;
	dx = (SCREEN_WIDTH - dw) / 2;
	dy = (SCREEN_HEIGHT - dh) / 2;

	label_x[0] = dx + a + FONT_WIDTH;
	label_x[1] = dx + (dw - strlen(labels[1]) * FONT_WIDTH) / 2;
	label_x[2] = dx + dw - a - strlen(labels[2]) * FONT_WIDTH;

	event = 1;
	while (1) {
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_LEFT) {
				event |= 2;
				sel--;
				if (sel < 0)
					sel = 2;
			} else if (new_pad & PAD_RIGHT) {
				event |= 2;
				sel++;
				if (sel > 2)
					sel = 0;
			} else if ((new_pad & PAD_TRIANGLE) || (!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				ret = HEADER_CREATE_PROMPT_CANCEL;
				break;
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
				ret = (sel == 0) ? HEADER_CREATE_PROMPT_CREATE : ((sel == 1) ? HEADER_CREATE_PROMPT_SKIP : HEADER_CREATE_PROMPT_CANCEL);
				break;
			}
		}

		if (event || post_event) {
			drawPopSprite(setting->color[COLOR_BACKGR],
			              dx, dy,
			              dx + dw, (dy + dh));
			drawFrame(dx, dy, dx + dw, (dy + dh), setting->color[COLOR_FRAME]);
			for (i = len = 0; i < n; i++) {
				printXY(&msg[len], dx + 2 + a, (dy + a + 2 + i * FONT_HEIGHT), setting->color[COLOR_TEXT], TRUE, 0);
				len += strlen(&msg[len]) + 1;
			}

			y = dy + a + b + 2 + n * FONT_HEIGHT;
			for (i = 0; i < 3; i++)
				printXY(labels[i], label_x[i], y, setting->color[COLOR_TEXT], TRUE, 0);
			drawChar(LEFT_CUR, label_x[sel] - FONT_WIDTH, y, setting->color[COLOR_TEXT]);
		}
		drawScr();
		post_event = event;
		event = 0;
	}

	return ret;
}

static int InjectHeaderIntoPartition(const char *partition_name, const char *source_device, HDDHEADERBULKSTATS *stats)
{
	char source_dir[MAX_PATH];
	char msg[MAX_TEXT_LINE];
	int pfs_files_copied;
	int pfs_copy_error;
	int ret;

	source_dir[0] = '\0';
	pfs_files_copied = 0;
	pfs_copy_error = 0;

	snprintf(msg, sizeof(msg), "Injecting header into hdd0:%s", partition_name);
	drawMsg(msg);

	ret = InjectHddPartitionHeaderFromSource(partition_name, source_device, source_dir, sizeof(source_dir),
	                                         &pfs_files_copied, &pfs_copy_error);
	if (ret >= 0) {
		if (stats != NULL) {
			stats->Injected++;
			if (pfs_copy_error < 0)
				stats->PfsCopyFailed++;
		}
		return 0;
	}

	if (stats != NULL)
		stats->Failed++;
	return ret;
}

static void ShowHeaderBulkSummary(const HDDHEADERBULKSTATS *stats)
{
	if (stats == NULL)
		return;

	snprintf(DbgMsg, sizeof(DbgMsg),
	         "Header injection summary\n"
	         "Injected: %d  Created: %d\n"
	         "Skipped: %d  Failed: %d\n"
	         "Invalid header folders: %d\n"
	         "PFS copy failed: %d%s",
	         stats->Injected, stats->Created,
	         stats->Skipped, stats->Failed,
	         stats->Invalid,
	         stats->PfsCopyFailed,
	         stats->Stopped ? "\nStopped." : "");
	DebugDisp(DbgMsg);
}

static void InjectMatchingHeadersFromSource(const char *source_device, int create_missing)
{
	HDDHEADERBULKSTATS stats;
	char confirm[MAX_PATH];
	int source_count;
	int invalid_count;
	int partition_index;
	int prompt_ret;
	int party_size;
	int ret;
	int i;

	memset(&stats, 0, sizeof(stats));

	source_count = HddHeaderListSourcePartitions(source_device, hdd_header_source_partitions, MAX_PARTITIONS, &invalid_count);
	if (source_count < 0) {
		snprintf(DbgMsg, sizeof(DbgMsg), "Unable to read %s/__Headers/\nError %d", source_device, source_count);
		DebugDisp(DbgMsg);
		return;
	}

	stats.Invalid = invalid_count;
	if (source_count == 0) {
		ShowHeaderBulkSummary(&stats);
		return;
	}

	if (create_missing) {
		snprintf(confirm, sizeof(confirm),
		         "Inject matching headers from %s?\n"
		         "Missing partitions will be prompted one at a time.",
		         source_device);
	} else {
		snprintf(confirm, sizeof(confirm),
		         "Inject headers into matching partitions from %s?\n"
		         "Use %s/__Headers/",
		         source_device, source_device);
	}
	if (ynDialog(confirm) != 1)
		return;

	unmountAll();
	unmountParty(0);
	unmountParty(1);

	for (i = 0; i < source_count; i++) {
		partition_index = FindInjectablePartitionIndexByName(hdd_header_source_partitions[i].name);
		if (partition_index >= 0) {
			InjectHeaderIntoPartition(PartyInfo[partition_index].Name, source_device, &stats);
			continue;
		}

		if (FindPartitionIndexByName(hdd_header_source_partitions[i].name) >= 0) {
			stats.Skipped++;
			continue;
		}

		if (!create_missing) {
			stats.Skipped++;
			continue;
		}

		prompt_ret = HeaderCreateMissingDialog(hdd_header_source_partitions[i].name);
		if (prompt_ret == HEADER_CREATE_PROMPT_SKIP) {
			stats.Skipped++;
			continue;
		}
		if (prompt_ret == HEADER_CREATE_PROMPT_CANCEL) {
			stats.Stopped = 1;
			break;
		}

		drawMsg(LNG(Select_New_Partition_Size_In_MB));
		drawMsg(LNG(Select_New_Partition_Size_In_MB));
		party_size = sizeSelector(128);
		if (party_size <= 0) {
			stats.Stopped = 1;
			break;
		}

		ret = CreatePartyExact(hdd_header_source_partitions[i].name, party_size, 1);
		if (ret == HDD_PARTITION_CREATE_CANCELLED) {
			stats.Skipped++;
			continue;
		} else if (ret > 0) {
			stats.Created++;
			invalidatePartitionCaches();
			InjectHeaderIntoPartition(hdd_header_source_partitions[i].name, source_device, &stats);
		} else {
			stats.Failed++;
		}
	}

	ShowHeaderBulkSummary(&stats);
}
//------------------------------
//endfunc InjectMatchingHeadersFromSource
//--------------------------------------------------------------
void hddManager(void)
{
	char c[MAX_PATH];
	int Angle;
	int x, y, y0, y1, x2, y2;
	float x3, y3;
	int i, ret;
	int partySize;
	int pfsFree;
	int ray = 50;
	u64 Color;
	char tmp[MAX_PATH];
	char tooltip[MAX_TEXT_LINE];
	int top = 0, rows;
	int event, post_event = 0;
	int browser_sel = 0, browser_nfiles = 0;
	int Treat;

	rows = (Menu_end_y - Menu_start_y) / FONT_HEIGHT;

	loadHddModules();
	GetHddInfo();

	event = 1;  //event = initial entry
	while (1) {

		//Pad response section
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad) {
				//printf("Selected Partition: %d\n", browser_sel);
				event |= 2;  //event |= pad command
			}
			if (new_pad & PAD_UP)
				browser_sel--;
			else if (new_pad & PAD_DOWN)
				browser_sel++;
			else if (new_pad & PAD_LEFT)
				browser_sel -= rows / 2;
			else if (new_pad & PAD_RIGHT)
				browser_sel += rows / 2;
			else if ((new_pad & PAD_SELECT) || (new_pad & PAD_TRIANGLE)) {
				//Prepare for exit from HddManager
				unmountAll();     //unmount all uLE-used mountpoints
				unmountParty(0);  //unconditionally unmount primary mountpoint
				unmountParty(1);  //unconditionally unmount secondary mountpoint
				return;
			} else if (new_pad & PAD_L1) {
				ShowHddStatusInfo();
			} else if (new_pad & PAD_SQUARE) {
				if (PartyInfo[browser_sel].Treatment == TREAT_HDL_RAW) {
					loadHdlInfoModule();
					ret = HdlGetGameInfo(PartyInfo[browser_sel].Name, &PartyInfo[browser_sel].Game);
					if (ret == 0)
						PartyInfo[browser_sel].Treatment = TREAT_HDL_GAME;
					else {
						sprintf(DbgMsg, "HdlGetGameInfo(\"%s\",bf)\n=> %d", PartyInfo[browser_sel].Name, ret);
						DebugDisp(DbgMsg);
					}
				} else if (PartyInfo[browser_sel].Treatment == TREAT_HDL_GAME)
					PartyInfo[browser_sel].Treatment = TREAT_HDL_RAW;  //Press Square again to unload HDL info
			} else if (new_pad & PAD_R1) {                             //Starts clause for R1 menu
				ret = MenuParty(PartyInfo[browser_sel]);
				tmp[0] = 0;
				if (ret == CREATE) {
					drawMsg(LNG(Enter_New_Partition_Name));
					drawMsg(LNG(Enter_New_Partition_Name));
					if (keyboard(tmp, MAX_PART_NAME) > 0) {
						partySize = 128;
						drawMsg(LNG(Select_New_Partition_Size_In_MB));
						drawMsg(LNG(Select_New_Partition_Size_In_MB));
						if ((ret = sizeSelector(partySize)) > 0) {
							if (ynDialog(LNG(Create_New_Partition)) == 1) {
								CreateParty(tmp, ret);
								invalidatePartitionCaches();  //Tell FileBrowser to refresh partition lists
							}
						}
					}
				} else if (ret == REMOVE) {
					if (ynDialog(LNG(Remove_Current_Partition)) == 1) {
						RemoveParty(PartyInfo[browser_sel]);
						invalidatePartitionCaches();  //Tell FileBrowser to refresh partition lists
					}
				} else if (ret == RENAME) {
					drawMsg(LNG(Enter_New_Partition_Name));
					drawMsg(LNG(Enter_New_Partition_Name));
					if (PartyInfo[browser_sel].Treatment == TREAT_HDL_GAME) {  //Rename HDL Game
						strcpy(tmp, PartyInfo[browser_sel].Game.Name);
						if (keyboard(tmp, MAX_PART_NAME) > 0) {
							if (ynDialog(LNG(Rename_Current_Game)) == 1)
								RenameGame(PartyInfo[browser_sel], tmp);
						}
					} else {  //starts clause for normal partition RENAME
						strcpy(tmp, PartyInfo[browser_sel].Name);
						if (keyboard(tmp, MAX_PART_NAME) > 0) {
							if (ynDialog(LNG(Rename_Current_Partition)) == 1) {
								RenameParty(PartyInfo[browser_sel], tmp);
								invalidatePartitionCaches();  //Tell FileBrowser to refresh partition lists
							}
						}
					}  //ends clause for normal partition RENAME
				} else if (ret == INJECT_HEADER) {
					const char *source_device = NULL;
					char source_dir[MAX_PATH];
					char confirm[MAX_PATH];
					int inject_mode;
					int pfs_files_copied;
					int pfs_copy_error;

					if (MenuHeaderSource(&source_device) >= 0)
						inject_mode = MenuHeaderInjectMode();
					else
						inject_mode = HEADER_INJECT_CANCEL;

					if (source_device != NULL && inject_mode == HEADER_INJECT_THIS_PARTITION) {
						snprintf(confirm, sizeof(confirm),
						         "Inject APA header into hdd0:%s from %s?\n"
						         "Use %s/__Headers/%s/\n"
						         "Required: system.cnf; PS2: icon.sys, list.ico\n"
						         "Optional: boot.kelf, info.sys, jkt_*.png, BOOT.ELF",
						         PartyInfo[browser_sel].Name, source_device, source_device, PartyInfo[browser_sel].Name);
						if (ynDialog(confirm) == 1) {
							unmountAll();
							unmountParty(0);
							unmountParty(1);
							source_dir[0] = '\0';
							pfs_files_copied = 0;
							pfs_copy_error = 0;
							ret = InjectHddPartitionHeaderFromSource(PartyInfo[browser_sel].Name, source_device, source_dir, sizeof(source_dir),
							                                         &pfs_files_copied, &pfs_copy_error);
							if (ret >= 0) {
								if (pfs_copy_error < 0)
									snprintf(tmp, sizeof(tmp), "Header injected; PFS copy failed: %d", pfs_copy_error);
								else if (pfs_files_copied > 0)
									snprintf(tmp, sizeof(tmp), "Header injected; PFS files copied: %d", pfs_files_copied);
								else
									snprintf(tmp, sizeof(tmp), "Header injected from %s", source_dir);
								drawMsg(tmp);
							} else {
								snprintf(tmp, sizeof(tmp), "Header injection failed: %d", ret);
								drawMsg(tmp);
							}
							WaitTime = Timer();
							while (Timer() < WaitTime + 1500)
								;
						}
					} else if (source_device != NULL && inject_mode == HEADER_INJECT_MATCHING_PARTITIONS) {
						InjectMatchingHeadersFromSource(source_device, 0);
					} else if (source_device != NULL && inject_mode == HEADER_INJECT_MATCHING_CREATE_MISSING) {
						InjectMatchingHeadersFromSource(source_device, 1);
					}
				} else if (ret == EXPAND) {
					drawMsg(LNG(Select_New_Partition_Size_In_MB));
					drawMsg(LNG(Select_New_Partition_Size_In_MB));
					partySize = PartyInfo[browser_sel].TotalSize;
					if ((ret = sizeSelector(partySize)) > 0) {
						if (ynDialog(LNG(Expand_Current_Partition)) == 1) {
							ret -= partySize;
							ExpandParty(PartyInfo[browser_sel], ret);
							invalidatePartitionCaches();  //Tell FileBrowser to refresh partition lists
						}
					}
				} else if (ret == FORMAT) {
					if (ynDialog(LNG(Format_HDD)) == 1) {
						FormatHdd();
						invalidatePartitionCaches();  //Tell FileBrowser to refresh partition lists
					}
				}
			}  //Ends clause for R1 menu
		}      //ends pad response section

		if (event || post_event) {  //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[COLOR_BACKGR]);

			browser_nfiles = numParty;
			//printf("Number Of Partition: %d\n", numParty);

			if (top > browser_nfiles - rows)
				top = browser_nfiles - rows;
			if (top < 0)
				top = 0;
			if (browser_sel >= browser_nfiles)
				browser_sel = browser_nfiles - 1;
			if (browser_sel < 0)
				browser_sel = 0;
			if (browser_sel >= top + rows)
				top = browser_sel - rows + 1;
			if (browser_sel < top)
				top = browser_sel;

			y = Menu_start_y;

			snprintf(c, sizeof(c), "%s: %d %s", LNG(HDD_STATUS), hddRealStatus, GetHddStatusShortText(hddRealStatus));
			x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);

			if (TV_mode != TV_mode_PAL)
				y += FONT_HEIGHT + 10;
			else
				y += FONT_HEIGHT + 11;

			drawOpSprite(setting->color[COLOR_FRAME],
			             SCREEN_MARGIN, y - 6,
			             SCREEN_WIDTH / 2 - 20, y - 4);

			if (hddConnected == 0)
				sprintf(c, "%s:  %s / %s:  %s",
				        LNG(CONNECTED), LNG(NO), LNG(FORMATED), LNG(NO));
			else if ((hddConnected == 1) && (hddFormated == 0))
				sprintf(c, "%s:  %s / %s:  %s",
				        LNG(CONNECTED), LNG(YES), LNG(FORMATED), LNG(NO));
			else if (hddFormated == 1)
				sprintf(c, "%s:  %s / %s:  %s",
				        LNG(CONNECTED), LNG(YES), LNG(FORMATED), LNG(YES));

			x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));

			drawOpSprite(setting->color[COLOR_FRAME],
			             SCREEN_WIDTH / 2 - 21, Frame_start_y,
			             SCREEN_WIDTH / 2 - 19, Frame_end_y);

			if (TV_mode != TV_mode_PAL)
				y += FONT_HEIGHT + 11;
			else
				y += FONT_HEIGHT + 12;

			drawOpSprite(setting->color[COLOR_FRAME],
			             SCREEN_MARGIN, y - 6,
			             SCREEN_WIDTH / 2 - 20, y - 4);

			if (hddFormated == 1) {

				sprintf(c, "%s: %u %s", LNG(HDD_SIZE), hddSize, LNG(MB));
				x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
				y += FONT_HEIGHT;
				sprintf(c, "%s: %u %s", LNG(HDD_USED), hddUsed, LNG(MB));
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
				y += FONT_HEIGHT;
				sprintf(c, "%s: %u %s", LNG(HDD_FREE), hddFreeSpace, LNG(MB));
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));

				if (TV_mode != TV_mode_PAL)
					ray = 45;
				else
					ray = 55;

				x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x);
				if (TV_mode != TV_mode_PAL)
					y += ray + 20;
				else
					y += ray + 25;

				Angle = 0;

				for (i = 0; i < 360; i++) {
					if ((Angle = i - 90) >= 360)
						Angle = i + 270;
					if (((i * 100) / 360) >= hddFree)
						Color = setting->color[COLOR_GRAPH2];
					else
						Color = setting->color[COLOR_GRAPH1];
					x3 = ray * cosdgf(Angle);
					if (TV_mode != TV_mode_PAL)
						y3 = (ray - 5) * sindgf(Angle);
					else
						y3 = (ray)*sindgf(Angle);
					x2 = x + x3;
					y2 = y + (y3);
					gsKit_prim_line(gsGlobal, x, y, x2, y2, 1, Color);
					gsKit_prim_line(gsGlobal, x, y, x2 + 1, y2, 1, Color);
					gsKit_prim_line(gsGlobal, x, y, x2, y2 + 1, 1, Color);
				}

				sprintf(c, "%d%% %s", hddFree, LNG(FREE));
				printXY(c, x - FONT_WIDTH * 4, y - FONT_HEIGHT / 4, setting->color[COLOR_TEXT], TRUE, 0);

				if (TV_mode != TV_mode_PAL)
					y += ray + 15;
				else
					y += ray + 20;

				drawOpSprite(setting->color[COLOR_FRAME],
				             SCREEN_MARGIN, y - 6,
				             SCREEN_WIDTH / 2 - 20, y - 4);

				Treat = PartyInfo[browser_sel].Treatment;
				if (Treat == TREAT_SYSTEM) {
					sprintf(c, "%s: %d %s", LNG(Raw_SIZE), (int)PartyInfo[browser_sel].RawSize, LNG(MB));
					x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
					printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
					y += FONT_HEIGHT;
					strcpy(c, LNG(Reserved_for_system));
					x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
					printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
					y += FONT_HEIGHT;
					pfsFree = 0;
				} else if (Treat == TREAT_NOACCESS) {
					sprintf(c, "%s: %d %s", LNG(Raw_SIZE), (int)PartyInfo[browser_sel].RawSize, LNG(MB));
					x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
					printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
					y += FONT_HEIGHT;
					strcpy(c, LNG(Inaccessible));
					x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
					printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
					y += FONT_HEIGHT;
					pfsFree = 0;
				} else if (Treat == TREAT_HDL_RAW) {  //starts clause for HDL without GameInfo
					//---------- Start of clause for HDL game partitions ----------
					//dlanor NB: Not properly implemented yet
					sprintf(c, "%s: %d %s", LNG(Raw_SIZE), (int)PartyInfo[browser_sel].RawSize, LNG(MB));
					x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
					printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
					y += FONT_HEIGHT * 4;
					strcpy(c, LNG(Info_not_loaded));
					x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
					printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
					pfsFree = -1;                      //Disable lower pie chart display
				} else if (Treat == TREAT_HDL_GAME) {  //starts clause for HDL with GameInfo
					y += FONT_HEIGHT / 4;

					x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) -
					    (strlen(LNG(GAME_INFORMATION)) * FONT_WIDTH) / 2;
					printXY(LNG(GAME_INFORMATION), x, y,
					        setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));

					x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(PartyInfo[browser_sel].Game.Name) * FONT_WIDTH) / 2;
					y += FONT_HEIGHT * 2;
					printXY(PartyInfo[browser_sel].Game.Name, x, y,
					        setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));

					y += FONT_HEIGHT + FONT_HEIGHT / 2 + FONT_HEIGHT / 4;
					sprintf(c, "%s: %s", LNG(STARTUP), PartyInfo[browser_sel].Game.Startup);
					x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
					printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
					y += FONT_HEIGHT + FONT_HEIGHT / 2;
					sprintf(c, "%s: %d %s", LNG(SIZE), (int)PartyInfo[browser_sel].RawSize, LNG(MB));
					x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
					printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
					y += FONT_HEIGHT + FONT_HEIGHT / 2;
					x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) -
					    (strlen(LNG(TYPE_DVD_GAME)) * FONT_WIDTH) / 2;
					if (PartyInfo[browser_sel].Game.Is_Dvd == 1)
						printXY(LNG(TYPE_DVD_GAME), x, y,
						        setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
					else
						printXY(LNG(TYPE_CD_GAME), x, y,
						        setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
					pfsFree = -1;  //Disable lower pie chart display
					//---------- End of clause for HDL game partitions ----------
				} else {  //ends clause for HDL, starts clause for normal partitions
					//---------- Start of clause for PFS partitions ----------

					sprintf(c, "%s: %d %s", LNG(PFS_SIZE), (int)PartyInfo[browser_sel].TotalSize, LNG(MB));
					x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
					printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
					y += FONT_HEIGHT;
					sprintf(c, "%s: %d %s", LNG(PFS_USED), (int)PartyInfo[browser_sel].UsedSize, LNG(MB));
					printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
					y += FONT_HEIGHT;
					sprintf(c, "%s: %d %s", LNG(PFS_FREE), (int)PartyInfo[browser_sel].FreeSize, LNG(MB));
					printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));

					pfsFree = (PartyInfo[browser_sel].FreeSize * 100) / PartyInfo[browser_sel].TotalSize;

					//---------- End of clause for normal partitions (not HDL games) ----------
				}  //ends clause for normal partitions

				if (pfsFree >= 0) {  //Will be negative when skipping this graph
					x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x);
					if (TV_mode != TV_mode_PAL)
						y += ray + 20;
					else
						y += ray + 25;

					Angle = 0;

					for (i = 0; i < 360; i++) {
						if ((Angle = i - 90) >= 360)
							Angle = i + 270;
						if (((i * 100) / 360) >= pfsFree)
							Color = setting->color[COLOR_GRAPH2];
						else
							Color = setting->color[COLOR_GRAPH1];
						x3 = ray * cosdgf(Angle);
						if (TV_mode != TV_mode_PAL)
							y3 = (ray - 5) * sindgf(Angle);
						else
							y3 = (ray)*sindgf(Angle);
						x2 = x + x3;
						y2 = y + (y3);
						gsKit_prim_line(gsGlobal, x, y, x2, y2, 1, Color);
						gsKit_prim_line(gsGlobal, x, y, x2 + 1, y2, 1, Color);
						gsKit_prim_line(gsGlobal, x, y, x2, y2 + 1, 1, Color);
					}

					sprintf(c, "%d%% %s", pfsFree, LNG(FREE));
					printXY(c, x - FONT_WIDTH * 4, y - FONT_HEIGHT / 2, setting->color[COLOR_TEXT], TRUE, 0);
				}

				rows = (Menu_end_y - Menu_start_y) / FONT_HEIGHT;

				x = SCREEN_WIDTH / 2 - FONT_WIDTH;
				y = Menu_start_y;

				for (i = 0; i < rows; i++) {
					if (top + i >= browser_nfiles)
						break;
					if (top + i == browser_sel)
						Color = setting->color[COLOR_SELECT];  //Highlight cursor line
					else
						Color = setting->color[COLOR_TEXT];

					strcpy(tmp, PartyInfo[top + i].Name);
					printXY(tmp, x + 4, y, Color, TRUE, ((SCREEN_WIDTH - SCREEN_MARGIN) - (SCREEN_WIDTH / 2 - FONT_WIDTH)));
					y += FONT_HEIGHT;
				}                             //ends for, so all browser rows were fixed above
				if (browser_nfiles > rows) {  //if more files than available rows, use scrollbar
					drawFrame(SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 8, Frame_start_y,
					          SCREEN_WIDTH - SCREEN_MARGIN, Frame_end_y, setting->color[COLOR_FRAME]);
					y0 = (Menu_end_y - Menu_start_y + 8) * ((double)top / browser_nfiles);
					y1 = (Menu_end_y - Menu_start_y + 8) * ((double)(top + rows) / browser_nfiles);
					drawOpSprite(setting->color[COLOR_FRAME],
					             SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 6, (y0 + Menu_start_y - 4),
					             SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 2, (y1 + Menu_start_y - 4));
				}  //ends clause for scrollbar
			}      //ends hdd formated
			//Tooltip section
			snprintf(tooltip, sizeof(tooltip), "R1:%s L1:%s  \xFF"
			                                   "3:%s",
			         LNG(MENU), LNG(Status_Info), LNG(Exit));
			if (PartyInfo[browser_sel].Treatment == TREAT_HDL_RAW) {
				sprintf(tmp, " \xFF"
				             "2:%s",
				        LNG(Load_HDL_Game_Info));
				strcat(tooltip, tmp);
			}
			if (PartyInfo[browser_sel].Treatment == TREAT_HDL_GAME) {
				sprintf(tmp, " \xFF"
				             "2:%s",
				        LNG(Unload_HDL_Game_Info));
				strcat(tooltip, tmp);
			}
			setScrTmp(LNG(PS2_HDD_MANAGER), tooltip);

		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	}  //ends while

	return;
}
//------------------------------
//endfunc hddManager
//--------------------------------------------------------------
