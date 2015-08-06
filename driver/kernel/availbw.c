#if !SONIC_KERNEL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "list.h"

#endif /* SONIC_KERNEL */

#include "sonic.h"
#include "fifo.h"

int sonic_app_availbw_loop(void * args)
{
    SONIC_THREAD_COMMON_VARIABLES (app, args);
    struct sonic_fifo *in_fifo = app->in_fifo;
    struct sonic_fifo *out_fifo = app->out_fifo;

    struct sonic_packets *packets, *bpackets, *tpackets;
    struct sonic_packet *packet, *tpacket, *bpacket, *fpacket;
    struct sonic_app_stat *stat = &app->stat;

    struct sonic_port_info *info = &app->port->info;

    struct ethhdr *eth;
    struct iphdr *iph;

    int b_num = info->chain_num;
    int b_gap = info->chain_idle;
    int b_cnt=0, b_total=0;
    int i, tcnt=0;
    int skip=200;
    uint64_t chain_gap=0, old_chain_gap=0, remaining=0;
    uint64_t tmp_idle=0;
    uint64_t debug=0;
    uint64_t original=0, new=0;
    uint64_t original_total=0, new_total=0;

    info->pkt_len = 1518;
    info->idle = b_gap;

    SONIC_PRINT(" %d %d\n", info->chain_num, info->chain_idle);
    START_CLOCK();

    // Buffer
    bpackets = (struct sonic_packets *) ALLOC_PAGES(out_fifo->exp);
    if (!bpackets) {
        SONIC_ERROR("No memory\n");
        goto end;
    }

    sonic_fill_buf_batching(info, bpackets, out_fifo->entry_size); // FIXME: crc?

    bpacket= bpackets->packets;
    bpackets->pkt_cnt = b_num;
    b_cnt = 0;

    START_CLOCK();

begin:
    packets = (struct sonic_packets *) get_read_entry(in_fifo);
    if (!packets)
        goto end;

    tcnt = 0;
    fpacket = NULL;

    FOR_SONIC_PACKETS(i, packets, packet) {
        if (packet->len < 400) 
            continue;
        else if (packet->len >= 400)
            b_total++;

        if (b_total > skip) {
//          SONIC_PRINT("%d %d %d %d %d\n", b_cnt, b_total, remaining, packet->idle, packet->len);
            original += packet->idle;
//            original_total += packet->idle;
            new += b_gap + 16;
 //           new_total += b_gap + 16;

            // buffer the packet
            bpacket->len = packet->len;
            bpacket->idle = b_gap;
            memcpy(bpacket->buf+8, packet->buf+8, packet->len -8);
            bpacket = SONIC_NEXT_PACKET(bpacket);
            b_cnt ++;

            // mark the first packet  in the original fifo
            if (unlikely(b_cnt == 1)) {
                fpacket = packet;
            }
            packet->len = 0;
            packet->idle = 0;

            if (unlikely(b_cnt >= b_num )) {
                bpacket = bpackets->packets;
                if (new > original) {
                    SONIC_PRINT("%d %d\n", new, original);
                } else {
  //                  if (fpacket)
   //                     fpacket->idle = (original - new) + 16 * b_cnt;
    //                else
                        bpacket->idle += (original - new);
                    new_total += (original-new);
                    original = 0;
                    new = 0;
                }

//                fpacket->idle = 16 * b_cnt;

                tpackets = (struct sonic_packets *) get_write_entry(out_fifo);
                if (!tpackets)
                    goto end;

                put_write_entry(out_fifo, bpackets);

                bpackets = tpackets;

                sonic_fill_buf_batching(info, bpackets, out_fifo->entry_size); // FIXME: crc?

                bpacket = bpackets->packets;
                bpackets->pkt_cnt = b_num;
                b_cnt = 0;

            }
            tmp_idle = 0;
        }

    }

    if (b_cnt && fpacket) {
        fpacket->idle =  16 * b_num;
//        fpacket->idle = (original - new) + 16 * b_num;
//        original = 0;
 //       new = 0;
    }

    push_entry(in_fifo, out_fifo, packets);

    if (*stopper == 0)
        goto begin;
end:
    STOP_CLOCK(stat);
    FREE_PAGES(bpackets, in_fifo->exp);
    SONIC_PRINT("%d %d ori = %llu %llu new = %llu %llu\n", b_cnt, b_total, original, original_total, new, new_total);

    return 0;
}
