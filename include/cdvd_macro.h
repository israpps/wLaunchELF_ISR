#ifndef __CDVD_COMPAT_MACRO
#define __CDVD_COMPAT_MACRO

#ifdef LIBCDVD_LEGACY
#define CDVD_INIT() CDVD_Init()
#define CDVD_STOP() CDVD_Stop()
#define CDVD_DISKREADY(x) CDVD_DiskReady(x)
#else
#define CDVD_INIT() // does not exist on sdk libcdvd
#define CDVD_STOP() sceCdStop()
#define CDVD_DISKREADY(x) sceCdDiskReady(x)
#endif
#endif