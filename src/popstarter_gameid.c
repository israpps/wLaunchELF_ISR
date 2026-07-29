#include "popstarter_internal.h"
#include "main_gameid.h"

static const char *getFinalNameComponent(const char *path)
{
	const char *name;
	const char *sep;

	if (path == NULL)
		return NULL;

	name = path;
	sep = strrchr(path, '/');
	if (sep != NULL)
		name = sep + 1;
	sep = strrchr(path, '\\');
	if (sep != NULL && sep + 1 > name)
		name = sep + 1;
	sep = strrchr(path, ':');
	if (sep != NULL && sep + 1 > name)
		name = sep + 1;

	return name;
}

static int buildGameIDFromPopstarterName(const char *name, char *gameID, size_t gameID_len)
{
	const char *id_name;

	id_name = getFinalNameComponent(name);
	if (id_name == NULL || id_name[0] == '\0')
		return 0;

	if (!prefixCaseCmp(id_name, "XX.", 3) ||
	    !prefixCaseCmp(id_name, "__.", 3) ||
	    !prefixCaseCmp(id_name, "PP.", 3))
		id_name += 3;

	return buildLaunchGameID(id_name, gameID, gameID_len);
}

static int isLikelyTitleID(const char *gameID)
{
	return (gameID != NULL &&
	        strlen(gameID) >= 11 &&
	        gameID[4] == '_' &&
	        (gameID[7] == '.' || gameID[8] == '.'));
}

static int buildVcdPvdTimestampGameID(const char *path, char *gameID, size_t gameID_len)
{
	unsigned char pvd[POPSTARTER_CD_SECTOR_DATA_SIZE] __attribute__((aligned(16)));
	int fd;

	if (gameID == NULL || gameID_len == 0)
		return 0;
	gameID[0] = '\0';

	fd = openPathForRead(path, NULL, 0);
	if (fd < 0)
		return 0;

	if (!readVcdIsoPrimaryVolumeDescriptor(fd, pvd)) {
		genClose(fd);
		return 0;
	}
	genClose(fd);

	return buildPS1GenericGameIDFromTimestamp((const char *)pvd + PS1_PVD_VOLUME_TIMESTAMP_OFFSET,
	                                          gameID, gameID_len);
}

static int lineStartsWithBootKey(const char *line)
{
	const char *p;

	if (line == NULL)
		return 0;

	p = line;
	while (*p == ' ' || *p == '\t')
		p++;
	if (prefixCaseCmp(p, "BOOT", 4))
		return 0;
	p += 4;
	return (*p == '=' || *p == ' ' || *p == '\t');
}

static int parseSystemCnfBootGameID(const char *cnf, char *gameID, size_t gameID_len)
{
	char boot_path[MAX_PATH];
	const char *line;
	const char *line_end;
	const char *value;
	size_t value_len;

	if (cnf == NULL || gameID == NULL || gameID_len == 0)
		return 0;

	gameID[0] = '\0';
	line = cnf;
	while (*line != '\0') {
		line_end = line;
		while (*line_end != '\0' && *line_end != '\r' && *line_end != '\n')
			line_end++;

		if (lineStartsWithBootKey(line)) {
			value = line + strcspn(line, "=");
			if (value < line_end && *value == '=') {
				value++;
				while (value < line_end && (*value == ' ' || *value == '\t'))
					value++;

				value_len = 0;
				while (value + value_len < line_end &&
				       value[value_len] != ' ' &&
				       value[value_len] != '\t')
					value_len++;
				if (value_len > 0 && value_len < sizeof(boot_path)) {
					memcpy(boot_path, value, value_len);
					boot_path[value_len] = '\0';
					if (buildGameIDFromPopstarterName(boot_path, gameID, gameID_len) &&
					    isLikelyTitleID(gameID))
						return 1;
					gameID[0] = '\0';
				}
			}
		}

		line = line_end;
		while (*line == '\r' || *line == '\n')
			line++;
	}

	return 0;
}

static int buildVcdSystemCnfGameID(const char *path, char *gameID, size_t gameID_len)
{
	char cnf[POPSTARTER_CD_SECTOR_DATA_SIZE + 1];
	int fd;
	int ret;

	if (gameID == NULL || gameID_len == 0)
		return 0;
	gameID[0] = '\0';

	fd = openPathForRead(path, NULL, 0);
	if (fd < 0)
		return 0;

	ret = 0;
	if (readVcdSystemCnf(fd, cnf, sizeof(cnf)))
		ret = parseSystemCnfBootGameID(cnf, gameID, gameID_len);
	genClose(fd);

	return ret;
}

int buildPopstarterVcdGameID(const char *path, char *gameID, size_t gameID_len)
{
	char hdd_device[6];
	char partition[MAX_PART_NAME + 1];
	const char *subpath;
	int ret;

	if (gameID == NULL || gameID_len == 0)
		return 0;
	gameID[0] = '\0';

	if (buildVcdSystemCnfGameID(path, gameID, gameID_len))
		return 1;
	if (buildVcdPvdTimestampGameID(path, gameID, gameID_len))
		return 1;

	ret = splitPopstarterHddLaunchPath(path, hdd_device, sizeof(hdd_device), partition, sizeof(partition), &subpath);
	if (ret > 0) {
		if (!strncmp(partition, "__.POPS", 7))
			return buildGameIDFromPopstarterName(subpath, gameID, gameID_len);
		if ((!strncmp(partition, "__.", 3) || !prefixCaseCmp(partition, "PP.", 3)) &&
		    subpath != NULL && !stricmp(subpath, "/IMAGE0.VCD"))
			return buildGameIDFromPopstarterName(partition, gameID, gameID_len);
	}

	return buildGameIDFromPopstarterName(path, gameID, gameID_len);
}
