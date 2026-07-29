
#include <dmacman.h>
#include <sio2regs.h>
#include <thbase.h>
#include <thevent.h>
#include <intrman.h>

#include "ioplib.h"
#include "irx_imports.h"
#include "iop_regs.h"

#include "mmce_sio2.h"
#include "sio2man_hook.h"

#include "module_debug.h"

#define EF_SIO2_INTR_COMPLETE	    0x00000200
#define EF_SIO2_TRANSFER_TIMEOUT    0x00000400

static int mmce_port;

//Port ctrl settings used for all transfers
static u32 mmce_sio2_port_ctrl1;
static u32 mmce_sio2_port_ctrl2;

static u32 sio2_save_ctrl;
static int event_flag = -1;

//Replacement intr handler
static int (*mmce_sio2_intr_handler_ptr)(void *arg);
static void *mmce_sio2_intr_arg_ptr;

iop_sys_clock_t timeout_200ms;
iop_sys_clock_t timeout_1s;
iop_sys_clock_t timeout_2s;

u8 mmce_sio2_use_alarm = 1;

int mmce_sio2_intr_handler(void *arg)
{
	int ef = *(int *)arg;

	inl_sio2_stat_set(0x3);

	iSetEventFlag(ef, EF_SIO2_INTR_COMPLETE);

	return 1;
}

unsigned int mmce_sio2_timeout_handler(void *arg)
{
    int ef = *(int *)arg;
    iSetEventFlag(ef, EF_SIO2_TRANSFER_TIMEOUT);
    return 0;
}

int mmce_sio2_init()
{
    int rv;

    //Install necessary hooks
    rv = sio2man_hook_init();
    if (rv == -1) {
        DPRINTF("Failed to init sio2man hook\n");
        return -1;
    }

    //Create event flag for intr
	iop_event_t event;

	event.attr = 2;
	event.option = 0;
	event.bits = 0;

	event_flag = CreateEventFlag(&event);
    if (event_flag < 0) {
        DPRINTF("Failed to create event flag\n");
        sio2man_hook_deinit();
        return -2;
    }

    //Assign intr pointers
    mmce_sio2_intr_handler_ptr = &mmce_sio2_intr_handler;
    mmce_sio2_intr_arg_ptr = &event_flag;

    //Ensure IOP DMA channels are enabled
    sceSetDMAPriority(IOP_DMAC_SIO2in, 3);
    sceSetDMAPriority(IOP_DMAC_SIO2out, 3);
    sceEnableDMAChannel(IOP_DMAC_SIO2in);
    sceEnableDMAChannel(IOP_DMAC_SIO2out);

    //Ensure SIO2 intrs are enabled
    EnableIntr(IOP_IRQ_SIO2);

    //200ms
    timeout_200ms.hi = 0x0;
    timeout_200ms.lo = 0x708000;

    //1s
    timeout_1s.hi = 0x0;
    timeout_1s.lo = 0x2328000;

    //2s
    timeout_2s.hi = 0x0;
    timeout_2s.lo = 0x4650000;

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

    return 0;
}

void mmce_sio2_deinit()
{
    sio2man_hook_deinit();
    DeleteEventFlag(event_flag);
}

void mmce_sio2_lock()
{
    //Lock sio2man driver so we can use it exclusively
    sio2man_hook_sio2_lock();

    sio2_save_ctrl = inl_sio2_ctrl_get();

    //Swap SIO2MAN's intr handler with ours
    sio2man_hook_sio2_set_intr_handler(mmce_sio2_intr_handler_ptr, mmce_sio2_intr_arg_ptr);

    //Copy port ctrl settings to SIO2 registers
    inl_sio2_portN_ctrl1_set(mmce_port, mmce_sio2_port_ctrl1);
    inl_sio2_portN_ctrl2_set(mmce_port, mmce_sio2_port_ctrl2);
}

