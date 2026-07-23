//--------------------------------------------------------------
//File name:    elf.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "init.h"
#include <elf.h>
#ifdef XFROM
#include <libsecr.h>
#endif

#define MAX_PATH 1025
#define MBR_PAYLOAD_LOAD_ADDR 0x01000000

extern u8 loader_elf[];
extern int size_loader_elf;

#define LAUNCH_ARG_MAX_COUNT 12
#define LAUNCH_ARG_MAX_LINE 255
#define LAUNCH_ARG_MAX_BYTES 2048
#define ELFLOAD_BASE_ARGC 3
#define ELFLOAD_MAX_ARGC 15

static char launchArgStorage[LAUNCH_ARG_MAX_COUNT][LAUNCH_ARG_MAX_LINE + 1];
static int launchArgCount;
static int launchArgBytes;
static char launchArgSource[MAX_PATH];

void LaunchArgsClear(void)
{
	launchArgCount = 0;
	launchArgBytes = 0;
	launchArgSource[0] = '\0';
}

int LaunchArgsPending(void)
{
	return launchArgCount > 0;
}

int LaunchArgsGetCount(void)
{
	return launchArgCount;
}

static int launchArgsSetError(char *message, size_t message_size, const char *format, int limit)
{
	if (message != NULL && message_size > 0)
		snprintf(message, message_size, format, limit);
	LaunchArgsClear();
	return -1;
}

static int launchArgsSetPlainError(char *message, size_t message_size, const char *text)
{
	if (message != NULL && message_size > 0)
		snprintf(message, message_size, "%s", text);
	LaunchArgsClear();
	return -1;
}

static void launchArgsMakeOpenPath(const char *path, char *file_path, size_t file_path_size)
{
	strncpy(file_path, path, file_path_size - 1);
	file_path[file_path_size - 1] = '\0';
	if (genFixPath(path, file_path) < 0) {
		strncpy(file_path, path, file_path_size - 1);
		file_path[file_path_size - 1] = '\0';
	}
}

static int launchArgsCommitLine(const char *line, int line_len, char *message, size_t message_size)
{
	if (line_len <= 0)
		return 0;
	if (launchArgCount >= LAUNCH_ARG_MAX_COUNT)
		return launchArgsSetError(message, message_size, LNG(Launch_Args_Too_Many), LAUNCH_ARG_MAX_COUNT);
	if (line_len > LAUNCH_ARG_MAX_LINE)
		return launchArgsSetError(message, message_size, LNG(Launch_Arg_Too_Long), LAUNCH_ARG_MAX_LINE);
	if (launchArgBytes + line_len + 1 > LAUNCH_ARG_MAX_BYTES)
		return launchArgsSetError(message, message_size, LNG(Launch_Args_Too_Large), LAUNCH_ARG_MAX_BYTES);

	memcpy(launchArgStorage[launchArgCount], line, line_len);
	launchArgStorage[launchArgCount][line_len] = '\0';
	launchArgCount++;
	launchArgBytes += line_len + 1;
	return 0;
}

static int launchArgsParseChar(int ch, char *line, int *line_len, int *skip_lf, char *message, size_t message_size)
{
	if (*skip_lf) {
		*skip_lf = 0;
		if (ch == '\n')
			return 0;
	}
	if (ch == '\r') {
		if (launchArgsCommitLine(line, *line_len, message, message_size) < 0)
			return -1;
		*line_len = 0;
		*skip_lf = 1;
		return 0;
	}
	if (ch == '\n') {
		if (launchArgsCommitLine(line, *line_len, message, message_size) < 0)
			return -1;
		*line_len = 0;
		return 0;
	}
	if (ch == '\0')
		return launchArgsSetPlainError(message, message_size, LNG(Launch_Args_Invalid));
	if (*line_len >= LAUNCH_ARG_MAX_LINE)
		return launchArgsSetError(message, message_size, LNG(Launch_Arg_Too_Long), LAUNCH_ARG_MAX_LINE);

	line[*line_len] = ch;
	(*line_len)++;
	return 0;
}

