#ifndef LAUNCHELF_H
#define LAUNCHELF_H
#define HACK_FOLDER "BXEXEC-OPENTUNA"

#if defined(SIO_DEBUG)
#define ULE_VERSION_DEBUG_SUFFIX " SIO_DEBUG"
#elif defined(POWERPC_UART)
#define ULE_VERSION_DEBUG_SUFFIX " PPC_UART"
#elif defined(UDPTTY)
#define ULE_VERSION_DEBUG_SUFFIX " UDPTTY"
#elif defined(ULE_DEBUG_BUILD)
#define ULE_VERSION_DEBUG_SUFFIX " DEBUG"
#else
#define ULE_VERSION_DEBUG_SUFFIX ""
#endif

#define ULE_VERSION "v4.76_R3Z" ULE_VERSION_DEBUG_SUFFIX
//#ifndef ULE_VERDATE
//#define ULE_VERDATE __DATE__
//#endif
#include "githash.h"

//#define SIO_DEBUG 1	//defined only for debug versions using the EE_SIO patch

#include <stdio.h>
#include <tamtypes.h>
#include <sifcmd.h>
#include <kernel.h>
#include <delaythread.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <libhdd.h>
#include <libmc.h>
#include <libpad.h>
#include <iopheap.h>
#include <errno.h>
#include <fileXio_rpc.h>
#include <io_common.h>
#include <iopcontrol.h>
#include <stdarg.h>
#include <sbv_patches.h>
#include <slib.h>
#include <smem.h>
#include <smod.h>
#include <debug.h>
#include <gsKit.h>
#include <dmaKit.h>
#ifdef LIBCDVD_LEGACY
#include <cdvd_rpc.h>
#else
#if defined(__has_include)
#if __has_include(<libcdvd-rpc.h>)
#include <libcdvd-rpc.h>
#elif __has_include(<cdvd_rpc.h>)
#include <cdvd_rpc.h>
#endif
#else
#include <cdvd_rpc.h>
#endif
#endif
#include <libcdvd.h>
#include <libkbd.h>
#include <math.h>
#include <usbhdfsd-common.h>
#include "hdl_rpc.h"
#include "cdvd_macro.h"
#include "iop/ds34usb/ee/libds34usb.h"
#include "iop/ds34bt/ee/libds34bt.h"

#include <sio.h>
#include <sior_rpc.h>

/* floatlib compatibility helpers removed in newer PS2SDK toolchains */
#ifndef WLE_DEG_TO_RADF
#define WLE_DEG_TO_RADF(_deg) ((_deg) * 0.01745329251994329577f)
#endif

#ifndef cosdgf
#define cosdgf(_deg) cosf(WLE_DEG_TO_RADF(_deg))
#endif

#ifndef sindgf
#define sindgf(_deg) sinf(WLE_DEG_TO_RADF(_deg))
#endif

static inline unsigned char wle_ascii_tolower(unsigned char c)
{
	return (c >= 'A' && c <= 'Z') ? (unsigned char)(c + ('a' - 'A')) : c;
}

static inline int wle_stricmp(const char *lhs, const char *rhs)
{
	unsigned char a;
	unsigned char b;

	while (*lhs && *rhs) {
		a = wle_ascii_tolower((unsigned char)*lhs++);
		b = wle_ascii_tolower((unsigned char)*rhs++);
		if (a != b)
			return (int)a - (int)b;
	}

	a = wle_ascii_tolower((unsigned char)*lhs);
	b = wle_ascii_tolower((unsigned char)*rhs);
	return (int)a - (int)b;
}

#ifndef stricmp
#define stricmp wle_stricmp
#endif

static inline size_t wle_strnlen(const char *s, size_t maxlen)
{
	size_t len = 0;

	if (s == NULL)
		return 0;
	while (len < maxlen && s[len] != '\0')
		len++;
	return len;
}

#ifndef strnlen
#define strnlen wle_strnlen
#endif

#ifndef FIO_MT_RDWR
#define FIO_MT_RDWR 0x00
#endif

#ifndef FIO_MT_RDONLY
#define FIO_MT_RDONLY 0x01
#endif

