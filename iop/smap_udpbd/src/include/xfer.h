#ifndef XFER_H
#define XFER_H


#include <stdint.h>
#include "main.h"


/**
 * Send data over the network
 * @param buf data buffer to be sent
 * @param size Size of the data in bytes
 * @return 0 on succes, -1 on failure
 */
int smap_transmit(void *header, uint16_t headersize, const void *data, uint16_t datasize);

void xfer_init(void);
int HandleRxIntr(struct SmapDriverData *SmapDrivPrivData);


#endif
