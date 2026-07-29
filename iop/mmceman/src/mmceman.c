#include <intrman.h>
#include <stdio.h>
#include <string.h>
#include <sysclib.h>

#include "irx_imports.h"

#include "sio2man_hook.h"
#include "mmce_sio2.h"
#include "mmce_cmds.h"
#include "mmce_fs.h"

#include "module_debug.h"

#define MAJOR 2
#define MINOR 1

const char *mmce_product_ids[] = {"Unknown", "SD2PSX", "MemCard PRO2", "PicoMemcard+", "PicoMemcardZero"};

IRX_ID("mmceman", MAJOR, MINOR);

static void mmce_init(int port)
{
    int res = -1;
    int id = 0;

    mmce_sio2_set_port(port);

    for (int i = 0; i < 6; i++) {
        res = mmce_cmd_ping();
        if (res != -1) {
            DPRINTF("Found card in port %i\n", port);

            id = (res & 0xFF00) >> 8;
            if (id < 5) {
                DPRINTF("Product ID: %i (%s)\n", id, mmce_product_ids[id]);
            } else {
                DPRINTF("Unknown Product ID: %i\n", id);
            }

            DPRINTF("Revision ID: %i\n", (res & 0xFF));
            DPRINTF("Protocol Version: %i\n", (res & 0xFF0000) >> 16);

            DPRINTF("Resetting MMCE FS...");
            res = mmce_cmd_reset();
            if (res != 0)
                DPRINTF("Failed: %i\n", res);
            else
                DPRINTF("Okay\n");

            break;
        }
    }
}

int __start(int argc, char *argv[], void *startaddr, ModuleInfo_t *mi)
{
    int rv;

    printf("Multipurpose Memory Card Emulator Manager (MMCEMAN) v%d.%d by the MMCE team\n", MAJOR, MINOR);

    //Install hooks
    rv = mmce_sio2_init();
    if (rv != 0) {
        DPRINTF("%s: mmce_sio2_init failed, rv %i\n", __func__, rv);
        return MODULE_NO_RESIDENT_END;
    }

    //Try to init MMCE's on both ports
    mmce_init(2);
    mmce_init(3);

    //Attach filesystem to iomanX
    mmce_fs_register();

    // If modload has certain flags set indicating new version,
    // set the unloadable flag
    if (mi && ((mi->newflags & 2) != 0))
        mi->newflags |= 0x10;
    return MODULE_RESIDENT_END;
}

int __stop(int argc, char *argv[], void *startaddr, ModuleInfo_t *mi)
{
    DPRINTF("Unloading module\n");

    mmce_fs_unregister();
    mmce_sio2_deinit();

    return MODULE_NO_RESIDENT_END;
}

int _start(int argc, char *argv[], void *startaddr, ModuleInfo_t *mi)
{
    if (argc >= 0)
        return __start(argc, argv, startaddr, mi);
    else
        return __stop(-argc, argv, startaddr, mi);
}
