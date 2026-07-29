#ifndef MMCE_CMDS_H
#define MMCE_CMDS_H

#include <tamtypes.h>

#define MMCE_ID 0x8B
#define MMCE_RESERVED 0xFF
#define MMCE_REPLY_CONST 0xAA

enum mmce_cmds {
    MMCE_CMD_PING = 0x1,
    MMCE_CMD_GET_STATUS,
    MMCE_CMD_GET_CARD,
    MMCE_CMD_SET_CARD,
    MMCE_CMD_GET_CHANNEL,
    MMCE_CMD_SET_CHANNEL,
    MMCE_CMD_GET_GAMEID,
    MMCE_CMD_SET_GAMEID,
    MMCE_CMD_RESET,
    MMCE_CMD_SET_CARD_CHANNEL,
    MMCE_SETTINGS_ACK_WAIT_CYCLES,
    MMCE_SETTINGS_SET_ALARMS,
};

enum mmce_cmds_fs {
    MMCE_CMD_FS_OPEN = 0x40,
    MMCE_CMD_FS_CLOSE = 0x41,
    MMCE_CMD_FS_READ = 0x42,
    MMCE_CMD_FS_WRITE = 0x43,
    MMCE_CMD_FS_LSEEK = 0x44,
    MMCE_CMD_FS_IOCTL = 0x45,
    MMCE_CMD_FS_REMOVE = 0x46,
    MMCE_CMD_FS_MKDIR = 0x47,
    MMCE_CMD_FS_RMDIR = 0x48,
    MMCE_CMD_FS_DOPEN = 0x49,
    MMCE_CMD_FS_DCLOSE = 0x4a,
    MMCE_CMD_FS_DREAD = 0x4b,
    MMCE_CMD_FS_GETSTAT = 0x4c,
    MMCE_CMD_FS_CHSTAT = 0x4d,
    MMCE_CMD_FS_RENAME = 0x4e,
    MMCE_CMD_FS_LSEEK64 = 0x53,
    MMCE_CMD_FS_READ_SECTOR = 0x58,
};

enum mmce_cmds_ioctl {
    MMCE_CMD_IOCTL_GET_FD = 0x80,
};

//Called through devctl
int mmce_cmd_ping(void);
int mmce_cmd_get_status(void);
int mmce_cmd_get_card(void);
int mmce_cmd_set_card(u8 type, u8 mode, u16 num);
int mmce_cmd_get_channel(void);
int mmce_cmd_set_channel(u8 mode, u16 num);
int mmce_cmd_get_gameid(void *ptr);
int mmce_cmd_set_gameid(void *ptr);
int mmce_cmd_reset(void);
int mmce_cmd_set_card_channel(u8 type, u16 card, u16 chan);

#endif
