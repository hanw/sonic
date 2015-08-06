#ifndef __SONIC_IOCTL__
#define __SONIC_IOCTL__
#if !SONIC_KERNEL
#include <stdint.h>
#endif

/* ioctl */
#define SONIC_IOC_MAGIC			'p'	// TODO

/* sonic control */
#define SONIC_IOC_RESET			_IO(SONIC_IOC_MAGIC, 0x00)
#define SONIC_IOC_RUN			_IOW(SONIC_IOC_MAGIC, 0x03, int)

#define SONIC_IOC_SET_MODE		_IOW(SONIC_IOC_MAGIC, 0x04, int)

#define SONIC_IOC_GET_STAT		_IOR(SONIC_IOC_MAGIC, 0x05, \
        struct sonic_sonic_stat)
#define SONIC_IOC_RESET_STAT		_IO(SONIC_IOC_MAGIC, 0x06)

/* configuration */
#define SONIC_IOC_PORT0_INFO_SET	_IOW(SONIC_IOC_MAGIC, 0x10, \
        struct sonic_config_runtime_port_args)
#define SONIC_IOC_PORT1_INFO_SET	_IOW(SONIC_IOC_MAGIC, 0x11, \
        struct sonic_config_runtime_port_args)

#define SONIC_IOC_MAXNR			0x11

#endif /*__SONIC_IOCTL__*/
