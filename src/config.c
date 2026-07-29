//---------------------------------------------------------------------------
// File name:   config.c
//---------------------------------------------------------------------------
#include "launchelf.h"
#include "config_private.h"
#include "gui_colors.h"
#include "init.h"
#include <stdbool.h>
#include <stdlib.h>


static const char LK_ID[SETTING_LK_COUNT][10] = {
	"auto",
	"Circle",
	"Cross",
	"Square",
	"Triangle",
	"L1",
	"R1",
	"L2",
	"R2",
	"L3",
	"R3",
	"Start",
	"Select",  //Predefined for "CONFIG"
	"Left",
	"Right",
	"ESR",
	"OSDSYS"};

char PathPad[MAX_PATH_PAD][MAX_PATH];
SETTING *setting = NULL;
static SETTING *tmpsetting;

void configFormatLabelValue(char *dst, size_t dst_size, const char *label, const char *value)
{
	int prefix_len;
	int value_len;

	if (dst_size == 0)
		return;
	prefix_len = snprintf(dst, dst_size, "  %s: ", label ? label : "");
	if (prefix_len < 0 || prefix_len >= (int)dst_size) {
		dst[dst_size - 1] = '\0';
		return;
	}

	value_len = (int)dst_size - prefix_len - 1;
	if (value_len < 0)
		value_len = 0;
	snprintf(dst + prefix_len, dst_size - prefix_len, "%.*s", value_len, value ? value : "");
}

void configFormatLabelValueAligned(char *dst, size_t dst_size, const char *label, const char *value, int label_width)
{
	int prefix_len;
	int value_len;
	int pad_spaces;
	int label_len;

	if (dst_size == 0)
		return;
	if (label_width < 0)
		label_width = 0;

	label_len = (label != NULL) ? (int)strlen(label) : 0;
	pad_spaces = label_width - label_len;
	if (pad_spaces < 0)
		pad_spaces = 0;

	prefix_len = snprintf(dst, dst_size, "  %s: %*s", label ? label : "", pad_spaces, "");
	if (prefix_len < 0 || prefix_len >= (int)dst_size) {
		dst[dst_size - 1] = '\0';
		return;
	}

	value_len = (int)dst_size - prefix_len - 1;
	if (value_len < 0)
		value_len = 0;
	snprintf(dst + prefix_len, dst_size - prefix_len, "%.*s", value_len, value ? value : "");
}

void configFormatSavePathValue(char *dst, size_t dst_size, const char *path, const char *loaded_path)
{
	static const char marker[] = " LOADED";
	const char *suffix;
	int path_width;

	if (dst_size == 0)
		return;
	if (path == NULL)
		path = "";

	suffix = (loaded_path != NULL && loaded_path[0] != '\0' && !stricmp(path, loaded_path)) ? marker : "";
	path_width = (int)dst_size - (int)strlen(suffix) - 3;
	if (path_width < 0)
		path_width = 0;
	snprintf(dst, dst_size, "\"%.*s\"%s", path_width, path, suffix);
}
//---------------------------------------------------------------------------
// End of declarations
// Start of functions
//---------------------------------------------------------------------------
// get_CNF_string is the main CNF parser called for each CNF variable in a
// CNF file. Input and output data is handled via its pointer parameters.
// The return value flags 'false' when no variable is found. (normal at EOF)
//---------------------------------------------------------------------------
int get_CNF_string(char **CNF_p_p,
                   char **name_p_p,
                   char **value_p_p)
{
	char *np, *vp, *tp = *CNF_p_p;

start_line:
	while ((*tp <= ' ') && (*tp > '\0'))
		tp += 1;  //Skip leading whitespace, if any
	if (*tp == '\0')
		return false;  //but exit at EOF
	np = tp;           //Current pos is potential name
	if (*tp < 'A')     //but may be a comment line
	{                  //We must skip a comment line
		while ((*tp != '\r') && (*tp != '\n') && (*tp > '\0'))
			tp += 1;      //Seek line end
		goto start_line;  //Go back to try next line
	}

	while ((*tp >= 'A') || ((*tp >= '0') && (*tp <= '9')))
		tp += 1;  //Seek name end
	if (*tp == '\0')
		return false;  //but exit at EOF

	while ((*tp <= ' ') && (*tp > '\0'))
		*tp++ = '\0';  //zero&skip post-name whitespace
	if (*tp != '=')
		return false;  //exit (syntax error) if '=' missing
	*tp++ = '\0';      //zero '=' (possibly terminating name)

	while ((*tp <= ' ') && (*tp > '\0')  //Skip pre-value whitespace, if any
	       && (*tp != '\r') && (*tp != '\n'))
		tp += 1;  //but do not pass the end of the line
	if (*tp == '\0')
		return false;  //but exit at EOF
	vp = tp;           //Current pos is potential value

	while ((*tp != '\r') && (*tp != '\n') && (*tp != '\0'))
		tp += 1;  //Seek line end
	if (*tp != '\0')
		*tp++ = '\0';  //terminate value (passing if not EOF)
	while ((*tp <= ' ') && (*tp > '\0'))
		tp += 1;  //Skip following whitespace, if any

	*CNF_p_p = tp;    //return new CNF file position
	*name_p_p = np;   //return found variable name
	*value_p_p = vp;  //return found variable value
	return true;      //return control to caller
}  //Ends get_CNF_string

//---------------------------------------------------------------------------
int CheckMC(void)
{
	int dummy, ret;

	mcGetInfo(0, 0, &dummy, &dummy, &dummy);
	mcSync(0, NULL, &ret);

	if (-1 == ret || 0 == ret)
		return 0;

	mcGetInfo(1, 0, &dummy, &dummy, &dummy);
	mcSync(0, NULL, &ret);

	if (-1 == ret || 0 == ret)
		return 1;

	return -11;
}

static int getMcPortFromPath(const char *path)
{
	int mcport;

	if (path == NULL)
		return -1;
	if (!strncmp(path, "mc0", 3))
		return 0;
	if (!strncmp(path, "mc1", 3))
		return 1;

	if (!strncmp(path, "mc:", 3)) {
		mcport = CheckMC();
		if (mcport == 0 || mcport == 1)
			return mcport;
		return 0;
	}

	return -1;
}

static int getExplicitMcPortFromPath(const char *path)
{
	if (path == NULL)
		return -1;
	if (!strncmp(path, "mc0", 3))
		return 0;
	if (!strncmp(path, "mc1", 3))
		return 1;

	return -1;
}

static int getLaunchMcPort(void)
{
	int mcport;

	mcport = getMcPortFromPath(LaunchElfDir);
	if (mcport == 0 || mcport == 1)
		return mcport;

	mcport = CheckMC();
	if (mcport == 0 || mcport == 1)
		return mcport;

	return 0;
}

static int configSysconfFileExistsOnPort(int port, const char *filename)
{
	char path[MAX_PATH];
	int fd;

	if ((port != 0 && port != 1) || filename == NULL || filename[0] == '\0')
		return 0;

	snprintf(path, sizeof(path), "mc%d:/SYS-CONF/%s", port, filename);
	fd = genOpen(path, FIO_O_RDONLY);
	if (fd < 0)
		return 0;
	genClose(fd);
	return 1;
}

static int configGetPreferredSysconfPort(const char *filename, const char *loaded_path)
{
	int mcport;
	int preferred_port;
	int mc0_has_file;
	int mc1_has_file;

	preferred_port = getExplicitMcPortFromPath(loaded_path);
	if (preferred_port < 0)
		preferred_port = getExplicitMcPortFromPath(LaunchElfBootDir);
	if (preferred_port < 0)
		preferred_port = getExplicitMcPortFromPath(LaunchElfDir);

	mc0_has_file = configSysconfFileExistsOnPort(0, filename);
	mc1_has_file = configSysconfFileExistsOnPort(1, filename);

	if ((preferred_port == 0 && mc0_has_file) || (preferred_port == 1 && mc1_has_file))
		return preferred_port;
	if (mc0_has_file && !mc1_has_file)
		return 0;
	if (mc1_has_file && !mc0_has_file)
		return 1;
	if (mc0_has_file && mc1_has_file)
		return 0;

	mcport = getMcPortFromPath(LaunchElfBootDir);
	if (mcport == 0 || mcport == 1)
		return mcport;

	return getLaunchMcPort();
}

