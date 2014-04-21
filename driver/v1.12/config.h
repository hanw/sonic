#ifndef __SONIC_CONFIG__
#define __SONIC_CONFIG__

#if !SONIC_KERNEL
#include <stdint.h>
#else
#include <linux/types.h>
#endif

#define TESTER_INT_ARGS				\
	TESTER(nloops, 1)			\
	TESTER(begin, 1518)			\
	TESTER(end, 1518)			\
	TESTER(step, 8)				\
	TESTER(mode, 1)				\
	/*TESTER(batching, 64)*/			\
	TESTER(batching, -1)			\
	TESTER(duration, 1)			\
	TESTER(idle, 12)			\
	TESTER(pkt_cnt, 1000)			\
	TESTER(chain_idle, 8*0x9502f90ULL)      \
	TESTER(wait, 0)				\
	TESTER(chain_num, 10000)		\
	TESTER(multi_queue, 1)			\
	TESTER(delta, 1018)			\
    TESTER(verbose, 0)          \
						\
	TESTER(log_porder, 4)			\
						\
	TESTER(tx_dma_num_desc, 1)		\
	TESTER(tx_dma_porder, 4)		\
	TESTER(tx_pcs_fnum, 3)			\
	TESTER(tx_pcs_porder, 4)		\
	TESTER(tx_mac_fnum, 3)			\
	TESTER(tx_mac_porder, 4)		\
						\
	TESTER(rx_dma_num_desc, 1)		\
	TESTER(rx_dma_porder, 3)		\
	TESTER(rx_pcs_fnum, 30)			\
	TESTER(rx_pcs_porder, 4)		\
	TESTER(rx_mac_fnum, 30)			\
	TESTER(rx_mac_porder, 4)		\

#define TESTER_INT_ARRAY_ARGS			\
	TESTER(log_cpu, 1, 1)			\
						\
	TESTER(tx_socket, 1, 0)			\
	TESTER(tx_pcs_cpu, 3, 3)		\
	TESTER(tx_mac_cpu, 2, 2)		\
						\
	TESTER(rx_socket, 1, 0)			\
	TESTER(rx_pcs_cpu, 4, 4)		\
	TESTER(rx_mac_cpu, 5, 5)		\
						\
	TESTER(port_socket, 1, 0)		\
	TESTER(port_src, 5000, 5008)		\
	TESTER(port_dst, 5008, 5008)

#define TESTER_STR_ARRAY_ARGS						\
	/*TESTER(mac_src, "02:80:c2:00:10:20", "01:80:c2:00:10:20", 18)	\
	TESTER(mac_dst, "01:80:c2:00:10:20", "02:80:c2:00:10:20", 18)*/	\
	TESTER(mac_src, "20:10:00:c2:80:02", "20:10:00:c2:80:02", 18)	\
/*	TESTER(mac_dst, "90:e2:ba:24:f2:60", "20:10:00:c2:80:01", 18)	\
	TESTER(ip_src, "192.168.1.8", "192.168.3.200", 16)		\
	TESTER(ip_dst, "192.168.2.8", "192.168.3.201", 16/)*/	/* left0 */	\
	TESTER(mac_dst, "00:60:dd:45:39:05", "20:10:00:c2:80:01", 18)   \
	TESTER(ip_src, "192.168.3.201", "192.168.3.215", 16)		\
	TESTER(ip_dst, "192.168.3.215", "192.168.3.215", 16) /* eth15 */	 	\
	TESTER(covert_msg, "Hello World", "Hello World", 12)		\
	//TODO others?

struct tester_args {
#define TESTER(x,y) int x;
	TESTER_INT_ARGS
#undef TESTER
#define TESTER(x,y,z) int x[2];
	TESTER_INT_ARRAY_ARGS
#undef TESTER
#define TESTER(x,y,z,w) char x [2][w];
	TESTER_STR_ARRAY_ARGS
#undef TESTER

#if !SONIC_KERNEL
//	char * f;
#endif
};

#define DECLARE_CLOCK				\
	struct timespec _t1, _t2;		\
	unsigned long long _time=0, _tbits=0;
#define START_CLOCK()	CLOCK(_t1)
#define STOP_CLOCK(x)				\
	CLOCK(_t2);				\
	_time = timediff2u64(&_t1, &_t2);	\
	_tbits = x;
#if !SONIC_KERNEL
#define	SONIC_OUTPUT(msg, args...) do {\
	SONIC_PRINT("thru= %f " msg, 	\
            (double)_tbits / _time, ##args);	\
	} while(0)
#else /* SONIC_KERNEL */
#define	SONIC_OUTPUT(msg, args...) do {\
	SONIC_PRINT("_time= %llu _tbits= %llu " msg, _time, _tbits, ##args);	\
	} while(0)
#endif /* SONIC_KERNEL */

#define SONIC_CPU(args, path, port_id, x)	((args-> path ## _socket[port_id]) * 6 + args->x ## _cpu[port_id])
#define SONIC_SOCKET(args, path, port_id)	((args-> path ## _socket[port_id]) * 6)

struct sonic_priv;
struct sonic_pcs_block;

/* tester.c */
void sonic_init_tester_args(struct tester_args*);
#if SONIC_KERNEL
int sonic_test_starter(struct sonic_priv*, char *);
int sonic_get_option(struct tester_args *, char *);
#else /* SONIC_KERNEL */
int sonic_tester(struct sonic_priv *);
int sonic_get_option(struct tester_args *, char **);
#endif /* SONIC_KERNEL */

/* tester_helper.c */
int sonic_encode_helper(uint64_t *, uint8_t *, int , int , unsigned , int , 
		struct sonic_pcs_block *, int );
int sonic_gearbox_helper(uint64_t *, int, struct sonic_pcs_block *, int);
#if !SONIC_KERNEL
struct sonic_logger;
int sonic_ins_helper(struct sonic_logger *, char *);
#endif

#define sonic_set_port_info_id(info, args, port_id)	\
	sonic_set_port_info (info, args->mac_src[port_id], args->mac_dst[port_id],\
		args->ip_src[port_id], args->ip_dst[port_id], \
		args->port_src[port_id], args->port_dst[port_id])
		


#endif /* __SONIC_CONFIG__ */
