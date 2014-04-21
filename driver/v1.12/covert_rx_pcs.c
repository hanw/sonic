#if !SONIC_KERNEL
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#endif

#include "sonic.h"
#include "covert.h"

double covert_bits = 0;
unsigned long long int descrambler_debug_c = 0;
static unsigned long long int dummy = 0;

//Clear receiver's buffer before we start to add data into the buffer. Resets offset variables.
void prepareReceiveDataBuffer(struct covert_tracker * rx_covert)
{
    rx_covert->byteOffset = 0;
    rx_covert->bitOffset = 0;
    memset(rx_covert->dataBuf, 0, sizeof(rx_covert->dataBuf));
}

//Returns true if the last byte of the data is the EndOfData byte (0xff)
int isEndOfDat(struct covert_tracker * rx_covert)
{
    if (rx_covert->byteOffset <= 0){
        return 0;
        }
    //return (buf[dataLen-1] == 0xff);
    if (rx_covert->dataBuf[rx_covert->byteOffset-1] == 0xff){
        //printf("%s\n", rx_covert->dataBuf);
	rx_covert->dataBuf[rx_covert->byteOffset-1] = 0;
        return 1;
    }
    else{
        return 0;
    }
}

//Adds covert bits of 'data' to buffer 'buf'
inline void addNBitToBuffer(struct covert_tracker * rx_covert, unsigned char data, int nBit)
{
    //return 0;
    static int nBitsFirst;
    static unsigned char mask;
    static unsigned char tmp;
    static unsigned char tmp1;
    static int nBitsSecond;

    if ((nBit+rx_covert->bitOffset) <= 8)
    {
        //Test stuff
	//return 0;
	
	//New data doesn't cross the byte boundary
	rx_covert->dataBuf[rx_covert->byteOffset] |= (data << rx_covert->bitOffset);
        rx_covert->bitOffset += nBit;
	if (rx_covert->bitOffset >= 8)
        {
            ++(rx_covert->byteOffset);
            rx_covert->bitOffset = 0;
        }
	
//	printf("Decode: extractNBitFromBuffer1: byte[%d] is %x,  data %x, bitOffset %d, numBits: %d\n", rx_covert->byteOffset, rx_covert->dataBuf[rx_covert->byteOffset], data, rx_covert->bitOffset, nBit);

    }
    else
    {
	//Test stuff
	//return 0;     

        //New data doesn't fit into the currrent byte in the buffer 
        //update the current byte using part of bits 
        nBitsFirst = 8 - rx_covert->bitOffset;
        //mask = pow(2,nBitsFirst) -1;
        mask = (1<< nBitsFirst) -1;
        tmp = data & mask;
        tmp1 = tmp << rx_covert->bitOffset;
        rx_covert->dataBuf[rx_covert->byteOffset] |= tmp1;
        ++(rx_covert->byteOffset);
        rx_covert->bitOffset = 0;

        //now update the next byte in the data buffer
        nBitsSecond = nBit - nBitsFirst;
        mask = (1<< nBitsSecond) -1;
        tmp = data >> nBitsFirst;
        tmp1 = mask & tmp;
        rx_covert->dataBuf[rx_covert->byteOffset] |= tmp1;
        rx_covert->bitOffset += nBitsSecond;
   
//	printf("Decode: extractNBitFromBuffer2: byte[%d] is %x,  data %x, bitOffset %d, numBits: %d\n", rx_covert->byteOffset, rx_covert->dataBuf[rx_covert->byteOffset], data, rx_covert->bitOffset, nBit);
    }

    //printf("%d\n",rx_covert->bitOffset);
    //printf("%d\n",rx_covert->byteOffset);
    //return rx_covert->byteOffset;
}


//Extracts covert bits from block and adds to existing buffer 
//inline void extractBits(uint64_t block, uint64_t block_type, struct covert_tracker * rx_covert){
inline int extractBits(uint64_t block, uint64_t block_type, struct covert_tracker * rx_covert){
        unsigned char tmpData = 0;
	int numBits = 0;	

        //printf("Code: %x\n", block_type);     
        switch(block_type) {
        /* /S/ */
        case 0x33:
                tmpData = (block >>= 36) & 0xf;
                addNBitToBuffer(rx_covert, tmpData, 4);
                numBits = 4;
		break;
        case 0x78:
                break;
        /* /T/ */
        case 0xff:
                break;
        case 0xe1:
                tmpData = (block >>= 56) & 0x1;
                addNBitToBuffer(rx_covert, tmpData, 1);
		numBits = 1;
                break;
        case 0xd2:
                tmpData = (block >>= 48) & 0x3;
                addNBitToBuffer(rx_covert, tmpData, 2);
		numBits = 2;
                break;
        case 0xcc:
                tmpData = (block >>= 40) & 0x7;
                addNBitToBuffer(rx_covert, tmpData, 3);
                numBits = 3;
		break;
        case 0xb4:
                tmpData = (block >>= 32) & 0xF;
                addNBitToBuffer(rx_covert, tmpData, 4);
                numBits = 4;
		break;
	case 0xaa:
                tmpData = (block >>= 24) & 0x1F;
                addNBitToBuffer(rx_covert, tmpData, 5);
		numBits = 5;                
		break;
        case 0x99:
                tmpData = (block >>= 16) & 0x3F;
                addNBitToBuffer(rx_covert, tmpData, 6);
                numBits = 6;
		break;
        case 0x87:
                tmpData = (block >>= 8) & 0x7F;
                addNBitToBuffer(rx_covert, tmpData, 7);
                numBits = 7;
		break;
	/* /E/ */
	case 0x1e:			
		break;
	default:
		//fprintf(stderr, "Error! Unknown block type: %a\n", block_type);
		break;
	}
	return numBits;
}

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
	descrambler_debug_c++;
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

	descrambler_debug_c++;
	return s;
}
#endif

