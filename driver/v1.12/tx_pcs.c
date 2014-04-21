#if SONIC_KERNEL
#include <linux/slab.h>
#else /* SONIC_KERNEL */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#endif /* SONIC_KERNEL */

#include "sonic.h"
#include "fifo.h"

const static uint8_t sonic_pcs_terminal[] = {0x87, 0x99, 0xaa, 0xb4, 0xcc, 0xd2, 0xe1, 0xff};

unsigned long long int scrambler_debug=0;

// FIXME: Too expensive
static inline int sonic_fill_page(struct sonic_dma_page *page, int index, uint64_t sh, uint64_t payload)
{
	int sh_index, sh_offset, cur_sh;
	
#if 1
	// syncheader
	sh_index = index >> 4;
	sh_offset = index & 0xf;

	cur_sh = page->syncheaders[sh_index];
//#if !SONIC_KERNEL
	cur_sh = cur_sh | (sh << (sh_offset << 1));
//#endif

	// payloads
	page->syncheaders[sh_index] = cur_sh;
#endif

	page->payloads[index] = payload;

	return 0;
}
//static int mdebug=0;
#if 1
inline void
sonic_gearbox(struct sonic_pcs *pcs, uint64_t prefix, uint64_t payload)
{
	int i;

#if 0
	int sh_index, sh_offset, cur_sh;
	int index = pcs->dma_idx;
	
	// syncheader
	sh_index = index >> 4;
	sh_offset = index & 0xf;

//	if (unlikely(!sh_offset)) {
//		cur_sh = sh;
//	} else {
		cur_sh = pcs->dma_page->syncheaders[sh_index];
		cur_sh = cur_sh | (prefix << (sh_offset << 1));
//	}

	// payloads
	pcs->dma_page->syncheaders[sh_index] = cur_sh;

	pcs->dma_page->payloads[index] = payload;
#else
	sonic_fill_page(pcs->dma_page, pcs->dma_idx , prefix, payload);
#endif

	pcs->dma_idx ++;

	if (unlikely(pcs->dma_idx == NUM_BLOCKS_IN_PAGE)) {
		pcs->dma_page_index = (pcs->dma_page_index + 1) & (pcs->dma_buf_pages-1);
#if SONIC_SFP
		if (unlikely(pcs->dma_page_index == 0)) {
			sonic_dma_tx(pcs);
		}
#endif /* SONIC_SFP */
		pcs->dma_page = ((struct sonic_dma_page *) pcs->dma_cur_buf) + pcs->dma_page_index;
	
		pcs->dma_idx = 0;

		for ( i = 0 ; i < 31 ; i ++ ) {
			pcs->dma_page->syncheaders[i] = 0;
		}

#if !SONIC_KERNEL
//		sonic_print_hex(pcs->dma_cur_buf, pcs->dma_buf_size, 32);
//		exit(1);
#endif
	}
}
#else
inline void
sonic_gearbox(struct sonic_pcs *pcs, uint64_t prefix, uint64_t payload)
{
//	uint64_t * page = (uint64_t *) pcs->dma_page;
	struct sonic_dma_page *page = pcs->dma_page;

	page->payloads[pcs->dma_idx] = payload;

	pcs->sh[pcs->dma_idx] = prefix;
	pcs->dma_idx ++;

	if (unlikely(pcs->dma_idx == NUM_BLOCKS_IN_PAGE)) {
		int sh_index, sh_offset, sh;

		int i=32, j=0, k=0;
		uint64_t tmp=0; 

		for ( j = 0 ; j < NUM_BLOCKS_IN_PAGE; j ++) {
			sh_index = j >> 4;
			sh_offset = j & 0xf;

			sh = page->syncheaders[sh_index];
			sh |= pcs->sh[j] << (sh_offset << 1);

			if (unlikely(sh_offset == 15))
			page->syncheaders[sh_index] = sh;
		}

		pcs->dma_page_index = (pcs->dma_page_index + 1) & (pcs->dma_buf_pages-1);
#if SONIC_SFP
		if (unlikely(pcs->dma_page_index == 0))
			sonic_dma_tx(pcs);
#endif /* SONIC_SFP */
		pcs->dma_page = ((struct sonic_dma_page *) pcs->dma_cur_buf) + pcs->dma_page_index;
		pcs->dma_idx = 0;

		for ( i = 0 ; i < 31 ; i ++ ) {
			pcs->dma_page->syncheaders[i] = 0;
		}

//		sonic_print_hex(pcs->dma_cur_buf, pcs->dma_buf_size, 32);
#if !SONIC_KERNEL
//		exit(1);
#endif
	}
}

#endif

