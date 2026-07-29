//--------------------------------------------------------------
//File name:   main_menu.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "main_menu.h"

//------------------------------
static void setLaunchKeys(void)
{
	if (!setting->LK_Flag[SETTING_LK_SELECT])
		strcpy(setting->LK_Path[SETTING_LK_SELECT], setting->Misc_Configure);
}
//------------------------------
//endfunc setLaunchKeys
//---------------------------------------------------------------------------
void MainMenuState_Init(MainMenuState *state)
{
	if (state == NULL)
		return;

	state->selected = 0;
	state->timeout = 0;
	state->prev_timeout = 1;
	state->init_delay = 0;
	state->prev_init_delay = 1;
	state->mode = MAIN_MENU_MODE_BUTTON;
	state->user_acted = 0;
	state->init_delay_start = 0;
	state->timeout_start = 0;
	memset(state->menu_lk, 0, sizeof(state->menu_lk));
}

void MainMenuState_BeginTimers(MainMenuState *state)
{
	if (state == NULL)
		return;

	state->init_delay = setting->Init_Delay * 1000;
	state->init_delay_start = Timer();
	state->timeout = (setting->timeout + 1) * 1000;
	state->timeout_start = Timer();
}

void MainMenuState_UpdateTimers(MainMenuState *state, int *event)
{
	if (state == NULL || event == NULL)
		return;

	if (state->init_delay) {
		state->prev_init_delay = state->init_delay;
		CurrTime = Timer();
		if (CurrTime > (state->init_delay_start + state->init_delay)) {
			state->init_delay = 0;
			state->timeout_start = CurrTime;
		} else {
			state->init_delay = state->init_delay_start + state->init_delay - CurrTime;
			state->init_delay_start = CurrTime;
		}
		if ((state->init_delay / 1000) != (state->prev_init_delay / 1000))
			*event |= 8;  //event |= visible delay change
	} else if (state->timeout && !state->user_acted) {
		state->prev_timeout = state->timeout;
		CurrTime = Timer();
		if (CurrTime > (state->timeout_start + state->timeout)) {
			state->timeout = 0;
		} else {
			state->timeout = state->timeout_start + state->timeout - CurrTime;
			state->timeout_start = CurrTime;
		}
		if ((state->timeout / 1000) != (state->prev_timeout / 1000))
			*event |= 8;  //event |= visible timeout change
	}

}

