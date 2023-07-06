#ifndef UDPBD_H
#define UDPBD_H


#include <stdint.h>


#define UDPBD_PORT            0xBDBD //The port on which to listen for incoming data

#define UDPBD_CMD_INFO        0x00 // client -> server
#define UDPBD_CMD_INFO_REPLY  0x01 // server -> client
#define UDPBD_CMD_READ        0x02 // client -> server
#define UDPBD_CMD_READ_RDMA   0x03 // server -> client
#define UDPBD_CMD_WRITE       0x04 // client -> server
#define UDPBD_CMD_WRITE_RDMA  0x05 // client -> server
#define UDPBD_CMD_WRITE_DONE  0x06 // server -> client


#define UDPBD_MAX_SECTOR_READ  512 // 512 sectors of 512 bytes = 256KiB


/*
 * UDPBD v2
 */

struct SUDPBDv2_Header { // 2 bytes - Must be a "(multiple of 4) + 2" for RDMA on the PS2 !
    union
    {
        uint16_t cmd16;
        struct
        {
            uint16_t cmd    : 5; // 0.. 31 - command
            uint16_t cmdid  : 3; // 0..  8 - increment with every new command sequence
            uint16_t cmdpkt : 8; // 0..255 - 0=request, 1 or more are response packets
        };
    };	
} __attribute__((__packed__));

/*
 * Info request. Can be a broadcast message to detect server on the network.
 *
 * Sequence of packets:
 * - client: InfoRequest
 * - server: InfoReply
 */
struct SUDPBDv2_InfoRequest {
	struct SUDPBDv2_Header hdr;
} __attribute__((__packed__));

struct SUDPBDv2_InfoReply {
	struct SUDPBDv2_Header hdr;
	uint32_t sector_size;
	uint32_t sector_count;
} __attribute__((__packed__));

/*
 * Read request, sequence of packets:
 * - client: ReadRequest
 * - server: RDMA (1 or more packets)
 *
 * Write request, sequence of packets:
 * - client: WriteRequest
 * - client: RDMA (1 or more packets)
 * - server: WriteDone
 */
struct SUDPBDv2_RWRequest {
	struct SUDPBDv2_Header hdr;
	uint32_t sector_nr;
	uint16_t sector_count;
} __attribute__((__packed__));

struct SUDPBDv2_WriteDone {
	struct SUDPBDv2_Header hdr;
	int32_t result;
} __attribute__((__packed__));

/*
 * Remote DMA (RDMA) packet
 * Used for transfering large blocks of data.
 * The heart of the protocol. Data must be a "(multiple of 4) + 2" for RDMA on the PS2 !
 */
union block_type
{
    uint32_t bt;
    struct
    {
        uint32_t block_shift :  4; // 0..7: blocks_size = 1 << (block_shift+2); min=0=4bytes, max=7=512bytes
        uint32_t block_count :  9; // 1..366 blocks
        uint32_t spare       : 19;
    };
};	
/*
 * Maximum payload for an RDMA packet depends on the used block size:
 * -   4 * 366 = 1464 bytes
 * -   8 * 183 = 1464 bytes
 * -  16 *  91 = 1456 bytes
 * -  32 *  45 = 1440 bytes
 * -  64 *  22 = 1408 bytes
 * - 128 *  11 = 1408 bytes <- default
 * - 256 *   5 = 1280 bytes
 * - 512 *   2 = 1024 bytes
 */
#define UDP_MAX_PAYLOAD  1472
#define RDMA_MAX_PAYLOAD (UDP_MAX_PAYLOAD - sizeof(struct SUDPBDv2_Header) - sizeof(union block_type)) // 1466

struct SUDPBDv2_RDMA {
	struct SUDPBDv2_Header hdr;
    union block_type bt;
    uint8_t data[RDMA_MAX_PAYLOAD];
} __attribute__((__packed__));


#endif
