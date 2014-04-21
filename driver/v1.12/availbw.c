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
    struct sonic_logger *logger = (struct sonic_logger *) args;
    struct sonic_fifo *in_fifo = logger->in_fifo;
    struct sonic_fifo *out_fifo = logger->out_fifo;

    struct sonic_mac_fifo_entry *mac_entry, *b_entry = NULL, *t_entry = NULL;
    struct sonic_packet_entry *packet, *b_packet;
    const volatile int * const stopper = &logger->port->stopper;

    struct list_head * log_head = &logger->log_head;
    struct sonic_log *log, *log_bw;
    struct sonic_log_entry *log_entry, *log_entry_bw;
    int log_cnt = 0, log_cnt_bw = 0;
    int log_porder = logger->log_porder;
    int log_max = order2bytes(log_porder) / sizeof(struct sonic_log_entry) -1;

    int pkt_cnt, j, b_cnt;
    uint64_t log_total = 0, bytes = 0, log_list = 0;
    uint64_t log_total_bw = 0, log_list_bw = 0;
    int fifo_cnt = 0;
    int tmp_idle = 0, tmp_idle_bits =0;

    struct sonic_udp_port_info *info = &logger->port->info;
    struct sonic_pkt_generator *gen = logger->port->gen;

    struct sonic_mac_fifo_entry * buffer;

    struct ethhdr *eth;
    struct iphdr *iph;

    int b_num = gen->bw_num;
    int b_gap = gen->bw_gap;
    DECLARE_CLOCK;

    SONIC_PRINT("\n");

    // Buffer
    buffer = (struct sonic_mac_fifo_entry *) ALLOC_PAGES2(in_fifo->porder);
    if (!buffer) {
        SONIC_ERROR("No memory\n");
        goto end;
    }
    b_entry = (struct sonic_mac_fifo_entry *) buffer;
    b_packet = b_entry->packets;
    b_cnt = 0;

    log = (struct sonic_log *) ALLOC_PAGES(log_porder);
    if (!log){ 
        SONIC_ERROR("out of memory\n");
        goto end;
    }
    log_entry = log->logs;

    log_bw = (struct sonic_log *) ALLOC_PAGES(log_porder);
    if (!log_bw){ 
        SONIC_ERROR("out of memory\n");
        goto end;
    }
    log_entry_bw = log_bw->logs;

    START_CLOCK();

begin:
    mac_entry = (struct sonic_mac_fifo_entry *) get_read_entry(in_fifo);
    if (!mac_entry)
        goto end;

    pkt_cnt = mac_entry->pkt_cnt;
    packet = mac_entry->packets;

    SONIC_DDPRINT("%p %d\n", mac_entry, pkt_cnt);

    for ( j = 0 ; j < pkt_cnt ; j ++ ) {
        if (packet->len == 0) {
            tmp_idle += packet->idle;
            tmp_idle_bits += packet->idle_bits;
            packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);
            continue;
        }
                    
        bytes += packet->len - 8;
        eth = (struct ethhdr *) (packet->buf + 8);
        iph = (struct iphdr *) (((uint8_t*)eth) + ETH_HLEN);
        
        if (iph->saddr == info->ip_src) {
            // TODO: Copy and change idles for the next packet
            b_cnt++;
            b_packet->len = packet->len;
            b_packet->idle = b_gap;
                    
            memcpy(b_packet->buf, packet->buf, packet->len);

            if (log_bw) {
                memcpy(log_entry_bw, &packet->type, 16);
                memcpy(log_entry_bw->buf, packet->buf + PREAMBLE_LEN, 48);
                log_entry_bw++;
                log_cnt_bw ++;
            }

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

                if(log_bw) {
#if SONIC_KERNEL
                    spin_lock_bh(&logger->lock);
#endif /* SONIC_KERNEL */
                    list_add_tail(&log->head, log_head);
                    log_list ++;
#if SONIC_KERNEL
                    spin_unlock_bh(&logger->lock);
#endif /* SONIC_KERNEL */
                    log_bw->log_cnt = log_cnt_bw;
                }

                log_bw = (struct sonic_log *) ALLOC_PAGES(log_porder);
                if (log_bw) {
                    log_entry_bw = log_bw->logs;
                    log_cnt_bw = 0;
                } else {
                    SONIC_ERROR("no memory\n");
                    log_entry_bw = NULL;
                }
            }
        } else {
            if (log) {
                memcpy(log_entry, &packet->type, 16);
                log_entry->idle += tmp_idle;
                log_entry->idle_bits += tmp_idle_bits;
                memcpy(log_entry->buf, packet->buf + PREAMBLE_LEN, 48);
                log_entry ++;
                log_cnt ++;
                tmp_idle = 0;
                tmp_idle_bits = 0;
            }
            if (log_cnt == log_max) {
                if(log) {
#if SONIC_KERNEL
                    spin_lock_bh(&logger->lock);
#endif /* SONIC_KERNEL */
                    list_add_tail(&log->head, log_head);
                    log_list ++;
#if SONIC_KERNEL
                    spin_unlock_bh(&logger->lock);
#endif /* SONIC_KERNEL */
                    log->log_cnt = log_cnt;
                }

                log = (struct sonic_log *) ALLOC_PAGES(log_porder);
                if (log) {
                    log_entry = log->logs;
                    log_cnt = 0;
                } else {
                    SONIC_ERROR("no memory\n");
                    log_entry = NULL;
                }
            }
        }

        log_total++;

        packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);
    }
    push_entry(in_fifo, out_fifo, mac_entry);
    
    fifo_cnt ++;

    if (unlikely(*stopper == 0))
        goto begin;
end:
    if (log && log_cnt && log_cnt != log_max) {
#if SONIC_KERNEL
        spin_lock_bh(&logger->lock);
#endif /* SONIC_KERNEL */
        list_add_tail(&log->head, log_head);
        log_list ++;
#if SONIC_KERNEL
        spin_unlock_bh(&logger->lock);
#endif /* SONIC_KERNEL */
        log->log_cnt = log_cnt;
    }
    if (log_bw && log_cnt_bw && log_cnt_bw != b_num) 
        FREE_PAGES(log_bw, log_porder);

//    FREE_PAGES(b_entry, in_fifo->porder);     // FIXME: memory leak

    STOP_CLOCK(bytes * 8);
    SONIC_OUTPUT("[p%d] b_cnt = %llu fifo_cnt = %d log_list = %lld\n", logger->port_id, 
            (unsigned long long) b_cnt, fifo_cnt, (unsigned long long )log_list);

    return 0;
}
