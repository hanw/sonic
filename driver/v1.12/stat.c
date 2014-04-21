#if SONIC_KERNEL

#else
#include <stdio.h>
#endif /* SONIC_KERNEL */

#include "sonic.h"
#include "ioctl.h"

static void sonic_reset_pcs_stat(struct sonic_pcs *pcs)
{
	pcs->stat.total_pkts = 0;
	pcs->stat.total_blocks = 0;
	pcs->stat.total_idles = 0;
	pcs->stat.total_errors = 0;
}

static void sonic_reset_mac_stat(struct sonic_mac *mac)
{
	mac->stat.total_pkts = 0;
	mac->stat.total_bytes = 0;
	mac->stat.total_crcerrors = 0;
	mac->stat.total_lenerrors = 0;
}

static void sonic_reset_logger_stat(struct sonic_logger *logger)
{

}

void sonic_reset_port_stat(struct sonic_port *port)
{
#define SONIC_THREADS(x,y,z) sonic_reset_ ##y ##_stat (port->x);
	SONIC_THREADS_ARGS
#undef SONIC_THREADS
}

void sonic_reset_stat(struct sonic_priv *sonic)
{
	int i;

	for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) 
		sonic_reset_port_stat(sonic->ports[i]);
}

// FIXME: buf must be large enough
static int sonic_print_pcs_stat(struct sonic_pcs *pcs, char *buf, int dir)
{
	int ret=0;

	ret = sprintf(buf, 
		"  %s PCS ==================\n"
		"    Packets = %llu"
		"    Blocks  = %llu"
		"    Idles   = %llu"
		"    Errors  = %llu\n",
		dir == SONIC_DIR_TX ? "TX" : "RX", 
		(unsigned long long) pcs->stat.total_pkts,
		(unsigned long long) pcs->stat.total_blocks,
		(unsigned long long) pcs->stat.total_idles,
		(unsigned long long) pcs->stat.total_errors);

	return ret;
}

// FIXME: buf must be large enough
static int sonic_print_mac_stat(struct sonic_mac *mac, char *buf, int dir)
{
	int ret=0;

	ret = sprintf(buf, 
		"  %s MAC ==================\n"
		"    Packets = %llu"
		"    Bytes   = %llu"
		"    CRC err = %llu"
		"    LEN err = %llu\n",
		dir == SONIC_DIR_TX ? "TX" : "RX", 
		(unsigned long long) mac->stat.total_pkts,
		(unsigned long long) mac->stat.total_bytes,
		(unsigned long long) mac->stat.total_crcerrors,
		(unsigned long long) mac->stat.total_lenerrors);

	return ret;
}

static int sonic_print_logger_stat(struct sonic_logger *logger, char *buf, int dir)
{
	return 0;
}

// FIXME: buf must be large enough
// TODO: error check
int sonic_print_port_stat(struct sonic_port *port, char *buf)
{
	int ret = 0;
	char *p = buf;

	ret = sprintf(p, 
		" Port [%d] ==============\n", port->port_id);
	p += ret;

#define SONIC_THREADS(x,y,z) \
	ret = sonic_print_ ##y ##_stat(port-> x, p, SONIC_DIR_ ##z);	\
	p += ret;
	SONIC_THREADS_ARGS
#undef SONIC_THREADS

	return ret;
}

void sonic_print_stat(struct sonic_priv *sonic, char *buf)
{
	int ret = 0, i;
	char *p = buf;

	for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
		ret = sonic_print_port_stat(sonic->ports[i], p);

		p += ret;
	}
}

void sonic_get_stat_pcs(struct sonic_pcs *pcs, uint64_t *buf)
{
	int i = 0;
	buf[i++] = pcs->stat.total_pkts;
	buf[i++] = pcs->stat.total_blocks;
	buf[i++] = pcs->stat.total_idles;
	buf[i++] = pcs->stat.total_errors;
}

void sonic_get_stat_mac(struct sonic_mac *mac, uint64_t *buf)
{
	int i = 0;
	buf[i++] = mac->stat.total_pkts;
	buf[i++] = mac->stat.total_bytes;
	buf[i++] = mac->stat.total_crcerrors;
	buf[i++] = mac->stat.total_lenerrors;
}

void sonic_get_stat_logger(struct sonic_logger *log, uint64_t *buf)
{

}

/* TODO buf must be large enough */
void sonic_get_stat(struct sonic_priv *sonic, uint64_t *buf)
{
	int i;
	uint64_t *p = buf;

	for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
#define SONIC_THREADS(x,y,z) \
		sonic_get_stat_ ##y (sonic->ports[i]-> x, p);	\
		p += 4;
		SONIC_THREADS_ARGS
#undef SONIC_THREADS
	}
}
