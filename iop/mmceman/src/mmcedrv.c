#include <intrman.h>
#include <stdio.h>
#include <string.h>
#include <sysclib.h>

#include "irx_imports.h"

#include "mmce_cmds.h"
#include "mmce_fs.h"
#include "mmce_sio2.h"
#include "mmcedrv_config.h"
#include "sio2man_hook.h"

#include "module_debug.h"

#define MAJOR 0
#define MINOR 1

IRX_ID("mmcedrv", MAJOR, MINOR);

extern struct irx_export_table _exp_mmcedrv;

s64 mmcedrv_get_size(int fd)
{
    int res;
    s64 position = -1;

    u8 wrbuf[0xd];
    u8 rdbuf[0x16];

    wrbuf[0x0] = MMCE_ID;                       //Identifier
    wrbuf[0x1] = MMCE_CMD_FS_LSEEK64;           //Command
    wrbuf[0x2] = MMCE_RESERVED;                 //Reserved
    wrbuf[0x3] = fd;                            //File descriptor

    wrbuf[0x4] = 0;   //Offset
    wrbuf[0x5] = 0;
    wrbuf[0x6] = 0;
    wrbuf[0x7] = 0;
    wrbuf[0x8] = 0;
    wrbuf[0x9] = 0;
    wrbuf[0xa] = 0;
    wrbuf[0xb] = 0;

    wrbuf[0xc] = 2; //Whence SEEK_END

    //Packet #1: Command, file descriptor, offset, and whence
    mmce_sio2_lock();
    res = mmce_sio2_tx_rx_pio(0xd, 0x16, wrbuf, rdbuf, &timeout_1s);
    mmce_sio2_unlock();
    if (res == -1) {
        DPRINTF("%s ERROR: P1 - Timedout waiting for /ACK\n", __func__);
        return -1;
    }

    if (rdbuf[0x1] != MMCE_REPLY_CONST) {
        DPRINTF("%s ERROR: Invalid response from card. Got 0x%x, Expected 0x%x\n", __func__, rdbuf[0x1], MMCE_REPLY_CONST);
        return -1;
    }

    position  = (s64)rdbuf[0xd] << 56;
    position |= (s64)rdbuf[0xe] << 48;
    position |= (s64)rdbuf[0xf] << 40;
    position |= (s64)rdbuf[0x10] << 32;
    position |= (s64)rdbuf[0x11] << 24;
    position |= (s64)rdbuf[0x12] << 16;
    position |= (s64)rdbuf[0x13] << 8;
    position |= (s64)rdbuf[0x14];

    DPRINTF("%s position %lli\n", __func__, position);

    return position;
}

int mmcedrv_read_sector(int fd, u32 sector, u32 count, void *buffer)
{
    int res;
    int sectors_read = 0;

    u8 wrbuf[0xB];
    u8 rdbuf[0xB];

    DPRINTF("%s fd: %i, starting sector: %i, count: %i\n", __func__, fd, sector, count);

    wrbuf[0x0] = MMCE_ID;                   //Identifier
    wrbuf[0x1] = MMCE_CMD_FS_READ_SECTOR;   //Command
    wrbuf[0x2] = MMCE_RESERVED;             //Reserved
    wrbuf[0x3] = fd;                        //File Descriptor

    wrbuf[0x4] = (sector & 0x00FF0000) >> 16;
    wrbuf[0x5] = (sector & 0x0000FF00) >> 8;
    wrbuf[0x6] = (sector & 0x000000FF);

    wrbuf[0x7] = (count & 0x00FF0000) >> 16;
    wrbuf[0x8] = (count & 0x0000FF00) >> 8;
    wrbuf[0x9] = (count & 0x000000FF);

    wrbuf[0xA] = 0xff;

    mmce_sio2_lock();

    //Packet #1: Command, file descriptor, and size
    res = mmce_sio2_tx_rx_pio(0xB, 0xB, wrbuf, rdbuf, &timeout_2s);
    if (res == -1) {
        DPRINTF("%s ERROR: P1 - Timedout waiting for /ACK\n", __func__);
        mmce_sio2_unlock();
        return -1;
    }

    if (rdbuf[0x1] != MMCE_REPLY_CONST) {
        DPRINTF("%s ERROR: Invalid response from card. Got 0x%x, Expected 0x%x\n", __func__, rdbuf[0x1], MMCE_REPLY_CONST);
        mmce_sio2_unlock();
        return -1;
    }

    if (rdbuf[0x9] != 0x0) {
        DPRINTF("%s ERROR: P1 - Got bad return value from card, res %i\n", __func__, rdbuf[0x9]);
        mmce_sio2_unlock();
        return -1;
    }

    //Packet #2 - n: Read data
    res = mmce_sio2_rx_dma(buffer, (count * 2048));
    if (res == -1) {
        DPRINTF("%s ERROR: P2 - Timedout waiting for /ACK\n", __func__);
        mmce_sio2_unlock();
        return -1;
    }

    //Packet #n + 1: Sectors read
    res = mmce_sio2_tx_rx_pio(0x0, 0x5, wrbuf, rdbuf, &timeout_1s);
    if (res == -1) {
        DPRINTF("%s ERROR: P3 - Timedout waiting for /ACK\n", __func__);
        mmce_sio2_unlock();
        return -1;
    }

    mmce_sio2_unlock();

    sectors_read  = rdbuf[0x1] << 16;
    sectors_read |= rdbuf[0x2] << 8;
    sectors_read |= rdbuf[0x3];

    if (sectors_read != count) {
        DPRINTF("%s ERROR: Sectors read: %i, expected: %i\n", __func__, sectors_read, count);
    }

    return sectors_read;
}

