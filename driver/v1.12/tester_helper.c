#if !SONIC_KERNEL
#include <string.h>
#endif /* SONIC_KERNEL */

#include "tester.h"
#include "sonic.h"
#include "fifo.h"

const static uint8_t sonic_pcs_terminal[] = 
		{0x87, 0x99, 0xaa, 0xb4, 0xcc, 0xd2, 0xe1, 0xff};

static inline uint64_t scrambler_bitwise(uint64_t *state, uint64_t syncheader, uint64_t payload)
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

	SONIC_DDPRINT("%.2x %.16llx %.16llx\n", syncheader, payload, scrambled);

	return scrambled;
}

/* for debugging only */
int sonic_encode_helper(uint64_t *state, uint8_t *buf, int len, int idles, 
		unsigned beginning_idles, int batching, struct sonic_pcs_block *blocks, 
		int blocks_cnt)
{
	int j=0, i, olen = len, k=0;
	int dic = 0;
	unsigned char *p, *tp;
	register uint64_t scrambled, tmp; 
	unsigned int ending_idles; 
	struct sonic_mac_fifo_entry *packets;
	struct sonic_packet_entry *packet;

	packets = (struct sonic_mac_fifo_entry *) buf;
	packet = packets->packets;

	p = packet->buf;
	SONIC_DDPRINT("pkt_cnt = %d\n", packets->pkt_cnt);

	for ( i = 0 ; i < packets->pkt_cnt ; i ++ ) {
		olen = packet->len;
		tp = p;

		switch(beginning_idles % 4) {
		case 1:
			if (dic < 3) {
				beginning_idles --;
				dic ++;
			}
			else {
				beginning_idles += 3;
				dic = 0;
			}
			break;
		case 2:
			if (dic < 2) {
				beginning_idles -= 2;
				dic += 2;
			}
			else  {
				beginning_idles += 2;
				dic -= 2;
			}
			break;
		case 3:
			if (dic == 0) {
				beginning_idles -= 3;
				dic = 3;
			} else {
				beginning_idles ++;
				dic --;
			}
		}

		while(beginning_idles >= 8) {
			scrambled = scrambler_bitwise(state, 0x1, IDLE_FRAME);
			blocks[j].syncheader = 0x1;
			blocks[j++].payload = scrambled;
			if ( j >= blocks_cnt)
				goto end;
			beginning_idles -= 8;
		}

		/* /S/ */
		if (beginning_idles == 4) {
			tmp = *(uint64_t *) tp;
			tmp <<= 32;
//			tmp <<= 40;
			tmp = 0x33 | tmp;

			tp += 4;
			olen -= 4;
		} else {
//			if (beginning_idles != 0) {
//				scrambled = scrambler_bitwise(state, 0x1, IDLE_FRAME);
//				blocks[j].syncheader = 0x1;
//				blocks[j++].payload = scrambled;
//				if ( j >= blocks_cnt)
//					goto end;
//			}
			tmp = *(uint64_t *) tp;
//			tmp <<= 8;
			tmp = 0x78 | tmp;

			tp += 8;
			olen -= 8;
		}
		scrambled = scrambler_bitwise(state, 0x1, tmp);
		blocks[j].syncheader = 0x1;
		blocks[j++].payload = scrambled;
			if ( j >= blocks_cnt)
				goto end;

		/* /D/ */
		while (olen >= 8) {
			tmp = *(uint64_t *) tp;
			scrambled = scrambler_bitwise(state, 0x2, tmp);
			blocks[j].syncheader = 0x2;
			blocks[j++].payload = scrambled;
			if ( j >= blocks_cnt)
				goto end;

			tp += 8;
			olen -= 8;
		}

		/* /T/ */
		tmp = 0;
		if (olen != 0) {
			tmp = *(uint64_t *) tp;
			tmp <<= (8-olen) * 8;
			tmp >>= (7-olen) * 8;
		}

		tmp |= sonic_pcs_terminal[olen];
		scrambled = scrambler_bitwise(state, 0x1, tmp);

		blocks[j].syncheader = 0x1;
		blocks[j++].payload = scrambled;
		if ( j >= blocks_cnt)
			goto end;
			
		/* /E/ */
		ending_idles = idles - (8 - olen);

#if 0
		while (ending_idles > 8) {

			scrambled = scrambler_bitwise(state, 0x1, IDLE_FRAME);
			blocks[j].syncheader = 0x1;
			blocks[j++].payload = scrambled;
			if ( j >= blocks_cnt)
				goto end;
			ending_idles -= 8;
		}
		if (ending_idles >4) {

			scrambled = scrambler_bitwise(state, 0x1, IDLE_FRAME);
			blocks[j].syncheader = 0x1;
			blocks[j++].payload = scrambled;
			if ( j >= blocks_cnt)
				goto end;
//			ending_idles = 0;

		}
#endif
		beginning_idles = ending_idles;
		
		k = j-1;

//		packet = packet->next;
		packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);
		p = packet->buf;
	}

