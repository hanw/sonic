#if !SONIC_KERNEL
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#endif /* SONIC_KERNEL */

#include "sonic.h"
#include "fifo.h"

unsigned long long int descrambler_debug = 0;
static unsigned long long int dummy = 0;
extern int packet_length;

#if SONIC_SCRAMBLER == SONIC_BITWISE_SCRAMBLER
static inline uint64_t descrambler(uint64_t *pstate, uint64_t payload)
{
	int i;
	uint64_t in_bit, out_bit;
	uint64_t state = * pstate;
	uint64_t descrambled = 0x0;

	for ( i = 0 ; i < 64 ; i ++ ) {
		in_bit = (payload >> i) & 0x1;
		out_bit = (in_bit ^ (state >> 38) ^ (state >> 57)) & 0x1;
		state = (state << 1) | in_bit;	// this part is different from scrambler
		descrambled |= (out_bit << i);
	}

	*pstate = state;
	return descrambled;
}

#elif SONIC_SCRAMBLER == SONIC_OPT_SCRAMBLER
static inline uint64_t descrambler(uint64_t *state, uint64_t payload)
{
	register uint64_t s = *state, r;

	r = (s >> 6) ^ (s >> 25) ^ payload;
	s = r ^ (payload << 39) ^ (payload << 58);

	*state = payload;
	SONIC_DDPRINT("state = %.16llx, payload = %.16llx\n", (unsigned long long)payload, 
			(unsigned long long)s);

	return s;
}
#else /* SONIC_SCRAMBLER */
#error Unknown scrambler
#endif /* SONIC_SCRAMBLER */

static inline int sonic_retrieve_page(struct sonic_dma_page *page, int index, struct sonic_pcs_block *block)
{
	int cur_sh;

	cur_sh = page->syncheaders[index>>4];
//	cur_sh = page->syncheaders[index/16];
	
	block->syncheader = (cur_sh >> ((index & 0xf) << 1)) & 0x3;
//	block->syncheader = (cur_sh >> ((index % 16) * 2)) & 0x3;
	block->payload = page->payloads[index];

//	page->payloads[index]=0;

	SONIC_DDPRINT("%x %.16llx\n", block->syncheader, block->payload);

	return 0;
}

static inline int sonic_decode_core(uint64_t *state, uint64_t syncheader, 
		uint64_t payload, struct sonic_packet_entry *packet, enum R_STATE *r_state)
{
	uint64_t descrambled;
	unsigned char *p = packet->buf + packet->len;
	int tail =0;

	SONIC_DDPRINT("packet = %p, p = %p, packet->len = %d\n", packet, p, packet->len);
	descrambled = descrambler(state, payload);
	SONIC_DDPRINT("%x %.16llx %.16llx %d\n", syncheader, descrambled, payload, packet->len);
#if SONIC_PCS_LEVEL == 0
	dummy += descrambled;
	return 0;
#endif

	/* /D/ */
	if (syncheader == 0x2) {
		if (!(*r_state & 0x10100)) {
			*r_state = RX_E;
			return -1;
		}
		
		* (uint64_t *) p = descrambled;
		packet->len += 8;
//		packet->packet_bits += 66;
		return 0;
	} 

	/* idles */
	if (descrambled == 0x1e) {
		packet->idle += 8;
		packet->idle_bits += 66;
		*r_state = RX_C;
		return 0;
	}
/*	if (descrambled == 0x1e 
		|| descrambled == 0x200004b
		|| descrambled == 0x0200000002000055ULL
		|| descrambled == 0x020000000000002dULL) {
//		packet -> idle = 0;
		packet->idle += 8;
		*r_state = RX_C;
		return 0;
	}
*/	

	switch(descrambled & 0xff) {
	case 0x66:	// TODO
	/* /S/ */
	case 0x33:
		if (!(*r_state & 0x11011)) {
			*r_state = RX_E;
			return -1;
		}
		SONIC_DDPRINT("START\n");

		descrambled >>= 32;
		* (uint64_t *) p = descrambled;
		p += 4;
		packet->len = 4;
		packet->type = 1;
		packet->idle += 4;
		packet->idle_bits += 42;
//		packet->packet_bits = 24;

		*r_state = RX_D;
		return 0;

	case 0x78:
		if (!(*r_state & 0x11011))  {
			*r_state = RX_E;
			return -1;
		}

		* (uint64_t *) p = descrambled;
		p += 8;
		packet->len = 8;
		packet->type = 2;
		packet->idle_bits += 10;
//		packet->packet_bits = 56;

		*r_state = RX_D;
		return 0;

	/* /T/ */
	case 0xff:
		tail ++;
	case 0xe1:
		tail ++;
	case 0xd2:
		tail ++;
	case 0xcc:
		tail ++;
	case 0xb4:
		tail ++;
	case 0xaa:
		tail ++;
	case 0x99:
		tail ++;
	case 0x87:
		if (*r_state != RX_D)  {
			*r_state = RX_E;
			return -1;
		}
		
		descrambled >>= 8;
		* (uint64_t *) p = descrambled;
		packet->len += tail;
//		packet->packet_bits += (tail << 3) + 10;

		*r_state = RX_T;
		return tail;

	case 0x2d:		// let's ignore ordered set for now
	case 0x55:
	case 0x4b:
	case 0x1e:
		if (!(*r_state & 0x11011))  {
			*r_state = RX_E;
			return -1;
		}

		packet->idle += 8;
		packet->idle_bits += 66;
		*r_state = RX_C;
		return 0;
	default:
		packet->idle += 8;
		packet->idle_bits += 66;
		*r_state = RX_E;
		return -1;
	}
}

