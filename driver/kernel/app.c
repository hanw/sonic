#if !SONIC_KERNEL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "list.h"
#include <arpa/inet.h>
#else
#include <linux/if_ether.h>
#endif /* SONIC_KERNEL */
#include "sonic.h"

int sonic_app_cap_loop(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(app, args);
    struct sonic_fifo *in_fifo = app->in_fifo;
    struct sonic_fifo *out_fifo = app->out_fifo;
    struct list_head *cap_head = &app->out_head;

    struct sonic_packets *packets;
    struct sonic_packet *packet;

    struct sonic_app_stat *stat = &app->stat;

    struct sonic_app_list *app_list;
    struct sonic_cap_entry *cap_entry;
    int i=0, cap_cnt=0, app_exp = app->app_exp;
    int cap_max = (order2bytes(app_exp) - sizeof(struct sonic_app_list))
                        / sizeof(struct sonic_cap_entry) - 1;   // TO be safe
    uint64_t tmp_idle=0, tmp_idle_bits=0;

    SONIC_DDPRINT("\n");

    app_list = (struct sonic_app_list *) ALLOC_PAGES(app_exp);
    if (!app_list) {
        SONIC_ERROR("out of memory\n");
        goto end;
    }
    cap_entry = (struct sonic_cap_entry *) app_list->entry;

    START_CLOCK_INTR();

begin:
    packets = (struct sonic_packets *) get_read_entry(in_fifo);
    if (!packets)
        goto end;

    FOR_SONIC_PACKETS(i, packets, packet) {
        if (packet->len == 0) {
            tmp_idle += packet->idle;
            tmp_idle_bits += packet->idle_bits;
            continue; 
        }

        stat->total_packets ++;
        stat->total_bytes += packet->len - 8;

        if (app_list) {
            memcpy(cap_entry, &packet->type, 16);
            cap_entry->idle += tmp_idle;
            cap_entry->idle_bits += tmp_idle_bits;
            memcpy(cap_entry->buf, packet->buf+PREAMBLE_LEN, 48);
            cap_entry ++;
            cap_cnt ++;
            tmp_idle = 0;
            tmp_idle_bits = 0;
        }

        if (cap_cnt == cap_max) {
            if (app_list) {
#if SONIC_KERNEL
                spin_lock_bh(&app->lock);
#endif /* SONIC_KERNEL */
                list_add_tail(&app_list->head, cap_head);
#if SONIC_KERNEL
                spin_unlock_bh(&app->lock);
#endif /* SONIC_KERNEL */
                app_list->cnt = cap_cnt;
            }
            app_list = (struct sonic_app_list *) ALLOC_PAGES(app_exp); 
            if (!app_list) {
                SONIC_ERROR("out of memory\n");
                cap_entry = NULL;
            } else {
                cap_entry = (struct sonic_cap_entry *) app_list->entry;
                cap_cnt = 0;
            }
        }
    }

//    put_read_entry(in_fifo, packets);
    push_entry(in_fifo, out_fifo, packets);

    if (likely(*stopper) == 0)
        goto begin;

end:
    if (app_list && cap_cnt && cap_cnt != cap_max) {
#if SONIC_KERNEL
        spin_lock_bh(&app->lock);
#endif /* SONIC_KERNEL */
        list_add_tail(&app_list->head, cap_head);
#if SONIC_KERNEL
        spin_unlock_bh(&app->lock);
#endif /* SONIC_KERNEL */
        app_list->cnt = cap_cnt;
    }

    STOP_CLOCK_INTR(stat);

    return 0;
}

