#if !SONIC_KERNEL
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#endif

#include "sonic.h"
#include "covert.h"

const static uint8_t sonic_pcs_terminal[] = {0x87, 0x99, 0xaa, 0xb4, 0xcc, 0xd2, 0xe1, 0xff};

unsigned long long int scrambler_debug_c=0;

//Add EndOfData byte (0xff) to the end of covert data. Initialize variables.
//void prepareSendDataBuffer(unsigned char * buf, int * dataLen)
inline void prepareSendDataBuffer(struct covert_tracker * tx_covert)
{
    //tx_covert->byteOffset = 0;
    tx_covert->bitOffset = 0;
    tx_covert->dataBuf[tx_covert->dataLen] = 0xff;
    ++(tx_covert->dataLen);

#ifdef DEBUG
    printf("prepareSendDataBuffer: send datalen %d\n", dataLen);
    for (int i=0; i<dataLen; i++) printf("%x ", buf[i]);
    printf("\n****************************************************\n");
#endif
}


// extract nBits of data from the sender buffer. the static variables byteOffset and
// bitOffset are used to keep tracks of what has been sent. 
//inline void extractNBitFromBuffer( struct covert_tracker * tx_covert, uint64_t * data, int nBit)
inline void extractNBitFromBuffer( struct covert_tracker * tx_covert, int nBit, uint64_t * payload, int shiftBits)
{

   	register int byteOffset = tx_covert->bitOffset >> 3;
   	register int bitOffset = tx_covert->bitOffset & 7;
	
	//short *tmp = &tx_covert->dataBuf[tx_covert->bitOffset >> 3];
        //short *tmp = &tx_covert->dataBuf[byteOffset];
        register short tmp = * (short *) (tx_covert->dataBuf + byteOffset);
        //uint64_t data = (uint64_t)*tmp << (64 - nBit - (tx_covert->bitOffset & 7)) >> (64 - nBit);
//	uint64_t data = (uint64_t)*tmp << (64 - nBit - bitOffset) >> (64 - nBit);
	register uint64_t data = (uint64_t)tmp << (64 - nBit - bitOffset) >> (64 - nBit);
	*payload = (*payload | (data << shiftBits));

        tx_covert->bitOffset += nBit; //- 8 + bitOffset;
	
	if (unlikely(tx_covert->bitOffset >> 3) >= tx_covert->dataLen) {
		tx_covert->bitOffset = 0;
	}

/**
	//return;
	//Bits to be extracted cross the byte boundary
        //Extract part of bits from current byte first
        int nBitsFirst = 8 - bitOffset;
//	unsigned char mask = (1<<nBitsFirst) -1;
//        unsigned char tmp = tx_covert->dataBuf[byteOffset] >> bitOffset;
	
	data = tx_covert->dataBuf[byteOffset] << (64-nBitsFirst-bitOffset) >> (64 - nBitsFirst);
//	*data = mask & tmp;
//	data = mask & tmp;

//	++(tx_covert->byteOffset);
//        tx_covert->bitOffset = 0;
        byteOffset += 1;

        //Extract the remaining bits from next byte
        int nBitsSecond = nBit - nBitsFirst;
//        mask = (1 << nBitsSecond)-1;
//        tmp = mask & tx_covert->dataBuf[byteOffset];
	//tmp = tx_covert->dataBuf[byteOffset] << (64-nBitsSecond) >> (64-nBitsSecond);
//        *data |= (tmp<<nBitsFirst);
        //data |= (tmp<<nBitsFirst);
	data |= (tx_covert->dataBuf[byteOffset] << (64-nBitsSecond) >> (64-nBitsSecond) << nBitsFirst);
	*payload = (*payload | (data << shiftBits));
	//tx_covert->bitOffset += nBitsSecond;
        tx_covert->bitOffset += nBit;        
*/
//	printf("Encode: extractNBitFromBuffer2: byte[%d] is %x, data %x, bitOffset %d, numBits: %d\n", byteOffset, tx_covert->dataBuf[byteOffset], data, bitOffset, nBit);
//    }

#ifdef DEBUG
    printf("extractNBitFromBuffer: end byteOffset %d bitOffset %d data %x\n", byteOffset, bitOffset, *data);
#endif

    //Print data being added
    //printf("Num bits: %d\n", nBit);
    //printf("Data: %x\n", *data);
    //return data;
}