inline int sonic_decode_no_fifo(uint64_t *state, void *buf, int buf_order,
		enum R_STATE *r_state, struct sonic_packet_entry *packet, int mode)
{
	int i, j =0, tail=0, tot=0, cnt; 
	int page_cnt = power_of_two(buf_order);
	struct sonic_dma_page *entry, *page = (struct sonic_dma_page *) buf;
	struct sonic_pcs_block block;
	SONIC_DDPRINT("buf_order = %d, page_cnt = %d\n", buf_order, page_cnt);

	entry = (struct sonic_dma_page *) buf;

	for ( i = 0 ; i < page_cnt ; i ++ ) {
		page = &entry[i];
		cnt= page->reserved;
		SONIC_DDPRINT("i = %d cnt = %d\n", i, cnt);
		
		for ( j = 0 ; j < cnt ; j ++ ) {
			sonic_retrieve_page(page, j, &block);
			tail = sonic_decode_core(state, block.syncheader, block.payload, packet, r_state);

//			if (*r_state== RX_E) {
//				SONIC_ERROR("ASDF %d\n", packet->len);
//			}

			if (unlikely(*r_state == RX_T)) {
				packet->len = 0;
				packet->idle = 7 - tail;
				tot ++;
			}
		}
	}
	return tot;
}

