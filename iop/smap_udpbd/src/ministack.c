#include <smapregs.h>
#include <stdio.h>
#include "ministack.h"
#include "xfer.h"


static uint32_t ip_addr = IP_ADDR(192, 168, 1, 10);

typedef struct {
    uint8_t  mac[6];
    uint32_t ip;
} arp_entry_t;
#define MS_ARP_ENTRIES 8
arp_entry_t arp_table[MS_ARP_ENTRIES];


void eth_packet_init(eth_packet_t *pkt, uint16_t type)
{
    // Ethernet, broadcast
    pkt->eth.addr_dst[0] = 0xff;
    pkt->eth.addr_dst[1] = 0xff;
    pkt->eth.addr_dst[2] = 0xff;
    pkt->eth.addr_dst[3] = 0xff;
    pkt->eth.addr_dst[4] = 0xff;
    pkt->eth.addr_dst[5] = 0xff;
    SMAPGetMACAddress(pkt->eth.addr_src);
    pkt->eth.type = htons(type);
}

void ip_packet_init(ip_packet_t *pkt, uint32_t ip_dest)
{
    eth_packet_init((eth_packet_t *)pkt, ETH_TYPE_IPV4);

    // IP, broadcast
    pkt->ip.hlen             = 0x45;
    pkt->ip.tos              = 0;
    //pkt->ip_len              = ;
    pkt->ip.id               = 0;
    pkt->ip.flags            = 0;
    pkt->ip.frag_offset      = 0;
    pkt->ip.ttl              = 64;
    pkt->ip.proto            = IP_PROTOCOL_UDP;
    //pkt->ip_csum             = ;
    pkt->ip.addr_src.addr[0] = (ip_addr >> 24) & 0xff;
    pkt->ip.addr_src.addr[1] = (ip_addr >> 16) & 0xff;
    pkt->ip.addr_src.addr[2] = (ip_addr >>  8) & 0xff;
    pkt->ip.addr_src.addr[3] = (ip_addr      ) & 0xff;
    pkt->ip.addr_dst.addr[0] = (ip_dest >> 24) & 0xff;
    pkt->ip.addr_dst.addr[1] = (ip_dest >> 16) & 0xff;
    pkt->ip.addr_dst.addr[2] = (ip_dest >>  8) & 0xff;
    pkt->ip.addr_dst.addr[3] = (ip_dest      ) & 0xff;
}

void udp_packet_init(udp_packet_t *pkt, uint32_t ip_dst, uint16_t port_dst)
{
    ip_packet_init((ip_packet_t *)pkt, ip_dst);

    //pkt->udp.port_src = ;
    pkt->udp.port_dst = htons(port_dst);
    //pkt->udp.len      = ;
    //pkt->udp.csum     = ;
}

static uint16_t ip_checksum(ip_header_t *ip)
{
    uint16_t *data = (uint16_t *)ip;
    int count = 10;
    uint32_t csum  = 0;

    while (count--)
        csum += *data++;
    csum = (csum >> 16) + (csum & 0xffff);
    csum = (csum >> 16) + (csum & 0xffff);

    return ~((uint16_t)csum & 0xffff);
}

int eth_packet_send_ll(eth_packet_t *pkt, uint16_t pktdatasize, const void *data, uint16_t datasize)
{
    return smap_transmit(pkt, sizeof(eth_header_t) + pktdatasize, data, datasize);
}

int ip_packet_send_ll(ip_packet_t *pkt, uint16_t pktdatasize, const void *data, uint16_t datasize)
{
    pkt->ip.len  = htons(sizeof(ip_header_t) + pktdatasize + datasize);
    pkt->ip.csum = 0;
    pkt->ip.csum = ip_checksum(&pkt->ip);

    return eth_packet_send_ll((eth_packet_t *)pkt, sizeof(ip_header_t) + pktdatasize, data, datasize);
}

#define UDP_MAX_PORTS 4
udp_socket_t udp_ports[UDP_MAX_PORTS];
udp_socket_t *udp_bind(uint16_t port_src, udp_port_handler handler, void *handler_arg)
{
    int i;

    for (i=0; i<UDP_MAX_PORTS; i++) {
        if (udp_ports[i].port_src == 0) {
            udp_ports[i].port_src    = port_src;
            udp_ports[i].handler     = handler;
            udp_ports[i].handler_arg = handler_arg;
            return &udp_ports[i];
        }
    }

    return NULL;
}

int udp_packet_send_ll(udp_socket_t *socket, udp_packet_t *pkt, uint16_t pktdatasize, const void *data, uint16_t datasize)
{
    pkt->udp.port_src = socket->port_src;
    pkt->udp.len  = htons(sizeof(udp_header_t) + pktdatasize + datasize);
    pkt->udp.csum = 0; // not needed

    return ip_packet_send_ll((ip_packet_t *)pkt, sizeof(udp_header_t) + pktdatasize, data, datasize);
}

