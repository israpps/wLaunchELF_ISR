//--------------------------------------------------------------
//File name:   main_actions.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "init.h"
#include "main_actions.h"
#include "main_exploit_installer.h"
#include "main_gameid.h"
#include "main_history.h"
#include "main_info_screens.h"

//CleanUpForExec releases uLE stuff preparatory to launching some other application
//------------------------------
void CleanUpForExec(void)
{
	clrScr(GS_SETREG_RGBA(0x00, 0x00, 0x00, 0));
	drawScr();
	clrScr(GS_SETREG_RGBA(0x00, 0x00, 0x00, 0));
	drawScr();
	free(setting);
	free(elisaFnt);
	free(External_Lang_Buffer);
	padPortClose(1, 0);
	padPortClose(0, 0);
	//    padEnd();  //Required when a newer libpad library is used.
	closeKeyboardIfOpened();
#ifdef DS34
	WaitSema(semRunning);
	isRunning = 0;
	SignalSema(semRunning);
	WaitSema(semFinish);
	ds34usb_reset();
	ds34bt_reset();
#endif
}
//------------------------------
//endfunc CleanUpForExec
//---------------------------------------------------------------------------
static int isHddLaunchPath(const char *path)
{
	return (!strncmp(path, "hdd", 3) && path[3] >= '0' && path[3] <= '9' && path[4] == ':' && path[5] == '/');
}

#ifdef XFROM
static int isMbrLaunchPath(const char *path)
{
	return (path != NULL &&
	        (!stricmp(path, "xfrom:/BIEXEC-SYSTEM/xosdmain") ||
	         !stricmp(path, "xfrom:/BIEXEC-SYSTEM/xosdmain.elf") ||
	         !stricmp(path, "hdd0:__system:pfs:/BIEXEC-SYSTEM/xosdmain.elf")));
}
#endif

static const char *getLaunchArgsSidecarBasePath(const char *path, const char *fullpath)
{
	if (path != NULL && !strncmp(path, "mc:/", 4) && fullpath != NULL && fullpath[0] != '\0')
		return fullpath;
	return path;
}

static void getTextEditorReturnPath(char *out)
{
	getFilePath(out, FALSE);
	LastDir[0] = '\0';
}

static void executeBrowserSelection(char *selected_path, const MainExecuteContext *ctx)
{
	int editor_result;

	while (selected_path[0]) {
		if (IsTextEditorFileType(selected_path)) {
			editor_result = TextEditor(selected_path);
			selected_path[0] = 0;
			if (editor_result == TEXTEDITOR_RESULT_LAUNCH_ARGS) {
				getFilePath(selected_path, FALSE);
				if (!selected_path[0])
					LaunchArgsClear();
				continue;
			}
			LaunchArgsClear();
			if (editor_result == TEXTEDITOR_RESULT_BROWSE_DIR) {
				getTextEditorReturnPath(selected_path);
				continue;
			}
		} else {
			if (LaunchArgsPending() &&
			    !genCmpFileExt(selected_path, "ELF") &&
			    !genCmpFileExt(selected_path, "KELF") &&
			    !genCmpFileExt(selected_path, "XLF"))
				LaunchArgsClear();
			ExecuteMainAction(selected_path, ctx);
		}
		break;
	}
}

