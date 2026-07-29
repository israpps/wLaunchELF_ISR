//---------------------------------------------------------------------------
// File name:   config_screen.c
//---------------------------------------------------------------------------
#include "config_private.h"

enum CONFIG_SCREEN {
	CONFIG_SCREEN_FIRST = 0,
	CONFIG_SCREEN_COL_FIRST = CONFIG_SCREEN_FIRST,
	CONFIG_SCREEN_COL_BACKGR_R = CONFIG_SCREEN_COL_FIRST,
	CONFIG_SCREEN_COL_BACKGR_G,
	CONFIG_SCREEN_COL_BACKGR_B,
	CONFIG_SCREEN_COL_FRAMES_R,
	CONFIG_SCREEN_COL_FRAMES_G,
	CONFIG_SCREEN_COL_FRAMES_B,
	CONFIG_SCREEN_COL_SELECT_R,
	CONFIG_SCREEN_COL_SELECT_G,
	CONFIG_SCREEN_COL_SELECT_B,
	CONFIG_SCREEN_COL_TEXT_R,
	CONFIG_SCREEN_COL_TEXT_G,
	CONFIG_SCREEN_COL_TEXT_B,
	CONFIG_SCREEN_COL_GRAPH1_R,
	CONFIG_SCREEN_COL_GRAPH1_G,
	CONFIG_SCREEN_COL_GRAPH1_B,
	CONFIG_SCREEN_COL_GRAPH2_R,
	CONFIG_SCREEN_COL_GRAPH2_G,
	CONFIG_SCREEN_COL_GRAPH2_B,
	CONFIG_SCREEN_COL_GRAPH3_R,
	CONFIG_SCREEN_COL_GRAPH3_G,
	CONFIG_SCREEN_COL_GRAPH3_B,
	CONFIG_SCREEN_COL_LAST,
	CONFIG_SCREEN_COL_GRAPH4_R = CONFIG_SCREEN_COL_LAST,
	CONFIG_SCREEN_COL_GRAPH4_G,
	CONFIG_SCREEN_COL_GRAPH4_B,

	//First option after colour selectors
	CONFIG_SCREEN_AFT_COLORS,
	CONFIG_SCREEN_TV_MODE = CONFIG_SCREEN_AFT_COLORS,
	CONFIG_SCREEN_TV_STARTX,
	CONFIG_SCREEN_TV_STARTY,

	CONFIG_SCREEN_MENU_FRAME,
	CONFIG_SCREEN_POPUP_OPAQUE,

	CONFIG_SCREEN_RETURN,
	CONFIG_SCREEN_DEFAULT,

	CONFIG_SCREEN_COUNT,
};

