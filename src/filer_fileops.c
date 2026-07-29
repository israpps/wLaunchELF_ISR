#include "launchelf.h"
#include "filer_shared.h"
#include "init.h"

#ifdef ULE_DEBUG_BUILD
#ifndef FILEOP_TRACE
#define FILEOP_TRACE 1
#endif
#else
#undef FILEOP_TRACE
#define FILEOP_TRACE 0
#endif

static void ensurePathMemoryCardAccessible(const char *path)
{
	if (path != NULL && !strncmp(path, "mc1:", 4))
		ensureMemoryCardPortAccessible(1);
}

#if FILEOP_TRACE
#define FILEOP_TRACE_FD_SLOTS 128
typedef struct
{
	int fd;
	int is_dir;
	int mode;
	char path[MAX_PATH];
} FILEOP_FD_TRACE;

static FILEOP_FD_TRACE fileop_fd_trace[FILEOP_TRACE_FD_SLOTS];
static int fileop_fd_trace_inited = 0;

static const char *fileopTraceDisplayPath(const char *path, char *display_path, size_t display_path_size)
{
	int pfs_index;

	if (path == NULL)
		return "<null>";
	if (display_path == NULL || display_path_size == 0)
		return path;

	if (!strncmp(path, "pfs", 3) && path[3] >= '0' && path[3] <= '9' && path[4] == ':') {
		pfs_index = path[3] - '0';
		if (pfs_index >= 0 && pfs_index < MOUNT_LIMIT && mountedParty[pfs_index][0] != '\0') {
			snprintf(display_path, display_path_size, "%s:pfs%d:%s", mountedParty[pfs_index], pfs_index, path + 5);
			display_path[display_path_size - 1] = '\0';
			return display_path;
		}
	}

	snprintf(display_path, display_path_size, "%s", path);
	return display_path;
}

static void fileopTraceInit(void)
{
	int i;

	if (fileop_fd_trace_inited)
		return;
	for (i = 0; i < FILEOP_TRACE_FD_SLOTS; i++) {
		fileop_fd_trace[i].fd = -1;
		fileop_fd_trace[i].is_dir = 0;
		fileop_fd_trace[i].mode = 0;
		fileop_fd_trace[i].path[0] = '\0';
	}
	fileop_fd_trace_inited = 1;
}

static int fileopTraceFindSlotByFd(int fd)
{
	int i;

	for (i = 0; i < FILEOP_TRACE_FD_SLOTS; i++) {
		if (fileop_fd_trace[i].fd == fd)
			return i;
	}
	return -1;
}

static int fileopTraceAllocSlot(void)
{
	int i;

	for (i = 0; i < FILEOP_TRACE_FD_SLOTS; i++) {
		if (fileop_fd_trace[i].fd < 0)
			return i;
	}
	/* Fall back to a deterministic replacement slot when table is full. */
	return (fileop_fd_trace_inited ? 0 : -1);
}

static void fileopTraceSet(int fd, const char *path, int mode, int is_dir)
{
	int slot;
	char display_path[MAX_PATH];

	if (fd < 0)
		return;

	fileopTraceInit();
	slot = fileopTraceFindSlotByFd(fd);
	if (slot < 0)
		slot = fileopTraceAllocSlot();
	if (slot < 0)
		return;

	fileop_fd_trace[slot].fd = fd;
	fileop_fd_trace[slot].is_dir = is_dir;
	fileop_fd_trace[slot].mode = mode;
	if (path != NULL) {
		fileopTraceDisplayPath(path, display_path, sizeof(display_path));
		snprintf(fileop_fd_trace[slot].path, sizeof(fileop_fd_trace[slot].path), "%s", display_path);
	} else {
		fileop_fd_trace[slot].path[0] = '\0';
	}
}

static void fileopTraceClear(int fd)
{
	int slot;

	if (fd < 0)
		return;

	fileopTraceInit();
	slot = fileopTraceFindSlotByFd(fd);
	if (slot < 0)
		return;

	fileop_fd_trace[slot].fd = -1;
	fileop_fd_trace[slot].is_dir = 0;
	fileop_fd_trace[slot].mode = 0;
	fileop_fd_trace[slot].path[0] = '\0';
}

