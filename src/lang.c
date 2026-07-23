//---------------------------------------------------------------------------
//File name:    lang.c
//---------------------------------------------------------------------------
#include "launchelf.h"

Language Lang_Default[] = {
#define lang(id, name, value) {value},
#include "../Lang/ENG.LNG"
#undef lang
    {NULL}};

static Language Lang_Italian[] = {
#define lang(id, name, value) {value},
#include "../Lang/ITA.LNG"
#undef lang
    {NULL}};

static Language Lang_Spanish[] = {
#define lang(id, name, value) {value},
#include "../Lang/SPA.LNG"
#undef lang
    {NULL}};

static Language Lang_German[] = {
#define lang(id, name, value) {value},
#include "../Lang/GER.LNG"
#undef lang
    {NULL}};

static Language Lang_Portuguese[] = {
#define lang(id, name, value) {value},
#include "../Lang/POR.LNG"
#undef lang
    {NULL}};

static Language Lang_Brazilian[] = {
#define lang(id, name, value) {value},
#include "../Lang/PTBR.LNG"
#undef lang
    {NULL}};

static Language Lang_Polish[] = {
#define lang(id, name, value) {value},
#include "../Lang/POL.LNG"
#undef lang
    {NULL}};

static Language Lang_French[] = {
#define lang(id, name, value) {value},
#include "../Lang/FRE.LNG"
#undef lang
    {NULL}};

static Language Lang_Hungarian[] = {
#define lang(id, name, value) {value},
#include "../Lang/HUN.LNG"
#undef lang
    {NULL}};

Language Lang_String[sizeof(Lang_Default) / sizeof(Lang_Default[0])];
Language Lang_Extern[sizeof(Lang_Default) / sizeof(Lang_Default[0])];

void *External_Lang_Buffer = NULL;

static const char *builtin_language_config_names[BUILTIN_LANGUAGE_COUNT] = {
    "english",
    "spanish",
    "french",
    "italian",
    "polish",
    "portuguese",
    "brazilian",
    "german",
    "hungarian",
};

static const char *builtin_language_native_names[BUILTIN_LANGUAGE_COUNT] = {
    "English",
    "Espanol",
    "Francais",
    "Italiano",
    "Polski",
    "Portugues",
    "Portugues Brasileiro",
    "Deutsch",
    "Magyar",
};

int normalizeBuiltinLanguage(int language)
{
	while (language < 0)
		language += BUILTIN_LANGUAGE_COUNT;
	while (language >= BUILTIN_LANGUAGE_COUNT)
		language -= BUILTIN_LANGUAGE_COUNT;
	return language;
}

static Language *getBuiltinLanguageTable(int language)
{
	switch (normalizeBuiltinLanguage(language)) {
		case BUILTIN_LANGUAGE_SPANISH:
			return Lang_Spanish;
		case BUILTIN_LANGUAGE_FRENCH:
			return Lang_French;
		case BUILTIN_LANGUAGE_ITALIAN:
			return Lang_Italian;
		case BUILTIN_LANGUAGE_POLISH:
			return Lang_Polish;
		case BUILTIN_LANGUAGE_PORTUGUESE:
			return Lang_Portuguese;
		case BUILTIN_LANGUAGE_BRAZILIAN:
			return Lang_Brazilian;
		case BUILTIN_LANGUAGE_GERMAN:
			return Lang_German;
		case BUILTIN_LANGUAGE_HUNGARIAN:
			return Lang_Hungarian;
		case BUILTIN_LANGUAGE_ENGLISH:
		default:
			return Lang_Default;
	}
}

const char *getBuiltinLanguageConfigName(int language)
{
	return builtin_language_config_names[normalizeBuiltinLanguage(language)];
}

const char *getBuiltinLanguageNativeName(int language)
{
	return builtin_language_native_names[normalizeBuiltinLanguage(language)];
}

