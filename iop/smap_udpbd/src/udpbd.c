#include <errno.h>
#include <bdm.h>
#include <thevent.h>
#include <stdio.h>
#include <smapregs.h>
#include <dmacman.h>
#include <dev9.h>

#include "udpbd.h"
#include "ministack.h"
#include "main.h"

#ifdef DEBUG
#define M_PRINTF(format, args...) printf("UDPBD: " format, ##args)
#define M_DEBUG(format, args...)  printf("UDPBD: " format, ##args)
#else
#define M_PRINTF(format, args...)
#define M_DEBUG(format, args...)
#endif

#define UINT32_MAX (4294967295U)

#define UDPBD_MAX_RETRIES         4


struct SUDPBDv2_Header_Padded32 {
    union
    {
        uint32_t cmd32;
        struct
        {
            uint16_t padding;
            struct SUDPBDv2_Header hdr;
        };
    };
} __attribute__((__packed__));

typedef struct
{
    eth_header_t eth;           // 14 bytes, offset + 0
    ip_header_t ip;             // 20 bytes, offset +14 (0x0E)
    udp_header_t udp;           //  8 bytes, offset +34 (0x22)
    struct SUDPBDv2_Header bd;  //  2 bytes, offset +42 (0x2A)
} __attribute__((packed, aligned(4))) udpbd_pkt_t;

typedef struct
{
    eth_header_t eth;           // 14 bytes, offset + 0
    ip_header_t ip;             // 20 bytes, offset +14 (0x0E)
    udp_header_t udp;           //  8 bytes, offset +34 (0x22)
    struct SUDPBDv2_RWRequest rw;
} __attribute__((packed, aligned(4))) udpbd_pkt_rw_t;

typedef struct
{
    eth_header_t eth;           // 14 bytes, offset + 0
    ip_header_t ip;             // 20 bytes, offset +14 (0x0E)
    udp_header_t udp;           //  8 bytes, offset +34 (0x22)
	struct SUDPBDv2_Header hdr;
    union block_type bt;
} __attribute__((packed, aligned(4))) udpbd_pkt_rdma_t;


static struct block_device g_udpbd;
static uint8_t g_cmdid   = 0;
static int g_ev_done   = 0;
static int g_read_cmdpkt = 0;
static int bdm_connected = 0;
static uint8_t *g_buffer = NULL;
static uint8_t *g_buffer_act = NULL;
static unsigned int g_read_size;
static int32_t g_errno = 0;
static udp_socket_t *udpbd_socket = NULL;


static unsigned int _udpbd_timeout(void *arg)
{
    g_read_size = 0;
    g_errno     = 1;
    iSetEventFlag(g_ev_done, 2);
    return 0;
}

