#if !SONIC_KERNEL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#else /* SONIC_KERNEL */
#include <asm/i387.h>
#endif /* SONIC_KERNEL */

#include "sonic.h"
#include "fifo.h"
#include "crc32.h"

//#define __PKT_NUM__	1000000
#define __PKT_NUM__	15000000
#define __IDLE__	48

void sonic_free_pkt_gen (struct sonic_port * port)
{
    //struct sonic_pkt_generator *gen = (struct sonic_pkt_generator *) (mac->data);
    struct sonic_pkt_generator *gen = port->gen;

    if (gen->msg)
        FREE(gen->msg);
    SONIC_DDPRINT("\n");
    FREE(gen);
}

//void sonic_prepare_pkt_gen (struct sonic_mac * mac, int batching)
void sonic_prepare_pkt_gen (struct sonic_pkt_generator * gen, 
        struct sonic_fifo *fifo, int batching)
{
//	struct sonic_pkt_generator * gen = (struct sonic_pkt_generator *) (mac->data);
//	struct sonic_fifo *fifo = mac->out_fifo;
	int i, fifosize;
	uint8_t * buf;

	SONIC_DDPRINT("%d  \n", batching);
	fifosize = fifo_entry_size(fifo);;

	for ( i = 0 ; i < fifo->size ; i ++ ) {
		buf = fifo->entry[i].data;
		gen->buf_batching = sonic_fill_buf_batching(&gen->info, buf, fifosize, batching, 
				gen->pkt_len, gen->idle);
	}

}

int sonic_alloc_pkt_gen (struct sonic_port *port)
{
	struct sonic_pkt_generator *gen;
	SONIC_DDPRINT("\n");

	gen = ALLOC(sizeof(struct sonic_pkt_generator));
	if (!gen) {
		SONIC_ERROR("mem alloc failed\n");
		return -1;
	}

	memcpy(&gen->info, &port->info, sizeof(struct sonic_udp_port_info));
	
	port->gen = gen;

	/* TODO: default values */
	gen->mode = 0;
	gen->pkt_len = 1518;
//	gen->pkt_num = 0;
	gen->pkt_num = __PKT_NUM__;	// 1508
//	gen->pkt_num = 15000000;	// 64
	gen->idle = __IDLE__;	// minimum

	return 0;
}
#undef __PKT_NUM__
#undef __IDLE__

int sonic_pkt_gen_set_len(struct sonic_mac *mac, int len)
{
	struct sonic_pkt_generator *gen;
	
	if (!mac->data)
		return -1;

	gen = (struct sonic_pkt_generator *) (mac->data);
	gen->pkt_len = len;

	return 0;
}

int sonic_pkt_gen_set_mode(struct sonic_mac *mac, int mode)
{
	struct sonic_pkt_generator *gen;
	
	if (!mac->data)
		return -1;

	gen = (struct sonic_pkt_generator *) (mac->data);
	gen->mode = mode;

	return 0;
}

int sonic_pkt_gen_set_pkt_num(struct sonic_mac *mac, int pkt_num)
{
	struct sonic_pkt_generator *gen;
	
	if (!mac->data)
		return -1;

	gen = (struct sonic_pkt_generator *) (mac->data);
	gen->pkt_num = pkt_num;

	return 0;
}