int sonic_app_rpt_loop(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(app, args);
    struct sonic_fifo *out_fifo = app->out_fifo;
    struct sonic_port_info *info = &app->port->info;
    struct list_head *rpt_head = &app->in_head;

    struct sonic_packets *packets;
    struct sonic_packet *packet;

    struct sonic_app_stat *stat = &app->stat;

    struct sonic_app_list *app_list = NULL;
    struct sonic_rpt_entry *rpt_entry = NULL;

    uint64_t default_idle = power_of_two(out_fifo->exp) * 496;
    uint32_t *pcrc, crc;
    int i=0, rpt_cnt, cnt=0, tcnt;

    tcnt = sonic_prepare_pkt_gen_fifo(out_fifo, info);     // FIXME

#if SONIC_KERNEL
    spin_lock_bh(&app->lock);
#endif /* SONIC_KERNEL */
    if (!list_empty(rpt_head)) {
        app_list = list_entry(rpt_head->next, struct sonic_app_list, head);
        list_del(&app_list->head);
    }
#if SONIC_KERNEL
    spin_unlock_bh(&app->lock);
#endif /* SONIC_KERNEL */
    if (app_list) {
        rpt_cnt = app_list->cnt;
        rpt_entry = (struct sonic_rpt_entry*) app_list->entry;
    } else 
        rpt_cnt = 0;

    START_CLOCK_INTR();

    if (sonic_gen_idles(app->port, out_fifo, info->wait))
        goto end;

begin:
    packets = (struct sonic_packets *) get_write_entry(out_fifo);
    if (!packets)
        goto end;

    packets->pkt_cnt = tcnt;

    cnt = 0;
    FOR_SONIC_PACKETS(i, packets, packet) {
        if ( i == 0 ) 
            packet->len = info->pkt_len + 8;

        if (!rpt_cnt) {
            cnt = 1;
            packet->len = 0;
            packet->idle = default_idle;
            break;
        }

        packet->idle = rpt_entry->idle < 12 ? 12 : rpt_entry->idle;
//        packet->idle = rpt_entry->idle;

        sonic_update_csum_dport_id(packet->buf, rpt_entry->id, info->multi_queue, 
                info->port_dst);

        pcrc = (uint32_t *) CRC_OFFSET(packet->buf, packet->len);
        *pcrc = 0;

        crc = SONIC_CRC(packet) ^ 0xffffffff;
        *pcrc = crc;

        stat->total_bytes += packet->len;
        stat->total_packets ++;
        cnt ++;
        rpt_entry ++;

        rpt_cnt --;

        if (rpt_cnt == 0) {
            FREE_PAGES(app_list, app->app_exp);

            app_list = NULL;
#if SONIC_KERNEL
            spin_lock_bh(&app->lock);
#endif /* SONIC_KERNEL */
            if (!list_empty(rpt_head)) {
                app_list = list_entry(rpt_head->next, struct sonic_app_list, head);
                list_del(&app_list->head);
            }
#if SONIC_KERNEL
            spin_unlock_bh(&app->lock);
#endif /* SONIC_KERNEL */
            if (app_list) {
                rpt_cnt = app_list->cnt;
                rpt_entry = (struct sonic_rpt_entry*) app_list->entry;
            } else {
                rpt_cnt = 0;
                packets->pkt_cnt = cnt;
                break;

            }
        }
    }
    packets->pkt_cnt = cnt;

    put_write_entry(out_fifo, packets);
    if (*stopper == 0)
        goto begin;
end:
    if (rpt_cnt) 
        FREE_PAGES(app_list, app->app_exp);

    STOP_CLOCK_INTR(stat);

    return 0;
}

