#include <stdio.h>
#include <sysclib.h>
#include <tamtypes.h>

#include "mmce_sio2.h"
#include "mmce_cmds.h"

#include "module_debug.h"

int mmce_cmd_ping(void)
{
    int res;

    u8 wrbuf[0x7];
    u8 rdbuf[0x7];

    wrbuf[0x0] = MMCE_ID;          //identifier
    wrbuf[0x1] = MMCE_CMD_PING;    //command
    wrbuf[0x2] = MMCE_RESERVED;    //reserved byte
    wrbuf[0x3] = 0;
    wrbuf[0x4] = 0;
    wrbuf[0x5] = 0;
    wrbuf[0x6] = 0;

    mmce_sio2_lock();
    res = mmce_sio2_tx_rx_pio(sizeof(wrbuf), sizeof(rdbuf), wrbuf, rdbuf, &timeout_200ms);
    mmce_sio2_unlock();
    if (res == -1) {
        DPRINTF("%s ERROR: Timedout waiting for /ACK\n", __func__);
        return -1;
    }

    //bits 24-16: protocol ver
    //bits 16-8: product id
    //bits 8-0: revision id
    if (rdbuf[0x1] == MMCE_REPLY_CONST) {
        res = rdbuf[0x3] << 16 | rdbuf[0x4] << 8 | rdbuf[0x5];
    } else {
        DPRINTF("%s ERROR: Invalid response from card. Got 0x%x, Expected 0x%x\n", __func__, rdbuf[0x1], MMCE_REPLY_CONST);
        res = -1;
    }

    return res;
}

//TODO:
int mmce_cmd_get_status(void)
{
    int res;

    u8 wrbuf[0x3];
    u8 rdbuf[0x6];

    wrbuf[0x0] = MMCE_ID;                //identifier
    wrbuf[0x1] = MMCE_CMD_GET_STATUS;    //command
    wrbuf[0x2] = MMCE_RESERVED;          //reserved byte

    mmce_sio2_lock();
    res = mmce_sio2_tx_rx_pio(sizeof(wrbuf), sizeof(rdbuf), wrbuf, rdbuf, &timeout_1s);
    mmce_sio2_unlock();
    if (res == -1) {
        DPRINTF("%s ERROR: Timedout waiting for /ACK\n", __func__);
        return -1;
    }

    if (rdbuf[0x1] == MMCE_REPLY_CONST) {
        res = rdbuf[0x3] << 8 | rdbuf[0x4];
    } else {
        DPRINTF("%s ERROR: Invalid response from card. Got 0x%x, Expected 0x%x\n", __func__, rdbuf[0x1], MMCE_REPLY_CONST);
        res = -1;
    }

    return res;
}

int mmce_cmd_get_card(void)
{
    int res;

    u8 wrbuf[0x3];
    u8 rdbuf[0x6];

    wrbuf[0x0] = MMCE_ID;            //identifier
    wrbuf[0x1] = MMCE_CMD_GET_CARD;  //command
    wrbuf[0x2] = MMCE_RESERVED;      //reserved byte

    mmce_sio2_lock();
    res = mmce_sio2_tx_rx_pio(sizeof(wrbuf), sizeof(rdbuf), wrbuf, rdbuf, &timeout_1s);
    mmce_sio2_unlock();
    if (res == -1) {
        DPRINTF("%s ERROR: Timedout waiting for /ACK\n", __func__);
        return -1;
    }

    if (rdbuf[0x1] == MMCE_REPLY_CONST) {
        res = rdbuf[0x3] << 8 | rdbuf[0x4];
    } else {
        DPRINTF("%s ERROR: Invalid response from card. Got 0x%x, Expected 0x%x\n", __func__, rdbuf[0x1], MMCE_REPLY_CONST);
        res = -1;
    }

    return res;
}

