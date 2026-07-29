#ifndef MAIN_GAMEID_H
#define MAIN_GAMEID_H

#include <stddef.h>

#define PS1_PVD_VOLUME_TIMESTAMP_OFFSET 0x32D
#define PS1_PVD_VOLUME_TIMESTAMP_LEN 16

typedef struct
{
	const char *volume_timestamp;
	const char *game_id;
} PS1_GENERIC_GAME_ID;

void displayRetroGemGameID(const char *gameID, int frames);
int buildLaunchGameID(const char *exec_path, char *gameID, size_t gameID_len);
int buildPS1GenericGameIDFromTimestamp(const char *volume_timestamp, char *gameID, size_t gameID_len);
int buildPS1GenericDiscGameID(char *gameID, size_t gameID_len);
int isLikelyDiscLaunch(const char *selected_path);

#endif
