//--------------------------------------------------------------
//File name:   draw.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "draw_private.h"
#include "main_title.h"

GSGLOBAL *gsGlobal;
GSTEXTURE TexIcon[2];
int SCREEN_WIDTH = 640;
int SCREEN_HEIGHT = 448;
//dlanor: values shown above are defaults for NTSC mode

int updateScr_1;      //dlanor: flags screen updates for drawScr()
int updateScr_2;      //dlanor: used for anti-flicker delay in drawScr()
u64 updateScr_t = 0;  //dlanor: exit time of last drawScr()

char LastMessage[MAX_TEXT_LINE + 2];
static int menu_title_refresh_frames = 0;

int Menu_start_x = SCREEN_MARGIN + LINE_THICKNESS + FONT_WIDTH;
int Menu_title_y = SCREEN_MARGIN;
int Menu_message_y = SCREEN_MARGIN + FONT_HEIGHT;
int Frame_start_y = SCREEN_MARGIN + 2 * FONT_HEIGHT + 2;  //First line of menu frame
int Menu_start_y = SCREEN_MARGIN + 2 * FONT_HEIGHT + LINE_THICKNESS + 5;
//dlanor: Menu_start_y is the 1st pixel line that may be used for main content of a menu
//dlanor: values below are only calculated when a rez is activated
int Menu_end_y;      //Normal menu display should not use pixels at this line or beyond
int Frame_end_y;     //first line of frame bottom
int Menu_tooltip_y;  //Menus may also use this row for tooltips


//--------------------------------------------------------------
static int getMenuTitleCharLimit(const char *version_text)
{
	int title_chars;

	title_chars = (SCREEN_WIDTH - (SCREEN_MARGIN * 2) - (FONT_WIDTH * ((int)strlen(version_text) + 1))) / FONT_WIDTH;
	if (title_chars < 0)
		title_chars = 0;
	if (title_chars > MAX_TEXT_LINE)
		title_chars = MAX_TEXT_LINE;

	return title_chars;
}
//--------------------------------------------------------------
static void drawMenuTitleLine(void)
{
	int x, y;
	char app_title[64];
	char status_title[MAX_TEXT_LINE + 1];

	if (setting == NULL)
		return;

	x = SCREEN_MARGIN;
	y = Menu_title_y;
	sprintf(app_title, "\xff\x34 wLaunchELF %s \xff\x34", ULE_VERSION);

	drawSprite(setting->color[COLOR_BACKGR], 0, Menu_title_y - 1, SCREEN_WIDTH, Menu_message_y - 1);
	printXY(app_title, x, y, setting->color[COLOR_FRAME], TRUE, 0);
	menuTitleFormat(status_title, getMenuTitleCharLimit(app_title) + 1);
	printXY(status_title, SCREEN_WIDTH - SCREEN_MARGIN - FONT_WIDTH * strlen(status_title), y,
	        setting->color[COLOR_TEXT], TRUE, 0);
}
//--------------------------------------------------------------
static void updateDynamicMenuTitleLine(void)
{
	if (menuTitleUpdateAsync(0))
		menu_title_refresh_frames = 2;

	if (menu_title_refresh_frames > 0) {
		drawMenuTitleLine();
		menu_title_refresh_frames--;
	}
}
//--------------------------------------------------------------
void setScrTmp(const char *msg0, const char *msg1)
{
	int x;

	x = SCREEN_MARGIN;
	drawMenuTitleLine();

	strncpy(LastMessage, msg0, MAX_TEXT_LINE);
	LastMessage[MAX_TEXT_LINE] = '\0';
	printXY(msg0, x, Menu_message_y, setting->color[COLOR_SELECT], TRUE, 0);

	if (setting->Menu_Frame)
		drawFrame(SCREEN_MARGIN, Frame_start_y,
		          SCREEN_WIDTH - SCREEN_MARGIN, Frame_end_y, setting->color[COLOR_FRAME]);

	printXY(msg1, x, Menu_tooltip_y, setting->color[COLOR_SELECT], TRUE, 0);
}
//--------------------------------------------------------------
void drawSprite(u64 color, int x1, int y1, int x2, int y2)
{
	gsKit_prim_sprite(gsGlobal, x1, y1, x2, y2, 0, color);
}
//--------------------------------------------------------------
void drawPopSprite(u64 color, int x1, int y1, int x2, int y2)
{
	gsKit_prim_sprite(gsGlobal, x1, y1, x2, y2, 0, color);
}
//--------------------------------------------------------------
//drawOpSprite exists only to eliminate the use of primitive sprite functions
//that are specific to the graphics lib used (currently gsKit). So normally
//it will merely be a 'wrapper' function for one of the lib calls, except
//that it will also perform any coordinate adjustments (if any)implemented for
//the functions drawSprite and drawPopSprite, to keep all of them compatible.
//
void drawOpSprite(u64 color, int x1, int y1, int x2, int y2)
{
	gsKit_prim_sprite(gsGlobal, x1, y1, x2, y2, 0, color);
}
//--------------------------------------------------------------
void drawMsg(const char *msg)
{
	strncpy(LastMessage, msg, MAX_TEXT_LINE);
	LastMessage[MAX_TEXT_LINE] = '\0';
	drawSprite(setting->color[COLOR_BACKGR], 0, Menu_message_y - 1,
	           SCREEN_WIDTH, Frame_start_y);
	printXY(msg, SCREEN_MARGIN, Menu_message_y, setting->color[COLOR_SELECT], TRUE, 0);
	drawScr();
}
//--------------------------------------------------------------
void drawLastMsg(void)
{
	drawSprite(setting->color[COLOR_BACKGR], 0, Menu_message_y - 1,
	           SCREEN_WIDTH, Frame_start_y);
	printXY(LastMessage, SCREEN_MARGIN, Menu_message_y, setting->color[COLOR_SELECT], TRUE, 0);
	drawScr();
}
//--------------------------------------------------------------