#if SONIC_SCRAMBLER == SONIC_BITWISE_SCRAMBLER
static inline uint64_t scrambler(struct sonic_pcs *pcs, uint64_t *state, 
		uint64_t syncheader, uint64_t payload)
{
	int i; 
	uint64_t in_bit, out_bit;
	uint64_t scrambled = 0x0;

	for ( i = 0 ; i < 64 ; i ++) {
		in_bit = (payload >> i) & 0x1;
		out_bit = (in_bit ^ (*state >> 38) ^ (*state >> 57)) & 0x1 ;
		*state = (*state << 1 ) | out_bit;
		scrambled |= (out_bit << i);
	}

	SONIC_DDPRINT("prefix = %x payload = %.16llx scrambled = %.16llx\n", prefix, payload, s);
#if SONIC_PCS_LEVEL == 1
	sonic_gearbox(pcs, prefix, scrambled);
#endif
//	scrambler_debug ++;
	return scrambled;
}
#elif SONIC_SCRAMBLER == SONIC_OPT_SCRAMBLER
static inline uint64_t scrambler(struct sonic_pcs *pcs, uint64_t *state,
		int prefix, uint64_t payload)
{
	register uint64_t s = *state, r;
	pcs->debug ++;

	r = (s >> 6) ^ (s >> 25) ^ payload;
	s = r ^ (r << 39) ^ (r << 58);

	*state = s;

	SONIC_DDPRINT("prefix = %x payload = %.16llx scrambled = %.16llx\n", prefix, payload, s);
#if SONIC_PCS_LEVEL == 1
	sonic_gearbox(pcs, prefix, s);
#endif
	return s;
}
#else /* SONIC_SCRAMBLER */
#error Unknown scrambler
#endif /* SONIC_SCRAMBLER */

uint64_t sonic_encode(struct sonic_pcs * pcs, uint64_t *state, uint8_t *data, int len, 
		int idles, unsigned beginning_idles)
{
	register uint64_t scrambled, tmp; 
	int ending_idles; 
#if! SONIC_KERNEL
	int idle_bits = (beginning_idles-1) * 8;
	int block_type = 0;

//	if (len)
//		SONIC_DPRINT("%d %d\n", idles, beginning_idles);
#endif
	beginning_idles = idles - beginning_idles;

	// DIC
	switch(beginning_idles % 4) {
	case 1:
		if (pcs->dic < 3) {
			beginning_idles --;
			pcs->dic ++;
		}
		else {
			beginning_idles += 3;
			pcs->dic = 0;
		}
		break;
	case 2:
		// FIXME
		if (pcs->dic < 2) {
			beginning_idles -= 2;
			pcs->dic += 2;
		}
		else  {
			beginning_idles += 2;
			pcs->dic -= 2;
		}
		break;
	case 3:
		if (pcs->dic == 0) {
			beginning_idles -= 3;
			pcs->dic = 3;
		} else {
			beginning_idles ++;
			pcs->dic --;
		}
	}


	while(beginning_idles >= 8) {
		scrambled = scrambler(pcs, state, 0x1, IDLE_FRAME);
		beginning_idles -= 8;
#if! SONIC_KERNEL
		idle_bits+= 66;
#endif
	}

	SONIC_DDPRINT("%d \n", beginning_idles);

	if (len == 0)
//		return 0;
		return beginning_idles;

#if SONIC_DEBUG
//	sonic_print_hex(data, len, 32);
//	SONIC_PRINT("%d %d %d\n", len, idles, beginning_idles);
#endif
	/* /S/ */
	if (beginning_idles == 4) {
//	if (beginning_idles && beginning_idles <=4) {
		tmp = *(uint64_t *) data;
		tmp <<= 32;
		tmp = 0x33 | tmp;

		data += 4;
		len -= 4;
#if! SONIC_KERNEL
		block_type = 1;
		idle_bits += 42;
#endif
	} else {
//		if (likely(beginning_idles != 0)) {
//			scrambled = scrambler(pcs, state, 0x1, IDLE_FRAME);
//		}
		tmp = *(uint64_t *) data;
		tmp = 0x78 | tmp;

		data += 8;
		len -= 8;
#if! SONIC_KERNEL
		idle_bits += 10;
#endif
	}
	scrambled = scrambler(pcs, state, 0x1, tmp);
#if! SONIC_KERNEL
//		SONIC_PRINT("idle_bits = %d %d\n", idle_bits, block_type);
#endif
	
	/* /D/ */
	while (likely(len >= 8)) {
		tmp = *(uint64_t *) data;
		scrambled = scrambler(pcs, state, 0x2, tmp);

		data += 8;
		len -= 8;
	}

	/* /T/ */
	tmp = 0 ;
	if (unlikely(len != 0)) {
		tmp = *(uint64_t *) data;
		tmp <<= (8-len) * 8;
		tmp >>= (7-len) * 8;
	}
	tmp |= sonic_pcs_terminal[len];
	
	scrambled = scrambler(pcs, state, 0x1, tmp);

	/* /E/ */
	ending_idles = (8 - len);

	return ending_idles;

//	ending_idles = idles - (8 - len);
//	beginning_idles = ending_idles;
//	return beginning_idles;
}

