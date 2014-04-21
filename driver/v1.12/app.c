#if !SONIC_KERNEL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "list.h"

#endif /* SONIC_KERNEL */

#include "sonic.h"
#include "fifo.h"

void sonic_free_logs(struct sonic_logger *logger)
{
	struct sonic_log *log, *tmp;
	int i =0;

	list_for_each_entry_safe(log, tmp, &logger->log_head, head) {
		list_del(&log->head);
		FREE_PAGES(log, logger->log_porder);
		i++;
	}

//	SONIC_PRINT("%d\n", i);
}

void sonic_free_ins(struct sonic_logger *logger)
{
	struct sonic_ins *ins, *tmp;
	int i =0;

	list_for_each_entry_safe(ins, tmp, &logger->ins_head, head) {
		list_del(&ins->head);
		FREE_PAGES(ins, logger->log_porder);
		i++;
	}

//	SONIC_PRINT("%d\n", i);
}

int sonic_log_loop(void * args)
{
	struct sonic_logger *logger = (struct sonic_logger *) args;
	struct sonic_fifo *in_fifo = logger->in_fifo;
	struct sonic_fifo *out_fifo = logger->out_fifo;

	struct list_head * log_head = &logger->log_head;

	struct sonic_mac_fifo_entry *mac_entry;
	struct sonic_packet_entry *packet;
	const volatile int * const stopper = &logger->port->stopper;

	struct sonic_log *log;
	struct sonic_log_entry *log_entry;
	int log_cnt = 0;
	int log_porder = logger->log_porder;
	int log_max = order2bytes(log_porder) / sizeof(struct sonic_log_entry) -1;
	
	int pkt_cnt, j;
	uint64_t log_total = 0, bytes = 0, log_list = 0;
	int fifo_cnt = 0;
	int tmp_idle = 0, tmp_idle_bits =0;

	struct sonic_udp_port_info *info = &logger->port->info;
	struct timespec first;
	int is_first=0;

	DECLARE_CLOCK;

	SONIC_DDPRINT("\n");

	log = (struct sonic_log *) ALLOC_PAGES(log_porder);
	if (!log){ 
		SONIC_ERROR("out of memory\n");
		goto end;
	}

	log_entry = log->logs;

	START_CLOCK();

begin:
	mac_entry = (struct sonic_mac_fifo_entry *) get_read_entry(in_fifo);
	if (!mac_entry)
//	if (unlikely(*stopper != 0))
		goto end;

	pkt_cnt = mac_entry->pkt_cnt;
	packet = mac_entry->packets;

	SONIC_DDPRINT("%p %d\n", mac_entry, pkt_cnt);

	for ( j = 0 ; j < pkt_cnt ; j ++ ) {
		SONIC_DDPRINT("%d %d %d\n", packet->len, packet->idle, packet->idle_bits);
		// TODO
		if (packet->len == 0) {
			tmp_idle += packet->idle;
			tmp_idle_bits += packet->idle_bits;
			packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);
			continue;
		}

		bytes += packet->len - 8;

		if (!is_first) {
			struct ethhdr *eth = (struct ethhdr *) (packet->buf + 8); // FIXME
			struct iphdr *iph = (struct iphdr *) (((uint8_t*)eth) + ETH_HLEN);
			if (iph->daddr == info->ip_src) {
				CLOCK(first);
				is_first = 1;
			}
		}

#if 1
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
#endif
		log_total ++;

		packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);
	}

//	put_read_entry(in_fifo, mac_entry);
	push_entry(in_fifo, out_fifo, mac_entry);

//	log_total += pkt_cnt;
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

	STOP_CLOCK(bytes * 8);
	SONIC_PRINT("[p%d] First packet was received at %d %lu\n", logger->port_id, (int) first.tv_sec, first.tv_nsec);
	SONIC_OUTPUT("[p%d] log_total = %llu fifo_cnt = %d log_list = %lld\n", logger->port_id, 
			(unsigned long long) log_total, fifo_cnt, (unsigned long long )log_list);

	return 0;
}

