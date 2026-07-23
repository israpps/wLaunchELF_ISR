#ifndef CONFIG_PRIVATE_H
#define CONFIG_PRIVATE_H

#include "launchelf.h"
#include <stddef.h>

enum {
	DEF_TIMEOUT = 10,
	DEF_HIDE_PATHS = TRUE,
#ifdef CUSTOM_COLORS
	DEF_COLOR1 = GS_SETREG_RGBA(0, 0, 0, 0),  //Backgr
	DEF_COLOR2 = GS_SETREG_RGBA(0x35, 0x35, 0x35, 0),  //Frame
	DEF_COLOR3 = GS_SETREG_RGBA(0x6c, 0xce, 0xff, 0),  //Select
	DEF_COLOR4 = GS_SETREG_RGBA(0xa0, 0xa0, 0xa0, 0),  //Text
	DEF_COLOR5 = GS_SETREG_RGBA(0xff, 0xcc, 0x99, 0),  //Folders
	DEF_COLOR6 = GS_SETREG_RGBA(0x48, 0x6a, 0, 0),     //ELFs
	DEF_COLOR7 = GS_SETREG_RGBA(0x43, 0x3a, 0x7c, 0),  //Unknown
	DEF_COLOR8 = GS_SETREG_RGBA(0xd6, 0xc4, 0x28, 0),  //TextEditor
#else
	DEF_COLOR1 = GS_SETREG_RGBA(0, 0, 0, 0),              //Backgr
	DEF_COLOR2 = GS_SETREG_RGBA(0x35, 0x35, 0x35, 0),     //Frame
	DEF_COLOR3 = GS_SETREG_RGBA(0x6c, 0xce, 0xff, 0),     //Select
	DEF_COLOR4 = GS_SETREG_RGBA(0xa0, 0xa0, 0xa0, 0),     //Text
	DEF_COLOR5 = GS_SETREG_RGBA(0xff, 0xcc, 0x99, 0),     //Folders
	DEF_COLOR6 = GS_SETREG_RGBA(0x48, 0x6a, 0, 0),        //ELFs
	DEF_COLOR7 = GS_SETREG_RGBA(0x43, 0x3a, 0x7c, 0),     //Unknown
	DEF_COLOR8 = GS_SETREG_RGBA(0xd6, 0xc4, 0x28, 0),     //TextEditor
#endif //CUSTOM_COLORS
	DEF_MENU_FRAME = TRUE,
	DEF_SWAPKEYS = FALSE,
	DEF_HOSTWRITE = FALSE,
	DEF_APP_GAMEID = TRUE,
	DEF_CDROM_DISABLE_GAMEID = FALSE,
	DEF_POPUP_OPAQUE = FALSE,
	DEF_INIT_DELAY = 0,
	DEF_USBKBD_USED = 1,
	DEF_LANGUAGE = BUILTIN_LANGUAGE_ENGLISH,
	DEF_STARTUP_RESET_IOP_ELFLOAD = 1,
	DEF_VIRTUAL_KEYBOARD_LAYOUT = VKEY_LAYOUT_QWERTY,
	DEF_HIDE_HDD = HIDE_HDD_HDD1_ATA1,
	DEF_HIDE_MCMMCE = 0,
	DEF_SHOW_TITLES = 1,
	DEF_PATHPAD_LOCK = 0,
	DEF_PSU_HUGENAMES = 0,
	DEF_PSU_DATENAMES = 0,
	DEF_PSU_NOOVERWRITE = 0,
	DEF_FB_NOICONS = 0,
};

void configFormatLabelValue(char *dst, size_t dst_size, const char *label, const char *value);
void configFormatLabelValueAligned(char *dst, size_t dst_size, const char *label, const char *value, int label_width);
void configFormatSavePathValue(char *dst, size_t dst_size, const char *path, const char *loaded_path);
void configAppendPathFile(char *dst, size_t dst_size, const char *dir, const char *filename);
void configBuildSysconfPath(char *dst, size_t dst_size, const char *filename);
void configEnsureSysconfDir(const char *path);

enum CONFIG_SAVE_TARGET {
	CONFIG_SAVE_TARGET_CANCEL = -1,
	CONFIG_SAVE_TARGET_OVERRIDE,
	CONFIG_SAVE_TARGET_CWD,
	CONFIG_SAVE_TARGET_SYSCONF
};

void configBuildSaveTargets(char *save_override_path, size_t save_override_path_size, char *save_cwd_path, size_t save_cwd_path_size, char *save_sysconf_path, size_t save_sysconf_path_size, const char *filename, const char *loaded_path, int *has_override_path);
void configRefreshSaveTargetForWrite(enum CONFIG_SAVE_TARGET save_target, char *target_path, size_t target_path_size, const char *filename, const char *loaded_path);
int configSaveTargetPrompt(const char *save_override_path, const char *save_cwd_path, const char *save_sysconf_path, const char *loaded_path, int has_override_path);

int CheckMC(void);

void Config_Screen(void);
void Config_Startup(void);
void Config_Network(void);
void Config_Advanced(void);

#endif