//Add covert bits to real payload
//inline uint64_t addData(uint64_t payload, int NBits, char type, struct covert_tracker * tx_covert){
inline uint64_t addData(uint64_t payload, int NBits, int shiftBits, struct covert_tracker * tx_covert){
	//uint64_t tmpData;
	//tmpData = extractNBitFromBuffer( tx_covert, NBits); 
        extractNBitFromBuffer( tx_covert, NBits, &payload, shiftBits);
/** 		
	//printf("Data before: %llu\n", payload);
        if(type == 's'){
                //extractNBitFromBuffer( tx_covert, &tmpData, NBits);
	      	extractNBitFromBuffer( tx_covert, NBits, &payload, 36);

        	//printf("Data: %x\n", tmpData);        
		//payload = (payload | tmpData << 36);
        }
        else {  
		if(NBits > 0 ){
			extractNBitFromBuffer( tx_covert, NBits, &payload, 64-8*NBits);
	                //extractNBitFromBuffer(tx_covert, &tmpData, NBits);		
			//payload = (payload | tmpData << (64-8*NBits));
		}
	}
*/
		/**
		switch(NBits){	
		case 7:
			payload = (payload | tmpData << 8);
			break;
		case 6:
			payload = (payload | tmpData << 16);
			break;
		case 5:
			payload = (payload | tmpData << 24);
			break;
		case 4:
			payload = (payload | tmpData << 32);
			break;
		case 3:
			payload = (payload | tmpData << 40);
			break;
		case 2:
			payload = (payload | tmpData << 48);
			break;
		case 1:
			payload = (payload | tmpData << 56);
			break;
		case 0:
			break;
		default:
			break;
			//fprintf(stderr, "Error! Incorrect number of covert bits to add!\n");
		}
		*/
	//printf("Data added: %llu\n", tmpData);
	//printf("Data After: %llu\n", payload);
	return payload;
}

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

/**	SONIC_DDPRINT("prefix = %x payload = %.16llx scrambled = %.16llx\n", prefix, payload, s);
#if SONIC_PCS_LEVEL == 1
	sonic_gearbox(pcs, prefix, scrambled);
#endif
*/
	scrambler_debug_c ++;
	return scrambled;
}
#elif SONIC_SCRAMBLER == SONIC_OPT_SCRAMBLER
static inline uint64_t scrambler(struct sonic_pcs *pcs, uint64_t *state,
		int prefix, uint64_t payload)
{
	register uint64_t s = *state, r;
	scrambler_debug_c ++;

	r = (s >> 6) ^ (s >> 25) ^ payload;
	s = r ^ (r << 39) ^ (r << 58);

	*state = s;

	SONIC_DDPRINT("prefix = %x payload = %.16llx scrambled = %.16llx\n", prefix, payload, s);
#if SONIC_PCS_LEVEL == 1
	sonic_gearbox(pcs, prefix, s);
#endif
	return s;
}
#endif

//SoNIC encode covert
uint64_t covert_encode(struct sonic_pcs * pcs, uint64_t *state, uint8_t *data, int len, 
		int idles, unsigned beginning_idles, struct covert_tracker * tx_covert)
{
	register uint64_t scrambled, tmp;
	//uint64_t tmp; 
	int ending_idles; 

#if SONIC_DDEBUG
	sonic_print_hex(data, len, 32);
#endif
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
	}

	/* /S/ */
	//Case 1: There are "filler bits" 
	if (beginning_idles == 4) {
//	if (beginning_idles && beginning_idles <=4) {
		tmp = *(uint64_t *) data;
		tmp <<= 32;
		tmp = 0x33 | tmp;

		data += 4;
		len -= 4;

		//Add covert data
		//if(likely(tx_covert->byteOffset < tx_covert->dataLen)){
//		if(likely((tx_covert->bitOffset >> 3) < tx_covert->dataLen)){
			//printf("hi\n");
			//tmp = addData(tmp, 4, 's', tx_covert);
			tmp = addData(tmp, 4, 36, tx_covert);
			//extractNBitFromBuffer( tx_covert, 4, &tmp, 36);
 //               }
		//For testing purposes to keep sending covert data 
//		else{
			//tx_covert->byteOffset = 0;
//			tx_covert->bitOffset = 0;
//		}

	//Case 2: There are no "filler bits" 
	} else {
//		if (likely(beginning_idles != 0)) {
//			scrambled = scrambler(pcs, state, 0x1, IDLE_FRAME);
//		}
		tmp = *(uint64_t *) data;
		tmp = 0x78 | tmp;

		data += 8;
		len -= 8;
	}
	scrambled = scrambler(pcs, state, 0x1, tmp);
	
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

	if (7-len > 0){
		//Add secret data
        	//if (likely(tx_covert->byteOffset < tx_covert->dataLen)){
//		if(likely((tx_covert->bitOffset >> 3) < tx_covert->dataLen)){
			//tmp = addData(tmp, 7-len, 't', tx_covert);
			tmp = addData(tmp, 7-len, 64-8*(7-len), tx_covert);
			//extractNBitFromBuffer( tx_covert, 7-len, &tmp, 64-8*(7-len));
//		}       
        	//For testing purposes to keep sending covert data 
//		else{
			//tx_covert->byteOffset = 0;
 //       	       	tx_covert->bitOffset = 0;
//	       	}
	}
	scrambled = scrambler(pcs, state, 0x1, tmp);

	/* /E/ */
	ending_idles = idles - (8 - len);

