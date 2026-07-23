//--------------------------------------------------------------
//File name:   filer_browser.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "gui_texteditor.h"
#include "filer_actions.h"
#include "filer_shared.h"
#include "gui_hdd0_format.h"
#include "init.h"

#define SOURCE_DEVICE_WAIT_INTERVAL_MS 1000
#define SOURCE_DEVICE_WAIT_TIMEOUT_MS 6000

static int isHddBrowserPath(const char *path)
{
	return (!strncmp(path, "hdd", 3) && path[3] >= '0' && path[3] <= '9' && path[4] == ':' && path[5] == '/');
}

static int isHddRootPath(const char *path)
{
	return (isHddBrowserPath(path) && path[6] == '\0');
}

static int isGenericUsbRootPath(const char *path)
{
	return (!strcmp(path, "usb:") || !strcmp(path, "usb:/"));
}

static const char *getUsbRootDeviceLabel(char unit)
{
	static char label[] = "usb0:/";

	if ((unit < '0') || (unit > '9'))
		return NULL;

	label[3] = unit;
	return label;
}

static const char *getRootDeviceLabel(const char *name)
{
	if (!strcmp(name, "mc0:"))
		return "mc0:/";
	if (!strcmp(name, "mc1:"))
		return "mc1:/";
	if (!strcmp(name, "mass:"))
		return "usb:/";
	if (!strncmp(name, "mass", 4) && name[4] >= '0' && name[4] <= '9' && name[5] == ':' && name[6] == '\0')
		return getUsbRootDeviceLabel(name[4]);
	if (!strcmp(name, "usb:"))
		return "usb:/";
	if (!strncmp(name, "usb", 3) && name[3] >= '0' && name[3] <= '9' && name[4] == ':' && name[5] == '\0')
		return getUsbRootDeviceLabel(name[3]);
#ifdef MMCE
	if (!strcmp(name, "mmce0:"))
		return "mmce0:/";
	if (!strcmp(name, "mmce1:"))
		return "mmce1:/";
#endif
#ifdef MX4SIO
	if (!strcmp(name, "mx4sio:"))
		return "mx4sio:/";
#endif
	if (!strcmp(name, "hdd0:"))
		return "hdd0:/";
	if (!strcmp(name, "hdd1:"))
		return "hdd1:/";
#ifdef EXFAT
	if (!strcmp(name, "ata:"))
		return "ata0:/";
	if (!strcmp(name, "ata0:"))
		return "ata0:/";
	if (!strcmp(name, "ata1:"))
		return "ata1:/";
#endif
	if (!strcmp(name, "cdfs:"))
		return "cdfs:/";
#ifdef XFROM
	if (!strcmp(name, "xfrom0:") || !strcmp(name, "xfrom:"))
		return "xfrom:/";
#endif
#ifdef DVRP
	if (!strcmp(name, "dvr_hdd0:"))
		return "dvr:/";
#endif
#ifdef ETH
	if (!strcmp(name, "host:"))
		return "host:/";
#endif
#ifdef UDPFS
	if (!strcmp(name, "udpfs:"))
		return "udpfs:/";
#endif
	if (!strcmp(name, LNG(MISC)))
		return "MISC/";

	return NULL;
}

static void waitUntilTimer(u64 end_time)
{
	while (Timer() < end_time) {
	}
}

static int probeDirectory(const char *path)
{
	char probe_path[MAX_PATH];
	int fd;

	if (path == NULL || path[0] == '\0')
		return FALSE;

	snprintf(probe_path, sizeof(probe_path), "%s", path);
	fd = genDopen(probe_path);
	if (fd < 0)
		return FALSE;
	genDclose(fd);
	return TRUE;
}

static void markMx4sioDestinationAfterWrite(const char *path)
{
	if (path == NULL || strncmp(path, "mx4sio", 6))
		return;

	discardNextMx4sioRootListing(path);
}

static void makeDeviceRootPath(const char *path, char *root, int root_size)
{
	const char *separator;
	int len;

	if (root_size <= 0)
		return;
	root[0] = '\0';
	if (path == NULL)
		return;

	separator = strchr(path, ':');
	if (separator == NULL)
		return;

	len = (int)(separator - path) + 1;
	if (len >= root_size)
		len = root_size - 1;
	memcpy(root, path, len);
	root[len] = '\0';

#if defined(ETH) || defined(UDPFS)
	if (!strncmp(root, "host:", 5))
		return;
#endif

	if (len + 1 < root_size) {
		root[len++] = '/';
		root[len] = '\0';
	}
}

static int clipboardHddSourceReady(void)
{
	char party[MAX_NAME], dir[MAX_PATH];
	int pfs_ix;

	if (getHddParty(clipPath, NULL, party, dir) < 0)
		return FALSE;
	if (!ensurePathDeviceStackReady(clipPath))
		return FALSE;
	pfs_ix = mountParty(party);
	return (pfs_ix >= 0);
}

#ifdef DVRP
static int clipboardDvrHddSourceReady(void)
{
	char party[MAX_NAME], dir[MAX_PATH];
	int pfs_ix;

	if (getHddDVRPParty(clipPath, NULL, party, dir) < 0)
		return FALSE;
	if (!ensurePathDeviceStackReady(clipPath))
		return FALSE;
	pfs_ix = mountDVRPParty(party);
	return (pfs_ix >= 0);
}
#endif

static int clipboardSourceDeviceReady(void)
{
	char fixed_path[MAX_PATH], root_path[MAX_PATH];

	if (clipPath[0] == '\0')
		return FALSE;
	if (!strncmp(clipPath, "mc", 2))
		return TRUE;
	if (!strncmp(clipPath, "vmc", 3)) {
		int vmc_index = clipPath[3] - '0';

		return (vmc_index >= 0 && vmc_index < 2 && vmcMounted[vmc_index]);
	}
	if (isHddBrowserPath(clipPath))
		return clipboardHddSourceReady();
#ifdef DVRP
	if (!strncmp(clipPath, "dvr_hdd", 7))
		return clipboardDvrHddSourceReady();
#endif

	if (!ensurePathDeviceStackReady(clipPath))
		return FALSE;
	if (genFixPath(clipPath, fixed_path) < 0)
		return FALSE;
	makeDeviceRootPath(fixed_path, root_path, sizeof(root_path));
	if (probeDirectory(root_path))
		return TRUE;
	return probeDirectory(fixed_path);
}

static int waitForClipboardSourceDevice(void)
{
	u64 deadline, next_check;

	if (clipIopResetGeneration == getIopResetGeneration())
		return 0;

	deadline = Timer() + SOURCE_DEVICE_WAIT_TIMEOUT_MS;
	while (1) {
		if (clipboardSourceDeviceReady()) {
			clipIopResetGeneration = getIopResetGeneration();
			return 0;
		}
		if (Timer() >= deadline)
			break;

		drawMsg(LNG(Pasting));
		next_check = Timer() + SOURCE_DEVICE_WAIT_INTERVAL_MS;
		if (next_check > deadline)
			next_check = deadline;
		waitUntilTimer(next_check);
	}

	return -1;
}

static void formatBrowserPathForDisplay(const char *path, char *display_path)
{
	const char *partition;
	const char *subpath;
	const char *sep;
	int part_len;

	if (!strncmp(path, "mass", 4)) {
		snprintf(display_path, MAX_PATH, "usb%s", path + 4);
		return;
	}

	if (isHddBrowserPath(path)) {
		char hdd_device[6];

		memcpy(hdd_device, path, 5);
		hdd_device[5] = '\0';
		partition = path + 6; // after "hdd0:/"
		if (partition[0] == '\0') {
			snprintf(display_path, MAX_PATH, "%s", path);
			return;
		}

		sep = strchr(partition, '/');
		if (sep == NULL) {
			part_len = (int)strlen(partition);
			subpath = "/";
		} else {
			part_len = (int)(sep - partition);
			subpath = sep;
		}

		if (part_len > 0) {
			snprintf(display_path, MAX_PATH, "%s%.*s:pfs:%s", hdd_device, part_len, partition, subpath);
			return;
		}
	}

#ifdef DVRP
	if (!strncmp(path, "dvr_hdd0:/", 10)) {
		partition = path + 10; // after "dvr_hdd0:/"
		if (partition[0] == '\0') {
			snprintf(display_path, MAX_PATH, "%s", path);
			return;
		}

		sep = strchr(partition, '/');
		if (sep == NULL) {
			part_len = (int)strlen(partition);
			subpath = "/";
		} else {
			part_len = (int)(sep - partition);
			subpath = sep;
		}

		if (part_len > 0) {
			snprintf(display_path, MAX_PATH, "dvr_hdd0:%.*s:pfs:%s", part_len, partition, subpath);
			return;
		}
	}
#endif

	snprintf(display_path, MAX_PATH, "%s", path);
}

static int isTitleCfgPathEligible(const char *path, int menu_disabled)
{
	return ((!strncmp(path, "mass", 4)) ||
	        (!strncmp(path, "usb", 3)) ||
#ifdef MMCE
	        (!strncmp(path, "mmce", 4)) ||
#endif
#ifdef MX4SIO
	        (!strncmp(path, "mx4sio", 6)) ||
#endif
	        (!strncmp(path, "ata", 3)) ||
	        (isHddBrowserPath(path) && !menu_disabled));
}

enum {
	PSU_ACTION_NONE = 0,
	PSU_ACTION_CREATE,
	PSU_ACTION_EXTRACT
};

static int isMcLikePath(const char *path)
{
	return (!strncmp(path, "mc", 2) || !strncmp(path, "vmc", 3));
}

static int classifyPsuAction(const char *destPath)
{
	int i;
	int all_psu = 1;
	int all_dirs = 1;
	int src_is_mc_like;

	if (nclipFiles <= 0)
		return PSU_ACTION_NONE;

	src_is_mc_like = isMcLikePath(clipPath);
	for (i = 0; i < nclipFiles; i++) {
		if (clipFiles[i].stats.AttrFile & sceMcFileAttrSubdir) {
			all_psu = 0;
			continue;
		}

		all_dirs = 0;
		if (!genCmpFileExt(clipFiles[i].name, "PSU"))
			all_psu = 0;
	}

	if (all_psu)
		return PSU_ACTION_EXTRACT;

	if (all_dirs && src_is_mc_like && destPath && destPath[0] != '\0' && !isMcLikePath(destPath))
		return PSU_ACTION_CREATE;

	return PSU_ACTION_NONE;
}

