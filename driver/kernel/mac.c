#if !SONIC_KERNEL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <arpa/inet.h>
#include <linux/socket.h>
#include <net/ethernet.h>
#else /* SONIC_KERNEL */
#include <asm/i387.h>
#include <linux/crc32.h>        // ether_crc
#include <linux/if_ether.h>     //struct ethhdr
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/time.h>
#endif /* SONIC_KERNEL */

#include "sonic.h"

inline int sonic_update_csum_dport_id(uint8_t *p, int id, 
        int num_queue, int port_base)
{
    struct iphdr *iph = (struct iphdr *) (p + PREAMBLE_LEN + ETH_HLEN + SONIC_VLAN_ADD);
    struct udphdr *uh = (struct udphdr *) (((uint8_t *) iph) + IP_HLEN);
    uint32_t *pid = (uint32_t *) (((uint8_t *) uh) + UDP_HLEN);
    int xid=*pid;
    uint64_t tmp;
    short pdest = uh->dest, pcsum = uh->check;

    uh->dest = htons(port_base + id % num_queue);
    *pid = htonl(id);

//    SONIC_PRINT("%d\n", ntohl(*pid));

    uh->check = 0;

    tmp = (~pcsum &0xffff)+ (~pdest&0xffff )+ htons(port_base + id % num_queue)
            + (~(xid >> 16) &0xffff)+ (~(xid & 0xffff)&0xffff) + htons(id>>16) + htons(id &0xffff);

    while (tmp>>16)
        tmp = (tmp & 0xffff) + (tmp >> 16);
    if (tmp == 0xffff)
        tmp = 0;

    uh->check = ~tmp;

#if !SONIC_KERNEL
//    SONIC_DDPRINT("xid = %x id = %x, num_queue = %d, port_base = %d (%x)\n", xid, id, num_queue, port_base, port_base);
//    SONIC_DDPRINT("%.4x %.4x (%.4x) %.4x \n", ntohs(pcsum), ntohs(pdest), ~ntohs(pdest), port_base + id%num_queue);
//    SONIC_DDPRINT("pdest = %.4x, pid = %x, pcheck = %x, dest = %x, id = %x, check = %x, tmp = %x (%x)\n",
//            pdest, xid, pcsum, uh->dest, id, uh->check,  (uint32_t) tmp, (uint32t) ~tmp);

//  if ((ncsum &0xffff )!= (uh->check & 0xffff)) {
//      SONIC_ERROR("csum error %x %x %x %x\n", xid, id, ncsum, uh->check);
//      exit(1);
//  }
//  if (id == 0xfffff)
//  if (id == 0x3)
//      exit(1);
#endif

    return 0;
}

static inline int retrieve_bit(char * msg, int msg_len, int id)
{
    int offset,byte_offset, bit_offset, bit;

    if (id==0)
        return 0;

    offset = (id-1) % (msg_len * 8);
    byte_offset = offset >> 3;
    bit_offset = offset & 0x7;
    bit = (msg[byte_offset] >> bit_offset) & 0x1;

    SONIC_DDPRINT("id = %d offset = %d byte_offset = %d bit_offset = %d, bit = %d\n",
        id, offset, byte_offset, bit_offset, bit);

    return bit;
}


inline int sonic_update_fifo_pkt_gen(struct sonic_packets *packets, 
        struct sonic_port_info *info, int tid, uint64_t default_idle)
{
    struct sonic_packet *packet;
    int i, covert_bit;
    uint32_t *pcrc, crc, idle;

    FOR_SONIC_PACKETS(i, packets, packet) {
        if (unlikely(tid == info->pkt_cnt)) {
            packet->len = 0;
            packet->idle = default_idle;
            packets->pkt_cnt = i == 0? 1 : i; 
            return tid;
        }

//        SONIC_PRINT("%d\n", tid);

        sonic_update_csum_dport_id(packet->buf, tid, info->multi_queue, 
                info->port_dst);

        idle = info->idle;
        switch(info->gen_mode) {
        case SONIC_PKT_GEN_CHAIN:
            if (tid % info->chain_num == 0) 
                idle = info->chain_idle;
            break;
        case SONIC_PKT_GEN_COVERT:
            covert_bit = retrieve_bit(info->covert_msg, strlen(info->covert_msg), tid);
            if (covert_bit)
                idle = ((info->idle - info->delta)< 12) ? 12 : (info->idle - info->delta);
            else
                idle = info->idle + info->delta;
            break;
        }

        if (unlikely(i == 0)){
            packet->len = info->pkt_len + 8;
            packet->idle = tid == 0 ? info->delay : idle;
        } else
            packet->idle = idle;

        pcrc = (uint32_t *) CRC_OFFSET(packet->buf, packet->len);
        *pcrc = 0;
        crc = SONIC_CRC(packet) ^ 0xffffffff;
        *pcrc = crc;

        tid++;
    }