//int  sonic_pkt_gen_set(struct sonic_mac *mac, int mode, int len, struct tester_args *args)
int  sonic_pkt_gen_set(struct sonic_pkt_generator *gen, int mode, int len, struct tester_args *args)
{
//	struct sonic_pkt_generator *gen;
	
//	if (!mac->data)
//		return -1;

    SONIC_DDPRINT("mode = %d, len = %d, pkt_num = %d, idle = %d, chain = %d, chain_num = %d\n",
            mode, len, args->pkt_cnt, args->idle, args->chain_idle, args->chain_num);

//	gen = (struct sonic_pkt_generator *) (mac->data);
	gen->mode = mode;
	gen->pkt_len = len;
	gen->pkt_num = args->pkt_cnt;
	gen->idle = args->idle;
	gen->idle_chain = args->chain_idle;
	gen->wait = args->wait;
	gen->chain_num = args->chain_num;
	gen->multi_queue = args->multi_queue;
	gen->port_base = args->port_dst[0];	// FIXME
    gen->bw_gap = args->bw_gap;
    gen->bw_num = args->bw_num;

	gen->msg = ALLOC(strlen(args->covert_msg[0]));
	strcpy((char*)gen->msg, (char *)args->covert_msg);
	gen->msg_len = strlen(args->covert_msg[0]);
	gen->delta = args->delta;

	SONIC_DDPRINT("Covert message = %s, len = %d\n", gen->msg, gen->msg_len);

	return 0;
}

int sonic_pkt_generator_loop(void *args)
{
	struct sonic_mac *mac = (struct sonic_mac *) args;
	struct sonic_pkt_generator *gen = (struct sonic_pkt_generator *) (mac->data);
	struct sonic_fifo *fifo = mac->out_fifo;
	struct sonic_mac_fifo_entry *entry = NULL;
	struct sonic_packet_entry *packet;
	int i, pkt_len, id = 0, batching, fifosize, cnt=0, k=0;
	uint64_t default_idle = power_of_two(fifo->porder) * 496;
	uint64_t pkt_cnt = gen->pkt_num, fifo_cnt=0;
	uint64_t j=0;
//	uint64_t beginning_idles = 360 * 0x9502f90ULL;
	uint64_t beginning_idles = 8 * 0x9502f90ULL;
	uint8_t *p;
	const volatile int * const stopper = &mac->port->stopper;
	struct timespec first;
#if SONIC_SFP
	unsigned long flags;
#endif /* SONIC_SFP */
	DECLARE_CLOCK;

	SONIC_DDPRINT("\n");

	fifosize = fifo_entry_size(fifo);;

	pkt_len = gen->pkt_len;
	batching = gen->buf_batching;
	beginning_idles *= gen->wait;

	START_CLOCK( );

#if SONIC_SFP
	local_irq_save(flags);
	local_irq_disable();
#endif

	// TODO this is nasty
	while(beginning_idles > default_idle) {
		entry = (struct sonic_mac_fifo_entry *) get_write_entry(fifo);

		entry->pkt_cnt = 1;
		packet = entry->packets;
		packet->len = 0;
		packet->idle = default_idle;

		beginning_idles -= default_idle;

		put_write_entry(fifo, entry);

		if (*stopper != 0)
			goto end;

		k ++;
	}

//	SONIC_DPRINT("Generated the first packet\n", k);
	CLOCK(first);

begin:
	for ( i = 0 ; i < 1 ; i ++ ) {
		p = (uint8_t *) get_write_entry(fifo);
        if (!p)
//        if (unlikely(*stopper))
            goto end;
		
		entry = (struct sonic_mac_fifo_entry *) p;
		

		cnt = batching;
		if ( pkt_cnt != 0 && j + cnt >= pkt_cnt ) {
			cnt = pkt_cnt - j;
		}

		if (cnt) {
			packet = entry->packets;
			packet->len = pkt_len + 8 ;
			packet->idle = id==0? 0 : gen->idle;	//	
//			packet->idle = gen->idle;	//
			entry->pkt_cnt = cnt;
			id = sonic_update_buf_batching(p, fifosize, id, gen->multi_queue, gen->port_base);
			
		} else {
			entry->pkt_cnt = 1;
			packet = entry->packets;
			packet->len = 0;
			packet->idle = default_idle; 
		}

		j += cnt;
		fifo_cnt ++;

		put_write_entry(fifo, p);
	}
	
	if (unlikely(*stopper == 0))
		goto begin;
end:
#if SONIC_SFP
	local_irq_restore(flags);
	local_irq_enable();
#endif

	STOP_CLOCK( (uint64_t)(pkt_len +8)* (uint64_t) j * 8);
	SONIC_DPRINT("[p%d] First packet was generated at %d %lu\n", mac->port_id, (int)first.tv_sec, first.tv_nsec);
	SONIC_OUTPUT("[p%d] pkt len = %d cnt = %llu %llu \n", mac->port_id, pkt_len, (unsigned long long)j,
			(unsigned long long)fifo_cnt);
	return 0;
}