int sonic_app_vrpt_loop(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(app, args);
    struct sonic_fifo *out_fifo = app->out_fifo;
    struct sonic_port_info *info = &app->port->info;
    struct list_head *rpt_head = &app->in_head;

    struct sonic_packets *packets;
    struct sonic_packet *packet;

    struct sonic_app_stat *stat = &app->stat;

    struct sonic_app_list *app_list = NULL;
    struct sonic_rpt_entry *rpt_entry = NULL;

    uint64_t default_idle = power_of_two(out_fifo->exp) * 496;
    int i=0, rpt_cnt, cnt=0, tcnt=0;

    info->pkt_len = 1518;       // FIXME
    tcnt = sonic_prepare_pkt_gen_fifo(app->port->fifo[0], info);     // FIXME
    tcnt = sonic_prepare_pkt_gen_fifo(app->port->fifo[1], info);     // FIXME

#if SONIC_KERNEL
    spin_lock_bh(&app->lock);
#endif /* SONIC_KERNEL */
    if (!list_empty(rpt_head)) {
        app_list = list_entry(rpt_head->next, struct sonic_app_list, head);
        list_del(&app_list->head);
    }
#if SONIC_KERNEL
    spin_unlock_bh(&app->lock);
#endif /* SONIC_KERNEL */
    if (app_list) {
        rpt_cnt = app_list->cnt;
        rpt_entry = (struct sonic_rpt_entry*) app_list->entry;
    } else 
        rpt_cnt = 0;

    START_CLOCK_INTR();

    if (sonic_gen_idles(app->port, out_fifo, info->wait))
        goto end;

    SONIC_PRINT("???\n");

begin:
    packets = (struct sonic_packets *) get_write_entry(out_fifo);
    if (!packets)
        goto end;

    packets->pkt_cnt = tcnt;
    cnt = 0;
    FOR_SONIC_PACKETS(i, packets, packet) {

        if (!rpt_cnt) {
            cnt = 1;
            packet->len = 0;
            packet->idle = default_idle;
            packets->pkt_cnt = cnt;
            break;
        }

        packet->len = rpt_entry->len;
        packet->idle = rpt_entry->idle < 12 ? 12 : rpt_entry->idle;
        
        {
            struct iphdr *iph = (struct iphdr *) (packet->buf + PREAMBLE_LEN + ETH_HLEN);
            struct udphdr *udp = (struct udphdr *) (((uint8_t *) iph) + IP_HLEN);
            uint32_t *pid = (uint32_t *) (((uint8_t *) udp) + UDP_HLEN);

            iph->tot_len=htons(packet->len - PREAMBLE_LEN-ETH_HLEN -4);
            iph->check = 0;
            
            *pid = htonl(rpt_entry->id);

    //        SONIC_PRINT("%d %d %d %d\n", ntohl(*pid), i, packet->len, packet->idle);
            
            udp->len = htons(packet->len - PREAMBLE_LEN - ETH_HLEN - IP_HLEN - 4);

            udp->check = 0;
            udp->check = htons(udp_csum(udp, iph));

#if SONIC_KERNEL
            iph->check = ip_fast_csum(iph, iph->ihl);	// ip checksum */
#else
//            iph->check = csum(iph, iph->ihl*4);
#endif
        }

#if 0
        sonic_update_csum_dport_id(packet->buf, rpt_entry->id, info->multi_queue, 
                info->port_dst);

        pcrc = (uint32_t *) CRC_OFFSET(packet->buf, packet->len);
        *pcrc = 0;

        crc = SONIC_CRC(packet) ^ 0xffffffff;
        *pcrc = crc;
#endif

        stat->total_bytes += packet->len;
        stat->total_packets ++;
        cnt ++;
        rpt_entry ++;

        rpt_cnt --;

        if (rpt_cnt == 0) {
            FREE_PAGES(app_list, app->app_exp);

            app_list = NULL;
#if SONIC_KERNEL
            spin_lock_bh(&app->lock);
#endif /* SONIC_KERNEL */
            if (!list_empty(rpt_head)) {
                app_list = list_entry(rpt_head->next, struct sonic_app_list, head);
                list_del(&app_list->head);
            }
#if SONIC_KERNEL
            spin_unlock_bh(&app->lock);
#endif /* SONIC_KERNEL */
            if (app_list) {
                rpt_cnt = app_list->cnt;
                rpt_entry = (struct sonic_rpt_entry*) app_list->entry;
            } else {
                rpt_cnt = 0;
                packets->pkt_cnt = cnt;
                break;

            }
        }
    }
//    packets->pkt_cnt = cnt;

    put_write_entry(out_fifo, packets);
    if (*stopper == 0)
        goto begin;
end:
    if (rpt_cnt) 
        FREE_PAGES(app_list, app->app_exp);

    STOP_CLOCK_INTR(stat);

    return 0;
}