static int launchArgsFinishParse(const char *source, const char *line, int line_len, char *message, size_t message_size)
{
	if (launchArgsCommitLine(line, line_len, message, message_size) < 0)
		return -1;
	if (launchArgCount <= 0)
		return launchArgsSetPlainError(message, message_size, LNG(Launch_Args_Empty));

	if (source != NULL) {
		strncpy(launchArgSource, source, sizeof(launchArgSource) - 1);
		launchArgSource[sizeof(launchArgSource) - 1] = '\0';
	}
	if (message != NULL && message_size > 0)
		snprintf(message, message_size, LNG(Launch_Args_Loaded), launchArgCount);
	return launchArgCount;
}

static int launchArgsReadFromFd(const char *source, int fd, char *message, size_t message_size)
{
	char line[LAUNCH_ARG_MAX_LINE + 1];
	unsigned char buffer[512];
	int i, rd, line_len, skip_lf;

	line_len = 0;
	skip_lf = 0;
	while ((rd = genRead(fd, buffer, (int)sizeof(buffer))) > 0) {
		for (i = 0; i < rd; i++) {
			if (launchArgsParseChar(buffer[i], line, &line_len, &skip_lf, message, message_size) < 0) {
				genClose(fd);
				return -1;
			}
		}
	}
	genClose(fd);
	if (rd < 0)
		return launchArgsSetPlainError(message, message_size, LNG(Launch_Args_Invalid));

	return launchArgsFinishParse(source, line, line_len, message, message_size);
}

int LaunchArgsLoadFromBuffer(const char *source, const char *buffer, int size, char *message, size_t message_size)
{
	char line[LAUNCH_ARG_MAX_LINE + 1];
	int i, line_len, skip_lf;

	LaunchArgsClear();
	if (buffer == NULL || size < 0)
		return launchArgsSetPlainError(message, message_size, LNG(Launch_Args_Invalid));

	line_len = 0;
	skip_lf = 0;
	for (i = 0; i < size; i++) {
		if (launchArgsParseChar((unsigned char)buffer[i], line, &line_len, &skip_lf, message, message_size) < 0)
			return -1;
	}

	return launchArgsFinishParse(source, line, line_len, message, message_size);
}

int LaunchArgsLoadFromFile(const char *path, char *message, size_t message_size)
{
	char file_path[MAX_PATH];
	int fd;

	LaunchArgsClear();
	if (path == NULL || path[0] == '\0')
		return launchArgsSetPlainError(message, message_size, LNG(Launch_Args_Invalid));

	launchArgsMakeOpenPath(path, file_path, sizeof(file_path));
	fd = genOpen(file_path, FIO_O_RDONLY);
	if (fd < 0)
		return launchArgsSetPlainError(message, message_size, LNG(Failed_Opening_File));

	return launchArgsReadFromFd(path, fd, message, message_size);
}

int LaunchArgsLoadSidecarForExec(const char *exec_path, char *message, size_t message_size)
{
	char arg_path[MAX_PATH];
	char file_path[MAX_PATH];
	const char *name_start;
	const char *slash;
	const char *colon;
	const char *dot;
	size_t base_len;
	int fd;

	if (exec_path == NULL || exec_path[0] == '\0' || LaunchArgsPending())
		return 0;

	slash = strrchr(exec_path, '/');
	colon = strrchr(exec_path, ':');
	name_start = exec_path;
	if (slash != NULL)
		name_start = slash + 1;
	if (colon != NULL && colon + 1 > name_start)
		name_start = colon + 1;

	dot = strrchr(name_start, '.');
	base_len = (dot != NULL) ? (size_t)(dot - exec_path) : strlen(exec_path);
	if (base_len + 4 >= sizeof(arg_path))
		return 0;

	snprintf(arg_path, sizeof(arg_path), "%.*s.arg", (int)base_len, exec_path);
	launchArgsMakeOpenPath(arg_path, file_path, sizeof(file_path));
	fd = genOpen(file_path, FIO_O_RDONLY);
	if (fd < 0)
		return 0;

	LaunchArgsClear();
	return launchArgsReadFromFd(arg_path, fd, message, message_size);
}