int sonic_pkt_generator_loop2(void *args)
{
	struct sonic_mac *mac = (struct sonic_mac *) args;
	struct sonic_pkt_generator *gen = (struct sonic_pkt_generator *) (mac->data);
	struct sonic_fifo *fifo = mac->out_fifo;
	struct sonic_mac_fifo_entry *entry = NULL;
	struct sonic_packet_entry *packet;
	int i, pkt_len, id = 0, batching, fifosize, k=0;
	uint64_t default_idle = power_of_two(fifo->porder) * 496;
//	uint64_t bursty_idle = gen->idle_chain;
//    uint64_t bursty_idle = 8*0x9502f90ULL;
	uint64_t fifo_cnt=0;
//	uint64_t j=0, t=0;
//	uint64_t beginning_idles = 360 * 0x9502f90ULL;
	uint64_t beginning_idles = 8 * 0x9502f90ULL;
	uint8_t *p;
	int offset=0;
	const volatile int * const stopper = &mac->port->stopper;
#if SONIC_SFP
	unsigned long flags;
#endif /* SONIC_SFP */
	DECLARE_CLOCK;

	SONIC_DPRINT(" %d %d\n", gen->wait, gen->buf_batching);

	fifosize = fifo_entry_size(fifo);;

	pkt_len = gen->pkt_len;
	batching = gen->buf_batching;
	beginning_idles *= gen->wait;

	START_CLOCK( );

#if SONIC_SFP
	local_irq_save(flags);
	local_irq_disable();
#endif

	// TODO this is nasty
	while(beginning_idles > default_idle) {
		entry = (struct sonic_mac_fifo_entry *) get_write_entry(fifo);

		entry->pkt_cnt = 1;
		packet = entry->packets;
		packet->len = 0;
		packet->idle = default_idle;

		beginning_idles -= default_idle;

		put_write_entry(fifo, entry);

		if (*stopper != 0)
			goto end;

		k ++;
	}

	SONIC_DDPRINT("%d\n", k);


begin:
	for ( i = 0 ; i < 1 ; i ++ ) {
		p = (uint8_t *) get_write_entry(fifo);
       if (!p)
        //if (unlikely(*stopper))
            goto end;
		
		entry = (struct sonic_mac_fifo_entry *) p;

		id = sonic_update_buf_batching2(p, fifosize, id, gen, &offset);
/*
		cnt = batching;
		if ( pkt_cnt != 0 && j + cnt >= pkt_cnt ) {
			cnt = pkt_cnt - j;
		}

        SONIC_DDPRINT("cnt = %d \n", cnt);

		if (cnt) {
			packet = entry->packets;
			packet->len = pkt_len + 8 ;
			packet->idle = gen->idle;	//
			entry->pkt_cnt = cnt;
			id = sonic_update_buf_batching2(p, fifosize, id, gen, &offset);
			
		} else {
            SONIC_DDPRINT("idle_chain = %d cnt = %d\n", bursty_idle, cnt);
			entry->pkt_cnt = 1;
			packet = entry->packets;
			packet->len = 0;
            packet->idle = bursty_idle;
//			packet->idle = default_idle; 
            j = 0;
		} 

		j += cnt;
*/
//		t += id;
		fifo_cnt ++;

		put_write_entry(fifo, p);
	}
	
	if (unlikely(*stopper == 0))
		goto begin;
end:
#if SONIC_SFP
	local_irq_restore(flags);
	local_irq_enable();
#endif

	STOP_CLOCK( (uint64_t)pkt_len * (uint64_t) id * 8);
	SONIC_OUTPUT("[p%d] pkt len = %d cnt = %llu %llu \n", mac->port_id, pkt_len, (unsigned long long)id,
			(unsigned long long)fifo_cnt);
	return 0;
}