static int menu(const char *path, FILEINFO *file)
{
	u64 color;
	char enable[NUM_MENU], tmp[80];
	const char *psu_action_label;
	int x, y, i, sel;
	int event, post_event = 0;
	int menu_disabled = 0;
	int write_disabled = 0;
	int psu_action;

	psu_action = classifyPsuAction(path);
	if (psu_action == PSU_ACTION_EXTRACT)
		psu_action_label = LNG(Extract_PSU);
	else if (psu_action == PSU_ACTION_CREATE)
		psu_action_label = LNG(Create_PSU);
	else
		psu_action_label = LNG(psuAction);

	int menu_len = strlen(LNG(Copy)) > strlen(LNG(Cut)) ?
	                   strlen(LNG(Copy)) :
	                   strlen(LNG(Cut));
	menu_len = strlen(LNG(Paste)) > menu_len ? strlen(LNG(Paste)) : menu_len;
	menu_len = strlen(LNG(Delete)) > menu_len ? strlen(LNG(Delete)) : menu_len;
	menu_len = strlen(LNG(Rename)) > menu_len ? strlen(LNG(Rename)) : menu_len;
	menu_len = strlen(LNG(New_Dir)) > menu_len ? strlen(LNG(New_Dir)) : menu_len;
	menu_len = strlen(LNG(Get_Size)) > menu_len ? strlen(LNG(Get_Size)) : menu_len;
	menu_len = strlen(LNG(TextEditor)) > menu_len ? strlen(LNG(TextEditor)) : menu_len;
	menu_len = strlen(LNG(Launch_With_Args)) > menu_len ? strlen(LNG(Launch_With_Args)) : menu_len;
	menu_len = strlen(psu_action_label) > menu_len ? strlen(psu_action_label) : menu_len;
	menu_len = strlen(LNG(time_manip)) > menu_len ? strlen(LNG(time_manip)) : menu_len;
	menu_len = strlen(LNG(title_cfg)) > menu_len ? strlen(LNG(title_cfg)) : menu_len;
	menu_len = (strlen(LNG(Mount)) + 6) > menu_len ? (strlen(LNG(Mount)) + 6) : menu_len;
	

	int menu_ch_w = menu_len + 1;                                 //Total characters in longest menu string
	int menu_ch_h = NUM_MENU;                                     //Total number of menu lines
	int mSprite_Y1 = 64;                                          //Top edge of sprite
	int mSprite_X2 = SCREEN_WIDTH - 35;                           //Right edge of sprite
	int mSprite_X1 = mSprite_X2 - (menu_ch_w + 3) * FONT_WIDTH;   //Left edge of sprite
	int mSprite_Y2 = mSprite_Y1 + (menu_ch_h + 1) * FONT_HEIGHT;  //Bottom edge of sprite

	memset(enable, TRUE, NUM_MENU);  //Assume that all menu items are legal by default

	//identify cases where write access is illegal, and disable menu items accordingly
	if ((!strncmp(path, "cdfs", 4))  //Writing is always illegal for CDVD drive
#if defined(ETH) && defined(UDPFS)
	    || ((!strncmp(path, "host", 4) || !strncmp(path, "udpfs", 5))
#elif defined(ETH)
	    || ((!strncmp(path, "host", 4))
#elif defined(UDPFS)
	    || ((!strncmp(path, "udpfs", 5))
#endif
#if defined(ETH) || defined(UDPFS)
	        && ((!setting->HOSTwrite)                         //host/udpfs writing is illegal if not enabled in CNF
	            || (host_elflist && !strcmp(path, "host:/"))  //it's also illegal in elflist.txt
	            ))
#endif
				)
		write_disabled = 1;

	if ((isHddRootPath(path) || !strcmp(path, "dvr_hdd0:/")) || path[0] == 0)  //No menu cmds in partition/device lists
		menu_disabled = 1;

	if (menu_disabled) {
		enable[COPY] = FALSE;
		enable[MOUNTVMC0] = FALSE;
		enable[MOUNTVMC1] = FALSE;
		enable[GETSIZE] = FALSE;
	}
//#ifdef TMANIP
	if (                                                        //if
	    (file->stats.AttrFile & sceMcFileAttrSubdir) &&         //pointing to a folder
	    (strcmp(file->name, "..")) &&                           //it isnt the ".." option
	    ((!strcmp(path, "mc0:/")) || (!strcmp(path, "mc1:/")))  //we're on Memory card roots
	) {
		enable[TIMEMANIP] = TRUE;
	} else {
		enable[TIMEMANIP] = FALSE;
	} 
//#endif //TMANIP
	if (genCmpFileExt(file->name, "ELF") && isTitleCfgPathEligible(path, menu_disabled))
		enable[TITLE_CFG] = TRUE;
	else
		enable[TITLE_CFG] = FALSE;

	enable[OPEN_TEXTEDITOR] = canOpenInTextEditor(path, file);
	enable[LAUNCH_ELF_ARGS] = enable[OPEN_TEXTEDITOR];


	if (write_disabled || menu_disabled) {
		enable[CUT] = FALSE;
		enable[PASTE] = FALSE;
		enable[PSUPASTE] = FALSE;
		enable[DELETE] = FALSE;
		enable[RENAME] = FALSE;
		enable[NEWDIR] = FALSE;
		enable[NEWICON] = FALSE;
		enable[TITLE_CFG] = FALSE;
	}

	if (nmarks == 0) {
		if (!strcmp(file->name, "..")) {
			enable[COPY] = FALSE;
			enable[CUT] = FALSE;
			enable[DELETE] = FALSE;
			enable[RENAME] = FALSE;
			enable[GETSIZE] = FALSE;
		}
		if (filerIsExploitProtectedPath(path, file))
			enable[RENAME] = FALSE;
	} else {
		enable[RENAME] = FALSE;
	}

	if ((file->stats.AttrFile & sceMcFileAttrSubdir) || !strncmp(path, "vmc", 3) || !strncmp(path, "mc", 2)) {
		enable[MOUNTVMC0] = FALSE;  //forbid insane VMC mounting
		enable[MOUNTVMC1] = FALSE;  //forbid insane VMC mounting
	}

	if (nclipFiles == 0) {
		//Nothing in clipboard
		enable[PASTE] = FALSE;
		enable[PSUPASTE] = FALSE;
	} else {
		//Something in clipboard
		enable[PSUPASTE] = (enable[PSUPASTE] && (psu_action != PSU_ACTION_NONE));
	}

	for (sel = 0; sel < NUM_MENU; sel++)  //loop to preselect the first enabled menu entry
		if (enable[sel] == TRUE)
			break;  //break loop if sel is at an enabled menu entry

	event = 1;  //event = initial entry
	while (1) {
		//Pad response section
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_UP && sel < NUM_MENU) {
				event |= 2;  //event |= valid pad command
				do {
					sel--;
					if (sel < 0)
						sel = NUM_MENU - 1;
				} while (!enable[sel]);
			} else if (new_pad & PAD_DOWN && sel < NUM_MENU) {
				event |= 2;  //event |= valid pad command
				do {
					sel++;
					if (sel == NUM_MENU)
						sel = 0;
				} while (!enable[sel]);
			} else if ((new_pad & PAD_TRIANGLE) || (!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				return -1;
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
				event |= 2;  //event |= valid pad command
				break;
			} else if (new_pad & PAD_SQUARE && sel == PASTE) {
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

			for (i = 0, y = mSprite_Y1 + FONT_HEIGHT / 2; i < NUM_MENU; i++) {
				if (i == COPY)
					strcpy(tmp, LNG(Copy));
				else if (i == CUT)
					strcpy(tmp, LNG(Cut));
				else if (i == PASTE)
					strcpy(tmp, LNG(Paste));
				else if (i == PSUPASTE)
					strcpy(tmp, psu_action_label);
				else if (i == DELETE)
					strcpy(tmp, LNG(Delete));
				else if (i == RENAME)
					strcpy(tmp, LNG(Rename));
				else if (i == NEWDIR)
					strcpy(tmp, LNG(New_Dir));
				else if (i == NEWICON)
					strcpy(tmp, LNG(New_Icon));
				else if (i == MOUNTVMC0)
					sprintf(tmp, "%s vmc0:", LNG(Mount));
				else if (i == MOUNTVMC1)
					sprintf(tmp, "%s vmc1:", LNG(Mount));
				else if (i == GETSIZE)
					strcpy(tmp, LNG(Get_Size));
				else if (i == OPEN_TEXTEDITOR)
					strcpy(tmp, LNG(TextEditor));
				else if (i == LAUNCH_ELF_ARGS)
					strcpy(tmp, LNG(Launch_With_Args));
				else if (i == TITLE_CFG)
					strcpy(tmp, LNG(title_cfg));
#ifdef TMANIP
				else if (i == TIMEMANIP)
					strcpy(tmp, LNG(time_manip));
#endif //TMANIP

				if (enable[i])
					color = setting->color[COLOR_TEXT];
				else
					color = setting->color[COLOR_FRAME];

				printXY(tmp, mSprite_X1 + 2 * FONT_WIDTH, y, color, TRUE, 0);
				y += FONT_HEIGHT;
			}
			if (sel < NUM_MENU)
				drawChar(LEFT_CUR, mSprite_X1 + FONT_WIDTH, mSprite_Y1 + (FONT_HEIGHT / 2 + sel * FONT_HEIGHT), setting->color[COLOR_TEXT]);

			//Tooltip section
			x = SCREEN_MARGIN;
			y = Menu_tooltip_y;
			drawSprite(setting->color[COLOR_BACKGR],
			           0, y - 1,
			           SCREEN_WIDTH, y + FONT_HEIGHT);
			if (swapKeys)
				sprintf(tmp, "\xFF"
				             "1:%s \xFF"
				             "0:%s",
				        LNG(OK), LNG(Cancel));
			else
				sprintf(tmp, "\xFF"
				             "0:%s \xFF"
				             "1:%s",
				        LNG(OK), LNG(Cancel));
			if (sel == PASTE)
				sprintf(tmp + strlen(tmp), " \xFF"
				                           "2:%s",
				        LNG(PasteRename));
			sprintf(tmp + strlen(tmp), " \xFF"
			                           "3:%s",
			        LNG(Back));
			printXY(tmp, x, y, setting->color[COLOR_SELECT], TRUE, 0);
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	}  //ends while
	return sel;
}  //ends menu

char *PathPad_menu(const char *path)
{
	u64 color;
	int x, y, dx, dy, dw, dh;
	int a = 6, b = 4, c = 2, tw, th;
	int i, sel_x, sel_y;
	int event, post_event = 0;
	char textrow[80], tmp[64];

	th = 10 * FONT_HEIGHT;          //Height in pixels of text area
	tw = 68 * FONT_WIDTH;           //Width in pixels of max text row
	dh = th + 2 * 2 + a + b + c;    //Height in pixels of entire frame
	dw = tw + 2 * 2 + a * 2;        //Width in pixels of entire frame
	dx = (SCREEN_WIDTH - dw) / 2;   //X position of frame (centred)
	dy = (SCREEN_HEIGHT - dh) / 2;  //Y position of frame (centred)

	sel_x = 0;
	sel_y = 0;
	event = 1;  //event = initial entry
	while (1) {
		//Pad response section
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad) {
				event |= 2;  //event |= valid pad command
				if (new_pad & PAD_UP) {
					sel_y--;
					if (sel_y < 0)
						sel_y = 9;
				} else if (new_pad & PAD_DOWN) {
					sel_y++;
					if (sel_y > 9)
						sel_y = 0;
				} else if (new_pad & PAD_LEFT) {
					sel_y -= 5;
					if (sel_y < 0)
						sel_y = 0;
				} else if (new_pad & PAD_RIGHT) {
					sel_y += 5;
					if (sel_y > 9)
						sel_y = 9;
				} else if (new_pad & PAD_L1) {
					sel_x--;
					if (sel_x < 0)
						sel_x = 2;
				} else if (new_pad & PAD_R1) {
					sel_x++;
					if (sel_x > 2)
						sel_x = 0;
				} else if (new_pad & PAD_TRIANGLE) {  //Pushed 'Back'
					return NULL;
				} else if (!setting->PathPad_Lock                                                            //if PathPad changes allowed ?
				           && ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE))) {  //Pushed 'Clear'
					PathPad[sel_x * 10 + sel_y][0] = '\0';
				} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {  //Pushed 'Use'
					return PathPad[sel_x * 10 + sel_y];
				} else if (!setting->PathPad_Lock && (new_pad & PAD_SQUARE)) {  //Pushed 'Set'
					strncpy(PathPad[sel_x * 10 + sel_y], path, MAX_PATH - 1);
					PathPad[sel_x * 10 + sel_y][MAX_PATH - 1] = '\0';
				}
			}  //ends 'if(new_pad)'
		}      //ends 'if(readpad())'

		if (event || post_event) {  //NB: We need to update two frame buffers per event

			//Display section
			drawSprite(setting->color[COLOR_BACKGR],
			           0, (Menu_message_y - 1),
			           SCREEN_WIDTH, (Frame_start_y));
			drawPopSprite(setting->color[COLOR_BACKGR],
			              dx, dy,
			              dx + dw, (dy + dh));
			drawFrame(dx, dy, dx + dw, (dy + dh), setting->color[COLOR_FRAME]);
			for (i = 0; i < 10; i++) {
				if (i == sel_y)
					color = setting->color[COLOR_SELECT];
				else
					color = setting->color[COLOR_TEXT];
				sprintf(textrow, "%02d=", (sel_x * 10 + i));
				strncat(textrow, PathPad[sel_x * 10 + i], 64);
				textrow[67] = '\0';
				printXY(textrow, dx + 2 + a, (dy + a + 2 + i * FONT_HEIGHT), color, TRUE, 0);
			}

			//Tooltip section
			x = SCREEN_MARGIN;
			y = Menu_tooltip_y;
			drawSprite(setting->color[COLOR_BACKGR], 0, y - 1, SCREEN_WIDTH, y + FONT_HEIGHT);

			if (swapKeys) {
				sprintf(textrow, "\xFF"
				                 "1:%s ",
				        LNG(Use));
				if (!setting->PathPad_Lock) {
					sprintf(tmp, "\xFF"
					             "0:%s \xFF"
					             "2:%s ",
					        LNG(Clear), LNG(Set));
					strcat(textrow, tmp);
				}
			} else {
				sprintf(textrow, "\xFF"
				                 "0:%s ",
				        LNG(Use));
				if (!setting->PathPad_Lock) {
					sprintf(tmp, "\xFF"
					             "1:%s \xFF"
					             "2:%s ",
					        LNG(Clear), LNG(Set));
					strcat(textrow, tmp);
				}
			}
			sprintf(tmp, "\xFF"
			             "3:%s L1/R1:%s",
			        LNG(Back), LNG(Page_leftright));
			strcat(textrow, tmp);
			printXY(textrow, x, y, setting->color[COLOR_SELECT], TRUE, 0);
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	}  //ends while
}
//------------------------------
//endfunc PathPad_menu

