//--------------------------------------------------------------
//File name:   editor_file.c
//--------------------------------------------------------------
#include "editor_private.h"

void editorResetState(void)
{
	int i;

	for (i = 0; i < MAX_ENTRY; i++)
		Editor_nRowsWidth[i] = 0;

	Editor_Cur = 0, Tmp_Cur = 0,
	Editor_nChar = 0, Editor_nRowsNum = 0,
	Editor_nCharRows = 0, Editor_nRowsTop = 0,
	Editor_PushRows = 0, Editor_TextEnd = 0;

	Top_Width = 0, Top_Height = 0;

	Rows_Width = (SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS - 26 - Menu_start_x) / FONT_WIDTH;
	Rows_Num = (Menu_end_y - Menu_start_y) / FONT_HEIGHT;

	KeyBoard_Cur = 2, KeyBoard_Active = 0,
	KeyBoard_Caps = 0,
	KeyBoard_Special = 0,
	Editor_Insert = 1, Editor_RetMode = TextMode[Active_Window];
	Editor_Home = 0, Editor_End = 0;

	del1 = 0, del2 = 0, del3 = 0, del4 = 0;
	ins1 = 0, ins2 = 0, ins3 = 0, ins4 = 0, ins5 = 0;

	if (Mark[MARK_ON]) {
		Mark[MARK_ON] = 0, Mark[MARK_IN] = 0,
		Mark[MARK_OUT] = 0, Mark[MARK_TMP] = 0,
		Mark[MARK_PRINT] = 0, Mark[MARK_COLOR] = 0;
	}
}
int editorNew(int Win)
{
	int ret = 0;

	TextSize[Win] = 0;

	if ((TextBuffer[Win] = malloc(TextSize[Win] + 256)) != NULL) {
		TextBuffer[Win][0] = '\0';
		TextMode[Win] = OTHER;
		Window[Win][CREATED] = 1, Window[Win][OPENED] = 1, Window[Win][SAVED] = 0;
		editorResetState();
		ret = 1;
	}

	if (ret) {
		drawMsg(LNG(File_Created));
	} else {
		drawMsg(LNG(Failed_Creating_File));
	}

	WaitTime = Timer();
	while (Timer() < WaitTime + 1500)
		;  // print operation result during 1.5 sec.

	return ret;
}
int editorOpenFile(int Win)
{
	getFilePath(Path[Win], TEXT_CNF);  // No Filtering, Be Careful.

	return editorOpen(Win, Path[Win]);
}
int editorOpen(int Win, char *path)
{
	int fd, i, ret = 0;
	int size, rd, total;
	char filePath[MAX_PATH];
	char probe;

	if (path[0] == '\0')
		goto abort;

	strncpy(filePath, path, MAX_PATH - 1);
	filePath[MAX_PATH - 1] = '\0';
	if (genFixPath(path, filePath) < 0) {
		strncpy(filePath, path, MAX_PATH - 1);
		filePath[MAX_PATH - 1] = '\0';
	}
	fd = genOpen(filePath, FIO_O_RDONLY);

	if (fd >= 0) {
		size = genLseek(fd, 0, SEEK_END);
		if (size > 512 * 1024)
			goto done;

		/*
		 * Rewind after size probe. Some stacks fail SEEK_END or keep the
		 * file pointer at EOF, which would otherwise yield blank reads.
		 */
		if (genLseek(fd, 0, SEEK_SET) < 0) {
			genClose(fd);
			fd = genOpen(filePath, FIO_O_RDONLY);
			if (fd < 0)
				goto done;
		}

		// Try fast path for known positive size, otherwise fallback to streamed read.
		if (size > 0) {
			if ((TextBuffer[Win] = malloc(size + 256)) != NULL) {
				memset(TextBuffer[Win], 0, size + 256);
				rd = genRead(fd, TextBuffer[Win], size);
				if (rd < 0 || rd == 0) {
					free(TextBuffer[Win]);
					TextBuffer[Win] = NULL;
					goto done;
				}
				TextSize[Win] = rd;
				ret = 1;
			}
		} else {
			if ((TextBuffer[Win] = malloc(512 * 1024 + 256)) != NULL) {
				memset(TextBuffer[Win], 0, 512 * 1024 + 256);
				total = 0;
				while (total < 512 * 1024) {
					int chunk = 4096;
					if ((512 * 1024 - total) < chunk)
						chunk = 512 * 1024 - total;
					rd = genRead(fd, TextBuffer[Win] + total, chunk);
					if (rd < 0) {
						free(TextBuffer[Win]);
						TextBuffer[Win] = NULL;
						goto done;
					}
					if (rd == 0)
						break;
					total += rd;
				}
				if (total == 512 * 1024 && genRead(fd, &probe, 1) > 0) {
					// File exceeds editor limit.
					free(TextBuffer[Win]);
					TextBuffer[Win] = NULL;
					goto done;
				}
				TextSize[Win] = total;
				ret = 1;
			}
		}

		if (ret) {
			for (i = 0; i < TextSize[Win]; i++) {  // Scan for text mode.
				if (TextBuffer[Win][i] == '\n' && (i == 0 || TextBuffer[Win][i - 1] != '\r')) {
					// Mode MAC: LF line endings.
					TextMode[Win] = MAC;
					break;
				} else if (TextBuffer[Win][i] == '\r' && (i + 1) < TextSize[Win] && TextBuffer[Win][i + 1] == '\n') {
					// Mode OTHER: CRLF line endings.
					TextMode[Win] = OTHER;
					break;
				} else if (TextBuffer[Win][i] == '\r' && ((i + 1) >= TextSize[Win] || TextBuffer[Win][i + 1] != '\n')) {
					// Mode UNIX: CR line endings.
					TextMode[Win] = UNIX;
					break;
				}
			}

			Window[Win][OPENED] = 1;
			Window[Win][SAVED] = 0;
			editorResetState();
		}
	}

done:
	if (fd >= 0)
		genClose(fd);

	if (ret) {
		drawMsg(LNG(File_Opened));
	} else {
	abort:
		TextSize[Win] = 0;
		drawMsg(LNG(Failed_Opening_File));
	}

	WaitTime = Timer();
	while (Timer() < WaitTime + 1500)
		;  // print operation result during 1.5 sec.

	return ret;
}
void editorClose(int Win)
{
	char msg[MAX_PATH];

	//memset(TextBuffer[Win], 0, TextSize[Win]+256); // 256 To Avoid Crash 256???
	free(TextBuffer[Win]);

	if (Window[Win][CREATED])
		strcpy(msg, LNG(File_Not_Yet_Saved_Closed));
	else
		sprintf(msg, LNG(File_Closed), Path[Win]);

	Path[Win][0] = '\0';

	TextMode[Win] = 0, TextSize[Win] = 0, Window[Win][CREATED] = 0, Window[Win][OPENED] = 0, Window[Win][SAVED] = 1;

	editorResetState();

	drawMsg(msg);

	WaitTime = Timer();
	while (Timer() < WaitTime + 1500)
		;  // print operation result during 1.5 sec.
}
void editorSave(int Win)
{
	int fd, ret = 0;

	char filePath[MAX_PATH];

	if (!strncmp(Path[Win], "cdfs", 4))
		goto abort;
	strncpy(filePath, Path[Win], MAX_PATH - 1);
	filePath[MAX_PATH - 1] = '\0';
	if (genFixPath(Path[Win], filePath) < 0) {
		strncpy(filePath, Path[Win], MAX_PATH - 1);
		filePath[MAX_PATH - 1] = '\0';
	}

	fd = genOpen(filePath, FIO_O_CREAT | FIO_O_WRONLY | FIO_O_TRUNC);

	if (fd >= 0) {
		if (TextMode[Win] == OTHER && TextBuffer[Win][TextSize[Win]] == '\n')
			genWrite(fd, TextBuffer[Win], TextSize[Win] - 1);
		else
			genWrite(fd, TextBuffer[Win], TextSize[Win]);
		Window[Win][OPENED] = 1, Window[Win][SAVED] = 1;
		ret = 1;
	}

	if (fd >= 0)
		genClose(fd);
	if (!strncmp(filePath, "pfs", 3))
		unmountParty(filePath[3] - '0');

	if (ret) {
		drawMsg(LNG(File_Saved));
	} else {
	abort:
		drawMsg(LNG(Failed_Saving_File));
	}

	WaitTime = Timer();
	while (Timer() < WaitTime + 1500)
		;  // print operation result during 1.5 sec.
}
void editorSaveAs(int Win)
{
	int fd, ret = 0;
	char tmp[MAX_PATH], oldPath[MAX_PATH], filePath[MAX_PATH];
	char *p;

	tmp[0] = '\0', oldPath[0] = '\0', filePath[0] = '\0';

	if (Path[Win][0] != '\0') {
		strcpy(oldPath, Path[Win]);
		Path[Win][0] = '\0';
		p = strrchr(oldPath, '/');
		if (p)
			strcpy(tmp, p + 1);
	}

	getFilePath(Path[Win], DIR_CNF);
	if (Path[Win][0] == '\0')
		goto abort;
	if (!strncmp(Path[Win], "cdfs", 4))
		goto abort;

	drawMsg(LNG(Enter_File_Name));

	if (keyboard(tmp, 36) > 0) {
		//strcat(Path[Win], tmp); //This is what we want, but malfunctions for MC!
		//sprintf(&Path[Win][strlen(Path[Win])], "%s", tmp); //This always works
		strcpy(&Path[Win][strlen(Path[Win])], tmp);  //And this one works too
		                                             //Note that the strcat call SHOULD have done the same thing, but won't.
	} else
		goto abort;

	strncpy(filePath, Path[Win], MAX_PATH - 1);
	filePath[MAX_PATH - 1] = '\0';
	if (genFixPath(Path[Win], filePath) < 0) {
		strncpy(filePath, Path[Win], MAX_PATH - 1);
		filePath[MAX_PATH - 1] = '\0';
	}

	fd = genOpen(filePath, FIO_O_CREAT | FIO_O_WRONLY | FIO_O_TRUNC);

	if (fd >= 0) {
		if (TextMode[Win] == OTHER && TextBuffer[Win][TextSize[Win]] == '\n')
			genWrite(fd, TextBuffer[Win], TextSize[Win] - 1);
		else
			genWrite(fd, TextBuffer[Win], TextSize[Win]);
		Window[Win][CREATED] = 0, Window[Win][OPENED] = 1, Window[Win][SAVED] = 1;
		ret = 1;
		genClose(fd);
	}

	if (!strncmp(filePath, "pfs", 3))
		unmountParty(filePath[3] - '0');

	if (ret) {
		drawMsg(LNG(File_Saved));
		goto result_delay;
	}

abort:
	if (oldPath[0] != '\0')
		strcpy(Path[Win], oldPath);
	drawMsg(LNG(Failed_Saving_File));

result_delay:
	WaitTime = Timer();
	while (Timer() < WaitTime + 1500)
		;  // display operation result during 1.5 sec.
}
//--------------------------------------------------------------
//End of file: editor_file.c
//--------------------------------------------------------------