#if 0
// replaying sonic-captured data
int sonic_app_crpt_loop(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(app, args);
    struct sonic_fifo *out_fifo = app->out_fifo;
    struct sonic_port_info *info = &app->port->info;
    struct list_head *rpt_head = &app->in_head;

    struct sonic_packets *packets;
    struct sonic_packet *packet, *next_packet = NULL;

    struct sonic_app_stat *stat = &app->stat;

    struct sonic_app_list *app_list = NULL;
    struct sonic_cap_entry *cap_entry = NULL;

    uint64_t default_idle = power_of_two(out_fifo->exp) * 496;
    uint8_t * p= NULL ;
    int i=0, rpt_cnt, cnt=0, consumed;
    int bufsize = out_fifo->entry_size;

    info->pkt_len = 1518;   // FIXME
    sonic_prepare_pkt_gen_fifo(out_fifo, info);     // FIXME

#if SONIC_KERNEL
    spin_lock_bh(&app->lock);
#endif /* SONIC_KERNEL */
    if (!list_empty(rpt_head)) {
        app_list = list_entry(rpt_head->next, struct sonic_app_list, head);
        list_del(&app_list->head);
    }
#if SONIC_KERNEL
    spin_unlock_bh(&app->lock);
#endif /* SONIC_KERNEL */
    if (app_list) {
        rpt_cnt = app_list->cnt;
        cap_entry = (struct sonic_cap_entry*) app_list->entry;
    } else 
        rpt_cnt = 0;

    START_CLOCK_INTR();

    if (sonic_gen_idles(app->port, out_fifo, info->wait))
        goto end;

begin:
    packets = (struct sonic_packets *) get_write_entry(out_fifo);
    if (!packets)
        goto end;

    cnt = 0;
    packet= packets->packets;

    consumed = sizeof(struct sonic_packets);

    while (1) {
        if (!rpt_cnt) {
            cnt = 1;
            packet->len = 0;
            packet->idle = default_idle;
            break;
        }

        if (consumed + sizeof(struct sonic_packet ) + cap_entry->len > bufsize) 
            break;

        memcpy(&packet->type, cap_entry, 16);
        
        p = packet->buf;
        *(uint64_t *) p = 0xd555555555555500UL;

        memcpy(packet->buf+PREAMBLE_LEN, cap_entry->buf, 48);

        p = packet->buf + PREAMBLE_LEN + 48;
        if (cap_entry->len < 72)
            cap_entry->len = 72;
        for ( i = 0 ; i < cap_entry->len - 48 - PREAMBLE_LEN;  i ++) {
            p[i] = 0;
        }

        // UDP checksum
        {
            struct iphdr *ip = (struct iphdr *) (packet->buf + PREAMBLE_LEN + ETH_HLEN);
            struct udphdr *udp = (struct udphdr *) (((uint8_t *) ip) + IP_HLEN);
            
            udp->check = 0;
            udp->check = htons(udp_csum(udp, ip));
        }

        next_packet = (struct sonic_packet *) 
                (SONIC_ALIGN(uint8_t, packet->buf + cap_entry->len + 8, 16) - 8);

        packet->next = (uint8_t *) next_packet - (uint8_t *) packet;

        consumed += packet->next;
        
        stat->total_bytes += packet->len - PREAMBLE_LEN;
        stat->total_packets ++;
        cnt ++;
        cap_entry ++;

        rpt_cnt --;

        if (rpt_cnt == 0) {
            FREE_PAGES(app_list, app->app_exp);

            app_list = NULL;
#if SONIC_KERNEL
            spin_lock_bh(&app->lock);
#endif /* SONIC_KERNEL */
            if (!list_empty(rpt_head)) {
                app_list = list_entry(rpt_head->next, struct sonic_app_list, head);
                list_del(&app_list->head);
            }
#if SONIC_KERNEL
            spin_unlock_bh(&app->lock);
#endif /* SONIC_KERNEL */
            if (app_list) {
                rpt_cnt = app_list->cnt;
                cap_entry = (struct sonic_cap_entry*) app_list->entry;
            } else {
                rpt_cnt = 0;
                packets->pkt_cnt = cnt;
                break;

            }
        }
    }
    packets->pkt_cnt = cnt;

#if !SONIC_KERNEL && SONIC_DDEBUG
    sonic_print_hex(packets, bufsize, 32);
#endif

    put_write_entry(out_fifo, packets);
    if (*stopper == 0)
        goto begin;
end:
    if (rpt_cnt) 
        FREE_PAGES(app_list, app->app_exp);

    STOP_CLOCK_INTR(stat);

    return 0;
}
#endif /*crpt_loop */

#if 0
int sonic_app_interactive_loop (void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(app, args);
    struct sonic_fifo *in_fifo = app->in_fifo;
    struct sonic_fifo *out_fifo = app->out_fifo;
    struct list_head *read_head = &app->out_head;
    struct list_head *write_head = &app->in_head;

    struct sonic_packets *packets;
    struct sonic_packet *packet;

    struct sonic_app_stat *stat = &app->stat;

    struct sonic_app_list *app_list;

    int i = 0, app_exp = app->app_exp;
    uint64_t tmp_idle=0;

    SONIC_DDPRINT("\n");

    app_list = (struct sonic_app_list *) ALLOC_PAGES(app_exp);
    if (!app_list) {
        SONIC_ERROR ("out of memory\n");
        goto end;
    }

    START_CLOCK_INTR();

begin:
    packets = (struct sonic_packets *) get_read_entry (in_fifo);
    if (!packets)
        goto end;

    FOR SONIC_PACKETS(i, packets, packet) {
        if (packet-.len == 0) {
            tmp_idle += packet->idle;
            continue;
        }

        stat->total_packets ++;
        stat->total_bytes += packet->len - 8;

    }

    if (likely(*stopper) == 0)
        goto begin;
end:

    STOP_CLOCK_INTR(stat);

    return 0;
}
#endif