int mmcedrv_read(int fd, int size, void *ptr)
{
    int res;
    int bytes_read;

    DPRINTF("%s: fd: %i, size: %i\n", __func__, fd, size);

    u8 wrbuf[0xA];
    u8 rdbuf[0xA];

    wrbuf[0x0] = MMCE_ID;                   //Identifier
    wrbuf[0x1] = MMCE_CMD_FS_READ;          //Command
    wrbuf[0x2] = MMCE_RESERVED;             //Reserved
    wrbuf[0x3] = 0x0;                       //Transfer mode (unused)
    wrbuf[0x4] = fd;                        //File Descriptor
    wrbuf[0x5] = (size & 0xFF000000) >> 24; //Size
    wrbuf[0x6] = (size & 0x00FF0000) >> 16;
    wrbuf[0x7] = (size & 0x0000FF00) >> 8;
    wrbuf[0x8] = (size & 0x000000FF);
    wrbuf[0x9] = 0xff;

    mmce_sio2_lock();

    //Packet #1: Command, file descriptor, and size
    res = mmce_sio2_tx_rx_pio(0xA, 0xA, wrbuf, rdbuf, &timeout_2s);
    if (res == -1) {
        DPRINTF("%s ERROR: P1 - Timedout waiting for /ACK\n", __func__);
        mmce_sio2_unlock();
        return -1;
    }

    if (rdbuf[0x1] != MMCE_REPLY_CONST) {
        DPRINTF("%s ERROR: Invalid response from card. Got 0x%x, Expected 0x%x\n", __func__, rdbuf[0x1], MMCE_REPLY_CONST);
        mmce_sio2_unlock();
        return -1;
    }

    if (rdbuf[0x9] != 0x0) {
        DPRINTF("%s ERROR: P1 - Got bad return value from card, res %i\n", __func__, rdbuf[0x9]);
        mmce_sio2_unlock();
        return -1;
    }

    //Packet #2 - n: Raw read data
    res = mmce_sio2_rx_mixed(ptr, size);
    if (res == -1) {
        DPRINTF("%s ERROR: P2 - Timedout waiting for /ACK\n", __func__);
        mmce_sio2_unlock();
        return -1;
    }

    //Packet #n + 1: Bytes read
    res = mmce_sio2_tx_rx_pio(0x0, 0x6, wrbuf, rdbuf, &timeout_1s);
    if (res == -1) {
        DPRINTF("%s ERROR: P3 - Timedout waiting for /ACK\n", __func__);
        mmce_sio2_unlock();
        return -1;
    }

    mmce_sio2_unlock();

    bytes_read  = rdbuf[0x1] << 24;
    bytes_read |= rdbuf[0x2] << 16;
    bytes_read |= rdbuf[0x3] << 8;
    bytes_read |= rdbuf[0x4];

    if (bytes_read != size) {
        DPRINTF("%s WARN: bytes read: %i, expected: %i\n", __func__, bytes_read, size);

        if (bytes_read > size)
            bytes_read = size;

    }

    return bytes_read;
}

int mmcedrv_write(int fd, int size, void *ptr)
{
    int res;
    int bytes_written;

    DPRINTF("%s: fd: %i, size: %i\n", __func__, fd, size);

    u8 wrbuf[0xA];
    u8 rdbuf[0xA];

    wrbuf[0x0] = MMCE_ID;                   //Identifier
    wrbuf[0x1] = MMCE_CMD_FS_WRITE;         //Command
    wrbuf[0x2] = MMCE_RESERVED;             //Reserved
    wrbuf[0x3] = 0x0;                       //Transfer mode (unimplemented)
    wrbuf[0x4] = fd;                        //File Descriptor
    wrbuf[0x5] = (size & 0xFF000000) >> 24;
    wrbuf[0x6] = (size & 0x00FF0000) >> 16;
    wrbuf[0x7] = (size & 0x0000FF00) >> 8;
    wrbuf[0x8] = (size & 0x000000FF);
    wrbuf[0x9] = 0xff;

    mmce_sio2_lock();

    //Packet #1: Command, file descriptor, and size
    res = mmce_sio2_tx_rx_pio(0xA, 0xA, wrbuf, rdbuf, &timeout_1s);
    if (res == -1) {
        DPRINTF("%s ERROR: P1 - Timedout waiting for /ACK\n", __func__);
        mmce_sio2_unlock();
        return -1;
    }

    if (rdbuf[0x1] != MMCE_REPLY_CONST) {
        DPRINTF("%s ERROR: Invalid response from card. Got 0x%x, Expected 0x%x\n", __func__, rdbuf[0x1], MMCE_REPLY_CONST);
        mmce_sio2_unlock();
        return -1;
    }

    if (rdbuf[0x9] != 0x0) {
        DPRINTF("%s ERROR: P1 - Got bad return value from card 0x%x\n", __func__, rdbuf[0x9]);
        mmce_sio2_unlock();
        return -1;
    }

    //Packet #2 - n: Raw write data
    res = mmce_sio2_tx_mixed(ptr, size);
    if (res == -1) {
        DPRINTF("%s ERROR: P2 - Timedout waiting for /ACK\n", __func__);
        mmce_sio2_unlock();
        return -1;
    }

    //Packets #n + 1: Bytes written
    res = mmce_sio2_tx_rx_pio(0x0, 0x6, wrbuf, rdbuf, &timeout_1s);
    if (res == -1) {
        DPRINTF("%s ERROR: P3 - Timedout waiting for /ACK\n", __func__);
        mmce_sio2_unlock();
        return -1;
    }

    mmce_sio2_unlock();

    bytes_written  = rdbuf[0x1] << 24;
    bytes_written |= rdbuf[0x2] << 16;
    bytes_written |= rdbuf[0x3] << 8;
    bytes_written |= rdbuf[0x4];

    if (bytes_written != size) {
        DPRINTF("%s bytes written: %i, expected: %i\n", __func__, bytes_written, size);
    }

    return bytes_written;
}

