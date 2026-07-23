//--------------------------------------------------------------
//File name:   main_fileops.c
//--------------------------------------------------------------
#include "launchelf.h"

//exists. Tests if a file exists or not
int wleExists(const char *path)
{
	int fd;

	fd = genOpen(path, FIO_O_RDONLY);
	if (fd < 0)
		return 0;
	genClose(fd);
	return 1;
}

//uLE_related. Tests if an uLE-related file exists or not.
//
// Note: please use genFixPath() for config files instead because this will not
//       load other device modules, even if LaunchELF actually supports the device.
//
// Returns:
//  1 == uLE related path with file present
//  0 == uLE related path with file missing
// -1 == Not uLE related path
int uLE_related(char *pathout, const char *pathin)
{
	int ret;

	if (!strncmp(pathin, "uLE:/", 5)) {
		sprintf(pathout, "%s%s", LaunchElfDir, pathin + 5);
		if (wleExists(pathout))
			return 1;

		sprintf(pathout, "%s%s", "mc0:/SYS-CONF/", pathin + 5);
		if (!strncmp(LaunchElfDir, "mc1", 3))
			pathout[2] = '1';
		if (wleExists(pathout))
			return 1;

		pathout[2] ^= 1;  //switch between mc0 and mc1
		if (wleExists(pathout))
			return 1;

		//Default to LaunchElfDir
		sprintf(pathout, "%s%s", LaunchElfDir, pathin + 5);
		return 0;
	}

	ret = -1;
	strcpy(pathout, pathin);
	return ret;
}

int IsTextEditorFileType(const char *path)
{
	return (genCmpFileExt(path, "TXT") ||
	        genCmpFileExt(path, "CHT") ||
	        genCmpFileExt(path, "CFG") ||
	        genCmpFileExt(path, "INI") ||
	        genCmpFileExt(path, "CNF") ||
	        genCmpFileExt(path, "PBT") ||
	        genCmpFileExt(path, "JS") ||
	        genCmpFileExt(path, "LUA") ||
	        genCmpFileExt(path, "XML") ||
	        genCmpFileExt(path, "TOML") ||
	        genCmpFileExt(path, "YAML") ||
	        genCmpFileExt(path, "YML") ||
	        genCmpFileExt(path, "ARG"));
}

int IsSupportedFileType(char *path)
{
	if (strchr(path, ':') != NULL) {
		if (genCmpFileExt(path, "ELF") || genCmpFileExt(path, "XLF") || genCmpFileExt(path, "KELF"))
			return (checkELFheader(path) >= 0);
		if (IsPopstarterVcdPath(path))
			return 1;
		if (IsTextEditorFileType(path))
			return 1;
		return 0;
	}

	//No ':', hence no device name in path, which means it is a special action (e.g. MISC/*).
	return 1;
}
