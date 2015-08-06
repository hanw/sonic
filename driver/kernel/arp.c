#if SONIC_KERNEL
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#else
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <string.h>
#endif
#include "sonic.h"

/* ARP protocol */
struct arp_ethernet {
    unsigned char mac_src[ETH_ALEN];
    unsigned char ip_src[4];
    unsigned char mac_dst[ETH_ALEN];
    unsigned char ip_dst[4];
};

static int fill_arp_packet(uint8_t *p, struct sonic_port_info *info, int isrequest)
{
    int i = 0, len=0;
#if !SONIC_VLAN
    struct ethhdr *eth = (struct ethhdr *) p;
    struct arphdr *arp = (struct arphdr *) (p + sizeof(struct ethhdr));
    struct arp_ethernet *arp_eth= (struct arp_ethernet *) (p + sizeof(struct ethhdr) + sizeof(struct arphdr));
    eth->h_proto = htons(ETH_P_ARP);
#else /* SONIC_VLAN */
    struct sonic_vlan_hdr *eth = (struct sonic_vlan_hdr *) p;
    struct arphdr *arp = (struct arphdr *) (p + sizeof(struct sonic_vlan_hdr));
    struct arp_ethernet *arp_eth= (struct arp_ethernet *) (p + sizeof(struct sonic_vlan_hdr) + sizeof(struct arphdr));
    eth->h_vlan_proto = htons(ETH_P_8021Q);
    eth->h_vlan_TCI = htons(info->vlan_id);
    eth->h_vlan_encapsulated_proto = htons(ETH_P_ARP);
#endif


    if (isrequest) {
        uint8_t *q = p;
        for (i = 0 ; i < 6 ; i ++ ) {
            *q++ = 0xff;
        }
        memcpy(eth->h_source, info->mac_src, ETH_ALEN);
    } else {
        memcpy(eth->h_dest, info->mac_dst, ETH_ALEN);
        memcpy(eth->h_source, info->mac_src, ETH_ALEN);
    }

    // FILL ARP packet header
    arp->ar_hrd = htons(ARPHRD_ETHER);
    arp->ar_pro = htons(ETH_P_IP);
    arp->ar_hln = 6;
    arp->ar_pln = 4;
    arp->ar_op = htons( isrequest ? ARPOP_REQUEST : ARPOP_REPLY);

    // FILL ARP packet payload
    if (isrequest) {
        memcpy(arp_eth->mac_src, info->mac_src, ETH_ALEN);
        memcpy(arp_eth->ip_src, &info->ip_src, 4);
        memset(arp_eth->mac_dst, 0, ETH_ALEN);
        memcpy(arp_eth->ip_dst, &info->ip_dst, 4);
    } else{
        memcpy(arp_eth->mac_src, info->mac_src, ETH_ALEN);
        memcpy(arp_eth->ip_src, &info->ip_src, 4);
        memcpy(arp_eth->mac_dst, info->mac_dst, ETH_ALEN);
        memcpy(arp_eth->ip_dst, &info->ip_dst, 4);
    }

#if !SONIC_VLAN
    i =  sizeof(struct ethhdr) + sizeof(struct arphdr) + sizeof(struct arp_ethernet);
    len = 64;
#else
    i =  sizeof(struct sonic_vlan_hdr) + sizeof(struct arphdr) + sizeof(struct arp_ethernet);
    len = 68;
#endif
    for ( ; i < len ; i ++ ) {
        *(p + i) = 0;
    }

    sonic_print_hex(p, i, 32);
    
    return i;
}

/* sending one ARP packet */
int sonic_mac_arp_loop(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(mac, args);
    struct sonic_fifo *out_fifo = mac->out_fifo;
    struct sonic_port_info *info = &mac->port->info;
    struct sonic_packets *packets;
    struct sonic_packet * packet;
    struct sonic_mac_stat *stat = &mac->stat;
    int tcnt=0, sent=0;
    uint64_t default_idle = power_of_two(out_fifo ->exp) * 496;
    uint32_t *pcrc, crc;
    int isrequest = info->gen_mode == 0;

    SONIC_DPRINT("\n");

    info->pkt_len = 1518;
    tcnt = sonic_prepare_pkt_gen_fifo(out_fifo, info);

    START_CLOCK();

    if (sonic_gen_idles(mac->port, out_fifo, info->wait))
        goto end;

begin:
    packets = (struct sonic_packets *) get_write_entry(out_fifo);
    if (!packets)
        goto end;

    if (!sent) {
        packet = packets->packets;
        packet->len = fill_arp_packet(packet->buf + 8, info, isrequest) + 8;
        packet->idle = 12;

        pcrc = (uint32_t *) CRC_OFFSET (packet->buf, packet->len);
        *pcrc = 0;
        crc = SONIC_CRC(packet) ^ 0xffffffff;
        *pcrc = crc;

        packets->pkt_cnt = 1;

        sent = 1;
    } else {
        packet = packets->packets;
        packet->len = 0;
        packet->idle = default_idle;
        packets->pkt_cnt = 1;
    }

    put_write_entry(out_fifo, packets);

    if (*stopper == 0)
        goto begin;
end:
    STOP_CLOCK(stat);

    return 0;
}


