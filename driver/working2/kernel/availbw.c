#if !SONIC_KERNEL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "list.h"

#endif /* SONIC_KERNEL */

#include "sonic.h"
#include "fifo.h"

int sonic_availbw_loop(void * args)
{
    struct sonic_app *app = (struct sonic_app *) args;
    struct sonic_fifo *in_fifo = app->in_fifo;
    struct sonic_fifo *out_fifo = app->out_fifo;

    struct sonic_mac_fifo_entry *mac_entry, *b_entry = NULL, *t_entry = NULL;
    struct sonic_packet_entry *packet, *b_packet;
    const volatile int * const stopper = &app->port->stopper;

    int pkt_cnt, j, b_cnt;
    uint64_t log_total = 0, bytes = 0, log_list = 0;
    int fifo_cnt = 0;

    struct sonic_udp_port_info *info = &app->port->info;
    struct sonic_pkt_gen_info *gen = app->port->gen;

    struct sonic_mac_fifo_entry * buffer;

    struct ethhdr *eth;
    struct iphdr *iph;

    int b_num = gen->bw_num;
    int b_gap = gen->bw_gap;
    DECLARE_CLOCK;

    // Buffer
    buffer = (struct sonic_mac_fifo_entry *) ALLOC_PAGES(in_fifo->porder);
    if (!buffer) {
        SONIC_ERROR("No memory\n");
        goto end;
    }
    b_entry = (struct sonic_mac_fifo_entry *) buffer;
    b_packet = b_entry->packets;
    b_cnt = 0;

    START_CLOCK();

begin:
    mac_entry = (struct sonic_mac_fifo_entry *) get_read_entry(in_fifo);
    if (!mac_entry)
        goto end;

    pkt_cnt = mac_entry->pkt_cnt;
    packet = mac_entry->packets;

    SONIC_DDPRINT("%p %d\n", mac_entry, pkt_cnt);

    for ( j = 0 ; j < pkt_cnt ; j ++ ) {
        bytes += packet->len - 8;

        eth = (struct ethhdr *) (packet->buf + 8);
        iph = (struct iphdr *) (((uint8_t*)eth) + ETH_HLEN);

        if (iph->saddr == info->ip_src) {
            // TODO: Copy and change idles for the next packet
            b_cnt++;
            b_packet->len = packet->len;
            b_packet->idle = b_gap;
            memcpy(b_packet->buf, packet->buf, packet->len);

            packet->len = 0;

            if (b_cnt == b_num) {
//                SONIC_PRINT("Ha!\n");
                b_entry->pkt_cnt = b_cnt;
                t_entry = (struct sonic_mac_fifo_entry *) get_write_entry(out_fifo);
                if (!t_entry)
                    goto end;

                put_write_entry(out_fifo, b_entry);

                b_entry = t_entry;
//                b_entry = (struct sonic_mac_fifo_entry *) get_write_entry(out_fifo);
//               if (!b_entry)
//                  goto end;
                b_packet = b_entry->packets;
                b_cnt = 0;
            }
        }

        packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);
    }

    push_entry(in_fifo, out_fifo, mac_entry);

    fifo_cnt ++;

    if (unlikely(*stopper == 0))
        goto begin;
end:

//    FREE_PAGES(b_entry, in_fifo->porder);

    STOP_CLOCK(bytes * 8);
    SONIC_OUTPUT("[p%d] log_total = %llu fifo_cnt = %d log_list = %lld\n", app->port_id, 
            (unsigned long long) log_total, fifo_cnt, (unsigned long long )log_list);

    return 0;
}
