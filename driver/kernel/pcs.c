#if SONIC_KERNEL
#include <linux/slab.h>
#else /* SONIC_KERNEL */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#endif /* SONIC_KERNEL */

#include "sonic.h"

const static uint8_t sonic_pcs_terminal[] = {0x87, 0x99, 0xaa, 0xb4, 0xcc, 0xd2, 0xe1, 0xff};

unsigned long long int scrambler_debug=0;

// FIXME: Too expensive
static inline void 
sonic_fill_page(struct sonic_dma_page *page, int index, uint64_t sh, uint64_t payload)
{
    int sh_index, sh_offset, cur_sh;

    // syncheader
    sh_index = index >> 4;
    sh_offset = index & 0xf;

    cur_sh = page->syncheaders[sh_index];
    cur_sh = cur_sh | (sh << (sh_offset << 1));

    page->syncheaders[sh_index] = cur_sh;

    // payloads
    page->payloads[index] = payload;
}

inline void
sonic_gearbox(struct sonic_pcs *pcs, uint64_t prefix, uint64_t payload)
{
    int i;

    sonic_fill_page(pcs->dma_page, pcs->dma_idx , prefix, payload);

    pcs->dma_idx ++;

    if (unlikely(pcs->dma_idx == NUM_BLOCKS_IN_PAGE)) {
        pcs->dma_page_index = (pcs->dma_page_index + 1) & (pcs->dma_buf_pages-1);
#if SONIC_SFP
        if (unlikely(pcs->dma_page_index == 0)) 
            sonic_dma_tx(pcs);
#endif /* SONIC_SFP */
#if !SONIC_KERNEL
        pcs->dma_page->reserved = NUM_BLOCKS_IN_PAGE;
#endif /* SONIC_KERNEL */
        pcs->dma_page = ((struct sonic_dma_page *) pcs->dma_cur_buf) + pcs->dma_page_index;

        pcs->dma_idx = 0;

        for ( i = 0 ; i < 31 ; i ++ ) 
            pcs->dma_page->syncheaders[i] = 0;
    }
}

/* This is non-optimized version. I keep this here for history and comparison */
inline uint64_t 
scrambler_bitwise(struct sonic_pcs *pcs, uint64_t syncheader, uint64_t payload)
{
    int i; 
    uint64_t in_bit, out_bit;
    uint64_t scrambled = 0x0;

    for ( i = 0 ; i < 64 ; i ++) {
        in_bit = (payload >> i) & 0x1;
        out_bit = (in_bit ^ (pcs->state >> 38) ^ (pcs->state >> 57)) & 0x1 ;
        pcs->state = (pcs->state << 1 ) | out_bit;
        scrambled |= (out_bit << i);
    }

    SONIC_DDPRINT("syncheader = %x payload = %.16llx scrambled = %.16llx\n", 
            (int)syncheader, (unsigned long long)payload, (unsigned long long)pcs->state);

    sonic_gearbox(pcs, syncheader, scrambled);

    return scrambled;
}

inline uint64_t
descrambler_bitwise(struct sonic_pcs *pcs, uint64_t payload)
{
    int i;
    uint64_t in_bit, out_bit;
    uint64_t state = pcs->state;
    uint64_t descrambled = 0x0;

    for ( i = 0 ; i < 64 ; i ++ ) {
        in_bit = (payload >> i) & 0x1;
        out_bit = (in_bit ^ (state >> 38) ^ (state >> 57)) & 0x1;
        state = (state << 1) | in_bit;	// this part is different from scrambler
        descrambled |= (out_bit << i);
    }

    pcs->state = state;
    return descrambled;
}

/* Optimized version */
static inline uint64_t 
scrambler(struct sonic_pcs *pcs, uint64_t syncheader, uint64_t payload)
{
    register uint64_t s = pcs->state, r;
//    pcs->debug ++;
    pcs->stat.total_blocks++;

    r = (s >> 6) ^ (s >> 25) ^ payload;
    s = r ^ (r << 39) ^ (r << 58);

    pcs->state = s;

    SONIC_DDPRINT("syncheader = %x payload = %.16llx scrambled = %.16llx\n", 
            (int)syncheader, (unsigned long long) payload, (unsigned long long) s);

    sonic_gearbox(pcs, syncheader, s);

    return s;
}