int mmcedrv_lseek(int fd, int offset, int whence)
{
    int res;
    int position = -1;

    DPRINTF("%s: fd: %i, offset: %i, whence: %i\n", __func__, fd, offset, whence);

    u8 wrbuf[0x9];
    u8 rdbuf[0xe];

    wrbuf[0x0] = MMCE_ID;                       //Identifier
    wrbuf[0x1] = MMCE_CMD_FS_LSEEK;             //Command
    wrbuf[0x2] = MMCE_RESERVED;                 //Reserved
    wrbuf[0x3] = fd;                            //File descriptor
    wrbuf[0x4] = (offset & 0xFF000000) >> 24;   //Offset
    wrbuf[0x5] = (offset & 0x00FF0000) >> 16;
    wrbuf[0x6] = (offset & 0x0000FF00) >> 8;
    wrbuf[0x7] = (offset & 0x000000FF);
    wrbuf[0x8] = (u8)(whence);                  //Whence

    //Packet #1: Command, file descriptor, offset, and whence
    mmce_sio2_lock();
    res = mmce_sio2_tx_rx_pio(0x9, 0xe, wrbuf, rdbuf, &timeout_1s);
    mmce_sio2_unlock();

    if (res == -1) {
        DPRINTF("%s ERROR: P1 - Timedout waiting for /ACK\n", __func__);
        return -1;
    }

    if (rdbuf[0x1] != MMCE_REPLY_CONST) {
        DPRINTF("%s ERROR: Invalid response from card. Got 0x%x, Expected 0x%x\n", __func__, rdbuf[0x1], MMCE_REPLY_CONST);
        return -1;
    }

    position  = rdbuf[0x9] << 24;
    position |= rdbuf[0xa] << 16;
    position |= rdbuf[0xb] << 8;
    position |= rdbuf[0xc];

    DPRINTF("%s position %i\n", __func__, position);

    return position;
}

//For OPL, called through CDVDMAN Device
void mmcedrv_config_set(int setting, int value)
{
    switch (setting) {
        case MMCEDRV_SETTING_PORT:
            if (value == 2 || value == 3)
                mmce_sio2_set_port(value);
            else
                DPRINTF("Invalid port setting: %i\n", value);
        break;

        case MMCEDRV_SETTING_ACK_WAIT_CYCLES:
            if (value < 5) {
                mmce_sio2_update_ack_wait_cycles(value);
            }
        break;

        case MMCEDRV_SETTING_USE_ALARMS:
            mmce_sio2_set_use_alarm(value);
        break;

        default:
        break;
    }
}

int __start(int argc, char *argv[], void *startaddr, ModuleInfo_t *mi)
{
    int rv;

    DPRINTF("Multipurpose Memory Card Emulator Driver (MMCEDRV) v%d.%d by the MMCE team\n", MAJOR, MINOR);

    //Install hooks
    rv = mmce_sio2_init();
    if (rv != 0) {
        DPRINTF("mmce_sio2_init failed, rv %i\n", rv);
        return MODULE_NO_RESIDENT_END;
    }

    //Register exports
    if (RegisterLibraryEntries(&_exp_mmcedrv) != 0) {
        DPRINTF("ERROR: library already registered\n");
        return MODULE_NO_RESIDENT_END;
    }

    // If modload has certain flags set indicating new version,
    // set the unloadable flag
    if (mi && ((mi->newflags & 2) != 0))
        mi->newflags |= 0x10;
    return MODULE_RESIDENT_END;
}

int __stop(int argc, char *argv[], void *startaddr, ModuleInfo_t *mi)
{
    DPRINTF("Unloading module\n");

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