static const FILEOP_FD_TRACE *fileopTraceGet(int fd)
{
	int slot;

	fileopTraceInit();
	slot = fileopTraceFindSlotByFd(fd);
	if (slot < 0)
		return NULL;

	return &fileop_fd_trace[slot];
}

static void fileopTraceLogRw(const char *op, int fd, int size, int ret, u64 start, u64 end)
{
	const FILEOP_FD_TRACE *fd_info;
	const char *path = "<unknown>";
	u64 elapsed = (end >= start) ? (end - start) : 0;

	fd_info = fileopTraceGet(fd);
	if ((fd_info != NULL) && fd_info->path[0] != '\0')
		path = fd_info->path;

	printf("[FILEOP] %s fd=%d path=%s size=%d ret=%d dt=%llu ms\n",
	       op, fd, path, size, ret, (unsigned long long)elapsed);
}
#endif

void genLimObjName(char *uLE_path, int reserve)
{
	char *p, *q, *r;
	int limit = 256;                                            //enforce a generic limit of 256 characters
	int folder_flag = (uLE_path[strlen(uLE_path) - 1] == '/');  //flag folder object
	int overflow;

	if (!strncmp(uLE_path, "mc", 2) || !strncmp(uLE_path, "vmc", 3))
		limit = 32;  //enforce MC limit of 32 characters

	if (folder_flag)                         //if path ends with path separator
		uLE_path[strlen(uLE_path) - 1] = 0;  //  remove final path separator (temporarily)

	p = uLE_path;                         //initially assume a pure object name (quite insanely :))
	if ((q = strchr(p, ':')) != NULL)     //if a drive separator is present
		p = q + 1;                        //  object name may start after drive separator
	if ((q = strrchr(p, '/')) != NULL)    //If there's any path separator in the string
		p = q + 1;                        //  object name starts after last path separator
	limit -= reserve;                     //lower limit by reserved character space
	overflow = strlen(p) - limit;         //Calculate length of string to remove (if positive)
	if ((limit <= 0) || (overflow <= 0))  //if limit invalid, or not exceeded
		goto limited;                     //  no further limitation is needed
	if ((q = strrchr(p, '.')) == NULL)    //if there's no extension separator
		goto limit_end;                   //limitation must be done at end of full name
	r = q - overflow;                     //r is the place to recopy file extension
	if (r > p) {                          //if this place is above string start
		strcpy(r, q);                     //remove overflow from end of prefix part
		goto limited;                     //which concludes the limitation
	}                                     //if we fall through here, the prefix part was too short for the limitation needed
limit_end:
	p[limit] = 0;  //  remove overflow from end of full name
limited:

	if (folder_flag)            //if original path ended with path separator
		strcat(uLE_path, "/");  //  reappend final path separator after name
}