int arp_add_entry(uint32_t ip, uint8_t mac[6])
{
    int i;

    // Update existing entry
    for (i=0; i<MS_ARP_ENTRIES; i++) {
        if (ip == arp_table[i].ip) {
            arp_table[i].mac[0] = mac[0];
            arp_table[i].mac[1] = mac[1];
            arp_table[i].mac[2] = mac[2];
            arp_table[i].mac[3] = mac[3];
            arp_table[i].mac[4] = mac[4];
            arp_table[i].mac[5] = mac[5];
            return 0;
        }
    }

    // Add new entry
    for (i=0; i<MS_ARP_ENTRIES; i++) {
        if (ip == 0) {
            arp_table[i].ip  = ip;
            arp_table[i].mac[0] = mac[0];
            arp_table[i].mac[1] = mac[1];
            arp_table[i].mac[2] = mac[2];
            arp_table[i].mac[3] = mac[3];
            arp_table[i].mac[4] = mac[4];
            arp_table[i].mac[5] = mac[5];
            return 0;
        }
    }

    return -1;
}

static inline int handle_rx_arp(uint16_t pointer)
{
    USE_SMAP_REGS;
    arp_packet_t req;
    static arp_packet_t reply;
    uint32_t *parp = (uint32_t*)&req;

    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + 12;
    parp[ 3] = SMAP_REG32(SMAP_R_RXFIFO_DATA); //  2
    parp[ 4] = SMAP_REG32(SMAP_R_RXFIFO_DATA); //  6
    parp[ 5] = SMAP_REG32(SMAP_R_RXFIFO_DATA); // 10
    parp[ 6] = SMAP_REG32(SMAP_R_RXFIFO_DATA); // 14
    parp[ 7] = SMAP_REG32(SMAP_R_RXFIFO_DATA); // 18
    parp[ 8] = SMAP_REG32(SMAP_R_RXFIFO_DATA); // 22
    parp[ 9] = SMAP_REG32(SMAP_R_RXFIFO_DATA); // 26
    parp[10] = SMAP_REG32(SMAP_R_RXFIFO_DATA); // 30

    if (ntohs(req.arp.oper) == 1 && ntohl(req.arp.target_ip) == ip_addr) {
        reply.eth.addr_dst[0] = req.arp.sender_mac[0];
        reply.eth.addr_dst[1] = req.arp.sender_mac[1];
        reply.eth.addr_dst[2] = req.arp.sender_mac[2];
        reply.eth.addr_dst[3] = req.arp.sender_mac[3];
        reply.eth.addr_dst[4] = req.arp.sender_mac[4];
        reply.eth.addr_dst[5] = req.arp.sender_mac[5];
        SMAPGetMACAddress(reply.eth.addr_src);
        reply.eth.type = htons(ETH_TYPE_ARP);
        reply.arp.htype = htons(1); // ethernet
        reply.arp.ptype = htons(ETH_TYPE_IPV4);
        reply.arp.hlen = 6;
        reply.arp.plen = 4;
        reply.arp.oper = htons(2); // reply
        SMAPGetMACAddress(reply.arp.sender_mac);
        reply.arp.sender_ip     = req.arp.target_ip;
        reply.arp.target_mac[0] = req.arp.sender_mac[0];
        reply.arp.target_mac[1] = req.arp.sender_mac[1];
        reply.arp.target_mac[2] = req.arp.sender_mac[2];
        reply.arp.target_mac[3] = req.arp.sender_mac[3];
        reply.arp.target_mac[4] = req.arp.sender_mac[4];
        reply.arp.target_mac[5] = req.arp.sender_mac[5];
        reply.arp.target_ip     = req.arp.sender_ip;
        smap_transmit(&reply, 0x2A, NULL, 0);
    }

    return -1;
}

static inline int handle_rx_udp(uint16_t pointer)
{
    USE_SMAP_REGS;
    uint16_t dport;
    int i;

    // Check port
    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + 0x24;
    dport = SMAP_REG16(SMAP_R_RXFIFO_DATA);

    for (i=0; i<UDP_MAX_PORTS; i++) {
        if (dport == udp_ports[i].port_src)
            return udp_ports[i].handler(&udp_ports[i], pointer, udp_ports[i].handler_arg);
    }

    PRINTF("ministack: udp: dport 0x%X\n", dport);
    return -1;
}

static inline int handle_rx_ipv4(uint16_t pointer)
{
    USE_SMAP_REGS;
    uint8_t protocol;

    // Check ethernet type
    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + 0x14;
    protocol = SMAP_REG32(SMAP_R_RXFIFO_DATA) >> 24;

    switch (protocol) {
        case IP_PROTOCOL_UDP:
            return handle_rx_udp(pointer);
        default:
            PRINTF("ministack: ipv4: protocol 0x%X\n", protocol);
            return -1;
    }
}

int handle_rx_eth(uint16_t pointer)
{
    USE_SMAP_REGS;
    uint16_t eth_type;

    // Check ethernet type
    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + 12;
    eth_type = ntohs(SMAP_REG16(SMAP_R_RXFIFO_DATA));

    switch (eth_type) {
        case ETH_TYPE_ARP:
            return handle_rx_arp(pointer);
        case ETH_TYPE_IPV4:
            return handle_rx_ipv4(pointer);
        default:
            PRINTF("ministack: eth: type 0x%X\n", eth_type);
            return -1;
    }
}

void ms_ip_set_ip(uint32_t ip)
{
    ip_addr = ip;
}

uint32_t ms_ip_get_ip()
{
    return ip_addr;
}
