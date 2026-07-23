#ifndef FILER_INTERNAL_H
#define FILER_INTERNAL_H

#include "filer_shared.h"

#ifdef DVRP
extern char mountedDVRPParty[MOUNT_LIMIT][MAX_NAME];
extern int latestDVRPMount;
#endif

#if defined(ETH) || defined(UDPFS)
extern int host_error;
extern int host_ready;
extern int host_use_Bsl;
#endif

void unmountHddPartiesNotNeededByClipboard(void);

#endif
