//--------------------------------------------------------------
//File name:   editor_menu.c
//--------------------------------------------------------------
#include "editor_private.h"

int editorMenu(void)
{
	u64 color;
	char enable[NUM_MENU], tmp[64];
	int x, y, i, Menu_Sel;
	int event, post_event = 0;

	int menu_len = strlen(LNG(New)) > strlen(LNG(Open)) ?
	                   strlen(LNG(New)) :
	                   strlen(LNG(Open));
	menu_len = strlen(LNG(Close)) > menu_len ? strlen(LNG(Close)) : menu_len;
	menu_len = strlen(LNG(Save)) > menu_len ? strlen(LNG(Save)) : menu_len;
	menu_len = strlen(LNG(Save_As)) > menu_len ? strlen(LNG(Save_As)) : menu_len;
	menu_len = strlen(LNG(Windows)) > menu_len ? strlen(LNG(Windows)) : menu_len;
	menu_len = strlen(LNG(Launch_With_Args)) > menu_len ? strlen(LNG(Launch_With_Args)) : menu_len;
	menu_len = strlen(LNG(Exit)) > menu_len ? strlen(LNG(Exit)) : menu_len;

	int menu_ch_w = menu_len + 1;                                 //Total characters in longest menu string.
	int menu_ch_h = NUM_MENU;                                     //Total number of menu lines.
	int mSprite_Y1 = 64;                                          //Top edge of sprite.
	int mSprite_X2 = SCREEN_WIDTH - 35;                           //Right edge of sprite.
	int mSprite_X1 = mSprite_X2 - (menu_ch_w + 3) * FONT_WIDTH;   //Left edge of sprite
	int mSprite_Y2 = mSprite_Y1 + (menu_ch_h + 1) * FONT_HEIGHT;  //Bottom edge of sprite

	memset(enable, TRUE, NUM_MENU);

	if (!Window[Active_Window][OPENED]) {
		enable[CLOSE] = FALSE;
		enable[SAVE] = FALSE;
		enable[SAVE_AS] = FALSE;
	}
	if (Window[Active_Window][CREATED]) {
		enable[SAVE] = FALSE;
	}
	if (!Num_Window)
		enable[WINDOWS] = FALSE;
	if (!Window[Active_Window][OPENED] || Window[Active_Window][CREATED] || Path[Active_Window][0] == '\0')
		enable[LAUNCH_WITH_ARGS] = FALSE;

	for (Menu_Sel = 0; Menu_Sel < NUM_MENU; Menu_Sel++)
		if (enable[Menu_Sel] == TRUE)
			break;

	event = 1;  //event = initial entry.
	while (1) {
		//Pad response section.
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_UP && Menu_Sel < NUM_MENU) {
				event |= 2;  //event |= valid pad command.
				do {
					Menu_Sel--;
					if (Menu_Sel < 0)
						Menu_Sel = NUM_MENU - 1;
				} while (!enable[Menu_Sel]);
			} else if (new_pad & PAD_DOWN && Menu_Sel < NUM_MENU) {
				event |= 2;  //event |= valid pad command.
				do {
					Menu_Sel++;
					if (Menu_Sel == NUM_MENU)
						Menu_Sel = 0;
				} while (!enable[Menu_Sel]);
			} else if ((new_pad & PAD_TRIANGLE) || (!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				return -1;
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
				event |= 2;  //event |= valid pad command.
				break;
			}
		}

		if (event || post_event) {  //NB: We need to update two frame buffers per event.

			//Display section.
			drawPopSprite(setting->color[COLOR_BACKGR],
			              mSprite_X1, mSprite_Y1,
			              mSprite_X2, mSprite_Y2);
			drawFrame(mSprite_X1, mSprite_Y1, mSprite_X2, mSprite_Y2, setting->color[COLOR_FRAME]);

			for (i = 0, y = mSprite_Y1 + FONT_HEIGHT / 2; i < NUM_MENU; i++) {
				if (i == NEW)
					strcpy(tmp, LNG(New));
				else if (i == OPEN)
					strcpy(tmp, LNG(Open));
				else if (i == CLOSE)
					strcpy(tmp, LNG(Close));
				else if (i == SAVE)
					strcpy(tmp, LNG(Save));
				else if (i == SAVE_AS)
					strcpy(tmp, LNG(Save_As));
				else if (i == WINDOWS)
					strcpy(tmp, LNG(Windows));
				else if (i == LAUNCH_WITH_ARGS)
					strcpy(tmp, LNG(Launch_With_Args));
				else if (i == EXIT)
					strcpy(tmp, LNG(Exit));

				if (enable[i])
					color = setting->color[COLOR_TEXT];
				else
					color = setting->color[COLOR_FRAME];

				printXY(tmp, mSprite_X1 + 2 * FONT_WIDTH, y, color, TRUE, 0);
				y += FONT_HEIGHT;
			}
			if (Menu_Sel < NUM_MENU)
				drawChar(LEFT_CUR, mSprite_X1 + FONT_WIDTH, mSprite_Y1 + (FONT_HEIGHT / 2 + Menu_Sel * FONT_HEIGHT), setting->color[COLOR_TEXT]);

			//Tooltip section.
			x = SCREEN_MARGIN;
			y = Menu_tooltip_y;
			drawSprite(setting->color[COLOR_BACKGR],
			           0, y - 1,
			           SCREEN_WIDTH, y + 16);
			if (swapKeys)
				sprintf(tmp, "\xFF"
				             "1:%s \xFF"
				             "0:%s \xFF"
				             "3:%s",
				        LNG(OK), LNG(Cancel), LNG(Back));
			else
				sprintf(tmp, "\xFF"
				             "0:%s \xFF"
				             "1:%s \xFF"
				             "3:%s",
				        LNG(OK), LNG(Cancel), LNG(Back));
			printXY(tmp, x, y, setting->color[COLOR_SELECT], TRUE, 0);
		}  //ends if(event||post_event).
		drawScr();
		post_event = event;
		event = 0;
	}  //ends while.
	return Menu_Sel;
}
int editorSelectWindow(void)
{
	u64 color;
	int x, y, i, Window_Sel = Active_Window;
	int event, post_event = 0;

	int Window_ch_w = 36;                                               //Total characters in longest Window Name.
	int Window_ch_h = 10;                                               //Total number of Window Menu lines.
	int wSprite_Y1 = 200;                                               //Top edge of sprite.
	int wSprite_X2 = SCREEN_WIDTH - 35;                                 //Right edge of sprite.
	int wSprite_X1 = wSprite_X2 - (Window_ch_w + 3) * FONT_WIDTH - 3;   //Left edge of sprite.
	int wSprite_Y2 = wSprite_Y1 + (Window_ch_h + 1) * FONT_HEIGHT + 3;  //Bottom edge of sprite.

	char tmp[64];

	event = 1;  //event = initial entry.
	while (1) {
		//Pad response section.
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_UP) {
				event |= 2;  //event |= valid pad command.
				if ((Window_Sel--) <= 0)
					Window_Sel = 9;
			} else if (new_pad & PAD_DOWN) {
				event |= 2;  //event |= valid pad command.
				if ((Window_Sel++) >= 9)
					Window_Sel = 0;
			} else if ((new_pad & PAD_TRIANGLE) || (!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				return -1;
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
				event |= 2;  //event |= valid pad command.
				break;
			}
		}

		if (event || post_event) {  //NB: We need to update two frame buffers per event.

			//Display section.
			drawPopSprite(setting->color[COLOR_BACKGR],
			              wSprite_X1, wSprite_Y1,
			              wSprite_X2, wSprite_Y2);
			drawFrame(wSprite_X1, wSprite_Y1, wSprite_X2, wSprite_Y2, setting->color[COLOR_FRAME]);

			for (i = 0, y = wSprite_Y1 + FONT_HEIGHT / 2; i < 10; i++) {
				if (Window_Sel == i)
					color = setting->color[COLOR_SELECT];
				else
					color = setting->color[COLOR_TEXT];

				if (!Window[i][OPENED])
					printXY(LNG(Free_Window), wSprite_X1 + 2 * FONT_WIDTH, y, color, TRUE, 0);
				else if (Window[i][CREATED])
					printXY(LNG(Window_Not_Yet_Saved), wSprite_X1 + 2 * FONT_WIDTH, y, color, TRUE, 0);
				else if (Window[i][OPENED])
					printXY(Path[i], wSprite_X1 + 2 * FONT_WIDTH, y, color, TRUE, 0);

				y += FONT_HEIGHT;
			}

			if (Window_Sel <= 10)
				drawChar(LEFT_CUR, wSprite_X1 + FONT_WIDTH, wSprite_Y1 + (FONT_HEIGHT / 2 + Window_Sel * FONT_HEIGHT), setting->color[COLOR_TEXT]);

			//Tooltip section.
			x = SCREEN_MARGIN;
			y = Menu_tooltip_y;
			drawSprite(setting->color[COLOR_BACKGR],
			           0, y - 1,
			           SCREEN_WIDTH, y + 16);
			if (swapKeys)
				sprintf(tmp, "\xFF"
				             "1:%s \xFF"
				             "0:%s \xFF"
				             "3:%s",
				        LNG(OK), LNG(Cancel), LNG(Back));
			else
				sprintf(tmp, "\xFF"
				             "0:%s \xFF"
				             "1:%s \xFF"
				             "3:%s",
				        LNG(OK), LNG(Cancel), LNG(Back));
			printXY(tmp, x, y, setting->color[COLOR_SELECT], TRUE, 0);
		}  //ends if(event||post_event).
		drawScr();
		post_event = event;
		event = 0;
	}  //ends while.
	return Window_Sel;
}  //--------------------------------------------------------------
//End of file: editor_menu.c
//--------------------------------------------------------------
