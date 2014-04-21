#ifndef __SONIC_IOCTL__
#define __SONIC_IOCTL__
#if !SONIC_KERNEL
#include <stdint.h>
#endif

struct sonic_pkt_gen_info {
	int mode;
	int pkt_num;
	int pkt_len;
	int idle;
	int chain;
	int wait;

	char mac_src[18];
	char mac_dst[18];

	char ip_src[16];
	char ip_dst[16];

	int port_src;
	int port_dst;
};

struct sonic_stat {
	uint64_t stats[32];
};

/* ioctl */
#define SONIC_IOC_MAGIC			'p'	// TODO

/* sonic control */
#define SONIC_IOC_RESET			_IO(SONIC_IOC_MAGIC, 0x00)
#define SONIC_IOC_STOP			_IO(SONIC_IOC_MAGIC, 0x01)
#define SONIC_IOC_START			_IO(SONIC_IOC_MAGIC, 0x02)
#define SONIC_IOC_RUN			_IOW(SONIC_IOC_MAGIC, 0x03, int)

#define SONIC_IOC_SET_MODE		_IOW(SONIC_IOC_MAGIC, 0x04, int)

#define SONIC_IOC_GET_STAT		_IOR(SONIC_IOC_MAGIC, 0x05, struct sonic_stat)
#define SONIC_IOC_RESET_STAT		_IO(SONIC_IOC_MAGIC, 0x06)

/* configuration */
#define SONIC_IOC_PORT0_INFO_SET	_IOW(SONIC_IOC_MAGIC, 0x10, struct sonic_pkt_gen_info)
#define SONIC_IOC_PORT1_INFO_SET	_IOW(SONIC_IOC_MAGIC, 0x11, struct sonic_pkt_gen_info)

#define SONIC_IOC_MAXNR			0x10



#endif /*__SONIC_IOCTL__*/
