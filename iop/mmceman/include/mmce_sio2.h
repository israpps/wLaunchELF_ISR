#ifndef MMCE_SIO2_H
#define MMCE_SIO2_H

#include <tamtypes.h>
#include <thbase.h>

extern iop_sys_clock_t timeout_200ms;
extern iop_sys_clock_t timeout_1s;
extern iop_sys_clock_t timeout_2s;

extern u8 mmce_sio2_use_alarm;

int mmce_sio2_init();

//Remove hooks and deinit
void mmce_sio2_deinit();

//Lock SIO2 for execlusive access
void mmce_sio2_lock();

//Unlock SIO2
void mmce_sio2_unlock();

//Assign port
void mmce_sio2_set_port(int port);

//Get currently assigned port
int mmce_sio2_get_port();

//Update PCTRL1_WAIT_CYCLES_AFTER_ACK_LOW value
void mmce_sio2_update_ack_wait_cycles(int cycles);

//Enable/disable setting alarms for transfer timeouts
void mmce_sio2_set_use_alarm(int value);

//RX TX PIO single transfer (1-256 bytes)
int mmce_sio2_tx_rx_pio(u8 tx_size, u8 rx_size, u8 *tx_buf, u8 *rx_buf, iop_sys_clock_t *timeout);

//RX DMA n * 256
int mmce_sio2_rx_dma(u8 *buffer, u32 size);

//RX DMA n * 256, PIO remainder
int mmce_sio2_rx_mixed(u8 *buffer, u32 size);

//TX DMA n * 256, PIO remainder
int mmce_sio2_tx_mixed(u8 *buffer, u32 size);

#endif
