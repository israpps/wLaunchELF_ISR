#ifndef __CDVD_COMPAT_MACRO
#define __CDVD_COMPAT_MACRO

#ifdef LIBCDVD_LEGACY
#define LCDVD_INIT() CDVD_Init()
#define LCDVD_STOP() CDVD_Stop()
#define LCDVD_DISKREADY(x) CDVD_DiskReady(x)
#else
#define LCDVD_INIT() // does not exist on sdk libcdvd
#define LCDVD_STOP() sceCdStop()
#define LCDVD_DISKREADY(x) sceCdDiskReady(x)
#endif
#endif