int sonic_pkt_generator_covert_loop(void *args)
{
	struct sonic_mac *mac = (struct sonic_mac *) args;
	struct sonic_pkt_generator *gen = (struct sonic_pkt_generator *) (mac->data);
	struct sonic_fifo *fifo = mac->out_fifo;
	struct sonic_mac_fifo_entry *entry = NULL;
	struct sonic_packet_entry *packet;
	int i, pkt_len, id = 0, batching, fifosize, k=0;
	uint64_t default_idle = power_of_two(fifo->porder) * 496;
	uint64_t fifo_cnt=0;
//	uint64_t j=0,t=0;
//	uint64_t beginning_idles = 360 * 0x9502f90ULL;
	uint64_t beginning_idles = 8 * 0x9502f90ULL;
	uint8_t *p;
	const volatile int * const stopper = &mac->port->stopper;
#if SONIC_SFP
	unsigned long flags;
#endif /* SONIC_SFP */
	DECLARE_CLOCK;

	SONIC_DDPRINT("\n");

	fifosize = fifo_entry_size(fifo);;

	pkt_len = gen->pkt_len;
	batching = gen->buf_batching;
	beginning_idles *= gen->wait;

	START_CLOCK( );

#if SONIC_SFP
	local_irq_save(flags);
	local_irq_disable();
#endif

	// TODO this is nasty
	while(beginning_idles > default_idle) {
		entry = (struct sonic_mac_fifo_entry *) get_write_entry(fifo);

		entry->pkt_cnt = 1;
		packet = entry->packets;
		packet->len = 0;
		packet->idle = default_idle;

		beginning_idles -= default_idle;

		put_write_entry(fifo, entry);

		if (*stopper != 0)
			goto end;

		k ++;
	}

	SONIC_DPRINT("%d\n", k);


begin:
	for ( i = 0 ; i < 1 ; i ++ ) {
		p = (uint8_t *) get_write_entry(fifo);
        if (!p)
        //if (unlikely(*stopper))
            goto end;
		
		entry = (struct sonic_mac_fifo_entry *) p;
		
		id = sonic_update_buf_batching_covert(p, fifosize, id, gen);

		fifo_cnt ++;

		put_write_entry(fifo, p);
	}
	
	if (unlikely(*stopper == 0))
		goto begin;
end:
#if SONIC_SFP
	local_irq_restore(flags);
	local_irq_enable();
#endif

	STOP_CLOCK( (uint64_t)pkt_len * (uint64_t) id * 8);
	SONIC_OUTPUT("[p%d] pkt len = %d cnt = %llu %llu \n", mac->port_id, pkt_len, (unsigned long long)id,
			(unsigned long long)fifo_cnt);
	return 0;
}