//	while (ending_idles > 8) {
//		scrambled = scrambler(pcs, state, 0x1, IDLE_FRAME);
//		ending_idles -= 8;
//	}
	beginning_idles = ending_idles;

	return beginning_idles;
}

//Helper function for test tx
int sonic_covert_tx_pcs_tester(void *args)
{
	struct sonic_pcs * pcs = (struct sonic_pcs *) args;
	struct sonic_fifo *fifo = pcs->pcs_fifo;
	struct sonic_mac_fifo_entry *packets;
	struct sonic_packet_entry *packet = NULL;
	struct covert_tracker tx_covert;
	int i, j, pkt_cnt, cnt =0;
	int idles = 12, beginning_idles=12;
//	int mode = pcs->mode;
	uint64_t state = PCS_INITIAL_STATE;
	const volatile int * const stopper = &pcs->port->stopper;
	int f=0;
	uint64_t prevdebug = 0;
	DECLARE_CLOCK;

	//Prepare to send covert data
    	memset(tx_covert.dataBuf, 0, sizeof(tx_covert.dataBuf));
    	strcpy((char*) tx_covert.dataBuf, "TX HELLO");
    	tx_covert.dataLen = strlen((char*) tx_covert.dataBuf);
    	prepareSendDataBuffer(& tx_covert);

	START_CLOCK();

	SONIC_DDPRINT("%p\n", fifo);

begin:
	// 50 iterations at a time
	for (i = 0 ; i < 50 ; i ++ ) {
		packets = (struct sonic_mac_fifo_entry *) get_read_entry_no_block(fifo);

		pkt_cnt = packets->pkt_cnt;
		packet = packets->packets;

		for ( j = 0 ; j < pkt_cnt ; j ++) {
			prevdebug = scrambler_debug_c;
			beginning_idles = covert_encode(pcs, &state, packet->buf, 
					packet->len, idles, beginning_idles, & tx_covert);
//			packet = packet->next;
			packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);

			if (!f) {
	//			SONIC_DPRINT("%d\n", scrambler_debug - prevdebug);
			}
		}

		cnt += pkt_cnt;

		put_read_entry_no_block(fifo);

		f = 1;
	}

	if (*stopper == 0)
		goto begin;

	STOP_CLOCK(scrambler_debug_c * 66);
	SONIC_OUTPUT("pkt_len = %d, scrambler_debug = %llu, %d\n", packet->len-8,(unsigned long long)scrambler_debug_c, cnt);

	scrambler_debug_c = 0;
	return 0;
}

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

int covert_encode_helper(uint64_t *state, uint8_t *buf, int len, int idles, 
		unsigned beginning_idles, int batching, struct sonic_pcs_block *blocks, 
		int blocks_cnt, struct covert_tracker * tx_covert)
{
	int j=0, i, olen = len, k=0;
	int dic = 0;
	unsigned char *p, *tp;
	register uint64_t scrambled, tmp; 
	//uint64_t tmp;
	unsigned int ending_idles; 
	struct sonic_mac_fifo_entry *packets;
	struct sonic_packet_entry *packet;

	packets = (struct sonic_mac_fifo_entry *) buf;
	packet = packets->packets;
	
	//printf("Covert Message: %s\n", tx_covert->dataBuf);	

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

			//Add covert data
			//if (tx_covert->byteOffset < tx_covert->dataLen){
			if(likely((tx_covert->bitOffset >> 3) < tx_covert->dataLen)){	
				//printf("hi\n");
				//tmp = addData(tmp, 4, 's', tx_covert);
                	        tmp = addData(tmp, 4, 36, tx_covert);
				//extractNBitFromBuffer( tx_covert, 4, &tmp, 36);
				//printf("Data After return: %llu\n", tmp);
			}	
			//For testing purposes to keep sending covert data 
			else{
				//tx_covert->byteOffset = 0;
				//tx_covert->bitOffset = 0;
			}

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
		
		if (7-olen > 0){	
			//Add secret data
            	    	//if (tx_covert->byteOffset < tx_covert->dataLen){
                        if(likely((tx_covert->bitOffset >> 3) < tx_covert->dataLen)){
				//printf("hello\n");
                        	//tmp = addData(tmp, 7-olen, 't', tx_covert);
				tmp = addData(tmp, 7-olen, 64-8*(7-olen), tx_covert);
				//extractNBitFromBuffer( tx_covert, 7-len, &tmp, 64-8*(7-len));
                	}
                	//For testing purposes to keep sending covert data 
               		else{
                        	//tx_covert->byteOffset = 0;
                        	//tx_covert->bitOffset = 0;
                	}
		}
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
		
		k = j;

//		packet = acket->next;
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