int sonic_rx_pcs_loop (void *args)
{
	struct sonic_pcs *pcs = (struct sonic_pcs *) args;
	uint64_t pkt_cnt = 0, dma_cnt = 0;//, total_pkt_cnt=0;
	uint64_t state = PCS_INITIAL_STATE;
	uint64_t i;
	enum R_STATE r_state = RX_INIT;
	int page_num = power_of_two(pcs->dma_buf_order);
	const volatile int * const stopper = &pcs->port->stopper;
	struct sonic_mac_fifo_entry *packets = NULL;
	struct sonic_packet_entry *packet = NULL, *tpacket;
	int tail = 0, cnt = 0, j, k;
	struct sonic_dma_page *entry, *page;
	struct sonic_pcs_block block;
//	int fifo_size = fifo_entry_size(pcs->pcs_fifo);
	int fifo_cnt=0;
	int e_state = 0;
	int fifo_required=0;
//	int tmp_idle=0, tmp_idle_bits=0;
	uint64_t total_pkt_cnt=0;

#if SONIC_SFP
	unsigned long flags;
#endif /* SONIC_SFP */
	DECLARE_CLOCK;

	SONIC_DDPRINT("\n");

	// warm up
#if SONIC_SFP
	sonic_dma_rx(pcs);
	cnt = NUM_BLOCKS_IN_PAGE;
#endif /*SONIC_SFP*/

//	packets = get_write_entry(pcs->pcs_fifo);
//	packets->pkt_cnt = 0;
//	packet = packets->packets;
//	packet->len = 0;
//	packet->idle = 0;

	START_CLOCK();

#if SONIC_SFP
	local_irq_save(flags);
	local_irq_disable();
#endif
begin:
	for ( i = 0 ; i < 1 ; i ++ ) {
#if SONIC_SFP
//		SONIC_PRINT("\n");
		sonic_dma_rx(pcs);
#endif /*SONIC_SFP*/
		entry = (struct sonic_dma_page *) pcs->dma_cur_buf;
		dma_cnt++;

		if (likely(fifo_required == 0)) {
			packets = get_write_entry(pcs->pcs_fifo);
            if (!packets)
                goto end;
			packet = packets->packets;
			packets->pkt_cnt = 0;
			packet->len = 0;
//			packet->idle = tmp_idle;
//			packet->idle_bits = tmp_idle_bits;
			packet->idle = 0;
			packet->idle_bits = 0;
			pkt_cnt = 0;

		}

		for ( j = 0 ; j < page_num ; j ++ ) {
			page = &entry[j];
#if !SONIC_SFP
			cnt = page->reserved;
#endif

			for ( k = 0 ; k < cnt ; k ++ ) {
				sonic_retrieve_page(page, k, &block);
				tail = sonic_decode_core(&state, block.syncheader, block.payload,
						packet, &r_state);

//				SONIC_PRINT("%d %d\n", j, k);

				if (r_state == RX_E) {
					SONIC_DDPRINT("ERROR %d %d\n", j, k);
					e_state ++;
				} 
				if (r_state == RX_T) {
					pkt_cnt ++;

					total_pkt_cnt ++;
					tpacket = (struct sonic_packet_entry *) (SONIC_ALIGN(uint8_t,
							packet->buf + packet->len + 8, 16) - 8);

//					if (((uint8_t *) tpacket) + 1600 > ((uint8_t *) packets) + fifo_size) {
					if (fifo_required) {
						packets->pkt_cnt = pkt_cnt;

						SONIC_DDPRINT("FIFO 1 %d\n", pkt_cnt);
						put_write_entry(pcs->pcs_fifo, packets);
						fifo_cnt ++;
						
						packets = get_write_entry(pcs->pcs_fifo);
                        if (!packets)
                            goto end;
						packet = packets->packets;
						packets->pkt_cnt = 0;
						pkt_cnt = 0;
						fifo_required = 0;
					} else {
//						packet->next = tpacket;
						packet->next = (uint8_t *) tpacket - (uint8_t *) packet;
						packet = tpacket;
					}

					packet->len = 0;
					packet->idle = 8 - tail;
					packet->idle_bits = (7-tail) << 3;
				}
			}
		}

		fifo_required = 1;
		if (r_state & 0x11010) {
			if (packet->len == 0)
				pkt_cnt ++;

			packets->pkt_cnt = pkt_cnt;

//			tmp_idle = packet->idle;
//			tmp_idle_bits = packet->idle_bits;

			SONIC_DDPRINT("FIFO 2 %d %d\n", fifo_cnt, pkt_cnt);
			put_write_entry(pcs->pcs_fifo, packets);
			fifo_cnt ++;
//		SONIC_PRINT("@@\n");

			fifo_required = 0;

//			packets = get_write_entry(pcs->pcs_fifo);
//		SONIC_PRINT("##\n");
//			packet = packets->packets;
//			packets->pkt_cnt = 0;
//			packet->len = 0;
//			packet->idle = 0;
//			packet->idle_bits = 0;
//			pkt_cnt = 0;

		} 

//		if (e_state) {
//			SONIC_PRINT("dma_cnt = %llu, e_state = %d\n", 
//				(unsigned long long) dma_cnt, e_state);
//			e_state =0 ;
//		}
#if !SONIC_SFP
		state = PCS_INITIAL_STATE;
		r_state = RX_INIT;
#endif
	}
#if !SONIC_SFP
	state = PCS_INITIAL_STATE;
	r_state = RX_INIT;
#endif

	if (unlikely(*stopper == 0))
		goto begin;
end:

#if SONIC_SFP
	local_irq_restore(flags);
	local_irq_enable();
#endif

	STOP_CLOCK(dma_cnt * pcs->dma_buf_pages * 496 * 66)

//	SONIC_OUTPUT("[p%d] dma_cnt = %llu fifo_cnt = %d e_state = %d\n", 
//			pcs->port_id, (unsigned long long) dma_cnt, fifo_cnt, e_state);
	SONIC_OUTPUT("[p%d] dma_cnt = %llu %llu %llx fifo_cnt = %d e_state = %d\n", 
			pcs->port_id, (unsigned long long) dma_cnt, (unsigned long long) total_pkt_cnt, 
					(unsigned long long) total_pkt_cnt, fifo_cnt, e_state);

	return 0;
}