int getBuiltinLanguageByConfigName(const char *name)
{
	if (name == NULL || name[0] == '\0')
		return -1;
	if (!stricmp(name, "0") || !stricmp(name, "english") || !stricmp(name, "eng") || !stricmp(name, "en"))
		return BUILTIN_LANGUAGE_ENGLISH;
	if (!stricmp(name, "1") || !stricmp(name, "spanish") || !stricmp(name, "spa") || !stricmp(name, "es") || !stricmp(name, "espanol"))
		return BUILTIN_LANGUAGE_SPANISH;
	if (!stricmp(name, "2") || !stricmp(name, "french") || !stricmp(name, "fre") || !stricmp(name, "fra") ||
	    !stricmp(name, "fr") || !stricmp(name, "francais"))
		return BUILTIN_LANGUAGE_FRENCH;
	if (!stricmp(name, "3") || !stricmp(name, "italian") || !stricmp(name, "ita") || !stricmp(name, "it") || !stricmp(name, "italiano"))
		return BUILTIN_LANGUAGE_ITALIAN;
	if (!stricmp(name, "4") || !stricmp(name, "polish") || !stricmp(name, "pol") || !stricmp(name, "pl") ||
	    !stricmp(name, "polski"))
		return BUILTIN_LANGUAGE_POLISH;
	if (!stricmp(name, "5") || !stricmp(name, "portuguese") || !stricmp(name, "por") || !stricmp(name, "pt") || !stricmp(name, "portugues"))
		return BUILTIN_LANGUAGE_PORTUGUESE;
	if (!stricmp(name, "6") || !stricmp(name, "brazilian") || !stricmp(name, "brazil") || !stricmp(name, "br") ||
	    !stricmp(name, "ptbr") || !stricmp(name, "pt-br") || !stricmp(name, "portuguese-br") ||
	    !stricmp(name, "brazilian_portuguese"))
		return BUILTIN_LANGUAGE_BRAZILIAN;
	if (!stricmp(name, "7") || !stricmp(name, "german") || !stricmp(name, "ger") || !stricmp(name, "deu") || !stricmp(name, "de") || !stricmp(name, "deutsch"))
		return BUILTIN_LANGUAGE_GERMAN;
	if (!stricmp(name, "8") || !stricmp(name, "hungarian") || !stricmp(name, "hun") || !stricmp(name, "hu") || !stricmp(name, "magyar"))
		return BUILTIN_LANGUAGE_HUNGARIAN;
	return -1;
}

static void releaseExternalLanguageBuffer(void)
{
	if (External_Lang_Buffer != NULL) {
		free(External_Lang_Buffer);
		External_Lang_Buffer = NULL;
	}
}