int LaunchArgsCopyToArgv(char **argv, int max_args)
{
	int i, count;

	count = launchArgCount;
	if (count > max_args)
		count = max_args;
	for (i = 0; i < count; i++)
		argv[i] = launchArgStorage[i];
	return count;
}

static int readExecHeader(const char *path, u8 *header, int header_len, int *opened_file)
{
	int fd, rd, total;

	if (opened_file != NULL)
		*opened_file = 0;

	if (path == NULL || path[0] == '\0')
		return -1;

	fd = genOpen(path, FIO_O_RDONLY);
	if (fd < 0)
		return -1;

	if (opened_file != NULL)
		*opened_file = 1;

	/* Some devices do not support SEEK_END reliably. SEEK_SET is enough here. */
	genLseek(fd, 0, SEEK_SET);

	total = 0;
	while (total < header_len) {
		rd = genRead(fd, header + total, header_len - total);
		if (rd <= 0)
			break;
		total += rd;
	}
	genClose(fd);

	return total;
}

static int classifyExecHeader(const u8 *header, int header_len)
{
	const Elf32_Ehdr *eh;

	if (header_len < SELFMAG)
		return -1;

	/*
	 * Heuristic requested for additional encrypted KELF variants:
	 * byte[0] == 0x01 and byte[2] == 0x00.
	 */
	if (header[0] == 0x01 && header[2] == 0x00)
		return 2;

	if (memcmp(header, ELFMAG, SELFMAG) != 0)
		return -1;

	if (header_len >= (int)sizeof(Elf32_Ehdr)) {
		eh = (const Elf32_Ehdr *)header;
		if ((eh->e_type != ET_EXEC) && (eh->e_type != ET_DYN))
			return -1;
		if (eh->e_machine != EM_MIPS)
			return -1;
	}

	return 1;
}

static int classifyExecByExtension(const char *path)
{
	if (genCmpFileExt(path, "KELF") || genCmpFileExt(path, "XLF"))
		return 2;
	if (genCmpFileExt(path, "ELF"))
		return 1;

	return -1;
}

static int parseUsbMassPathUnit(const char *path, const char *prefix, int prefix_len, int *unit, const char **suffix)
{
	if (strncmp(path, prefix, prefix_len))
		return 0;

	if (path[prefix_len] == ':') {
		*unit = 0;
		*suffix = path + prefix_len + 1;
		return 1;
	}
	if (path[prefix_len] >= '0' && path[prefix_len] <= '9' && path[prefix_len + 1] == ':') {
		*unit = path[prefix_len] - '0';
		*suffix = path + prefix_len + 2;
		return 1;
	}

	return 0;
}

static int isHddPartyPath(const char *path)
{
	return (!strncmp(path, "hdd", 3) && path[3] >= '0' && path[3] <= '9' && path[4] == ':');
}

static int isHddBrowserPath(const char *path)
{
	return (isHddPartyPath(path) && path[5] == '/');
}

static int isExplicitHddHandoffPath(const char *path)
{
	return (path != NULL && (isHddBrowserPath(path) || !strncmp(path, "uLE:", 4)));
}

static const char *normalizeExecArg0Path(const char *path, char *buffer, size_t buffer_size)
{
	const char *partition;
	const char *subpath;
	const char *suffix;
	int part_len;
	int unit = 0;

	if (path == NULL || path[0] == '\0' || buffer == NULL || buffer_size == 0)
		return path;

	if (isHddBrowserPath(path)) {
		partition = path + 6;
		if (partition[0] == '\0')
			return path;

		subpath = strchr(partition, '/');
		if (subpath == NULL) {
			snprintf(buffer, buffer_size, "hdd%c:%s:pfs:/", path[3], partition);
		} else {
			part_len = (int)(subpath - partition);
			if (part_len <= 0)
				return path;
			snprintf(buffer, buffer_size, "hdd%c:%.*s:pfs:%s", path[3], part_len, partition, subpath);
		}
		return buffer;
	}

	if (!parseUsbMassPathUnit(path, "usb", 3, &unit, &suffix) &&
	    !parseUsbMassPathUnit(path, "mass", 4, &unit, &suffix))
		return path;

	if (*suffix == '\0')
		suffix = "/";

	if (*suffix != '/')
		snprintf(buffer, buffer_size, "mass%d:/%s", unit, suffix);
	else
		snprintf(buffer, buffer_size, "mass%d:%s", unit, suffix);

	return buffer;
}

