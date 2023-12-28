//---------------------------------------------------------------------------
//File name:   SMB_test.c
//---------------------------------------------------------------------------

smbServer_t smbServer;  //List of active servers

int smbServerListCount = 0;  //Count of valid server entries in ServerList
int smbCurrentServer = -1;   //Index of server for current traffic (-1 if none)

//---------------------------------------------------------------------------
//Function to log on to an index selected server
//------------------------------
int smbLogon_Server(int Index)
{
	int ret;
	smbLogOn_in_t logon;

	load_smbman();

	if (smbServer.Server_Logon_f == 1) {
		DPRINTF("smbLogon_Server: Request for duplicate logon noted.\n");
		return -1;
	}

	if (smbServer.Username[0] == 0)  //if Username invalid
		strcpy(smbServer.Username, "GUEST");

	if ((smbServer.PasswordType > 0)  //if hashing wanted
	    && (smbServer.PassHash_f == 0)) {
		ret = fileXioDevctl("smb:", SMB_DEVCTL_GETPASSWORDHASHES, (void *)&smbServer.Password, sizeof(smbServer.Password), (void *)&smbServer.PassHash, sizeof(smbServer.PassHash));
		if (ret) {
			DPRINTF("smbLogon_Server: PassHash error %d\n", ret);
			return -1;
		}
		smbServer.PassHash_f = 1;  //PassHash is now valid for future use
	}

	strcpy(logon.serverIP, smbServer.Server_IP);
	logon.serverPort = smbServer.Server_Port;
	strcpy(logon.User, smbServer.Username);
	if (smbServer.PasswordType > 0)  //if hashing wanted
		memcpy((void *)logon.Password, (void *)smbServer.PassHash, sizeof(smbServer.PassHash));
	else
		strcpy(logon.Password, smbServer.Password);
	logon.PasswordType = smbServer.PasswordType;

	ret = fileXioDevctl("smb:", SMB_DEVCTL_LOGON, (void *)&logon, sizeof(logon), NULL, 0);
	if (ret) {
		DPRINTF("smbLogon_Server: Logon Error %d\n", ret);
		return -1;
	}
	smbServer.Server_Logon_f = 1;
	DPRINTF("smbLogon_Server: Logon succeeded!\n");
	return 0;  //Here basic Logon has been achieved
}
//------------------------------
//endfunc smbLogon_CurrentServer
//---------------------------------------------------------------------------
//Function to log on to a currently selected SMB server
//------------------------------
int smbLogon_CurrentServer()
{
	return smbLogon_Server(smbCurrentServer);
}
//------------------------------
//endfunc smbLogon_CurrentServer
//---------------------------------------------------------------------------
//storeSMBCNF will save SMB specific settings to a pre-allocated RAM area
//------------------------------
size_t storeSMBCNF(char *cnf_buf)
{
	size_t CNF_size, size_step;
	int i;
	sprintf(cnf_buf + CNF_size,
	        "smbServer_IP = %s\r\n"
	        "smbServer_Port = %d\r\n"
	        "smbUsername = %s\r\n"
	        "smbPassword = %s\r\n"
	        "smbPasswordType = %d\r\n"
	        "smbClient_ID = %s\r\n"
	        "smbServer_ID = %s\r\n"
	        "smbServer_FBID = %s\r\n"
	        "%n",  // %n causes NO output, but only a measurement
	        smbServer.Server_IP,
	        smbServer.Server_Port,
	        smbServer.Username,
	        smbServer.Password,
	        smbServer.PasswordType,
	        smbServer.Client_ID,
	        smbServer.Server_ID,
	        smbServer.Server_FBID,
	        &size_step  // This measures the size of sprintf data
	        );
	CNF_size += size_step;
	return CNF_size;
}
//------------------------------
//endfunc storeSMBCNF
//---------------------------------------------------------------------------
//saveSMBCNF will save SMB specific settings to an SMB CNF file
//------------------------------
int saveSMBCNF(char *CNFpath)
{
	int ret, fd;
	char tmp[SERVERLIST_MAX * 1500];  //Must have room for entire SMB CNF file
	char cnf_path[MAX_PATH];
	size_t CNF_size;

	CNF_size = storeSMBCNF(tmp);

	ret = genFixPath(CNFpath, cnf_path);  //Fix raw path argument
	if ((ret < 0) || ((fd = genOpen(cnf_path, O_CREAT | O_WRONLY | O_TRUNC)) < 0)) {
		return -1;  //Failed open
	}
	ret = genWrite(fd, &tmp, CNF_size);
	if (ret != CNF_size)
		ret = -2;  //Failed writing
	genClose(fd);

	return ret;
}
//-----------------------------
//endfunc saveSMBCNF
//---------------------------------------------------------------------------
//scanSMBCNF will check for SMB specific variables of an SMB CNF in RAM
//------------------------------
int scanSMBCNF(unsigned char *name, unsigned char *value)
{
	if (!strcmp(name, "smbServer_IP"))
		strcpy(smbServer.Server_IP, value);
	else if (!strcmp(name, "smbServer_Port"))
		smbServer.Server_Port = atoi(value);
	else if (!strcmp(name, "smbUsername"))
		strcpy(smbServer.Username, value);
	else if (!strcmp(name, "smbPassword"))
		strcpy(smbServer.Password, value);
	else if (!strcmp(name, "smbPasswordType"))
		smbServer.PasswordType = atoi(value);
	else if (!strcmp(name, "smbClient_ID"))
		strcpy(smbServer.Client_ID, value);
	else if (!strcmp(name, "smbServer_ID"))
		strcpy(smbServer.Server_ID, value);
	else if (!strcmp(name, "smbServer_FBID"))
		strcpy(smbServer.Server_FBID, value);
	else
		return 0;  //when no SMB variable
	return 1;      //when SMB variable found
}
//------------------------------
//endfunc scanSMBCNF
//---------------------------------------------------------------------------
//loadSMBCNF will load SMB specific settings from an SMB CNF file
//------------------------------
int loadSMBCNF(char *path)
{
	int dummy, var_cnt;
	unsigned char *RAM_p, *CNF_p, *name, *value;

	if (!(RAM_p = preloadCNF(path)))
		return -1;
	CNF_p = RAM_p;
	for (var_cnt = 0; get_CNF_string(&CNF_p, &name, &value); var_cnt++)
		dummy = scanSMBCNF(name, value);
	free(RAM_p);
	return 0;
}
//------------------------------
//endfunc loadSMBCNF
//---------------------------------------------------------------------------
// End of file
//---------------------------------------------------------------------------