int mmce_cmd_set_card(u8 type, u8 mode, u16 num)
{
    int res;

    u8 wrbuf[0x8];
    u8 rdbuf[0x2];

    wrbuf[0x0] = MMCE_ID;           //identifier
    wrbuf[0x1] = MMCE_CMD_SET_CARD; //command
    wrbuf[0x2] = MMCE_RESERVED;     //reserved byte
    wrbuf[0x3] = type;              //card type (0 = regular, 1 = boot)
    wrbuf[0x4] = mode;              //set mode (num, next, prev)
    wrbuf[0x5] = num >> 8;          //card number upper 8 bits
    wrbuf[0x6] = num & 0xFF;        //card number lower 8 bits

    mmce_sio2_lock();
    res = mmce_sio2_tx_rx_pio(sizeof(wrbuf), sizeof(rdbuf), wrbuf, rdbuf, &timeout_1s);
    mmce_sio2_unlock();
    if (res == -1) {
        DPRINTF("%s ERROR: Timedout waiting for /ACK\n", __func__);
        return -1;
    }

    if (rdbuf[0x1] != MMCE_REPLY_CONST) {
        DPRINTF("%s ERROR: Invalid response from card. Got 0x%x, Expected 0x%x\n", __func__, rdbuf[0x1], MMCE_REPLY_CONST);
        return -1;
    }

    return 0;
}

int mmce_cmd_get_channel(void)
{
    int res;

    u8 wrbuf[0x3];
    u8 rdbuf[0x6];

    wrbuf[0x0] = MMCE_ID;                //identifier
    wrbuf[0x1] = MMCE_CMD_GET_CHANNEL;   //command
    wrbuf[0x2] = MMCE_RESERVED;          //reserved byte

    mmce_sio2_lock();
    res = mmce_sio2_tx_rx_pio(sizeof(wrbuf), sizeof(rdbuf), wrbuf, rdbuf, &timeout_1s);
    mmce_sio2_unlock();
    if (res == -1) {
        DPRINTF("%s ERROR: Timedout waiting for /ACK\n", __func__);
        return -1;
    }

    if (rdbuf[0x1] == MMCE_REPLY_CONST) {
        res = rdbuf[0x3] << 8 | rdbuf[0x4];
    } else {
        DPRINTF("%s ERROR: Invalid response from card. Got 0x%x, Expected 0x%x\n", __func__, rdbuf[0x1], MMCE_REPLY_CONST);
        res = -1;
    }

    return res;
}

int mmce_cmd_set_channel(u8 mode, u16 num)
{
    int res;

    u8 wrbuf[0x7];
    u8 rdbuf[0x2];

    wrbuf[0x0] = MMCE_ID;               //identifier
    wrbuf[0x1] = MMCE_CMD_SET_CHANNEL;  //command
    wrbuf[0x2] = MMCE_RESERVED;         //reserved byte
    wrbuf[0x3] = mode;                  //set mode (num, next, prev)
    wrbuf[0x4] = num >> 8;              //channel number upper 8 bits
    wrbuf[0x5] = num & 0xFF;            //channel number lower 8 bits

    mmce_sio2_lock();
    res = mmce_sio2_tx_rx_pio(sizeof(wrbuf), sizeof(rdbuf), wrbuf, rdbuf, &timeout_1s);
    mmce_sio2_unlock();
    if (res == -1) {
        DPRINTF("%s ERROR: Timedout waiting for /ACK\n", __func__);
        return -1;
    }

    if (rdbuf[0x1] != MMCE_REPLY_CONST) {
        DPRINTF("%s ERROR: Invalid response from card. Got 0x%x, Expected 0x%x\n", __func__, rdbuf[0x1], MMCE_REPLY_CONST);
        return -1;
    }

    return 0;
}

int mmce_cmd_get_gameid(void *ptr)
{
    int res;

    u8 wrbuf[0x3];
    u8 rdbuf[0xFF]; //fixed packet size of 255 bytes

    wrbuf[0x0] = MMCE_ID;             //identifier
    wrbuf[0x1] = MMCE_CMD_GET_GAMEID; //command
    wrbuf[0x2] = MMCE_RESERVED;       //reserved byte

    mmce_sio2_lock();
    res = mmce_sio2_tx_rx_pio(sizeof(wrbuf), sizeof(rdbuf), wrbuf, rdbuf, &timeout_1s);
    mmce_sio2_unlock();
    if (res == -1) {
        DPRINTF("%s ERROR: Timedout waiting for /ACK\n", __func__);
        return -1;
    }

    if (rdbuf[0x1] == MMCE_REPLY_CONST) {
        char* str = &rdbuf[0x4];
        strcpy(ptr, str);
        res = 0;
    } else {
        DPRINTF("%s ERROR: Invalid response from card. Got 0x%x, Expected 0x%x\n", __func__, rdbuf[0x1], MMCE_REPLY_CONST);
        res = -1;
    }

    return res;
}

