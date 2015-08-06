#if SONIC_KERNEL
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#else /* SONIC_KERNEL */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#endif /* SONIC_KERNEL */

#include "sonic.h"
#include "crc32.h"
#include "mode.h"

void sonic_fill_dma_buffer_for_rx_test(struct sonic_pcs *pcs, 
        struct sonic_fifo *fifo)
{
    struct sonic_packets *packets;
    struct sonic_packet *packet;
    int i, j, first=0;

    for ( i = 0 ; fifo->size; i ++ ) {
        packets = (struct sonic_packets *) fifo->entry[i].data;
        FOR_SONIC_PACKETS(j, packets, packet) {
            if (first == 0) {
                packet->idle = 16;
                first = 1;
            }
            sonic_encode(pcs, packet);

            if ((pcs->dma_page_index == pcs->dma_buf_pages -1) 
                    && (pcs->dma_idx + (packet->len + packet->idle) / 8 + 1 
                    >= NUM_BLOCKS_IN_PAGE))  {
                pcs->dma_page->reserved = pcs->dma_idx;
                SONIC_PRINT("page_index = %d idx = %d\n", pcs->dma_page_index, pcs->dma_idx);
                return;
            }

            packet = SONIC_NEXT_PACKET(packet);
        }
    }
}

// Check the correctness of crc algorithms
int sonic_test_crc_value(struct sonic_priv *sonic)
{
    int crc[4], i=0, pkt_len, ret=0;
    unsigned char *buf, *p;
    struct sonic_port_info * info = &sonic->ports[0]->info;

    if (!(buf = (unsigned char *) ALLOC(2000))) {
        SONIC_ERROR("Memory alloc failed\n");
        return -1;
    }

    SONIC_PRINT("Test: Computing CRC values with different algorithms\n");
    SONIC_PRINT("      To make sure our algorithms work correctly\n");

    for ( pkt_len = 64 ; pkt_len <= 1518 ; pkt_len ++ ) {
        p = (SONIC_ALIGN(uint8_t, buf + PREAMBLE_LEN, 16) -8);
        sonic_fill_frame(info, p, pkt_len);

        crc[0] = crc32_le(0xffffffff, p + PREAMBLE_LEN, pkt_len-4);
        crc[1] = crc32_bitwise(0xffffffff, p + PREAMBLE_LEN, pkt_len-4);
        crc[2] = fast_crc(0xffffffff, p+PREAMBLE_LEN, pkt_len-4);
        crc[3] = fast_crc_nobr(0xffffffff, p+PREAMBLE_LEN, pkt_len-4);

        if (crc[0] == crc[1] && crc[1] == crc[2] && crc[2] == crc[3]) {
            SONIC_DPRINT("%4d matches %.8x\n", pkt_len, crc[0]);
        } else {
            SONIC_PRINT("%4d does not match %.8x %.8x %.8x %.8x\n", pkt_len, 
                    crc[0], crc[1], crc[2], crc[3]);
            i ++;
        }
    }

    if (i == 0) 
        SONIC_PRINT("No Errors\n");
    else {
        SONIC_PRINT("%d mismatches\n", i);
        ret = -1;
    }

    FREE(buf);
    return ret;
}

// CRC tester
int sonic_test_crc_performance(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(mac, args);
    struct sonic_fifo *out_fifo = mac->out_fifo;
    struct sonic_port_info *info = &mac->port->info;
    int i, pkt_len, crc_mode;
    struct sonic_packets *packets;
    struct sonic_packet *packet;
    struct sonic_mac_stat *stat = &mac->stat;
    uint32_t *pcrc, crc=0;

    pkt_len = info->pkt_len;
    crc_mode = info->gen_mode;

    SONIC_DPRINT("\n");

    sonic_prepare_pkt_gen_fifo(out_fifo, info);

    switch(crc_mode) {
    case 1:
        sonic_crc = crc32_le;
        break;
    case 2:
        sonic_crc = crc32_bitwise;
        break;
    case 3:
        sonic_crc = fast_crc;
        break;
    case 4:
        sonic_crc = fast_crc_nobr;
        break;
    }

    START_CLOCK( );
begin:
    packets = (struct sonic_packets *) get_write_entry_no_block(out_fifo);
    if (!packets)
        goto end;

#if SONIC_DDEBUG
//    sonic_print_hex(packets, fifosize, 32);
#endif

    FOR_SONIC_PACKETS(i, packets, packet) {
//        SONIC_DPRINT("%d %d %d\n", i, packet->len, packet->idle);
        pcrc = (uint32_t *) CRC_OFFSET(packet->buf, packet->len);
        *pcrc = 0;

        SONIC_CRC(packet);

        *pcrc = crc;
        stat->total_packets ++;
        stat->total_bytes += pkt_len;
    }

    put_write_entry_no_block(out_fifo);

    if (*stopper == 0)
        goto begin;
end:
    STOP_CLOCK(stat);
    return 0;
}

