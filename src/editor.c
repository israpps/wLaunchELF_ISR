//--------------------------------------------------------------
//File name:    editor.c
//--------------------------------------------------------------
#include "editor_private.h"


char *TextBuffer[12];       // Text Buffers, 10 Windows Max + 1 TMP + 1 EDIT. See above.
int Window[10][NUM_STATE],  // Windowing System, 10 Windows Max.
    TextMode[10],                  // Text Mode, UNIX, MAC, OTHER.
    TextSize[10],                  // Text Size, 10 Windows Max.
    Editor_nRowsWidth[MAX_ENTRY],  // Current Window, Char Number For Each Rows. (MAX_ENTRY???)
    Mark[NUM_MARK],                // Marking System,(Mark, Copy, Cut, Paste, Delete),Work From 1 Window To An Other.
    Active_Window,                 // Activated Windows Number.
    Num_Window,                    // Opened Windows Count.
    Editor_Cur,                    // Text Cursor.
    Tmp_Cur,                       // Temp Cursor For Rules Calculations.
    Editor_nChar,                  // Current Window, Total Char Number.
    Editor_nRowsNum,               // Current Window, Total Rows Number.
    Editor_nCharRows,              // Current Window, Char Number Per Rows Height.
    Editor_nRowsTop,               // Current Window, Rows Number Above Top Screen For Rules Calculations.
    Rows_Num,                      // Rows Number Feeting In Screen Height.
    Rows_Width,                    // Char Number Feeting In Screen Width.
    Top_Width,                     // Char Number In Rows Above Top Screen.
    Top_Height,                    // Current Window Rows Number Above Top Screen.
    Editor_Insert,                 // Setting Insert On/Off.
    Editor_RetMode,                // Setting Insert On/Off.
    Editor_Home,                   // Goto Line Home.
    Editor_End,                    // Goto Line End
    Editor_PushRows,               // Push 1 Row Height Up(+1) Or Down(-1), Use For Page Up/Down Too.
    Editor_TextEnd,                // Set To 1 When '\0' Char Is Found Tell Text End.
    KeyBoard_Cur,                  // Virtual KeyBoard Cursor.
    KeyBoard_Active,               // Virtual KeyBoard Activated Or Not.
    KeyBoard_Caps,                 // Virtual KeyBoard caps toggle.
    KeyBoard_Special,              // Virtual KeyBoard special layout toggle.
    del1, del2, del3, del4,        // Deleted Chars Different Cases.
    ins1, ins2, ins3, ins4, ins5,  // Added Chars Different Cases.
    t;                             // Text Cursor Timer.
char Path[10][MAX_PATH];    // File Path For Each Opened Windows. 10 Max.


//--------------------------------------------------------------
//ends menu.
//--------------------------------------------------------------
//------------------------------
//endfunc Virt_KeyBoard_Entry
//--------------------------------------------------------------
//------------------------------
//endfunc KeyBoard_Entry
//--------------------------------------------------------------
//--------------------------------------------------------------

static int editorKeyboardTextWidth(int x, int box_right)
{
	int width;

	width = box_right - x + 1;
	return (width > 0) ? width : FONT_WIDTH;
}

static void editorKeyboardPrintLeft(const char *label, int box_left, int box_right, int y, u64 color)
{
	int x = box_left + LINE_THICKNESS;

	printXY(label, x, y, color, TRUE, editorKeyboardTextWidth(x, box_right));
}

//ends Window_Selector.
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------