int drawMainMenuScreen(MainMenuState *state, const char *main_msg)
{
	int nElfs = 0;
	int i;
	int x, y;
	u64 color;
	char *p;
	char c[MAX_PATH + 8], f[MAX_PATH];

	if (state == NULL)
		return 0;

	setLaunchKeys();

	clrScr(setting->color[COLOR_BACKGR]);

	x = Menu_start_x;
	y = Menu_start_y;
	c[0] = 0;
	if (state->init_delay)
		sprintf(c, "%s: %d", LNG(Init_Delay), state->init_delay / 1000);
	else if (setting->LK_Path[SETTING_LK_AUTO][0]) {
		if (!state->user_acted)
			sprintf(c, "%s: %d", LNG(TIMEOUT), state->timeout / 1000);
		else
			sprintf(c, "%s: %s", LNG(TIMEOUT), LNG(Halted));
	}
	if (c[0]) {
		printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
		y += FONT_HEIGHT * 2;
	}
	for (i = 0; i < SETTING_LK_BTN_COUNT; i++) {
		if ((setting->LK_Path[i][0]) && ((i <= SETTING_LK_SELECT) || setting->LK_Flag[i])) {
			state->menu_lk[nElfs] = i;  //memorize RunElf index for this menu entry
			switch (i) {
				case SETTING_LK_AUTO:
					strcpy(c, "Default: ");
					break;
				case SETTING_LK_CIRCLE:
					strcpy(c, "     \xFF"
					          "0: ");
					break;
				case SETTING_LK_CROSS:
					strcpy(c, "     \xFF"
					          "1: ");
					break;
				case SETTING_LK_SQUARE:
					strcpy(c, "     \xFF"
					          "2: ");
					break;
				case SETTING_LK_TRIANGLE:
					strcpy(c, "     \xFF"
					          "3: ");
					break;
				case SETTING_LK_L1:
					strcpy(c, "     L1: ");
					break;
				case SETTING_LK_R1:
					strcpy(c, "     R1: ");
					break;
				case SETTING_LK_L2:
					strcpy(c, "     L2: ");
					break;
				case SETTING_LK_R2:
					strcpy(c, "     R2: ");
					break;
				case SETTING_LK_L3:
					strcpy(c, "     L3: ");
					break;
				case SETTING_LK_R3:
					strcpy(c, "     R3: ");
					break;
				case SETTING_LK_START:
					strcpy(c, "  START: ");
					break;
				case SETTING_LK_SELECT:
					strcpy(c, " SELECT: ");
					break;
				case SETTING_LK_LEFT:
					sprintf(c, "%s: ", LNG(LEFT));
					break;
				case SETTING_LK_RIGHT:
					sprintf(c, "%s: ", LNG(RIGHT));
					break;
			}                          //ends switch
			if (setting->Show_Titles)  //Show Launch Titles ?
				strcpy(f, setting->LK_Title[i]);
			else
				f[0] = '\0';
			if (!f[0]) {                                          //No title present, or allowed ?
				if (setting->Hide_Paths) {                        //Hide full path ?
					if ((p = strrchr(setting->LK_Path[i], '/')))  // found delimiter ?
						strcpy(f, p + 1);
					else  // No delimiter !
						strcpy(f, setting->LK_Path[i]);
					if ((p = strrchr(f, '.')))
						*p = 0;
				} else {  //Show full path !
					strcpy(f, setting->LK_Path[i]);
				}
			}  //ends clause for No title
			if (nElfs++ == state->selected && state->mode == MAIN_MENU_MODE_DPAD)
				color = setting->color[COLOR_SELECT];
			else
				color = setting->color[COLOR_TEXT];
			{
				int len = (strlen(LNG(LEFT)) + 2 > strlen(LNG(RIGHT)) + 2) ?
				              strlen(LNG(LEFT)) + 2 :
				              strlen(LNG(RIGHT)) + 2;
				if (i == SETTING_LK_LEFT) {  // LEFT
					if (strlen(LNG(RIGHT)) + 2 > strlen(LNG(LEFT)) + 2)
						printXY(c, x + (strlen(LNG(RIGHT)) + 2 > 9 ? ((strlen(LNG(RIGHT)) + 2) - (strlen(LNG(LEFT)) + 2)) * FONT_WIDTH : (9 - (strlen(LNG(LEFT)) + 2)) * FONT_WIDTH),
						        y, color, TRUE, 0);
					else
						printXY(c, x + (strlen(LNG(LEFT)) + 2 > 9 ? 0 : (9 - (strlen(LNG(LEFT)) + 2)) * FONT_WIDTH),
						        y, color, TRUE, 0);
				} else if (i == SETTING_LK_RIGHT) {  // RIGHT
					if (strlen(LNG(LEFT)) + 2 > strlen(LNG(RIGHT)) + 2)
						printXY(c, x + (strlen(LNG(LEFT)) + 2 > 9 ? ((strlen(LNG(LEFT)) + 2) - (strlen(LNG(RIGHT)) + 2)) * FONT_WIDTH : (9 - (strlen(LNG(RIGHT)) + 2)) * FONT_WIDTH),
						        y, color, TRUE, 0);
					else
						printXY(c, x + (strlen(LNG(RIGHT)) + 2 > 9 ? 0 : (9 - (strlen(LNG(RIGHT)) + 2)) * FONT_WIDTH),
						        y, color, TRUE, 0);
				} else
					printXY(c, x + (len > 9 ? (len - 9) * FONT_WIDTH : 0), y, color, TRUE, 0);
				printXY(f, x + (len > 9 ? len * FONT_WIDTH : 9 * FONT_WIDTH), y, color, TRUE, 0);
			}
			y += FONT_HEIGHT;
		}  //ends clause for defined LK_Path[i] valid for menu
	}      //ends for

	if (state->mode == MAIN_MENU_MODE_BUTTON)
		sprintf(c, "%s!", LNG(PUSH_ANY_BUTTON_or_DPAD));
	else {
		if (swapKeys)
			sprintf(c, "\xFF"
			           "1:%s \xFF"
			           "0:%s",
			        LNG(OK), LNG(Cancel));
		else
			sprintf(c, "\xFF"
			           "0:%s \xFF"
			           "1:%s",
			        LNG(OK), LNG(Cancel));
	}

	setScrTmp(main_msg, c);

	return nElfs;
}
//------------------------------
//endfunc drawMainMenuScreen
//---------------------------------------------------------------------------
void MainMenuHandlePad(MainMenuState *state, int nElfs, char *runPath, size_t runPath_len, int *event)
{
	int RunELF_index;

	if (state == NULL || runPath == NULL || runPath_len == 0 || event == NULL)
		return;

	if (!state->init_delay && (waitAnyPadReady(), readpad())) {
		if (new_pad)
			*event |= 2;  //event |= pad command

		RunELF_index = -1;
		switch (state->mode) {
			case MAIN_MENU_MODE_BUTTON:
				if ((new_pad & PAD_SELECT) && (new_pad & PAD_START)) {
					initConfig();
					updateScreenMode();
				} else if (new_pad & PAD_CIRCLE)
					RunELF_index = SETTING_LK_CIRCLE;
				else if (new_pad & PAD_CROSS)
					RunELF_index = SETTING_LK_CROSS;
				else if (new_pad & PAD_SQUARE)
					RunELF_index = SETTING_LK_SQUARE;
				else if (new_pad & PAD_TRIANGLE)
					RunELF_index = SETTING_LK_TRIANGLE;
				else if (new_pad & PAD_L1)
					RunELF_index = SETTING_LK_L1;
				else if (new_pad & PAD_R1)
					RunELF_index = SETTING_LK_R1;
				else if (new_pad & PAD_L2)
					RunELF_index = SETTING_LK_L2;
				else if (new_pad & PAD_R2)
					RunELF_index = SETTING_LK_R2;
				else if (new_pad & PAD_L3)
					RunELF_index = SETTING_LK_L3;
				else if (new_pad & PAD_R3)
					RunELF_index = SETTING_LK_R3;
				else if (new_pad & PAD_START)
					RunELF_index = SETTING_LK_START;
				else if (new_pad & PAD_SELECT)
					RunELF_index = SETTING_LK_SELECT;
				else if ((new_pad & PAD_LEFT) && setting->LK_Flag[SETTING_LK_LEFT])
					RunELF_index = SETTING_LK_LEFT;
				else if ((new_pad & PAD_RIGHT) && setting->LK_Flag[SETTING_LK_RIGHT])
					RunELF_index = SETTING_LK_RIGHT;
				else if (new_pad & PAD_UP || new_pad & PAD_DOWN) {
					state->user_acted = 1;
					state->selected = 0;
					state->mode = MAIN_MENU_MODE_DPAD;
				}
				if (RunELF_index >= 0 && setting->LK_Path[RunELF_index][0])
					snprintf(runPath, runPath_len, "%s", setting->LK_Path[RunELF_index]);
				break;

			case MAIN_MENU_MODE_DPAD:
				if (new_pad & PAD_UP) {
					state->selected--;
					if (state->selected < 0)
						state->selected = nElfs - 1;
				} else if (new_pad & PAD_DOWN) {
					state->selected++;
					if (state->selected >= nElfs)
						state->selected = 0;
				} else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
					state->mode = MAIN_MENU_MODE_BUTTON;
				} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
					if (setting->LK_Path[state->menu_lk[state->selected]][0])
						snprintf(runPath, runPath_len, "%s", setting->LK_Path[state->menu_lk[state->selected]]);
				}
				break;
		}  //ends switch(mode)
	}
}

void MainMenuHandleAutoLaunch(MainMenuState *state, char *runPath, size_t runPath_len, int *event)
{
	if (state == NULL || runPath == NULL || runPath_len == 0 || event == NULL)
		return;

	if (!state->user_acted && ((state->timeout / 1000) == 0) && setting->LK_Path[SETTING_LK_AUTO][0] && state->mode == MAIN_MENU_MODE_BUTTON) {
		*event |= 8;  //event |= visible timeout change
		snprintf(runPath, runPath_len, "%s", setting->LK_Path[SETTING_LK_AUTO]);
	}
}

void MainMenuMarkActionTaken(MainMenuState *state)
{
	if (state == NULL)
		return;

	state->user_acted = 1;
	state->mode = MAIN_MENU_MODE_BUTTON;
}