int sonic_rpt_helper(struct sonic_app *app, char *fname, int cnt)
{
    struct sonic_app_list *ins;
    struct sonic_rpt_entry *ins_entry;
    struct list_head *log_head = &app->in_head;
    char buf[256];
    FILE *fin;
    int first=0, i=0, total=0;
    SONIC_PRINT("\n");

    fin = fopen(fname, "r");

    ins = (struct sonic_app_list *) ALLOC_PAGES(app->app_exp);
    if (!ins) {
        SONIC_ERROR("out of memory\n");
        exit(EXIT_FAILURE);
    }


    ins_entry = (struct sonic_rpt_entry *) ins->entry;
    while(fgets(buf, 256, fin)) {
        int pid, ptype, plen, pidle;
        if (buf[0] == '#')
            continue;

        sscanf(buf, "%d %d %d %d", &pid, &ptype, &plen, &pidle);

//        SONIC_PRINT("%d %d %d %d\n", pid, ptype, plen, pidle);

        if (!first)
            if (ptype == 1)
                pidle=12;
        first = 1;


        ins_entry->type = ptype;
        ins_entry->len = plen;
        ins_entry->idle = pidle;
        ins_entry->id = pid;

        total++;
        i++;

        if (total == cnt)
            break;

        ins_entry++;

        if (i == order2bytes(app->app_exp) / sizeof(struct sonic_rpt_entry)-1) {
            ins->cnt = i;
            list_add_tail(&ins->head, log_head);
            ins = (struct sonic_app_list *) ALLOC_PAGES(app->app_exp);
            if (!ins) {
                SONIC_ERROR("out of memory\n");
                exit(EXIT_FAILURE);
            }
            i=0;
            ins_entry = (struct sonic_rpt_entry *) ins->entry;
        }
    }

    if (i) {
//       SONIC_PRINT("i = %d %p %p\n", i, log_head, &ins->head);
        ins->cnt = i;
        list_add_tail(&ins->head, log_head);
//        SONIC_PRINT("%d\n", list_empty(log_head));
    }

    SONIC_PRINT("Total processed=%d\n", total);

    return 0;
}

#if 0
int sonic_crpt_helper(struct sonic_app *app, char *fname, int cnt)
{
    struct sonic_app_list *ins;
    struct sonic_cap_entry *ins_entry;
    struct list_head *log_head = &app->in_head;
    char buf[256];
    FILE *fin;
    int first=0, i=0, total=0;
    SONIC_PRINT("\n");

    fin = fopen(fname, "r");

    ins = (struct sonic_app_list *) ALLOC_PAGES(app->app_exp);
    if (!ins) {
        SONIC_ERROR("out of memory\n");
        exit(EXIT_FAILURE);
    }

    ins_entry = (struct sonic_cap_entry *) ins->entry;
    while(fgets(buf, 256, fin)) {
        int pid, ptype, plen, pidle;
        if (buf[0] == '#')
            continue;

        sscanf(buf, "%d %d %d %d", &pid, &ptype, &plen, &pidle);

//        SONIC_PRINT("%d %d %d %d\n", pid, ptype, plen, pidle);

        if (!first)
            if (ptype == 1)
                pidle=12;
        first = 1;

#if 0
        ins_entry->type = ptype;
        ins_entry->len = plen;
        ins_entry->idle = pidle;
        ins_entry->id = pid;
#endif

        total++;
        i++;

        if (total == cnt)
            break;

        ins_entry++;

        if (i == order2bytes(app->app_exp) / sizeof(struct sonic_rpt_entry)-1) {
            ins->cnt = i;
            list_add_tail(&ins->head, log_head);
            ins = (struct sonic_app_list *) ALLOC_PAGES(app->app_exp);
            if (!ins) {
                SONIC_ERROR("out of memory\n");
                exit(EXIT_FAILURE);
            }
            i=0;
//            ins_entry = (struct sonic_rpt_entry *) ins->entry;
        }
    }

    if (i) {
//       SONIC_PRINT("i = %d %p %p\n", i, log_head, &ins->head);
        ins->cnt = i;
        list_add_tail(&ins->head, log_head);
//        SONIC_PRINT("%d\n", list_empty(log_head));
    }

    SONIC_PRINT("Total processed=%d\n", total);

    return 0;
}
#endif