//--------------------------------------------------------------
void loadIcon(void)
{
	TexIcon[0].Width = 16;
	TexIcon[0].Height = 16;
	TexIcon[0].PSM = GS_PSM_CT32;
	TexIcon[0].Mem = (void *)icon_folder;
	gsGlobal->CurrentPointer = 0x280000;
	TexIcon[0].Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(TexIcon[0].Width, TexIcon[0].Height, TexIcon[0].PSM), GSKIT_ALLOC_USERBUFFER);
	TexIcon[0].Filter = GS_FILTER_LINEAR;
	gsKit_texture_upload(gsGlobal, &TexIcon[0]);
	free(TexIcon[0].Mem);

	TexIcon[1].Width = 16;
	TexIcon[1].Height = 16;
	TexIcon[1].PSM = GS_PSM_CT32;
	TexIcon[1].Mem = (void *)icon_warning;
	gsGlobal->CurrentPointer = 0x284000;
	TexIcon[1].Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(TexIcon[0].Width, TexIcon[0].Height, TexIcon[0].PSM), GSKIT_ALLOC_USERBUFFER);
	TexIcon[1].Filter = GS_FILTER_LINEAR;
	gsKit_texture_upload(gsGlobal, &TexIcon[1]);
	free(TexIcon[1].Mem);
}
//--------------------------------------------------------------
//------------------------------
//endfunc loadFont
//--------------------------------------------------------------
void clrScr(u64 color)
{
	gsKit_prim_sprite(gsGlobal, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, color);
}