//
// Block device interface
//
static int _udpbd_read(struct block_device *bd, uint64_t sector, void *buffer, uint16_t count)
{
    uint32_t EFBits;
    iop_sys_clock_t clock;
    udpbd_pkt_rw_t pkt;

    //M_DEBUG("%s: sector=%d, count=%d\n", __func__, sector, count);

    g_cmdid         = (g_cmdid + 1) & 0x7;
    g_buffer        = buffer;
    g_buffer_act    = buffer;
    g_read_size     = count * g_udpbd.sectorSize;
    g_read_cmdpkt   = 1; // First reply packet should be cmdpkt==1

    udp_packet_init((udp_packet_t *)&pkt, IP_ADDR(255,255,255,255), UDPBD_PORT);
    pkt.rw.hdr.cmd    = UDPBD_CMD_READ;
    pkt.rw.hdr.cmdid  = g_cmdid;
    pkt.rw.hdr.cmdpkt = 0;
    pkt.rw.sector_count = count;
    pkt.rw.sector_nr = sector;

    if (udp_packet_send(udpbd_socket, (udp_packet_t *)&pkt, sizeof(struct SUDPBDv2_RWRequest)) < 0)
        return -1;

    // Set alarm in case something hangs
    // 200ms + 2ms / sector
    USec2SysClock((200 * 1000) + (count * 2000), &clock);
    SetAlarm(&clock, _udpbd_timeout, NULL);

    //wait for data...
    WaitEventFlag(g_ev_done, 2 | 1, WEF_OR | WEF_CLEAR, &EFBits);

    // Cancel alarm
    CancelAlarm(_udpbd_timeout, NULL);

    g_buffer      = NULL;
    g_buffer_act  = NULL;
    g_read_size   = 0;
    g_read_cmdpkt = 0;

    switch (g_errno)
    {
        case 0:
            //M_DEBUG("%s(%d, %d): ok\n", __func__);
            break;
        case 1:
            M_DEBUG("%s(%d, %d): ERROR: timeout\n", __func__, sector, count);
            break;
        case 2:
            //M_DEBUG("%s(%d, %d): ERROR: invalid packet order!\n", __func__, sector, count);
            break;
        case 3:
            //M_DEBUG("%s(%d, %d): ERROR: invalid packet size!\n", __func__, sector, count);
            break;
        default:
            M_DEBUG("%s(%d, %d): ERROR: unknown %d\n", __func__, sector, count, g_errno);
            break;
    }

    if (EFBits & 1)
    { // done
        return count;
    }

    g_errno = 0;
    return -EIO;
}

static int udpbd_read(struct block_device *bd, uint64_t sector, void *buffer, uint16_t count)
{
    int retries;
    uint16_t count_left = count;

    //M_DEBUG("%s: sector=%d, count=%d\n", __func__, sector, count);

    if (bdm_connected == 0)
        return -EIO;

    while (count_left > 0)
    {
        uint16_t count_block = count_left > UDPBD_MAX_SECTOR_READ ? UDPBD_MAX_SECTOR_READ : count_left;

        for (retries = 0; retries < UDPBD_MAX_RETRIES; retries++)
        {
            if (_udpbd_read(bd, sector, buffer, count_block) == count_block)
                break;
            DelayThread(1000);
        }

        if (retries == UDPBD_MAX_RETRIES)
        {
            M_DEBUG("%s: too many errors, disconnecting\n", __func__);
            bdm_disconnect_bd(&g_udpbd);
            bdm_connected = 0;
            return -EIO;
        }

        count_left -= count_block;
        sector += count_block;
        buffer += count_block * g_udpbd.sectorSize;
    }

    return count;
}