#ifdef SIO_DEBUG //EE SIO will be printed separated. no need for diferentiation
		#define DPRINTF(format, args...) \
			sio_printf(format, ##args)
#elif defined(POWERPC_UART) || defined(COMMON_PRINTF) || defined(UDPTTY) //printf has to travel to IOP, add color escape to make up the diff
	#define DPRINTF(format, args...) \
		printf("\033[1;94;40m"format"\033[m", ##args)
#else
	#define DPRINTF(format, args...)// strip away printf from consumer builds
#endif

#define TRUE 1
#define FALSE 0

#ifndef SCECdESRDVD_0
#define SCECdESRDVD_0 0x15  // ESR-patched DVD, as seen without ESR driver active
#endif

#ifndef SCECdESRDVD_1
#define SCECdESRDVD_1 0x16  // ESR-patched DVD, as seen with ESR driver active
#endif

enum {                // cnfmode values for getFilePath in browsing for configurable file paths
	NON_CNF = 0,      // Normal browser mode, not configuration mode
	LK_ELF_CNF,       // Normal ELF choice for launch keys
	USBD_IRX_CNF,     // USBD.IRX choice for startup
	USBKBD_IRX_CNF,   // USB keyboard IRX choice (only PS2SDK)
	KBDMAP_FILE_CNF,  // USB keyboard mapping table choice
	CNF_PATH_CNF,     // CNF Path override choice
	TEXT_CNF,         // No restriction choice
	DIR_CNF,          // Directory choice
	USBMASS_IRX_CNF,  // USB_MASS.IRX choice for startup
	LANG_CNF,         // Language file choice
	FONT_CNF,         // Font file choice ( .fnt )
	SAVE_CNF,         // Generic Save choice (with or without selected file)
	ELF_FILE_CNF,     // Generic ELF file choice
	CNFMODE_CNT       // Total number of cnfmode values defined
};

enum {
	SCREEN_MARGIN = 16,
	FONT_WIDTH = 8,
	FONT_HEIGHT = 16,
	LINE_THICKNESS = 3,

	MAX_NAME = 256,
	MAX_PART_NAME = 32,
	MAX_PATH_PAD = 30,
	MAX_PATH = 1025,
	MAX_ENTRY = 2048,
	MAX_PARTITIONS = 1400,
	MAX_ELF_TITLE = 72,
	MAX_TEXT_LINE = 80
};

enum COLOR {
	COLOR_BACKGR = 0,
	COLOR_FRAME,
	COLOR_SELECT,
	COLOR_TEXT,
	COLOR_GRAPH1,
	COLOR_GRAPH2,
	COLOR_GRAPH3,
	COLOR_GRAPH4,

	COLOR_COUNT
};

enum SETTING_LK {
	SETTING_LK_AUTO = 0,
	SETTING_LK_CIRCLE,
	SETTING_LK_CROSS,
	SETTING_LK_SQUARE,
	SETTING_LK_TRIANGLE,
	SETTING_LK_L1,
	SETTING_LK_R1,
	SETTING_LK_L2,
	SETTING_LK_R2,
	SETTING_LK_L3,
	SETTING_LK_R3,
	SETTING_LK_START,
	SETTING_LK_SELECT,
	SETTING_LK_LEFT,
	SETTING_LK_RIGHT,

	SETTING_LK_BTN_COUNT,

	//Special paths
		SETTING_LK_ESR = SETTING_LK_BTN_COUNT,
		SETTING_LK_OSDSYS,

		SETTING_LK_COUNT
	};

enum VIRTUAL_KEYBOARD_LAYOUT {
	VKEY_LAYOUT_ABC = 0,
	VKEY_LAYOUT_QWERTY,
	VKEY_LAYOUT_DVORAK,
	VKEY_LAYOUT_AZERTY,
	VKEY_LAYOUT_QWERTZ,
	VKEY_LAYOUT_ABNT,

	VKEY_LAYOUT_COUNT
};

#define VKEY_LAYOUT_COLS 17
#define VKEY_LAYOUT_ROWS 5
#define VKEY_LAYOUT_SIZE (VKEY_LAYOUT_COLS * VKEY_LAYOUT_ROWS)
#define VKEY_EDITOR_COLS (VKEY_LAYOUT_COLS + 3)
#define VKEY_EDITOR_SIZE (VKEY_EDITOR_COLS * VKEY_LAYOUT_ROWS)

enum HIDE_HDD_MODE {
	HIDE_HDD_SHOW_ALL = 0,
	HIDE_HDD_HDD1,
	HIDE_HDD_HDD01,
	HIDE_HDD_ATA1,
	HIDE_HDD_ATA01,
	HIDE_HDD_HDD1_ATA1,
	HIDE_HDD_HDD01_ATA01,

	HIDE_HDD_COUNT
};

typedef struct
{
	char CNF_Path[MAX_PATH];
	char LK_Path[SETTING_LK_COUNT][MAX_PATH];
	char LK_Title[SETTING_LK_COUNT][MAX_ELF_TITLE];
	int LK_Flag[SETTING_LK_COUNT];
	char Misc[64];
	char Misc_PS2Disc[64];
	char Misc_FileBrowser[64];
	char Misc_PS2Browser[64];
	char Misc_PS2Net[64];
	char Misc_PS2PowerOff[64];
	char Misc_HddManager[64];
	char Misc_TextEditor[64];
	char Misc_Configure[64];
	char Misc_ShowFont[64];
	char Misc_Debug_Info[64];
	char Misc_About_uLE[64];
	char Misc_Show_Build_Info[64];
	char Misc_OSDSYS[64];
	char Misc_Exploit_Installer[64];
	char Misc_Reboot_IOP[64];
	char usbkbd_file[MAX_PATH];
	char kbdmap_file[MAX_PATH];
	char lang_file[MAX_PATH];
	char font_file[MAX_PATH];
	char popstarter_file[MAX_PATH];
	int Menu_Frame;
	int timeout;
	int Hide_Paths;
	u64 color[8];
	int screen_x;
	int screen_y;
	int swapKeys;
	int HOSTwrite;
	int app_gameid;
	int cdrom_disable_gameid;
	int TV_mode;
	int Popup_Opaque;
	int Init_Delay;
	int usbkbd_used;
	int language;
	int reboot_iop_elf_load;
	int virtual_keyboard_layout;
	int Hide_Hdd;
	int Hide_MCMMCE;
	int Show_Titles;
	int PathPad_Lock;
	int PSU_HugeNames;
	int PSU_DateNames;
	int PSU_NoOverwrite;
	int FB_NoIcons;
} SETTING;

typedef struct
{
	int ip[4];
	int nm[4];
	int gw[4];
} data_ip_struct;

extern char LaunchElfDir[MAX_PATH], LaunchElfBootDir[MAX_PATH], LastDir[MAX_NAME];
extern char LoadedConfigPath[MAX_PATH], LoadedIPConfigPath[MAX_PATH];

#ifndef IPCONF_MAX_LEN
#define IPCONF_MAX_LEN (3 * 16)
#endif

/* main.c */
extern int TV_mode;
extern int swapKeys;
extern int cdmode;      //Last detected disc type
extern u8 console_is_PSX;
extern u8 boot_show_all_devices;
extern char if_conf[IPCONF_MAX_LEN];
extern int if_conf_len;
extern char ip[16];
extern char netmask[16];
extern char gw[16];
extern char netConfig[IPCONF_MAX_LEN + 64];
extern int BootDiscType;
extern char SystemCnf_BOOT[MAX_PATH];
extern char SystemCnf_BOOT2[MAX_PATH];
extern char SystemCnf_VER[10];
extern char SystemCnf_VMODE[10];
extern char ROMVER_data[16];
extern char rough_region;

#ifdef MX4SIO
extern u8 mx4sio_driver_running;
#endif

int load_vmcman(void);
int get_vmcman_last_error(void);
#ifdef ETH
void load_ps2host(void);
#endif
#ifdef UDPFS
int load_udpfs(void);
#endif
int loadHddModules(void);
#ifdef DVRP
int loadDVRPHddModules(void);
#endif
void loadHdlInfoModule(void);
void loadCdModules(void);
void applyXPARAM(const char *gameID);
int uLE_related(char *pathout, const char *pathin);
int wleExists(const char *path);
int IsTextEditorFileType(const char *path);
void getIpConfig(void);
int readSystemCnf(void);
int uLE_InitializeRegion(void);
int uLE_cdDiscValid(void);
int uLE_cdStop(void);
int IsSupportedFileType(char *path);
void CleanUpForExec(void);
#ifdef XFROM
int loadFlashModules(void);
#endif
#ifdef MMCE
int loadMmceModules(void);
#endif
#ifdef MX4SIO
int loadMx4sioModules(void);
#endif
#ifdef EXFAT
int loadAtaModules(void);
#endif

/* elf.c */
int checkELFheader(char *filename);
void RunLoaderElf(char *filename, char *party, const char *selected_path, int exec_kind, int reboot_iop_elf_load);
void RunLoaderMemory(const char *arg0, const char *mem_arg, int reboot_iop);
void LaunchArgsClear(void);
int LaunchArgsPending(void);
int LaunchArgsGetCount(void);
int LaunchArgsLoadFromFile(const char *path, char *message, size_t message_size);
int LaunchArgsLoadSidecarForExec(const char *exec_path, char *message, size_t message_size);
int LaunchArgsLoadFromBuffer(const char *source, const char *buffer, int size, char *message, size_t message_size);
int LaunchArgsCopyToArgv(char **argv, int max_args);
#ifdef XFROM
int PrepareMbrLaunchPayload(const char *path, char *mem_arg, size_t mem_arg_size);
#endif

/* popstarter.c */
int IsPopstarterVcdPath(const char *path);
int LaunchPopstarterVcd(const char *path, char *message, size_t message_size);

/* draw.c */
#define FOLDER 0
#define WARNING 1

extern unsigned char icon_folder[];
extern unsigned char icon_warning[];

extern GSGLOBAL *gsGlobal;
extern GSTEXTURE TexIcon[2];
extern int SCREEN_WIDTH;
extern int SCREEN_HEIGHT;
extern int Menu_start_x;
extern int Menu_title_y;
extern int Menu_message_y;
extern int Frame_start_y;
extern int Menu_start_y;
extern int Menu_end_y;
extern int Frame_end_y;
extern int Menu_tooltip_y;
extern u8 FontBuffer[256 * 16];

void setScrTmp(const char *msg0, const char *msg1);
void drawSprite(u64 color, int x1, int y1, int x2, int y2);
void drawPopSprite(u64 color, int x1, int y1, int x2, int y2);
void drawOpSprite(u64 color, int x1, int y1, int x2, int y2);
void drawMsg(const char *msg);
void drawLastMsg(void);
void setupGS(void);
void updateScreenMode(void);
void clrScr(u64 color);
void drawScr(void);
void drawFrame(int x1, int y1, int x2, int y2, u64 color);
void drawChar(unsigned int c, int x, int y, u64 colour);
int printXY(const char *s, int x, int y, u64 colour, int draw, int space);
int printXY_sjis(const unsigned char *s, int x, int y, u64 colour, int);
char *transcpy_sjis(char *d, const unsigned char *s);
void loadIcon(void);
int loadFont(char *path_arg);
//Comment out WriteFont_C when not used (also requires patch in draw.c)
//int	WriteFont_C(char *pathname);

/* pad.c */
#define PAD_R3_V0 0x010000
#define PAD_R3_V1 0x020000
#define PAD_R3_H0 0x040000
#define PAD_R3_H1 0x080000
#define PAD_L3_V0 0x100000
#define PAD_L3_V1 0x200000
#define PAD_L3_H0 0x400000
#define PAD_L3_H1 0x800000

extern u32 joy_value;
extern u32 new_pad;
#ifdef DS34
extern int semRunning,semFinish;
extern int isRunning;
#endif
int setupPad(void);
int readpad(void);
int readpad_no_KB(void);
int readpad_noRepeat(void);
void clearPadPressState(void);
void waitPadReady(int port, int slot);
void waitAnyPadReady(void);

/* config.c */
enum TV_mode {
	TV_mode_AUTO = 0,
	TV_mode_NTSC,
	TV_mode_PAL,
	TV_mode_VGA,
	TV_mode_480P,

	TV_mode_COUNT
};

extern char PathPad[30][MAX_PATH];
extern SETTING *setting;
void initConfig(void);
int loadConfig(char *, char *);  //0==OK, -1==load failed
void config(char *, char *);
void saveConfigToPath(char *, char *, const char *);
int get_CNF_string(char **CNF_p_p,
                   char **name_p_p,
                   char **value_p_p);  //main CNF name,value parser
char *preloadCNF(char *path);          //loads file into RAM it allocates

/* filer.c */
typedef struct
{
	char name[MAX_NAME];
	unsigned char title[32 * 2 + 1];
	sceMcTblGetDir stats;
} FILEINFO;

#define MOUNT_LIMIT 4
extern char mountedParty[MOUNT_LIMIT][MAX_NAME];
extern int latestMount;
#ifdef DVRP
extern int latestDVRPMount;
#endif
extern int vmcMounted[2];
extern int vmc_PartyIndex[2];            //PFS index for each VMC, unless -1
extern int Party_vmcIndex[MOUNT_LIMIT];  //VMC index for each PFS, unless -1
extern unsigned char *elisaFnt;
char *PathPad_menu(const char *path);
int getFilePath(char *out, const int cnfmode);
#ifdef ETH
void initHOST(void);
#endif
#if defined(ETH) || defined(UDPFS)
char *makeHostPath(char *dp, char *sp);
#endif
int ynDialog(const char *message);
void nonDialog(char *message);
int keyboard(char *out, int max);
void genLimObjName(char *uLE_path, int reserve);
int genFixPath(const char *inp_path, char *gen_path);
/* mode must use fileXio-compatible flags (FIO_O_*). */
int genOpen(const char *path, int mode);
s64 genLseek(int fd, s64 where, int how);
int genRead(int fd, void *buf, int size);
int genWrite(int fd, void *buf, int size);
int genClose(int fd);
int genDopen(char *path);
int genDclose(int fd);
int genGetStat(const char *path, iox_stat_t *stat);
int genMkdir(const char *path, int mode);
int genRemove(char *path);
int genRmdir(char *path);
int genCmpFileExt(const char *filename, const char *extension);
int mountParty(const char *party);
void unmountParty(int party_ix);
int getDVRPPartyMountIndex(const char *party);
#ifdef DVRP
void unmountDVRPParty(int party_ix);
int mountDVRPParty(const char *party);
#endif
void unmountAll(void);
void invalidatePartitionCaches(void);
int setFileList(const char *path, const char *ext, FILEINFO *files, int cnfmode);

/* hdd.c */
void DebugDisp(char *Message);
void hddManager(void);

/* editor.c */
enum {
	TEXTEDITOR_RESULT_EXIT = 0,
	TEXTEDITOR_RESULT_LAUNCH_ARGS = 1,
	TEXTEDITOR_RESULT_BROWSE_DIR = 2
};

int TextEditor(char *path);

/* timer.c */
extern u64 WaitTime;
extern u64 CurrTime;

u64 Timer(void);

/* lang.c */
typedef struct Language
{
	char *String;
} Language;

enum BuiltinLanguage {
	BUILTIN_LANGUAGE_ENGLISH,
	BUILTIN_LANGUAGE_SPANISH,
	BUILTIN_LANGUAGE_FRENCH,
	BUILTIN_LANGUAGE_ITALIAN,
	BUILTIN_LANGUAGE_POLISH,
	BUILTIN_LANGUAGE_PORTUGUESE,
	BUILTIN_LANGUAGE_BRAZILIAN,
	BUILTIN_LANGUAGE_GERMAN,
	BUILTIN_LANGUAGE_HUNGARIAN,
	BUILTIN_LANGUAGE_COUNT
};

enum {
#define lang(id, name, value) LANG_##name,
#include "lang.h"
#undef lang
	LANG_COUNT
};

#define LNG(name) Lang_String[LANG_##name].String
#define LNG_DEF(name) Lang_Default[LANG_##name].String

extern Language Lang_String[];
extern Language Lang_Default[];
extern void *External_Lang_Buffer;

int normalizeVirtualKeyboardLayout(int layout);
const char *getVirtualKeyboardLayoutConfigName(int layout);
const char *getVirtualKeyboardLayoutDisplayName(int layout);
int getVirtualKeyboardLayoutByConfigName(const char *name);
int getVirtualKeyboardLayoutFirstColumn(int layout);
int getVirtualKeyboardLayoutColumnCount(int layout);
char getVirtualKeyboardLayoutChar(int layout, int index, int caps);
char getVirtualKeyboardLayoutDisplayChar(int layout, int index, int caps);
int isVirtualKeyboardLayoutKey(int layout, int index);
int getVirtualKeyboardLayoutNextKey(int layout, int index, int dx, int dy);
int getVirtualKeyboardEditorLayoutIndex(int editor_index);
char getVirtualKeyboardEditorChar(int layout, int editor_index, int caps);
char getVirtualKeyboardEditorDisplayChar(int layout, int editor_index, int caps);
int isVirtualKeyboardEditorKey(int layout, int editor_index);
int getVirtualKeyboardEditorNextKey(int layout, int editor_index, int dx, int dy);

void Init_Default_Language(void);
void Load_External_Language(void);
void Set_Language(int language);
int normalizeBuiltinLanguage(int language);
int getBuiltinLanguageByConfigName(const char *name);
const char *getBuiltinLanguageConfigName(int language);
const char *getBuiltinLanguageNativeName(int language);

/* font_uLE.c */

extern unsigned char font_uLE[];
enum {
	//0x100-0x109 are 5 double width characters for D-Pad buttons, which are accessed as:
	//"ÿ0"==Circle  "ÿ1"==Cross  "ÿ2"==Square  "ÿ3"==Triangle  "ÿ4"==filled Square
	RIGHT_CUR = 0x10A,  //Triangle pointing left, for use to the right of an item
	LEFT_CUR = 0x10B,   //Triangle pointing right, for use to the left of an item
	UP_ARROW = 0x10C,   //Arrow pointing up
	DN_ARROW = 0x10D,   //Arrow pointing up
	LT_ARROW = 0x10E,   //Arrow pointing up
	RT_ARROW = 0x10F,   //Arrow pointing up
	TEXT_CUR = 0x110,   //Vertical bar, for use between two text characters
	UL_ARROW = 0x111,   //Arrow pointing up and to the left, from a vertical start.
	BR_SPLIT = 0x112,   //Splits rectangle from BL to TR with BR portion filled
	BL_SPLIT = 0x113,   //Splits rectangle from TL to BR with BL portion filled
	                    //0x114-0x11B are 4 double width characters for D-Pad buttons, which are accessed as:
	                    //"ÿ:"==Right  "ÿ;"==Down  "ÿ<"==Left  "ÿ="==Up
	                    //0x11C-0x123 are 4 doubled characters used as normal/marked folder/file icons
	ICON_FOLDER = 0x11C,
	ICON_M_FOLDER = 0x11E,
	ICON_FILE = 0x120,
	ICON_M_FILE = 0x122,
	FONT_COUNT = 0x124  //Total number of characters in font
};

/* makeicon.c */
int make_icon(char *icontext, char *filename);
int make_iconsys(char *title, char *iconname, char *filename);

// chkesr_rpc.c
extern int Check_ESR_Disc(void);

//USB_mass definitions for multiple drive usage

#define USB_MASS_MAX_DRIVES 10

extern char USB_mass_ix[10];
extern int USB_mass_max_drives;
extern u64 USB_mass_scan_time;
extern int USB_mass_scanned;
extern int USB_mass_loaded;  //0==none, 1==internal, 2==external
void loadUsbModules(void);
int prepareUsbRootBrowse(void);
int ensureUsbKeyboardReady(void);

#endif