int genRmdir(char *path)
{
	int ret;
#if FILEOP_TRACE
	u64 t0, t1;
	char log_path[MAX_PATH];
#endif
#if defined(ETH) || defined(UDPFS)
	char mapped_path[MAX_PATH];

	makeHostPath(mapped_path, path);
	path = mapped_path;
#endif

	ensurePathMemoryCardAccessible(path);
	genLimObjName(path, 0);
#if FILEOP_TRACE
	t0 = Timer();
#endif
	ret = fileXioRmdir(path);
#if FILEOP_TRACE
	t1 = Timer();
	printf("[FILEOP] rmdir path=%s ret=%d dt=%llu ms\n",
	       fileopTraceDisplayPath(path, log_path, sizeof(log_path)),
	       ret, (unsigned long long)((t1 >= t0) ? (t1 - t0) : 0));
#endif
	return ret;
}
//------------------------------
//endfunc genRmdir
//--------------------------------------------------------------
int genRemove(char *path)
{
	int ret;
#if FILEOP_TRACE
	u64 t0, t1;
	char log_path[MAX_PATH];
#endif
#if defined(ETH) || defined(UDPFS)
	char mapped_path[MAX_PATH];
#endif

	DPRINTF("%s: '%s'\n", __FUNCTION__, path);
#if defined(ETH) || defined(UDPFS)
	makeHostPath(mapped_path, path);
	path = mapped_path;
#endif

	ensurePathMemoryCardAccessible(path);
	genLimObjName(path, 0);
#if FILEOP_TRACE
	t0 = Timer();
#endif
	ret = fileXioRemove(path);
#if FILEOP_TRACE
	t1 = Timer();
	printf("[FILEOP] remove path=%s ret=%d dt=%llu ms\n",
	       fileopTraceDisplayPath(path, log_path, sizeof(log_path)),
	       ret, (unsigned long long)((t1 >= t0) ? (t1 - t0) : 0));
#endif
	return ret;
}
//------------------------------
//endfunc genRemove
//--------------------------------------------------------------
int genGetStat(const char *path, iox_stat_t *stat)
{
	char stat_path[MAX_PATH];

	if (path == NULL || path[0] == '\0' || stat == NULL)
		return -1;
	snprintf(stat_path, sizeof(stat_path), "%s", path);
#if defined(ETH) || defined(UDPFS)
	makeHostPath(stat_path, stat_path);
#endif
	ensurePathMemoryCardAccessible(stat_path);
	genLimObjName(stat_path, 0);
	return fileXioGetStat(stat_path, stat);
}
//------------------------------
//endfunc genGetStat
//--------------------------------------------------------------
int genMkdir(const char *path, int mode)
{
	char mkdir_path[MAX_PATH];
	int ret;
#if FILEOP_TRACE
	u64 t0, t1;
	char log_path[MAX_PATH];
#endif

	if (path == NULL || path[0] == '\0')
		return -1;
	snprintf(mkdir_path, sizeof(mkdir_path), "%s", path);
#if defined(ETH) || defined(UDPFS)
	makeHostPath(mkdir_path, mkdir_path);
#endif
	ensurePathMemoryCardAccessible(mkdir_path);
	genLimObjName(mkdir_path, 0);

#if FILEOP_TRACE
	t0 = Timer();
#endif
	ret = fileXioMkdir(mkdir_path, mode);
#if FILEOP_TRACE
	t1 = Timer();
	printf("[FILEOP] mkdir path=%s mode=0x%x ret=%d dt=%llu ms\n",
	       fileopTraceDisplayPath(mkdir_path, log_path, sizeof(log_path)),
	       mode, ret, (unsigned long long)((t1 >= t0) ? (t1 - t0) : 0));
#endif
	return ret;
}
//------------------------------
//endfunc genMkdir
//--------------------------------------------------------------
int genOpen(const char *path, int mode)
{
	char open_path[MAX_PATH], alt_path[MAX_PATH], *sep;
	int fd;
#if FILEOP_TRACE
	u64 t0, t1;
	char req_log_path[MAX_PATH];
	char mapped_log_path[MAX_PATH];
#endif

	if (path == NULL || path[0] == '\0')
		return -1;
	DPRINTF("%s: '%s' @ %d\n", __FUNCTION__, path, mode);
	snprintf(open_path, sizeof(open_path), "%s", path);
#if defined(ETH) || defined(UDPFS)
	makeHostPath(open_path, open_path);
#endif
	ensurePathMemoryCardAccessible(open_path);
	genLimObjName(open_path, 0);
#if FILEOP_TRACE
	t0 = Timer();
#endif
	fd = fileXioOpen(open_path, mode, fileMode);
#if FILEOP_TRACE
	t1 = Timer();
	printf("[FILEOP] open req=%s mapped=%s mode=0x%x fd=%d dt=%llu ms\n",
	       fileopTraceDisplayPath(path, req_log_path, sizeof(req_log_path)),
	       fileopTraceDisplayPath(open_path, mapped_log_path, sizeof(mapped_log_path)),
	       mode, fd,
	       (unsigned long long)((t1 >= t0) ? (t1 - t0) : 0));
#endif
	if (fd >= 0)
#if FILEOP_TRACE
	{
		fileopTraceSet(fd, open_path, mode, 0);
		return fd;
	}
#else
		return fd;
#endif

	/*
	 * Path-style compatibility fallback:
	 * try both "dev:/path" and "dev:path" when one style fails.
	 */
	snprintf(alt_path, sizeof(alt_path), "%s", open_path);
	sep = strchr(alt_path, ':');
	if (sep == NULL || sep[1] == '\0')
		return fd;

	if (sep[1] == '/' || sep[1] == '\\') {
		memmove(sep + 1, sep + 2, strlen(sep + 2) + 1);
	} else {
		size_t len = strlen(alt_path);
		if (len + 1 >= MAX_PATH)
			return fd;
		memmove(sep + 2, sep + 1, strlen(sep + 1) + 1);
		sep[1] = '/';
	}

	genLimObjName(alt_path, 0);
	ensurePathMemoryCardAccessible(alt_path);
#if FILEOP_TRACE
	t0 = Timer();
#endif
	fd = fileXioOpen(alt_path, mode, fileMode);
#if FILEOP_TRACE
	t1 = Timer();
	printf("[FILEOP] open-fallback req=%s mapped=%s mode=0x%x fd=%d dt=%llu ms\n",
	       fileopTraceDisplayPath(path, req_log_path, sizeof(req_log_path)),
	       fileopTraceDisplayPath(alt_path, mapped_log_path, sizeof(mapped_log_path)),
	       mode, fd,
	       (unsigned long long)((t1 >= t0) ? (t1 - t0) : 0));
	if (fd >= 0)
		fileopTraceSet(fd, alt_path, mode, 0);
#endif
	return fd;
}
//------------------------------
//endfunc genOpen
//--------------------------------------------------------------
int genDopen(char *path)
{
	int fd;
#if FILEOP_TRACE
	u64 t0, t1;
	char req_log_path[MAX_PATH];
	char mapped_log_path[MAX_PATH];
#endif
#if defined(ETH) || defined(UDPFS)
	char mapped_path[MAX_PATH];

	makeHostPath(mapped_path, path);
	path = mapped_path;
#endif

	DPRINTF("%s: '%s'\n", __FUNCTION__, path);

	ensurePathMemoryCardAccessible(path);
	if (!strncmp(path, "pfs", 3) || !strncmp(path, "vmc", 3)) {
		char tmp[MAX_PATH];

		strcpy(tmp, path);
		if (tmp[strlen(tmp) - 1] == '/')
			tmp[strlen(tmp) - 1] = '\0';
#if FILEOP_TRACE
		t0 = Timer();
#endif
		fd = fileXioDopen(tmp);
#if FILEOP_TRACE
		t1 = Timer();
		printf("[FILEOP] dopen req=%s mapped=%s fd=%d dt=%llu ms\n",
		       fileopTraceDisplayPath(path, req_log_path, sizeof(req_log_path)),
		       fileopTraceDisplayPath(tmp, mapped_log_path, sizeof(mapped_log_path)),
		       fd,
		       (unsigned long long)((t1 >= t0) ? (t1 - t0) : 0));
		if (fd >= 0)
			fileopTraceSet(fd, tmp, 0, 1);
#endif
	} else {
#if FILEOP_TRACE
		t0 = Timer();
#endif
		fd = fileXioDopen(path);
#if FILEOP_TRACE
		t1 = Timer();
		printf("[FILEOP] dopen req=%s mapped=%s fd=%d dt=%llu ms\n",
		       fileopTraceDisplayPath(path, req_log_path, sizeof(req_log_path)),
		       fileopTraceDisplayPath(path, mapped_log_path, sizeof(mapped_log_path)),
		       fd,
		       (unsigned long long)((t1 >= t0) ? (t1 - t0) : 0));
		if (fd >= 0)
			fileopTraceSet(fd, path, 0, 1);
#endif
	}

	return fd;
}
//------------------------------
//endfunc genDopen
//--------------------------------------------------------------
s64 genLseek(int fd, s64 where, int how)
{
	s64 ret64;
#if FILEOP_TRACE
	int ret32 = -1;
	int fallback_used = 0;
	u64 t0, t1, t2 = 0, t3 = 0;

	t0 = Timer();
#endif
	ret64 = fileXioLseek64(fd, where, how);
#if FILEOP_TRACE
	t1 = Timer();
#endif

	if (ret64 < 0 && where <= 0x7FFFFFFFLL && where >= (-0x7FFFFFFFLL - 1)) {
#if FILEOP_TRACE
		fallback_used = 1;
		t2 = Timer();
		ret32 = fileXioLseek(fd, (int)where, how);
		t3 = Timer();
		ret64 = ret32;
#else
		ret64 = fileXioLseek(fd, (int)where, how);
#endif
	}

#if FILEOP_TRACE
	printf("[FILEOP] lseek fd=%d where=0x%08x%08x how=%d ret=0x%08x%08x lseek64_dt=%llu ms fallback=%d fallback_ret=%d fallback_dt=%llu ms\n",
	       fd,
	       (unsigned int)(where >> 32), (unsigned int)where, how,
	       (unsigned int)(ret64 >> 32), (unsigned int)ret64,
	       (unsigned long long)((t1 >= t0) ? (t1 - t0) : 0),
	       fallback_used, ret32,
	       (unsigned long long)((t3 >= t2) ? (t3 - t2) : 0));
#endif
	return ret64;
}
//------------------------------
//endfunc genLseek
//--------------------------------------------------------------
int genRead(int fd, void *buf, int size)
{
	int ret;
#if FILEOP_TRACE
	u64 t0, t1;

	t0 = Timer();
#endif
	ret = fileXioRead(fd, buf, size);
#if FILEOP_TRACE
	t1 = Timer();
	fileopTraceLogRw("read", fd, size, ret, t0, t1);
#endif
	return ret;
}
//------------------------------
//endfunc genRead
//--------------------------------------------------------------
int genWrite(int fd, void *buf, int size)
{
	int ret;
#if FILEOP_TRACE
	u64 t0, t1;

	t0 = Timer();
#endif
	ret = fileXioWrite(fd, buf, size);
#if FILEOP_TRACE
	t1 = Timer();
	fileopTraceLogRw("write", fd, size, ret, t0, t1);
#endif
	return ret;
}
//------------------------------
//endfunc genWrite
//--------------------------------------------------------------
int genClose(int fd)
{
	int ret;
#if FILEOP_TRACE
	u64 t0, t1;

	t0 = Timer();
#endif
	ret = fileXioClose(fd);
#if FILEOP_TRACE
	t1 = Timer();
	printf("[FILEOP] close fd=%d ret=%d dt=%llu ms\n",
	       fd, ret, (unsigned long long)((t1 >= t0) ? (t1 - t0) : 0));
	fileopTraceClear(fd);
#endif
	return ret;
}
//------------------------------
//endfunc genClose
//--------------------------------------------------------------
int genDclose(int fd)
{
	int ret;
#if FILEOP_TRACE
	u64 t0, t1;

	t0 = Timer();
#endif
	ret = fileXioDclose(fd);
#if FILEOP_TRACE
	t1 = Timer();
	printf("[FILEOP] dclose fd=%d ret=%d dt=%llu ms\n",
	       fd, ret, (unsigned long long)((t1 >= t0) ? (t1 - t0) : 0));
	fileopTraceClear(fd);
#endif
	return ret;
}
//------------------------------
//endfunc genDclose
//--------------------------------------------------------------
int genCmpFileExt(const char *filename, const char *extension)
{
	const char *p;

	p = strrchr(filename, '.');
	return (p != NULL && !stricmp(p + 1, extension));
}
//------------------------------
//endfunc genCmpFileExt
//--------------------------------------------------------------
//End of file: filer_fileops.c
//--------------------------------------------------------------