static int udpbd_write(struct block_device *bd, uint64_t sector, const void *buffer, uint16_t count)
{
    uint32_t EFBits;

    M_DEBUG("%s: sector=%d, count=%d\n", __func__, sector, count);

    if (sector > UINT32_MAX) {
        // server expects u32 sector, disconnect to avoid potential data corruption
        M_DEBUG("%s: write past u32, disconnecting\n", __func__);
        bdm_disconnect_bd(&g_udpbd);
        bdm_connected = 0;
        return -EIO;
    }

    g_cmdid = (g_cmdid + 1) & 0x7;

    // Send write command
    {
        udpbd_pkt_rw_t pkt;
        udp_packet_init((udp_packet_t *)&pkt, IP_ADDR(255,255,255,255), UDPBD_PORT);
        pkt.rw.hdr.cmd    = UDPBD_CMD_WRITE;
        pkt.rw.hdr.cmdid  = g_cmdid;
        pkt.rw.hdr.cmdpkt = 0;
        pkt.rw.sector_count = count;
        pkt.rw.sector_nr = sector;

        if (udp_packet_send(udpbd_socket, (udp_packet_t *)&pkt, sizeof(struct SUDPBDv2_RWRequest)) < 0) {
            M_DEBUG("%s(%d, %d): ERROR\n", __func__, sector, count);
            return -1;
        }
    }
    
    // Send data
    {
        uint16_t count_left = count;
        udpbd_pkt_rdma_t pkt;
        udp_packet_init((udp_packet_t *)&pkt, IP_ADDR(255,255,255,255), UDPBD_PORT);
        pkt.hdr.cmd    = UDPBD_CMD_WRITE_RDMA;
        pkt.hdr.cmdid  = g_cmdid;
        pkt.hdr.cmdpkt = 0;
        pkt.bt.block_count = 4; // 4 x 128b = 512b
        pkt.bt.block_shift = 5; // 128 byte blocks

        while (count_left--) {
            pkt.hdr.cmdpkt++;
            if (udp_packet_send_ll(udpbd_socket, (udp_packet_t *)&pkt, sizeof(struct SUDPBDv2_Header) + sizeof(union block_type), buffer, 512) < 0) {
                M_DEBUG("%s(%d, %d): ERROR\n", __func__, sector, count);
                return -1;
            }
            buffer = (const uint8_t *)buffer + 512;
        }
    }

    // Wait for 'done' from server
    {
        iop_sys_clock_t clock;

        // Set alarm in case something hangs
        // 200ms
        USec2SysClock((200 * 1000), &clock);
        SetAlarm(&clock, _udpbd_timeout, NULL);

        //wait for done...
        WaitEventFlag(g_ev_done, 2 | 1, WEF_OR | WEF_CLEAR, &EFBits);

        // Cancel alarm
        CancelAlarm(_udpbd_timeout, NULL);
    }

    switch (g_errno)
    {
        case 0:
            //M_DEBUG("%s(%d, %d): ok\n", __func__);
            break;
        case 1:
            M_DEBUG("%s(%d, %d): ERROR: timeout\n", __func__, sector, count);
            break;
        case 2:
            //M_DEBUG("%s(%d, %d): ERROR: invalid packet order!\n", __func__, sector, count);
            break;
        case 3:
            //M_DEBUG("%s(%d, %d): ERROR: invalid packet size!\n", __func__, sector, count);
            break;
        default:
            M_DEBUG("%s(%d, %d): ERROR: unknown %d\n", __func__, sector, count, g_errno);
            break;
    }

    if (EFBits & 1)
    { // done
        return count;
    }

    M_DEBUG("%s(%d, %d): ERROR\n", __func__, sector, count);
    g_errno = 0;
    return -EIO;
}

static void udpbd_flush(struct block_device *bd)
{
    M_DEBUG("%s\n", __func__);
}

static int udpbd_stop(struct block_device *bd)
{
    M_DEBUG("%s\n", __func__);

    return 0;
}

static inline void _cmd_info_reply(struct SUDPBDv2_Header *hdr)
{
    if (bdm_connected == 0)
    {
        USE_SMAP_REGS;
        g_udpbd.sectorSize  = SMAP_REG32(SMAP_R_RXFIFO_DATA);
        g_udpbd.sectorCount = SMAP_REG32(SMAP_R_RXFIFO_DATA);
        bdm_connected = 1;
        bdm_connect_bd(&g_udpbd);
    }
}

static inline void _cmd_read_rdma(struct SUDPBDv2_Header *hdr)
{
    USE_SMAP_REGS;
    union block_type bt;
    uint32_t size;

    bt.bt = SMAP_REG32(SMAP_R_RXFIFO_DATA);
    size = bt.block_count << (bt.block_shift + 2);

    if (g_buffer == NULL) {
        M_DEBUG("%s: unexpected packet (cmd %d, cmdid %d, cmdpkt %d)\n", __func__, hdr->cmd, hdr->cmdid, hdr->cmdpkt);
        return;
    }

    // Validate packet order
    if (hdr->cmdpkt != (g_read_cmdpkt & 0xff))
    {
        // Error, wakeup caller
        g_read_size = 0;
        g_errno     = 2;
        M_DEBUG("%s: invalid cmdpkt (cmd %d, cmdid %d, cmdpkt %d != %d)\n", __func__, hdr->cmd, hdr->cmdid, hdr->cmdpkt, g_read_cmdpkt);
        SetEventFlag(g_ev_done, 2);
        return;
    }
    g_read_cmdpkt++;

    // Validate packet data size
    if (size > RDMA_MAX_PAYLOAD)
    {
        // Error, wakeup caller
        g_read_size = 0;
        g_errno     = 3;
        M_DEBUG("%s: invalid size %d\n", __func__, size);
        SetEventFlag(g_ev_done, 2);
        return;
    }

    // Directly DMA the packet data into the user buffer
    dev9DmaTransfer(1, g_buffer_act, bt.block_count << 16 | (1 << bt.block_shift), DMAC_TO_MEM);

    g_buffer_act += size;
    g_read_size -= size;
    if (g_read_size == 0)
    {
        // Done, wakeup caller
        SetEventFlag(g_ev_done, 1);
        return;
    }
}