int mmce_cmd_set_gameid(void *ptr)
{
    int res;

    u8 len = strlen(ptr) + 1;

    u8 wrbuf[0xFF];
    u8 rdbuf[0x2];

    wrbuf[0x0] = MMCE_ID;             //identifier
    wrbuf[0x1] = MMCE_CMD_SET_GAMEID; //command
    wrbuf[0x2] = MMCE_RESERVED;       //reserved byte
    wrbuf[0x3] = len;                 //gameid length

    char *str = &wrbuf[0x4];
    strcpy(str, ptr);

    mmce_sio2_lock();
    res = mmce_sio2_tx_rx_pio(len + 5, sizeof(rdbuf), wrbuf, rdbuf, &timeout_1s);
    mmce_sio2_unlock();
    if (res == -1) {
        DPRINTF("%s ERROR: Timedout waiting for /ACK\n", __func__);
        return -1;
    }

    if (rdbuf[0x1] != MMCE_REPLY_CONST) {
        DPRINTF("%s ERROR: Invalid response from card. Got 0x%x, Expected 0x%x\n", __func__, rdbuf[0x1], MMCE_REPLY_CONST);
        return -1;
    }

    return 0;
}

int mmce_cmd_reset(void)
{
    int res;

    u8 wrbuf[0x5];
    u8 rdbuf[0x5];

    wrbuf[0x0] = MMCE_ID;           //identifier
    wrbuf[0x1] = MMCE_CMD_RESET;    //command
    wrbuf[0x2] = MMCE_RESERVED;     //reserved byte

    mmce_sio2_lock();
    res = mmce_sio2_tx_rx_pio(sizeof(wrbuf), sizeof(rdbuf), wrbuf, rdbuf, &timeout_1s);
    mmce_sio2_unlock();
    if (res == -1) {
        DPRINTF("%s ERROR: Timedout waiting for /ACK\n", __func__);
        return -1;
    }

    if (rdbuf[0x1] != MMCE_REPLY_CONST) {
        DPRINTF("%s ERROR: Invalid response from card. Got 0x%x, Expected 0x%x\n", __func__, rdbuf[0x1], MMCE_REPLY_CONST);
        return -1;
    }

    return 0;
}

int mmce_cmd_set_card_channel(u8 type, u16 card, u16 chan)
{
    int res;

    u8 wrbuf[0x9];
    u8 rdbuf[0x2];

    wrbuf[0x0] = MMCE_ID;           //identifier
    wrbuf[0x1] = MMCE_CMD_SET_CARD_CHANNEL; //command
    wrbuf[0x2] = MMCE_RESERVED;     //reserved byte
    wrbuf[0x3] = type;              //card type (0 = regular, 1 = boot)
    wrbuf[0x4] = card >> 8;          //card number upper 8 bits
    wrbuf[0x5] = card & 0xFF;        //card number lower 8 bits
    wrbuf[0x6] = chan >> 8;          //channel number upper 8 bits
    wrbuf[0x7] = chan & 0xFF;        //channel number lower 8 bits
    wrbuf[0x8] = 0xFF;               //termination byte

    mmce_sio2_lock();
    res = mmce_sio2_tx_rx_pio(sizeof(wrbuf), sizeof(rdbuf), wrbuf, rdbuf, &timeout_1s);
    mmce_sio2_unlock();
    if (res == -1) {
        DPRINTF("%s ERROR: Timedout waiting for /ACK\n", __func__);
        return -1;
    }

    if (rdbuf[0x1] != MMCE_REPLY_CONST) {
        DPRINTF("%s ERROR: Invalid response from card. Got 0x%x, Expected 0x%x\n", __func__, rdbuf[0x1], MMCE_REPLY_CONST);
        return -1;
    }

    return 0;
}