static int BrowserModePopup(void)
{
	char tmp[80];
	int y, i, test;
	int event, post_event = 0;

	int entry_file_show = file_show;
	int entry_file_sort = file_sort;

	int Show_len = strlen(LNG(Show_Content_as)) + 1;
	int Sort_len = strlen(LNG(Sort_Content_by)) + 1;

	int menu_len = Show_len;
	if (menu_len < (i = Sort_len))
		menu_len = i;
	if (menu_len < (i = strlen(LNG(Filename)) + strlen(LNG(_plus_Details))))
		menu_len = i;
	if (menu_len < (i = strlen(LNG(Game_Title)) + strlen(LNG(_plus_Details))))
		menu_len = i;
	if (menu_len < (i = strlen(LNG(No_Sort))))
		menu_len = i;
	if (menu_len < (i = strlen(LNG(Timestamp))))
		menu_len = i;
	if (menu_len < (i = strlen(LNG(Back_to_Browser))))
		menu_len = i;
	menu_len += 3;  //All of the above strings are indented 3 spaces, for tooltips

	int menu_ch_w = menu_len + 1;  //Total characters in longest menu string
	int menu_ch_h = 14;            //Total number of menu lines
	int mSprite_w = (menu_ch_w + 3) * FONT_WIDTH;
	int mSprite_h = (menu_ch_h + 1) * FONT_HEIGHT;
	int mSprite_X1 = SCREEN_WIDTH / 2 - mSprite_w / 2;
	int mSprite_Y1 = SCREEN_HEIGHT / 2 - mSprite_h / 2;
	int mSprite_X2 = mSprite_X1 + mSprite_w;
	int mSprite_Y2 = mSprite_Y1 + mSprite_h;

	char minuses_s[81];

	for (i = 0; i < 80; i++)
		minuses_s[i] = '-';
	minuses_s[80] = '\0';

	event = 1;  //event = initial entry
	while (1) {
		//Pad response section
		waitPadReady(0, 0);
		if (readpad()) {
			switch (new_pad) {
				case PAD_RIGHT:
					file_sort = 0;
					event |= 2;  //event |= valid pad command
					break;
				case PAD_DOWN:
					file_sort = 1;
					event |= 2;  //event |= valid pad command
					break;
				case PAD_LEFT:
					file_sort = 2;
					event |= 2;  //event |= valid pad command
					break;
				case PAD_UP:
					file_sort = 3;
					event |= 2;  //event |= valid pad command
					break;
				case PAD_CIRCLE:
					file_show = 0;
					event |= 2;  //event |= valid pad command
					break;
				case PAD_CROSS:
					file_show = 1;
					event |= 2;  //event |= valid pad command
					break;
				case PAD_SQUARE:
					file_show = 2;
					event |= 2;  //event |= valid pad command
					if ((file_show == 2) && (elisaFnt == NULL) && (elisa_failed == FALSE)) {
						int fd, res;
						elisa_failed = TRUE;  //Default to FAILED. If it succeeds, then this status will be cleared.

						res = genFixPath("uLE:/ELISA100.FNT", tmp);
						if (!strncmp(tmp, "cdrom", 5))
							strcat(tmp, ";1");
						if (res >= 0) {
							fd = genOpen(tmp, FIO_O_RDONLY);
							if (fd >= 0) {
								test = genLseek(fd, 0, SEEK_END);
								if (test == 55016) {
									elisaFnt = (unsigned char *)memalign(64, test);
									genLseek(fd, 0, SEEK_SET);
									genRead(fd, elisaFnt, test);

									elisa_failed = FALSE;
								}
								genClose(fd);
							}
						}
					}
					break;
				case PAD_TRIANGLE:
					return (file_show != entry_file_show) || (file_sort != entry_file_sort);
			}  //ends switch(new_pad)
		}      //ends if(readpad())

		if (event || post_event) {  //NB: We need to update two frame buffers per event

			//Display section
			drawPopSprite(setting->color[COLOR_BACKGR],
			              mSprite_X1, mSprite_Y1,
			              mSprite_X2, mSprite_Y2);
			drawFrame(mSprite_X1, mSprite_Y1, mSprite_X2, mSprite_Y2, setting->color[COLOR_FRAME]);

			for (i = 0, y = mSprite_Y1 + FONT_HEIGHT / 2; i < menu_ch_h; i++) {
				if (i == 0)
					sprintf(tmp, "   %s:", LNG(Show_Content_as));
				else if (i == 1)
					sprintf(tmp, "   %s", &minuses_s[80 - Show_len]);
				else if (i == 2)
					sprintf(tmp, "\xFF"
					             "0 %s",
					        LNG(Filename));
				else if (i == 3)
					sprintf(tmp, "\xFF"
					             "1 %s%s",
					        LNG(Filename), LNG(_plus_Details));
				else if (i == 4)
					sprintf(tmp, "\xFF"
					             "2 %s%s",
					        LNG(Game_Title), LNG(_plus_Details));
				else if (i == 6)
					sprintf(tmp, "   %s:", LNG(Sort_Content_by));
				else if (i == 7)
					sprintf(tmp, "   %s", &minuses_s[80 - Sort_len]);
				else if (i == 8)
					sprintf(tmp, "\xFF"
					             ": %s",
					        LNG(No_Sort));
				else if (i == 9)
					sprintf(tmp, "\xFF"
					             "; %s",
					        LNG(Filename));
				else if (i == 10)
					sprintf(tmp, "\xFF"
					             "< %s",
					        LNG(Game_Title));
				else if (i == 11)
					sprintf(tmp, "\xFF"
					             "= %s",
					        LNG(Timestamp));
				else if (i == 13)
					sprintf(tmp, "\xFF"
					             "3 %s",
					        LNG(Back_to_Browser));
				else
					tmp[0] = 0;

				printXY(tmp, mSprite_X1 + 2 * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
				//Display marker for current modes
				if ((file_show == i - 2) || (file_sort == i - 8))
					drawChar(LEFT_CUR, mSprite_X1 + FONT_WIDTH / 2, y, setting->color[COLOR_SELECT]);
				y += FONT_HEIGHT;

			}  //ends for loop handling one text row per loop

			//Tooltip section
			// x = SCREEN_MARGIN;
			y = Menu_tooltip_y;
			drawSprite(setting->color[COLOR_BACKGR],
			           0, y - 1,
			           SCREEN_WIDTH, y + FONT_HEIGHT);
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	}  //ends while
}
//------------------------------
//endfunc BrowserModePopup

// get_FilePath is the main browser function.
// It also contains the menu handler for the R1 submenu
// The static variables declared here are only for the use of
// this function and the submenu functions that it calls
//--------------------------------------------------------------
// sincro: ADD USBD_IRX_CNF mode for found IRX file for USBD.IRX
// example: getFilePath(setting->usbd_file, USBD_IRX_CNF);
// getFilePath selects a path according to the requested configuration mode.
// dlanor: ADD USBKBD_IRX_CNF mode for found IRX file for USBKBD.IRX
// example: getFilePath(setting->usbkbd_file, USBKBD_IRX_CNF);
// dlanor: ADD USBMASS_IRX_CNF mode for found IRX file for usb_mass
// example: getFilePath(setting->usbmass_file, USBMASS_IRX_CNF);
// dlanor: ADD SAVE_CNF mode returning either pure path or pathname
// dlanor: ADD return value 0=pure path, 1=pathname, negative=error/no_selection
static int browser_cd, browser_up, browser_repos, browser_pushed;
static int browser_sel, browser_nfiles;
static void submenu_func_GetSize(char *mess, char *path, FILEINFO *files);
static void submenu_func_Paste(char *mess, char *path);
static void submenu_func_psuPaste(char *mess, char *path);

static int isRootSpacerEntry(const char *path, const FILEINFO *file)
{
	return path[0] == '\0' && file->name[0] == '\0';
}

static void skipRootSpacerSelection(const char *path, FILEINFO *files, int nfiles, int *sel, int direction)
{
	int original;

	if (nfiles <= 0)
		return;

	if (direction == 0)
		direction = 1;

	original = *sel;
	while (isRootSpacerEntry(path, &files[*sel])) {
		*sel += direction;
		if (*sel >= nfiles)
			*sel = 0;
		else if (*sel < 0)
			*sel = nfiles - 1;
		if (*sel == original)
			break;
	}
}

int getFilePath(char *out, int cnfmode)
{
	char path[MAX_PATH], cursorEntry[MAX_PATH],
	    msg0[MAX_PATH], msg1[MAX_PATH],
	    tmp[MAX_PATH], tmp1[MAX_PATH], tmp2[MAX_PATH], ext[8], *p;
	const unsigned char *mcTitle;
	u64 color;
	FILEINFO files[MAX_ENTRY];
	int top = 0, rows;
	int x, y, y0, y1;
	int i, j, ret, rv = -1;  //NB: rv is for return value of this function
	int usb_unit;
	int event, post_event = 0;
	int font_height;
	int iconbase, iconcolr;

	elisa_failed = FALSE;  //set at failure to load font, cleared at each browser entry

	browser_cd = TRUE;
	browser_up = FALSE;
	browser_repos = FALSE;
	browser_pushed = TRUE;
	browser_sel = 0;
	browser_nfiles = 0;

	strcpy(ext, cnfmode_extL[cnfmode]);

	if ((cnfmode == USBD_IRX_CNF) || (cnfmode == USBKBD_IRX_CNF) || (cnfmode == USBMASS_IRX_CNF) || ((!strncmp(LastDir, setting->Misc, strlen(setting->Misc))) && (cnfmode > LK_ELF_CNF)))
		path[0] = '\0';  //start in main root if recent folder unreasonable
	else
		strcpy(path, LastDir);  //If reasonable, start in recent folder

	unmountAll();  //unmount all uLE-used mountpoints

	clipPath[0] = 0;
	nclipFiles = 0;
	browser_cut = 0;

	file_show = 1;
	file_sort = 1;

	font_height = FONT_HEIGHT;
	if ((file_show == 2) && (elisaFnt != NULL))
		font_height = FONT_HEIGHT + 2;
	rows = (Menu_end_y - Menu_start_y) / font_height;

	event = 1;  //event = initial entry
	while (1) {

		//Pad response section
		waitPadReady(0, 0);
		if (readpad()) {
			int step;
			if (new_pad) {
				browser_pushed = TRUE;
				event |= 2;  //event |= pad command
			}
			if (new_pad & PAD_UP) {
				if (browser_nfiles > 0) {
					if (browser_sel > 0)
						browser_sel--;
					else
						browser_sel = browser_nfiles - 1;
					skipRootSpacerSelection(path, files, browser_nfiles, &browser_sel, -1);
				}
			} else if (new_pad & PAD_DOWN) {
				if (browser_nfiles > 0) {
					if (browser_sel < browser_nfiles - 1)
						browser_sel++;
					else
						browser_sel = 0;
					skipRootSpacerSelection(path, files, browser_nfiles, &browser_sel, 1);
				}
				} else if (new_pad & PAD_LEFT) {
					if (browser_nfiles > 0) {
						step = rows / 2;
						if (step < 1)
							step = 1;
						if (browser_sel == 0)
							browser_sel = browser_nfiles - 1;
						else if (browser_sel < step)
							browser_sel = 0;
						else
							browser_sel -= step;
						skipRootSpacerSelection(path, files, browser_nfiles, &browser_sel, -1);
					}
				} else if (new_pad & PAD_RIGHT) {
					if (browser_nfiles > 0) {
						step = rows / 2;
						if (step < 1)
							step = 1;
						if (browser_sel == browser_nfiles - 1)
							browser_sel = 0;
						else if (browser_sel >= (browser_nfiles - step))
							browser_sel = browser_nfiles - 1;
						else
							browser_sel += step;
						skipRootSpacerSelection(path, files, browser_nfiles, &browser_sel, 1);
					}
				}
			else if (new_pad & PAD_TRIANGLE)
				browser_up = TRUE;
			else if ((swapKeys && (new_pad & PAD_CROSS)) || (!swapKeys && (new_pad & PAD_CIRCLE))) {  //Pushed OK
				if (files[browser_sel].stats.AttrFile & sceMcFileAttrSubdir) {
					//pushed OK for a folder
					if (!strcmp(files[browser_sel].name, ".."))
						browser_up = TRUE;
					else {
						strcat(path, files[browser_sel].name);
						strcat(path, "/");
						browser_cd = TRUE;
					}
				} else {
					//pushed OK for a file
					sprintf(out, "%s%s", path, files[browser_sel].name);
					// Must to include a function for check IRX Header
					if (((cnfmode == LK_ELF_CNF) || (cnfmode == NON_CNF)) && (!IsSupportedFileType(out))) {
						browser_pushed = FALSE;
						sprintf(msg0, "%s.", LNG(This_file_isnt_an_ELF));
						out[0] = 0;
					} else {
						strcpy(LastDir, path);
						rv = 1;  //flag pathname selected
						break;
					}
				}
			} else if (new_pad & PAD_R3) {  //New clause for uLE-relative paths
				if (files[browser_sel].stats.AttrFile & sceMcFileAttrSubdir) {
					//pushed R3 for a folder (navigate to uLE CNF folder)
					strcpy(path, LaunchElfDir);
					if ((p = strchr(path, ':'))) {                         //device separator ?
						if (p[1] != '/') {                                 //missing path separator ? (mass: & host:)
							p[1] = '/';                                    //insert path separator
							strcpy(p + 2, LaunchElfDir + (p - path) + 1);  //append rest of pathname
						}
					}
					browser_cd = TRUE;
				} else {
					//pushed R3 for a file (treat as uLE-related)
					sprintf(out, "%s%s", path, files[browser_sel].name);
					// Must to include a function for check IRX Header
					if (((cnfmode == LK_ELF_CNF) || (cnfmode == NON_CNF)) && (checkELFheader(out) < 0)) {
						browser_pushed = FALSE;
						sprintf(msg0, "%s.", LNG(This_file_isnt_an_ELF));
						out[0] = 0;
					} else {
						strcpy(LastDir, path);
						sprintf(out, "%s%s", "uLE:/", files[browser_sel].name);
						rv = 1;  //flag pathname selected
						break;
					}
				}
			} else if (new_pad & PAD_R2) {
				char *temp = PathPad_menu(path);

				if (temp != NULL) {
					strcpy(path, temp);
					browser_cd = TRUE;
					vfreeSpace = FALSE;
				}
			} else if (new_pad & PAD_L1) {
				browser_cd = BrowserModePopup();
			}
			//pad checks above are for commands common to all browser modes
			//pad checks below are for commands that differ depending on cnfmode
			if (cnfmode == DIR_CNF) {
				if (new_pad & PAD_START) {
					strcpy(out, path);
					strcpy(LastDir, path);
					rv = 0;  //flag pathname selected
					break;
				}
			} else if (cnfmode == SAVE_CNF) {  //Generic Save commands
				if (new_pad & PAD_START) {
					if (files[browser_sel].stats.AttrFile & sceMcFileAttrSubdir) {
						//no file was highlighted, so prep to save with empty filename
						strcpy(out, path);
						rv = 0;  //flag pure path selected
					} else {
						//a file was highlighted, so prep to save with that filename
						sprintf(out, "%s%s", path, files[browser_sel].name);
						rv = 1;  //flag pathname selected
					}
					strcpy(LastDir, path);
					break;
				}
			}
			if (cnfmode) {  //A file is to be selected, not in normal browser mode
				if (new_pad & PAD_SQUARE) {
					if (!strcmp(ext, "*"))
						strcpy(ext, cnfmode_extL[cnfmode]);
					else
						strcpy(ext, "*");
					browser_cd = TRUE;
				} else if ((!swapKeys && (new_pad & PAD_CROSS)) || (swapKeys && (new_pad & PAD_CIRCLE))) {  //Cancel command ?
					LaunchArgsClear();
					unmountAll();
					return rv;
				}
			} else {  //cnfmode == FALSE
				if (new_pad & PAD_R1) {
					ret = menu(path, &files[browser_sel]);
					if (ret == COPY || ret == CUT) {
						if (ret == CUT) {
							const FILEINFO *protected_file = NULL;

							if (nmarks > 0) {
								for (i = 0; i < browser_nfiles; i++) {
									if (marks[i] && filerIsExploitProtectedPath(path, &files[i])) {
										protected_file = &files[i];
										break;
									}
								}
							} else if (filerIsExploitProtectedPath(path, &files[browser_sel]))
								protected_file = &files[browser_sel];
							if (protected_file != NULL) {
								if (filerConfirmExploitModify(path, protected_file) < 0) {
									browser_pushed = FALSE;
									continue;
								}
							}
						}
						strcpy(clipPath, path);
						if (nmarks > 0) {
							for (i = nclipFiles = 0; i < browser_nfiles; i++)
								if (marks[i] && strcmp(files[i].name, ".") && strcmp(files[i].name, ".."))
									clipFiles[nclipFiles++] = files[i];
							if (nclipFiles == 0) {
								strcpy(msg0, LNG(Failed));
								browser_pushed = FALSE;
								continue;
							}
						} else {
							if (!strcmp(files[browser_sel].name, ".") || !strcmp(files[browser_sel].name, "..")) {
								strcpy(msg0, LNG(Failed));
								browser_pushed = FALSE;
								continue;
							}
							clipFiles[0] = files[browser_sel];
							nclipFiles = 1;
						}
						clipIopResetGeneration = getIopResetGeneration();
						sprintf(msg0, "%s", LNG(Copied_to_the_Clipboard));
						browser_pushed = FALSE;
						if (ret == CUT)
							browser_cut = TRUE;
						else
							browser_cut = FALSE;
					}  //ends COPY and CUT
					else if (ret == DELETE) {
						if (nmarks == 0) {  //dlanor: using title was inappropriate here (filesystem op)
							if (filerIsExploitProtectedPath(path, &files[browser_sel])) {
								ret = filerConfirmExploitDelete(path, &files[browser_sel]);
							} else {
								sprintf(tmp, "%s", files[browser_sel].name);
								if (files[browser_sel].stats.AttrFile & sceMcFileAttrSubdir)
									strcat(tmp, "/");
								sprintf(tmp1, "\n%s ?", LNG(Delete));
								strcat(tmp, tmp1);
								ret = ynDialog(tmp);
							}
						} else
							ret = ynDialog(LNG(Mark_Files_Delete));

						if (ret > 0) {
							int first_deleted = -1;
							if (nmarks == 0) {
								strcpy(tmp, files[browser_sel].name);
								if (files[browser_sel].stats.AttrFile & sceMcFileAttrSubdir)
									strcat(tmp, "/");
								sprintf(tmp1, " %s", LNG(deleting));
								strcat(tmp, tmp1);
								drawMsg(tmp);
								ret = delete (path, &files[browser_sel]);
								if (ret >= 0)
									first_deleted = browser_sel;
							} else {
								for (i = 0; i < browser_nfiles; i++) {
									if (marks[i]) {
										if (filerIsExploitProtectedPath(path, &files[i]) &&
										    filerConfirmExploitDelete(path, &files[i]) < 0)
											continue;
										if (first_deleted < 0)   //if this is the first mark
											first_deleted = i;  //then memorize it for cursor positioning
										strcpy(tmp, files[i].name);
										if (files[i].stats.AttrFile & sceMcFileAttrSubdir)
											strcat(tmp, "/");
										sprintf(tmp1, " %s", LNG(deleting));
										strcat(tmp, tmp1);
										drawMsg(tmp);
										ret = delete (path, &files[i]);
										if (ret < 0)
											break;
									}
								}
							}
							if (ret >= 0) {
								if (first_deleted >= 0) {
									int cursor_source = first_deleted - 1;
									if (cursor_source < 0)
										cursor_source = 0;
									strcpy(cursorEntry, files[cursor_source].name);
								}
							} else {
								strcpy(cursorEntry, files[browser_sel].name);
								sprintf(msg0, "%s Err=%d", LNG(Delete_Failed), ret);
								browser_pushed = FALSE;
							}
							browser_cd = TRUE;
							browser_repos = TRUE;
						}
					}  //ends DELETE
					else if (ret == RENAME) {
						if (filerIsExploitProtectedPath(path, &files[browser_sel])) {
							browser_pushed = FALSE;
							strcpy(msg0, LNG(Rename_Failed));
						} else {
							strcpy(tmp, files[browser_sel].name);
							if (keyboard(tmp, 36) > 0) {
								if (Rename(path, &files[browser_sel], tmp) < 0) {
									browser_pushed = FALSE;
									strcpy(msg0, LNG(Rename_Failed));
								} else
									browser_cd = TRUE;
							}
						}
					}  //ends RENAME
					else if (ret == PASTE) {
						if (filerConfirmExploitModify(path, NULL) > 0)
							submenu_func_Paste(msg0, path);
						else
							browser_pushed = FALSE;
					} else if (ret == PSUPASTE) {
						if (filerConfirmExploitModify(path, NULL) > 0)
							submenu_func_psuPaste(msg0, path);
						else
							browser_pushed = FALSE;
					}
					else if (ret == NEWDIR) {
						tmp[0] = 0;
						if (filerConfirmExploitModify(path, NULL) > 0 && keyboard(tmp, 36) > 0) {
							ret = newdir(path, tmp);
							if (ret == -17) {
								strcpy(msg0, LNG(directory_already_exists));
								browser_pushed = FALSE;
							} else if (ret < 0) {
								strcpy(msg0, LNG(NewDir_Failed));
								browser_pushed = FALSE;
							} else {  //dlanor: modified for similarity to PC browsers
								sprintf(msg0, "%s: ", LNG(Created_folder));
								strcat(msg0, tmp);
								browser_pushed = FALSE;
								strcpy(cursorEntry, tmp);
								browser_repos = TRUE;
								browser_cd = TRUE;
							}
						}
					}  //ends NEWDIR
					else if (ret == NEWICON) {
						if (filerConfirmExploitModify(path, NULL) < 0) {
							browser_pushed = FALSE;
							continue;
						}
						strcpy(tmp, LNG(Icon_Title));
							if (keyboard(tmp, 36) <= 0)
								goto DoneIcon;
							if (genFixPath(path, tmp1) < 0) {
								sprintf(msg0, "Path conversion failed: %s", path);
								goto DoneIcon;
							}
							strcat(tmp1, "icon.sys");
							if ((ret = genOpen(tmp1, FIO_O_RDONLY)) >= 0) {  //if old "icon.sys" file exists
								genClose(ret);
							sprintf(msg1,
							        "\n\"icon.sys\" %s.\n\n%s ?", LNG(file_alredy_exists),
							        LNG(Do_you_wish_to_overwrite_it));
							if (ynDialog(msg1) < 0)
								goto DoneIcon;
							genRemove(tmp1);
							}
							make_iconsys(tmp, "icon.icn", tmp1);
							browser_cd = TRUE;
							strcpy(tmp, LNG(IconText));
							keyboard(tmp, 36);
							if (genFixPath(path, tmp1) < 0) {
								sprintf(msg0, "Path conversion failed: %s", path);
								goto DoneIcon;
							}
							strcat(tmp1, "icon.icn");
							if ((ret = genOpen(tmp1, FIO_O_RDONLY)) >= 0) {  //if old "icon.icn" file exists
								genClose(ret);
							sprintf(msg1,
							        "\n\"icon.icn\" %s.\n\n%s ?", LNG(file_alredy_exists),
							        LNG(Do_you_wish_to_overwrite_it));
							if (ynDialog(msg1) < 0)
								goto DoneIcon;
							genRemove(tmp1);
						}
						make_icon(tmp, tmp1);
					DoneIcon:
						strcpy(tmp, tmp1);  //Dummy code to make 'goto DoneIcon' legal for gcc
					}                       //ends NEWICON
					else if ((ret == MOUNTVMC0) || (ret == MOUNTVMC1)) {
						i = ret - MOUNTVMC0;
						sprintf(tmp, "vmc%d:", i);
						if (vmcMounted[i]) {
							if ((j = vmc_PartyIndex[i]) >= 0) {
								vmc_PartyIndex[i] = -1;
								if (j != vmc_PartyIndex[1 ^ i])
									Party_vmcIndex[j] = -1;
							}
							fileXioUmount(tmp);
							vmcMounted[i] = 0;
						}
						j = genFixPath(path, tmp1);
						if (j < 0) {
							sprintf(msg1, "\n'%s vmc%d:' for \"%s\"\nResult=%d",
							        LNG(Mount), i, path, j);
							(void)ynDialog(msg1);
							browser_pushed = FALSE;
							continue;
						}
						strcpy(tmp2, tmp1);
#if defined(ETH) || defined(UDPFS)
						if (!strncmp(path, "host:", 5) || !strncmp(path, "udpfs:", 6)) {
							makeHostPath(tmp2, tmp1);
						}
#endif
						strcat(tmp2, files[browser_sel].name);
						/* genFixPath may reset the IOP while lazy-loading storage stacks. */
						x = load_vmcman();
						if (!x) {
							x = get_vmcman_last_error();
							sprintf(msg1, "\n'%s vmc%d:' for \"%s\"\nvmcman not registered\nResult=%d",
							        LNG(Mount), i, tmp2, x);
							(void)ynDialog(msg1);
						} else if ((x = fileXioMount(tmp, tmp2, FIO_MT_RDWR)) >= 0) {
							if ((j >= 0) && (j < MOUNT_LIMIT)) {
								vmc_PartyIndex[i] = j;
								Party_vmcIndex[j] = i;
							}
							vmcMounted[i] = 1;
							snprintf(path, sizeof(path), "%.*s/", (int)sizeof(path) - 2, tmp);
							browser_cd = TRUE;
							cnfmode = NON_CNF;
							strcpy(ext, cnfmode_extL[cnfmode]);
						} else {
							sprintf(msg1, "\n'%s vmc%d:' for \"%s\"\nResult=%d",
							        LNG(Mount), i, tmp2, x);
							(void)ynDialog(msg1);
						}
					}  //ends MOUNTVMCx
					else if (ret == OPEN_TEXTEDITOR) {
						if (filerConfirmExploitModify(path, &files[browser_sel]) > 0) {
							snprintf(tmp1, sizeof(tmp1), "%s%s", path, files[browser_sel].name);
							ret = TextEditor(tmp1);
							strcpy(cursorEntry, files[browser_sel].name);
							browser_pushed = FALSE;
							browser_repos = TRUE;
							browser_cd = TRUE;
							if (ret == TEXTEDITOR_RESULT_LAUNCH_ARGS)
								snprintf(msg0, sizeof(msg0), LNG(Launch_Args_Loaded), LaunchArgsGetCount());
							else
								LaunchArgsClear();
						} else
							browser_pushed = FALSE;
					}  //ends OPEN_TEXTEDITOR
					else if (ret == LAUNCH_ELF_ARGS) {
						snprintf(tmp1, sizeof(tmp1), "%s%s", path, files[browser_sel].name);
						if (LaunchArgsLoadFromFile(tmp1, msg0, sizeof(msg0)) > 0) {
							strcpy(cursorEntry, files[browser_sel].name);
							browser_repos = TRUE;
						}
						browser_pushed = FALSE;
					}  //ends LAUNCH_ELF_ARGS
					else if (ret == GETSIZE) {
						submenu_func_GetSize(msg0, path, files);
					}  //ends GETSIZE
//#ifdef TMANIP
					else if (ret == TIMEMANIP) {
#ifdef TMANIP_MORON
						sprintf(msg1, "\n\n %s  [%s]  ?\n", LNG(change_timestamp_of), HACK_FOLDER);
#else
						sprintf(msg1, "\n\n %s  [%s]  ?\n", LNG(change_timestamp_of), files[browser_sel].name);
#endif //TMANIP_MORON
						if (filerConfirmExploitModify(path, &files[browser_sel]) > 0 && ynDialog(msg1) > 0) {
							time_manip(path, &files[browser_sel], msg0);
							browser_pushed = FALSE;
							browser_repos = TRUE;  // TEST
							browser_cd = TRUE;     //TEST
						}
					}
//#endif //TMANIP

				else if (ret == TITLE_CFG) {
					if (filerConfirmExploitModify(path, &files[browser_sel]) > 0) {
						make_title_cfg(path, &files[browser_sel], msg0);
						browser_pushed = FALSE;
						browser_repos = TRUE;  // TEST
						browser_cd = TRUE;     //TEST
					} else
						browser_pushed = FALSE;
				}
				   //R1 menu handling is completed above
			} else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				if (browser_sel != 0 && strcmp(files[browser_sel].name, ".") && strcmp(files[browser_sel].name, "..") && path[0] != 0 && (!isHddRootPath(path) && strcmp(path, "dvr_hdd0:/"))) {
					if (marks[browser_sel]) {
						marks[browser_sel] = FALSE;
						nmarks--;
					} else {
						marks[browser_sel] = TRUE;
						nmarks++;
					}
				}
				browser_sel++;
				if (browser_sel >= browser_nfiles)
					browser_sel = 0;
				skipRootSpacerSelection(path, files, browser_nfiles, &browser_sel, 1);
			} else if (new_pad & PAD_SQUARE) {
				if (path[0] != 0 && (!isHddRootPath(path) && strcmp(path, "dvr_hdd0:/"))) {
					for (i = 1; i < browser_nfiles; i++) {
						if (marks[i]) {
							marks[i] = FALSE;
							nmarks--;
						} else {
							marks[i] = TRUE;
							nmarks++;
						}
					}
				}
			} else if (new_pad & PAD_SELECT) {  //Leaving the browser ?
				LaunchArgsClear();
				unmountAll();
				return rv;
			}
			}
		}  //ends pad response section

		//browser path adjustment section
		if (browser_up) {
			if ((p = strrchr(path, '/')) != NULL)
				*p = 0;
			if ((p = strrchr(path, '/')) != NULL) {
				p++;
				strcpy(cursorEntry, p);
				*p = 0;
			} else {
				strcpy(cursorEntry, path);
				path[0] = 0;
			}
			browser_cd = TRUE;
			browser_repos = TRUE;
		}  //ends 'if(browser_up)'
		if (!browser_cd && path[0] == '\0' && !boot_show_all_devices && setting != NULL && setting->Hide_MCMMCE && pollRootMemoryCardDevices()) {
			if (browser_nfiles > 0)
				strcpy(cursorEntry, files[browser_sel].name);
			else
				cursorEntry[0] = '\0';
			browser_cd = TRUE;
			browser_repos = TRUE;
			event |= 2;
		}
		//----- Process newly entered directory here (incl initial entry)
		if (browser_cd) {
			if (isGenericUsbRootPath(path)) {
				usb_unit = prepareUsbRootBrowse();
				if (usb_unit >= 0)
					snprintf(path, sizeof(path), "usb%d:/", usb_unit);
			}
			browser_nfiles = setFileList(path, ext, files, cnfmode);
#ifdef UDPFS
			if (udpfs_dir_open_failed) {
				udpfs_dir_open_failed = 0;
				strcpy(msg0, LNG(UDPFS_Server_not_found));
				browser_pushed = FALSE;
				rebootIopAndReloadCoreStackSilent();
				path[0] = '\0';
				strcpy(cursorEntry, "udpfs:");
				browser_repos = TRUE;
				browser_nfiles = setFileList(path, ext, files, cnfmode);
			}
#endif
			if (!cnfmode) {  //Calculate free space (unless configuring)
				if (!strncmp(path, "mc", 2)) {
					mcGetInfo(path[2] - '0', 0, &mctype_PSx, &mcfreeSpace, NULL);
					mcSync(0, NULL, &ret);
					freeSpace = mcfreeSpace * ((mctype_PSx == 1) ? 8192 : 1024);
					vfreeSpace = TRUE;
#ifdef XFROM
				} else if (!strncmp(path, "xfrom", 5)) {
					xfromGetInfo(0, 0, &mctype_PSx, &mcfreeSpace, NULL);
					xfromSync(0, NULL, &ret);
					freeSpace = mcfreeSpace * ((mctype_PSx == 1) ? 8192 : 1024);
					vfreeSpace = TRUE;
#endif
				} else if (!strncmp(path, "hdd", 3) && !isHddRootPath(path)) {
					u64 ZoneFree, ZoneSize;
					char pfs_str[6];

					strcpy(pfs_str, "pfs0:");
					pfs_str[3] += latestMount;
					ZoneFree = fileXioDevctl(pfs_str, PFSCTL_GET_ZONE_FREE, NULL, 0, NULL, 0);
					ZoneSize = fileXioDevctl(pfs_str, PFSCTL_GET_ZONE_SIZE, NULL, 0, NULL, 0);
					freeSpace = ZoneFree * ZoneSize;
					vfreeSpace = TRUE;
#ifdef DVRP
				} else if (!strncmp(path, "dvr_hdd", 7) && strcmp(path, "dvr_hdd0:/")) {
					u64 ZoneFree, ZoneSize;
					char pfs_str[10];

					strcpy(pfs_str, "dvr_pfs0:");
					pfs_str[7] += latestDVRPMount;
					ZoneFree = fileXioDevctl(pfs_str, PFSCTL_GET_ZONE_FREE, NULL, 0, NULL, 0);
					ZoneSize = fileXioDevctl(pfs_str, PFSCTL_GET_ZONE_SIZE, NULL, 0, NULL, 0);
					//printf("ZoneFree==%d  ZoneSize==%d\r\n", ZoneFree, ZoneSize);
					freeSpace = ZoneFree * ZoneSize;
					vfreeSpace = TRUE;
#endif
				}
			}
			browser_sel = 0;
			top = 0;
			if (browser_repos) {
				browser_repos = FALSE;
				for (i = 0; i < browser_nfiles; i++) {
					if (!strcmp(cursorEntry, files[i].name)) {
						browser_sel = i;
						top = browser_sel - 3;
						break;
					}
				}
			}  //ends if(browser_repos)
			nmarks = 0;
			memset(marks, 0, MAX_ENTRY);
			browser_cd = FALSE;
			browser_up = FALSE;
		}  //ends if(browser_cd)
		if (!strncmp(path, "cdfs", 4))
			uLE_cdStop();
		if (top > browser_nfiles - rows)
			top = browser_nfiles - rows;
		if (top < 0)
			top = 0;
		if (browser_sel >= browser_nfiles)
			browser_sel = browser_nfiles - 1;
		if (browser_sel < 0)
			browser_sel = 0;
		if (browser_nfiles > 0)
			skipRootSpacerSelection(path, files, browser_nfiles, &browser_sel, 1);
		if (browser_sel >= top + rows)
			top = browser_sel - rows + 1;
		if (browser_sel < top)
			top = browser_sel;

		if (event || post_event) {  //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[COLOR_BACKGR]);

			x = Menu_start_x;
			y = Menu_start_y;
			font_height = FONT_HEIGHT;
			if ((file_show == 2) && (elisaFnt != NULL)) {
				y -= 2;
				font_height = FONT_HEIGHT + 2;
			}
			rows = (Menu_end_y - Menu_start_y) / font_height;

			for (i = 0; i < rows; i++)  //Repeat loop for each browser text row
			{
				mcTitle = NULL;      //Assume that normal file/folder names are to be displayed
				int name_limit = 0;  //Assume that no name length problems exist

				if (top + i >= browser_nfiles)
					break;
				if (isRootSpacerEntry(path, &files[top + i])) {
					y += font_height;
					continue;
				}
				if (top + i == browser_sel)
					color = setting->color[COLOR_SELECT];  //Highlight cursor line
				else
					color = setting->color[COLOR_TEXT];

				if (!strcmp(files[top + i].name, ".."))
					strcpy(tmp, "..");

				else if ((file_show == 2) && files[top + i].title[0] != 0) {
					mcTitle = files[top + i].title;
				} else {  //Show normal file/folder names
					const char *root_label;

					root_label = NULL;
					if (path[0] == 0)
						root_label = getRootDeviceLabel(files[top + i].name);
					if (root_label != NULL)
						strcpy(tmp, root_label);
					else
						strcpy(tmp, files[top + i].name);
					if (file_show > 0) {  //Does display mode include file details ?
						name_limit = 43 * 8;
					} else {  //Filenames are shown without file details
						name_limit = 71 * 8;
					}
				}
				if (name_limit) {                   //Do we need to check name length ?
					int name_end = name_limit / 7;  //Max string length for acceptable spacing

					if (files[top + i].stats.AttrFile & sceMcFileAttrSubdir)
						name_end -= 1;             //For folders, reserve one character for final '/'
					if (strlen(tmp) > name_end) {  //Is name too long for clean display ?
						tmp[name_end - 1] = '~';   //indicate filename abbreviation
						tmp[name_end] = 0;         //abbreviate name length to make room for details
					}
				}

				if (files[top + i].stats.AttrFile & sceMcFileAttrSubdir && path[0] != 0)
					strcat(tmp, "/");
				if (mcTitle != NULL)
					printXY_sjis(mcTitle, x + 4, y, color, TRUE);
				else
					printXY(tmp, x + 4, y, color, TRUE, name_limit);
				if (file_show > 0) {
					//					unsigned int size = files[top+i].stats.fileSizeByte;
					u64 size = ((u64)files[top + i].stats.Reserve2 << 32) | files[top + i].stats.FileSizeByte;
					int scale = 0;  //0==Bytes, 1==KBytes, 2==MBytes, 3==GB
					char scale_s[6] = " KMGTP";
					PS2TIME timestamp = *(PS2TIME *)&files[top + i].stats._Modify;

					if (!size_valid)
						size = 0;
					if (!time_valid)
						memset((void *)&timestamp, 0, sizeof(timestamp));

					if (!size_valid || !(top + i))
						strcpy(tmp, "----- B");
					else if ((files[top + i].stats.AttrFile & sceMcFileAttrSubdir) && size == 0)
						strcpy(tmp, "    - B");
					else {
						while (size > 99999) {
							scale++;
							size /= 1024;
						}
						sprintf(tmp, "%5llu%cB", (unsigned long long)size, scale_s[scale]);
					}

					if (!time_valid || !(top + i))
						strcat(tmp, " ----.--.-- --:--:--");
					else {
						sprintf(tmp + strlen(tmp), " %04d.%02d.%02d %02d:%02d:%02d",
						        ((timestamp.year < 2256) ? timestamp.year : (timestamp.year - 256)),
						        timestamp.month,
						        timestamp.day,
						        timestamp.hour,
						        timestamp.min,
						        timestamp.sec);
					}

					printXY(tmp, x + 4 + 44 * FONT_WIDTH, y, color, TRUE, 0);
				}
				if (setting->FB_NoIcons) {  //if FileBrowser should not use icons
					if (marks[top + i])
						drawChar('*', x - 6, y, setting->color[COLOR_TEXT]);
				} else {  //if Icons must be used in front of file/folder names
					if (files[top + i].stats.AttrFile & sceMcFileAttrSubdir) {
						iconbase = ICON_FOLDER;
						iconcolr = COLOR_GRAPH1;
					} else {
						iconbase = ICON_FILE;
						if (genCmpFileExt(files[top + i].name, "ELF"))
							iconcolr = COLOR_GRAPH2;
						else if (
									genCmpFileExt(files[top + i].name, "TXT") || 
									genCmpFileExt(files[top + i].name, "CFG") || 
									genCmpFileExt(files[top + i].name, "CNF") || 
									genCmpFileExt(files[top + i].name, "INI") || 
									genCmpFileExt(files[top + i].name, "CHT") || 
									genCmpFileExt(files[top + i].name, "PBT") ||
									genCmpFileExt(files[top + i].name, "JS") ||
									genCmpFileExt(files[top + i].name, "LUA") ||
									genCmpFileExt(files[top + i].name, "XML") ||
									genCmpFileExt(files[top + i].name, "TOML") ||
									genCmpFileExt(files[top + i].name, "YAML") ||
									genCmpFileExt(files[top + i].name, "YML") ||
									genCmpFileExt(files[top + i].name, "ARG")
									)
							iconcolr = COLOR_GRAPH4;
						else
							iconcolr = COLOR_GRAPH3;
					}
					if (marks[top + i])
						iconbase += 2;
					drawChar(iconbase, x - 3 - FONT_WIDTH, y, setting->color[iconcolr]);
					drawChar(iconbase + 1, x - 3, y, setting->color[iconcolr]);
				}
				y += font_height;
			}                             //ends for, so all browser rows were fixed above
			if (browser_nfiles > rows) {  //if more files than available rows, use scrollbar
				drawFrame(SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 8, Frame_start_y,
				          SCREEN_WIDTH - SCREEN_MARGIN, Frame_end_y, setting->color[COLOR_FRAME]);
				y0 = (Menu_end_y - Menu_start_y + 8) * ((double)top / browser_nfiles);
				y1 = (Menu_end_y - Menu_start_y + 8) * ((double)(top + rows) / browser_nfiles);
				drawOpSprite(setting->color[COLOR_FRAME],
				             SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 6, (y0 + Menu_start_y - 4),
				             SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 2, (y1 + Menu_start_y - 4));
			}                  //ends clause for scrollbar
			if (nclipFiles) {  //if Something in clipboard, emulate LED indicator
				u64 LED_colour, RIM_colour = GS_SETREG_RGBA(0, 0, 0, 0);
				int RIM_w = 4, LED_w = 6, indicator_w = LED_w + 2 * RIM_w;
				int x2 = SCREEN_WIDTH - SCREEN_MARGIN - 2, x1 = x2 - indicator_w;
				int y1 = Frame_start_y + 2, y2 = y1 + indicator_w;

				if (browser_cut)
					LED_colour = GS_SETREG_RGBA(0xC0, 0, 0, 0);  //Red LED == CUT
				else
					LED_colour = GS_SETREG_RGBA(0, 0xC0, 0, 0);  //Green LED == COPY
				drawOpSprite(RIM_colour, x1, y1, x2, y2);
				drawOpSprite(LED_colour, x1 + RIM_w, y1 + RIM_w, x2 - RIM_w, y2 - RIM_w);
			}  //ends clause for clipboard indicator
				if (browser_pushed) {
					char display_path[MAX_PATH];

					formatBrowserPathForDisplay(path, display_path);
					snprintf(msg0, sizeof(msg0), "%s", display_path);
				}

			//Tooltip section
			if (cnfmode) {  //cnfmode indicates configurable file selection
				if (swapKeys)
					sprintf(msg1, "\xFF"
					              "1:%s \xFF"
					              "0:%s \xFF"
					              "3:%s \xFF"
					              "2:",
					        LNG(OK), LNG(Cancel), LNG(Up));
				else
					sprintf(msg1, "\xFF"
					              "0:%s \xFF"
					              "1:%s \xFF"
					              "3:%s \xFF"
					              "2:",
					        LNG(OK), LNG(Cancel), LNG(Up));
				if (ext[0] == '*')
					strcat(msg1, "*->");
				strcat(msg1, cnfmode_extU[cnfmode]);
				if (ext[0] != '*')
					strcat(msg1, "->*");
				sprintf(tmp, " R2:%s", LNG(PathPad));
				strcat(msg1, tmp);
				if ((cnfmode == DIR_CNF) || (cnfmode == SAVE_CNF)) {  //modes using Start
					sprintf(tmp, " Start:%s", LNG(Choose));
					strcat(msg1, tmp);
				}
			} else {  // cnfmode == FALSE
				if (swapKeys)
					sprintf(msg1, "\xFF"
					              "1:%s \xFF"
					              "3:%s \xFF"
					              "0:%s \xFF"
					              "2:%s L1:%s R1:%s R2:%s Sel:%s",
					        LNG(OK), LNG(Up), LNG(Mark), LNG(RevMark),
					        LNG(Mode), LNG(Menu), LNG(PathPad), LNG(Exit));
				else
					sprintf(msg1, "\xFF"
					              "0:%s \xFF"
					              "3:%s \xFF"
					              "1:%s \xFF"
					              "2:%s L1:%s R1:%s R2:%s Sel:%s",
					        LNG(OK), LNG(Up), LNG(Mark), LNG(RevMark),
					        LNG(Mode), LNG(Menu), LNG(PathPad), LNG(Exit));
			}
			setScrTmp(msg0, msg1);
			if (vfreeSpace) {
				if (freeSpace >= 1024 * 1024)
					sprintf(tmp, "[%.1fMB %s]", (double)freeSpace / 1024 / 1024, LNG(free));
				else if (freeSpace >= 1024)
					sprintf(tmp, "[%.1fKB %s]", (double)freeSpace / 1024, LNG(free));
				else
					sprintf(tmp, "[%dB %s]", (int)freeSpace, LNG(free));
				ret = strlen(tmp);
				drawSprite(setting->color[COLOR_BACKGR],
				           SCREEN_WIDTH - SCREEN_MARGIN - (ret + 1) * FONT_WIDTH, (Menu_message_y - 1),
				           SCREEN_WIDTH - SCREEN_MARGIN, (Menu_message_y + FONT_HEIGHT));
				printXY(tmp,
				        SCREEN_WIDTH - SCREEN_MARGIN - ret * FONT_WIDTH,
				        (Menu_message_y),
				        setting->color[COLOR_SELECT], TRUE, 0);
			}
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	}  //ends while

	//Leaving the browser
	if (rv <= 0)
		LaunchArgsClear();
	unmountAll();
	return rv;
}
//------------------------------
//endfunc getFilePath
//--------------------------------------------------------------
static void submenu_func_GetSize(char *mess, char *path, FILEINFO *files)
{
	u64 size;
	u64 entry_size;
	int ret, i, text_pos, text_inc, sel = -1;
	char filepath[MAX_PATH];

	/*
	int test;
	iox_stat_t stats;
	PS2TIME *time;
*/

	drawMsg(LNG(Checking_Size));
	if (nmarks == 0) {
		size = getFileSize(path, &files[browser_sel]);
		sel = browser_sel;  //for stat checking
		if ((size != (u64)-1) && (files[browser_sel].stats.AttrFile & sceMcFileAttrSubdir)) {
			files[browser_sel].stats.FileSizeByte = (u32)size;
			files[browser_sel].stats.Reserve2 = (u32)(size >> 32);
		}
	} else {
		for (i = size = 0; i < browser_nfiles; i++) {
			if (marks[i]) {
				entry_size = getFileSize(path, &files[i]);
				if (entry_size == (u64)-1) {
					size = (u64)-1;
					break;
				}
				size += entry_size;
				if (files[i].stats.AttrFile & sceMcFileAttrSubdir) {
					files[i].stats.FileSizeByte = (u32)entry_size;
					files[i].stats.Reserve2 = (u32)(entry_size >> 32);
				}
				sel = i;  //for stat checking
			}
		}
	}
	DPRINTF("size result = %llu\r\n", (unsigned long long)size);
	if (size == (u64)-1) {
		strcpy(mess, LNG(Size_test_Failed));
		text_pos = strlen(mess);
	} else {
		text_pos = 0;
		if (size >= 1024 * 1024)
			sprintf(mess, "%s = %.1fMB%n", LNG(SIZE), (double)size / 1024 / 1024, &text_inc);
		else if (size >= 1024)
			sprintf(mess, "%s = %.1fKB%n", LNG(SIZE), (double)size / 1024, &text_inc);
		else
			sprintf(mess, "%s = %lluB%n", LNG(SIZE), (unsigned long long)size, &text_inc);
		text_pos += text_inc;
	}

	//----- Comment out this section to skip attributes entirely -----
	if ((nmarks < 2) && (sel >= 0)) {
		sprintf(filepath, "%s%s", path, files[sel].name);
		//----- Start of section for debug display of attributes -----
		/*
		printf("path =\"%s\"\r\n", path);
		printf("file =\"%s\"\r\n", files[sel].name);
		if	(!strncmp(filepath, "host:/", 6))
			makeHostPath(filepath, filepath);
		test = fileXioGetStat(filepath, &stats);
		printf("test = %d\r\n", test);
		printf("mode = %08X\r\n", stats.mode);
		printf("attr = %08X\r\n", stats.attr);
		printf("size = %08X\r\n", stats.size);
		time = (PS2TIME *) stats.ctime;
		printf("ctime = %04d.%02d.%02d %02d:%02d:%02d.%02d\r\n",
			time->year,time->month,time->day,
			time->hour,time->min,time->sec,time->unknown);
		time = (PS2TIME *) stats.atime;
		printf("atime = %04d.%02d.%02d %02d:%02d:%02d.%02d\r\n",
			time->year,time->month,time->day,
			time->hour,time->min,time->sec,time->unknown);
		time = (PS2TIME *) stats.mtime;
		printf("mtime = %04d.%02d.%02d %02d:%02d:%02d.%02d\r\n",
			time->year,time->month,time->day,
			time->hour,time->min,time->sec,time->unknown);
*/
		//----- End of section for debug display of attributes -----
		sprintf(mess + text_pos, " m=%04X %04d.%02d.%02d %02d:%02d:%02d%n",
		        files[sel].stats.AttrFile,
		        files[sel].stats._Modify.Year,
		        files[sel].stats._Modify.Month,
		        files[sel].stats._Modify.Day,
		        files[sel].stats._Modify.Hour,
		        files[sel].stats._Modify.Min,
		        files[sel].stats._Modify.Sec,
		        &text_inc);
		text_pos += text_inc;
		if (!strncmp(path, "mc", 2)) {
			mcGetInfo(path[2] - '0', 0, &mctype_PSx, NULL, NULL);
			mcSync(0, NULL, &ret);
			sprintf(mess + text_pos, " %s=%d%n", LNG(mctype), mctype_PSx, &text_inc);
			text_pos += text_inc;
#ifdef XFROM
		} else if (!strncmp(path, "xfrom", 5)) {
			xfromGetInfo(0, 0, &mctype_PSx, NULL, NULL);
			xfromSync(0, NULL, &ret);
			sprintf(mess + text_pos, " %s=%d%n", LNG(mctype), mctype_PSx, &text_inc);
			text_pos += text_inc;
#endif
		}
		//sprintf(mess+text_pos, " mcTsz=%d%n", files[sel].stats.fileSizeByte, &text_inc);
		u64 size = ((u64)files[sel].stats.Reserve2 << 32) | files[sel].stats.FileSizeByte;
		//Max length is 20 characters+NULL
		char sizeC[21] = {0};
		char *sizeP = &sizeC[21];
		do {
			*(--sizeP) = '0' + (size % 10);
		} while (size /= 10);
		sprintf(mess + text_pos, " mcTsz=%s%n", sizeP, &text_inc);
		text_pos += text_inc;
	}
	//----- End of sections that show attributes -----
	browser_pushed = FALSE;
}
//------------------------------
//endfunc submenu_func_GetSize
//--------------------------------------------------------------
static void subfunc_Paste(char *mess, char *path)
{
	char tmp[MAX_PATH], tmp1[MAX_PATH];
	int i, ret = -1, trace_vmc_paste;

	written_size = 0;
	PasteTime = Timer();  //Note initial pasting time
	if (!strcmp(path, clipPath))
		goto finished;
	trace_vmc_paste = (!strncmp(clipPath, "vmc", 3) || !strncmp(path, "vmc", 3));
	ret = prepareTransferDeviceStacks(clipPath, path);
	if (ret == TRANSFER_STACK_INCOMPATIBLE) {
		printf("[PASTE] incompatible stacks ret=%d src='%s' dst='%s'\n", ret, clipPath, path);
		ynDialog("Incompatible drivers to perform action");
		browser_pushed = FALSE;
		return;
	}
	if (ret < 0) {
		printf("[PASTE] prepare stacks failed ret=%d src='%s' dst='%s'\n", ret, clipPath, path);
		goto finished;
	}
	drawMsg(LNG(Pasting));
	if (waitForClipboardSourceDevice() < 0) {
		printf("[PASTE] source device unavailable after IOP reset src='%s' dst='%s'\n", clipPath, path);
		ret = -1;
		goto finished;
	}
	if (trace_vmc_paste)
		printf("[PASTE] start src='%s' dst='%s' items=%d mode=%d cut=%d\n",
		       clipPath, path, nclipFiles, PasteMode, browser_cut);

	for (i = 0; i < nclipFiles; i++) {
		strcpy(tmp, clipFiles[i].name);
		if (clipFiles[i].stats.AttrFile & sceMcFileAttrSubdir)
			strcat(tmp, "/");
		sprintf(tmp1, " %s", LNG(pasting));
		strcat(tmp, tmp1);
		drawMsg(tmp);
		PM_flag[0] = PM_NORMAL;  //Always use normal mode at top level
		PM_file[0] = -1;         //Thus no attribute file is used here
		if (trace_vmc_paste)
			printf("[PASTE] item src='%s' dst='%s' name='%s' index=%d/%d attr=0x%x size=%u:%u\n",
			       clipPath, path, clipFiles[i].name, i + 1, nclipFiles,
			       clipFiles[i].stats.AttrFile, clipFiles[i].stats.Reserve2, clipFiles[i].stats.FileSizeByte);
		ret = copy(path, clipPath, clipFiles[i], 0);
		if (ret < 0) {
			printf("[PASTE] copy failed ret=%d src='%s' dst='%s' item='%s' index=%d/%d mode=%d cut=%d\n",
			       ret, clipPath, path, clipFiles[i].name, i + 1, nclipFiles, PasteMode, browser_cut);
			break;
		}
	}
	if ((ret >= 0) && browser_cut) {
		for (i = 0; i < nclipFiles; i++) {
			if (filerConfirmExploitDelete(clipPath, &clipFiles[i]) < 0) {
				ret = -1;
				break;
			}
			ret = delete (clipPath, &clipFiles[i]);
			if (ret < 0)
				break;
		}
	}

//	unmountAll(); //disabled to avoid interference with VMC implementation

finished:
	if (ret < 0) {
		printf("[PASTE] failed ret=%d src='%s' dst='%s' mode=%d cut=%d\n",
		       ret, clipPath, path, PasteMode, browser_cut);
		strcpy(mess, LNG(Paste_Failed));
		browser_pushed = FALSE;
	} else {
		if (browser_cut)
			nclipFiles = 0;
		markMx4sioDestinationAfterWrite(path);
	}
	browser_cd = TRUE;
}
//------------------------------
//endfunc subfunc_Paste
//--------------------------------------------------------------
static void submenu_func_Paste(char *mess, char *path)
{
	if (new_pad & PAD_SQUARE)
		PasteMode = PM_RENAME;
	else
		PasteMode = PM_NORMAL;
	subfunc_Paste(mess, path);
}
//------------------------------
//endfunc submenu_func_Paste
//--------------------------------------------------------------
static void submenu_func_psuPaste(char *mess, char *path)
{
	int psu_action = classifyPsuAction(path);

	if (psu_action == PSU_ACTION_EXTRACT) {
		PasteMode = PM_PSU_RESTORE;
	} else if (psu_action == PSU_ACTION_CREATE) {
		PasteMode = PM_PSU_BACKUP;
	} else {
		strcpy(mess, LNG(Paste_Failed));
		browser_pushed = FALSE;
		return;
	}
	subfunc_Paste(mess, path);
}
//------------------------------
//endfunc submenu_func_psuPaste
//--------------------------------------------------------------
//End of file: filer_browser.c
//--------------------------------------------------------------