static inline void _cmd_write_done(struct SUDPBDv2_Header *hdr)
{
    USE_SMAP_REGS;
    int32_t g_errno = SMAP_REG32(SMAP_R_RXFIFO_DATA);

    // Done, wakeup caller
    SetEventFlag(g_ev_done, (g_errno >= 0) ? 1 : 2);
    return;
}

static int udpbd_isr(udp_socket_t *socket, uint16_t pointer, void *arg)
{
    USE_SMAP_REGS;
    struct SUDPBDv2_Header_Padded32 hdr32;

    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + 0x28;
    hdr32.cmd32 = SMAP_REG32(SMAP_R_RXFIFO_DATA);

    if (hdr32.hdr.cmdid != g_cmdid) {
        M_DEBUG("%s: unexpected packet (cmd %d, cmdid %d, cmdpkt %d)\n", __func__, hdr32.hdr.cmd, hdr32.hdr.cmdid, hdr32.hdr.cmdpkt);
        return 0;
    }

    switch (hdr32.hdr.cmd)
    {
        case UDPBD_CMD_INFO_REPLY:
            _cmd_info_reply(&hdr32.hdr);
            break;
        case UDPBD_CMD_READ_RDMA:
            _cmd_read_rdma(&hdr32.hdr);
            break;
        case UDPBD_CMD_WRITE_DONE:
            _cmd_write_done(&hdr32.hdr);
            break;
        default:
            M_DEBUG("%s: invalid (cmd %d, cmdid %d, cmdpkt %d)\n", __func__, hdr32.hdr.cmd, hdr32.hdr.cmdid, hdr32.hdr.cmdpkt);
    };

    return 0;
}

//
// Public functions
//
int udpbd_init(void)
{
    udpbd_pkt_t pkt;
    iop_event_t EventFlagData;

    M_DEBUG("%s\n", __func__);

    EventFlagData.attr   = 0;
    EventFlagData.option = 0;
    EventFlagData.bits   = 0;
    if (g_ev_done <= 0)
        g_ev_done = CreateEventFlag(&EventFlagData);

    g_udpbd.name         = "udp";
    g_udpbd.devNr        = 0;
    g_udpbd.parNr        = 0;
    g_udpbd.sectorOffset = 0;
    g_udpbd.priv         = NULL;
    g_udpbd.read         = udpbd_read;
    g_udpbd.write        = udpbd_write;
    g_udpbd.flush        = udpbd_flush;
    g_udpbd.stop         = udpbd_stop;

    // Bind to UDP socket
    udpbd_socket = udp_bind(UDPBD_PORT, udpbd_isr, NULL);

    // Broadcast request for block device information
    udp_packet_init((udp_packet_t *)&pkt, IP_ADDR(255,255,255,255), UDPBD_PORT);
    pkt.bd.cmd    = UDPBD_CMD_INFO;
    pkt.bd.cmdid  = g_cmdid;
    pkt.bd.cmdpkt = 0;
    udp_packet_send(udpbd_socket, (udp_packet_t *)&pkt, sizeof(struct SUDPBDv2_Header));

    return 0;
}