//---------------------------------------------------------------------------
// get_LANG_string is the main parser called for each language dependent
// string in a language header file. (eg: "Francais.h" or "Francais.lng")
// Call values for all input arguments should be addresses of string pointers
// LANG_p_p is for file to be scanned, moved to point beyond scanned data.
// id_p_p is for a string defining the index value (suitable for 'atoi')
// value_p_p is for the string value itself (not NUL-terminated)
// The function returns the length of each string found, but -1 at EOF,
// and various error codes less than -1 (-2 etc) for various syntax errors,
// which also applies to EOF occurring where valid macro parts are expected.
//---------------------------------------------------------------------------
int get_LANG_string(char **LANG_p_p, char **id_p_p, char **value_p_p)
{
	char *cp, *ip, *vp, *tp = *LANG_p_p;
	int ret, length;

	ip = NULL;
	vp = NULL;
	ret = -1;

start_line:
	while (*tp <= ' ' && *tp > '\0')
		tp += 1;  //Skip leading whitespace, if any
	if (*tp == '\0')
		goto exit;  //but exit at EOF
	//Current pos is potential "lang(" entry, but we must verify this
	if (tp[0] == '/' && tp[1] == '/')  //It may be a comment line
	{                                  //We must skip a comment line
		while (*tp != '\r' && *tp != '\n' && *tp > '\0')
			tp += 1;      //Seek line end
		goto start_line;  //Go back to try next line
	}
	ret = -2;
	//Here tp points to a non-zero string that is not a comment
	if (strncmp(tp, "lang", 4))
		goto exit;  //Return error if not 'lang' macro
	tp += 4;        //but if it is, step past that name
	ret = -3;
	while (*tp <= ' ' && *tp != '\r' && *tp != '\n' && *tp > '\0')
		tp += 1;  //skip inline whitespace
	if (*tp == '\0')
		goto exit;  //but exit at EOF
	ret = -4;
	//Here tp points to a non-zero string that should be an opening parenthesis
	if (*tp != '(')
		goto exit;  //Return error if no opening parenthesis
	tp += 1;        //but if it is, step past this character
	ret = -5;
	while (*tp <= ' ' && *tp != '\r' && *tp != '\n' && *tp > '\0')
		tp += 1;  //skip inline whitespace
	if (*tp == '\0')
		goto exit;  //but exit at EOF
	ret = -6;
	//Here tp points to a non-zero string that should be an index number
	if (*tp < '0' || *tp > '9')
		goto exit;  //Return error if it's not a number
	ip = tp;        //but if it is, save this pos as id start
	while (*tp >= '0' && *tp <= '9')
		tp += 1;  //skip past the index number
	ret = -7;
	while (*tp <= ' ' && *tp != '\r' && *tp != '\n' && *tp > '\0')
		tp += 1;  //skip inline whitespace
	if (*tp == '\0')
		goto exit;  //but exit at EOF
	ret = -8;
	//Here tp points to a non-zero string that should be a comma
	if (*tp != ',')
		goto exit;  //Return error if no comma after index
	tp += 1;        //but if present, step past that comma
	ret = -9;
	while (*tp <= ' ' && *tp != '\r' && *tp != '\n' && *tp > '\0')
		tp += 1;  //skip inline whitespace
	if (*tp == '\0')
		goto exit;  //but exit at EOF
	ret = -10;
	//Here tp points to a non-zero string that should be a symbolic string name
	//But we don't need to process this for language switch purposes, so we ignore it
	//This may be changed later, to use the name for generating error messages
	while (*tp != ',' && *tp != '\r' && *tp != '\n' && *tp > '\0')
		tp += 1;  //seek inline comma
	if (*tp != ',')
		goto exit;  //Return error if no comma after string name
	tp += 1;        //but if present, step past that comma
	ret = -11;
	while (*tp <= ' ' && *tp != '\r' && *tp != '\n' && *tp > '\0')
		tp += 1;  //skip inline whitespace
	if (*tp == '\0')
		goto exit;  //but exit at EOF
	ret = -12;
	//Here tp points to a non-zero string that should be the opening quote character
	if (*tp != '\"')
		goto exit;  //Return error if no opening quote
	tp += 1;        //but if present, step past that quote
	ret = -13;
	vp = tp;  //save this pos as value start
close_quote:
	while (*tp != '\"' && *tp != '\r' && *tp != '\n' && *tp > '\0')
		tp += 1;  //seek inline quote
	if (*tp != '\"')
		return -13;  //Return error if no closing quote
	cp = tp - 1;     //save previous pos as check pointer
	tp += 1;         //step past the quote character
	if (*cp == '\\')
		goto close_quote;  //if this was an 'escaped' quote, try again
	//Here tp points to the character after the closing quote.
	length = (tp - 1) - vp;  //prepare string length for return value
	ret = -14;
	while (*tp <= ' ' && *tp != '\r' && *tp != '\n' && *tp > '\0')
		tp += 1;  //skip inline whitespace
	if (*tp == '\0')
		goto exit;  //but exit at EOF
	ret = -15;
	//Here tp points to a non-zero string that should be closing parenthesis
	if (*tp != ')')
		goto exit;  //Return error if no closing parenthesis
	tp += 1;        //but if present, step past the parenthesis
	ret = -16;
	while (*tp <= ' ' && *tp != '\r' && *tp != '\n' && *tp > '\0')
		tp += 1;  //skip inline whitespace
	if (*tp == '\0')
		goto exit;  //but exit at EOF
	//Here tp points to a non-zero string that should be line end or a comment
	if (tp[0] != '/' || tp[1] != '/')
		goto finish_line;  //if no comment, go handle line end
	ret = -17;
	while (*tp != '\r' && *tp != '\n' && *tp > '\0')
		tp += 1;  //Seek line end
	if (*tp == '\0')
		goto exit;  //but exit at EOF
finish_line:
	ret = -18;
	if (*tp != '\r' && *tp != '\n')
		goto exit;  //Return error if not valid line end
	if (tp[0] == '\r' && tp[1] == '\n')
		tp += 1;  //Step an extra pos for CR+LF
	tp += 1;      //Step past valid line end
	//Here tp points beyond the line of the processed string, so we're done
	ret = length;

exit:
	*LANG_p_p = tp;   //return new LANG file position
	*id_p_p = ip;     //return found index
	*value_p_p = vp;  //return found string value
	return ret;       //return control to caller
}
//Ends get_LANG_string
//---------------------------------------------------------------------------
static int copy_LANG_value(char *dst, const char *src, int len)
{
	int si;
	int di = 0;

	for (si = 0; si < len; si++) {
		if (src[si] == '\\' && si + 1 < len) {
			switch (src[si + 1]) {
				case 'n':
					dst[di++] = '\n';
					si++;
					continue;
				case 'r':
					dst[di++] = '\r';
					si++;
					continue;
				case 't':
					dst[di++] = '\t';
					si++;
					continue;
				case '"':
					dst[di++] = '"';
					si++;
					continue;
				case '\\':
					dst[di++] = '\\';
					si++;
					continue;
				default:
					break;
			}
		}
		dst[di++] = src[si];
	}

	dst[di] = '\0';
	return di;
}