static inline uint64_t
descrambler(struct sonic_pcs *pcs, uint64_t payload)
{
    register uint64_t s = pcs->state, r;
//    pcs->debug ++;
    pcs->stat.total_blocks++;

    r = (s >> 6) ^ (s >> 25) ^ payload;
    s = r ^ (payload << 39) ^ (payload << 58);

    pcs->state = payload;
    SONIC_DDPRINT("state = %.16llx, payload = %.16llx\n", 
            (unsigned long long)payload, (unsigned long long)s);

    return s;
}

static inline int sonic_retrieve_from_dma_page(struct sonic_dma_page *page, 
        int index, struct sonic_pcs_block *block)
{
    register int cur_sh;

    cur_sh = page->syncheaders[index>>4];

    block->syncheader = (cur_sh >> ((index & 0xf) << 1)) & 0x3;
    block->payload = page->payloads[index];

    SONIC_DDPRINT("%x %.16llx\n", block->syncheader, 
            (unsigned long long) block->payload);

    return 0;
}

#define sonic_retrieve_block(pcs, idx, block)\
    sonic_retrieve_from_dma_page(pcs->dma_page, idx, block);

inline void sonic_encode(struct sonic_pcs * pcs,  struct sonic_packet *packet)
{
    register uint64_t tmp; 
//    int ending_idles; 
    int idles = packet->idle;
    int len = packet->len;
    uint8_t *data = packet->buf;

    if (packet->idle == 0 && packet->len == 0)
        return;
//    if (packet->idle != 31744)
//    SONIC_PRINT("idle=%d len=%d begin=%d\n", idles, len, pcs->beginning_idles);

    pcs->beginning_idles = idles - pcs->beginning_idles;

    // DIC
    switch(pcs->beginning_idles % 4) {
    case 1:
        if (pcs->dic < 3) {
            pcs->beginning_idles --;
            pcs->dic ++;
        }
        else {
            pcs->beginning_idles += 3;
            pcs->dic = 0;
        }
        break;
    case 2:
        // FIXME
        if (pcs->dic < 2) {
            pcs->beginning_idles -= 2;
            pcs->dic += 2;
        }
        else  {
            pcs->beginning_idles += 2;
            pcs->dic -= 2;
        }
        break;
    case 3:
        if (pcs->dic == 0) {
            pcs->beginning_idles -= 3;
            pcs->dic = 3;
        } else {
            pcs->beginning_idles ++;
            pcs->dic --;
        }
    }

    SONIC_DDPRINT("after DIC=%d begin=%d\n", pcs->dic, pcs->beginning_idles);

    while(pcs->beginning_idles >= 8) {
        scrambler(pcs, 0x1, IDLE_FRAME);
        pcs->beginning_idles -= 8;
    }

    SONIC_DDPRINT("%d \n", pcs->beginning_idles);

    if (len == 0)
        return; 

    /* /S/ */
    if (pcs->beginning_idles == 4) {
        tmp = *(uint64_t *) data;
        tmp <<= 32;
        tmp = 0x33 | tmp;

        data += 4;
        len -= 4;
    } else {
        tmp = *(uint64_t *) data;
        tmp = 0x78 | tmp;

        data += 8;
        len -= 8;
    }
    scrambler(pcs, 0x1, tmp);

    /* /D/ */
    while (likely(len >= 8)) {
        tmp = *(uint64_t *) data;
        scrambler(pcs, 0x2, tmp);

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

    scrambler(pcs, 0x1, tmp);

    /* /E/ */
    pcs->beginning_idles = (8 - len);
    SONIC_DDPRINT("end=%d\n", pcs->beginning_idles);
}

int inline sonic_decode_core(struct sonic_pcs *pcs, struct sonic_pcs_block *block, 
        struct sonic_packet *packet)
{
    uint64_t descrambled;
    unsigned char *p = packet->buf + packet->len;
    int tail =0;

    SONIC_DDPRINT("packet = %p, p = %p, packet->len = %d\n", packet, p, packet->len);
    descrambled = descrambler(pcs, block->payload);

    /* /D/ */
    if (block->syncheader == 0x2) {
        if (!(pcs->r_state & 0x10100)) {
            pcs->r_state = RX_E;
            return -1;
        }
        
        * (uint64_t *) p = descrambled;
        packet->len += 8;
        return 0;
    } 

    /* idles */
    if (descrambled == 0x1e) {
        packet->idle += 8;
        packet->idle_bits += 66;
        pcs->r_state = RX_C;
        return 0;
    }

    switch(descrambled & 0xff) {
    case 0x66:  // TODO
    /* /S/ */
    case 0x33:
        if (!(pcs->r_state & 0x11011)) {
            pcs->r_state = RX_E;
            return -1;
        }

        descrambled >>= 32;
        * (uint64_t *) p = descrambled;
        p += 4;
        packet->len = 4;
        packet->type = 1;
        packet->idle += 4;
        packet->idle_bits += 42;

        pcs->r_state = RX_D;
        return 0;

    case 0x78:
        if (!(pcs->r_state & 0x11011))  {
            pcs->r_state = RX_E;
            return -1;
        }

        * (uint64_t *) p = descrambled;
        p += 8;
        packet->len = 8;
        packet->type = 2;
        packet->idle_bits += 10;

        pcs->r_state = RX_D;
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
        if (pcs->r_state != RX_D)  {
            pcs->r_state = RX_E;
            return -1;
        }
        descrambled >>= 8;
        * (uint64_t *) p = descrambled;
        packet->len += tail;

        pcs->r_state = RX_T;
        return tail;

    /* Ordered Sets and /E/ */
    case 0x2d:      // let's ignore ordered set for now
    case 0x55:
    case 0x4b:
    case 0x1e:
        if (!(pcs->r_state & 0x11011))  {
            pcs->r_state = RX_E;
            return -1;
        }
        packet->idle += 8;
        packet->idle_bits += 66;
        pcs->r_state = RX_C;
        return 0;
    default:
        packet->idle += 8;
        packet->idle_bits += 66;
        pcs->r_state = RX_E;
        return -1;
    }
}

inline int sonic_decode(struct sonic_pcs *pcs, int idx, struct sonic_packet *packet)
{
    struct sonic_pcs_block block;
    sonic_retrieve_block(pcs, idx, &block);
    return sonic_decode_core(pcs, &block, packet);
}

int sonic_pcs_tx_loop(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(pcs, args);
    struct sonic_fifo *in_fifo = pcs->in_fifo;
    struct sonic_packets *packets;
    struct sonic_packet *packet;
    struct sonic_pcs_stat *stat = &pcs->stat;
    int i;
#if SONIC_DDEBUG
    struct iphdr *iph;
    struct udphdr *uh;
    uint32_t *pid;
#endif /* SONIC_DDEBUG */

    SONIC_DDPRINT("\n");

    START_CLOCK( );

begin:
    packets = (struct sonic_packets *) get_read_entry(in_fifo);
    if (!packets)
        goto end;

    FOR_SONIC_PACKETS(i, packets, packet) {
        sonic_encode(pcs, packet);

//        if (packet->len == 1526)
//            SONIC_PRINT("idle = %d\n", packet->idle);

        if (packet->len) {
            stat->total_packets ++;
//            total_pkt++;
#if SONIC_DDEBUG
            iph = (struct iphdr *) (packet->buf + PREAMBLE_LEN + ETH_HLEN);
            uh = (struct udphdr *) (((uint8_t *)iph) + IP_HLEN);
            pid = (uint32_t *) (((uint8_t *) uh) + UDP_HLEN);

            SONIC_PRINT("%d %d %d %d \n", i, ntohl(*pid), packet->len, packet->idle);
#endif /* SONIC_DDEBUG */
        }
    }

    put_read_entry(in_fifo, packets);

    if (*stopper == 0)
        goto begin;

end:

//    STOP_CLOCK(pcs->debug * 66);
    STOP_CLOCK(stat);
//    SONIC_OUTPUT(pcs, "%u\n", (unsigned) total_pkt);

    return 0;
}

int sonic_pcs_rx_loop(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(pcs, args);
    struct sonic_fifo *out_fifo = pcs->out_fifo;
    struct sonic_packets *packets = NULL;
    struct sonic_packet *packet = NULL, *tpacket = NULL;
    struct sonic_pcs_stat *stat = &pcs->stat;
    int i, j, cnt, fifo_required=0, pkt_cnt=0, tail=0;
    uint64_t e_state=0;
#if !SONIC_KERNEL
    struct sonic_port_info *info = &pcs->port->info;
#endif /* SONIC_KERNEL */

    SONIC_DDPRINT("\n");

#if !SONIC_KERNEL
    sonic_prepare_pkt_gen_fifo(pcs->port->fifo[3], info);
    /* fill DMA buffer */
    sonic_fill_dma_buffer_for_rx_test(pcs, pcs->port->fifo[3]);
#else /* SONIC_KERNEL */
    sonic_dma_rx(pcs);
    cnt = NUM_BLOCKS_IN_PAGE;
#endif /* SONIC_KERNEL */

    START_CLOCK();

begin:
#if SONIC_SFP
    sonic_dma_rx(pcs);
#endif /* SONIC_SFP */

    if (likely(fifo_required == 0)) {
        packets = (struct sonic_packets *) get_write_entry(out_fifo);
        if (!packets)
            goto end;

        packets->pkt_cnt = 0;
        packet = packets->packets;
        packet->len = 0;
        packet->idle = 0;
        packet->idle_bits = 0;
        pkt_cnt = 0;
    }

    for (i = 0 ; i < pcs->dma_buf_pages; i ++) {
        pcs->dma_page = ((struct sonic_dma_page*) pcs->dma_cur_buf) + i;
#if !SONIC_SFP
        cnt = pcs->dma_page->reserved;
#endif /* !SONIC_SFP */
        for ( j = 0 ; j < cnt ; j ++) {
            tail = sonic_decode(pcs, j, packet);
            if (pcs->r_state == RX_E) {
                SONIC_DDPRINT("ERROR state\n");
                e_state ++;
            }
            if (pcs->r_state == RX_T) {
                pkt_cnt ++;
//                total_pkt_cnt ++;
                stat->total_packets ++;

                tpacket = SONIC_COMPUTE_NEXT(packet->buf, packet->len - 8);

                if (fifo_required) {
                    packets->pkt_cnt = pkt_cnt;

                    put_write_entry(pcs->out_fifo, packets);

                    packets = get_write_entry(out_fifo);
                    if (!packets)
                        goto end;

                    packet = packets->packets;
                    pkt_cnt = 0;
                    fifo_required = 0;
                } else {
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
    if (pcs->r_state & 0x11010) {
        if (packet->len == 0)
            pkt_cnt ++;

        packets->pkt_cnt = pkt_cnt;

        put_write_entry(out_fifo, packets);
        fifo_required = 0;
    }

#if !SONIC_SFP
    pcs->state = PCS_INITIAL_STATE;
    pcs->r_state = RX_INIT;
#endif /* SONIC_SFP */

    if (*stopper == 0)
        goto begin;

end:

//    STOP_CLOCK(pcs->debug * 66);
    STOP_CLOCK(stat);
//    SONIC_OUTPUT(pcs, "%u\n", (unsigned) total_pkt_cnt);

    return 0;
}

#if SONIC_SFP
int sonic_pcs_tx_idle_loop(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(pcs, args);
    int i,j;

    SONIC_DDPRINT("\n");

    START_CLOCK();

begin:
    for ( i = 0 ; i < 50 ; i ++ ) {
        for (j = 0 ; j < 0xffff; j ++ ) {
            scrambler(pcs, 0x1, IDLE_FRAME);
        }
    }

    if (*stopper == 0)
        goto begin;

    STOP_CLOCK(&pcs->stat);
    //SONIC_PRINT("[%s] counter = %llx (%llu) %llx (%llu) %llu %llu %llu\n", pcs->name, pcs->synch_counter, pcs->synch_counter >> 3, pcs->synch_rcounter, pcs->synch_rcounter >> 3, pcs->synch_debug, pcs->synch_debug2, pcs->synch_debug3);
//    SONIC_PRINT("[%s] counter = %llx %llx %llu\n", pcs->name, pcs->synch_counter, pcs->synch_rcounter, pcs->synch_debug);

    return 0;
}

int sonic_pcs_rx_idle_loop(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(pcs, args);
    int i, j, k, l=0;
    uint64_t ordered_4b=0, ordered_55 = 0, ordered_2d=0, idles =0, error= 0;
    uint64_t descrambled;
    struct sonic_pcs_block block;

    SONIC_DDPRINT("\n");

    START_CLOCK();

begin:
    for ( i = 0 ; i < 50 ; i ++ ) {
#if SONIC_SFP
        sonic_dma_rx(pcs);
#endif
        for ( j = 0 ; j < pcs->dma_buf_pages; j ++ ) {
            pcs->dma_page = ((struct sonic_dma_page*) pcs->dma_cur_buf) + j;
            for ( k = 0 ; k < NUM_BLOCKS_IN_PAGE ; k ++ ) {
                sonic_retrieve_block(pcs, k, &block);

                descrambled = descrambler(pcs, block.payload);
                if ( descrambled == 0x1eULL ) 
                    idles ++;
                else if ( descrambled == 0x200004b ) 
                    ordered_4b ++;
                else if ( descrambled == 0x0200000002000055ULL ) 
                    ordered_55 ++; 
                else if ( descrambled == 0x020000000000002dULL ) 
                    ordered_2d ++;
                else {
                    l++;
                    error++;
                }
            }
        }
        if (l) 
            SONIC_DPRINT("[p%d] %d errors\n", pcs->port_id, l);
        l=0;
    }

    if (*stopper == 0)
        goto begin;

    STOP_CLOCK(&pcs->stat);
//    SONIC_PRINT("[%s] counter = %llx (%llu) %llx (%llu) %llu %llu %llu\n", pcs->name, pcs->synch_counter, pcs->synch_counter >> 3, pcs->synch_rcounter, pcs->synch_rcounter >> 3, pcs->synch_debug, pcs->synch_debug2, pcs->synch_debug3);
    SONIC_PRINT("[p%d] %llu errors %llu idles %llu ordered_4b %llu ordered_55 %llu ordered_2d\n", 
            pcs->port_id,  error, idles, ordered_4b, ordered_55, ordered_2d);

    return 0;
}

#define DMA_ITERATIONS  50

int sonic_pcs_tx_dma_loop(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(pcs, args);
    int i;
    uint64_t j=0;

    START_CLOCK();
begin:
    for ( i = 0 ; i < DMA_ITERATIONS; i ++ ) {
        sonic_dma_tx(pcs);
        j++;
    }

    if (*stopper == 0)
        goto begin;

    STOP_CLOCK(&pcs->stat);
    pcs->stat.total_blocks = j * pcs->dma_buf_pages * 496;

    return 0;
}

int sonic_pcs_rx_dma_loop(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(pcs, args);
    int i;
    uint64_t j=0;

    START_CLOCK();
begin:
    for ( i = 0 ; i < DMA_ITERATIONS; i ++ ) {
        sonic_dma_rx(pcs);
        j++;
    }

    if (*stopper == 0)
        goto begin;

    STOP_CLOCK(&pcs->stat);
    pcs->stat.total_blocks = j * pcs->dma_buf_pages * 496;

    return 0;
}
#undef DMA_ITERATIONS
#endif /* SONIC_SFP */

#if !SONIC_KERNEL 
int sonic_test_encoder(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(pcs, args);
    struct sonic_fifo *in_fifo = pcs->in_fifo;
    struct sonic_port_info *info = &pcs->port->info;
    struct sonic_packets *packets;
    struct sonic_packet *packet;
    struct sonic_pcs_stat *stat = &pcs->stat;
    int i;

    SONIC_DPRINT("\n");

    sonic_prepare_pkt_gen_fifo(in_fifo, info);

    START_CLOCK( );

begin:
    packets = (struct sonic_packets *) get_read_entry_no_block(in_fifo);
    if (!packets)
        goto end;

    FOR_SONIC_PACKETS(i, packets, packet) {
        sonic_encode(pcs, packet);
        packet = SONIC_NEXT_PACKET(packet);
        stat->total_packets ++;
    }

    put_read_entry_no_block(in_fifo);

    if (*stopper == 0)
        goto begin;

end:
    STOP_CLOCK(stat);
//    STOP_CLOCK(pcs->debug * 66);
//    SONIC_OUTPUT(pcs, "%llu\n", (unsigned long long) pcs->debug);

    return 0;
}

int sonic_test_decoder(void *args)
{
    SONIC_THREAD_COMMON_VARIABLES(pcs, args);
    struct sonic_fifo *out_fifo = pcs->out_fifo;
    struct sonic_port_info *info = &pcs->port->info;
    struct sonic_packets *packets;
    struct sonic_packet *packet, *tpacket;
    struct sonic_pcs_stat *stat = &pcs->stat;
    int i, j, cnt, tail, pkt_cnt=0;
//    struct sonic_pcs_block block;

    SONIC_DPRINT("\n");

    sonic_prepare_pkt_gen_fifo(pcs->port->fifo[3], info);
    /* fill DMA buffer */
    sonic_fill_dma_buffer_for_rx_test(pcs, pcs->port->fifo[3]);

    pcs->debug = 0;
    pcs->state = PCS_INITIAL_STATE;
    START_CLOCK();

begin:
    packets = (struct sonic_packets *) get_write_entry_no_block(out_fifo);
    if (!packets)
        goto end;

    packets->pkt_cnt = 0;
    packet = packets->packets;
    packet->len = 0;
    packet->idle = 0;
    packet->idle_bits = 0;

    for ( i = 0 ; i < pcs->dma_buf_pages; i ++ ) {
        pcs->dma_page = ((struct sonic_dma_page *) pcs->dma_cur_buf) + i;
        cnt = pcs->dma_page->reserved;
        
        for ( j = 0 ; j < cnt ; j ++ ) {
            tail = sonic_decode(pcs, j, packet);

            if (unlikely(pcs->r_state == RX_T)) {
                pkt_cnt ++;
//                total_pkt_cnt++;
                stat->total_packets ++;
                
                tpacket = SONIC_COMPUTE_NEXT(packet->buf, packet->len -8); // FIXME

                SONIC_DDPRINT("%d %d tpacket = %p\n", packet->len, packet->idle, tpacket);

                packet->next = (uint8_t *) tpacket - (uint8_t*) packet;
                packet = tpacket;

                packet->len = 0;
                packet->idle = 8 - tail;
                packet->idle_bits = (7-tail) << 3;
            }
        }
    }

    pcs->state = PCS_INITIAL_STATE;

    put_write_entry_no_block(out_fifo);

    if (*stopper == 0)
        goto begin;

end:
    STOP_CLOCK(stat);
//    STOP_CLOCK(pcs->debug * 66);
//    SONIC_OUTPUT(pcs, "%llu %llu\n", (unsigned long long) pcs->debug, 
//            (unsigned long long) total_pkt_cnt);

    return 0;
}
#endif /*SONIC_KERNEL*/