int sonic_ins_loop(void * args)
{
    struct sonic_logger *logger = (struct sonic_logger *) args;
    struct sonic_fifo *out_fifo = logger->out_fifo;
    struct sonic_pkt_generator *gen = logger->port->gen;

    struct sonic_mac_fifo_entry *mac_entry;
    struct sonic_packet_entry *packet;
    const volatile int * const stopper = &logger->port->stopper;

    struct sonic_ins *ins=NULL;
    struct sonic_ins_entry *ins_entry=NULL;
    int ins_cnt = 0;
    int pkt_cnt, j, cnt=0;
    uint64_t ins_total = 0, bytes = 0;
    uint64_t default_idle = power_of_two(out_fifo->porder) * 496;
    uint64_t fifo_cnt = 0;
	uint64_t beginning_idles = 8 * 0x9502f90ULL;

    uint32_t *pcrc, crc;
    int batching;

    DECLARE_CLOCK;

    beginning_idles *= gen->wait;
    batching = gen->buf_batching;

    SONIC_DPRINT("\n");

#if SONIC_KERNEL
    spin_lock_bh(&logger->lock);
#endif
    if (!list_empty(&logger->ins_head)) {
        ins = list_entry(logger->ins_head.next, struct sonic_ins, head);
        list_del(&ins->head);
    }
#if SONIC_KERNEL
    spin_unlock_bh(&logger->lock);
#endif
    if (ins) {
        ins_cnt = ins->ins_cnt;
        ins_entry = ins->inss;
    }
    else {
        ins_cnt=0;
    }

    START_CLOCK();

	// TODO this is nasty
	while(beginning_idles > default_idle) {
		mac_entry = (struct sonic_mac_fifo_entry *) get_write_entry(out_fifo);
        if (!mac_entry)
            goto end;

		mac_entry->pkt_cnt = 1;
		packet = mac_entry->packets;
		packet->len = 0;
		packet->idle = default_idle;

		beginning_idles -= default_idle;

		put_write_entry(out_fifo, mac_entry);

		if (*stopper != 0)
			goto end;
	}

    SONIC_DPRINT(".\n");

begin:
    mac_entry = (struct sonic_mac_fifo_entry *) get_write_entry(out_fifo);
//    if (unlikely(*stopper != 0))
    if (!mac_entry)
        goto end;

    pkt_cnt = batching;
//    pkt_cnt = mac_entry->pkt_cnt;
    packet = mac_entry->packets;
    packet->len = gen->pkt_len+8; //FIXME

    SONIC_DDPRINT("%p %d\n", mac_entry, pkt_cnt);

    cnt = 0;

    for ( j = 0 ; j < pkt_cnt ; j ++ ) {
        if (!ins_cnt) {
            cnt = 1;
            packet->len=0;
            packet->idle = default_idle; // FIXME
            break;
        }


        packet->idle = ins_entry->idle;

        SONIC_DPRINT("%d %d %d %d\n", ins_entry->id, packet->len, packet->idle, packet->idle_bits);
        // update id and csum TODO
        sonic_update_csum_id(packet->buf, ins_entry->id, gen->multi_queue, gen->port_base);

        pcrc = (uint32_t *) CRC_OFFSET(packet->buf, packet->len);
        *pcrc=0;

        crc = fast_crc(packet->buf + PREAMBLE_LEN, packet->len - PREAMBLE_LEN) ^ 0xffffffff;;

        *pcrc = crc;

        bytes += packet->len;

        cnt++;
        ins_total ++;
        ins_entry ++;

        ins_cnt --;

        if (ins_cnt == 0) {
            FREE_PAGES(ins, logger->log_porder);

#if SONIC_KERNEL
            spin_lock_bh(&logger->lock);
#endif
            ins = NULL;
            if (!list_empty(&logger->ins_head)) {
                ins = list_entry(logger->ins_head.next, struct sonic_ins, head);
                list_del(&ins->head);
            }
#if SONIC_KERNEL
            spin_unlock_bh(&logger->lock);
#endif
            if (ins) {
                ins_cnt = ins->ins_cnt;
                ins_entry = ins->inss;
            }
            else {
                ins_cnt = 0;
                mac_entry->pkt_cnt = cnt;
                break;
            }
        }


        packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);
    }
#if SONIC_DDEBUG
    sonic_print_hex(mac_entry, fifo_entry_size(out_fifo), 32);
#endif
    mac_entry->pkt_cnt = cnt;

    put_write_entry(out_fifo, mac_entry);
//    push_entry(in_fifo, out_fifo, mac_entry);

    fifo_cnt ++;

    if (unlikely(*stopper == 0))
        goto begin;
end:

    // if anything left
    if (ins_cnt) { 
        FREE_PAGES(ins, logger->log_porder);
    }

#if SONIC_KERNEL
    spin_lock_bh(&logger->lock);
#endif
    while (!list_empty(&logger->ins_head)) {
        ins = list_entry(logger->ins_head.next, struct sonic_ins, head);
        list_del(&ins->head);
        FREE_PAGES(ins, logger->log_porder);
    }
#if SONIC_KERNEL
    spin_unlock_bh(&logger->lock);
#endif

    STOP_CLOCK(bytes * 8);
    SONIC_OUTPUT("[p%d] ins_total = %llu fifo_cnt = %llu \n", logger->port_id, 
            (unsigned long long) ins_total, (unsigned long long) fifo_cnt );

    return 0;
}