static int isMiscLaunchNameAlias(const char *name, const char *configured_path, const char *default_name)
{
	return !strcmp(name, configured_path + strlen(setting->Misc)) || !strcmp(name, default_name);
}

static void updateLocalizedMiscPaths(void)
{
	int i;
	char *tmp;
	char default_misc[64];
	size_t default_misc_len;

	if (setting == NULL)
		return;

	sprintf(default_misc, "%s/", LNG_DEF(MISC));
	default_misc_len = strlen(default_misc);

	if (strlen(setting->Misc) > 0) {
		for (i = 0; i < 16; i++) {  //Loop to rename the ELF paths with new language for launch keys
			if ((i < 12) || (setting->LK_Flag[i] != 0)) {
				if (!strncmp(setting->LK_Path[i], setting->Misc, strlen(setting->Misc)) ||
				    !strncmp(setting->LK_Path[i], default_misc, default_misc_len)) {
					tmp = strrchr(setting->LK_Path[i], '/');
					if (tmp == NULL)
						continue;
					if (isMiscLaunchNameAlias(tmp + 1, setting->Misc_PS2Disc, LNG_DEF(PS2Disc)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(PS2Disc));
					else if (isMiscLaunchNameAlias(tmp + 1, setting->Misc_FileBrowser, LNG_DEF(FileBrowser)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(FileBrowser));
					else if (isMiscLaunchNameAlias(tmp + 1, setting->Misc_PS2Browser, LNG_DEF(PS2Browser)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(PS2Browser));
					else if (isMiscLaunchNameAlias(tmp + 1, setting->Misc_PS2Net, LNG_DEF(PS2Net)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(PS2Net));
					else if (isMiscLaunchNameAlias(tmp + 1, setting->Misc_PS2PowerOff, LNG_DEF(PS2PowerOff)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(PS2PowerOff));
					else if (isMiscLaunchNameAlias(tmp + 1, setting->Misc_HddManager, LNG_DEF(HddManager)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(HddManager));
					else if (isMiscLaunchNameAlias(tmp + 1, setting->Misc_TextEditor, LNG_DEF(TextEditor)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(TextEditor));
					else if (isMiscLaunchNameAlias(tmp + 1, setting->Misc_Configure, LNG_DEF(Configure)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(Configure));
					else if (isMiscLaunchNameAlias(tmp + 1, setting->Misc_ShowFont, LNG_DEF(ShowFont)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(ShowFont));
					else if (isMiscLaunchNameAlias(tmp + 1, setting->Misc_Debug_Info, LNG_DEF(Debug_Info)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(Debug_Info));
					else if (isMiscLaunchNameAlias(tmp + 1, setting->Misc_About_uLE, LNG_DEF(About_uLE)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(About_uLE));
					else if (isMiscLaunchNameAlias(tmp + 1, setting->Misc_Show_Build_Info, LNG_DEF(Build_Info)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(Build_Info));
					else if (isMiscLaunchNameAlias(tmp + 1, setting->Misc_OSDSYS, LNG_DEF(OSDSYS)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(OSDSYS));
					else if (isMiscLaunchNameAlias(tmp + 1, setting->Misc_Exploit_Installer, LNG_DEF(Exploit_Installer)) ||
					         !strcmp(tmp + 1, "Exploit Installer"))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(Exploit_Installer));
					else if (isMiscLaunchNameAlias(tmp + 1, setting->Misc_Reboot_IOP, LNG_DEF(Reboot_IOP)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(Reboot_IOP));
				}  // end if Misc
			}      // end if LK assigned
		}          // end for
	}              // end if Misc Initialized

	sprintf(setting->Misc, "%s/", LNG(MISC));
	sprintf(setting->Misc_PS2Disc, "%s/%s", LNG(MISC), LNG(PS2Disc));
	sprintf(setting->Misc_FileBrowser, "%s/%s", LNG(MISC), LNG(FileBrowser));
	sprintf(setting->Misc_PS2Browser, "%s/%s", LNG(MISC), LNG(PS2Browser));
	sprintf(setting->Misc_PS2Net, "%s/%s", LNG(MISC), LNG(PS2Net));
	sprintf(setting->Misc_PS2PowerOff, "%s/%s", LNG(MISC), LNG(PS2PowerOff));
	sprintf(setting->Misc_HddManager, "%s/%s", LNG(MISC), LNG(HddManager));
	sprintf(setting->Misc_TextEditor, "%s/%s", LNG(MISC), LNG(TextEditor));
	sprintf(setting->Misc_Configure, "%s/%s", LNG(MISC), LNG(Configure));
	sprintf(setting->Misc_ShowFont, "%s/%s", LNG(MISC), LNG(ShowFont));
	sprintf(setting->Misc_Debug_Info, "%s/%s", LNG(MISC), LNG(Debug_Info));
	sprintf(setting->Misc_About_uLE, "%s/%s", LNG(MISC), LNG(About_uLE));
	sprintf(setting->Misc_Show_Build_Info, "%s/%s", LNG(MISC), LNG(Build_Info));
	sprintf(setting->Misc_OSDSYS, "%s/%s", LNG(MISC), LNG(OSDSYS));
	sprintf(setting->Misc_Exploit_Installer, "%s/%s", LNG(MISC), LNG(Exploit_Installer));
	sprintf(setting->Misc_Reboot_IOP, "%s/%s", LNG(MISC), LNG(Reboot_IOP));
}
//---------------------------------------------------------------------------
void Init_Default_Language(void)
{
	memcpy(Lang_String, Lang_Default, sizeof(Lang_String));
}
//Ends Init_Default_Language
//---------------------------------------------------------------------------
void Set_Language(int language)
{
	releaseExternalLanguageBuffer();
	if (setting != NULL)
		setting->language = normalizeBuiltinLanguage(language);
	memcpy(Lang_String, getBuiltinLanguageTable(language), sizeof(Lang_String));
	updateLocalizedMiscPaths();
}
//Ends Set_Language
//---------------------------------------------------------------------------
void Load_External_Language(void)
{
	int error_id = -1;
	int test = 0;
	u32 index = 0;
	char filePath[MAX_PATH];
	char *file_bp, *file_tp, *lang_bp, *lang_tp, *oldf_tp = NULL;
	char *id_p, *value_p;
	int lang_size = 0;
	int fd;
	Language *Lang;

	releaseExternalLanguageBuffer();

	Lang = getBuiltinLanguageTable(setting != NULL ? setting->language : BUILTIN_LANGUAGE_ENGLISH);
	memcpy(Lang_String, Lang, sizeof(Lang_String));

	if (setting != NULL && strlen(setting->lang_file) != 0) {  //if language file string set

		error_id = -2;
		genFixPath(setting->lang_file, filePath);
		fd = genOpen(filePath, FIO_O_RDONLY);
		if (fd >= 0) {  //if file opened OK
			int file_size = genLseek(fd, 0, SEEK_END);

			error_id = -3;
			if (file_size > 0) {  //if file size OK
				error_id = -4;
				file_bp = malloc(file_size + 1);
				if (file_bp == NULL)
					goto aborted_1;

				error_id = -5;
				genLseek(fd, 0, SEEK_SET);
				if (genRead(fd, file_bp, file_size) != file_size)
					goto release_1;
				file_bp[file_size] = '\0';  //enforce termination at buffer end

				error_id = -6;
				file_tp = file_bp;
				while (1) {
					oldf_tp = file_tp;
					test = get_LANG_string(&file_tp, &id_p, &value_p);
					if (test == -1)           //if EOF reached without other error
						break;                //break from the loop normally
					if (test < 0)             //At any fatal error result
						goto release_1;       //go release file buffer
					index = atoi(id_p);       //get the string index
					if (index >= LANG_COUNT)  //At any fatal error result
						goto release_1;       //go release file buffer
					lang_size += test + 1;    //Include terminator space for total size
				}
				//Here lang_size is the space needed for real language buffer,

				error_id = -7;
				lang_bp = malloc(lang_size + 1);  //allocate real language buffer
				if (lang_bp == NULL)
					goto release_1;

				//We're ready to read language strings, but must first init all pointers
				//to use default strings, for any indexes left undefined by the file
				memcpy(Lang_Extern, Lang, sizeof(Lang_Extern));

				file_tp = file_bp;
				lang_tp = lang_bp;
				while ((test = get_LANG_string(&file_tp, &id_p, &value_p)) >= 0) {
					int decoded_len;

					index = atoi(id_p);                   //get the string index
					Lang_Extern[index].String = lang_tp;  //save pointer to this string base
					decoded_len = copy_LANG_value(lang_tp, value_p, test);
					lang_tp += decoded_len + 1;           //move dest pointer past this string
				}
				External_Lang_Buffer = lang_bp;  //Save base pointer for releases
				Lang = Lang_Extern;
				error_id = 0;
			release_1:
				free(file_bp);
			}  // end if clause for file size OK
		aborted_1:
			genClose(fd);
		}  // end if clause for file opened OK
	}      // end if language file string set

	if (error_id < -1) {
		char tmp_s[4096], t1_s[102], t2_s[102];
		int pos = 0, stp = 0;
		sprintf(tmp_s,
		        "LNG loading failed with error_id==%d and test==%d\n"
		        "The latest string index (possibly invalid) was %d\n"
		        "%n",
		        error_id, test, index, &stp);
		pos += stp;
		if (error_id == -2) {  //if file open failure
			sprintf(tmp_s + pos,
			        "This was a failure to open the file:\n"
			        "\"%s\"\n",
			        filePath);
		}
		if (error_id == -6) {  //if parsing error
			strncpy(t1_s, oldf_tp, 100);
			t1_s[100] = '\0';
			strncpy(t2_s, file_tp, 100);
			t2_s[100] = '\0';
			sprintf(tmp_s + pos,
			        "This was a parsing error when trying to parse the text:\n"
			        "\"%s\"\n"
			        "That attempt failed somehow, after reaching this point:\n"
			        "\"%s\"\n",
			        t1_s, t2_s);
		}
		strcat(tmp_s, "Use either OK or CANCEL to continue (no diff)");
		ynDialog(tmp_s);
	}

	memcpy(Lang_String, Lang, sizeof(Lang_String));
	updateLocalizedMiscPaths();
}
//Ends Load_External_Language
//---------------------------------------------------------------------------
//End of file:  lang.c
//---------------------------------------------------------------------------