// Execute. Execute an action. May be called recursively.
// For any path specified, its device must be accessible.
//------------------------------
void ExecuteMainAction(char *pathin, const MainExecuteContext *ctx)
{
	char tmp[MAX_PATH];
	static char path[MAX_PATH];
	static char fullpath[MAX_PATH];
	static char party[MAX_PATH];
	char *p;
	int x, t = 0;
	char dvdpl_path[] = "mc0:/BREXEC-DVDPLAYER/dvdplayer.elf";
	int dvdpl_update;
#ifdef XFROM
	char mbr_mem_arg[32];
#endif

	if (ctx == NULL || pathin == NULL || pathin[0] == 0)
		return;

	if (!uLE_related(path, pathin))  //1==uLE_rel 0==missing, -1==other dev
		return;

	if (LaunchArgsPending() &&
	    !genCmpFileExt(path, "ELF") &&
	    !genCmpFileExt(path, "KELF") &&
	    !genCmpFileExt(path, "XLF"))
		LaunchArgsClear();

Recurse_for_ESR:  //Recurse here for PS2Disc command with ESR disc

	if (IsPopstarterVcdPath(path)) {
		LaunchPopstarterVcd(path, ctx->main_msg, MAX_PATH);
		return;
	}

	if (!strncmp(path, "mc", 2)) {
		party[0] = 0;
		if (path[2] != ':')
			goto CheckELF_path;
		strcpy(fullpath, "mc0:");
		strcat(fullpath, path + 3);
		if ((t = checkELFheader(fullpath)) > 0)
			goto ELFchecked;
		fullpath[2] = '1';
		goto CheckELF_fullpath;

	} else if (!strncmp(path, "vmc", 3)) {
		x = path[3] - '0';
		if ((x < 0) || (x > 1) || !vmcMounted[x])
			goto ELFnotFound;
		goto CheckELF_path;
	}
#ifdef XFROM
	else if (isMbrLaunchPath(path)) {
		snprintf(fullpath, sizeof(fullpath), "%s", path);
		t = 0;
		if (!console_is_PSX)
			goto ELFnotFound;
		if (PrepareMbrLaunchPayload(path, mbr_mem_arg, sizeof(mbr_mem_arg)) < 0)
			goto ELFnotFound;

		x = setting->reboot_iop_elf_load;
		CleanUpForExec();
		LaunchArgsClear();
		RunLoaderMemory("rom0:HDDBOOT", mbr_mem_arg, x);
		return;
	}
#endif
	else if (isHddLaunchPath(path)) {
		loadHddModules();
		if ((t = checkELFheader(path)) <= 0)
			goto ELFnotFound;
		//coming here means the ELF is fine
		snprintf(party, sizeof(party), "hdd%c:%s", path[3], path + 6);
		p = strchr(party, '/');
		snprintf(fullpath, sizeof(fullpath), "pfs0:%s", p);
		*p = 0;
		goto ELFchecked;
	} else if (!strncmp(path, "dvr_hdd0:/", 10)) {
#ifdef DVRP
		if (!console_is_PSX)
			goto ELFnotFound;
		loadDVRPHddModules();
		if ((t = checkELFheader(path)) <= 0)
			goto ELFnotFound;
		//coming here means the ELF is fine
		snprintf(party, sizeof(party), "dvr_hdd0:%s", path + 10);
		p = strchr(party, '/');
		if (p != NULL) {
			snprintf(fullpath, sizeof(fullpath), "dvr_pfs0:%s", p);
			*p = 0;
			fullpath[7] = (getDVRPPartyMountIndex(party) == 1) ? '1' : '0';
		} else {
			snprintf(fullpath, sizeof(fullpath), "dvr_pfs%d:/", (getDVRPPartyMountIndex(party) == 1) ? 1 : 0);
		}
		goto ELFchecked;
#else
		goto ELFnotFound;
#endif
	} else if (!strncmp(path, "xfrom", 5)) {
#ifdef XFROM
		if (!console_is_PSX)
			goto ELFnotFound;
		if (!loadFlashModules())
			goto ELFnotFound;
		if ((t = checkELFheader(path)) <= 0)
			goto ELFnotFound;
		strcpy(fullpath, path);
		goto ELFchecked;
#else
		goto ELFnotFound;
#endif
	} else if (!strncmp(path, "mx4sio", 6)) {
#ifdef MX4SIO
		if (!mx4sio_driver_running && !loadMx4sioModules())
			goto ELFnotFound;
		if ((t = checkELFheader(path)) <= 0)
			goto ELFnotFound;
		party[0] = 0;
		strcpy(fullpath, path);
		goto ELFchecked;
#else
		goto ELFnotFound;
#endif
	} else if (!strncmp(path, "mmce", 4)) {
#ifdef MMCE
		loadMmceModules();
		if ((t = checkELFheader(path)) <= 0)
			goto ELFnotFound;
		strcpy(fullpath, path);
		goto ELFchecked;
#else
		goto ELFnotFound;
#endif
	} else if (!strncmp(path, "usb", 3)) {
		char *pathSep;

		if ((t = checkELFheader(path)) <= 0)
			goto ELFnotFound;
		party[0] = 0;
		if (genFixPath(path, fullpath) < 0)
			goto ELFnotFound;
		pathSep = strchr(fullpath, '/');
		if (pathSep && (pathSep - fullpath < 7) && pathSep[-1] == ':')
			strcpy(fullpath + (pathSep - fullpath), pathSep + 1);
		goto ELFchecked;
	} else if (!strncmp(path, "ata", 3)) {
		char *pathSep;

#ifdef EXFAT
		loadAtaModules();
#endif
		if ((t = checkELFheader(path)) <= 0)
			goto ELFnotFound;
		party[0] = 0;
		if (!strncmp(path, "ata:", 4)) {
			size_t tail_len;

			tail_len = strlen(path + 4);
			if (tail_len >= sizeof(fullpath) - 5)
				goto ELFnotFound;
			memcpy(fullpath, "ata0:", 5);
			memcpy(fullpath + 5, path + 4, tail_len + 1);
		} else
			strcpy(fullpath, path);
		pathSep = strchr(fullpath, '/');
		if (pathSep && (pathSep - fullpath < 7) && pathSep[-1] == ':')
			strcpy(fullpath + (pathSep - fullpath), pathSep + 1);
		goto ELFchecked;
	} else if (!strncmp(path, "mass", 4)) {
		char *pathSep;

		if ((t = checkELFheader(path)) <= 0)
			goto ELFnotFound;
		party[0] = 0;

		strcpy(fullpath, path);
		pathSep = strchr(path, '/');
		if (pathSep && (pathSep - path < 7) && pathSep[-1] == ':')
			strcpy(fullpath + (pathSep - path), pathSep + 1);
		goto ELFchecked;
	} else if (!strncmp(path, "host:", 5)) {
#ifdef ETH
		initHOST();
		party[0] = 0;
		strcpy(fullpath, "host:");
		if (path[5] == '/')
			strcat(fullpath, path + 6);
		else
			strcat(fullpath, path + 5);
		makeHostPath(fullpath, fullpath);
		goto CheckELF_fullpath;
#else
		goto ELFnotFound;
#endif
	} else if (!strncmp(path, "udpfs:", 6)) {
#ifdef UDPFS
		load_udpfs();
		party[0] = 0;
		snprintf(fullpath, sizeof(fullpath), "%s", path);
		goto CheckELF_fullpath;
#else
		goto ELFnotFound;
#endif
	} else if (!stricmp(path, setting->Misc_OSDSYS)) {
		char arg0[20], arg1[20], arg2[20], arg3[40];
		char *args[4] = {arg0, arg1, arg2, arg3};
		char kelf_loader[40];
		int fd, argc;

		if (setting->LK_Flag[SETTING_LK_OSDSYS] && setting->LK_Path[SETTING_LK_OSDSYS][0])
			strcpy(path, setting->LK_Path[SETTING_LK_OSDSYS]);
		else
			strcpy(path, ctx->default_osdsys_path);

		fd = genOpen(path, FIO_O_RDONLY);
		if (fd >= 0)
			goto close_fd_and_launch_OSDSYS;
		if (strncmp(path, "mc:", 3) != 0)
			goto ELFnotFound;
		strcpy(fullpath, path);
		path[2] = '0';
		strcpy(path + 3, fullpath + 2);
		fd = genOpen(path, FIO_O_RDONLY);
		if (fd >= 0)
			goto close_fd_and_launch_OSDSYS;
		path[2] = '1';
		fd = genOpen(path, FIO_O_RDONLY);
		if (fd >= 0)
			goto close_fd_and_launch_OSDSYS;
		if (fd < 0)
			goto ELFnotFound;
	close_fd_and_launch_OSDSYS:
		genClose(fd);
		strcpy(arg0, "-m rom0:SIO2MAN");
		strcpy(arg1, "-m rom0:MCMAN");
		strcpy(arg2, "-m rom0:MCSERV");
		sprintf(arg3, "-x %s", path);
		argc = 4;
		strcpy(kelf_loader, "moduleload");
		CleanUpForExec();
		LoadExecPS2(kelf_loader, argc, args);

	} else if (!stricmp(path, setting->Misc_PS2Disc)) {
		drawMsg(LNG(Reading_SYSTEMCNF));
		party[0] = 0;
		readSystemCnf();
		if (BootDiscType == 2) {  //Boot a PS2 disc
			strcpy(fullpath, SystemCnf_BOOT2);
			goto CheckELF_fullpath;
		}
		if (BootDiscType == 1) {  //Boot a PS1 disc
			char disc_gameid[12];
			int have_disc_gameid;
			char *args[2] = {SystemCnf_BOOT, SystemCnf_VER};

			have_disc_gameid = buildLaunchGameID(SystemCnf_BOOT, disc_gameid, sizeof(disc_gameid));
			if (have_disc_gameid) {
				updateOSDHistoryFile(disc_gameid);
				applyXPARAM(disc_gameid);
			}

			CleanUpForExec();
			if (have_disc_gameid && !setting->cdrom_disable_gameid)
				displayRetroGemGameID(disc_gameid, 2);
			LoadExecPS2("rom0:PS1DRV", 2, args);
			sprintf(ctx->main_msg, "PS1DRV %s", LNG(Failed));
			goto Done_PS2Disc;
		}
		if (uLE_cdDiscValid()) {
			if (cdmode == SCECdDVDV) {
				x = Check_ESR_Disc();
				DPRINTF("Check_ESR_Disc => %d\n", x);
				if (x > 0) {  //ESR Disc, so launch ESR
					if (setting->LK_Flag[SETTING_LK_ESR] && setting->LK_Path[SETTING_LK_ESR][0])
						strcpy(path, setting->LK_Path[SETTING_LK_ESR]);
					else
						strcpy(path, ctx->default_esr_path);

					goto Recurse_for_ESR;
				}

				//DVD Video Disc, so launch DVD player
				char arg0[20], arg1[20], arg2[20], arg3[40];
				char *args[4] = {arg0, arg1, arg2, arg3};
				char kelf_loader[40];
				char MG_region[10];
				int i, pos, tst, argc;

				if ((tst = SifLoadModule("rom0:ADDDRV", 0, NULL)) < 0)
					goto Fail_DVD_Video;

				strcpy(arg0, "-k rom1:EROMDRVA");
				strcpy(arg1, "-m erom0:UDFIO");
				strcpy(arg2, "-x erom0:DVDPLA");
				argc = 3;
				strcpy(kelf_loader, "moduleload2 rom1:UDNL rom1:DVDCNF");

				strcpy(MG_region, "ACEJMORU");
				pos = strlen(arg0) - 1;
				for (i = 0; i < 9; i++) {  //NB: MG_region[8] is a string terminator
					arg0[pos] = MG_region[i];
					tst = SifLoadModuleEncrypted(arg0 + 3, 0, NULL);
					if (tst >= 0)
						break;
				}

				pos = strlen(arg2);
				if (i == 8)
					strcpy(&arg2[pos - 3], "ELF");
				else
					arg2[pos - 1] = MG_region[i];
				//At this point all args are ready to use internal DVD player

				//We must check for an updated player on MC
				dvdpl_path[6] = ctx->rough_region;
				dvdpl_update = 0;
				for (i = 0; i < 2; i++) {
					dvdpl_path[2] = '0' + i;
					if (wleExists(dvdpl_path)) {
						dvdpl_update = 1;
						break;
					}
				}

				if ((tst < 0) && (dvdpl_update == 0))
					goto Fail_PS2Disc;  //We must abort if no working kelf found

				if (dvdpl_update) {  // Launch DVD player from memory card
					strcpy(arg0, "-m rom0:SIO2MAN");
					strcpy(arg1, "-m rom0:MCMAN");
					strcpy(arg2, "-m rom0:MCSERV");
					sprintf(arg3, "-x %s", dvdpl_path);  // -x :elf is encrypted for mc
					argc = 4;
					strcpy(kelf_loader, "moduleload");
				}

				CleanUpForExec();
				LoadExecPS2(kelf_loader, argc, args);

			Fail_DVD_Video:
				sprintf(ctx->main_msg, "DVD-Video %s", LNG(Failed));
				goto Done_PS2Disc;
			}
			if (cdmode == SCECdCDDA) {
				//Fail_CDDA:
				sprintf(ctx->main_msg, "CDDA %s", LNG(Failed));
				goto Done_PS2Disc;
			}
		}
	Fail_PS2Disc:
		sprintf(ctx->main_msg, "%s => %s CDVD 0x%02X", LNG(PS2Disc), LNG(Failed), cdmode);
	Done_PS2Disc:
		x = x;
	} else if (!stricmp(path, setting->Misc_FileBrowser)) {
		ctx->main_msg[0] = 0;
		tmp[0] = 0;
		LastDir[0] = 0;
		getFilePath(tmp, FALSE);
		executeBrowserSelection(tmp, ctx);
		return;
	} else if (!stricmp(path, setting->Misc_PS2Browser)) {
		char *args[1] = {"BootBrowser"};

		CleanUpForExec();
		ExecOSD(1, args);
#ifdef ETH
	} else if (!stricmp(path, setting->Misc_PS2Net)) {
		ctx->main_msg[0] = 0;
		loadNetModules();
		snprintf(ctx->main_msg, MAX_PATH, "%s", netConfig);
		return;
#endif
	} else if (!stricmp(path, setting->Misc_PS2PowerOff)) {
		ctx->main_msg[0] = 0;
		drawMsg(LNG(Powering_Off_Console));
		setupPowerOff();
		closeAllAndPoweroff();
		return;
	} else if (!stricmp(path, setting->Misc_HddManager)) {
		hddManager();
		return;
	} else if (!stricmp(path, setting->Misc_TextEditor)) {
		t = TextEditor(NULL);
		if (t == TEXTEDITOR_RESULT_LAUNCH_ARGS) {
			tmp[0] = 0;
			getFilePath(tmp, FALSE);
			if (!tmp[0])
				LaunchArgsClear();
			executeBrowserSelection(tmp, ctx);
		} else if (t == TEXTEDITOR_RESULT_BROWSE_DIR) {
			tmp[0] = 0;
			getTextEditorReturnPath(tmp);
			executeBrowserSelection(tmp, ctx);
		}
		return;
	} else if (!stricmp(path, setting->Misc_Configure)) {
		Load_External_Language();
		loadFont(setting->font_file);
		config(ctx->main_msg, ctx->cnf_path);
		return;
		//Next clause is for an optional font test routine
	} else if (!stricmp(path, setting->Misc_ShowFont)) {
		ShowFont();
		return;
	} else if (!stricmp(path, setting->Misc_Debug_Info)) {
		ShowDebugInfo(ctx->boot_argc, ctx->boot_argv, ctx->boot_path, ctx->default_osdsys_path2, ctx->rough_region, ctx->romver_data);
		return;
	} else if (!stricmp(path, setting->Misc_About_uLE)) {
		Show_About_uLE();
		return;
	} else if (!stricmp(path, setting->Misc_Show_Build_Info)) {
		Show_build_info();
		return;
	} else if (!stricmp(path, setting->Misc_Exploit_Installer)) {
		ShowExploitInstaller(ctx);
		return;
	} else if (!stricmp(path, setting->Misc_Reboot_IOP)) {
		ctx->main_msg[0] = 0;
		rebootIopAndReloadCoreStack();
		ctx->main_msg[0] = 0;
		return;
	} else if (!strncmp(path, "cdfs", 4)) {
		loadCdModules();
		LCDVD_FLUSHCACHE();
		LCDVD_DISKREADY(0);
		party[0] = 0;
		goto CheckELF_path;
	} else if (!strncmp(path, "rom", 3)) {
		party[0] = 0;
	CheckELF_path:
		strcpy(fullpath, path);
	CheckELF_fullpath:
		if ((t = checkELFheader(fullpath)) <= 0)
			goto ELFnotFound;
	ELFchecked:
		{
			int show_launch_gameid = 0;
			int disc_launch = isLikelyDiscLaunch(path);
			int have_launch_gameid = 0;
			char launch_gameid[12];

			if (!LaunchArgsPending() &&
			    LaunchArgsLoadSidecarForExec(getLaunchArgsSidecarBasePath(path, fullpath), ctx->main_msg, MAX_PATH) < 0)
				return;

			if (disc_launch) {
				have_launch_gameid = buildLaunchGameID(fullpath, launch_gameid, sizeof(launch_gameid));
				if (have_launch_gameid) {
					updateOSDHistoryFile(launch_gameid);
					applyXPARAM(launch_gameid);
				}
				show_launch_gameid = have_launch_gameid && !setting->cdrom_disable_gameid;
			} else if (setting->app_gameid) {
				show_launch_gameid = buildLaunchGameID(fullpath, launch_gameid, sizeof(launch_gameid));
			}

			x = setting->reboot_iop_elf_load;
			CleanUpForExec();
			if (show_launch_gameid)
				displayRetroGemGameID(launch_gameid, 2);
			RunLoaderElf(fullpath, party, path, t, x);
		}
	} else {  //Invalid path
		t = 0;
	ELFnotFound:
		LaunchArgsClear();
		if (t == 0)
			sprintf(ctx->main_msg, "%s %s.", fullpath, LNG(is_Not_Found));
		else
			sprintf(ctx->main_msg, "%s: %s.", LNG(This_file_isnt_an_ELF), fullpath);
		return;
	}
}
//------------------------------
//endfunc ExecuteMainAction
//---------------------------------------------------------------------------