static int tryCheckExecPath(const char *path, int *opened_any)
{
	u8 header[256];
	int opened_file = 0;
	int header_size;
	int kind;

	header_size = readExecHeader(path, header, sizeof(header), &opened_file);
	if (opened_file && opened_any != NULL)
		*opened_any = 1;

	if (header_size < 0)
		return -1;  // open failure
	if (header_size < 4)
		return 0;  // open success but not enough data

	kind = classifyExecHeader(header, header_size);
	return kind;
}

//--------------------------------------------------------------
//End of data declarations
//--------------------------------------------------------------
//Start of function code
//--------------------------------------------------------------
// checkELFheader Tests for valid ELF file
// Modified version of loader from Independence
//	(C) 2003 Marcus R. Brown <mrbrown@0xd6.org>
//--------------------------------------------------------------
int checkELFheader(char *path)
{
	int ret, kind, opened_any, fallback_kind;
	char fullpath[MAX_PATH], tmp[MAX_PATH];

	if (path == NULL || path[0] == '\0')
		return -1;

	strcpy(fullpath, path);
	if (!strncmp(fullpath, "cdfs", 4))
		loadCdModules();
#ifdef MMCE
	if (!strncmp(fullpath, "mmce", 4))
		loadMmceModules();
#endif
#ifdef MX4SIO
	if (!strncmp(fullpath, "mx4sio", 6) && !mx4sio_driver_running && !loadMx4sioModules())
		goto error;
#endif
#ifdef EXFAT
	if (!strncmp(fullpath, "ata", 3))
		loadAtaModules();
#endif

	opened_any = 0;

	/* 1) Try raw path first. */
	kind = tryCheckExecPath(path, &opened_any);
	if (kind > 0)
		return kind;

	/* 2) Try host path without the extra slash after device separator. */
	if (!strncmp(path, "host:/", 6)) {
		snprintf(tmp, sizeof(tmp), "host:%s", path + 6);
		kind = tryCheckExecPath(tmp, &opened_any);
		if (kind > 0)
			return kind;
	}

	/* 3) Try generic fixed path (includes HDD partition mount/translation). */
	ret = genFixPath(path, fullpath);
	if ((ret >= -99) && strcmp(fullpath, path)) {
		kind = tryCheckExecPath(fullpath, &opened_any);
		if (kind > 0)
			return kind;
	}

	/* 4) Resolve generic memory card alias explicitly. */
	if (!strncmp(path, "mc:/", 4)) {
		snprintf(tmp, sizeof(tmp), "mc0:%s", path + 3);
		kind = tryCheckExecPath(tmp, &opened_any);
		if (kind > 0)
			return kind;
		snprintf(tmp, sizeof(tmp), "mc1:%s", path + 3);
		kind = tryCheckExecPath(tmp, &opened_any);
		if (kind > 0)
			return kind;
	}

	/*
	 * 5) Last-resort fallback:
	 * if the file opens but header read is unreliable, trust known executable
	 * extensions to avoid false negatives on some device stacks.
	 */
	if (opened_any) {
		fallback_kind = classifyExecByExtension(path);
		if (fallback_kind > 0)
			return fallback_kind;
	}

error:
	return -1;  //return -1 for failed check
}
//------------------------------
//End of func:  int checkELFheader(const char *path)
//--------------------------------------------------------------
static void RunEmbeddedLoader(int argc, char **argv)
{
	u8 *boot_elf;
	Elf32_Ehdr *eh;
	Elf32_Phdr *eph;
	void *pdata;
	int i;

	/* NB: LOADER.ELF is embedded  */
	boot_elf = (u8 *)loader_elf;
	eh = (Elf32_Ehdr *)boot_elf;
	if (memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0)
		asm volatile("break\n");
	DPRINTF("RunEmbeddedLoader: loader embedded entry=0x%08x phoff=0x%08x phnum=%u\n",
	        eh->e_entry, eh->e_phoff, eh->e_phnum);

	eph = (Elf32_Phdr *)(boot_elf + eh->e_phoff);

	/* Scan through the ELF's program headers and copy them into RAM, then
									zero out any non-loaded regions.  */
	for (i = 0; i < eh->e_phnum; i++) {
		if (eph[i].p_type != PT_LOAD)
			continue;
		DPRINTF("RunEmbeddedLoader: loader phdr[%d] vaddr=%p offset=0x%08x filesz=0x%08x memsz=0x%08x\n",
		        i, (void *)eph[i].p_vaddr, eph[i].p_offset, eph[i].p_filesz, eph[i].p_memsz);

		pdata = (void *)(boot_elf + eph[i].p_offset);
		memcpy((void *)eph[i].p_vaddr, pdata, eph[i].p_filesz);

		if (eph[i].p_memsz > eph[i].p_filesz)
			memset((void *)(eph[i].p_vaddr + eph[i].p_filesz), 0,
			       eph[i].p_memsz - eph[i].p_filesz);
	}
	if (eh->e_entry == 0 || (eh->e_entry & 0x3) != 0) {
		DPRINTF("RunEmbeddedLoader: invalid embedded loader entry=0x%08x\n", eh->e_entry);
		return;
	}
	/* Let's go.  */
	SifExitRpc();
	FlushCache(0);
	FlushCache(2);

	ExecPS2((void *)eh->e_entry, NULL, argc, argv);
}
//--------------------------------------------------------------
//End of func:  void RunEmbeddedLoader(int argc, char **argv)
//--------------------------------------------------------------
void RunLoaderMemory(const char *arg0, const char *mem_arg, int reboot_iop)
{
#define MEMLOAD_ARGC 3
	char *argv[MEMLOAD_ARGC];
	static char loader_arg[8];

	snprintf(loader_arg, sizeof(loader_arg), "%s", (reboot_iop) ? "-la=ER" : "-la=E");
	argv[0] = (char *)arg0;
	argv[1] = (char *)mem_arg;
	argv[2] = loader_arg;

	RunEmbeddedLoader(MEMLOAD_ARGC, argv);
}
//--------------------------------------------------------------
//End of func:  void RunLoaderMemory(const char *arg0, const char *mem_arg, int reboot_iop)
//--------------------------------------------------------------
#ifdef XFROM
static int isElfPayload(const u8 *payload, int payload_size)
{
	const Elf32_Ehdr *eh;

	if (payload == NULL || payload_size < (int)sizeof(Elf32_Ehdr))
		return 0;

	eh = (const Elf32_Ehdr *)payload;
	return (memcmp(eh->e_ident, ELFMAG, SELFMAG) == 0 &&
	        ((eh->e_type == ET_EXEC) || (eh->e_type == ET_DYN)) &&
	        eh->e_machine == EM_MIPS);
}

