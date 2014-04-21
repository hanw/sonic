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
#include "fifo.h"
#include "tester.h"
#include "crc32.h"
#include "covert.h"
#include "mode.h"

int sonic_verbose=0;

void print_args(struct tester_args *args)
{
	SONIC_PRINT("SONIC_KERNEL : %d\n", SONIC_KERNEL);
#if SONIC_KERNEL
	SONIC_PRINT("SONIC_PCIE : %d\n", SONIC_PCIE);
	SONIC_PRINT("SONIC_PROC : %d\n", SONIC_PROC);
	SONIC_PRINT("SONIC_FS : %d\n", SONIC_FS);
	SONIC_PRINT("SONIC_SFP : %d\n", SONIC_SFP);
	SONIC_PRINT("SONIC_DISABLE_IRQ : %d\n", SONIC_DISABLE_IRQ);
#endif
	SONIC_PRINT("SONIC_TWO_PORTS : %d\n", SONIC_TWO_PORTS);
	SONIC_PRINT("SONIC_PCS_LEVEL: %d\n", SONIC_PCS_LEVEL);

#define TESTER(x,y)	SONIC_PRINT( "%-20s = %d\n", #x, args->x);
	TESTER_INT_ARGS
#undef TESTER
#define TESTER(x,y,z) \
	SONIC_PRINT( "%-20s[0] = %d\n", #x, args->x [0]);	\
	SONIC_PRINT( "%-20s[1] = %d\n", #x, args->x [1]);
	TESTER_INT_ARRAY_ARGS
#undef TESTER
#define TESTER(x,y,z,w)	\
	SONIC_PRINT( "%-20s[0] = %s\n", #x, args->x [0]);	\
	SONIC_PRINT( "%-20s[1] = %s\n", #x, args->x [1]);
	TESTER_STR_ARRAY_ARGS
#undef TESTER
}

static void check_args(struct tester_args *args)
{
	if (args->mode > MODE_MAX || args->mode < 0) {
		SONIC_ERROR("Invalid test mode\n");
		args->mode = 0;
	}

	if (args->mode == 0) {
		args->nloops = 1;
		args->batching = 1;
	}

	if (args->begin > 1518) {
		SONIC_ERROR("Ethernet frame length must be less than"
				" or equal to 1518\n");
		args->begin = 1518;
	}

	if (args->end < 64) {
		SONIC_ERROR("Ethernet frame length must be greater than"
				" or equal to 64\n");
		args->end = 64;
	}

	if (args->step <= 0) {
		SONIC_ERROR("Step must be a positive integer\n");
		args->step = 8;
	}

	if (args->begin < args->end) {
		int tmp;
		SONIC_ERROR("begin must be larger then end\n");
		tmp = args->end;
		args->end = args->begin;
		args->begin = tmp;
	}

	if (args->batching < -1) {
		SONIC_ERROR("batching must be a positive integer\n");
		args->batching = 1;
	}
    sonic_verbose=args->verbose;

#if SONIC_DEBUG
    if (args->verbose >1 )
        print_args(args);
#endif
}

static int sonic_simple_parse(struct tester_args *args, const char * str)
{
	char *p = strchr(str, '=');
	if ( p == NULL ) {
		SONIC_ERROR("Wrong arguments\n");
		return -1;
	}
	*p = '\0';

#define TESTER(x,y) if (!strncmp(str, #x, strlen(#x))) sscanf(p+1, "%d", &args-> x ); else
	TESTER_INT_ARGS
#undef TESTER
#define TESTER(x,y,z) \
	if (!strncmp(str, #x, strlen(#x))) sscanf(p+1, "%d,%d", &args-> x [0], &args->x [1]); else
	TESTER_INT_ARRAY_ARGS
#undef TESTER
#define TESTER(x,y,z,w) \
	if (!strncmp(str, #x, strlen(#x))) sscanf(p+1, "%s,%s", args-> x [0], args-> x [2]); else 
	TESTER_STR_ARRAY_ARGS       // FIXME
#undef TESTER
	SONIC_ERROR("Unknown args\n");

	return 0;
}

void sonic_init_tester_args(struct tester_args *args)
{
#define TESTER(x,y) args->x = y;
	TESTER_INT_ARGS
#undef TESTER
#define TESTER(x,y,z) \
	args->x [0] = y; \
	args->x [1] = z;
	TESTER_INT_ARRAY_ARGS
#undef TESTER
#define TESTER(x,y,z,w) \
	strncpy(args->x [0], y, w);\
	strncpy(args->x [1], z, w);
	TESTER_STR_ARRAY_ARGS
#undef TESTER
}

#if !SONIC_KERNEL
int sonic_get_option(struct tester_args *args, char **argv)
{
	char ** p = argv;
	int ret;

	while (*p) {
		ret = sonic_simple_parse(args, *p);
		p++;
		if (ret == -1) 
			continue;
	}
	return 0;
}
#else
int sonic_get_option(struct tester_args *args, char *str)
{
	int i =0, ret;
	char delim[] = " \t\n", *token, *tstr = str;

	SONIC_DPRINT("%s\n", str);

	while ((token = strsep(&tstr, delim)) != NULL) {
		if (*token == '\0')
			continue;

		if (i == 0) {
			if(strcmp(token, "tester")) 
				return -1;	
		} else {
			ret = sonic_simple_parse(args, token);
			if (ret == -1) 
				continue;
		}
		i++;
	}
	return 0;
}
#endif

// Check the correctness of crc algorithms
void sonic_test_crc(struct tester_args *args)
{
	int crc[4], i=0, l;
	unsigned char *buf, *p;
	struct sonic_udp_port_info info;

	if (!(buf = (unsigned char *) ALLOC(2000))) {
		SONIC_ERROR("Memory alloc failed\n");
		return;
	}

	SONIC_PRINT("\n");

	sonic_set_port_info_id(&info, args, 0);

	for ( l = args->begin ; l >= args->end ; l -= args->step ) {
		p = (SONIC_ALIGN(uint8_t, buf + PREAMBLE_LEN, 16) -8);
		sonic_fill_frame(&info, p, l);
#if SONIC_DDEBUG
		sonic_print_hex(p, l + 8, 32);
#endif

		crc[0] = crc32_le(0xffffffff, p + PREAMBLE_LEN, l-4);
		crc[1] = crc32_bitwise(0xffffffff, p + PREAMBLE_LEN, l-4);
		crc[2] = fast_crc(p+PREAMBLE_LEN, l);
		crc[3] = fast_crc_nobr(p+PREAMBLE_LEN, l-4);

		if (crc[0] == crc[1] && crc[1] == crc[2] && crc[2] == crc[3] && crc[3] == crc[0]) {
			SONIC_DDPRINT("%4d matches %.8x\n", l, crc[0]);
		} else {
			SONIC_PRINT("%4d does not match %.8x %.8x %.8x %.8x\n", l, crc[0], crc[1], crc[2], crc[3]);
			i ++;
		}
	}

	SONIC_PRINT("%d mismatches\n", i);

	FREE(buf);
}

struct tester_helper {
	struct sonic_udp_port_info info[SONIC_PORT_SIZE];
	struct sonic_pcs_block *blocks[SONIC_PORT_SIZE];
	uint8_t *buf[SONIC_PORT_SIZE];
	int blocks_cnt;
	int covert;
};

#if !SONIC_SFP
static int sonic_prepare_test_rx(struct sonic_port *port, uint8_t **pbuf, 
		struct sonic_pcs_block **pblocks, int porder)
{
	uint8_t *buf;
	struct sonic_pcs_block *blocks;

	if (!(buf = (unsigned char *) ALLOC_PAGES(porder))) 
		return -1;
	SONIC_MPRINT(buf, "\n");

	if (!(blocks = (struct sonic_pcs_block *) ALLOC_PAGES(porder + 1))) {
		FREE_PAGES(buf, porder);
		return -1;
	}
	SONIC_MPRINT(blocks, "\n");

	*pbuf = buf;
	*pblocks = blocks;

	return order2bytes(porder+1) / sizeof(struct sonic_pcs_block);
}

static int sonic_prepare_rx_buffer(struct sonic_port *port, struct tester_helper *helper, 
		int pcs_porder, int dma_porder, int pkt_len, int idx)
{
	uint64_t state = PCS_INITIAL_STATE;
	int bat, cnt, cnt2;
	struct covert_tracker tx_covert;

	bat = sonic_fill_buf_batching(&helper->info[idx], helper->buf[idx], 
			order2bytes(pcs_porder), -1, pkt_len, 12);
	sonic_update_buf_batching(helper->buf[idx], order2bytes(pcs_porder), 0, 1, 0);

	//Prepare to send covert data
	if (helper->covert) {
		memset(tx_covert.dataBuf, 0, sizeof(tx_covert.dataBuf));
		strcpy((char*) tx_covert.dataBuf, "HELLO");
		tx_covert.dataLen = strlen((char*) tx_covert.dataBuf);
		prepareSendDataBuffer(& tx_covert);

		cnt = covert_encode_helper(&state, helper->buf[idx], pkt_len, 12, 12, bat, 
				helper->blocks[idx], 496 * power_of_two(dma_porder), &tx_covert);
	} else {
		cnt = sonic_encode_helper(&state, helper->buf[idx], pkt_len, 12, 12, bat, 
				helper->blocks[idx], 496 * power_of_two(dma_porder));
	}

	cnt2 = sonic_gearbox_helper(port->rx_pcs->dma_buf, port->rx_pcs->dma_buf_order,
			helper->blocks[idx], cnt);

	port->rx_pcs->dma_idx = cnt2;

	return 0;
}
#endif

void sonic_tester_helper_release(struct sonic_priv *sonic, struct tester_args *args,
		struct tester_helper *helper)
{
	int i;
	for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
		if (helper->blocks[i])
			FREE_PAGES(helper->blocks[i], args->rx_pcs_porder +1);
		if (helper->buf[i])
			FREE_PAGES(helper->buf[i], args->rx_pcs_porder);

//		if (sonic->ports[i]->tx_mac->data) 
//			sonic_free_pkt_gen(sonic->ports[i]->tx_mac);
	}
}

int sonic_tester_helper_prepare(struct sonic_priv *sonic, struct tester_args *args, 
		struct tester_helper *helper)
{
	int i, ret = -1;

	memset(helper, 0, sizeof(struct tester_helper));

	for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
		sonic_set_port_info_id(&helper->info[i], args, i);
#if !SONIC_SFP
		if (args->mode >= MODE_DECODE) {
			helper->blocks_cnt = sonic_prepare_test_rx(sonic->ports[i], &helper->buf[i], 
					&helper->blocks[i], args->rx_pcs_porder);
			if (helper->blocks_cnt == -1) {
				goto out;
			}
			if (args->mode == MODE_COVERT_DECODE)
				helper->covert = 1;
		}
#endif /* SONIC_SFP */
	}

	ret = 0;
#if !SONIC_SFP
out:
#endif
	return ret;
}

void sonic_tester_helper_init(struct sonic_priv *sonic, struct tester_args *args, 
		struct tester_helper *helper, int pkt_len)
{
	int i;

    SONIC_DPRINT("\n");

	for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
#if !SONIC_SFP
		if (args->mode >= MODE_DECODE) {
			sonic_prepare_rx_buffer(sonic->ports[i], helper,
				args->rx_pcs_porder, args->rx_dma_porder, pkt_len, i);
		}
#endif /* !SONIC_SFP */
		if ((args->mode >= MODE_CRC && args->mode <= MODE_TX_MAX) ||
				(args->mode >= MODE_PCS_FWRD && args->mode < MODE_PORT_MAX)) {
			sonic_pkt_gen_set(sonic->ports[i]->gen, 0, pkt_len, args);
//			sonic_prepare_pkt_gen(sonic->ports[i]->tx_mac, args->batching);
            if ((args->mode == MODE_PKT_RPT || args->mode == MODE_PKT_RPT_CAP) && i == 0) {
                sonic_prepare_pkt_gen(sonic->ports[i]->gen, 
                        sonic->ports[i]->log->out_fifo, args->batching);
            }
            else if (args->mode == MODE_PKT_CRC_RPT) {
                sonic_prepare_pkt_gen(sonic->ports[i]->gen, 
                        sonic->ports[i]->log->out_fifo, args->batching);
                sonic_prepare_pkt_gen(sonic->ports[i]->gen, 
                        sonic->ports[i]->tx_mac->out_fifo, args->batching);
            }
            else if (args->mode != MODE_PCS_APP_FWRD && args->mode != MODE_PCS_APP_CAP_FWRD) {
                sonic_prepare_pkt_gen(sonic->ports[i]->gen, 
                        sonic->ports[i]->tx_mac->out_fifo, args->batching);
            }
		}
	}
}

/* integrated test function */
int sonic_tester_sonic(struct sonic_priv *sonic)
{
    struct tester_args * args = &sonic->tester_args;
    int len, loop, ret=0;
    struct tester_helper helper;

    SONIC_DPRINT("\n");

    sonic_init_sonic(sonic, args->mode);

    sonic_tester_helper_prepare(sonic, args, &helper);

    for ( len = args->begin ; len >= args->end ; len -= args->step ) {
        SONIC_DPRINT("pkt_len = %d\n", len);

        sonic_tester_helper_init(sonic, args, &helper, len);

        for ( loop = 0 ; loop < args->nloops ; loop ++ ) {
            ret = sonic_run_sonic(sonic, args->duration);

            if (ret)
                goto out;
        }
        SLEEP(1);
    }

out:
    sonic_tester_helper_release(sonic, args, &helper);

    sonic_release_sonic(sonic);

    return ret;
}

int sonic_tester(struct sonic_priv *sonic)
{
    struct tester_args * args = &sonic->tester_args;
    int ret=0;

    SONIC_DPRINT("\n");

    check_args(args);

    if (args->mode == MODE_CRC_CMP) {
        sonic_test_crc(args);
    } else if (args->mode <= MODE_PORT_MAX) {
        ret = sonic_tester_sonic(sonic); 
#if SONIC_SFP	// idles, DMAs
    } else if (args->mode <= MODE_MAX) {
        int i;
        sonic_init_sonic(sonic, args->mode);
        for ( i = 0 ; i < args->nloops; i ++ )
            ret = sonic_run_sonic(sonic, args->duration);
#endif
    } else {
        SONIC_ERROR("Wrong mode\n");
        return -1;
    }

    if (!ret)
        SONIC_DPRINT("DONE\n");
    else
        SONIC_ERROR("ERROR\n");

    return ret;
}

/* tester entry from kernel */
#if SONIC_KERNEL
void sonic_test_wrapper(void *arg)
{
    struct sonic_priv *sonic = (struct sonic_priv *) arg;

    SONIC_DDPRINT("\n");

    set_cpus_allowed(current, cpumask_of_cpu(6));

    sonic_tester(sonic);
}

#define SONIC_TESTER_WAIT
// entrance from kernel/proc
int sonic_test_starter(struct sonic_priv *sonic, char *buf)
{
#ifndef SONIC_TESTER_WAIT
    pid_t pid;
    unsigned long flags = CLONE_FS | CLONE_FILES | CLONE_SIGHAND;
#endif
    struct tester_args args;
    SONIC_DDPRINT("\n");

    // FIXME: Move these to init_sonic
    sonic_init_tester_args(&args);

    sonic_get_option(&args, buf);

    memcpy(&sonic->tester_args, &args, sizeof(struct tester_args));

#ifndef  SONIC_TESTER_WAIT
    if ((pid = kernel_thread(sonic_test_wrapper, sonic, flags))< 0) {
        SONIC_ERROR("kernel_thread failed\n");
        return -1;
    }
#else 
    sonic_test_wrapper(sonic);
#endif
    return 0;
}
#endif /* SONIC_KERNEL */