static void configResolveMcAliasPath(char *path, size_t path_size)
{
	char suffix[MAX_PATH];
	int mcport;

	if (path == NULL || path_size == 0 || strncmp(path, "mc:", 3))
		return;

	snprintf(suffix, sizeof(suffix), "%s", path + 2);
	mcport = CheckMC();
	if (mcport != 0 && mcport != 1)
		mcport = 0;
	snprintf(path, path_size, "mc%d%s", mcport, suffix);
}

void configAppendPathFile(char *dst, size_t dst_size, const char *dir, const char *filename)
{
	size_t len;
	const char *separator;

	if (dst == NULL || dst_size == 0)
		return;
	if (filename == NULL)
		filename = "";
	if (dir == NULL || dir[0] == '\0') {
		snprintf(dst, dst_size, "%s", filename);
		return;
	}

	len = strlen(dir);
	separator = (dir[len - 1] == '/' || dir[len - 1] == '\\' || dir[len - 1] == ':') ? "" : "/";
	snprintf(dst, dst_size, "%s%s%s", dir, separator, filename);
}

void configBuildSysconfPath(char *dst, size_t dst_size, const char *filename)
{
	snprintf(dst, dst_size, "mc%d:/SYS-CONF/%s", configGetPreferredSysconfPort(filename, NULL), (filename != NULL) ? filename : "");
}

static void configBuildSysconfPathForLoaded(char *dst, size_t dst_size, const char *filename, const char *loaded_path)
{
	snprintf(dst, dst_size, "mc%d:/SYS-CONF/%s", configGetPreferredSysconfPort(filename, loaded_path), (filename != NULL) ? filename : "");
}

void configBuildSaveTargets(char *save_override_path, size_t save_override_path_size, char *save_cwd_path, size_t save_cwd_path_size, char *save_sysconf_path, size_t save_sysconf_path_size, const char *filename, const char *loaded_path, int *has_override_path)
{
	int use_override;

	use_override = (setting->CNF_Path[0] != '\0');
	if (has_override_path != NULL)
		*has_override_path = use_override;

	if (use_override) {
		configAppendPathFile(save_override_path, save_override_path_size, setting->CNF_Path, filename);
		configResolveMcAliasPath(save_override_path, save_override_path_size);
	} else if (save_override_path != NULL && save_override_path_size > 0) {
		save_override_path[0] = '\0';
	}

	configAppendPathFile(save_cwd_path, save_cwd_path_size, LaunchElfBootDir[0] ? LaunchElfBootDir : LaunchElfDir, filename);
	configResolveMcAliasPath(save_cwd_path, save_cwd_path_size);
	configBuildSysconfPathForLoaded(save_sysconf_path, save_sysconf_path_size, filename, loaded_path);
}

void configRefreshSaveTargetForWrite(enum CONFIG_SAVE_TARGET save_target, char *target_path, size_t target_path_size, const char *filename, const char *loaded_path)
{
	if (target_path == NULL || target_path_size == 0)
		return;

	if (save_target == CONFIG_SAVE_TARGET_SYSCONF)
		configBuildSysconfPathForLoaded(target_path, target_path_size, filename, loaded_path);
	else
		configResolveMcAliasPath(target_path, target_path_size);
}

