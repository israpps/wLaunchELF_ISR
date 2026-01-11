#ifndef MMCE_CMD
#define MMCE_CMD


#define VMC_NORMAL    0
#define VMC_BOOTCARD  1

#define SET_SPECIFIC  0
#define SET_NEXT_CARD 1
#define SET_PREV_CARD 2
#define BUILD_SETCARD(param, type, mode, num) param  = type << 24; param |= mode << 16; param |= num;
#define BUILD_SETCHANNEL(param, mode, num) param  = mode << 16; param |= num;
enum mmceman_cmds {
    MMCE_CMD_PING = 0x1,
    MMCE_CMD_GET_STATUS,
    MMCE_CMD_GET_CARD,
    MMCE_CMD_SET_CARD,
    MMCE_CMD_GET_CHANNEL,
    MMCE_CMD_SET_CHANNEL,
    MMCE_CMD_GET_GAMEID,
    MMCE_CMD_SET_GAMEID,
    MMCE_CMD_RESET,
    MMCE_SETTINGS_ACK_WAIT_CYCLES,
    MMCE_SETTINGS_SET_ALARMS,
};

#define MAX_GAMEID_LEN 256

#endif