static int isLikelyEncryptedPayload(const u8 *payload, int payload_size)
{
	return (payload != NULL && payload_size >= 4 && payload[0] == 0x01 && payload[2] == 0x00);
}

static int getMbrOpenPath(const char *path, char *open_path, size_t open_path_size)
{
	const char *pfs;
	char party[MAX_PATH];
	size_t party_len;
	int party_ix;

	if (path == NULL || open_path == NULL || open_path_size == 0)
		return -1;

	if (!strncmp(path, "xfrom:", 6)) {
		if (!loadFlashModules())
			return -1;
		snprintf(open_path, open_path_size, "%s", path);
		return 0;
	}

	if (strncmp(path, "hdd", 3) || path[3] < '0' || path[3] > '9' || path[4] != ':')
		return -1;

	if (!loadHddModules())
		return -1;

	pfs = strstr(path, ":pfs:");
	if (pfs == NULL)
		return -1;

	party_len = (size_t)(pfs - path);
	if (party_len == 0 || party_len >= sizeof(party))
		return -1;

	memcpy(party, path, party_len);
	party[party_len] = '\0';

	party_ix = mountParty(party);
	if (party_ix < 0)
		return -1;

	snprintf(open_path, open_path_size, "pfs%d:%s", party_ix, pfs + 5);
	return 0;
}