void Config_Screen(void)
{
	int i;
	int s, max_s = CONFIG_SCREEN_COUNT - 1;  //define cursor index and its max value
	int x, y;
	int len;
	int event, post_event = 0;
	u8 rgb[COLOR_COUNT][3];
	char c[MAX_PATH];
	char value_text[32];
	int bool_label_width;
	const char *tv_mode_value;
	int space = ((SCREEN_WIDTH - SCREEN_MARGIN - 4 * FONT_WIDTH) - (Menu_start_x + 2 * FONT_WIDTH)) / 8;

	event = 1;  //event = initial entry

	for (i = 0; i < COLOR_COUNT; i++) {
		rgb[i][0] = setting->color[i] & 0xFF;
		rgb[i][1] = setting->color[i] >> 8 & 0xFF;
		rgb[i][2] = setting->color[i] >> 16 & 0xFF;
	}

	s = CONFIG_SCREEN_FIRST;
	while (1) {
		//Pad response section
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_UP) {
				event |= 2;  //event |= valid pad command
				if (s == CONFIG_SCREEN_FIRST)
					s = max_s;
				else if (s == CONFIG_SCREEN_AFT_COLORS)
					s = CONFIG_SCREEN_COL_BACKGR_B;
				else
					s--;
			} else if (new_pad & PAD_DOWN) {
				event |= 2;  //event |= valid pad command
				if ((s < CONFIG_SCREEN_AFT_COLORS) && (s % 3 == 2))
					s = CONFIG_SCREEN_AFT_COLORS;
				else if (s == max_s)
					s = CONFIG_SCREEN_FIRST;
				else
					s++;
			} else if (new_pad & PAD_LEFT) {
				event |= 2;  //event |= valid pad command
				if (s >= CONFIG_SCREEN_RETURN)
					s = CONFIG_SCREEN_MENU_FRAME;
				else if (s >= CONFIG_SCREEN_MENU_FRAME)
					s = CONFIG_SCREEN_TV_MODE;
				else if (s >= CONFIG_SCREEN_TV_STARTX)
					s = CONFIG_SCREEN_TV_MODE;  //at or
				else if (s >= CONFIG_SCREEN_AFT_COLORS)
					s = CONFIG_SCREEN_COL_LAST;  //if s beyond color settings
				else if (s >= CONFIG_SCREEN_COL_FIRST + 3)
					s -= 3;  //if s in a color beyond the first colour, step to preceding color
			} else if (new_pad & PAD_RIGHT) {
				event |= 2;  //event |= valid pad command
				if (s >= CONFIG_SCREEN_MENU_FRAME)
					s = CONFIG_SCREEN_RETURN;
				else if (s >= CONFIG_SCREEN_TV_STARTX)
					s = CONFIG_SCREEN_MENU_FRAME;
				else if (s >= CONFIG_SCREEN_TV_MODE)
					s = CONFIG_SCREEN_TV_STARTX;
				else if (s >= CONFIG_SCREEN_COL_LAST)
					s = CONFIG_SCREEN_AFT_COLORS;  //if s in the last colour, move it to the first control after colour selection.
				else
					s += 3;
			} else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {  //User pressed CANCEL=>Subtract/Clear
				event |= 2;                                                                         //event |= valid pad command
				if (s < CONFIG_SCREEN_AFT_COLORS) {
					if (rgb[s / 3][s % 3] > 0) {
						rgb[s / 3][s % 3]--;
						setting->color[s / 3] =
						    GS_SETREG_RGBA(rgb[s / 3][0], rgb[s / 3][1], rgb[s / 3][2], 0);
					}
				} else if (s == CONFIG_SCREEN_TV_STARTX) {
					if (setting->screen_x > -gsGlobal->StartX) {
						setting->screen_x--;
						updateScreenMode();
					}
				} else if (s == CONFIG_SCREEN_TV_STARTY) {
					if (setting->screen_y > -gsGlobal->StartY) {
						setting->screen_y--;
						updateScreenMode();
					}
				}
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {  //User pressed OK=>Add/Ok/Edit
				event |= 2;                                                                         //event |= valid pad command
				if (s < CONFIG_SCREEN_AFT_COLORS) {
					if (rgb[s / 3][s % 3] < 255) {
						rgb[s / 3][s % 3]++;
						setting->color[s / 3] =
						    GS_SETREG_RGBA(rgb[s / 3][0], rgb[s / 3][1], rgb[s / 3][2], 0);
					}
				} else if (s == CONFIG_SCREEN_TV_MODE) {
					setting->TV_mode = (setting->TV_mode + 1) % TV_mode_COUNT;  //Change between the various modes
					updateScreenMode();
				} else if (s == CONFIG_SCREEN_TV_STARTX) {
					if (setting->screen_x < gsGlobal->StartX) {
						setting->screen_x++;
						updateScreenMode();
					}
				} else if (s == CONFIG_SCREEN_TV_STARTY) {
					if (setting->screen_y < gsGlobal->StartY) {
						setting->screen_y++;
						updateScreenMode();
					}
				} else if (s == CONFIG_SCREEN_MENU_FRAME) {
					setting->Menu_Frame = !setting->Menu_Frame;
				} else if (s == CONFIG_SCREEN_POPUP_OPAQUE) {
					setting->Popup_Opaque = !setting->Popup_Opaque;
				} else if (s == CONFIG_SCREEN_RETURN) {  //Always put 'RETURN' next to last
					return;
				} else if (s == CONFIG_SCREEN_DEFAULT) {  //Always put 'DEFAULT SCREEN SETTINGS' last
					setting->color[COLOR_BACKGR] = DEF_COLOR1;
					setting->color[COLOR_FRAME] = DEF_COLOR2;
					setting->color[COLOR_SELECT] = DEF_COLOR3;
					setting->color[COLOR_TEXT] = DEF_COLOR4;
					setting->color[COLOR_GRAPH1] = DEF_COLOR5;
					setting->color[COLOR_GRAPH2] = DEF_COLOR6;
					setting->color[COLOR_GRAPH3] = DEF_COLOR7;
					setting->color[COLOR_GRAPH4] = DEF_COLOR8;
					setting->TV_mode = TV_mode_AUTO;
					setting->screen_x = 0;
					setting->screen_y = 0;
					setting->Menu_Frame = DEF_MENU_FRAME;
					setting->Popup_Opaque = DEF_POPUP_OPAQUE;
					updateScreenMode();

					for (i = 0; i < COLOR_COUNT; i++) {
						rgb[i][0] = setting->color[i] & 0xFF;
						rgb[i][1] = setting->color[i] >> 8 & 0xFF;
						rgb[i][2] = setting->color[i] >> 16 & 0xFF;
					}
				}
			} else if (new_pad & PAD_TRIANGLE)
				return;
		}

		if (event || post_event) {  //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[COLOR_BACKGR]);

			x = Menu_start_x;

			for (i = 0; i < COLOR_COUNT; i++) {
				y = Menu_start_y;
				sprintf(c, "%s%d", LNG(Color), i + 1);
				printXY(c, x + (space * (i + 1)) - (printXY(c, 0, 0, 0, FALSE, space - FONT_WIDTH / 2) / 2), y,
				        setting->color[COLOR_TEXT], TRUE, space - FONT_WIDTH / 2);
				if (i == COLOR_BACKGR)
					sprintf(c, "%s", LNG(Backgr));
				else if (i == COLOR_FRAME)
					sprintf(c, "%s", LNG(Frames));
				else if (i == COLOR_SELECT)
					sprintf(c, "%s", LNG(Select));
				else if (i == COLOR_TEXT)
					sprintf(c, "%s", LNG(Text));
				else if (i == COLOR_GRAPH1)
					sprintf(c, "%s", LNG(Folders));
				else if (i == COLOR_GRAPH2)
					sprintf(c, "%s", LNG(ELFs));
				else if (i == COLOR_GRAPH3)
					sprintf(c, "%s", LNG(Unknown));
				else if (i == COLOR_GRAPH4)
					sprintf(c, "%s", LNG(TextEditor));
				printXY(c, x + (space * (i + 1)) - (printXY(c, 0, 0, 0, FALSE, space - FONT_WIDTH / 2) / 2), y + FONT_HEIGHT,
				        setting->color[COLOR_TEXT], TRUE, space - FONT_WIDTH / 2);
				y += FONT_HEIGHT * 2;
				printXY("R:", x, y, setting->color[COLOR_TEXT], TRUE, 0);
				sprintf(c, "%02X", rgb[i][0]);
				printXY(c, x + (space * (i + 1)) - FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				printXY("G:", x, y, setting->color[COLOR_TEXT], TRUE, 0);
				sprintf(c, "%02X", rgb[i][1]);
				printXY(c, x + (space * (i + 1)) - FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				printXY("B:", x, y, setting->color[COLOR_TEXT], TRUE, 0);
				sprintf(c, "%02X", rgb[i][2]);
				printXY(c, x + (space * (i + 1)) - FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				sprintf(c, "\xFF"
				           "4");
				printXY(c, x + (space * (i + 1)) - FONT_WIDTH, y, setting->color[i], TRUE, 0);
				}  //ends loop for colour RGB values
				y += FONT_HEIGHT * 2;
				bool_label_width = (int)strlen(LNG(TV_mode));
				if ((int)strlen(LNG(Screen_X_offset)) > bool_label_width)
					bool_label_width = (int)strlen(LNG(Screen_X_offset));
				if ((int)strlen(LNG(Screen_Y_offset)) > bool_label_width)
					bool_label_width = (int)strlen(LNG(Screen_Y_offset));
				if ((int)strlen(LNG(Menu_Frame)) > bool_label_width)
					bool_label_width = (int)strlen(LNG(Menu_Frame));
				if ((int)strlen(LNG(Popups_Opaque)) > bool_label_width)
					bool_label_width = (int)strlen(LNG(Popups_Opaque));
				if (setting->TV_mode == TV_mode_NTSC)
					tv_mode_value = "NTSC";
				else if (setting->TV_mode == TV_mode_PAL)
					tv_mode_value = "PAL";
				else if (setting->TV_mode == TV_mode_VGA)
					tv_mode_value = "VGA";
				else if (setting->TV_mode == TV_mode_480P)
					tv_mode_value = "Progressive";
				else
					tv_mode_value = "AUTO";
				configFormatLabelValueAligned(c, sizeof(c), LNG(TV_mode), tv_mode_value, bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				y += FONT_HEIGHT / 2;

			snprintf(value_text, sizeof(value_text), "%d", setting->screen_x);
			configFormatLabelValueAligned(c, sizeof(c), LNG(Screen_X_offset), value_text, bool_label_width);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			snprintf(value_text, sizeof(value_text), "%d", setting->screen_y);
			configFormatLabelValueAligned(c, sizeof(c), LNG(Screen_Y_offset), value_text, bool_label_width);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			configFormatLabelValueAligned(c, sizeof(c), LNG(Menu_Frame), setting->Menu_Frame ? LNG(ON) : LNG(OFF), bool_label_width);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			configFormatLabelValueAligned(c, sizeof(c), LNG(Popups_Opaque), setting->Popup_Opaque ? LNG(ON) : LNG(OFF), bool_label_width);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			sprintf(c, "  %s", LNG(RETURN));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			sprintf(c, "  %s", LNG(Use_Default_Screen_Settings));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			//Cursor positioning section
			x = Menu_start_x;
			y = Menu_start_y;

			if (s < CONFIG_SCREEN_AFT_COLORS) {  //if cursor indicates a colour component
				int colnum = s / 3;
				int comnum = s - colnum * 3;
				x += (space * (colnum + 1)) - (FONT_WIDTH * 4);
				y += (2 + comnum) * FONT_HEIGHT;
			} else {                                                                      //if cursor indicates anything after colour components
				y += (s - CONFIG_SCREEN_AFT_COLORS + 6) * FONT_HEIGHT + FONT_HEIGHT / 2;  //adjust y for cursor beyond colours
				//Here y is almost correct, except for additional group spacing
				if (s >= CONFIG_SCREEN_AFT_COLORS)  //if cursor at or beyond TV mode choice
					y += FONT_HEIGHT / 2;           //adjust for half-row space below colours
				if (s >= CONFIG_SCREEN_TV_STARTX)   //if cursor at or beyond screen offsets
					y += FONT_HEIGHT / 2;           //adjust for half-row space below TV mode choice
				if (s >= CONFIG_SCREEN_MENU_FRAME)  //if cursor at or beyond 'Menu Frame'
					y += FONT_HEIGHT / 2;           //adjust for half-row space below screen offsets
				if (s >= CONFIG_SCREEN_RETURN)      //if cursor at or beyond 'RETURN'
					y += FONT_HEIGHT / 2;           //adjust for half-row space below 'Popups Opaque'
			}
			drawChar(LEFT_CUR, x, y, setting->color[COLOR_TEXT]);  //draw cursor

			//Tooltip section
			if (s < CONFIG_SCREEN_AFT_COLORS || s == CONFIG_SCREEN_TV_STARTX || s == CONFIG_SCREEN_TV_STARTY) {  //if cursor at a colour component or a screen offset
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s \xFF"
					                 "0:%s",
					              LNG(Add), LNG(Subtract));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s \xFF"
					                 "1:%s",
					              LNG(Add), LNG(Subtract));
			} else if (s == CONFIG_SCREEN_TV_MODE || s == CONFIG_SCREEN_MENU_FRAME || s == CONFIG_SCREEN_POPUP_OPAQUE) {
				//if cursor at 'TV mode', 'Menu Frame' or 'Popups Opaque'
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s",
					              LNG(Change));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s",
					              LNG(Change));
			} else {  //if cursor at 'RETURN' or 'DEFAULT' options
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
			setScrTmp("", c);
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;

	}  //ends while
}  //ends Config_Screen
//---------------------------------------------------------------------------
// End of file: config_screen.c
//---------------------------------------------------------------------------