int sonic_pkt_receiver_loop(void *args)
{
	struct sonic_mac *mac = (struct sonic_mac *) args;
	struct sonic_fifo *in_fifo = mac->in_fifo;
	struct sonic_fifo *out_fifo = mac->out_fifo;

	struct sonic_packet_entry *packet;
	struct sonic_mac_fifo_entry *mac_entry;

	uint32_t crc, crcerror=0;
	int i=0, pkt_cnt, fifosize, j=0;
	const volatile int * const stopper = &mac->port->stopper;
	uint64_t bytes=0, total=0;
	uint8_t *pcrc;
	int fifo_cnt=0;
	uint32_t lenerror=0;
#if SONIC_SFP
	unsigned long flags;
#endif /* SONIC_SFP */

	DECLARE_CLOCK;

	SONIC_DDPRINT("\n");

	fifosize = fifo_entry_size(in_fifo);

	START_CLOCK();

#if SONIC_SFP
	local_irq_save(flags);
	local_irq_disable();
#endif
begin:
	for ( i = 0 ; i < 1 ; i ++ ) {
		mac_entry = (struct sonic_mac_fifo_entry *) get_read_entry(in_fifo);
//        if (unlikely(*stopper != 0))
        if (!mac_entry)
            goto end;

		pkt_cnt = mac_entry->pkt_cnt;
		packet = mac_entry->packets;

		for ( j = 0 ; j < pkt_cnt ; j ++ ) {

			if (packet->len == 0) {
				packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);
				continue;
			}

			bytes += packet->len;	// PREAMBLE
			pcrc = packet->buf + packet->len - 4;


//			id = packet->buf + PAYLOAD_OFFSET;
			total ++;

			if (packet->len < 64) {
				SONIC_DDPRINT("ERROR %d %d %d\n", j, packet->len, pkt_cnt);
				packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);
				lenerror++;
				continue;
			}

			crc = ~ fast_crc(packet->buf + PREAMBLE_LEN, packet->len - PREAMBLE_LEN);

			if (crc)  {
				crcerror ++;
			}

			packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);
		}

		push_entry(in_fifo, out_fifo, mac_entry);

		fifo_cnt++;
	}

	if (unlikely(*stopper == 0))
		goto begin;
end:

#if SONIC_SFP
	local_irq_restore(flags);
	local_irq_enable();
#endif
	STOP_CLOCK(bytes * 8);
	SONIC_OUTPUT("[p%d] crcerror = %u lenerror = %u pkt_cnt = %llu fifo_cnt= %d\n", 
			mac->port_id, (unsigned)crcerror, (unsigned) lenerror, (unsigned long long)total, fifo_cnt);

	return 0;
}

// CRC tester
int sonic_tx_mac_tester(void *args)
{
	struct sonic_mac *mac = (struct sonic_mac *) args;
	struct sonic_pkt_generator *gen = (struct sonic_pkt_generator *) (mac->data);
	struct sonic_fifo *fifo = mac->out_fifo;
	int i, j = 0, k, pkt_len, batching, fifosize;
	struct sonic_mac_fifo_entry *packets;
	struct sonic_packet_entry *packet;
	uint8_t *p;
	uint32_t *pcrc, crc=0;//, crc2=0;
	const volatile int * const stopper = &mac->port->stopper;
#if SONIC_DISABLE_IRQ
	unsigned long flags;
#endif
	DECLARE_CLOCK;

	pkt_len = gen->pkt_len;
	fifosize = fifo_entry_size(fifo);;

	SONIC_DDPRINT("\n");

	START_CLOCK( );
#if SONIC_DISABLE_IRQ
	local_irq_save(flags);
	local_irq_disable();
#endif

begin:
	for ( i = 0 ; i < 50 ; i ++ ) {
		packets = (struct sonic_mac_fifo_entry *) get_write_entry_no_block(fifo);
		batching = packets->pkt_cnt;

#if SONIC_DDEBUG
		sonic_print_hex(packets, fifosize, 32);
#endif

#if  SONIC_KERNEL
//	kernel_fpu_begin();
#endif

		packet = packets->packets;
		p = packet->buf;

		for ( k = 0 ; k < batching ; k ++ ) {
			pcrc = (uint32_t *) CRC_OFFSET(p, packet->len);
			*pcrc = 0;

#if SONIC_DDEBUG
			SONIC_PRINT("%d %d\n", packet->len, packet->len- PREAMBLE_LEN);
			sonic_print_hex(p, packet->len, 32);
#endif
//			sonic_set_pkt_id(p + PREAMBLE_LEN, id++);
			sonic_set_pkt_id(p + PREAMBLE_LEN, 0);

//			SONIC_PRINT("%d %d %p %d\n", j, k, packet->buf, sizeof(struct sonic_packet_entry));

#if SONIC_CRC == SONIC_FASTCRC
			crc = fast_crc(p + PREAMBLE_LEN, packet->len - PREAMBLE_LEN);
#elif SONIC_CRC == SONIC_LOOKUPCRC
			crc = crc32_le(0xffffffff, p + PREAMBLE_LEN, packet->len - PREAMBLE_LEN - 4);
#elif SONIC_CRC == SONIC_BITCRC
			crc = crc32_bitwise(0xffffffff, p + PREAMBLE_LEN, packet->len - PREAMBLE_LEN - 4);
#elif SONIC_CRC == SONIC_FASTCRC_O
			crc = fast_crc_nobr(p + PREAMBLE_LEN, packet->len - PREAMBLE_LEN - 4);
#else
#error Unknown CRC
#endif
//			crc2 = fast_crc(p + PREAMBLE_LEN, packet->len - PREAMBLE_LEN);
//			if (crc != crc2) {
//				SONIC_ERROR("does not match %x %x\n", crc, crc2);
//			}

			*pcrc = crc;

//			packet = packet->next;
			packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);
			p = packet->buf;
		}

#if  SONIC_KERNEL
//	kernel_fpu_end();
#endif
		j += batching;

		put_write_entry_no_block(fifo);
	}

	if (*stopper == 0)
		goto begin;