int PrepareMbrLaunchPayload(const char *path, char *mem_arg, size_t mem_arg_size)
{
	char open_path[MAX_PATH];
	u8 *payload;
	u8 *launch_payload;
	s64 payload_size64;
	u32 ee_mem_end;
	int payload_size;
	int encrypted_payload;
	int fd, total, rd;

	if (path == NULL || mem_arg == NULL || mem_arg_size < 22)
		return -1;

	if (getMbrOpenPath(path, open_path, sizeof(open_path)) < 0)
		return -1;

	fd = genOpen(open_path, FIO_O_RDONLY);
	if (fd < 0)
		return -1;

	ee_mem_end = GetMemorySize();
	payload_size64 = genLseek(fd, 0, SEEK_END);
	if (ee_mem_end <= MBR_PAYLOAD_LOAD_ADDR ||
	    payload_size64 <= 0 ||
	    payload_size64 > (s64)(ee_mem_end - MBR_PAYLOAD_LOAD_ADDR)) {
		genClose(fd);
		return -1;
	}
	genLseek(fd, 0, SEEK_SET);

	payload = (u8 *)MBR_PAYLOAD_LOAD_ADDR;
	payload_size = (int)payload_size64;
	total = 0;
	while (total < payload_size) {
		rd = genRead(fd, payload + total, payload_size - total);
		if (rd <= 0)
			break;
		total += rd;
	}
	genClose(fd);

	if (total != payload_size)
		return -1;

	launch_payload = payload;
	encrypted_payload = isLikelyEncryptedPayload(payload, payload_size);
	if (!isElfPayload(payload, payload_size) && encrypted_payload) {
		void *decrypted_payload;

		if (!loadSecrSifModule())
			return -1;

		if (!SecrInit())
			return -1;
		decrypted_payload = SecrDiskBootFile(payload);
		SecrDeinit();

		if (decrypted_payload == NULL)
			return -1;

		launch_payload = (u8 *)decrypted_payload;
		if (launch_payload < payload || launch_payload >= payload + payload_size)
			return -1;
		payload_size -= (int)(launch_payload - payload);
	}

	snprintf(mem_arg, mem_arg_size, "mem:%08X:%08X", (u32)launch_payload, (u32)payload_size);
	return 0;
}
//--------------------------------------------------------------
//End of func:  int PrepareMbrLaunchPayload(const char *path, char *mem_arg, size_t mem_arg_size)
//--------------------------------------------------------------
#endif
//--------------------------------------------------------------
// RunLoaderElf loads LOADER.ELF from program memory and passes
// args of selected ELF and partition to it
// Modified version of loader from Independence
//	(C) 2003 Marcus R. Brown <mrbrown@0xd6.org>
//------------------------------
void RunLoaderElf(char *filename, char *party, const char *selected_path, int exec_kind, int reboot_iop_elf_load)
{
	char *argv[ELFLOAD_MAX_ARGC], bootpath[256];
	static char exec_target[MAX_PATH];
	static char exec_arg0[MAX_PATH];
	static char loader_arg[8];
	const char *handoff_path = NULL;
	int argc;
#ifdef DVRP
	int dvr_pfs_ix = -1;
	char dvr_pfs_name[10] = "dvr_pfs0:";
#endif

	if (selected_path != NULL && selected_path[0] != '\0')
		handoff_path = normalizeExecArg0Path(selected_path, exec_arg0, sizeof(exec_arg0));
	snprintf(exec_target, sizeof(exec_target), "%s", filename);
	if (exec_kind == 1 && handoff_path != NULL && !strncmp(handoff_path, "mass", 4))
		snprintf(exec_target, sizeof(exec_target), "%s", handoff_path);
	DPRINTF("RunLoaderElf: exec_kind=%d reboot_iop=%d target='%s' handoff='%s' party='%s'\n",
	        exec_kind, reboot_iop_elf_load, filename,
	        (handoff_path != NULL) ? handoff_path : "",
	        (party != NULL) ? party : "");
	DPRINTF("RunLoaderElf: loader target='%s'\n", exec_target);
#ifdef DVRP
	dvr_pfs_ix = (party != NULL) ? getDVRPPartyMountIndex(party) : -1;
	if (dvr_pfs_ix >= 0)
		dvr_pfs_name[7] = '0' + dvr_pfs_ix;
#endif

	if (isHddPartyPath(party) && (!strncmp(filename, "pfs0:", 5))) {
		if (0 > fileXioMount("pfs0:", party, FIO_MT_RDONLY)) {
			//Some error occurred, it could be due to something else having used pfs0
			unmountParty(0);  //So we try unmounting pfs0, to try again
			if (0 > fileXioMount("pfs0:", party, FIO_MT_RDONLY))
				return;  //If it still fails, we have to give up...
		}

		//If a path to a file on PFS is specified, change it to the standard format.
		//hddN:partition:pfs:path/to/file
		if (strncmp(filename, "pfs0:", 5) == 0) {
			sprintf(bootpath, "%s:pfs:%s", party, &filename[5]);
		} else {
			sprintf(bootpath, "%s:%s", party, filename);
		}

		argv[0] = exec_target;
		if (isExplicitHddHandoffPath(handoff_path))
			argv[1] = (char *)handoff_path;
		else
			argv[1] = bootpath;
#ifdef DVRP
	} else if (dvr_pfs_ix >= 0 && !strncmp(filename, dvr_pfs_name, 9)) {
		if (0 > fileXioMount(dvr_pfs_name, party, FIO_MT_RDONLY)) {
			//Some error occurred, it could be due to something else having used pfs0
			unmountDVRPParty(dvr_pfs_ix);  //So we try unmounting pfs, to try again
			if (0 > fileXioMount(dvr_pfs_name, party, FIO_MT_RDONLY))
				return;  //If it still fails, we have to give up...
		}

		//If a path to a file on PFS is specified, change it to the standard format.
		//dvr_hdd0:partition:pfs:path/to/file
		if (strncmp(filename, dvr_pfs_name, 9) == 0) {
			sprintf(bootpath, "%s:pfs:%s", party, &filename[9]);
		} else {
			sprintf(bootpath, "%s:%s", party, filename);
		}
		argv[0] = exec_target;
		if ((handoff_path != NULL) && !strncmp(handoff_path, "dvr_hdd0:/", 10))
			argv[1] = (char *)handoff_path;
		else
			argv[1] = bootpath;
#endif
	} else {
		argv[0] = exec_target;
		argv[1] = (char *)((handoff_path != NULL) ? handoff_path : filename);
	}

	(void)exec_kind;
	argc = ELFLOAD_BASE_ARGC - 1;
	if (LaunchArgsPending())
		argc += LaunchArgsCopyToArgv(&argv[argc], ELFLOAD_MAX_ARGC - ELFLOAD_BASE_ARGC);
	snprintf(loader_arg, sizeof(loader_arg), "%s", (reboot_iop_elf_load) ? "-la=AR" : "-la=A");
	argv[argc++] = loader_arg;
	LaunchArgsClear();
	DPRINTF("RunLoaderElf: loader mode arg='%s' argc=%d\n", loader_arg, argc);

	RunEmbeddedLoader(argc, argv);
}
//------------------------------
//End of func:  void RunLoaderElf(char *filename, char *party, const char *selected_path, int exec_kind, int reboot_iop_elf_load)
//--------------------------------------------------------------
//End of file:  elf.c
//--------------------------------------------------------------
