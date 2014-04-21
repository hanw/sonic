#include "sonic.h"
#include "fifo.h"

struct covert_tracker{
        int byteOffset;
        int bitOffset;
        unsigned char dataBuf[100];
        int dataLen;
};

/* tx_pcs.c */
uint64_t covert_encode(struct sonic_pcs *, uint64_t *, uint8_t *, int, int, unsigned, struct covert_tracker *);
inline uint64_t addData(uint64_t, int, int, struct covert_tracker *);
void prepareSendDataBuffer(struct covert_tracker *);
int sonic_covert_tx_pcs_tester(void *);
int covert_encode_helper(uint64_t *state, uint8_t *buf, int len, int idles, unsigned beginning_idles, int batching, struct sonic_pcs_block *blocks, int blocks_cnt, struct covert_tracker *);


/* rx_pcs.c */
int covert_decode_no_fifo(uint64_t *, void *, int , enum R_STATE *, struct sonic_packet_entry *, int, struct covert_tracker *);
inline void prepareReceiveDataBuffer(struct covert_tracker *);
int sonic_covert_rx_pcs_tester(void *);
