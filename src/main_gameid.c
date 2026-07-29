//--------------------------------------------------------------
//File name:   main_gameid.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "main_gameid.h"

#define PS1_PVD_LBA 16
#define PS1_CD_SECTOR_DATA_SIZE 2048

extern const PS1_GENERIC_GAME_ID ps1_generic_game_ids[];
extern const size_t ps1_generic_game_ids_count;

static u8 calculateRetroGemCRC(const u8 *data, int len)
{
	int i;
	u8 crc = 0x00;

	for (i = 0; i < len; i++)
		crc = (u8)(crc + data[i]);

	return (u8)(0x100 - crc);
}

void displayRetroGemGameID(const char *gameID, int frames)
{
	GSGLOBAL *gid_gs;
	u8 data[64];
	int gidlen, dpos, data_len, xstart, ystart, height;
	int i, j, frame;

	if (gameID == NULL || gameID[0] == '\0')
		return;

	gidlen = strnlen(gameID, 11);
	if (gidlen <= 0)
		return;

	if (frames < 1)
		frames = 1;
	else if (frames > 3)
		frames = 3;

	gid_gs = gsKit_init_global();
	if (gid_gs == NULL)
		return;

	gid_gs->DoubleBuffering = GS_SETTING_ON;
	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
	if (dmaKit_chan_init(DMA_CHANNEL_GIF) != 0) {
		gsKit_deinit_global(gid_gs);
		return;
	}

	gsKit_init_screen(gid_gs);
	gsKit_display_buffer(gid_gs);
	gsKit_mode_switch(gid_gs, GS_ONESHOT);

	memset(data, 0, sizeof(data));
	dpos = 0;
	data[dpos++] = 0xA5;
	data[dpos++] = 0x00;
	dpos++; // CRC placeholder
	data[dpos++] = (u8)gidlen;
	memcpy(&data[dpos], gameID, gidlen);
	dpos += gidlen;
	data[dpos++] = 0x00;
	data[dpos++] = 0xD5;
	data[dpos++] = 0x00;

	data_len = dpos;
	data[2] = calculateRetroGemCRC(&data[3], data_len - 3);

	xstart = (gid_gs->Width / 2) - (data_len * 8);
	ystart = gid_gs->Height - (((gid_gs->Height / 8) * 2) + 20);
	height = 2;

	for (frame = 0; frame < frames; frame++) {
		gsKit_clear(gid_gs, GS_SETREG_RGBA(0x00, 0x00, 0x00, 0x00));

		for (i = 0; i < data_len; i++) {
			for (j = 7; j >= 0; j--) {
				int x = xstart + (i * 16 + ((7 - j) * 2));
				int x1 = x + 1;
				u64 bit_color;

				gsKit_prim_sprite(gid_gs, x, ystart, x1, ystart + height, 0, GS_SETREG_RGBA(0xFF, 0x00, 0xFF, 0x00));
				bit_color = ((data[i] >> j) & 1) ? GS_SETREG_RGBA(0x00, 0xFF, 0xFF, 0x00) : GS_SETREG_RGBA(0xFF, 0xFF, 0x00, 0x00);
				gsKit_prim_sprite(gid_gs, x1, ystart, x1 + 1, ystart + height, 0, bit_color);
			}
		}

		gsKit_queue_exec(gid_gs);
		gsKit_finish();
		gsKit_sync_flip(gid_gs);
	}

	for (frame = 0; frame < 2; frame++) {
		gsKit_clear(gid_gs, GS_SETREG_RGBA(0x00, 0x00, 0x00, 0x00));
		gsKit_queue_exec(gid_gs);
		gsKit_finish();
		gsKit_sync_flip(gid_gs);
	}

	gsKit_deinit_global(gid_gs);
}

static int isDiscExecPath(const char *exec_path)
{
	return (!strncmp(exec_path, "cdrom", 5) || !strncmp(exec_path, "cdfs", 4));
}

static int isLikelyTitleID(const char *gameID)
{
	return (gameID != NULL &&
	        strlen(gameID) >= 11 &&
	        gameID[4] == '_' &&
	        (gameID[7] == '.' || gameID[8] == '.'));
}