void configEnsureSysconfDir(const char *path)
{
	int ret;

	if (path == NULL)
		return;
	if (!strncmp(path, "mc0:/SYS-CONF/", 14) || !strncmp(path, "mc1:/SYS-CONF/", 14)) {
		ensureMemoryCardPortAccessible(path[2] - '0');
		mcSync(0, NULL, NULL);
		mcMkDir(path[2] - '0', 0, "SYS-CONF");
		mcSync(0, NULL, &ret);
	}
}
//---------------------------------------------------------------------------
static char *preloadCNFFd(int fd, const char *cnf_path)
{
	int rd, total, cnf_seek_size;
	size_t CNF_size;
	char probe;
	char *RAM_p;
	const int CNF_MAX_SIZE = 128 * 1024;

	if (fd < 0)
		return NULL;

	cnf_seek_size = genLseek(fd, 0, SEEK_END);
	DPRINTF("%s: CNF_size=%d\n", __func__, cnf_seek_size);
	if (cnf_seek_size > 0 && genLseek(fd, 0, SEEK_SET) >= 0) {
		CNF_size = (size_t)cnf_seek_size;
		RAM_p = (char *)memalign(64, CNF_size + 1);
		if (RAM_p == NULL) {
			goto fallback_stream;
		}

		rd = genRead(fd, RAM_p, CNF_size);  //Read CNF as one long string
		genClose(fd);
		if (rd <= 0) {
			free(RAM_p);
			return NULL;
		}
		RAM_p[rd] = '\0';  //Terminate the CNF string
		return RAM_p;
	}

	/*
	 * Some stacks may fail SEEK_END/SEEK_SET even though reads are valid.
	 * Retry with a streamed read and conservative size cap.
	 */
fallback_stream:
	genClose(fd);
	fd = genOpen(cnf_path, FIO_O_RDONLY);
	if (fd < 0)
		return NULL;

	RAM_p = (char *)memalign(64, CNF_MAX_SIZE + 1);
	if (RAM_p == NULL) {
		genClose(fd);
		return NULL;
	}
	memset(RAM_p, 0, CNF_MAX_SIZE + 1);

	total = 0;
	while (total < CNF_MAX_SIZE) {
		int chunk = CNF_MAX_SIZE - total;
		if (chunk > 4096)
			chunk = 4096;

		rd = genRead(fd, RAM_p + total, chunk);
		if (rd < 0) {
			free(RAM_p);
			genClose(fd);
			return NULL;
		}
		if (rd == 0)
			break;
		total += rd;
	}
	if (total == CNF_MAX_SIZE && genRead(fd, &probe, 1) > 0) {
		// Reject suspiciously large CNF payloads.
		free(RAM_p);
		genClose(fd);
		return NULL;
	}

	genClose(fd);
	if (total <= 0) {
		free(RAM_p);
		return NULL;
	}
	RAM_p[total] = '\0';
	return RAM_p;
}
//preloadCNF loads an entire CNF file into RAM it allocates
//------------------------------
char *preloadCNF(char *path)
{
	int fd, tst;
	char cnf_path[MAX_PATH];

	fd = -1;
	if ((tst = genFixPath(path, cnf_path)) >= 0)
		fd = genOpen(cnf_path, FIO_O_RDONLY);
	if (fd < 0)
		return NULL;

	return preloadCNFFd(fd, cnf_path);
}
//------------------------------
//endfunc preloadCNF
// Save LAUNCHELF.CNF
//---------------------------------------------------------------------------
void saveConfigToPath(char *mainMsg, char *CNF, const char *target_path)
{
	int i, ret, fd;
	char c[MAX_PATH], tmp[26 * MAX_PATH + 30 * MAX_PATH];
	char cnf_path[MAX_PATH];
	size_t CNF_size, CNF_step;

	sprintf(tmp, "CNF_version = 3\r\n%n", &CNF_size);  //Start CNF with version header

	for (i = 0; i < SETTING_LK_COUNT; i++) {  //Loop to save the ELF paths for launch keys
		if ((i <= SETTING_LK_SELECT) || (setting->LK_Flag[i] != 0)) {
			sprintf(tmp + CNF_size,
			        "LK_%s_E1 = %s\r\n"
			        "%n",  // %n causes NO output, but only a measurement
			        LK_ID[i],
			        setting->LK_Path[i],
			        &CNF_step  // This variable measures the size of sprintf data
			        );
			CNF_size += CNF_step;
		}
	}  //ends for

	i = strlen(setting->Misc);
	sprintf(tmp + CNF_size,
	        "Misc = %s\r\n"
	        "Misc_PS2Disc = %s\r\n"
	        "Misc_FileBrowser = %s\r\n"
	        "Misc_PS2Browser = %s\r\n"
	        "Misc_PS2Net = %s\r\n"
	        "Misc_PS2PowerOff = %s\r\n"
	        "Misc_HddManager = %s\r\n"
	        "Misc_Exploit_Installer = %s\r\n"
	        "Misc_TextEditor = %s\r\n"
	        "Misc_Configure = %s\r\n"
	        "Misc_ShowFont = %s\r\n"
	        "Misc_Debug_Info = %s\r\n"
	        "Misc_About_uLE = %s\r\n"
	        "Misc_Show_Build_Info = %s\r\n"
	        "Misc_OSDSYS = %s\r\n"
	        "Misc_Reboot_IOP = %s\r\n"
	        "%n",  // %n causes NO output, but only a measurement
	        setting->Misc,
	        setting->Misc_PS2Disc + i,
	        setting->Misc_FileBrowser + i,
	        setting->Misc_PS2Browser + i,
	        setting->Misc_PS2Net + i,
	        setting->Misc_PS2PowerOff + i,
	        setting->Misc_HddManager + i,
	        setting->Misc_Exploit_Installer + i,
	        setting->Misc_TextEditor + i,
	        setting->Misc_Configure + i,
	        setting->Misc_ShowFont + i,
	        setting->Misc_Debug_Info + i,
	        setting->Misc_About_uLE + i,
	        setting->Misc_Show_Build_Info + i,
	        setting->Misc_OSDSYS + i,
	        setting->Misc_Reboot_IOP + i,
	        &CNF_step  // This variable measures the size of sprintf data
	        );
	CNF_size += CNF_step;

	CNF_size += storeGuiColorsCNF(tmp + CNF_size);

	sprintf(tmp + CNF_size,
	        "LK_auto_Timer = %d\r\n"
	        "Menu_Hide_Paths = %d\r\n"
	        "GUI_Swap_Keys = %d\r\n"
	        "NET_HOSTwrite = %d\r\n"
	        "APP_GameID = %d\r\n"
	        "CDROM_Disable_GameID = %d\r\n"
	        "Init_Delay = %d\r\n"
	        "REBOOT_IOP_ELFLOAD = %d\r\n"
	        "VirtualKeyboardLayout = %s\r\n"
	        "Hide_Hdd = %d\r\n"
	        "Hide_MCMMCE = %d\r\n"
	        "USBKBD_USED = %d\r\n"
	        "USBKBD_FILE = %s\r\n"
	        "KBDMAP_FILE = %s\r\n"
	        "Menu_Show_Titles = %d\r\n"
	        "PathPad_Lock = %d\r\n"
	        "CNF_Path = %s\r\n"
	        "language = %s\r\n"
	        "LANG_FILE = %s\r\n"
	        "FONT_FILE = %s\r\n"
	        "POPSTARTER_ELF = %s\r\n"
	        "PSU_HugeNames = %d\r\n"
	        "PSU_DateNames = %d\r\n"
	        "PSU_NoOverwrite = %d\r\n"
	        "FB_NoIcons = %d\r\n"
	        "%n",                      // %n causes NO output, but only a measurement
	        setting->timeout,          //auto_Timer
	        setting->Hide_Paths,       //Menu_Hide_Paths
	        setting->swapKeys,         //GUI_Swap_Keys
	        setting->HOSTwrite,        //NET_HOST_write
	        setting->app_gameid,       //app_gameid
	        setting->cdrom_disable_gameid, //cdrom_disable_gameid
	        setting->Init_Delay,       //Init_Delay
	        setting->reboot_iop_elf_load,
	        getVirtualKeyboardLayoutConfigName(setting->virtual_keyboard_layout),
	        setting->Hide_Hdd,
	        setting->Hide_MCMMCE,
	        setting->usbkbd_used,      //USBKBD_USED
	        setting->usbkbd_file,      //USBKBD_FILE
	        setting->kbdmap_file,      //KBDMAP_FILE
	        setting->Show_Titles,      //Menu_Show_Titles
	        setting->PathPad_Lock,     //PathPad_Lock
	        setting->CNF_Path,         //CNF_Path
	        getBuiltinLanguageConfigName(setting->language),
	        setting->lang_file,        //LANG_FILE
	        setting->font_file,        //FONT_FILE
	        setting->popstarter_file,  //POPSTARTER_ELF
	        setting->PSU_HugeNames,    //PSU_HugeNames
	        setting->PSU_DateNames,    //PSU_DateNames
	        setting->PSU_NoOverwrite,  //PSU_NoOverwrite
	        setting->FB_NoIcons,       //FB_NoIcons
	        &CNF_step                  // This variable measures the size of sprintf data
	        );
	CNF_size += CNF_step;

	for (i = 0; i < SETTING_LK_BTN_COUNT; i++) {  //Loop to save user defined launch key titles
		if (setting->LK_Title[i][0]) {            //Only save non-empty strings
			sprintf(tmp + CNF_size,
			        "LK_%s_Title = %s\r\n"
			        "%n",  // %n causes NO output, but only a measurement
			        LK_ID[i],
			        setting->LK_Title[i],
			        &CNF_step  // This variable measures the size of sprintf data
			        );
			CNF_size += CNF_step;
		}  //ends if
	}      //ends for

	sprintf(tmp + CNF_size,
	        "PathPad_Lock = %d\r\n"
	        "%n",                   // %n causes NO output, but only a measurement
	        setting->PathPad_Lock,  //PathPad_Lock
	        &CNF_step               // This variable measures the size of sprintf data
	        );
	CNF_size += CNF_step;

	for (i = 0; i < MAX_PATH_PAD; i++) {  //Loop to save non-empty PathPad entries
		if (PathPad[i][0]) {              //Only save non-empty strings
			sprintf(tmp + CNF_size,
			        "PathPad[%02d] = %s\r\n"
			        "%n",  // %n causes NO output, but only a measurement
			        i,
			        PathPad[i],
			        &CNF_step  // This variable measures the size of sprintf data
			        );
			CNF_size += CNF_step;
		}  //ends if
	}      //ends for

	if (target_path != NULL && target_path[0] != '\0') {
		snprintf(c, sizeof(c), "%s", target_path);
	} else {
		strcpy(c, LaunchElfDir);
		strcat(c, CNF);
		ret = genFixPath(c, cnf_path);
		if ((ret >= 0) && ((fd = genOpen(cnf_path, FIO_O_RDONLY)) >= 0))
			genClose(fd);
		else {                                //Start of clause for failure to use LaunchElfDir
			if (setting->CNF_Path[0] == 0) {  //if NO CNF Path override defined
				sprintf(c, "mc%d:/SYS-CONF", getLaunchMcPort());

				if ((fd = fileXioDopen(c)) >= 0) {
					fileXioDclose(fd);
					char strtmp[MAX_PATH] = "/";
					strcat(c, strcat(strtmp, CNF));
				} else {
					strcpy(c, LaunchElfDir);
					strcat(c, CNF);
				}
			}
		}  //End of clause for failure to use LaunchElfDir
	}

	configEnsureSysconfDir(c);
	ret = genFixPath(c, cnf_path);
	if ((ret < 0) || ((fd = genOpen(cnf_path, FIO_O_CREAT | FIO_O_WRONLY | FIO_O_TRUNC)) < 0)) {
		if (target_path != NULL && target_path[0] != '\0') {
			sprintf(mainMsg, "%s %s", LNG(Failed_To_Save), CNF);
			return;
		}
		sprintf(c, "mc%d:/SYS-CONF", getLaunchMcPort());
		if ((fd = fileXioDopen(c)) >= 0) {
			fileXioDclose(fd);
			char strtmp[MAX_PATH] = "/";
			strcat(c, strcat(strtmp, CNF));
		} else {
			strcpy(c, LaunchElfDir);
			strcat(c, CNF);
		}
		ret = genFixPath(c, cnf_path);
		if ((fd = genOpen(cnf_path, FIO_O_CREAT | FIO_O_WRONLY | FIO_O_TRUNC)) < 0) {
			sprintf(mainMsg, "%s %s", LNG(Failed_To_Save), CNF);
			return;
		}
	}
	ret = genWrite(fd, &tmp, CNF_size);
	if (ret == CNF_size) {
		snprintf(LoadedConfigPath, sizeof(LoadedConfigPath), "%s", c);
		sprintf(mainMsg, "%s (%s)", LNG(Saved_Config), c);
	} else
		sprintf(mainMsg, "%s (%s)", LNG(Failed_writing), CNF);
	genClose(fd);
}
void saveConfig(char *mainMsg, char *CNF)
{
	saveConfigToPath(mainMsg, CNF, NULL);
}
//---------------------------------------------------------------------------
void initConfig(void)
{
	int i;

	if (setting != NULL)
		free(setting);
	setting = (SETTING *)malloc(sizeof(SETTING));

	sprintf(setting->Misc, "%s/", LNG_DEF(MISC));
	sprintf(setting->Misc_PS2Disc, "%s/%s", LNG_DEF(MISC), LNG_DEF(PS2Disc));
	sprintf(setting->Misc_FileBrowser, "%s/%s", LNG_DEF(MISC), LNG_DEF(FileBrowser));
	sprintf(setting->Misc_PS2Browser, "%s/%s", LNG_DEF(MISC), LNG_DEF(PS2Browser));
	sprintf(setting->Misc_PS2Net, "%s/%s", LNG_DEF(MISC), LNG_DEF(PS2Net));
	sprintf(setting->Misc_PS2PowerOff, "%s/%s", LNG_DEF(MISC), LNG_DEF(PS2PowerOff));
	sprintf(setting->Misc_HddManager, "%s/%s", LNG_DEF(MISC), LNG_DEF(HddManager));
	sprintf(setting->Misc_Exploit_Installer, "%s/%s", LNG_DEF(MISC), LNG_DEF(Exploit_Installer));
	sprintf(setting->Misc_TextEditor, "%s/%s", LNG_DEF(MISC), LNG_DEF(TextEditor));
	sprintf(setting->Misc_Configure, "%s/%s", LNG_DEF(MISC), LNG_DEF(Configure));
	sprintf(setting->Misc_ShowFont, "%s/%s", LNG_DEF(MISC), LNG_DEF(ShowFont));
	sprintf(setting->Misc_Debug_Info, "%s/%s", LNG_DEF(MISC), LNG_DEF(Debug_Info));
	sprintf(setting->Misc_About_uLE, "%s/%s", LNG_DEF(MISC), LNG_DEF(About_uLE));
	sprintf(setting->Misc_Show_Build_Info, "%s/%s", LNG(MISC), LNG(Build_Info));
	sprintf(setting->Misc_OSDSYS, "%s/%s", LNG_DEF(MISC), LNG_DEF(OSDSYS));
	sprintf(setting->Misc_Reboot_IOP, "%s/%s", LNG_DEF(MISC), LNG_DEF(Reboot_IOP));

	for (i = 0; i < SETTING_LK_COUNT; i++) {
		setting->LK_Path[i][0] = 0;
		setting->LK_Title[i][0] = 0;
		setting->LK_Flag[i] = 0;
	}
	for (i = 0; i < MAX_PATH_PAD; i++)
		PathPad[i][0] = 0;

	strcpy(setting->LK_Path[SETTING_LK_CIRCLE], setting->Misc_FileBrowser);
	setting->LK_Flag[SETTING_LK_CIRCLE] = 1;
	strcpy(setting->LK_Path[SETTING_LK_TRIANGLE], setting->Misc_About_uLE);
	setting->LK_Flag[SETTING_LK_TRIANGLE] = 1;
	setting->usbkbd_file[0] = '\0';
	setting->kbdmap_file[0] = '\0';
	setting->CNF_Path[0] = '\0';
	setting->lang_file[0] = '\0';
	setting->font_file[0] = '\0';
	setting->popstarter_file[0] = '\0';
	setting->timeout = DEF_TIMEOUT;
	setting->Hide_Paths = DEF_HIDE_PATHS;
	setting->color[COLOR_BACKGR] = DEF_COLOR1;
	setting->color[COLOR_FRAME] = DEF_COLOR2;
	setting->color[COLOR_SELECT] = DEF_COLOR3;
	setting->color[COLOR_TEXT] = DEF_COLOR4;
	setting->color[COLOR_GRAPH1] = DEF_COLOR5;
	setting->color[COLOR_GRAPH2] = DEF_COLOR6;
	setting->color[COLOR_GRAPH3] = DEF_COLOR7;
	setting->color[COLOR_GRAPH4] = DEF_COLOR8;
	setting->screen_x = 0;
	setting->screen_y = 0;
	setting->Menu_Frame = DEF_MENU_FRAME;
	setting->swapKeys = DEF_SWAPKEYS;
	setting->HOSTwrite = DEF_HOSTWRITE;
	setting->app_gameid = DEF_APP_GAMEID;
	setting->cdrom_disable_gameid = DEF_CDROM_DISABLE_GAMEID;
	setting->TV_mode = TV_mode_AUTO;
	setting->Popup_Opaque = DEF_POPUP_OPAQUE;
	setting->Init_Delay = DEF_INIT_DELAY;
	setting->usbkbd_used = DEF_USBKBD_USED;
	setting->language = DEF_LANGUAGE;
	setting->reboot_iop_elf_load = DEF_STARTUP_RESET_IOP_ELFLOAD;
	setting->virtual_keyboard_layout = DEF_VIRTUAL_KEYBOARD_LAYOUT;
	setting->Hide_Hdd = DEF_HIDE_HDD;
	setting->Hide_MCMMCE = DEF_HIDE_MCMMCE;
	setting->Show_Titles = DEF_SHOW_TITLES;
	setting->PathPad_Lock = DEF_PATHPAD_LOCK;
	setting->PSU_HugeNames = DEF_PSU_HUGENAMES;
	setting->PSU_DateNames = DEF_PSU_DATENAMES;
	setting->PSU_NoOverwrite = DEF_PSU_NOOVERWRITE;
	setting->FB_NoIcons = DEF_FB_NOICONS;
}
//------------------------------
//endfunc initConfig
//---------------------------------------------------------------------------
// Load LAUNCHELF.CNF
// Return value: 0==OK, -1==failure
//---------------------------------------------------------------------------
int loadConfig(char *mainMsg, char *CNF)
{
	int i, fd, tst, len, mcport, var_cnt, CNF_version;
	char tsts[MAX_PATH];
	char path[MAX_PATH];
	char cnf_path[MAX_PATH];
	char loaded_path[MAX_PATH];
	char *RAM_p, *CNF_p, *name, *value;

	initConfig();
	LoadedConfigPath[0] = '\0';
	loaded_path[0] = '\0';

	strcpy(path, LaunchElfDir);
	strcat(path, CNF);
	if (!strncmp(path, "cdrom", 5))
		strcat(path, ";1");

	fd = -1;
	if ((tst = genFixPath(path, cnf_path)) >= 0)
		fd = genOpen(cnf_path, FIO_O_RDONLY);
	if (fd >= 0)
		snprintf(loaded_path, sizeof(loaded_path), "%s", path);
	if (fd < 0) {
		char strtmp[MAX_PATH];
		int ports_to_try[2];
		int port_count = 0;
		int port_ix;

		mcport = getLaunchMcPort();
		if (mcport == 0 || mcport == 1)
			ports_to_try[port_count++] = mcport;
		if (mcport != 0)
			ports_to_try[port_count++] = 0;
		if (mcport != 1)
			ports_to_try[port_count++] = 1;

		for (port_ix = 0; port_ix < port_count && fd < 0; port_ix++) {
			sprintf(strtmp, "mc%d:/SYS-CONF/", ports_to_try[port_ix]);
			strcpy(cnf_path, strtmp);
			strcat(cnf_path, CNF);
			fd = genOpen(cnf_path, FIO_O_RDONLY);
			if (fd >= 0) {
				strcpy(LaunchElfDir, strtmp);
				snprintf(loaded_path, sizeof(loaded_path), "%s", cnf_path);
			}
		}
	}
	if (fd < 0) {
	failed_load:
		sprintf(mainMsg, "%s %s", LNG(Failed_To_Load), CNF);
		return -1;
	}
	// This point is only reached after succesfully opening CNF.
	// Reuse this open descriptor to avoid probing/opening the same CNF twice.
	if ((RAM_p = preloadCNFFd(fd, cnf_path)) == NULL)
		goto failed_load;
	CNF_p = RAM_p;

	//RA NB: in the code below, the 'LK_' variables have been implemented such that
	//       any _Ex suffix will be accepted, with identical results. This will need
	//       to be modified when more execution methods are implemented.

	CNF_version = 0;                                                       // The CNF version is still unidentified
	for (var_cnt = 0; get_CNF_string(&CNF_p, &name, &value); var_cnt++) {  // A variable was found, now we dispose of its value.
		if (!strcmp(name, "CNF_version")) {
			CNF_version = atoi(value);
			continue;
		}

		if (scanGuiColorsCNF(name, value))
			continue;

		for (i = 0; i < SETTING_LK_COUNT; i++) {
			len = snprintf(tsts, sizeof(tsts), "LK_%s_E", LK_ID[i]);
			if ((len > 0) && (len < (int)sizeof(tsts)) && !strncmp(name, tsts, len)) {
				strcpy(setting->LK_Path[i], value);
				setting->LK_Flag[i] = 1;
				break;
			}
		}
		if (i < SETTING_LK_COUNT)
			continue;
		//----------
		//In the next group, the Misc device must be defined before its subprograms
		//----------
		else if (!strcmp(name, "Misc"))
			sprintf(setting->Misc, "%s/", value);
			else if (!strcmp(name, "Misc_PS2Disc"))
				sprintf(setting->Misc_PS2Disc, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_FileBrowser"))
				sprintf(setting->Misc_FileBrowser, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_PS2Browser"))
				sprintf(setting->Misc_PS2Browser, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_PS2Net"))
				sprintf(setting->Misc_PS2Net, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_PS2PowerOff"))
				sprintf(setting->Misc_PS2PowerOff, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_HddManager"))
				sprintf(setting->Misc_HddManager, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_TextEditor"))
				sprintf(setting->Misc_TextEditor, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_Configure"))
				sprintf(setting->Misc_Configure, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_ShowFont"))
				sprintf(setting->Misc_ShowFont, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_Debug_Info"))
				sprintf(setting->Misc_Debug_Info, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_About_uLE"))
				sprintf(setting->Misc_About_uLE, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_Show_Build_Info"))
				sprintf(setting->Misc_Show_Build_Info, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_OSDSYS"))
				sprintf(setting->Misc_OSDSYS, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_Exploit_Installer"))
				sprintf(setting->Misc_Exploit_Installer, "%s%s", setting->Misc,
				        !strcmp(value, "Exploit Installer") ? LNG_DEF(Exploit_Installer) : value);
			else if (!strcmp(name, "Misc_Reboot_IOP"))
				sprintf(setting->Misc_Reboot_IOP, "%s%s", setting->Misc, value);
			//----------
			else if (!strcmp(name, "LK_auto_Timer"))
				setting->timeout = atoi(value);
			else if (!strcmp(name, "Menu_Hide_Paths"))
				setting->Hide_Paths = atoi(value);
			//---------- NB: color settings moved to scanGuiColorsCNF
			else if (!strcmp(name, "Menu_Pages")) {
				// Legacy multi-CNF key, ignored.
			} else if (!strcmp(name, "GUI_Swap_Keys"))
				setting->swapKeys = atoi(value);
		else if (!strcmp(name, "NET_HOSTwrite"))
			setting->HOSTwrite = atoi(value);
		else if (!strcmp(name, "APP_GameID") || !strcmp(name, "app_gameid") || !strcmp(name, "Enable_GameID"))
			setting->app_gameid = atoi(value);
		else if (!strcmp(name, "CDROM_Disable_GameID") || !strcmp(name, "cdrom_disable_gameid") || !strcmp(name, "Disable_Disc_GameID"))
			setting->cdrom_disable_gameid = atoi(value);
		else if (!strcmp(name, "Menu_Title")) {
			// Legacy setting ignored. The title bar is fixed at runtime.
		} else if (!strcmp(name, "Init_Delay"))
			setting->Init_Delay = atoi(value);
		else if (!strcmp(name, "USBKBD_USED"))
			setting->usbkbd_used = atoi(value);
		else if (!strcmp(name, "REBOOT_IOP_ELFLOAD"))
			setting->reboot_iop_elf_load = atoi(value);
		else if (!stricmp(name, "VirtualKeyboardLayout") || !stricmp(name, "Virtual_Keyboard_Layout") || !stricmp(name, "VKEY_Layout")) {
			int layout = getVirtualKeyboardLayoutByConfigName(value);
			setting->virtual_keyboard_layout = (layout >= 0) ? layout : DEF_VIRTUAL_KEYBOARD_LAYOUT;
		} else if (!strcmp(name, "Hide_Hdd")) {
			int hide_hdd = atoi(value);
			setting->Hide_Hdd = (hide_hdd >= 0 && hide_hdd < HIDE_HDD_COUNT) ? hide_hdd : DEF_HIDE_HDD;
		} else if (!strcmp(name, "Hide_MCMMCE")) {
			setting->Hide_MCMMCE = (atoi(value) != 0);
		} else if (!strcmp(name, "USBKBD_FILE"))
			strcpy(setting->usbkbd_file, value);
		else if (!strcmp(name, "KBDMAP_FILE"))
			strcpy(setting->kbdmap_file, value);
		else if (!strcmp(name, "Menu_Show_Titles"))
			setting->Show_Titles = atoi(value);
		else if (!strcmp(name, "PathPad_Lock"))
			setting->PathPad_Lock = atoi(value);
		else if (!strcmp(name, "CNF_Path"))
			strcpy(setting->CNF_Path, value);
		else if (!stricmp(name, "language")) {
			int language = getBuiltinLanguageByConfigName(value);
			setting->language = (language >= 0) ? language : DEF_LANGUAGE;
		} else if (!strcmp(name, "LANG_FILE"))
			strcpy(setting->lang_file, value);
		else if (!strcmp(name, "FONT_FILE"))
			strcpy(setting->font_file, value);
		else if (!strcmp(name, "POPSTARTER_ELF"))
			strcpy(setting->popstarter_file, value);
		//----------
		else if (!strcmp(name, "PSU_HugeNames"))
			setting->PSU_HugeNames = atoi(value);
		else if (!strcmp(name, "PSU_DateNames"))
			setting->PSU_DateNames = atoi(value);
		else if (!strcmp(name, "PSU_NoOverwrite"))
			setting->PSU_NoOverwrite = atoi(value);
		else if (!strcmp(name, "FB_NoIcons"))
			setting->FB_NoIcons = atoi(value);
			//----------
			else {
				for (i = 0; i < SETTING_LK_BTN_COUNT; i++) {
					snprintf(tsts, sizeof(tsts), "LK_%s_Title", LK_ID[i]);
					if (!strcmp(name, tsts)) {
						strncpy(setting->LK_Title[i], value, MAX_ELF_TITLE - 1);
						setting->LK_Title[i][MAX_ELF_TITLE - 1] = '\0';
						break;
					}
			}
			if (i < SETTING_LK_BTN_COUNT)
				continue;
			else if (!strncmp(name, "PathPad[", 8)) {
				i = atoi(name + 8);
				if (i < MAX_PATH_PAD) {
					strncpy(PathPad[i], value, MAX_PATH - 1);
					PathPad[i][MAX_PATH - 1] = '\0';
				}
			}
		}
	}  //ends for
	if (CNF_version == 0)
		DPRINTF("loadConfig: CNF_version missing, treating \"%s\" as legacy config.\n", cnf_path);
	for (i = 0; i < SETTING_LK_BTN_COUNT; i++)
		setting->LK_Title[i][MAX_ELF_TITLE - 1] = 0;
	free(RAM_p);
	snprintf(LoadedConfigPath, sizeof(LoadedConfigPath), "%s", loaded_path[0] ? loaded_path : cnf_path);
	sprintf(mainMsg, "%s (%s)", LNG(Loaded_Config), cnf_path);
	return 0;
}
//------------------------------
//endfunc loadConfig
//---------------------------------------------------------------------------
// Polo: added GUI color menu with preview
// suloku: added main cosmetic/color selection
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// Other settings by EP
// sincro: ADD USBD SELECTOR MENU
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Configuration menu
//---------------------------------------------------------------------------
enum CONFIG_MAIN {
	CONFIG_MAIN_FIRST = 0,
	CONFIG_MAIN_DEFAULT = CONFIG_MAIN_FIRST,

	CONFIG_MAIN_BTN_CIRCLE,
	CONFIG_MAIN_BTN_CROSS,
	CONFIG_MAIN_BTN_SQUARE,
	CONFIG_MAIN_BTN_TRIANGLE,
	CONFIG_MAIN_BTN_L1,
	CONFIG_MAIN_BTN_R1,
	CONFIG_MAIN_BTN_L2,
	CONFIG_MAIN_BTN_R2,
	CONFIG_MAIN_BTN_L3,
	CONFIG_MAIN_BTN_R3,
	CONFIG_MAIN_BTN_START,

	//After button settings
	CONFIG_MAIN_AFT_BTNS,
	CONFIG_MAIN_SHOW_TITLES = CONFIG_MAIN_AFT_BTNS,
	CONFIG_MAIN_FILENAME,
	CONFIG_MAIN_SCREEN,
	CONFIG_MAIN_SETTINGS,
	CONFIG_MAIN_NETWORK,
	CONFIG_MAIN_ADVANCED,
	CONFIG_MAIN_LAST = CONFIG_MAIN_ADVANCED,

	CONFIG_MAIN_SAVE,
	CONFIG_MAIN_CANCEL,

	CONFIG_MAIN_COUNT
};

static void configFormatPromptLabel(char *dst, size_t dst_size, const char *label)
{
	size_t label_len;
	size_t copy_len;
	int append_colon;

	if (dst_size == 0)
		return;
	if (label == NULL)
		label = "";

	label_len = strlen(label);
	append_colon = !(label_len > 0 && label[label_len - 1] == ':');
	copy_len = label_len;
	if (copy_len >= dst_size)
		copy_len = dst_size - 1;

	memcpy(dst, label, copy_len);
	if (append_colon && copy_len + 1 < dst_size)
		dst[copy_len++] = ':';
	dst[copy_len] = '\0';
}

static void configFormatSavePromptLine(char *dst, size_t dst_size, const char *path, const char *loaded_path, int max_chars)
{
	static const char marker[] = " LOADED";
	const char *suffix;
	int prefix_len;
	int path_width;

	if (dst_size == 0)
		return;
	if (path == NULL)
		path = "";

	suffix = (loaded_path != NULL && loaded_path[0] != '\0' && !stricmp(path, loaded_path)) ? marker : "";
	prefix_len = snprintf(dst, dst_size, " \"");
	if (prefix_len < 0 || prefix_len >= (int)dst_size) {
		dst[dst_size - 1] = '\0';
		return;
	}

	path_width = max_chars - prefix_len - (int)strlen(suffix) - 1;
	if (path_width < 0)
		path_width = 0;
	snprintf(dst + prefix_len, dst_size - prefix_len, "%.*s\"%s", path_width, path, suffix);
}

static int configPathIsDisc(const char *path)
{
	return (path != NULL && (!strncmp(path, "cdfs", 4) || !strncmp(path, "cdrom", 5)));
}

static void configNormalizeComparablePath(const char *src, char *dst, size_t dst_size)
{
	size_t len;

	if (dst == NULL || dst_size == 0)
		return;
	if (src == NULL) {
		dst[0] = '\0';
		return;
	}

	snprintf(dst, dst_size, "%s", src);
	if (configPathIsDisc(dst)) {
		len = strlen(dst);
		if (len >= 2 && dst[len - 2] == ';' && dst[len - 1] == '1')
			dst[len - 2] = '\0';
	}
}

static int configPathsMatch(const char *a, const char *b)
{
	char norm_a[MAX_PATH];
	char norm_b[MAX_PATH];

	if (a == NULL || b == NULL || a[0] == '\0' || b[0] == '\0')
		return 0;

	configNormalizeComparablePath(a, norm_a, sizeof(norm_a));
	configNormalizeComparablePath(b, norm_b, sizeof(norm_b));
	return !stricmp(norm_a, norm_b);
}

static int configFindSaveTargetIndex(const enum CONFIG_SAVE_TARGET *targets, int option_count, enum CONFIG_SAVE_TARGET target)
{
	int i;

	for (i = 0; i < option_count; i++) {
		if (targets[i] == target)
			return i;
	}

	return -1;
}

static const char *configGetSaveTargetPath(enum CONFIG_SAVE_TARGET target, const char *save_override_path, const char *save_cwd_path, const char *save_sysconf_path)
{
	if (target == CONFIG_SAVE_TARGET_OVERRIDE)
		return save_override_path;
	if (target == CONFIG_SAVE_TARGET_CWD)
		return save_cwd_path;
	if (target == CONFIG_SAVE_TARGET_SYSCONF)
		return save_sysconf_path;

	return "";
}

static int configSaveTargetIsDefaultable(enum CONFIG_SAVE_TARGET target, const char *path, int sysconf_mcport)
{
	if (target == CONFIG_SAVE_TARGET_CANCEL || path == NULL || path[0] == '\0' || configPathIsDisc(path))
		return 0;
	if (target != CONFIG_SAVE_TARGET_SYSCONF)
		return 1;

	return (sysconf_mcport == 0 || sysconf_mcport == 1);
}

static int configSelectInitialSaveTarget(const enum CONFIG_SAVE_TARGET *targets, int option_count, const char *save_override_path, const char *save_cwd_path, const char *save_sysconf_path, const char *loaded_path, int has_override_path)
{
	int i;
	int cwd_index;
	int sysconf_index;
	int override_index;
	int cancel_index;
	int loaded_from_cwd;
	int sysconf_mcport;
	const char *path;

	override_index = configFindSaveTargetIndex(targets, option_count, CONFIG_SAVE_TARGET_OVERRIDE);
	cwd_index = configFindSaveTargetIndex(targets, option_count, CONFIG_SAVE_TARGET_CWD);
	sysconf_index = configFindSaveTargetIndex(targets, option_count, CONFIG_SAVE_TARGET_SYSCONF);
	cancel_index = configFindSaveTargetIndex(targets, option_count, CONFIG_SAVE_TARGET_CANCEL);
	sysconf_mcport = CheckMC();

	loaded_from_cwd = configPathsMatch(loaded_path, save_cwd_path);
	if (has_override_path && loaded_from_cwd && override_index >= 0 &&
	    configSaveTargetIsDefaultable(CONFIG_SAVE_TARGET_OVERRIDE, save_override_path, sysconf_mcport))
		return override_index;

	if (loaded_path != NULL && loaded_path[0] != '\0') {
		for (i = 0; i < option_count; i++) {
			path = configGetSaveTargetPath(targets[i], save_override_path, save_cwd_path, save_sysconf_path);
			if (configPathsMatch(loaded_path, path) && configSaveTargetIsDefaultable(targets[i], path, sysconf_mcport))
				return i;
		}
	}

	if (configPathIsDisc(save_cwd_path)) {
		if (override_index >= 0 && configSaveTargetIsDefaultable(CONFIG_SAVE_TARGET_OVERRIDE, save_override_path, sysconf_mcport))
			return override_index;
		if (sysconf_index >= 0 && configSaveTargetIsDefaultable(CONFIG_SAVE_TARGET_SYSCONF, save_sysconf_path, sysconf_mcport))
			return sysconf_index;
		if (cancel_index >= 0)
			return cancel_index;
	}

	if (loaded_path == NULL || loaded_path[0] == '\0') {
		if (sysconf_index >= 0 && configSaveTargetIsDefaultable(CONFIG_SAVE_TARGET_SYSCONF, save_sysconf_path, sysconf_mcport))
			return sysconf_index;
		if (cwd_index >= 0 && configSaveTargetIsDefaultable(CONFIG_SAVE_TARGET_CWD, save_cwd_path, sysconf_mcport))
			return cwd_index;
	}

	if (override_index >= 0 && configSaveTargetIsDefaultable(CONFIG_SAVE_TARGET_OVERRIDE, save_override_path, sysconf_mcport))
		return override_index;
	if (cwd_index >= 0 && configSaveTargetIsDefaultable(CONFIG_SAVE_TARGET_CWD, save_cwd_path, sysconf_mcport))
		return cwd_index;
	if (sysconf_index >= 0 && configSaveTargetIsDefaultable(CONFIG_SAVE_TARGET_SYSCONF, save_sysconf_path, sysconf_mcport))
		return sysconf_index;
	if (cancel_index >= 0)
		return cancel_index;

	return 0;
}

int configSaveTargetPrompt(const char *save_override_path, const char *save_cwd_path, const char *save_sysconf_path, const char *loaded_path, int has_override_path)
{
	enum CONFIG_SAVE_TARGET targets[4];
	char lines[4][MAX_PATH];
	char title[MAX_PATH];
	int option_count;
	int sel;
	int default_sel;
	int event, post_event;
	int i;
	int ret;
	int tw;
	int line_width;
	int max_chars;
	int a = 6, b = 4;
	int dh, dw, dx, dy;
	int y;

	configFormatPromptLabel(title, sizeof(title), LNG(Save_to));
	option_count = 0;
	default_sel = -1;
	if (has_override_path) {
		targets[option_count] = CONFIG_SAVE_TARGET_OVERRIDE;
		configFormatSavePromptLine(lines[option_count], sizeof(lines[option_count]), save_override_path, loaded_path, 0x7fffffff);
		option_count++;
	}
	targets[option_count] = CONFIG_SAVE_TARGET_CWD;
	configFormatSavePromptLine(lines[option_count], sizeof(lines[option_count]), save_cwd_path, loaded_path, 0x7fffffff);
	option_count++;
	targets[option_count] = CONFIG_SAVE_TARGET_SYSCONF;
	configFormatSavePromptLine(lines[option_count], sizeof(lines[option_count]), save_sysconf_path, loaded_path, 0x7fffffff);
	option_count++;
	targets[option_count] = CONFIG_SAVE_TARGET_CANCEL;
	snprintf(lines[option_count], sizeof(lines[option_count]), " %s", LNG(Cancel));
	option_count++;

	for (i = 0; i < option_count; i++) {
		if (loaded_path != NULL && loaded_path[0] != '\0') {
			if ((targets[i] == CONFIG_SAVE_TARGET_OVERRIDE && save_override_path != NULL && !stricmp(save_override_path, loaded_path)) ||
			    (targets[i] == CONFIG_SAVE_TARGET_CWD && save_cwd_path != NULL && !stricmp(save_cwd_path, loaded_path)) ||
			    (targets[i] == CONFIG_SAVE_TARGET_SYSCONF && save_sysconf_path != NULL && !stricmp(save_sysconf_path, loaded_path))) {
				default_sel = i;
				break;
			}
		}
		if (default_sel < 0 && targets[i] == CONFIG_SAVE_TARGET_SYSCONF)
			default_sel = i;
	}

	max_chars = (SCREEN_WIDTH - 2 * SCREEN_MARGIN - 2 * LINE_THICKNESS - 2 * a - 2 * FONT_WIDTH) / FONT_WIDTH;
	if (max_chars < 16)
		max_chars = 16;
	for (i = 0; i < option_count; i++) {
		if (targets[i] == CONFIG_SAVE_TARGET_OVERRIDE)
			configFormatSavePromptLine(lines[i], sizeof(lines[i]), save_override_path, loaded_path, max_chars);
		else if (targets[i] == CONFIG_SAVE_TARGET_CWD)
			configFormatSavePromptLine(lines[i], sizeof(lines[i]), save_cwd_path, loaded_path, max_chars);
		else if (targets[i] == CONFIG_SAVE_TARGET_SYSCONF)
			configFormatSavePromptLine(lines[i], sizeof(lines[i]), save_sysconf_path, loaded_path, max_chars);
	}

	tw = printXY(title, 0, 0, 0, FALSE, 0);
	line_width = printXY(LNG(Config_Search_Order), 0, 0, 0, FALSE, 0);
	if (line_width > tw)
		tw = line_width;
	for (i = 0; i < option_count; i++) {
		line_width = printXY(lines[i], 0, 0, 0, FALSE, 0);
		if (line_width > tw)
			tw = line_width;
	}
	if (tw < 160)
		tw = 160;

	dw = 2 * LINE_THICKNESS + a * 2 + tw + FONT_WIDTH;
	dh = 2 * LINE_THICKNESS + b * 2 + FONT_HEIGHT * (option_count + 3);
	dx = (SCREEN_WIDTH - dw) / 2;
	dy = (SCREEN_HEIGHT - dh) / 2;
	sel = configSelectInitialSaveTarget(targets, option_count, save_override_path, save_cwd_path, save_sysconf_path, loaded_path, has_override_path);
	event = 1;
	ret = CONFIG_SAVE_TARGET_CANCEL;

	while (1) {
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_UP) {
				event |= 2;
				if (sel > 0)
					sel--;
				else
					sel = option_count - 1;
			} else if (new_pad & PAD_DOWN) {
				event |= 2;
				if (sel < option_count - 1)
					sel++;
				else
					sel = 0;
			} else if ((new_pad & PAD_TRIANGLE) || (!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				ret = CONFIG_SAVE_TARGET_CANCEL;
				break;
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
				ret = targets[sel];
				break;
			}
		}

		if (event || post_event) {
			drawPopSprite(setting->color[COLOR_BACKGR], dx, dy, dx + dw, dy + dh);
			drawFrame(dx, dy, dx + dw, dy + dh, setting->color[COLOR_FRAME]);
			printXY(title, dx + LINE_THICKNESS + a, dy + LINE_THICKNESS + b, setting->color[COLOR_TEXT], TRUE, 0);
			printXY(LNG(Config_Search_Order), dx + LINE_THICKNESS + a, dy + LINE_THICKNESS + b + FONT_HEIGHT, setting->color[COLOR_TEXT], TRUE, 0);
			y = dy + LINE_THICKNESS + b + FONT_HEIGHT * 3;
			for (i = 0; i < option_count; i++) {
				printXY(lines[i], dx + LINE_THICKNESS + a + FONT_WIDTH, y + i * FONT_HEIGHT, setting->color[COLOR_TEXT], TRUE, 0);
				if (i == sel)
					drawChar(LEFT_CUR, dx + LINE_THICKNESS + a, y + i * FONT_HEIGHT, setting->color[COLOR_TEXT]);
			}
		}
		drawLastMsg();
		post_event = event;
		event = 0;
	}

	return ret;
}

void config(char *mainMsg, char *CNF)
{
	char c[MAX_PATH];
	char value[MAX_PATH];
	char save_override_path[MAX_PATH];
	char save_cwd_path[MAX_PATH];
	char save_sysconf_path[MAX_PATH];
	char title_tmp[MAX_ELF_TITLE];
	char *localMsg;
	int i;
	int s;
	int x, y;
	int len;
	int bool_label_width;
	int has_override_path;
	int save_target;
	char *save_path;
	int event, post_event = 0;

	tmpsetting = setting;
	setting = (SETTING *)malloc(sizeof(SETTING));
	*setting = *tmpsetting;

	event = 1;  //event = initial entry
	s = CONFIG_MAIN_FIRST;
	configBuildSaveTargets(save_override_path, sizeof(save_override_path),
	                       save_cwd_path, sizeof(save_cwd_path),
	                       save_sysconf_path, sizeof(save_sysconf_path),
	                       CNF, LoadedConfigPath, &has_override_path);
	while (1) {
		//Pad response section
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_UP) {
				event |= 2;  //event |= valid pad command
				if (s != CONFIG_MAIN_FIRST)
					s--;
				else
					s = CONFIG_MAIN_COUNT - 1;
			} else if (new_pad & PAD_DOWN) {
				event |= 2;  //event |= valid pad command
				if (s != CONFIG_MAIN_COUNT - 1)
					s++;
				else
					s = 0;
			} else if (new_pad & PAD_LEFT) {
				event |= 2;  //event |= valid pad command
				if (s > CONFIG_MAIN_LAST)
					s = CONFIG_MAIN_AFT_BTNS;
				else
					s = CONFIG_MAIN_FIRST;
				} else if (new_pad & PAD_RIGHT) {
					event |= 2;  //event |= valid pad command
					if (s < CONFIG_MAIN_AFT_BTNS)
						s = CONFIG_MAIN_AFT_BTNS;
					else if (s <= CONFIG_MAIN_LAST)
						s = CONFIG_MAIN_SAVE;
				} else if ((new_pad & PAD_SQUARE) && (s < CONFIG_MAIN_AFT_BTNS)) {
					event |= 2;  //event |= valid pad command
					strcpy(title_tmp, setting->LK_Title[s]);
					if (keyboard(title_tmp, MAX_ELF_TITLE) >= 0)
						strcpy(setting->LK_Title[s], title_tmp);
				} else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
					event |= 2;  //event |= valid pad command
					if (s < CONFIG_MAIN_AFT_BTNS) {
						if (!setting->PathPad_Lock) {
							setting->LK_Path[s][0] = 0;
							setting->LK_Title[s][0] = 0;
						}
					}
				} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
					event |= 2;  //event |= valid pad command
					if (s < CONFIG_MAIN_AFT_BTNS) {
						if (!setting->PathPad_Lock) {
							getFilePath(setting->LK_Path[s], TRUE);
							if (!strncmp(setting->LK_Path[s], "mc0", 3) ||
							    !strncmp(setting->LK_Path[s], "mc1", 3)) {
								snprintf(c, sizeof(c), "mc%.*s", (int)sizeof(c) - 3, &setting->LK_Path[s][3]);
								strcpy(setting->LK_Path[s], c);
							}
						}
					} else if (s == CONFIG_MAIN_SHOW_TITLES)
						setting->Show_Titles = !setting->Show_Titles;
					else if (s == CONFIG_MAIN_FILENAME)
						setting->Hide_Paths = !setting->Hide_Paths;
					else if (s == CONFIG_MAIN_SCREEN)
						Config_Screen();
					else if (s == CONFIG_MAIN_SETTINGS) {
						Config_Startup();
						configBuildSaveTargets(save_override_path, sizeof(save_override_path),
						                       save_cwd_path, sizeof(save_cwd_path),
						                       save_sysconf_path, sizeof(save_sysconf_path),
						                       CNF, LoadedConfigPath, &has_override_path);
					} else if (s == CONFIG_MAIN_NETWORK) {
						Config_Network();
					} else if (s == CONFIG_MAIN_ADVANCED) {
						Config_Advanced();
					} else if (s == CONFIG_MAIN_SAVE) {
						save_target = configSaveTargetPrompt(save_override_path, save_cwd_path, save_sysconf_path, LoadedConfigPath, has_override_path);
						if (save_target != CONFIG_SAVE_TARGET_CANCEL) {
							if (save_target == CONFIG_SAVE_TARGET_OVERRIDE)
								save_path = save_override_path;
							else if (save_target == CONFIG_SAVE_TARGET_CWD)
								save_path = save_cwd_path;
							else
								save_path = save_sysconf_path;
							configRefreshSaveTargetForWrite(save_target, save_path, MAX_PATH, CNF, LoadedConfigPath);
							free(tmpsetting);
							saveConfigToPath(mainMsg, CNF, save_path);
							break;
						}
					} else if (s == CONFIG_MAIN_CANCEL)
					goto cancel_exit;
			} else if (new_pad & PAD_TRIANGLE) {
			cancel_exit:
				free(setting);
				setting = tmpsetting;
				updateScreenMode();
				Load_External_Language();
				loadFont(setting->font_file);
				mainMsg[0] = 0;
				break;
			}
		}  //end if(readpad())

		if (event || post_event) {  //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[COLOR_BACKGR]);

			if (s < CONFIG_MAIN_AFT_BTNS)
				localMsg = setting->LK_Title[s];
			else
				localMsg = "";

			x = Menu_start_x;
			y = Menu_start_y;
			printXY(LNG(Button_Settings), x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			for (i = CONFIG_MAIN_FIRST; i < CONFIG_MAIN_AFT_BTNS; i++) {
				switch (i) {
					case CONFIG_MAIN_DEFAULT:
						strcpy(c, "  Default: ");
						break;
					case CONFIG_MAIN_BTN_CIRCLE:
						strcpy(c, "  \xFF"
						          "0     : ");
						break;
					case CONFIG_MAIN_BTN_CROSS:
						strcpy(c, "  \xFF"
						          "1     : ");
						break;
					case CONFIG_MAIN_BTN_SQUARE:
						strcpy(c, "  \xFF"
						          "2     : ");
						break;
					case CONFIG_MAIN_BTN_TRIANGLE:
						strcpy(c, "  \xFF"
						          "3     : ");
						break;
					case CONFIG_MAIN_BTN_L1:
						strcpy(c, "  L1     : ");
						break;
					case CONFIG_MAIN_BTN_R1:
						strcpy(c, "  R1     : ");
						break;
					case CONFIG_MAIN_BTN_L2:
						strcpy(c, "  L2     : ");
						break;
					case CONFIG_MAIN_BTN_R2:
						strcpy(c, "  R2     : ");
						break;
					case CONFIG_MAIN_BTN_L3:
						strcpy(c, "  L3     : ");
						break;
					case CONFIG_MAIN_BTN_R3:
						strcpy(c, "  R3     : ");
						break;
					case CONFIG_MAIN_BTN_START:
						strcpy(c, "  START  : ");
						break;
				}
				strcat(c, setting->LK_Path[i]);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
			}

			y += FONT_HEIGHT / 2;

			bool_label_width = (int)strlen(LNG(Show_launch_titles));
			if ((int)strlen(LNG(Hide_full_ELF_paths)) > bool_label_width)
				bool_label_width = (int)strlen(LNG(Hide_full_ELF_paths));

			configFormatLabelValueAligned(c, sizeof(c), LNG(Show_launch_titles), (setting->Show_Titles)? LNG(ON) : LNG(OFF), bool_label_width);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			configFormatLabelValueAligned(c, sizeof(c), LNG(Hide_full_ELF_paths), (setting->Hide_Paths)?LNG(ON): LNG(OFF), bool_label_width);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			sprintf(c, "  %s...", LNG(Screen_Settings));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			sprintf(c, "  %s...", LNG(Startup_Settings));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			sprintf(c, "  %s...", LNG(Network_Settings));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			sprintf(c, "  Advanced Settings...");
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			snprintf(value, sizeof(value), "...");
			configFormatLabelValue(c, sizeof(c), LNG(Save_to), value);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			sprintf(c, "  %s", LNG(Cancel));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);

			//Cursor positioning section
			y = Menu_start_y + (s + 1) * FONT_HEIGHT;
			if (s >= CONFIG_MAIN_AFT_BTNS)
				y += FONT_HEIGHT / 2;
			if (s >= CONFIG_MAIN_SAVE)
				y += FONT_HEIGHT / 2;
			drawChar(LEFT_CUR, x, y, setting->color[COLOR_TEXT]);

				//Tooltip section
				if (s < CONFIG_MAIN_AFT_BTNS) {
					if (setting->PathPad_Lock) {
						len = sprintf(c, "\xFF"
						                 "2:%s",
						              LNG(Edit_Title));
					} else if (swapKeys) {
						len = sprintf(c, "\xFF"
						                 "1:%s \xFF"
						                 "0:%s \xFF"
						                 "2:%s",
						              LNG(Browse), LNG(Clear), LNG(Edit_Title));
					} else {
						len = sprintf(c, "\xFF"
						                 "0:%s \xFF"
						                 "1:%s \xFF"
						                 "2:%s",
						              LNG(Browse), LNG(Clear), LNG(Edit_Title));
				}
			} else if ((s == CONFIG_MAIN_SHOW_TITLES) || (s == CONFIG_MAIN_FILENAME)) {
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s",
					              LNG(Change));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s",
					              LNG(Change));
			} else if (s == CONFIG_MAIN_SAVE) {
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s",
					              LNG(Select));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s",
					              LNG(Select));
			} else {
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s",
					              LNG(OK));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s",
					              LNG(OK));
			}
			sprintf(&c[len], " \xFF"
			                 "3:%s",
			        LNG(Return));
			setScrTmp(localMsg, c);
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;

	}  //ends while
}  //ends config
//---------------------------------------------------------------------------
// End of file: config.c
//---------------------------------------------------------------------------