static inline int sonic_retrieve_page(struct sonic_dma_page *page, int index, struct sonic_pcs_block *block)
{
	int cur_sh;

	cur_sh = page->syncheaders[index>>4];
//	cur_sh = page->syncheaders[index/16];
	
	block->syncheader = (cur_sh >> ((index & 0xf) << 1)) & 0x3;
//	block->syncheader = (cur_sh >> ((index % 16) * 2)) & 0x3;
	block->payload = page->payloads[index];

	SONIC_DDPRINT("%x %.16llx\n", block->syncheader, block->payload);

	return 0;
}

//Covert Decode Core
static inline int covert_decode_core(uint64_t *state, uint64_t syncheader, 
		uint64_t payload, struct sonic_packet_entry *packet, enum R_STATE *r_state, struct covert_tracker * rx_covert)
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
		return 0;
	} 
	else {

		//Throughput Test
		//Extract covert data 
		//if(!isEndOfDat(rx_covert)){
                if(rx_covert->byteOffset < 100){ 
			covert_bits +=extractBits(descrambled, descrambled & 0xff, rx_covert);
			//total_bits += extractBits(descrambled, descrambled & 0xff, rx_covert);
			//extractBits(descrambled, descrambled & 0xff, rx_covert);
                }
		else{
			rx_covert->byteOffset = 0;
			rx_covert->bitOffset = 0;
			//memset(rx_covert->dataBuf, 0, sizeof(rx_covert->dataBuf));
			//prepareReceiveDataBuffer(rx_covert);
		}		
		
	/**	
		//Correctness Test
                //Extract covert data 
               	if(!isEndOfDat(rx_covert)){
		//if(rx_covert->byteOffset < 6){
                       	total_bits += extractBits(descrambled, descrambled & 0xff, rx_covert);
               		//printf("%d\n", rx_covert->byteOffset); 
			//printf("%s\n", rx_covert->dataBuf);
		} 
               	else{
                	//printf("hi\n");
			//rx_covert->dataBuf[rx_covert->byteOffset-1] = 0;
			//printf("%s\n", rx_covert->dataBuf[0]);
			printf("%s\n", rx_covert->dataBuf);
			//prepareReceiveDataBuffer(rx_covert);
		}                       
	*/			
		switch(descrambled & 0xff) {
		case 0x66:
		/* /S/ */
		case 0x33:
			if (!(*r_state & 0x11011)) {
				*r_state = RX_E;
				return -1;
			}

			packet->idle += 4;
			descrambled >>= 32;
			* (uint64_t *) p = descrambled;
			p += 4;
			packet->len = 4;

			*r_state = RX_D;
			return 0;

		case 0x78:
			if (!(*r_state & 0x11011))  {
				*r_state = RX_E;
				return -1;
			}

	//		descrambled >>= 8;
			* (uint64_t *) p = descrambled;
			p += 8;
			packet->len = 8;

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
	//		packet->idle = 7 - tail;

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

			packet->idle = 0;
			packet->idle += 8;
			*r_state = RX_C;
			return 0;
		default:
			*r_state = RX_E;
			return -1;
		}
	}
	//return total_bits;
}

// Covert Decode no Fifo
inline int covert_decode_no_fifo(uint64_t *state, void *buf, int buf_order,
		enum R_STATE *r_state, struct sonic_packet_entry *packet, int mode, struct covert_tracker * rx_covert)
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
			tail = covert_decode_core(state, block.syncheader, block.payload, packet, r_state, rx_covert);
//			covert_bits +=tail;
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

// rx pcs tester
int sonic_covert_rx_pcs_tester(void *args)
{
	struct sonic_pcs *pcs = (struct sonic_pcs * )args;
	const volatile int * const stopper = &pcs->port->stopper;
	struct sonic_packet_entry *packet;
	uint64_t state = PCS_INITIAL_STATE;
	enum R_STATE r_state = RX_INIT;
	uint64_t i, cnt=0;
	int mode = pcs->mode;
	struct covert_tracker rx_covert;
	double covert_thru = 0;
	DECLARE_CLOCK;
	
	/**
        //Prepare to send covert data
        memset(tx_covert.dataBuf, 0, sizeof(tx_covert.dataBuf));
        strcpy((char*) tx_covert.dataBuf, "RX HELLO");
        tx_covert.dataLen = strlen((char*) tx_covert.dataBuf);
        prepareSendDataBuffer(& tx_covert);
        //printf("Data : %s\n", tx_covert.dataBuf);
	*/

	//Setup covert data receive buffer
	prepareReceiveDataBuffer(& rx_covert);

	covert_bits = 0;
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

		cnt+=covert_decode_no_fifo(&state, pcs->dma_cur_buf, pcs->dma_buf_order, &r_state, packet, mode, & rx_covert);
#if !SONIC_KERNEL
//		state = PCS_INITIAL_STATE;
//		r_state = RX_INIT;
#endif

	}

#if !SONIC_KERNEL
	state = PCS_INITIAL_STATE;
//	r_state = RX_INIT;
#endif

	if (*stopper == 0)
		goto begin;

	STOP_CLOCK((uint64_t) descrambler_debug_c * 66)

	covert_thru = (1024*covert_bits)/_time;
	SONIC_OUTPUT("c_thru: %f, %llu %llu\n", covert_thru, (unsigned long long)cnt, dummy);

	descrambler_debug_c = 0;

	return 0;
}