int buildPS1GenericGameIDFromTimestamp(const char *volume_timestamp, char *gameID, size_t gameID_len)
{
	size_t i;

	if (volume_timestamp == NULL || gameID == NULL || gameID_len < 12)
		return 0;

	gameID[0] = '\0';
	for (i = 0; i < ps1_generic_game_ids_count; i++) {
		if (!strncmp(volume_timestamp,
		             ps1_generic_game_ids[i].volume_timestamp,
		             PS1_PVD_VOLUME_TIMESTAMP_LEN)) {
			snprintf(gameID, gameID_len, "%s", ps1_generic_game_ids[i].game_id);
			return 1;
		}
	}

	return 0;
}

int buildPS1GenericDiscGameID(char *gameID, size_t gameID_len)
{
	unsigned char sector[PS1_CD_SECTOR_DATA_SIZE] __attribute__((aligned(16)));
	sceCdRMode read_mode;

	if (gameID == NULL || gameID_len < 12)
		return 0;
	gameID[0] = '\0';

	memset(sector, 0, sizeof(sector));
	read_mode.trycount = 3;
	read_mode.spindlctrl = SCECdSpinNom;
	read_mode.datapattern = SCECdSecS2048;
	read_mode.pad = 0;

	if (!sceCdRead(PS1_PVD_LBA, 1, sector, &read_mode) || sceCdSync(0))
		return 0;
	if (strncmp((const char *)sector + 1, "CD001", 5))
		return 0;

	return buildPS1GenericGameIDFromTimestamp((const char *)sector + PS1_PVD_VOLUME_TIMESTAMP_OFFSET,
	                                          gameID, gameID_len);
}

int buildLaunchGameID(const char *exec_path, char *gameID, size_t gameID_len)
{
	const char *start;
	const char *sep;
	size_t i;
	int disc_exec_path;
	int title_id_like;

	if (gameID == NULL || gameID_len < 12 || exec_path == NULL || exec_path[0] == '\0')
		return 0;

	gameID[0] = '\0';
	disc_exec_path = isDiscExecPath(exec_path);

	/* Disc executables (e.g. cdrom0:\\SLUS_123.45;1 or cdfs:/SLUS_123.45;1) already carry a title ID. */
	if (disc_exec_path) {
		start = strchr(exec_path, '\\');
		if (start == NULL)
			start = strchr(exec_path, ':');
		if (start != NULL) {
			start++;
			while (*start == '/' || *start == '\\')
				start++;
		}
	} else {
		start = exec_path;
		sep = strrchr(exec_path, '/');
		if (sep != NULL)
			start = sep + 1;
		sep = strrchr(exec_path, '\\');
		if (sep != NULL && (sep + 1) > start)
			start = sep + 1;
		sep = strrchr(exec_path, ':');
		if (sep != NULL && (sep + 1) > start)
			start = sep + 1;
	}

	if (start == NULL || *start == '\0')
		return 0;

	title_id_like = isLikelyTitleID(start);
	if (disc_exec_path && !title_id_like)
		return buildPS1GenericDiscGameID(gameID, gameID_len);

	for (i = 0; i < 11 && start[i] != '\0'; i++) {
		char c = start[i];
		if (c == ';' || c == '/' || c == '\\')
			break;
		if (c == '.' && !disc_exec_path && !title_id_like)
			break;
		gameID[i] = c;
	}
	gameID[i] = '\0';

	while (i > 0) {
		char c = gameID[i - 1];
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
			gameID[--i] = '\0';
		else
			break;
	}

	if (!strcmp(gameID, "???"))
		return 0;

	return gameID[0] != '\0';
}

int isLikelyDiscLaunch(const char *selected_path)
{
	if (selected_path == NULL || selected_path[0] == '\0')
		return 0;
	if (!stricmp(selected_path, setting->Misc_PS2Disc))
		return 1;
	if (isDiscExecPath(selected_path))
		return 1;
	return 0;
}