#if SONIC_DISABLE_IRQ
	local_irq_restore(flags);
	local_irq_enable();
#endif
	STOP_CLOCK( (uint64_t)pkt_len * (uint64_t) j * 8);
	SONIC_OUTPUT("pkt len = %d cnt = %d\n", pkt_len, j);
	return 0;
}

int sonic_tx_mac_loop(void *args)
{
    struct sonic_mac *mac = (struct sonic_mac *) args;
    struct sonic_fifo *in_fifo = mac->in_fifo, *out_fifo = mac->out_fifo;
    struct sonic_mac_fifo_entry *mac_entry = NULL;
    struct sonic_packet_entry *packet;

    int i=0, j=0;
    uint8_t *p;
    uint32_t *pcrc, crc;
    uint64_t total_bytes=0, total_pkt_cnt=0;
    
    const volatile int *const stopper = &mac->port->stopper;

#if SONIC_SFP
    unsigned long flags;
#endif /* SONIC_SFP */
    DECLARE_CLOCK;

    SONIC_DDPRINT("\n");

    START_CLOCK();

#if SONIC_SFP
    local_irq_save(flags);
    local_irq_disable();
#endif

begin:
    for ( i = 0 ; i < 1 ; i ++ ) {
        mac_entry = (struct sonic_mac_fifo_entry *) get_read_entry(in_fifo);
//        if (unlikely(*stopper))
        if (!mac_entry)
            goto end;

        packet = mac_entry->packets;
        p = packet->buf;

        for ( j = 0 ; j < mac_entry->pkt_cnt ; j ++ ) {
//            SONIC_PRINT("len = %d idle = %d\n", packet->len, packet->idle);
            if (packet->len == 0) {
                packet = NEXT_PKT(packet);
                p = packet->buf;
                continue;
            }

            total_bytes += packet->len;
            total_pkt_cnt ++;

            pcrc = (uint32_t *) CRC_OFFSET(packet->buf, packet->len);

            crc = fast_crc(p + PREAMBLE_LEN, packet->len - PREAMBLE_LEN) ^ 0xffffffff;;

            *pcrc = crc;

            packet = NEXT_PKT(packet);
            p = packet->buf;
        }

        push_entry(in_fifo, out_fifo, mac_entry); 
//        put_read_entry(in_fifo, mac_entry);
    }

    if (unlikely(*stopper == 0))
        goto begin;
end:

#if SONIC_SFP
    local_irq_restore(flags);
    local_irq_enable();
#endif

    STOP_CLOCK( total_bytes * 8);
    SONIC_OUTPUT("[p%d] pkt_cnt= %llu\n", mac->port_id, 
            (unsigned long long) total_pkt_cnt);

    return 0;
}
