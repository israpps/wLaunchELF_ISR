#ifndef MODULE_DEBUG_H
#define MODULE_DEBUG_H

#ifndef MMCEDRV
#define MODNAME "mmceman"
#else
#define MODNAME "mmcedrv"
#endif

//#define DEBUG

#ifdef DEBUG
#include <stdio.h>
#define DPRINTF(fmt, x...) printf(MODNAME": "fmt, ##x)
#else
#define DPRINTF(x...)
#endif

#endif
