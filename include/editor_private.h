#ifndef EDITOR_PRIVATE_H
#define EDITOR_PRIVATE_H

#include "launchelf.h"

enum {
	COL_NORM_BG = GS_SETREG_RGBA(160, 160, 160, 0),
	COL_NORM_TEXT = GS_SETREG_RGBA(0, 0, 0, 0),
	COL_MARK_BG = GS_SETREG_RGBA(0, 40, 160, 0),
	COL_MARK_TEXT = GS_SETREG_RGBA(160, 160, 160, 0),
	COL_CUR_INSERT = GS_SETREG_RGBA(0, 160, 0, 0),
	COL_CUR_OVERWR = GS_SETREG_RGBA(160, 0, 0, 0),
	COL_LINE_END = GS_SETREG_RGBA(160, 80, 0, 0),
	COL_TAB = GS_SETREG_RGBA(0, 0, 160, 0),
	COL_TEXT_END = GS_SETREG_RGBA(0, 160, 160, 0),
	COL_dummy
};

enum {
	NEW,
	OPEN,
	CLOSE,
	SAVE,
	SAVE_AS,
	WINDOWS,
	LAUNCH_WITH_ARGS,
	EXIT,
	NUM_MENU
};  // Menu Fonction.

enum {
	MARK_START,
	MARK_ON,
	MARK_COPY,
	MARK_CUT,
	MARK_IN,
	MARK_OUT,
	MARK_TMP,
	MARK_SIZE,
	MARK_PRINT,
	MARK_COLOR,
	NUM_MARK
};  // Marking Fonction/State.

enum {
	CREATED,
	OPENED,
	SAVED,
	NUM_STATE
};  // Windowing State.

#define OTHER 1  // CR/LF Return Text Mode, 'All Normal System'.
#define UNIX 2   // CR Return Text Mode, Unix.
#define MAC 3    // LF Return Text Mode, Mac.

#define TMP 10   // Temp Buffer For Add / Remove Char.
#define EDIT 11  // Edit Buffer For Copy / Cut / Paste.

extern char *TextBuffer[12];
extern int Window[10][NUM_STATE];
extern int TextMode[10];
extern int TextSize[10];
extern int Editor_nRowsWidth[MAX_ENTRY];
extern int Mark[NUM_MARK];
extern int Active_Window;
extern int Num_Window;
extern int Editor_Cur;
extern int Tmp_Cur;
extern int Editor_nChar;
extern int Editor_nRowsNum;
extern int Editor_nCharRows;
extern int Editor_nRowsTop;
extern int Rows_Num;
extern int Rows_Width;
extern int Top_Width;
extern int Top_Height;
extern int Editor_Insert;
extern int Editor_RetMode;
extern int Editor_Home;
extern int Editor_End;
extern int Editor_PushRows;
extern int Editor_TextEnd;
extern int KeyBoard_Cur;
extern int KeyBoard_Active;
extern int KeyBoard_Caps;
extern int KeyBoard_Special;
extern int del1, del2, del3, del4;
extern int ins1, ins2, ins3, ins4, ins5;
extern int t;
extern char Path[10][MAX_PATH];
extern const int WFONTS;
extern const int HFONTS;
extern const unsigned char EditorSpecialChars[];
extern const int EditorSpecialCharCount;

int editorMenu(void);
void editorVirtualKeyboardEntry(void);
int editorKeyboardEntry(void);
int isEditorSpecialKeyboardKey(int editor_index);
unsigned char getEditorSpecialKeyboardChar(int editor_index);
int getEditorSpecialKeyboardNextKey(int editor_index, int dx, int dy);
void editorApplyRules(void);
int editorSelectWindow(void);
void editorResetState(void);
int editorNew(int Win);
int editorOpenFile(int Win);
int editorOpen(int Win, char *path);
void editorClose(int Win);
void editorSave(int Win);
void editorSaveAs(int Win);

#endif
