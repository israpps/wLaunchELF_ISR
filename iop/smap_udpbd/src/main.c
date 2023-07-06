#include <errno.h>
#include <stdio.h>
#include <loadcore.h>
#include <thbase.h>
#include <irx.h>

#include "main.h"
#include "xfer.h"

// Last SDK 3.1.0 has INET family version "2.26.0"
// SMAP module is the same as "2.25.0"
IRX_ID("SMAP_driver", 0x2, 0x1A);

//While the header of the export table is small, the large size of the export table (as a whole) places it in data instead of sdata.
extern struct irx_export_table _exp_smap __attribute__((section("data")));

int _start(int argc, char *argv[])
{
    int result;

    if (RegisterLibraryEntries(&_exp_smap) != 0) {
        PRINTF("smap: module already loaded\n");
        return MODULE_NO_RESIDENT_END;
    }

    if ((result = smap_init(argc, argv)) < 0) {
        PRINTF("smap: smap_init -> %d\n", result);
        ReleaseLibraryEntries(&_exp_smap);
        return MODULE_NO_RESIDENT_END;
    }

    return MODULE_RESIDENT_END;
}