int sonic_rx_pcs_tester(void *args)
{
	struct sonic_pcs *pcs = (struct sonic_pcs * )args;
	const volatile int * const stopper = &pcs->port->stopper;
	struct sonic_packet_entry *packet;
	uint64_t state = PCS_INITIAL_STATE;
	enum R_STATE r_state = RX_INIT;
	uint64_t i, cnt=0, dma_cnt=0;
	int mode = pcs->mode;
	DECLARE_CLOCK;

	START_CLOCK();

	packet = (struct sonic_packet_entry *) ALLOC_PAGE();
	if (!packet) {
		SONIC_ERROR("Memory alloc failed\n");
		return -1;
	}

	SONIC_DDPRINT("mode = %d %p\n", mode, stopper);

begin:
	for ( i = 0 ; i < 50 ; i ++ ){
#if SONIC_SFP
		sonic_dma_rx(pcs);
#endif /*SONIC_SFP*/

		cnt+=sonic_decode_no_fifo(&state, pcs->dma_cur_buf, pcs->dma_buf_order, &r_state, packet, mode);
		dma_cnt ++;
	}

#if !SONIC_KERNEL
	state = PCS_INITIAL_STATE;
//	r_state = RX_INIT;
#endif

	if (*stopper == 0)
		goto begin;

	STOP_CLOCK((uint64_t) dma_cnt * 496 * power_of_two(pcs->dma_buf_order) * 66)

	SONIC_OUTPUT(" %llu %llu\n", (unsigned long long)cnt, dummy);

	descrambler_debug = 0;

	return 0;
}

int sonic_rx_pcs_descrambler_loop(void *args)
{
	struct sonic_pcs *pcs = (struct sonic_pcs *) args;
	uint64_t state = PCS_INITIAL_STATE, descrambled;
	uint64_t tcnt=0, dma_cnt=0;
	int i, j, k;
	struct sonic_dma_page *page, *entry, *tpage, *tentry;

	const volatile int * const stopper = &pcs->port->stopper;
#if SONIC_DISABLE_IRQ
	unsigned long flags;
#endif
	DECLARE_CLOCK;
	START_CLOCK();
	
#if SONIC_DISABLE_IRQ
	local_irq_save(flags);
	local_irq_disable();
#endif
begin:
	tentry = (struct sonic_dma_page *)  get_write_entry(pcs->pcs_fifo);
	for ( i = 0 ; i < 2 ; i ++ ) {
#if SONIC_SFP
		sonic_dma_rx(pcs);
#endif
		entry = (struct sonic_dma_page *)  pcs->dma_cur_buf;

		for ( j = 0 ; j < pcs->dma_buf_pages ; j ++ ){
			page = &entry[j];
			tpage = &tentry[j + i * power_of_two(pcs->dma_buf_order)];

			for ( k = 0 ; k < 31 ; k ++ ) 
				tpage->syncheaders[k] = page->syncheaders[k];

			for ( k = 0 ; k < NUM_BLOCKS_IN_PAGE ; k ++ ) {
				descrambled = descrambler(&state, page->payloads[k]);

				tpage->payloads[k] = descrambled;
			}
			tcnt+= NUM_BLOCKS_IN_PAGE;
		}
		dma_cnt ++;
	}
	put_write_entry(pcs->pcs_fifo, tentry);
	
	if (unlikely(*stopper == 0))
		goto begin;

#if SONIC_DISABLE_IRQ
	local_irq_restore(flags);
	local_irq_enable();
#endif

	STOP_CLOCK(tcnt * 66);
	SONIC_OUTPUT("[p%d] dma_cnt = %llu tcnt = %llu\n", pcs->port_id, (unsigned long long) dma_cnt, 
			(unsigned long long)tcnt);

//	descrambler_debug = 0;

	return 0;
}


