//--------------------------------------------------------------
//File name:   main_info_screens.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "main_console_info.h"
#include "main_startup.h"
#include "main_info_screens.h"

//Function to print a text row to the 'gs' screen
//------------------------------
static int PrintRow(int row_f, char *text_p)
{
	static int row;
	int x = (Menu_start_x + 4);
	int y;

	if (row_f >= 0)
		row = row_f;
	y = (Menu_start_y + FONT_HEIGHT * row++);
	printXY(text_p, x, y, setting->color[COLOR_TEXT], TRUE, 0);
	return row;
}
//------------------------------
//endfunc PrintRow
//---------------------------------------------------------------------------
//Function to print a text row with text positioning
//------------------------------
static int PrintPos(int row_f, int column, char *text_p, int COLORID)
{
	static int row;
	int x = (Menu_start_x + 4 + column * FONT_WIDTH);
	int y;

	if (row_f >= 0)
		row = row_f;
	y = (Menu_start_y + FONT_HEIGHT * row++);
	printXY(text_p, x, y, setting->color[COLORID], TRUE, 0);
	return row;
}
//------------------------------
//endfunc PrintPos
//---------------------------------------------------------------------------
//Function to show a screen with program credits ("About uLE")
//------------------------------
void Show_About_uLE(void)
{
	char TextRow[256];
	int event, post_event = 0;
	int hpos = 12;

	event = 1;  //event = initial entry
	//----- Start of event loop -----
	while (1) {
		//Pad response section
		waitAnyPadReady();
		if (readpad() && new_pad) {
			event |= 2;
			break;
		}

		//Display section
		if (event || post_event) {  //NB: We need to update two frame buffers per event
			clrScr(setting->color[COLOR_BACKGR]);
			sprintf(TextRow, "About wLaunchELF %s  %s", ULE_VERSION, ULE_VERDATE);
			PrintPos(01, hpos, TextRow, COLOR_SELECT);
			sprintf(TextRow, " commit: %s", GIT_HASH);
			PrintPos(02, hpos, TextRow, COLOR_TEXT);
			PrintPos(03, hpos, "Mod by: R3Z3N & Codex 5.3 LLM", COLOR_TEXT);
			PrintPos(-1, hpos, "DS3/DS4 support by Alex Parrado", COLOR_TEXT);
			PrintPos(-1, hpos, "No thanks to: TnA Plastic of PS2 Scene and PSX-Place", COLOR_TEXT);
			PrintPos(-1, hpos, "Project maintainers:  sp193 & AKuHAK", COLOR_TEXT);
			PrintPos(-1, hpos, "  ", COLOR_TEXT);
			PrintPos(-1, hpos, "uLaunchELF Project maintainers:", COLOR_TEXT);
			PrintPos(-1, hpos, "  Eric Price       (aka: 'E P')", COLOR_TEXT);
			PrintPos(-1, hpos, "  Ronald Andersson (aka: 'dlanor')", COLOR_TEXT);
			PrintPos(-1, hpos, " ", COLOR_TEXT);
			PrintPos(-1, hpos, "Other contributors:", COLOR_TEXT);
			PrintPos(-1, hpos, "  Polo35, radad, Drakonite, sincro", COLOR_TEXT);
			PrintPos(-1, hpos, "  kthu, Slam-Tilt, chip, pixel, Hermes, PCM720", COLOR_TEXT);
			PrintPos(-1, hpos, "  and others in the PS2Dev community", COLOR_TEXT);
			PrintPos(-1, hpos, " ", COLOR_TEXT);
			PrintPos(-1, hpos, "Main release site:", COLOR_TEXT);
			PrintPos(-1, hpos, "   github.com/ps2homebrew/wLaunchELF/releases", COLOR_TEXT);
			PrintPos(-1, hpos, "Mod Release site:", COLOR_SELECT);
			PrintPos(-1, hpos, "   github.com/saildot4k/wLaunchELF_R3Z/releases", COLOR_TEXT);
			PrintPos(-1, hpos, "Ancestral project: LaunchELF v3.41 by Mirakichi", COLOR_TEXT);
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	}  //ends while
	   //----- End of event loop -----
}

void Show_build_info(void)
{
	char TextRow[256];
	int event, post_event = 0;
	int hpos = 16;

	event = 1;  //event = initial entry
	//----- Start of event loop -----
	while (1) {
		//Pad response section
		waitAnyPadReady();
		if (readpad() && new_pad) {
			event |= 2;
			break;
		}

		//Display section
		if (event || post_event) {  //NB: We need to update two frame buffers per event
			clrScr(setting->color[COLOR_BACKGR]);
			sprintf(TextRow, " wLaunchELF %s (%s)", ULE_VERSION, GIT_HASH);
			PrintPos(03, hpos, TextRow, COLOR_TEXT);
			PrintPos(-1, hpos, "Build features:", COLOR_SELECT);

			PrintPos(-1, hpos,
#ifdef ETH
			         " ETH=1"
#else
			         " ETH=0"
#endif
#ifdef UDPFS
			         " UDPFS=1"
#else
			         " UDPFS=0"
#endif
			         , COLOR_TEXT);
			PrintPos(-1, hpos,
#ifdef XFROM
			         " XFROM=1"
#else
			         " XFROM=0"
#endif
#ifdef DVRP
			         " DVRP_HDD=1"
#else
			         " DVRP_HDD=0"
#endif
			         , COLOR_TEXT);

			PrintPos(-1, hpos,
#ifdef EXFAT
			         " EXFAT=1"
#else
			         " EXFAT=0"
#endif
#ifdef DS34
			         " DS34=1"
#else
			         " DS34=0"
#endif
			         , COLOR_TEXT);
			PrintPos(-1, hpos,
#ifdef MX4SIO
			         " MX4SIO=1"
#else
			         " MX4SIO=0"
#endif
#ifdef MMCE
			         " MMCE=1"
#else
			         " MMCE=0"
#endif
			         , COLOR_TEXT);
#if defined(UDPTTY) || defined(SIO_DEBUG) || defined(POWERPC_UART) || defined(NO_IOP_RESET)


			PrintPos(-1, hpos, "Debug Features:", COLOR_SELECT);
			PrintPos(-1, hpos,
#ifdef NO_IOP_RESET
			         " IOP_RESET=0"
#else
			         " IOP_RESET=1"
#endif
#ifdef UDPTTY
			         " UDPTTY=1"
#else
			         " UDPTTY=0"
#endif
			         , COLOR_TEXT);
			PrintPos(-1, hpos,
#ifdef SIO_DEBUG
			         " SIO_DEBUG=1"
#else
			         " SIO_DEBUG=0"
#endif
#ifdef POWERPC_UART
			         " PPC_UART=1"
#else
			         " PPC_UART=0"
#endif
			         , COLOR_TEXT);
#endif
			PrintPos(-1, hpos, "Mod Release site:", COLOR_TEXT);
			PrintPos(-1, hpos, "   github.com/saildot4k/wLaunchELF_R3Z/releases", COLOR_TEXT);
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	}  //ends while
	   //----- End of event loop -----
}

//------------------------------
//Function to show a screen with debugging info
//------------------------------
void ShowDebugInfo(int boot_argc, char *boot_argv[], const char *boot_path, const char *default_osdsys_path2, char rough_region, char *romver_data)
{
	char TextRow[256];
	char console_model[CONSOLE_MODEL_NAME_MAX_LEN + 1];
	unsigned short romVersion;
	int i, event, post_event = 0;

	console_model[0] = '\0';
	event = 1;  //event = initial entry
	//----- Start of event loop -----
	while (1) {
		//Pad response section
		waitAnyPadReady();
		if (readpad() && new_pad) {
			event |= 2;
			break;
		}

		//Display section
		if (event || post_event) {  //NB: We need to update two frame buffers per event
			clrScr(setting->color[COLOR_BACKGR]);
			PrintRow(0, "Debug Info Screen:");
			if (romver_data[0] == '\0')
				uLE_InitializeRegion();
			if (console_model[0] == '\0')
				GetConsoleModelName(romver_data, console_model, sizeof(console_model));
			if (romver_data[0] == '\0')
				snprintf(TextRow, sizeof(TextRow), "rom0:ROMVER == \"<unavailable>\"");
			else
				snprintf(TextRow, sizeof(TextRow), "rom0:ROMVER == \"%s\"", romver_data);
			PrintRow(2, TextRow);
			snprintf(TextRow, sizeof(TextRow), "Console model == \"%s\"", console_model);
			PrintRow(-1, TextRow);
			sprintf(TextRow, "argc == %d", boot_argc);
			PrintRow(5, TextRow);
			for (i = 0; (i < boot_argc) && (i < 8); i++) {
				sprintf(TextRow, "argv[%d] == \"%s\"", i, boot_argv[i]);
				PrintRow(-1, TextRow);
			}
			sprintf(TextRow, "Main System Update KELF == \"%s\"", (console_is_PSX) ? "BIEXEC-SYSTEM/xosdmain.elf" : (strchr(default_osdsys_path2, '/') + 1));
			PrintRow(-1, TextRow);
			romVersion = getROMVersion();
			if ((romVersion < 0x230) && (romVersion > 0x130)) {
				sprintf(TextRow, "Specific System Update KELF == \"B%cEXEC-SYSTEM/osd%03x.elf\"", rough_region, (romVersion + 10) & ~0x0F);
				PrintRow(-1, TextRow);
			}
			{
				int max_path = (int)sizeof(TextRow) - (int)strlen("boot_path == \"\"") - 1;
				if (max_path < 0)
					max_path = 0;
				snprintf(TextRow, sizeof(TextRow), "boot_path == \"%.*s\"", max_path, boot_path);
			}
			PrintRow(-1, TextRow);
			{
				int max_path = (int)sizeof(TextRow) - (int)strlen("LaunchElfDir == \"\"") - 1;
				if (max_path < 0)
					max_path = 0;
				snprintf(TextRow, sizeof(TextRow), "LaunchElfDir == \"%.*s\"", max_path, LaunchElfDir);
			}
			PrintRow(-1, TextRow);
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	}  //ends while
	   //----- End of event loop -----
}
//------------------------------
//endfunc ShowDebugInfo
//---------------------------------------------------------------------------
void ShowFont(void)
{
	int test_type = 0;
	int test_types = 2;  //Patch test_types for number of test loops
	int i, j, event, post_event = 0;
	char Hex[18] = "0123456789ABCDEF";
	int ch_x_stp = 1 + FONT_WIDTH + 1 + LINE_THICKNESS;
	int ch_y_stp = 2 + FONT_HEIGHT + 1 + LINE_THICKNESS;
	int mat_w = LINE_THICKNESS + 17 * ch_x_stp;
	int mat_h = LINE_THICKNESS + 17 * ch_y_stp;
	int mat_x = (((SCREEN_WIDTH - mat_w) / 2) & -2);
	int mat_y = (((SCREEN_HEIGHT - mat_h) / 2) & -2);
	int ch_x = mat_x + LINE_THICKNESS + 1;
	//	int	ch_y  = mat_y+LINE_THICKNESS+2;
	int px, ly, cy;
	u64 col_0 = setting->color[COLOR_BACKGR], col_1 = setting->color[COLOR_FRAME], col_3 = setting->color[COLOR_TEXT];

	//The next line is a patch to save font, if/when needed (needs patch in draw.c too)
	//	WriteFont_C("mc0:/SYS-CONF/font_uLE.c");

	event = 1;  //event = initial entry
	//----- Start of event loop -----
	while (1) {
		//Display section
		if (event || post_event) {  //NB: We need to update two frame buffers per event
			drawOpSprite(col_0, mat_x, mat_y, mat_x + mat_w - 1, mat_y + mat_h - 1);
			//Here the background rectangle has been prepared
			/* //Start of commented out section //Move this line as needed for tests
			//Start of gsKit test section
			if(test_type > 1) goto done_test;
			gsKit_prim_point(gsGlobal, mat_x+16, mat_y+16, 1, col_3);
			gsKit_prim_point(gsGlobal, mat_x+33, mat_y+16, 1, col_3);
			gsKit_prim_point(gsGlobal, mat_x+33, mat_y+33, 1, col_3);
			gsKit_prim_point(gsGlobal, mat_x+16, mat_y+33, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+48, mat_y+48, mat_x+65, mat_y+48, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+65, mat_y+48, mat_x+65, mat_y+65, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+65, mat_y+65, mat_x+48, mat_y+65, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+48, mat_y+65, mat_x+48, mat_y+48, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+80, mat_y+80, mat_x+97, mat_y+81, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+97, mat_y+80, mat_x+96, mat_y+97, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+97, mat_y+97, mat_x+80, mat_y+96, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+80, mat_y+97, mat_x+81, mat_y+80, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+80, mat_y+16, mat_x+81, mat_y+16, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+97, mat_y+16, mat_x+97, mat_y+17, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+97, mat_y+33, mat_x+96, mat_y+33, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+80, mat_y+33, mat_x+80, mat_y+32, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+16, mat_y+80, mat_x+17, mat_y+81, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+33, mat_y+80, mat_x+32, mat_y+81, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+33, mat_y+97, mat_x+32, mat_y+96, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+16, mat_y+97, mat_x+17, mat_y+96, 1, col_3);
			goto end_display;
done_test:
			//End of gsKit test section
*/  //End of commented out section  //Move this line as needed for tests
			//Start of font display section
			//Now we start to draw all vertical frame lines
			px = mat_x;
			drawOpSprite(col_1, px, mat_y, px + LINE_THICKNESS - 1, mat_y + mat_h - 1);
			for (j = 0; j < 17; j++) {  //for each font column, plus the row_index column
				px += ch_x_stp;
				drawOpSprite(col_1, px, mat_y, px + LINE_THICKNESS - 1, mat_y + mat_h - 1);
			}  //ends for each font column, plus the row_index column
			//Here all the vertical frame lines have been drawn
			//Next we draw the top horizontal line
			drawOpSprite(col_1, mat_x, mat_y, mat_x + mat_w - 1, mat_y + LINE_THICKNESS - 1);
			cy = mat_y + LINE_THICKNESS + 2;
			ly = mat_y;
			for (i = 0; i < 17; i++) {  //for each font row
				px = ch_x;
				if (!i) {                                 //if top row (which holds the column indexes)
					drawChar('\\', px, cy, col_3);        //Display '\' at index crosspoint
				} else {                                  //else a real font row
					drawChar(Hex[i - 1], px, cy, col_3);  //Display row index
				}
				for (j = 0; j < 16; j++) {  //for each font column
					px += ch_x_stp;
					if (!i) {                             //if top row (which holds the column indexes)
						drawChar(Hex[j], px, cy, col_3);  //Display Column index
					} else {
						drawChar((i - 1) * 16 + j, px, cy, col_3);  //Display font character
					}
				}  //ends for each font column
				ly += ch_y_stp;
				drawOpSprite(col_1, mat_x, ly, mat_x + mat_w - 1, ly + LINE_THICKNESS - 1);
				cy += ch_y_stp;
			}  //ends for each font row
			   //End of font display section
		}      //ends if(event||post_event)
		       //end_display:
		drawScr();
		post_event = event;
		event = 0;

		//Pad response section
		waitAnyPadReady();
		if (readpad() && new_pad) {
			event |= 2;
			if ((++test_type) < test_types) {
				mat_y++;
				continue;
			}
			break;
		}
	}  //ends while
	   //----- End of event loop -----
}
//------------------------------
//endfunc ShowFont
//---------------------------------------------------------------------------