/* for packet generator */
int sonic_tx_pcs_loop(void *args)
{
	struct sonic_pcs *pcs = (struct sonic_pcs *) args;
	struct sonic_fifo *fifo = pcs->pcs_fifo;
	struct sonic_mac_fifo_entry *packets;
	struct sonic_packet_entry *packet;
	int j, pkt_cnt, cnt =0;
	int idles = 12, beginning_idles=0;
	uint64_t state = PCS_INITIAL_STATE, fifo_cnt=0;
	const volatile int * const stopper = &pcs->port->stopper;
	int dma_cnt=0;

#if SONIC_DISABLE_IRQ
	unsigned long flags;
#endif
	DECLARE_CLOCK;
	START_CLOCK();

#if SONIC_DISABLE_IRQ
	local_irq_save(flags);
	local_irq_disable();
#endif

	if (idles < 12)
		idles = 12;

	pcs->dma_idx = 0;		// FIXME

#if SONIC_SFP
	// FIXME
//	for ( i = 0 ; i < 1 ; i ++ ) 
//		for ( j =0 ; j < 317440 ; j ++ ) {
//		for ( j =0 ; j < 0x9502f90 ; j ++ ) {
//			scrambler(pcs, &state, 0x1, IDLE_FRAME);
//		}
#endif

begin:
	// 50 iterations at a time
//	for (i = 0 ; i < 50 ; i ++ ) {
		packets = (struct sonic_mac_fifo_entry *) get_read_entry(fifo);
        if (!packets)
//        if (unlikely(*stopper))
            goto end;

		pkt_cnt = packets->pkt_cnt;
		packet = packets->packets;

		for ( j = 0 ; j < pkt_cnt ; j ++) {
//			if (packet->len != 0)
				SONIC_DDPRINT("packet->len = %d packet->idle %d\n", packet->len, packet->idle);
			beginning_idles = sonic_encode(pcs, &state, packet->buf, 
					packet->len, packet->idle, beginning_idles);

			if (packet->len) {
//				SONIC_PRINT("%d %d\n", packet->len, packet->idle);
//				sonic_print_hex(packet->buf, packet->len, 32);
				cnt++;
			}

//			packet = packet->next;
			packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);

		}

//		cnt += pkt_cnt;
		fifo_cnt++;

		put_read_entry(fifo, packets);
		dma_cnt++;
//	}

	if (unlikely(*stopper == 0))
		goto begin;

end:
#if SONIC_SFP 
	for ( j =0 ; j < 31744 ; j ++ ) {
		scrambler(pcs, &state, 0x1, IDLE_FRAME);
	}
#endif

#if SONIC_DISABLE_IRQ
	local_irq_restore(flags);
	local_irq_enable();
#endif
	STOP_CLOCK(pcs->debug * 66);
	SONIC_OUTPUT("[p%d] scrambler_debug = %llu, %d  %x %d %llu\n", pcs->port_id, (unsigned long long)pcs->debug, cnt, cnt, 
			dma_cnt, (unsigned long long) fifo_cnt);

	scrambler_debug = 0;
	return 0;
}

int sonic_tx_pcs_tester(void *args)
{
	struct sonic_pcs * pcs = (struct sonic_pcs *) args;
	struct sonic_fifo *fifo = pcs->pcs_fifo;
	struct sonic_mac_fifo_entry *packets;
	struct sonic_packet_entry *packet = 0;
	int i, j, pkt_cnt, cnt =0;
//	int idles = 12;
	int beginning_idles=12;
//	int mode = pcs->mode;
	uint64_t state = PCS_INITIAL_STATE;
	const volatile int * const stopper = &pcs->port->stopper;
	uint64_t prevdebug = 0;

	DECLARE_CLOCK;
	START_CLOCK();

	SONIC_DDPRINT("%p\n", fifo);

begin:
	// 50 iterations at a time
	for (i = 0 ; i < 50 ; i ++ ) {
		packets = (struct sonic_mac_fifo_entry *) get_read_entry_no_block(fifo);

		pkt_cnt = packets->pkt_cnt;
		packet = packets->packets;

		for ( j = 0 ; j < pkt_cnt ; j ++) {
			prevdebug = scrambler_debug;
			beginning_idles = sonic_encode(pcs, &state, packet->buf, 
					packet->len, packet->idle, beginning_idles);
//			packet = packet->next;
			packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);

		}

		cnt += pkt_cnt;

		put_read_entry_no_block(fifo);

	}

	if (*stopper == 0)
		goto begin;

	STOP_CLOCK(pcs->debug * 66);
	SONIC_OUTPUT("pkt_len: %d, scrambler_debug = %llu, %d\n", packet->len-8, (unsigned long long)pcs->debug, cnt);

	scrambler_debug = 0;
	return 0;
}