//--------------------------------------------------------------
void drawScr(void)
{
	updateDynamicMenuTitleLine();

	if (updateScr_2) {  //Did we render anything last time
		while (Timer() < updateScr_t + 5)
			;  //if so, delay to complete rendering
	}
	gsKit_sync_flip(gsGlobal);   //Await sync and flip buffers
	gsKit_queue_exec(gsGlobal);  //Start rendering recent transfers for NEXT time
	updateScr_t = Timer();       //Note the time when the rendering started
	updateScr_2 = updateScr_1;   //Note if this rendering had expected updates
	updateScr_1 = 0;             //Note that we've nothing expected for next time
}  //NB: Apparently the GS keeps rendering while we continue with other work
//--------------------------------------------------------------
void drawFrame(int x1, int y1, int x2, int y2, u64 color)
{
	updateScr_1 = 1;

	//Top horizontal edge
	gsKit_prim_sprite(gsGlobal, x1, y1, x2, y1 + LINE_THICKNESS - 1, 1, color);

	//Bottom horizontal
	gsKit_prim_sprite(gsGlobal, x1, y2 - LINE_THICKNESS + 1, x2, y2, 1, color);

	//Left vertical edge
	gsKit_prim_sprite(gsGlobal, x1, y1, x1 + LINE_THICKNESS - 1, y2, 1, color);

	//Right vertical edge
	gsKit_prim_sprite(gsGlobal, x2 - LINE_THICKNESS + 1, y1, x2, y2, 1, color);
}

//--------------------------------------------------------------
// draw a char using the system font (16x16)
//------------------------------
//endfunc drawChar
//--------------------------------------------------------------
// draw a char using the ELISA font (16x16)
//------------------------------
//endfunc drawChar2
//--------------------------------------------------------------
// draw a string of characters, without shift-JIS support
//------------------------------
//endfunc printXY
//--------------------------------------------------------------
// draw a string of characters, with shift-JIS support (only for gamesave titles)
//------------------------------
//endfunc printXY_sjis
//--------------------------------------------------------------
//translate a string from shift-JIS to ascii (for gamesave titles)
//------------------------------
//endfunc transcpy_sjis
//--------------------------------------------------------------
//WriteFont_C is used to save the current font as C source code
//Comment it out if not used
/*
int	WriteFont_C(char *path_arg)
{
	u8  path[MAX_PATH];
	u8  text[80*2], char_info[80];
	u8	*p;
	int ret, tst, i, fd=-1;

	ret=-1; tst=genFixPath(path_arg, path);
	if(tst < 0) goto finish;
	ret=-2; tst=genOpen(path, FIO_O_CREAT | FIO_O_WRONLY | FIO_O_TRUNC);
	if(tst < 0) goto finish;
	fd = tst;
	sprintf(text, "unsigned char font_uLE[] = {\r\n");
	ret=-3; tst = genWrite(fd, text, strlen(text));
	if(tst != strlen(text)) goto finish;
	for(i=0x000; i<0x10A; i++){
		p = font_uLE + i*16;
		text[0] = '\0';
		if((i & 0x07) == 0)
			sprintf(text, "//Font position 0x%03X\r\n", i);
		if((i < 0x20) || (i>0x80 && i<0xA0))
			sprintf(char_info, "//char 0x%03X == '_' (free for use)", i);
		else if(i < 0x100)
			sprintf(char_info, "//char 0x%03X == '%c'", i, i);
		else //(i > 0x0FF)
			sprintf(char_info, "//char 0x%03X == special for uLE", i);
		sprintf(text+strlen(text),
			"	0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, %s\r\n"
			"	0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X"
			, p[0],p[1], p[2],p[3], p[4],p[5], p[6],p[7], char_info
			, p[8],p[9], p[10],p[11], p[12],p[13], p[14],p[15]
		);
		if(i<0x109) strcat(text, ",");
		strcat(text, "\r\n");
		ret=-4; tst = genWrite(fd, text, strlen(text));
		if(tst != strlen(text)) break;
		ret = 0;
	} //ends for
	if(ret == 0){
		sprintf(text,
			"//Font position 0x%03X\r\n"
			"}; //ends font_uLE\r\n", i);
		ret=-5; tst = genWrite(fd, text, strlen(text));
		if(tst == strlen(text)) ret = 0;
	}
finish:
	if(fd >= 0) genClose(fd);
	sprintf(text,"Saving %s => %d\nChoose either option to continue", path, ret);
	ynDialog(text);
	return ret;
}
*/
//------------------------------
//endfunc WriteFont_C
//--------------------------------------------------------------
//End of file: draw.c
//--------------------------------------------------------------