end:
#if SONIC_DDEBUG
	SONIC_PRINT("k = %d blocks_cnt %d\n", k, blocks_cnt);
	for ( j = 0 ; j < k ; j ++ ) {
		SONIC_PRINT("%.2x %.16llx\n", blocks[j].syncheader, blocks[j].payload);

	}
#endif
	return k;
}

int sonic_gearbox_helper(uint64_t * buf, int buf_order, struct sonic_pcs_block *blocks, int blocks_cnt)
{
	int i = 0, j = 0, b =0;
	struct sonic_dma_page *page = (struct sonic_dma_page *) buf;
	int sh_index, sh_offset;
	int cur_sh;

	SONIC_DDPRINT("blocks_cnt = %d\n", blocks_cnt);

	while(j < blocks_cnt) {
		sh_index = i / 16;
		sh_offset = i % 16;

		if (sh_offset == 0)
			cur_sh = 0;
		else
			cur_sh = page->syncheaders[sh_index];

		cur_sh = cur_sh | ((blocks[j].syncheader &0x3)<< (sh_offset * 2));
		page->syncheaders[sh_index] = cur_sh;

		page->payloads[i] = blocks[j].payload;

		i ++;
		j ++;

		if (i == NUM_BLOCKS_IN_PAGE)  {
			page->reserved = NUM_BLOCKS_IN_PAGE;
#if SONIC_DDEBUG
			print_page(page);
#endif
			page ++;
			i = 0;
			b++;
		}
	}
	page->reserved = i;

#if SONIC_DDEBUG
	print_page(page);
	SONIC_PRINT("%d\n", j);
#endif

	for ( ++b ; b < power_of_two(buf_order) ; b ++ )  {
		page++;
		page->reserved = 0;
	}
	return j;
}

#if !SONIC_KERNEL
int sonic_ins_helper(struct sonic_logger *logger, char *fname)
{
    struct sonic_ins *ins;
    struct sonic_ins_entry *ins_entry;
    struct list_head *log_head = &logger->ins_head;
    char buf[256];
    FILE *fin;
    int first=0, i=0, total=0;
    SONIC_PRINT("\n");

    fin = fopen(fname, "r");

    ins = (struct sonic_ins *) ALLOC_PAGES(logger->log_porder);
    if (!ins) {
        SONIC_ERROR("out of memory\n");
        exit(EXIT_FAILURE);
    }


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

        ins_entry = &ins->inss[i];

        ins_entry->type = ptype;
        ins_entry->len = plen;
        ins_entry->idle = pidle;
        ins_entry->id = pid;

        total++;
        i++;

        if (i == order2bytes(logger->log_porder) / sizeof(struct sonic_ins_entry)-1) {
            ins->ins_cnt = i;
            list_add_tail(&ins->head, log_head);
            ins = (struct sonic_ins *) ALLOC_PAGES(logger->log_porder);
            if (!ins) {
                SONIC_ERROR("out of memory\n");
                exit(EXIT_FAILURE);
            }
            i=0;
        }
    }

    if (i) {
//       SONIC_PRINT("i = %d %p %p\n", i, log_head, &ins->head);
        ins->ins_cnt = i;
        list_add_tail(&ins->head, log_head);
//        SONIC_PRINT("%d\n", list_empty(log_head));
    }

    SONIC_PRINT("Total processed=%d\n", total);

    return 0;
}

int sonic_log_helper(struct sonic_logger *logger)
{

    return 0;
}
#endif /* !SONIC_KERNEL */