static inline uint64_t __scrambler(uint64_t *state, uint64_t payload)
{
	register uint64_t s = *state, r;

	r = (s >> 6) ^ (s >> 25) ^ payload;
	s = r ^ (r << 39) ^ (r << 58);

	*state = s;
	return s;
}

int sonic_tx_pcs_scrambler_loop(void *args)
{
	struct sonic_pcs *pcs = (struct sonic_pcs *) args;
	uint64_t state = PCS_INITIAL_STATE, scrambled;
	uint64_t tcnt=0, dma_cnt=0;
	int i, j, k;
//	const int * const stopper = &path->stopper;
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

	SONIC_MPRINT(pcs, "[%d]", pcs->port_id);
	SONIC_MPRINT(pcs->dma_cur_buf, "[%d]", pcs->port_id);

begin:
	for ( i = 0 ; i < 1 ; i ++ ) {
		entry = (struct sonic_dma_page *) get_read_entry(pcs->pcs_fifo);
		tentry = (struct sonic_dma_page *) pcs->dma_cur_buf;

		for ( j = 0 ; j < pcs->dma_buf_pages ; j ++ ) {
			page = &entry[j];
			tpage = &tentry[j];

			for ( k = 0 ; k < 31 ; k ++ ) 
				tpage->syncheaders[k] = page->syncheaders[k];

			for ( k = 0 ; k < NUM_BLOCKS_IN_PAGE ; k ++ ) {
				scrambled = __scrambler(&state, page->payloads[k]);

				tpage->payloads[k] = scrambled;
			}
			tcnt+= NUM_BLOCKS_IN_PAGE;
		}

		dma_cnt ++;

		put_read_entry(pcs->pcs_fifo, entry);

#if SONIC_SFP
		sonic_dma_tx(pcs);
#endif
	}

	if (unlikely(*stopper == 0))
		goto begin;

#if SONIC_DISABLE_IRQ
	local_irq_restore(flags);
	local_irq_enable();
#endif

	STOP_CLOCK(tcnt * 66);
	SONIC_OUTPUT("[p%d] dma_cnt = %llu tcnt = %llu\n", pcs->port_id, (unsigned long long) dma_cnt, 
			(unsigned long long)tcnt);
	return 0;
}

#if SONIC_SFP
/* idle loop */
int sonic_tx_pcs_idle_loop(void *args)
{
	struct sonic_pcs *pcs = (struct sonic_pcs *) args;
	uint64_t state = PCS_INITIAL_STATE;
	uint64_t tcnt=0;
	uint64_t base = 0xDEADBEEFDEAD0000ULL, t;
	int i, j;
//	const int * const stopper = &path->stopper;
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

	SONIC_MPRINT(pcs, "[%d]", pcs->port_id);
	SONIC_MPRINT(pcs->dma_cur_buf, "[%d]", pcs->port_id);

begin:
	for ( i = 0 ; i < 50 ; i ++ ) {
		t = 0;
		for ( j = 0 ; j < 0xffff ; j ++ ) {
			scrambler(pcs, &state, 0x1, IDLE_FRAME);

			t = base + j;
//			scrambler(pcs, &state, 0x1, t);
			tcnt++;
		}
		



	}

	if (*stopper == 0)
		goto begin;

#if SONIC_DISABLE_IRQ
	local_irq_restore(flags);
	local_irq_enable();
#endif

	STOP_CLOCK(tcnt * 66);

	SONIC_OUTPUT("tcnt = %llu [p%d]\n", (unsigned long long)tcnt, pcs->port_id);

	scrambler_debug = 0;

	return 0;
}

int sonic_tx_dma_tester(void *args)
{
	struct sonic_pcs *pcs = (struct sonic_pcs *) args;
	const volatile int * const stopper = &pcs->port->stopper;
	uint64_t i, j =0;
#if SONIC_DISABLE_IRQ
	unsigned long flags;
#endif
	DECLARE_CLOCK;
	START_CLOCK();
	
#if SONIC_DISABLE_IRQ
	local_irq_save(flags);
	local_irq_disable();
#endif
	SONIC_DDPRINT("\n");

begin:
	for ( i = 0 ; i < DMA_ITERATIONS ; i ++ ) {
		sonic_dma_tx(pcs);
		j++;
	}

	if (*stopper == 0)
		goto begin;

#if SONIC_DISABLE_IRQ
	local_irq_restore(flags);
	local_irq_enable();
#endif
	STOP_CLOCK(j * pcs->dma_buf_pages * 496 * 66);
	SONIC_OUTPUT(" [p%d], %llu\n", pcs->port->port_id, j);
	return 0;
}
#endif /*SONIC_KERNEL*/