//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
int TextEditor(char *path)
{
	char tmp[MAX_PATH], tmp1[MAX_PATH], tmp2[MAX_PATH];
	unsigned int key_char;
	const char *key_label;
	const char *special_toggle_label;
	int ch;
	int x, y, x0, x1, y0, y1;
	int i = 0, j, ret = 0, layout_index;
	int tmpLen = 0;
	int event = 1, post_event = 0;
	int Editor_Start = 0;
	int editor_result = TEXTEDITOR_RESULT_EXIT;
	int key_grid_first_col, key_grid_visible_cols, key_grid_visible_w, key_grid_area_x, key_grid_area_w, KEY_GRID_X, KEY_SPECIAL_GRID_X;
	u64 color;
	const int KEY_W = 350,
	          KEY_H = 98,
	          KEY_X = (SCREEN_WIDTH - KEY_W) / 2,
	          KEY_Y = (Menu_end_y - KEY_H),
	          KEY_CELL_W = 20,
	          KEY_BOX1_LEFT = SCREEN_MARGIN,
	          KEY_BOX1_RIGHT = KEY_X - 48 - 1,
	          KEY_BOX2_LEFT = KEY_X - 48 + LINE_THICKNESS,
	          KEY_BOX2_RIGHT = KEY_X + 32 - 1,
	          KEY_BOX4_LEFT = KEY_X + KEY_W + 32 + LINE_THICKNESS,
	          KEY_BOX4_RIGHT = SCREEN_WIDTH - SCREEN_MARGIN - 1;
	int KEY_LEN = VKEY_EDITOR_SIZE;

	tmp[0] = '\0', tmp1[0] = '\0', ch = '\0';
	key_grid_first_col = getVirtualKeyboardLayoutFirstColumn(setting->virtual_keyboard_layout);
	key_grid_visible_cols = getVirtualKeyboardLayoutColumnCount(setting->virtual_keyboard_layout);
	key_grid_visible_w = (key_grid_visible_cols - 1) * KEY_CELL_W + FONT_WIDTH;
	key_grid_area_x = KEY_X + 32 + LINE_THICKNESS;
	key_grid_area_w = KEY_W - LINE_THICKNESS;
	KEY_GRID_X = key_grid_area_x + ((key_grid_area_w - key_grid_visible_w) / 2) - key_grid_first_col * KEY_CELL_W;
	KEY_SPECIAL_GRID_X = key_grid_area_x + ((key_grid_area_w - ((VKEY_LAYOUT_COLS - 1) * KEY_CELL_W + FONT_WIDTH)) / 2);

	Active_Window = 0, Num_Window = 0;

	for (i = 0; i < 10; i++) {
		Window[i][CREATED] = 0;
		Window[i][OPENED] = 0;
		Window[i][SAVED] = 1;
		TextMode[i] = 0;
		TextSize[i] = 0;
		Path[i][0] = '\0';
	}

	for (i = 0; i < NUM_MARK; i++)
		Mark[i] = 0;

	editorResetState();

	t = 0;

	event = 1;  //event = initial entry.

	Rows_Width = (SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS - 26 - Menu_start_x) / FONT_WIDTH;
	Rows_Num = (Menu_end_y - Menu_start_y) / FONT_HEIGHT;

	x = Menu_start_x;
	y = Menu_start_y;

	if (path != NULL) {
		Active_Window = 0;

		strncpy(Path[Active_Window], path, MAX_PATH - 1);
		Path[Active_Window][MAX_PATH - 1] = '\0';
		ret = editorOpen(Active_Window, Path[Active_Window]);
		if (!ret)
			goto fail;

		Editor_Cur = 0, Editor_PushRows = 0;
		Num_Window = 1;
	}

	while (1) {

		//Pad response section.
		waitPadReady(0, 0);
		if (readpad_no_KB()) {
			if (new_pad) {
				event |= 2;  //event |= pad command.
			}
			if (!KeyBoard_Active) {      // Pad Response Without KeyBoard.
				if (new_pad & PAD_UP) {  // Text move up.
					if (Editor_Cur > 0)
						Editor_PushRows++;
					else if (Editor_nChar > 0) {
						Editor_Cur = Editor_nChar - 1;
						if (Editor_Cur > 0 && TextBuffer[Active_Window][Editor_Cur] == '\0')
							Editor_Cur--;
						Editor_PushRows = 0;
					}
				} else if (new_pad & PAD_DOWN) {  // Text move down.
					if (Editor_Cur < Editor_nChar && TextBuffer[Active_Window][Editor_Cur] != '\0')
						Editor_PushRows--;
					else {
						Editor_Cur = 0;
						Editor_PushRows = 0;
					}
				} else if (new_pad & PAD_LEFT) {  // Text move left.
					if (Editor_Cur > 0)
						Editor_Cur--;
				} else if (new_pad & PAD_RIGHT) {  // Text move right.
					if (Editor_Cur < Editor_nChar && TextBuffer[Active_Window][Editor_Cur] != '\0')
						Editor_Cur++;
				} else if (new_pad & PAD_L2) {  // Text move page up.
					if (Editor_Cur > 0)
						Editor_PushRows += 1 * (Rows_Num - 1);
				} else if (new_pad & PAD_R2) {  // Text move page down.
					if (Editor_Cur < Editor_nChar && TextBuffer[Active_Window][Editor_Cur] != '\0')
						Editor_PushRows -= 1 * (Rows_Num - 1);
				} else if (new_pad & PAD_SELECT && Window[Active_Window][OPENED]) {  // Virtual KeyBoard Active Rows_Num -= 7.
					KeyBoard_Cur = 2;
					KeyBoard_Special = 0;
					Rows_Num -= 6;
					KeyBoard_Active = 1;
				}
			} else {  // Pad Response With Virtual KeyBoard.
				editorVirtualKeyboardEntry();
			}

			if (new_pad & PAD_TRIANGLE) {  // General Pad Response.
			exit:
				drawMsg(LNG(Exiting_Editor));
				for (i = 0; i < 10; i++) {
					if (!Window[i][SAVED])
						goto unsave;
				}
			force:
				for (i = 0; i < 10; i++) {
					if (Window[i][OPENED]) {
						editorClose(i);
						Path[i][0] = '\0';
					}
				}
				if (Mark[MARK_COPY] || Mark[MARK_CUT])
					free(TextBuffer[EDIT]);
				Mark[MARK_START] = 0, Mark[MARK_ON] = 0, Mark[MARK_COPY] = 0, Mark[MARK_CUT] = 0,
				Mark[MARK_IN] = 0, Mark[MARK_OUT] = 0, Mark[MARK_TMP] = 0,
				Mark[MARK_SIZE] = 0, Mark[MARK_PRINT] = 0, Mark[MARK_COLOR] = 0;

				return editor_result;
			unsave:
				if (ynDialog(LNG(Exit_Without_Saving)) != 1) {
					if (editor_result == TEXTEDITOR_RESULT_LAUNCH_ARGS) {
						LaunchArgsClear();
						editor_result = TEXTEDITOR_RESULT_EXIT;
					}
					goto abort;
				} else
					goto force;
			} else if (new_pad & PAD_R1) {
			menu:
				ret = editorMenu();
				if (ret == NEW) {
					Num_Window = 0;
					for (i = 0; i < 10; i++) {
						if (Window[i][OPENED])
							Num_Window++;
					}
					if (Num_Window < 10) {
						for (i = 0; i < 10; i++) {
							if (!Window[i][OPENED]) {
								Active_Window = i;
								break;
							}
						}
						ret = editorNew(Active_Window);
						if (!ret)
							goto fail;
						Editor_Cur = 0, Editor_PushRows = 0;
					}
					Num_Window = 0;
					for (i = 0; i < 10; i++) {
						if (Window[i][OPENED])
							Num_Window++;
					}
				} else if (ret == OPEN) {
					drawMsg(LNG(Select_A_File_For_Editing));
					Num_Window = 0;
					for (i = 0; i < 10; i++) {
						if (Window[i][OPENED])
							Num_Window++;
					}
					if (Num_Window < 10) {
						for (i = 0; i < 10; i++) {
							if (!Window[i][OPENED]) {
								Active_Window = i;
								break;
							}
						}
						ret = editorOpenFile(Active_Window);
						if (!ret)
							goto fail;
						Editor_Cur = 0, Editor_PushRows = 0;
					}
					Num_Window = 0;
					for (i = 0; i < 10; i++) {
						if (Window[i][OPENED])
							Num_Window++;
					}
				} else if (ret == CLOSE) {
					if (!Window[Active_Window][SAVED]) {
						if (ynDialog(LNG(Close_Without_Saving)) != 1)
							goto abort;
					}
					editorClose(Active_Window);
				fail:
					for (i = 9; i > -1; i--) {
						if (Window[i][OPENED]) {
							Active_Window = i;
							break;
						}
					}
					Num_Window = 0;
					for (i = 0; i < 10; i++) {
						if (Window[i][OPENED])
							Num_Window++;
					}
				abort:
					i = 0;  // just for compiler warning.
				} else if (ret == SAVE) {
					editorSave(Active_Window);
				} else if (ret == SAVE_AS) {
					editorSaveAs(Active_Window);
				} else if (ret == WINDOWS) {
					ret = editorSelectWindow();
					if (ret >= 0) {
						Active_Window = ret;
						editorResetState();
					}
				} else if (ret == LAUNCH_WITH_ARGS) {
					ret = LaunchArgsLoadFromBuffer(Path[Active_Window], TextBuffer[Active_Window], TextSize[Active_Window], tmp, sizeof(tmp));
					drawMsg(tmp);
					if (ret > 0) {
						editor_result = TEXTEDITOR_RESULT_LAUNCH_ARGS;
						goto exit;
					}
				} else if (ret == EXIT) {
					goto exit;
				}
			}
		}  //ends pad response section.

		if (!Num_Window)
			Editor_Start++;
		if (Editor_Start == 4) {
			Editor_Start = 0;
			event |= 2;
			goto menu;
		}

		if (setting->usbkbd_used) {  // Kbd response section.

			ret = editorKeyboardEntry();
			if (ret)
				event |= 2;
			if (ret == 2)
				goto menu;
			else if (ret == 3)
				goto exit;

		}  // end Kbd response section.

		t++;

		if (t & 0x0F)
			event |= 4;  //repetitive timer event.

		if (event || post_event) {  //NB: We need to update two frame buffers per event.

			//Display section.
			clrScr(setting->color[COLOR_BACKGR]);

			if (!Window[Active_Window][OPENED])
				goto end;

			drawOpSprite(COL_NORM_BG,
			             SCREEN_MARGIN, Frame_start_y,
			             SCREEN_WIDTH - SCREEN_MARGIN, Frame_end_y);

				if (KeyBoard_Active) {  //Display Virtual KeyBoard Section.

					drawPopSprite(setting->color[COLOR_BACKGR],
					              SCREEN_MARGIN, KEY_Y + 6,
				              SCREEN_WIDTH - SCREEN_MARGIN, Frame_end_y);
				drawOpSprite(setting->color[COLOR_FRAME],
				             SCREEN_MARGIN, KEY_Y + 6,
				             SCREEN_WIDTH - SCREEN_MARGIN, KEY_Y + 6 + LINE_THICKNESS - 1);
				drawOpSprite(setting->color[COLOR_FRAME],
				             KEY_X - 48, KEY_Y + 6,
				             KEY_X - 48 + LINE_THICKNESS - 1, Frame_end_y);
				drawOpSprite(setting->color[COLOR_FRAME],
				             KEY_X + 32, KEY_Y + 6,
				             KEY_X + 32 + LINE_THICKNESS - 1, Frame_end_y);
					drawOpSprite(setting->color[COLOR_FRAME],
					             KEY_X + KEY_W + 32, KEY_Y + 6,
					             KEY_X + KEY_W + 32 + LINE_THICKNESS - 1, Frame_end_y);

					if ((KeyBoard_Special && isEditorSpecialKeyboardKey(KeyBoard_Cur)) ||
					    (!KeyBoard_Special && isVirtualKeyboardEditorKey(setting->virtual_keyboard_layout, KeyBoard_Cur))) {
						if (KeyBoard_Cur % WFONTS == 0) {
							x0 = KEY_BOX1_LEFT;
							x1 = KEY_BOX1_RIGHT;
						} else if (KeyBoard_Cur == 1 || (KeyBoard_Cur - 1) % WFONTS == 0) {
							x0 = KEY_BOX2_LEFT;
							x1 = KEY_BOX2_RIGHT;
						} else if ((KeyBoard_Cur + 1) % WFONTS == 0) {
							x0 = KEY_BOX4_LEFT;
							x1 = KEY_BOX4_RIGHT;
						} else {
							layout_index = getVirtualKeyboardEditorLayoutIndex(KeyBoard_Cur);
							if (KeyBoard_Special) {
								x = KEY_SPECIAL_GRID_X + (layout_index % VKEY_LAYOUT_COLS) * KEY_CELL_W;
								key_char = getEditorSpecialKeyboardChar(KeyBoard_Cur);
							} else {
								x = KEY_GRID_X + (layout_index % VKEY_LAYOUT_COLS) * KEY_CELL_W;
								key_char = (layout_index >= 0) ? getVirtualKeyboardLayoutChar(setting->virtual_keyboard_layout, layout_index, KeyBoard_Caps) : 0;
							}
							j = (key_char == ' ') ? strlen(LNG(SPACE)) * FONT_WIDTH + 4 : FONT_WIDTH + 4;
							x0 = x - 2;
							x1 = x + j;
						}
						y = KEY_Y + 12 + (KeyBoard_Cur / WFONTS) * 18;
						drawOpSprite(setting->color[COLOR_SELECT], x0, y, x1, y + FONT_HEIGHT - 1);
					}

					if (Mark[MARK_ON])
						color = setting->color[COLOR_SELECT];
					else
						color = setting->color[COLOR_TEXT];
					editorKeyboardPrintLeft(LNG(MARK), KEY_BOX1_LEFT, KEY_BOX1_RIGHT, KEY_Y + 12,
					                        (KeyBoard_Cur == 0) ? setting->color[COLOR_BACKGR] : color);
					editorKeyboardPrintLeft(LNG(LINE_UP), KEY_BOX2_LEFT, KEY_BOX2_RIGHT, KEY_Y + 12,
					                        (KeyBoard_Cur == 1) ? setting->color[COLOR_BACKGR] : setting->color[COLOR_TEXT]);
				if (Mark[MARK_COPY])
					color = setting->color[COLOR_SELECT];
					else
						color = setting->color[COLOR_TEXT];
					editorKeyboardPrintLeft(LNG(COPY), KEY_BOX1_LEFT, KEY_BOX1_RIGHT, KEY_Y + 12 + FONT_HEIGHT + 2,
					                        (KeyBoard_Cur == WFONTS) ? setting->color[COLOR_BACKGR] : color);
					editorKeyboardPrintLeft(LNG(LINE_DOWN), KEY_BOX2_LEFT, KEY_BOX2_RIGHT, KEY_Y + 12 + FONT_HEIGHT + 2,
					                        (KeyBoard_Cur == WFONTS + 1) ? setting->color[COLOR_BACKGR] : setting->color[COLOR_TEXT]);
				if (Mark[MARK_CUT])
					color = setting->color[COLOR_SELECT];
					else
						color = setting->color[COLOR_TEXT];
					editorKeyboardPrintLeft(LNG(CUT), KEY_BOX1_LEFT, KEY_BOX1_RIGHT, KEY_Y + 12 + FONT_HEIGHT * 2 + 4,
					                        (KeyBoard_Cur == 2 * WFONTS) ? setting->color[COLOR_BACKGR] : color);
					editorKeyboardPrintLeft(LNG(PAGE_UP), KEY_BOX2_LEFT, KEY_BOX2_RIGHT, KEY_Y + 12 + FONT_HEIGHT * 2 + 4,
					                        (KeyBoard_Cur == 2 * WFONTS + 1) ? setting->color[COLOR_BACKGR] : setting->color[COLOR_TEXT]);
					editorKeyboardPrintLeft(LNG(PASTE), KEY_BOX1_LEFT, KEY_BOX1_RIGHT, KEY_Y + 12 + FONT_HEIGHT * 3 + 6,
					                        (KeyBoard_Cur == 3 * WFONTS) ? setting->color[COLOR_BACKGR] : setting->color[COLOR_TEXT]);
					editorKeyboardPrintLeft(LNG(PAGE_DOWN), KEY_BOX2_LEFT, KEY_BOX2_RIGHT, KEY_Y + 12 + FONT_HEIGHT * 3 + 6,
					                        (KeyBoard_Cur == 3 * WFONTS + 1) ? setting->color[COLOR_BACKGR] : setting->color[COLOR_TEXT]);
					editorKeyboardPrintLeft(LNG(HOME), KEY_BOX1_LEFT, KEY_BOX1_RIGHT, KEY_Y + 12 + FONT_HEIGHT * 4 + 8,
					                        (KeyBoard_Cur == 4 * WFONTS) ? setting->color[COLOR_BACKGR] : setting->color[COLOR_TEXT]);
					editorKeyboardPrintLeft(LNG(END), KEY_BOX2_LEFT, KEY_BOX2_RIGHT, KEY_Y + 12 + FONT_HEIGHT * 4 + 8,
					                        (KeyBoard_Cur == 4 * WFONTS + 1) ? setting->color[COLOR_BACKGR] : setting->color[COLOR_TEXT]);

				if (Editor_Insert)
					color = setting->color[COLOR_SELECT];
				else
					color = setting->color[COLOR_TEXT];
				editorKeyboardPrintLeft(LNG(INSERT), KEY_BOX4_LEFT, KEY_BOX4_RIGHT, KEY_Y + 12,
				                        (KeyBoard_Cur == WFONTS - 1) ? setting->color[COLOR_BACKGR] : color);
				tmp[0] = '\0';
				if (Editor_RetMode == OTHER)
					strcpy(tmp, LNG(RET_CRLF));
				else if (Editor_RetMode == UNIX)
					strcpy(tmp, LNG(RET_CR));
				else if (Editor_RetMode == MAC)
					strcpy(tmp, LNG(RET_LF));
				editorKeyboardPrintLeft(tmp, KEY_BOX4_LEFT, KEY_BOX4_RIGHT, KEY_Y + 12 + FONT_HEIGHT + 2,
				                        (KeyBoard_Cur == 2 * WFONTS - 1) ? setting->color[COLOR_BACKGR] : setting->color[COLOR_TEXT]);
				editorKeyboardPrintLeft(LNG(TAB), KEY_BOX4_LEFT, KEY_BOX4_RIGHT, KEY_Y + 12 + FONT_HEIGHT * 2 + 4,
				                        (KeyBoard_Cur == 3 * WFONTS - 1) ? setting->color[COLOR_BACKGR] : setting->color[COLOR_TEXT]);
				special_toggle_label = KeyBoard_Special ? getVirtualKeyboardLayoutDisplayName(setting->virtual_keyboard_layout) : LNG(SPECIAL);
				editorKeyboardPrintLeft(special_toggle_label, KEY_BOX4_LEFT, KEY_BOX4_RIGHT, KEY_Y + 12 + FONT_HEIGHT * 3 + 6,
				                        (KeyBoard_Cur == 4 * WFONTS - 1) ? setting->color[COLOR_BACKGR] : setting->color[COLOR_TEXT]);
				editorKeyboardPrintLeft(LNG(KB_RETURN), KEY_BOX4_LEFT, KEY_BOX4_RIGHT, KEY_Y + 12 + FONT_HEIGHT * 4 + 8,
				                        (KeyBoard_Cur == 5 * WFONTS - 1) ? setting->color[COLOR_BACKGR] : setting->color[COLOR_TEXT]);

					for (i = 0; i < KEY_LEN; i++) {
						layout_index = getVirtualKeyboardEditorLayoutIndex(i);
						if (KeyBoard_Special && layout_index >= 0 && isEditorSpecialKeyboardKey(i)) {
							x = KEY_SPECIAL_GRID_X + (layout_index % VKEY_LAYOUT_COLS) * KEY_CELL_W;
							y = KEY_Y + 12 + (i / WFONTS) * 18;
							drawChar(getEditorSpecialKeyboardChar(i), x, y,
							         (i == KeyBoard_Cur) ? setting->color[COLOR_BACKGR] : setting->color[COLOR_TEXT]);
						} else if (!KeyBoard_Special && layout_index >= 0 && isVirtualKeyboardEditorKey(setting->virtual_keyboard_layout, i)) {
							x = KEY_GRID_X + (layout_index % VKEY_LAYOUT_COLS) * KEY_CELL_W;
							y = KEY_Y + 12 + (i / WFONTS) * 18;
							key_char = getVirtualKeyboardLayoutChar(setting->virtual_keyboard_layout, layout_index, KeyBoard_Caps);
							key_label = (key_char == ' ') ? LNG(SPACE) : NULL;
							if (key_label != NULL)
								printXY(key_label, x, y, (i == KeyBoard_Cur) ? setting->color[COLOR_BACKGR] : setting->color[COLOR_TEXT], TRUE, 0);
							else
								drawChar(getVirtualKeyboardLayoutDisplayChar(setting->virtual_keyboard_layout, layout_index, KeyBoard_Caps), x, y,
								         (i == KeyBoard_Cur) ? setting->color[COLOR_BACKGR] : setting->color[COLOR_TEXT]);
						}
					}

				}  // end Display Virtual KeyBoard Section.

			x = Menu_start_x;
			y = Menu_start_y;

			editorApplyRules();

			Editor_TextEnd = 0, tmpLen = 0;

			for (i = Top_Height; i < Rows_Num + Top_Height; i++) {
				for (j = 0; j < Editor_nRowsWidth[i]; j++) {
					Mark[MARK_COLOR] = 0;

					if (Mark[MARK_ON] && Mark[MARK_PRINT] > 0) {  //Mark Text.
						if (Mark[MARK_SIZE] > 0) {
							if (Top_Width + tmpLen + j == (Editor_Cur - Mark[MARK_PRINT])) {
								drawOpSprite(COL_MARK_BG, x, y - 1, x + FONT_WIDTH, y + FONT_HEIGHT - 1);
								Mark[MARK_COLOR] = 1;
								Mark[MARK_PRINT]--;
							}
						} else if (Mark[MARK_SIZE] < 0) {
							if (Top_Width + tmpLen + j == (Editor_Cur + Mark[MARK_PRINT] - 1)) {
								drawOpSprite(COL_MARK_BG, x, y - 1, x + FONT_WIDTH, y + FONT_HEIGHT - 1);
								Mark[MARK_COLOR] = 1;
								if ((Mark[MARK_PRINT]++) == (-Mark[MARK_SIZE]))
									Mark[MARK_PRINT] = 0;
							}
						}
					}  // end mark.

					if (Top_Width + tmpLen + j == Editor_Cur) {  //Text Cursor.
						if (Editor_Insert)
							color = COL_CUR_INSERT;
						else
							color = COL_CUR_OVERWR;
						if (((event | post_event) & 4) && (t & 0x10))
							drawChar(TEXT_CUR, x - 4, y, color);
					}

					if (TextBuffer[Active_Window][Top_Width + tmpLen + j] == '\n') {  // Line Feed.
						ch = DN_ARROW;
						color = COL_LINE_END;
					} else if (TextBuffer[Active_Window][Top_Width + tmpLen + j] == '\r') {  // Carriage Return.
						ch = LT_ARROW;
						color = COL_LINE_END;
					} else if (TextBuffer[Active_Window][Top_Width + tmpLen + j] == '\t') {  // Tabulation.
						ch = RT_ARROW;
						color = COL_TAB;
					} else if (TextBuffer[Active_Window][Top_Width + tmpLen + j] == '\0') {  // Text End.
						ch = BR_SPLIT;
						color = COL_TEXT_END;
						Editor_TextEnd = 1;
					} else {
						ch = (unsigned char)TextBuffer[Active_Window][Top_Width + tmpLen + j];
						if (Mark[MARK_ON] && Mark[MARK_COLOR])  //Text Color Black / White If Mark.
							color = COL_MARK_TEXT;
						else
							color = COL_NORM_TEXT;
					}

					drawChar(ch, x, y, color);

					if (Editor_TextEnd)
						goto end;

					x += FONT_WIDTH;
				}

				tmpLen += Editor_nRowsWidth[i];

				x = Menu_start_x;
				y += FONT_HEIGHT;

			}  //ends for, so all editor Rows_Num were fixed above.
		end:
			if (Editor_nRowsNum > Rows_Num) {  //if more lines than available Rows_Num, use scrollbar.
				if (KeyBoard_Active) {
					drawFrame(SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 8, Frame_start_y,
					          SCREEN_WIDTH - SCREEN_MARGIN, KEY_Y + 6, setting->color[COLOR_FRAME]);
					y0 = (KEY_Y + 6 - Menu_start_y + 8) * ((double)Top_Height / Editor_nRowsNum);
					y1 = (KEY_Y + 6 - Menu_start_y + 8) * ((double)(Top_Height + Rows_Num) / Editor_nRowsNum);
					drawOpSprite(setting->color[COLOR_FRAME],
					             SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 6, (y0 + Menu_start_y - 2),
					             SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 2, (y1 + Menu_start_y - 10));
				} else {
					drawFrame(SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 8, Frame_start_y,
					          SCREEN_WIDTH - SCREEN_MARGIN, Frame_end_y, setting->color[COLOR_FRAME]);
					y0 = (Menu_end_y - Menu_start_y + 8) * ((double)Top_Height / Editor_nRowsNum);
					y1 = (Menu_end_y - Menu_start_y + 8) * ((double)(Top_Height + Rows_Num) / Editor_nRowsNum);
					drawOpSprite(setting->color[COLOR_FRAME],
					             SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 6, (y0 + Menu_start_y - 2),
					             SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 2, (y1 + Menu_start_y - 6));
				}  //ends clause for scrollbar with KeyBoard.
			}      //ends clause for scrollbar.

			//Tooltip section.
			tmp[0] = '\0', tmp1[0] = '\0', tmp2[0] = '\0';
			if (KeyBoard_Active) {  //Display Virtual KeyBoard Tooltip.
				if (swapKeys)
					sprintf(tmp1, "R1:%s \xFF"
					              "3:%s \xFF"
					              "1:%s \xFF"
					              "0:%s \xFF"
					              "2:%s L2:%s R2:%s Sel:%s",
					        LNG(Menu), LNG(Exit), LNG(Sel), LNG(BackSpace),
					        LNG(CAPS), LNG(Left), LNG(Right), LNG(Close_KB));
				else
					sprintf(tmp1, "R1:%s \xFF"
					              "3:%s \xFF"
					              "0:%s \xFF"
					              "1:%s \xFF"
					              "2:%s L2:%s R2:%s Sel:%s",
					        LNG(Menu), LNG(Exit), LNG(Sel), LNG(BackSpace),
					        LNG(CAPS), LNG(Left), LNG(Right), LNG(Close_KB));
			} else if (setting->usbkbd_used) {  //Display KeyBoard Tooltip.
				if (Window[Active_Window][OPENED]) {
					if (Mark[MARK_ON])
						sprintf(tmp1, "F1/R1:%s Esc/\xFF"
						              "3:%s Ctrl+ b:%s: %s ",
						        LNG(Menu), LNG(Exit), LNG(Mark), LNG(On));
					else
						sprintf(tmp1, "F1/R1:%s Esc/\xFF"
						              "3:%s Ctrl+ b:%s: %s ",
						        LNG(Menu), LNG(Exit), LNG(Mark), LNG(Off));
					sprintf(tmp2, "x:%s c:%s v:%s ", LNG(Cut), LNG(Copy), LNG(Paste));
					strcat(tmp1, tmp2);
					if (Editor_RetMode == OTHER)
						sprintf(tmp2, "r:%s ", LNG(CrLf));
					else if (Editor_RetMode == UNIX)
						sprintf(tmp2, "r:%s ", LNG(Cr));
					else if (Editor_RetMode == MAC)
						sprintf(tmp2, "r:%s ", LNG(Lf));
					strcat(tmp1, tmp2);
					if (Editor_Insert)
						sprintf(tmp2, "%s:%s", LNG(Ins), LNG(On));
					else
						sprintf(tmp2, "%s:%s", LNG(Ins), LNG(Off));
					strcat(tmp1, tmp2);
				} else
					sprintf(tmp1, "F1/R1:%s Esc/\xFF"
					              "3:%s",
					        LNG(Menu), LNG(Exit));
			} else {  //Display Basic Tooltip.
				if (Window[Active_Window][OPENED])
					sprintf(tmp1, "R1:%s \xFF"
					              "3:%s Select:%s",
					        LNG(Menu), LNG(Exit), LNG(Open_KeyBoard));
				else
					sprintf(tmp1, "R1:%s \xFF"
					              "3:%s",
					        LNG(Menu), LNG(Exit));
			}
			if (Window[Active_Window][CREATED])
				sprintf(tmp, "%s : %s", LNG(PS2_TEXT_EDITOR), LNG(File_Not_Yet_Saved));
			else if (Window[Active_Window][OPENED])
				sprintf(tmp, "%s : %s", LNG(PS2_TEXT_EDITOR), Path[Active_Window]);
			else
				strcpy(tmp, LNG(PS2_TEXT_EDITOR));
			setScrTmp(tmp, tmp1);
		}  //ends if(event||post_event).
		drawScr();
		post_event = event;
		event = 0;
	}  //ends while.

	return TEXTEDITOR_RESULT_EXIT;
}
//--------------------------------------------------------------
//End of file: editor.c
//--------------------------------------------------------------