void mmce_sio2_unlock()
{
    //Swap our intr handler with SIO2MAN's intr handler
    sio2man_hook_sio2_set_intr_handler(NULL, NULL);

    //Restore ctrl state, and reset STATE + FIFOS
    inl_sio2_ctrl_set(sio2_save_ctrl | 0xc);

    //Unlock sio2man driver
    sio2man_hook_sio2_unlock();
}

void mmce_sio2_set_port(int port)
{
    mmce_port = port;
}

int mmce_sio2_get_port()
{
    return mmce_port;
}

void mmce_sio2_update_ack_wait_cycles(int cycles)
{
    DPRINTF("mmceman: setting cycles to: 0x%x\n", cycles);

    mmce_sio2_port_ctrl2 =
        PCTRL1_ACK_TIMEOUT_AFTER(0xffff)        |
        PCTRL1_WAIT_CYCLES_AFTER_ACK_LOW(cycles)|
        PCTRL1_UNK24(0x0)                       |
        PCTRL1_IF_MODE_SPI_DIFF(0x0);
}

void mmce_sio2_set_use_alarm(int value)
{
    mmce_sio2_use_alarm = value;
}

int mmce_sio2_tx_rx_pio(u8 tx_size, u8 rx_size, u8 *tx_buf, u8 *rx_buf, iop_sys_clock_t *timeout)
{
    u32 resbits;

    //Reset SIO2 + FIFO pointers, enable interrupts
    inl_sio2_ctrl_set(0x3ac);

    //Add transfer to queue
    inl_sio2_regN_set(0,
                        TR_CTRL_PORT_NR(mmce_port)  |
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
    inl_sio2_ctrl_set(0x3a1);

    //Set timeout alarm
    SetAlarm(timeout, mmce_sio2_timeout_handler, &event_flag);

    //Wait for completion or timeout
    WaitEventFlag(event_flag, EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE, 1, &resbits);
    if (resbits & EF_SIO2_TRANSFER_TIMEOUT) {
        DPRINTF("Transfer timed out, resetting SIO2\n");
        inl_sio2_ctrl_set(0x3ac);   //Reset SIO2
        ClearEventFlag(event_flag, ~(EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE));
        return -1;
    }

    //Cancel timeout alarm
    CancelAlarm(mmce_sio2_timeout_handler,  &event_flag);

    //Clear flags
    ClearEventFlag(event_flag, ~(EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE));

    //Copy data out of RX FIFO
    for (int i = 0; i < rx_size; i++) {
        rx_buf[i] = inl_sio2_data_in();
    }

    return 0;
}

//Simplified read for sectors / MMCEDRV
int mmce_sio2_rx_dma(u8 *buffer, u32 size)
{
    u32 resbits;

    //dma element count (round down)
    u32 elements = size / 256;

    u32 elements_do = 0;
    u32 elements_done = 0;

    //used for all elements 256 bytes in size
    u32 dma_element = TR_CTRL_PORT_NR(mmce_port)      |
                      TR_CTRL_PAUSE(0)                |
                      TR_CTRL_TX_MODE_PIO_DMA(0)      |
                      TR_CTRL_RX_MODE_PIO_DMA(1)      |
                      TR_CTRL_NORMAL_TR(1)            |
                      TR_CTRL_SPECIAL_TR(0)           |
                      TR_CTRL_BAUD_DIV(0)             |
                      TR_CTRL_WAIT_ACK_FOREVER(0)     |
                      TR_CTRL_TX_DATA_SZ(0)           |
                      TR_CTRL_RX_DATA_SZ(256);

   while (elements != 0) {
        //Reset SIO2 + FIFO pointers, enable interrupts
        inl_sio2_ctrl_set(0x3ac);

        elements_do = elements > 16 ? 16 : elements;

        //Copy DMA transfer elements to SIO2 registers
        for (int i = 0; i < elements_do; i++) {
            inl_sio2_regN_set(i, dma_element);
        }

        //Start DMA transfer
        sceSetSliceDMA(IOP_DMAC_SIO2out, &buffer[elements_done * 256], 0x100 >> 2, elements_do, DMAC_TO_MEM);
        sceStartDMA(IOP_DMAC_SIO2out);

        //Start the transfer
        inl_sio2_ctrl_set(0x3a1);

        //Set timeout alarm
        if (mmce_sio2_use_alarm)
            SetAlarm(&timeout_2s, mmce_sio2_timeout_handler, &event_flag);

        //Wait for completion or timeout
        WaitEventFlag(event_flag, EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE, 1, &resbits);
        if (resbits & EF_SIO2_TRANSFER_TIMEOUT) {
            DPRINTF("Transfer timed out, resetting SIO2\n");
            inl_sio2_ctrl_set(0x3ac); //Reset SIO2
            ClearEventFlag(event_flag, ~(EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE));
            return -1;
        }

        //Cancel timeout alarm
        if (mmce_sio2_use_alarm)
            CancelAlarm(mmce_sio2_timeout_handler,  &event_flag);

        //Clear flags
        ClearEventFlag(event_flag, ~(EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE));

        elements -= elements_do;
        elements_done += elements_do;
    }

    return 0;
}

//Mixed DMA / PIO RX transfer
int mmce_sio2_rx_mixed(u8 *buffer, u32 size)
{
    u32 resbits;

    //dma element count (round down)
    u32 elements = size / 256;

    //remainder < 256
    u32 pio_size = size - (elements * 256);

    u32 bytes_done = 0;
    u32 elements_do = 0;

    u32 offset = 0;
    u32 offset_pio = 0;

    //used for all elements 256 bytes in size
    u32 dma_element = TR_CTRL_PORT_NR(mmce_port)      |
                      TR_CTRL_PAUSE(0)                |
                      TR_CTRL_TX_MODE_PIO_DMA(0)      |
                      TR_CTRL_RX_MODE_PIO_DMA(1)      |
                      TR_CTRL_NORMAL_TR(1)            |
                      TR_CTRL_SPECIAL_TR(0)           |
                      TR_CTRL_BAUD_DIV(0)             |
                      TR_CTRL_WAIT_ACK_FOREVER(0)     |
                      TR_CTRL_TX_DATA_SZ(0)           |
                      TR_CTRL_RX_DATA_SZ(256);

    //used for all elements less than 256 bytes in size
    u32 pio_element = TR_CTRL_PORT_NR(mmce_port)      |
                      TR_CTRL_PAUSE(0)                |
                      TR_CTRL_TX_MODE_PIO_DMA(0)      |
                      TR_CTRL_RX_MODE_PIO_DMA(0)      |
                      TR_CTRL_NORMAL_TR(1)            |
                      TR_CTRL_SPECIAL_TR(0)           |
                      TR_CTRL_BAUD_DIV(0)             |
                      TR_CTRL_WAIT_ACK_FOREVER(0)     |
                      TR_CTRL_TX_DATA_SZ(0)           |
                      TR_CTRL_RX_DATA_SZ(pio_size);

    while (bytes_done < size) {
        //Reset SIO2 + FIFO pointers, enable interrupts
        inl_sio2_ctrl_set(0x3ac);

        //Used for DMA
        offset = bytes_done;

        elements_do = elements > 16 ? 16 : elements;

        //Copy DMA transfer elements to SIO2 registers
        for (int i = 0; i < elements_do; i++) {
            inl_sio2_regN_set(i, dma_element);
        }

        bytes_done += elements_do * 256;

        //Copy PIO element / term transfer queue if needed
        if (elements_do < 16) {
            if (pio_size != 0x0) {
                inl_sio2_regN_set(elements_do, pio_element);

                offset_pio = bytes_done;
                bytes_done += pio_size;
            }
        }

        //Start DMA transfer if needed
        if (elements_do != 0) {
            sceSetSliceDMA(IOP_DMAC_SIO2out, &buffer[offset], 0x100 >> 2, elements_do, DMAC_TO_MEM);
            sceStartDMA(IOP_DMAC_SIO2out);
        }

        //Start the transfer
        inl_sio2_ctrl_set(0x3a1);

        //Set timeout alarm
        if (mmce_sio2_use_alarm)
            SetAlarm(&timeout_2s, mmce_sio2_timeout_handler, &event_flag);

        //Wait for completion or timeout
        WaitEventFlag(event_flag, EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE, 1, &resbits);
        if (resbits & EF_SIO2_TRANSFER_TIMEOUT) {
            DPRINTF("Detected transfer timeout, attempting to reset SIO2\n");
            inl_sio2_ctrl_set(0x3ac); //Reset SIO2
            ClearEventFlag(event_flag, ~(EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE));
            return -1;
        }

        //Cancel timeout alarm
        if (mmce_sio2_use_alarm)
            CancelAlarm(mmce_sio2_timeout_handler,  &event_flag);

        //Clear flags
        ClearEventFlag(event_flag, ~(EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE));

        elements -= elements_do;
    }

    //Copy data from RX FIFO
    if (pio_size != 0x0) {
        for (int i = 0; i < pio_size; i++) {
            buffer[offset_pio + i] = inl_sio2_data_in();
        }
    }

    return 0;
}

int mmce_sio2_tx_mixed(u8 *buffer, u32 size)
{
    int res;
    u32 resbits;

    //dma element count (round down)
    u32 elements = size / 256;

    //remainder < 256
    u32 pio_size = size - (elements * 256);

    u32 bytes_done = 0;
    u32 elements_do = 0;

    u8 waiting = 1;
    u8 rx_buf[3];

    //used for all elements 256 bytes in size
    u32 dma_element = TR_CTRL_PORT_NR(mmce_port)      |
                      TR_CTRL_PAUSE(0)                |
                      TR_CTRL_TX_MODE_PIO_DMA(1)      |
                      TR_CTRL_RX_MODE_PIO_DMA(0)      |
                      TR_CTRL_NORMAL_TR(1)            |
                      TR_CTRL_SPECIAL_TR(0)           |
                      TR_CTRL_BAUD_DIV(0)             |
                      TR_CTRL_WAIT_ACK_FOREVER(0)     |
                      TR_CTRL_TX_DATA_SZ(256)         |
                      TR_CTRL_RX_DATA_SZ(0);

    //used for all elements less than 256 bytes in size
    u32 pio_element = TR_CTRL_PORT_NR(mmce_port)      |
                      TR_CTRL_PAUSE(0)                |
                      TR_CTRL_TX_MODE_PIO_DMA(0)      |
                      TR_CTRL_RX_MODE_PIO_DMA(0)      |
                      TR_CTRL_NORMAL_TR(1)            |
                      TR_CTRL_SPECIAL_TR(0)           |
                      TR_CTRL_BAUD_DIV(0)             |
                      TR_CTRL_WAIT_ACK_FOREVER(0)     |
                      TR_CTRL_TX_DATA_SZ(pio_size)    |
                      TR_CTRL_RX_DATA_SZ(0);

    while(1) {
        //Waiting stage
        if (waiting == 1) {
            res = mmce_sio2_tx_rx_pio(0, 2, NULL, rx_buf, &timeout_2s);
            if (res == -1) {
                DPRINTF("%s ERROR: Timed out waiting for ready\n", __func__);
                return -1;
            }

            if (bytes_done >= size)
                break;

            //Move to transfer stage
            waiting = 0;

        //Transfer stage
        } else if (waiting == 0) {
            elements_do = elements > 16 ? 16 : elements;

            //Full 4KB DMA transfer
            if (elements_do == 16) {
                //Reset SIO2 + FIFO pointers, enable interrupts
                inl_sio2_ctrl_set(0x3ac);

                for (int i = 0; i < elements_do; i++) {
                    inl_sio2_regN_set(i, dma_element);
                }

                sceSetSliceDMA(IOP_DMAC_SIO2in, &buffer[bytes_done], 0x100 >> 2, elements_do, DMAC_FROM_MEM);
                sceStartDMA(IOP_DMAC_SIO2in);

                bytes_done += elements_do * 256;
                elements -= elements_do;

                //Start the transfer
                inl_sio2_ctrl_set(0x3a1);

                //Set timeout alarm
                SetAlarm(&timeout_2s, mmce_sio2_timeout_handler, &event_flag);

                //Wait for completion or timeout
                WaitEventFlag(event_flag, EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE, 1, &resbits);
                if (resbits & EF_SIO2_TRANSFER_TIMEOUT) {
                    DPRINTF("Detected transfer timeout, attempting to reset SIO2\n");
                    inl_sio2_ctrl_set(0x3ac); //Reset SIO2
                    ClearEventFlag(event_flag, ~(EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE));
                    return -1;
                }

                //Cancel timeout alarm
                CancelAlarm(mmce_sio2_timeout_handler,  &event_flag);

                //Clear flags
                ClearEventFlag(event_flag, ~(EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE));

                waiting = 1;

            //Sub 4KB DMA transfer
            } else {
                //Remaining DMA elements
                if (elements_do != 0) {

                    //Reset SIO2 + FIFO pointers, enable interrupts
                    inl_sio2_ctrl_set(0x3ac);

                    for (int i = 0; i < elements_do; i++) {
                        inl_sio2_regN_set(i, dma_element);
                    }

                    sceSetSliceDMA(IOP_DMAC_SIO2in, &buffer[bytes_done], 0x100 >> 2, elements_do, DMAC_FROM_MEM);
                    sceStartDMA(IOP_DMAC_SIO2in);

                    bytes_done += elements_do * 256;
                    elements -= elements_do;

                    //Start the transfer
                    inl_sio2_ctrl_set(0x3a1);

                    //Set timeout alarm
                    SetAlarm(&timeout_2s, mmce_sio2_timeout_handler, &event_flag);

                    //Wait for completion or timeout
                    WaitEventFlag(event_flag, EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE, 1, &resbits);
                    if (resbits & EF_SIO2_TRANSFER_TIMEOUT) {
                        DPRINTF("Detected transfer timeout, attempting to reset SIO2\n");
                        inl_sio2_ctrl_set(0x3ac); //Reset SIO2
                        ClearEventFlag(event_flag, ~(EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE));
                        return -1;
                    }

                    //Cancel timeout alarm
                    CancelAlarm(mmce_sio2_timeout_handler,  &event_flag);

                    //Clear flags
                    ClearEventFlag(event_flag, ~(EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE));
                }
                //PIO element
                if (pio_size != 0) {

                    //Reset SIO2 + FIFO pointers, enable interrupts
                    inl_sio2_ctrl_set(0x3ac);

                    inl_sio2_regN_set(0, pio_element);

                    //Place data TX FIFO
                    for (int i = 0; i < pio_size; i++) {
                        inl_sio2_data_out(buffer[bytes_done + i]);
                    }

                    bytes_done += pio_size;
                    pio_size = 0;

                    //Start the transfer
                    inl_sio2_ctrl_set(0x3a1);

                    //Set timeout alarm
                    SetAlarm(&timeout_2s, mmce_sio2_timeout_handler, &event_flag);

                    //Wait for completion or timeout
                    WaitEventFlag(event_flag, EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE, 1, &resbits);
                    if (resbits & EF_SIO2_TRANSFER_TIMEOUT) {
                        DPRINTF("Detected transfer timeout, attempting to reset SIO2\n");
                        inl_sio2_ctrl_set(0x3ac); //Reset SIO2
                        ClearEventFlag(event_flag, ~(EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE));
                        return -1;
                    }

                    //Cancel timeout alarm
                    CancelAlarm(mmce_sio2_timeout_handler,  &event_flag);

                    //Clear flags
                    ClearEventFlag(event_flag, ~(EF_SIO2_TRANSFER_TIMEOUT | EF_SIO2_INTR_COMPLETE));
                }
                waiting = 1;
            }

        }
    }
    return 0;
}