#if SONIC_SFP
int sonic_rx_pcs_idle_loop(void *args)
{
	struct sonic_pcs *pcs = (struct sonic_pcs *) args;
	uint64_t state = PCS_INITIAL_STATE, descrambled;
	uint64_t tcnt=0, error = 0;
	uint64_t prevpayload, prevdescrambled;
//	uint64_t pprevpayload, pprevdescrambled;
	int i, j, k, l=0;
//	int f=0;
	struct sonic_dma_page *page, *entry;
	struct sonic_pcs_block block;
	uint64_t ordered_4b=0, ordered_55 = 0, ordered_2d=0, idles =0;

	const volatile int * const stopper = &pcs->port->stopper;
#if SONIC_DISABLE_IRQ
	unsigned long flags;
#endif
	DECLARE_CLOCK;
	START_CLOCK();
	
#if SONIC_DISABLE_IRQ
	local_irq_save(flags);
	local_irq_disable();
#endif
begin:
	for ( i = 0 ; i < 50 ; i ++ ) {
#if SONIC_SFP
		sonic_dma_rx(pcs);
#endif
		entry = (struct sonic_dma_page *)  pcs->dma_cur_buf;
		l = 0;
		for ( j = 0 ; j < pcs->dma_buf_pages ; j ++ ){
			page = &entry[j];

			for ( k = 0 ; k < NUM_BLOCKS_IN_PAGE ; k ++ ) {
				sonic_retrieve_page(page, k, &block);

				descrambled = descrambler(&state, block.payload);

				if (descrambled == 0x1eULL) {
					idles ++;
				} else if ( descrambled == 0x200004b ) {
					ordered_4b ++;
				} else if ( descrambled == 0x0200000002000055ULL ) {
					ordered_55 ++; 
				} else if (descrambled	== 0x020000000000002dULL) {
					ordered_2d ++;
				} else {
					SONIC_DDPRINT("%d %d %d %.16llx %.16llx\n", i, j, k, block.payload, descrambled);
					l++;
					error++;
				}
				tcnt++;

				prevpayload = block.payload;
				prevdescrambled = descrambled;
			}
		}
		if (l) 
		SONIC_PRINT("[p%d] %d errors\n", pcs->port_id, l);
	}
	
	if (*stopper == 0)
		goto begin;

#if SONIC_DISABLE_IRQ
	local_irq_restore(flags);
	local_irq_enable();
#endif

	STOP_CLOCK(tcnt * 66);
	SONIC_PRINT("[p%d] %d errors %llu idles %llu ordered_4b %llu ordered_55 %llu ordered_2d\n", pcs->port_id,  l, idles, ordered_4b, ordered_55, ordered_2d);

	SONIC_OUTPUT("[p%d] error = %llu tcnt = %llu\n", pcs->port_id, (unsigned long long) error, (unsigned long long)tcnt);

//	descrambler_debug = 0;

	return 0;
}

int sonic_rx_dma_tester(void *args)
{
	struct sonic_pcs *pcs = (struct sonic_pcs *) args;
	const volatile int * const stopper = &pcs->port->stopper;
	uint64_t i, j=0;
#if SONIC_DISABLE_IRQ
	unsigned long flags;
#endif
	DECLARE_CLOCK;
	START_CLOCK();
	
#if SONIC_DISABLE_IRQ
	local_irq_save(flags);
	local_irq_disable();
#endif

begin:
	for ( i = 0 ; i < DMA_ITERATIONS ; i ++ ) {
		sonic_dma_rx(pcs);
		j++;
	}

	if (*stopper == 0)
		goto begin;

#if SONIC_DISABLE_IRQ
	local_irq_restore(flags);
	local_irq_enable();
#endif
	STOP_CLOCK(j * pcs->dma_buf_pages * 496 * 66);
	SONIC_OUTPUT(" [p%d]\n", pcs->port->port_id);
	return 0;
}
#endif /*SONIC_KERNEL*/
