#include <string.h>
#include <sysclib.h>
#include <tamtypes.h>

#include "mmce_cmds.h"
#include "sio2regs.h"
#include "irx_imports.h"

static u32 mmce_sio2_port_ctrl1;
static u32 mmce_sio2_port_ctrl2;

static inline void delay_us(int us)
{
    for (int i = 0; i < us; i++) {
        for (int j = 0; j < 36; j++) {
            asm volatile("nop");
        }
    }
}

static int mmce_sio2_tx_rx_pio(int port, u8 tx_size, u8 rx_size, u8 *tx_buf, u8 *rx_buf)
{
    //Reset SIO2 + FIFO pointers, disable interrupts
    inl_sio2_ctrl_set(0xBC);

    //Add transfer to queue
    inl_sio2_regN_set(0,
                        TR_CTRL_PORT_NR(port)       |
                        TR_CTRL_PAUSE(0)            |
                        TR_CTRL_TX_MODE_PIO_DMA(0)  |
                        TR_CTRL_RX_MODE_PIO_DMA(0)  |
                        TR_CTRL_NORMAL_TR(1)        |
                        TR_CTRL_SPECIAL_TR(0)       |
                        TR_CTRL_BAUD_DIV(0)         |
                        TR_CTRL_WAIT_ACK_FOREVER(0) |
                        TR_CTRL_TX_DATA_SZ(tx_size) |
                        TR_CTRL_RX_DATA_SZ(rx_size));

    //Copy data to TX FIFO
    for (int i = 0; i < tx_size; i++) {
        inl_sio2_data_out(tx_buf[i]);
    }

    //Start transfer
    inl_sio2_ctrl_set(0xB1);

    //Wait for transfer to complete
    while ((inl_sio2_stat6c_get() & (1 << 12)) == 0) {
        //Polling stat6c too frequently on FATs can cause the SIO2 freeze
        delay_us(5);
    }

    //Timeout detected
    if ((inl_sio2_stat6c_get() & 0x8000) != 0)
        return -1;

    //Copy data out of RX FIFO
    for (int i = 0; i < rx_size; i++) {
        rx_buf[i] = inl_sio2_data_in();
    }

    return 0;
}

static int cmd_get_status(int port)
{
    int res;

    u8 wrbuf[0x3];
    u8 rdbuf[0x6];

    wrbuf[0x0] = MMCE_ID;                //identifier
    wrbuf[0x1] = MMCE_CMD_GET_STATUS;    //command
    wrbuf[0x2] = MMCE_RESERVED;          //reserved byte

    res = mmce_sio2_tx_rx_pio(port, sizeof(wrbuf), sizeof(rdbuf), wrbuf, rdbuf);
    if (res == -1)
        return -1;

    if (rdbuf[0x1] == MMCE_REPLY_CONST)
        res = rdbuf[0x3] << 8 | rdbuf[0x4];
    else
        res = -1;

    return res;
}

static int cmd_set_card(int port, u8 type, u8 mode, u16 num)
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

    res = mmce_sio2_tx_rx_pio(port, sizeof(wrbuf), sizeof(rdbuf), wrbuf, rdbuf);
    if (res == -1)
        return -1;

    if (rdbuf[0x1] != MMCE_REPLY_CONST)
        return -1;

    return 0;
}

int _start(int argc, char *argv[])
{
    int res;
    int slot[2] = {0};

    if (argc < 1)
        return MODULE_NO_RESIDENT_END;

    if (argv[1][0] == '1')
        slot[0] = 1;

    if (argv[1][1] == '1')
        slot[1] = 1;

    mmce_sio2_port_ctrl1 =
        PCTRL0_ATT_LOW_PER(0x5)      |
        PCTRL0_ATT_MIN_HIGH_PER(0x0) |
        PCTRL0_BAUD0_DIV(0x2)        |
        PCTRL0_BAUD1_DIV(0xff);

    mmce_sio2_port_ctrl2 =
        PCTRL1_ACK_TIMEOUT_AFTER(0xffff)        |
        PCTRL1_WAIT_CYCLES_AFTER_ACK_LOW(0x5)   |
        PCTRL1_UNK24(0x0)                       |
        PCTRL1_IF_MODE_SPI_DIFF(0x0);

    u32 sio2_state = inl_sio2_ctrl_get();

    for (int i = 0; i < 2; i++) {
        if (slot[i] == 1) {
            //Set port ctrls
            inl_sio2_portN_ctrl1_set(i + 2, mmce_sio2_port_ctrl1);
            inl_sio2_portN_ctrl2_set(i + 2, mmce_sio2_port_ctrl2);

            //Send set card BOOTCARD
            res = cmd_set_card(i + 2, 1, 0, 0);
            if (res != -1) {
                //Poll for ~7.5s
                for (int j = 0; j < 15; j++) {

                    delay_us(500000); //500ms
                    res = cmd_get_status(i + 2);

                    //Not busy, switch done
                    if ((res & 1) == 0)
                        break;
                }
            }
        }
    }

    //restore SIO2 state + reset
    inl_sio2_ctrl_set(sio2_state | 0xc);

    return MODULE_NO_RESIDENT_END;
}