    packets->pkt_cnt = i;

    return tid;
}

int sonic_mac_pkt_generator_loop(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(mac, args);
    struct sonic_fifo *out_fifo = mac->out_fifo;
    struct sonic_port_info *info = &mac->port->info;
    struct sonic_packets *packets;
    struct sonic_mac_stat *stat = &mac->stat;
    int tid=0, tcnt=0;
    uint64_t default_idle = power_of_two(out_fifo->exp) * 496;

    SONIC_DPRINT("\n");

    tcnt = sonic_prepare_pkt_gen_fifo(out_fifo, info);

    START_CLOCK();

    if (sonic_gen_idles(mac->port, out_fifo, info->wait))
        goto end;

begin:
    packets = (struct sonic_packets *) get_write_entry(out_fifo);
    if (!packets)
        goto end;

    packets->pkt_cnt = tcnt;
    tid = sonic_update_fifo_pkt_gen(packets, info, tid, default_idle);

    put_write_entry(out_fifo, packets);

    if (*stopper == 0)
        goto begin;
end:
    STOP_CLOCK(stat);
    stat->total_bytes = (uint64_t) tid * (info->pkt_len +8);
    stat->total_packets = tid;

    return 0;
}

int sonic_mac_tx_loop(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(mac, args);
    struct sonic_fifo * in_fifo = mac->in_fifo;
    struct sonic_fifo * out_fifo = mac->out_fifo;
    struct sonic_packets *packets;
    struct sonic_packet *packet;
    struct sonic_mac_stat *stat = &mac->stat;

    int i;
    uint32_t *pcrc, crc;

    SONIC_DPRINT("\n");

    START_CLOCK();

begin:
    packets = (struct sonic_packets *) get_read_entry(in_fifo);
    if (!packets)
        goto end;

    FOR_SONIC_PACKETS(i, packets, packet) {
        if (packet->len == 0)
            continue;

//        SONIC_PRINT("%d %d %d %d %p %p \n", i, packet->len, packet->idle, packet->next, packets, packet);

        pcrc = (uint32_t *) CRC_OFFSET(packet->buf, packet->len);

        stat->total_bytes += packet->len;
        stat->total_packets ++;

        *pcrc = 0;
        crc = SONIC_CRC(packet) ^ 0xffffffff;
        *pcrc = crc;
    }

    push_entry(in_fifo, out_fifo, packets);

    if (*stopper == 0)
        goto begin;
end:
    STOP_CLOCK(stat);

    return 0;
}

int sonic_mac_rx_loop(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(mac, args);
    struct sonic_fifo *in_fifo = mac->in_fifo;
    struct sonic_packets *packets;
    struct sonic_packet *packet;
    struct sonic_mac_stat *stat = &mac->stat;
//    uint64_t total_pkt_cnt=0, total_bytes=0, crc_error=0;
    int i;
    uint32_t crc;

    SONIC_DPRINT("\n");

    START_CLOCK();

begin:
    packets = (struct sonic_packets *) get_read_entry(in_fifo);
    if (!packets)
        goto end;

    FOR_SONIC_PACKETS(i, packets, packet) {
        if (packet->len < 72) {         // FIXME
            continue;
        }

        stat->total_bytes += packet->len;
        stat->total_packets ++;

        crc = ~SONIC_CRC(packet);

        if (crc) {
            stat->total_error_crc ++;
        }
    }

    put_read_entry(in_fifo, packets);

    if (*stopper == 0)
        goto begin;
end:
    STOP_CLOCK(stat);

    return 0;
}
